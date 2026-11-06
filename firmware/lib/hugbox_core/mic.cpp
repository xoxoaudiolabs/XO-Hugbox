// XO-Hugbox: mic sampling (INMP441). The recorder (audio core), the sample editor, user samples on flash, and the
// SAMPLE view. Part of MainApp; see docs/mic-plan.md.
//
// Audio core: micIn() runs the input chain (gain, 20 Hz DC blocker, 16-bit) into a 300 ms pre-roll ring, and into
// recBuf_ while a take records. The control core only raises request flags (recArm_, recStop_, recCancel_, recAck_);
// the audio core owns recState_.
// Control core: a pad held for TAP_MS arms a take (the pre-roll covers the press); releasing it stops the take.
// The sample editor is a port of XOXO Studio's (fm1-bt-bridge/sampler.py + sampleui.py, same author): the same
// opening selection, fades, -6 dBFS normalise and reverse, and the same 128-column waveform with two markers, so a
// sample comes out the way the Studio would make it. Its source is the raw take, or a copy of the pad's sample, in
// recBuf_. buildSample() renders the selection into a new buffer and hands it to the sampler through the event
// queue; the audio core passes the replaced buffer back on retired_ to be freed here.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "main_app.h"

namespace hugbox {

static inline int voiceForPad(int pad) { return (3 - pad / 4) * 4 + pad % 4; }   // same as main_app.cpp
static constexpr uint32_t TAP_MS = 250;           // a shorter press selects and plays the pad instead of recording
static constexpr uint32_t HOLD_SAMPLE_MS = 600;   // Shift + REC held this long opens the SAMPLE view
// sampler.py's numbers
static constexpr int SILENCE = 184;                       // -45 dBFS: quieter than this is the silence around a take
static constexpr uint32_t SHORTEST = SAMPLE_RATE / 50;    // 20 ms: a selection can't be shorter
static constexpr float PEAK = 16422.0f;                   // normalise to -6 dBFS: room for the pads to mix several
static constexpr uint32_t FADE_STEP = SAMPLE_RATE / 100;  // fade knob unit, 10 ms

static float thrDb(int v) { return -60.0f + 0.6f * v; }
static float thrLin(int v) { return v <= 0 ? 0.0f : powf(10.0f, thrDb(v) / 20.0f); }
static inline int iabs(int v) { return v < 0 ? -v : v; }

// ================================================================ audio core
void MainApp::micIn(const int32_t *in, int n) {
  if (n > AUDIO_FRAMES) n = AUDIO_FRAMES;
  uint8_t st = recState_;
  if (recCancel_) { recCancel_ = false; recArm_ = false; recStop_ = false; if (st != R_DONE) st = R_IDLE; }
  if (recAck_ && st == R_DONE) { recAck_ = false; st = R_IDLE; }
  const bool active = view_ == V_SMP;
  monValid_ = active && m_.value[P_MMON];
  if (!active && st == R_IDLE) { recState_ = st; return; }      // costs nothing outside the SAMPLE view
  const float g = powf(10.0f, m_.value[P_MGN] / 20.0f) / 8388608.0f;   // 24-bit -> +-1, then the gain
  const float thr = thrLin(m_.value[P_MTHR]);
  bool go = false;
  if (recArm_ && st == R_IDLE && recBuf_) {
    recArm_ = false;
    st = R_ARMED;
    // the press itself may already have been loud enough: check what the pre-roll caught
    if (thr <= 0) go = true;
    else
      for (uint32_t j = 0; j < preFill_ && !go; j++) if (iabs(pre_[j]) >= thr * 32767.0f) go = true;
  }
  float pk = micPeak_;
  for (int i = 0; i < n; i++) {
    const float x = in[i] * g;
    float y = x - hpX_ + 0.99715f * hpY_;     // one-pole high-pass, ~20 Hz
    hpX_ = x;
    hpY_ = y;
    if (y > 1.0f) y = 1.0f; else if (y < -1.0f) y = -1.0f;
    mon_[i] = y;
    const float a = fabsf(y);
    if (a > pk) pk = a;
    const int16_t s = (int16_t)(y * 32767.0f);
    if (st == R_ARMED && (go || a >= thr)) {           // start: the pre-roll first, oldest sample first
      uint32_t k = 0;
      for (uint32_t j = 0; j < preFill_; j++) recBuf_[k++] = pre_[(preHead_ + PREROLL - preFill_ + j) % PREROLL];
      recLen_ = k;
      st = R_REC;
    }
    if (st == R_REC) {
      if (recLen_ < TAKE_MAX) { recBuf_[recLen_] = s; recLen_ = recLen_ + 1; }
      else st = R_DONE;                                   // 20 s: the take ends by itself
    } else {
      pre_[preHead_] = s;
      preHead_ = (preHead_ + 1) % PREROLL;
      if (preFill_ < (uint32_t)PREROLL) preFill_++;
    }
  }
  if (recStop_) { recStop_ = false; if (st == R_REC) st = R_DONE; else if (st == R_ARMED) st = R_IDLE; }
  recState_ = st;
  micPeak_ = pk;
}

// ================================================================ control core
void MainApp::loadUserSamples() {
  for (int i = 0; i < 16; i++) snprintf(userName_[i], sizeof userName_[i], "U%02d", i + 1);
  if (!p_.sampleFrames || !p_.loadSample) return;
  uint32_t total = 0;
  int count = 0;
  for (int i = 0; i < 16; i++) {
    uint32_t n = p_.sampleFrames(i);
    if (n < 2) continue;
    if (n > TAKE_MAX) n = TAKE_MAX;
    if (total + n > USER_MAX) { logf("user sample %d skipped: over the %u s limit\n", i + 1, (unsigned)(USER_MAX / SAMPLE_RATE)); continue; }
    int16_t *d = (int16_t *)p_.alloc(n * sizeof(int16_t));
    if (!d) { logf("user sample %d: out of memory\n", i + 1); break; }
    const uint32_t got = p_.loadSample(i, d, n);
    if (got < 2) { p_.release(d); continue; }
    sampler_.setUser(i, d, got);                        // audio isn't running yet
    userData_[i] = d;
    userFrames_[i] = got;
    total += got;
    count++;
  }
  if (count) logf("user samples: %d loaded, %u.%u s\n", count, (unsigned)(total / SAMPLE_RATE), (unsigned)(total % SAMPLE_RATE * 10 / SAMPLE_RATE));
}

const char *MainApp::voiceName(int v) const {
  const int s = m_.drum[v & 15][P_D_SMPL - P_D_PTCH] & 15;
  return userFrames_[s] ? userName_[s] : DRUM_NAMES[v & 15];
}

void MainApp::openSample(uint32_t now) {
  if (!recBuf_) recBuf_ = (int16_t *)p_.alloc(TAKE_MAX * sizeof(int16_t));
  if (!recBuf_) { toast("NO MEMORY TO SAMPLE", now); logf("SAMPLE: could not allocate the 20 s take buffer\n"); return; }
  view_ = V_SMP;
  track_ = T_DRUM;                                     // samples live on the drum pads
  heldStep_ = -1;
  mapOpen_ = false;
  for (int i = 0; i < 16; i++) releasePad(i);
  toast("HOLD A PAD = REC", now);
  logf("SAMPLE view: hold a pad to record the mic onto it, tap to select and play\n");
}

void MainApp::samplePadDown(int pad, uint32_t now) {
  m_.value[P_SPAD] = voiceForPad(pad);
  if (recPad_ >= 0 || recState_ != R_IDLE || !recBuf_ || saveAtMs_) return;   // one take at a time
  recPad_ = pad;
  recPadMs_ = now;
  recArmed_ = false;
  recSlot_ = selSlot();
}

void MainApp::samplePadUp(int pad, uint32_t now) {
  if (pad != recPad_) return;
  recPad_ = -1;
  if (!recArmed_) { post(E_DRUM, voiceForPad(pad), 100); return; }   // a tap: play the pad
  if (recState_ == R_ARMED) toast("NOTHING OVER THRS", now);
  recStop_ = true;
}

bool MainApp::editorShown(uint32_t now) const {
  return view_ == V_SMP && recState_ == R_IDLE && recPad_ < 0 && takeHere() &&
         now - gainMs_ > 1500;                         // turning the gain shows the input meter
}

void MainApp::sampleLoop(uint32_t now) {
  while (retTail_ != retHead_) {                       // buffers the audio core let go of
    p_.release((void *)retired_[retTail_]);
    retTail_ = (retTail_ + 1) & 31;
  }
  if (recHold_ && now - recHoldMs_ >= HOLD_SAMPLE_MS) { recHold_ = false; openSample(now); }
  if (recPad_ >= 0 && !recArmed_ && now - recPadMs_ >= TAP_MS) {   // held: arm a take (the pre-roll covers the press)
    recArmed_ = true;
    ed_.len = 0;                                       // recBuf_ is about to be overwritten
    recArm_ = true;
  }
  if (lastView_ == V_SMP && view_ != V_SMP) {          // leaving the view: drop a running take, save to flash
    recPad_ = -1;
    if (recState_ != R_IDLE) recCancel_ = true;
    bool dirty = false;
    for (bool d : userDirty_) dirty |= d;
    if (dirty && p_.saveSample) {
      muteOut_ = true;                                 // let the output go quiet before the flash write stalls it
      saveAtMs_ = now + 60;
      if (!saveAtMs_) saveAtMs_ = 1;
      toast("SAVING SAMPLES", now);
    }
  }
  lastView_ = view_;
  if (saveAtMs_ && (int32_t)(now - saveAtMs_) >= 0) { saveSamples(); saveAtMs_ = 0; muteOut_ = false; }
  if (recState_ == R_DONE && !recAck_) finishTake(now);
  // a pad with a sample: open it in the editor (a copy; the raw take of another pad is dropped)
  if (view_ == V_SMP && recState_ == R_IDLE && recPad_ < 0 && !takeHere() && userFrames_[selSlot()]) loadSource(selSlot());
  if (rebuild_ && now - rebuildMs_ >= 120) { rebuild_ = false; buildSample(now); }
}

// A take has ended: sampler.py's opening selection (the first sound to the last), then the pad's sample.
void MainApp::finishTake(uint32_t now) {
  const uint32_t n = recLen_;
  int peak = 0;
  for (uint32_t i = 0; i < n; i++) { const int a = iabs(recBuf_[i]); if (a > peak) peak = a; }
  recAck_ = true;                                      // the audio core goes back to idle; the buffer stays as it is
  if (n < SHORTEST) { toast("TOO SHORT", now); return; }
  const int quiet = peak * 3 / 100 > SILENCE ? peak * 3 / 100 : SILENCE;   // -30 dB under the peak: hiss isn't sound
  uint32_t first = 0, last = n;
  while (first < n && iabs(recBuf_[first]) <= quiet) first++;
  while (last > first && iabs(recBuf_[last - 1]) <= quiet) last--;
  ed_ = Edit();
  ed_.len = n;
  ed_.slot = recSlot_;
  if (first >= n) { ed_.start = 0; ed_.end = n; }     // nothing loud: all of it
  else {
    ed_.start = first > SAMPLE_RATE / 200 ? first - SAMPLE_RATE / 200 : 0;   // 5 ms before
    ed_.end = last + SAMPLE_RATE / 20 < n ? last + SAMPLE_RATE / 20 : n;      // 50 ms after
  }
  if (ed_.end < ed_.start + SHORTEST) ed_.end = ed_.start + SHORTEST < n ? ed_.start + SHORTEST : n;
  makeWave();
  logf("take: %u.%02u s, selection %u.%02u s, peak %d dBFS\n", (unsigned)(n / SAMPLE_RATE), (unsigned)(n % SAMPLE_RATE * 100 / SAMPLE_RATE),
       (unsigned)((ed_.end - ed_.start) / SAMPLE_RATE), (unsigned)((ed_.end - ed_.start) % SAMPLE_RATE * 100 / SAMPLE_RATE),
       peak ? (int)(20.0f * log10f(peak / 32767.0f) - 0.5f) : -96);
  if (peak < 300) toast("VERY QUIET: MORE GAIN", now);
  if (buildSample(now)) {
    for (int v = 0; v < 16; v++)                       // play it once
      if ((m_.drum[v][P_D_SMPL - P_D_PTCH] & 15) == ed_.slot) { post(E_DRUM, v, 100); break; }
  }
}

// Edit a pad's existing sample: a copy of it becomes the source, all of it selected, nothing applied.
bool MainApp::loadSource(int slot) {
  if (!recBuf_ || !userFrames_[slot] || recState_ != R_IDLE) return false;
  const uint32_t n = userFrames_[slot] < TAKE_MAX ? userFrames_[slot] : TAKE_MAX;
  memcpy(recBuf_, userData_[slot], n * sizeof(int16_t));
  ed_ = Edit();
  ed_.len = n;
  ed_.slot = slot;
  ed_.end = n;
  ed_.fadeOut = 0;
  ed_.norm = false;
  makeWave();
  return true;
}

// (low, high) per OLED column, scaled to the source's peak, as sampler.waveform()
void MainApp::makeWave() {
  memset(wave_, 0, sizeof wave_);
  const uint32_t n = ed_.len;
  if (n < 128) return;
  int full = 1;
  for (uint32_t i = 0; i < n; i++) { const int a = iabs(recBuf_[i]); if (a > full) full = a; }
  for (int c = 0; c < 128; c++) {
    const uint32_t a = (uint32_t)((uint64_t)n * c / 128), b = (uint32_t)((uint64_t)n * (c + 1) / 128);
    int lo = 0, hi = 0;
    for (uint32_t i = a; i < b; i++) { if (recBuf_[i] < lo) lo = recBuf_[i]; if (recBuf_[i] > hi) hi = recBuf_[i]; }
    wave_[c][0] = (int8_t)(lo * 60 / full);
    wave_[c][1] = (int8_t)(hi * 60 / full);
  }
}

// sampler.render(): the selection, reversed if asked, linear fades, then normalised to PEAK; into a new buffer.
bool MainApp::buildSample(uint32_t now) {
  if (!ed_.len || ed_.end < ed_.start + SHORTEST) return false;
  const int slot = ed_.slot;
  uint32_t n = ed_.end - ed_.start, used = 0;
  for (int i = 0; i < 16; i++) if (i != slot) used += userFrames_[i];
  if (used + n > USER_MAX) {
    if (used + SHORTEST > USER_MAX) { toast("SAMPLE MEMORY FULL", now); return false; }
    n = USER_MAX - used;
    toast("MEMORY FULL: CUT", now);
  }
  int16_t *d = (int16_t *)p_.alloc(n * sizeof(int16_t));
  if (!d) { toast("OUT OF MEMORY", now); return false; }
  const int16_t *src = recBuf_ + ed_.start;
  const uint32_t fi = ed_.fadeIn * FADE_STEP < n ? ed_.fadeIn * FADE_STEP : n;
  const uint32_t fo = ed_.fadeOut * FADE_STEP < n ? ed_.fadeOut * FADE_STEP : n;
  auto at = [&](uint32_t i) {
    float x = ed_.rev ? src[n - 1 - i] : src[i];
    if (i < fi) x *= fi > 1 ? (float)i / (fi - 1) : 0.0f;
    if (i >= n - fo) x *= fo > 1 ? (float)(n - 1 - i) / (fo - 1) : 0.0f;
    return x;
  };
  float peak = 0;
  for (uint32_t i = 0; i < n; i++) { const float a = fabsf(at(i)); if (a > peak) peak = a; }
  const float gain = ed_.norm && peak > 0 ? PEAK / peak : 1.0f;
  for (uint32_t i = 0; i < n; i++) {
    const float x = roundf(at(i) * gain);
    d[i] = (int16_t)(x > 32767.0f ? 32767.0f : x < -32768.0f ? -32768.0f : x);
  }
  installSample(slot, d, n);
  return true;
}

// Hand a buffer (or nullptr = back to the kit sound) to the sampler on the audio core.
void MainApp::installSample(int slot, int16_t *data, uint32_t frames) {
  newData_[slot] = data;
  newLen_[slot] = frames;
  if (!post(E_SMP_SET, (uint8_t)slot)) { if (data) p_.release(data); toast("BUSY, TRY AGAIN", lastLoopMs_); return; }
  userData_[slot] = data;
  userFrames_[slot] = data ? frames : 0;
  userDirty_[slot] = true;
}

void MainApp::saveSamples() {
  bool ok = true;
  int n = 0;
  for (int i = 0; i < 16; i++) {
    if (!userDirty_[i]) continue;
    userDirty_[i] = false;
    if (userFrames_[i]) ok &= p_.saveSample(i, userData_[i], userFrames_[i]);
    else if (p_.deleteSample) p_.deleteSample(i);
    n++;
  }
  toast(ok ? "SAMPLES SAVED" : "SAVE FAILED", lastLoopMs_);
  logf("samples: %d slot(s) written%s\n", n, ok ? "" : ", FAILED");
}

// ---- the editor's knobs (getVal/setVal route P_MTST..P_MSRC here)
int MainApp::editGet(uint8_t id) const {
  const bool here = takeHere();
  switch (id) {
    case P_MTST: return here ? (int)((uint64_t)ed_.start * 127 / ed_.len) : 0;
    case P_MTEN: return here ? (int)(((uint64_t)ed_.end * 127 + ed_.len - 1) / ed_.len) : 127;
    case P_MFIN: return here ? ed_.fadeIn : 0;
    case P_MFOU: return here ? ed_.fadeOut : 0;
    case P_MNRM: return here ? ed_.norm : 0;
    case P_MREV: return here ? ed_.rev : 0;
    default: return userFrames_[selSlot()] ? 1 : 0;   // P_MSRC
  }
}

void MainApp::editSet(uint8_t id, int v) {
  const uint32_t now = lastLoopMs_;
  const int slot = selSlot();
  if (id == P_MSRC) {
    if (v == 0 && userFrames_[slot]) {                 // back to the built-in sound
      installSample(slot, nullptr, 0);
      toast("KIT SOUND", now);                         // the editor keeps its copy: turning back to USER undoes it
    } else if (v == 1 && !userFrames_[slot] && takeHere() && buildSample(now)) toast("USER SOUND", now);
    return;
  }
  if (view_ != V_SMP || recState_ != R_IDLE) return;
  if (!takeHere() && !loadSource(slot)) { toast("RECORD FIRST", now); return; }
  const uint32_t n = ed_.len;
  switch (id) {
    case P_MTST: {                                     // one waveform column per step, like the Studio's arrow keys
      uint32_t s = (uint32_t)((uint64_t)n * v / 127);
      if (s + SHORTEST > ed_.end) s = ed_.end > SHORTEST ? ed_.end - SHORTEST : 0;
      ed_.start = s;
    } break;
    case P_MTEN: {
      uint32_t e = v >= 127 ? n : (uint32_t)((uint64_t)n * v / 127);
      if (e < ed_.start + SHORTEST) e = ed_.start + SHORTEST < n ? ed_.start + SHORTEST : n;
      ed_.end = e;
    } break;
    case P_MFIN: ed_.fadeIn = v; break;
    case P_MFOU: ed_.fadeOut = v; break;
    case P_MNRM: ed_.norm = v != 0; break;
    case P_MREV: ed_.rev = v != 0; break;
  }
  rebuild_ = true;
  rebuildMs_ = now;
}

// ================================================================ display
void MainApp::drawSample(uint32_t now) {
  if (editorShown(now)) { drawEditor(now); return; }
  u8g2_t *g = p_.oled;
  char s[24];
  uint32_t used = 0;
  for (uint32_t f : userFrames_) used += f;
  const uint32_t freeF = used < USER_MAX ? USER_MAX - used : 0;
  u8g2_SetFont(g, u8g2_font_5x7_tr);
  snprintf(s, sizeof s, "SAMPLE %u.%us", (unsigned)(freeF / SAMPLE_RATE), (unsigned)(freeF % SAMPLE_RATE * 10 / SAMPLE_RATE));
  u8g2_DrawStr(g, 0, 18, s);
  u8g2_SetFont(g, u8g2_font_helvB10_tr);
  u8g2_DrawStr(g, 0, 33, voiceName(m_.value[P_SPAD]));
  u8g2_SetFont(g, u8g2_font_4x6_tr);
  const uint8_t st = recState_;
  if (st == R_REC) {
    snprintf(s, sizeof s, "REC %u.%02us", (unsigned)(recLen_ / SAMPLE_RATE), (unsigned)(recLen_ % SAMPLE_RATE * 100 / SAMPLE_RATE));
    if ((now / 250) & 1) u8g2_DrawDisc(g, 55, 38, 2, U8G2_DRAW_ALL);
  } else if (st == R_ARMED) strcpy(s, "WAITING...");
  else if (recPad_ >= 0) strcpy(s, "HOLD...");
  else strcpy(s, "HOLD PAD = REC");
  u8g2_DrawStr(g, 0, 41, s);
  // input meter, -60..0 dBFS after the gain, with the threshold mark
  const float pk = micPeak_;
  micPeak_ = 0;
  meter_ = pk > meter_ ? pk : meter_ * 0.85f;
  float db = meter_ > 0.001f ? 20.0f * log10f(meter_) : -60.0f;
  if (db < -60) db = -60;
  u8g2_DrawFrame(g, 0, 44, 58, 6);
  u8g2_DrawBox(g, 1, 45, (int)((db + 60.0f) * 56.0f / 60.0f), 4);
  if (meter_ >= 0.99f) u8g2_DrawStr(g, 42, 41, "CLIP");
  if (m_.value[P_MTHR] > 0) {
    const int x = 1 + (int)((thrDb(m_.value[P_MTHR]) + 60.0f) * 56.0f / 60.0f);
    u8g2_SetDrawColor(g, 2);
    u8g2_DrawVLine(g, x, 43, 8);
    u8g2_SetDrawColor(g, 1);
  }
  drawPadGrid(now);
}

// sampleui.draw_editor(): the take's waveform, the selection between two markers, dotted outside it
void MainApp::drawEditor(uint32_t now) {
  u8g2_t *g = p_.oled;
  char s[32], v[16];
  constexpr int MID = 34, TOP = 20, BOT = 48;
  const uint32_t sel = ed_.end - ed_.start;
  snprintf(s, sizeof s, "%u.%us", (unsigned)(sel / SAMPLE_RATE), (unsigned)(sel % SAMPLE_RATE * 10 / SAMPLE_RATE));
  u8g2_SetFont(g, u8g2_font_5x7_tr);
  const int w = u8g2_GetStrWidth(g, s);
  u8g2_DrawStr(g, 127 - w, 18, s);
  // the top line names what the last knob changed, else the pad
  const int id = touchKnob_ >= 0 && now - touchMs_ < 2000 ? page().slot[touchKnob_ + (shift_ ? 4 : 0)] : P_NONE;
  if (id != P_NONE) {
    fmtVal((uint8_t)id, getVal((uint8_t)id), v);
    snprintf(s, sizeof s, "%s %s%s", PARAMS[id].label, v, latched_[touchKnob_] ? "" : " <>");
  } else snprintf(s, sizeof s, "%s  EDIT", voiceName(m_.value[P_SPAD]));
  u8g2_DrawStr(g, 0, 18, s);
  const int sx = (int)((uint64_t)ed_.start * 127 / ed_.len), ex = (int)((uint64_t)ed_.end * 127 / ed_.len);
  for (int i = 0; i < 128; i++)
    if ((i >= sx && i <= ex) || i % 2 == 0) u8g2_DrawVLine(g, i, MID - wave_[i][1] / 5, 1 + (wave_[i][1] - wave_[i][0]) / 5);
  for (int k = 0; k < 2; k++) {
    const int x = k ? ex : sx;
    u8g2_SetDrawColor(g, 0);                           // a dark edge each side
    if (x > 0) u8g2_DrawVLine(g, x - 1, TOP, BOT - TOP + 1);
    if (x < 127) u8g2_DrawVLine(g, x + 1, TOP, BOT - TOP + 1);
    u8g2_SetDrawColor(g, 1);
    u8g2_DrawVLine(g, x, TOP, BOT - TOP + 1);
    if (id == (k ? P_MTEN : P_MTST)) u8g2_DrawTriangle(g, x - 3, TOP, x + 4, TOP, x, TOP + 4);   // the one being moved
  }
  if (ed_.rev) { u8g2_SetFont(g, u8g2_font_4x6_tr); u8g2_DrawStr(g, 0, 51, "REV"); }
}

}  // namespace hugbox
