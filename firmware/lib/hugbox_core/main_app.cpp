// XO-Hugbox main app - see main_app.h for the control map.
#include "main_app.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace hugbox {


// ---------------------------------------------------------------- knob pages
static const Page PG_PLAY_DRUM = {"PLAY", {P_D_PTCH, P_D_DCY, P_D_CUT, P_D_LVL, P_DMIX, P_DRV, P_BPM, P_MVOL}};
static const Page PG_PLAY_SYN = {"PLAY", {P_CUT, P_RES, P_ATK, P_REL, P_OCT, P_SCALE, P_CHRD, P_ARP}};
static const Page PG_SEQ_DRUM = {"SEQ", {P_SPAD, P_BPM, P_SWNG, P_LEN, P_DMIX, P_DRV, P_CRSH, P_MVOL}};
static const Page PG_SEQ_SYN = {"SEQ", {P_SCALE, P_BPM, P_SWNG, P_LEN, P_DMIX, P_DRV, P_CRSH, P_MVOL}};
static const Page PG_STEP = {"STEP", {P_ST_VEL, P_ST_NOTE, P_ST_GATE, P_ST_PROB, P_DMIX, P_DRV, P_CRSH, P_MVOL}};
static const Page PG_SND_DRUM = {"PAD", {P_D_PTCH, P_D_DCY, P_D_CUT, P_D_LVL, P_D_SMPL, P_D_PAN, P_D_CHOK, P_KITV}};
static constexpr int SYN_PAGES = 3;
static const Page PG_SND_SYN[SYN_PAGES] = {
  {"OSC", {P_W1, P_W2, P_DTUN, P_MIX, P_FM, P_FMR, P_DRFT, P_GLID}},
  {"FILTER", {P_CUT, P_RES, P_ENV, P_SVOL, P_ATK, P_DCY, P_SUS, P_REL}},
  {"CHORD/ARP", {P_CHRD, P_INV, P_ARP, P_ARPR, P_ARPO, P_ARPG, P_LTCH, P_ROOT}},
};
static const Page PG_FX = {"FX", {P_DTIM, P_DFB, P_DMIX, P_DRV, P_CRSH, P_RATE, P_SHPF, P_MVOL}};
static const Page PG_SET = {"SETTINGS", {P_MCH, P_DCH, P_SYNC, P_BLE, P_BRT, P_MTHR, P_MMON, P_NONE}};
// SAMPLE view: XOXO Studio's sample editor on the knobs; the gain and the editor's switches on the shift layer
static const Page PG_SMP = {"SAMPLE", {P_MTST, P_MTEN, P_MFIN, P_MFOU, P_MGN, P_MNRM, P_MREV, P_MSRC}};
static const char *const VIEW_NAMES[V_COUNT] = {"PLAY", "SEQ", "SOUND", "FX", "SETTINGS", "SAMPLE"};
static const char *const MODE_NAMES[3] = {"STD", "CTL", "LAY"};
static const char *const MODE_LONG[3] = {"STANDALONE", "CONTROLLER", "LAYERED"};

// pad n = row*4 + col, row 0 at the top (nearest the knobs). Voices/notes count from the bottom-left
// like a drum machine; steps read like text from the top-left.
static inline int voiceForPad(int pad) { return (3 - pad / 4) * 4 + pad % 4; }

// fonts
#define F_TINY u8g2_font_4x6_tr
#define F_SMALL u8g2_font_5x7_tr
#define F_MED u8g2_font_6x10_tf
#define F_BIG u8g2_font_helvB10_tr

static float msFrom(int v) { const float x = v / 100.0f; return 1.0f + x * x * 3000.0f; }   // same curve as engine.cpp

// ---------------------------------------------------------------- setup
void MainApp::begin(const Platform &p) {
  p_ = p;
  m_.reset();
  kitBytes_ = sampler_.init(p.alloc, p.release);
  if (!fx_.init(p.alloc)) logf("Delay: out of memory, delay disabled\n");
  logf("Built-in kit: %u KB of samples %s\n", (unsigned)(kitBytes_ >> 10), sampler_.ready() ? "ready" : "FAILED (out of memory)");
  loadUserSamples();
  if (!loadSlot(0, 0)) logf("no autosave yet: starting from the default project\n");
  autosaveHash_ = pendingHash_ = modelHash();
  scanSlots();
  logf("XO-Hugbox main UI. Shift+pad: top row views, row 2 pages, row 3 track/run/rec, bottom row modes.\n");
}

void MainApp::logf(const char *fmt, ...) {
  if (!p_.log) return;
  char line[120];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  p_.log(line);
}

void MainApp::toast(const char *msg, uint32_t now) {
  snprintf(toast_, sizeof toast_, "%s", msg);
  toastMs_ = now;
}

// ---------------------------------------------------------------- params
const Page &MainApp::page() const {
  switch (view_) {
    case V_PLAY: return track_ == T_DRUM ? PG_PLAY_DRUM : PG_PLAY_SYN;
    case V_SEQ:
      if (heldStep_ >= 0) return PG_STEP;
      return track_ == T_DRUM ? PG_SEQ_DRUM : PG_SEQ_SYN;
    case V_SOUND: return track_ == T_DRUM ? PG_SND_DRUM : PG_SND_SYN[soundPage_];
    case V_FX: return PG_FX;
    case V_SMP: return PG_SMP;
    default: return PG_SET;
  }
}

Step *MainApp::heldStepPtr() {
  if (heldStep_ < 0) return nullptr;
  return track_ == T_DRUM ? &m_.drumSteps[m_.value[P_SPAD]][heldStep_] : &m_.synthSteps[heldStep_];
}

int MainApp::getVal(uint8_t id) const {
  if (id >= P_MTST && id <= P_MSRC) return editGet(id);
  if (id >= P_D_PTCH && id <= P_D_CHOK) return m_.drum[m_.value[P_SPAD]][id - P_D_PTCH];
  if (id >= P_ST_VEL && id <= P_ST_PROB) {
    const Step *s = const_cast<MainApp *>(this)->heldStepPtr();
    if (!s) return PARAMS[id].def;
    switch (id) {
      case P_ST_VEL: return s->vel;
      case P_ST_NOTE: return s->note;
      case P_ST_GATE: return s->gate;
      default: return s->prob;
    }
  }
  return m_.value[id];
}

void MainApp::setVal(uint8_t id, int v) {
  const ParamDef &d = PARAMS[id];
  if (v < d.min) v = d.min;
  if (v > d.max) v = d.max;
  if (id >= P_MTST && id <= P_MSRC) { editSet(id, v); return; }
  if (id == P_MGN || id == P_MTHR) gainMs_ = lastLoopMs_;   // the SAMPLE view shows its input meter
  if (id >= P_D_PTCH && id <= P_D_CHOK) { m_.drum[m_.value[P_SPAD]][id - P_D_PTCH] = v; return; }
  if (id >= P_ST_VEL && id <= P_ST_PROB) {
    Step *s = heldStepPtr();
    if (!s) return;
    heldStepEdited_ = true;
    switch (id) {
      case P_ST_VEL: s->vel = v; break;
      case P_ST_NOTE: s->note = v; break;
      case P_ST_GATE: s->gate = v; break;
      default: s->prob = v; break;
    }
    return;
  }
  m_.value[id] = v;
}

void MainApp::fmtVal(uint8_t id, int v, char *out) const {
  static const char *const WAVES[4] = {"SIN", "SAW", "SQR", "NOIS"};
  static const char *const SYNC[3] = {"INT", "USB", "BLE"};
  if (id == P_ST_NOTE && track_ == T_SYNTH) {           // synth steps hold a scale degree
    noteName(synthNoteForPad(-1) + degreeToSemis(m_.value[P_SCALE], v), out);
    return;
  }
  switch (PARAMS[id].fmt) {
    case F_PCT: case F_NUM: case F_BITS: sprintf(out, "%d", v); break;
    case F_WAVE: strcpy(out, WAVES[v & 3]); break;
    case F_SCALE: strcpy(out, SCALE_NAMES[v]); break;
    case F_NOTE: { char n[6]; noteName(v + 12, n); n[strlen(n) - 1] = 0; strcpy(out, n); } break;
    case F_ONOFF: strcpy(out, v ? "ON" : "OFF"); break;
    case F_SYNC: strcpy(out, SYNC[v % 3]); break;
    case F_SIGNED: case F_SEMI: sprintf(out, v > 0 ? "+%d" : "%d", v); break;
    case F_PAN: if (v == 0) strcpy(out, "C"); else sprintf(out, "%c%d", v < 0 ? 'L' : 'R', abs(v)); break;
    case F_LEN: sprintf(out, "%d", v * 16); break;
    case F_PAD: strcpy(out, voiceName(v & 15)); break;
    case F_DB: sprintf(out, "+%ddB", v); break;
    case F_THR: if (v == 0) strcpy(out, "OFF"); else sprintf(out, "%ddB", (int)(-60.0f + 0.6f * v - 0.5f)); break;
    case F_TRIM: {                                     // a marker: seconds into the source
      const uint32_t f = takeHere() ? (uint32_t)((uint64_t)ed_.len * v / 127) : 0;
      sprintf(out, "%u.%02us", (unsigned)(f / SAMPLE_RATE), (unsigned)(f % SAMPLE_RATE * 100 / SAMPLE_RATE));
    } break;
    case F_FADE: if (v < 100) sprintf(out, "%dms", v * 10); else sprintf(out, "%d.%02ds", v / 100, v % 100); break;
    case F_SRC: strcpy(out, v ? "USER" : "KIT"); break;
    case F_CHORD: strcpy(out, CHORD_NAMES[v % 6]); break;
    case F_ARP: strcpy(out, ARP_NAMES[v % 6]); break;
    case F_ARPRATE: strcpy(out, ARP_RATE_NAMES[v % 6]); break;
    case F_DIV: strcpy(out, DELAY_DIV_NAMES[v % 10]); break;
    case F_HZ: if (v == 0) strcpy(out, "OFF"); else sprintf(out, "%d", (int)(20.0f * powf(2.0f, v / 100.0f * 4.32f))); break;
    case F_RATIO: sprintf(out, "%d.%d", v / 2, (v & 1) * 5); break;
    case F_MS: {
      const float ms = msFrom(v);
      if (ms < 1000) sprintf(out, "%dms", (int)ms); else sprintf(out, "%d.%ds", (int)(ms / 1000), ((int)ms % 1000) / 100);
    } break;
  }
}

int MainApp::synthNoteForPad(int pad) const {
  const int base = 12 * (m_.value[P_OCT] + 1) + m_.value[P_ROOT];
  if (pad < 0) return base;
  return base + degreeToSemis(m_.value[P_SCALE], voiceForPad(pad));
}

// ---------------------------------------------------------------- input
int MainApp::quantizedStep() const {
  const int len = m_.value[P_LEN] * 16;
  int s = curStep_ < 0 ? 0 : curStep_;
  if (samplesLeft_ < stepLen_ / 2) s = (s + 1) % len;   // closer to the next step
  return s;
}

void MainApp::padDown(int pad, uint32_t now) {
  if (pad == 11 && (shift_ || mapOpen_)) {           // REC: a tap arms sequencer record, held 2 s records a sample
    if (shift_) shiftUsed_ = true;
    mapOpen_ = false;
    recHold_ = true;
    recHoldMs_ = now;
    return;
  }
  if (shift_) { shiftUsed_ = true; mapOpen_ = false; shortcut(pad, now); return; }
  if (mapOpen_) { mapOpen_ = false; shortcut(pad, now); return; }   // map open: a tap runs that shortcut
  if (view_ == V_SET) { settingsPadDown(pad, now); return; }
  if (view_ == V_SMP) { samplePadDown(pad, now); return; }
  if (view_ == V_SEQ) {
    const int idx = seqPage_ * 16 + pad;
    if (heldStep_ >= 0) return;                       // one step at a time
    heldStep_ = idx;
    Step *s = heldStepPtr();
    heldStepWasOn_ = s->on;
    heldStepEdited_ = false;
    s->on = true;
    return;
  }
  if (track_ == T_DRUM) {
    const int v = voiceForPad(pad);
    m_.value[P_SPAD] = v;
    post(E_DRUM, v, 100);
    if (midiOutOn()) { midiSend(0x90 | (m_.value[P_DCH] - 1), GM_DRUM[v], 100); padDrum_[pad] = GM_DRUM[v]; }
    if (rec_ && playing_) m_.drumSteps[v][quantizedStep()].on = true;
  } else {
    const int root = synthNoteForPad(-1);
    int semis[4];
    const int n = chordSemis(m_.value[P_SCALE], m_.value[P_CHRD], m_.value[P_INV], voiceForPad(pad), semis);
    const bool arp = m_.value[P_ARP] != 0;
    if (arp && m_.value[P_LTCH]) {               // latch: a fresh press (no other pad held) replaces the pool
      bool others = false;
      for (int i = 0; i < 16; i++) if (i != pad && padNoteN_[i]) others = true;
      if (!others) post(E_ARP_CLEAR);
    }
    for (int i = 0; i < n; i++) {
      const int note = root + semis[i];
      if (note < 0 || note > 127) continue;
      padNotes_[pad][padNoteN_[pad]++] = note;
      post(arp ? E_ARP_ADD : E_NOTE_ON, note, 100);
      if (!arp && midiOutOn()) midiSend(0x90 | (m_.value[P_MCH] - 1), note, 100);   // arp notes go out from the audio core
    }
    padArp_[pad] = arp;
    lastNote_ = root + semis[0];
    if (rec_ && playing_) {
      Step &s = m_.synthSteps[quantizedStep()];
      s.on = true;
      s.note = voiceForPad(pad);
    }
  }
}

void MainApp::padUp(int pad, uint32_t now) {
  if (pad == 11 && recHold_) { recHold_ = false; shortcut(11, now); return; }   // a quick tap
  if (view_ == V_SET) { settingsPadUp(pad, now); return; }
  if (view_ == V_SMP) { samplePadUp(pad, now); return; }
  if (padDrum_[pad] >= 0) { midiSend(0x80 | (m_.value[P_DCH] - 1), padDrum_[pad], 0); padDrum_[pad] = -1; }
  if (view_ == V_SEQ) {
    if (heldStep_ == seqPage_ * 16 + pad) {
      Step *s = heldStepPtr();
      if (heldStepWasOn_ && !heldStepEdited_) s->on = false;   // tap on a lit step clears it
      heldStep_ = -1;
    }
    return;
  }
  releasePad(pad);
}

void MainApp::releasePad(int pad) {
  for (int i = 0; i < padNoteN_[pad]; i++) {
    if (!padArp_[pad]) {
      post(E_NOTE_OFF, padNotes_[pad][i]);
      if (midiOutOn()) midiSend(0x80 | (m_.value[P_MCH] - 1), padNotes_[pad][i], 0);
    }
    else if (!m_.value[P_LTCH]) post(E_ARP_DEL, padNotes_[pad][i]);
  }
  padNoteN_[pad] = 0;
}

void MainApp::shortcut(int pad, uint32_t now) {
  char msg[24];
  switch (pad) {
    case 0: case 1: case 3:
      view_ = pad == 0 ? V_PLAY : pad == 1 ? V_SEQ : V_FX;
      break;
    case 2:
      if (view_ == V_SOUND && track_ == T_SYNTH) soundPage_ = (soundPage_ + 1) % SYN_PAGES;
      view_ = V_SOUND;
      break;
    case 4: case 5: case 6: case 7:
      seqPage_ = pad - 4;
      view_ = V_SEQ;
      if (seqPage_ >= m_.value[P_LEN]) { snprintf(msg, sizeof msg, "PAGE %d (LEN %d)", seqPage_ + 1, m_.value[P_LEN] * 16); toast(msg, now); }
      break;
    case 8: case 9:
      track_ = pad == 8 ? T_DRUM : T_SYNTH;
      // FX, SETTINGS and SAMPLE don't show a track, so picking one jumps to its PLAY view;
      // PLAY / SEQ / SOUND stay put and redraw for the new track
      if (view_ == V_FX || view_ == V_SET || view_ == V_SMP) view_ = V_PLAY;
      toast(track_ == T_DRUM ? "DRUM TRACK" : "SYNTH TRACK", now);
      break;
    case 10:
      if (extSync_) { toast("FOLLOWING EXT CLOCK", now); break; }
      playing_ = !playing_;
      toast(playing_ ? "RUN" : "STOP", now);
      break;
    case 11:
      rec_ = !rec_;
      toast(rec_ ? "REC ARMED" : "REC OFF", now);
      break;
    case 12: case 13: case 14:
      mode_ = (Mode)(pad - 12);
      toast(MODE_LONG[mode_], now);
      if (mode_ == M_CTL) post(E_ALL_OFF);
      break;
    case 15:
      view_ = V_SET;
      break;
  }
  heldStep_ = -1;
  for (int i = 0; i < 16; i++) releasePad(i);
  logf("shift+pad %d -> %s %s %s%s\n", pad, VIEW_NAMES[view_], track_ == T_DRUM ? "DRUM" : "SYNTH",
       MODE_NAMES[mode_], playing_ ? " RUN" : "");
}

// Absolute pots + pages that change under them: a knob only takes over a value once it has been turned
// across it ("pick-up"), so switching pages never makes values jump. Selectors with few steps jump directly.
void MainApp::pollKnobs(uint32_t now) {
  const Page &pg = page();
  for (int k = 0; k < 4; k++) {
    const uint8_t id = pg.slot[k + (shift_ ? 4 : 0)];
    const uint32_t key = (uint32_t)id | (uint32_t)m_.value[P_SPAD] << 8 | (uint32_t)(heldStep_ + 1) << 16;
    if (key != knobKey_[k]) { knobKey_[k] = key; latched_[k] = false; prevTarget_[k] = -1; }
    if (abs((int)pot_[k] - (int)accepted_[k]) <= 24) continue;   // ADC noise
    accepted_[k] = pot_[k];
    if (mode_ == M_CTL && view_ != V_SET && view_ != V_SMP) { if (shift_) shiftUsed_ = true; knobCC(k, now); continue; }
    if (id == P_NONE) continue;
    if (shift_) shiftUsed_ = true;
    const ParamDef &d = PARAMS[id];
    const int range = d.max - d.min;
    const int target = d.min + (int)((pot_[k] * (float)range) / 4095.0f + 0.5f);
    const int cur = getVal(id);
    if (!latched_[k]) {
      const int pt = prevTarget_[k];
      if (range <= 16 || target == cur || (pt >= 0 && ((pt <= cur && target >= cur) || (pt >= cur && target <= cur))))
        latched_[k] = true;
    }
    prevTarget_[k] = target;
    if (latched_[k] && target != cur) setVal(id, target);
    touchKnob_ = k;
    touchMs_ = now;
    mapOpen_ = false;                                  // show the readout, not the map
  }
}

void MainApp::loop(uint32_t now, const Inputs &in) {
  lastLoopMs_ = now;
  extSync_ = m_.value[P_SYNC] != 0;
  if (!potsInit_) {
    for (int k = 0; k < 4; k++) pot_[k] = accepted_[k] = in.potRaw[k];
    potsInit_ = true;
  }
  if (now - lastScan_ >= 5) {                         // 200 Hz pad scan
    lastScan_ = now;
    const uint16_t pads = p_.pcfOk ? in.padBits : 0;
    // Shift: holding it only flips the knobs to their second set; the screen never changes.
    // A quick tap (no pad or knob used) closes the shortcut map; two quick taps open it.
    if (in.shift && !shift_) { shiftDownMs_ = now; shiftUsed_ = false; }
    if (!in.shift && shift_ && !shiftUsed_ && now - shiftDownMs_ < 300) {
      if (mapOpen_) { mapOpen_ = false; lastTapMs_ = 0; }
      else if (lastTapMs_ && now - lastTapMs_ < 400) { mapOpen_ = true; mapMs_ = now; lastTapMs_ = 0; }
      else lastTapMs_ = now;
    }
    shift_ = in.shift;
    if (mapOpen_ && now - mapMs_ > 5000) mapOpen_ = false;
    const uint16_t changed = pads ^ pads_;
    pads_ = pads;
    for (int n = 0; n < 16; n++)
      if (changed & (1u << n)) { if (pads & (1u << n)) padDown(n, now); else padUp(n, now); }
  }
  if (now - lastPots_ >= 10) {                        // 100 Hz knobs
    lastPots_ = now;
    for (int k = 0; k < 4; k++) pot_[k] = (pot_[k] * 3 + in.potRaw[k]) / 4;
    pollKnobs(now);
    vbat_ = in.vsenseMv * 2 / 1000.0f;
  }
  if (m_.value[P_ARP] != lastArp_ || m_.value[P_LTCH] != lastLatch_) {
    // arp switched off, or latch released with nothing held: drop the pool
    bool held = false;
    for (int i = 0; i < 16; i++) if (padNoteN_[i]) held = true;
    if (!m_.value[P_ARP] || (!m_.value[P_LTCH] && !held)) post(E_ARP_CLEAR);
    lastArp_ = m_.value[P_ARP];
    lastLatch_ = m_.value[P_LTCH];
  }
  if (playing_ != wasPlaying_) {
    wasPlaying_ = playing_;
    if (!playing_) post(E_ALL_OFF);
    if (midiOutOn() && !extSync_) midiSend(playing_ ? 0xFA : 0xFC, 0, 0);   // start / stop for followers
    if (!playing_) for (auto &o : offs_) if (o.left > 0) o.left = 1;        // flush pending note-offs
  }
  settingsHold(now);
  sampleLoop(now);
  autosave(now);
  midiDrain();
  for (int v = 0; v < 16; v++)
    if (drumHit_[v] != seenHit_[v]) { seenHit_[v] = drumHit_[v]; flashMs_[v] = now; }
  if (now - lastDraw_ >= 40) { lastDraw_ = now; draw(now); }   // 25 fps
}

// ---------------------------------------------------------------- display
void MainApp::draw(uint32_t now) {
  u8g2_t *g = p_.oled;
  if (!g) return;
  if (m_.value[P_BRT] != lastBrt_) { lastBrt_ = m_.value[P_BRT]; u8g2_SetContrast(g, 10 + lastBrt_ * 245 / 100); }
  u8g2_ClearBuffer(g);
  u8g2_SetFontMode(g, 1);
  u8g2_SetDrawColor(g, 1);
  drawStatus(now);
  if (mapOpen_) {
    drawShiftMap();
  } else {
    switch (view_) {
      case V_PLAY: drawPlay(now); drawKnobBar(); break;
      case V_SEQ: drawSeq(now); drawKnobBar(); break;
      case V_SET: drawSettings(now); drawKnobBar(); break;
      case V_SMP: drawSample(now); drawKnobBar(); break;
      default: drawParams(); break;
    }
    // (the sample editor names the knob on its own top line: the readout would hide the waveform)
    if (touchKnob_ >= 0 && now - touchMs_ < 900 && !editorShown(now)) drawReadout();
  }
  if (toast_[0] && now - toastMs_ < 800) drawToast();
  // only push the frame over I2C when it changed (a full frame is ~26 ms of bus time at 400 kHz)
  uint32_t h = 2166136261u;
  const uint8_t *fb = u8g2_GetBufferPtr(g);
  for (int i = 0; i < 1024; i++) h = (h ^ fb[i]) * 16777619u;
  if (h == lastFrameHash_ && frames_) return;
  lastFrameHash_ = h;
  u8g2_SendBuffer(g);
  frames_++;
}

void MainApp::drawStatus(uint32_t now) {
  u8g2_t *g = p_.oled;
  char s[16];
  u8g2_SetFont(g, F_SMALL);
  u8g2_DrawBox(g, 0, 0, 17, 9);
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawStr(g, 1, 7, MODE_NAMES[mode_]);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawStr(g, 20, 7, track_ == T_DRUM ? "DRUM" : "SYNTH");
  snprintf(s, sizeof s, "%d", m_.value[P_BPM]);
  u8g2_DrawStr(g, 50, 7, s);
  snprintf(s, sizeof s, "P%d", seqPage_ + 1);
  u8g2_DrawStr(g, 68, 7, s);
  if (playing_) u8g2_DrawTriangle(g, 81, 1, 81, 8, 86, 4);   // run
  else u8g2_DrawBox(g, 81, 2, 5, 5);                         // stop
  if (rec_ && (!playing_ || (now / 250) & 1)) u8g2_DrawDisc(g, 93, 4, 3, U8G2_DRAW_ALL);
  else if (rec_) u8g2_DrawCircle(g, 93, 4, 3, U8G2_DRAW_ALL);
  if (m_.value[P_BLE]) { u8g2_SetFont(g, F_TINY); u8g2_DrawStr(g, 100, 7, "BT"); }
  // battery: 3.3 V empty .. 4.15 V full
  float pct = (vbat_ - 3.3f) / 0.85f;
  pct = pct < 0 ? 0 : pct > 1 ? 1 : pct;
  u8g2_DrawFrame(g, 111, 1, 14, 7);
  u8g2_DrawBox(g, 125, 3, 2, 3);
  u8g2_DrawBox(g, 113, 3, (int)(10 * pct + 0.5f), 3);
  u8g2_DrawHLine(g, 0, 9, 128);
}

void MainApp::drawPadGrid(uint32_t now) {
  u8g2_t *g = p_.oled;
  u8g2_SetFont(g, F_TINY);
  const int len = m_.value[P_LEN] * 16;
  for (int pad = 0; pad < 16; pad++) {
    const int x = 63 + (pad % 4) * 16, y = 11 + (pad / 4) * 10;
    char lab[6] = "";
    bool fill = false, mark = false, head = false, held = false;
    if (view_ == V_SET) {
      settingsCell(pad, lab, fill, mark);
      held = (pads_ >> pad) & 1;
    } else if (view_ == V_SMP) {                     // recorded pads filled, the selected one underlined
      const int v = voiceForPad(pad);
      strcpy(lab, voiceName(v));
      fill = userFrames_[m_.drum[v][P_D_SMPL - P_D_PTCH] & 15] > 0;
      mark = v == m_.value[P_SPAD];
      held = (pads_ >> pad) & 1;
      if (pad == recShow_ && (recState_ == R_REC || beeping_) && ((now / 150) & 1)) fill = !fill;
    } else if (view_ == V_SEQ) {
      const int idx = seqPage_ * 16 + pad;
      const Step &st = track_ == T_DRUM ? m_.drumSteps[m_.value[P_SPAD]][idx] : m_.synthSteps[idx];
      fill = st.on;
      head = playing_ && curStep_ == idx;
      held = heldStep_ == idx;
      if (idx >= len) { u8g2_DrawPixel(g, x + 7, y + 4); continue; }   // beyond pattern length
    } else {
      const int v = voiceForPad(pad);
      fill = (pads_ >> pad) & 1;
      if (track_ == T_DRUM) {
        strcpy(lab, voiceName(v));
        fill = fill || now - flashMs_[v] < 90;
        mark = v == m_.value[P_SPAD];
      } else {
        noteName(synthNoteForPad(pad), lab);
        mark = degreeToSemis(m_.value[P_SCALE], v) % 12 == 0;   // root notes underlined
      }
    }
    if (fill) u8g2_DrawBox(g, x, y, 15, 9); else u8g2_DrawFrame(g, x, y, 15, 9);
    if (held) u8g2_DrawFrame(g, x - 1, y - 1, 17, 11);
    if (lab[0]) {
      u8g2_SetDrawColor(g, fill ? 0 : 1);
      u8g2_DrawStr(g, x + (15 - (int)strlen(lab) * 4) / 2 + 1, y + 7, lab);
      if (mark) u8g2_DrawHLine(g, x + 3, y + 7, 9);   // inside the frame, under the glyphs
      u8g2_SetDrawColor(g, 1);
    }
    if (head) { u8g2_SetDrawColor(g, 2); u8g2_DrawBox(g, x + 3, y + 2, 9, 5); u8g2_SetDrawColor(g, 1); }
  }
}

void MainApp::drawPlay(uint32_t now) {
  u8g2_t *g = p_.oled;
  char s[24];
  u8g2_SetFont(g, F_SMALL);
  u8g2_DrawStr(g, 0, 18, "PLAY");
  if (track_ == T_DRUM) {
    const int v = m_.value[P_SPAD];
    u8g2_SetFont(g, F_BIG);
    u8g2_DrawStr(g, 0, 33, voiceName(v));
    u8g2_SetFont(g, F_TINY);
    snprintf(s, sizeof s, "PAD %d", v + 1);
    u8g2_DrawStr(g, 0, 42, s);
  } else {
    u8g2_SetFont(g, F_BIG);
    const int shown = m_.value[P_ARP] && arpNote_ >= 0 ? arpNote_ : lastNote_;
    if (shown >= 0) noteName(shown, s); else strcpy(s, "--");
    u8g2_DrawStr(g, 0, 33, s);
    u8g2_SetFont(g, F_TINY);
    char r[6];
    fmtVal(P_ROOT, m_.value[P_ROOT], r);
    snprintf(s, sizeof s, "%s %s", r, SCALE_NAMES[m_.value[P_SCALE]]);
    u8g2_DrawStr(g, 0, 42, s);
  }
  u8g2_SetFont(g, F_TINY);
  if (mode_ == M_CTL) u8g2_DrawStr(g, 0, 50, "MIDI ONLY");
  else if (track_ == T_SYNTH && (m_.value[P_CHRD] || m_.value[P_ARP])) {
    snprintf(s, sizeof s, "%s%s%s", m_.value[P_CHRD] ? CHORD_NAMES[m_.value[P_CHRD]] : "",
             m_.value[P_CHRD] && m_.value[P_ARP] ? " " : "", m_.value[P_ARP] ? ARP_NAMES[m_.value[P_ARP]] : "");
    u8g2_DrawStr(g, 0, 50, s);
    if (m_.value[P_ARP] && m_.value[P_LTCH]) u8g2_DrawStr(g, 44, 42, "LTCH");
  }
  drawPadGrid(now);
}

void MainApp::drawSeq(uint32_t now) {
  u8g2_t *g = p_.oled;
  char s[24];
  u8g2_SetFont(g, F_SMALL);
  u8g2_DrawStr(g, 0, 18, "SEQ");
  u8g2_SetFont(g, F_BIG);
  u8g2_DrawStr(g, 0, 33, track_ == T_DRUM ? voiceName(m_.value[P_SPAD]) : "SYN");
  u8g2_SetFont(g, F_TINY);
  if (heldStep_ >= 0) snprintf(s, sizeof s, "STEP %d", heldStep_ + 1);
  else snprintf(s, sizeof s, "PAGE %d/%d", seqPage_ + 1, m_.value[P_LEN]);
  u8g2_DrawStr(g, 0, 42, s);
  for (int pg = 0; pg < 4; pg++) {                       // page boxes, playhead page underlined
    const int x = pg * 9;
    if (pg >= m_.value[P_LEN]) { u8g2_DrawPixel(g, x + 3, 47); continue; }
    if (pg == seqPage_) u8g2_DrawBox(g, x, 45, 7, 5); else u8g2_DrawFrame(g, x, 45, 7, 5);
    if (playing_ && curStep_ / 16 == pg) u8g2_DrawHLine(g, x, 51, 7);
  }
  drawPadGrid(now);
}

void MainApp::drawParams() {
  u8g2_t *g = p_.oled;
  const Page &pg = page();
  char s[32];
  u8g2_SetFont(g, F_SMALL);
  if (view_ == V_SOUND && track_ == T_DRUM) snprintf(s, sizeof s, "SOUND  PAD %s", voiceName(m_.value[P_SPAD]));
  else if (view_ == V_SOUND) snprintf(s, sizeof s, "SOUND %d/%d  %s", soundPage_ + 1, SYN_PAGES, pg.title);
  else snprintf(s, sizeof s, "%s", VIEW_NAMES[view_]);
  u8g2_DrawStr(g, 0, 18, s);
  for (int row = 0; row < 2; row++) {
    const int top = 21 + row * 21;
    const bool active = (row == 1) == shift_;
    for (int k = 0; k < 4; k++) {
      const uint8_t id = pg.slot[row * 4 + k];
      const int x = k * 32;
      if (id == P_NONE) continue;
      u8g2_SetFont(g, F_SMALL);
      if (active) { u8g2_DrawBox(g, x, top, 31, 8); u8g2_SetDrawColor(g, 0); }
      u8g2_DrawStr(g, x + 1, top + 7, PARAMS[id].label);
      u8g2_SetDrawColor(g, 1);
      const int v = getVal(id);
      fmtVal(id, v, s);
      u8g2_SetFont(g, F_MED);
      u8g2_DrawStr(g, x + 1, top + 17, s);
      const ParamDef &d = PARAMS[id];
      u8g2_DrawHLine(g, x, top + 19, 1 + (v - d.min) * 30 / (d.max - d.min));
    }
  }
}

void MainApp::drawKnobBar() {
  u8g2_t *g = p_.oled;
  const Page &pg = page();
  if (shift_) { u8g2_DrawBox(g, 0, 54, 128, 10); u8g2_SetDrawColor(g, 0); }
  else u8g2_DrawHLine(g, 0, 53, 128);
  u8g2_SetFont(g, F_SMALL);
  for (int k = 0; k < 4; k++) {
    const uint8_t id = pg.slot[k + (shift_ ? 4 : 0)];
    if (id == P_NONE) continue;
    const char *l = PARAMS[id].label;
    const int x = k * 32;
    u8g2_DrawStr(g, x + (32 - (int)strlen(l) * 5) / 2, 61, l);
    const ParamDef &d = PARAMS[id];
    u8g2_DrawHLine(g, x + 1, 63, 1 + (getVal(id) - d.min) * 29 / (d.max - d.min));
  }
  u8g2_SetDrawColor(g, 1);
}

void MainApp::drawReadout() {
  u8g2_t *g = p_.oled;
  const uint8_t id = page().slot[touchKnob_ + (shift_ ? 4 : 0)];
  if (id == P_NONE) return;
  const ParamDef &d = PARAMS[id];
  const int v = getVal(id);
  char s[16];
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawBox(g, 6, 13, 116, 38);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawFrame(g, 6, 13, 116, 38);
  u8g2_SetFont(g, F_SMALL);
  u8g2_DrawStr(g, 10, 22, d.name);
  fmtVal(id, v, s);
  u8g2_SetFont(g, F_BIG);
  u8g2_DrawStr(g, 10, 38, s);
  u8g2_DrawFrame(g, 10, 42, 108, 6);
  u8g2_DrawBox(g, 11, 43, 1 + (v - d.min) * 105 / (d.max - d.min), 4);
  if (!latched_[touchKnob_]) {
    const int kx = 11 + pot_[touchKnob_] * 105 / 4095;
    u8g2_DrawVLine(g, kx, 40, 10);
    u8g2_SetFont(g, F_TINY);
    const int target = d.min + (int)(pot_[touchKnob_] * (float)(d.max - d.min) / 4095.0f + 0.5f);
    u8g2_DrawStr(g, 82, 36, target < v ? "TURN >" : "< TURN");
  }
}

void MainApp::drawShiftMap() {
  static const char *const L[16] = {"PLAY", "SEQ", "SND", "FX", "PG1", "PG2", "PG3", "PG4",
                                    "DRUM", "SYN", "RUN", "REC", "STD", "CTL", "LAY", "SET"};
  u8g2_t *g = p_.oled;
  u8g2_SetFont(g, F_SMALL);
  for (int pad = 0; pad < 16; pad++) {
    const int x = (pad % 4) * 32, y = 11 + (pad / 4) * 13;
    bool on = false;
    switch (pad) {
      case 0: on = view_ == V_PLAY; break;
      case 1: on = view_ == V_SEQ; break;
      case 2: on = view_ == V_SOUND; break;
      case 3: on = view_ == V_FX; break;
      case 8: on = track_ == T_DRUM; break;
      case 9: on = track_ == T_SYNTH; break;
      case 10: on = playing_; break;
      case 11: on = rec_; break;
      case 15: on = view_ == V_SET; break;
      default:
        if (pad >= 4 && pad <= 7) on = seqPage_ == pad - 4;
        else if (pad >= 12) on = mode_ == pad - 12;
    }
    const char *l = pad == 10 && playing_ ? "STOP" : pad == 11 ? "REC/SMP" : L[pad];
    const int cw = pad == 11 ? 4 : 5;                 // REC: tap = record, hold = SAMPLE (small font to fit)
    if (on) { u8g2_DrawBox(g, x + 1, y, 30, 12); u8g2_SetDrawColor(g, 0); }
    else u8g2_DrawFrame(g, x + 1, y, 30, 12);
    if (pad == 11) u8g2_SetFont(g, F_TINY);
    u8g2_DrawStr(g, x + 1 + (30 - (int)strlen(l) * cw) / 2, y + (pad == 11 ? 8 : 9), l);
    if (pad == 11) u8g2_SetFont(g, F_SMALL);
    u8g2_SetDrawColor(g, 1);
  }
}

void MainApp::drawToast() {
  u8g2_t *g = p_.oled;
  u8g2_SetFont(g, F_MED);
  const int w = u8g2_GetStrWidth(g, toast_) + 12;
  const int x = (128 - w) / 2;
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawBox(g, x, 24, w, 17);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawFrame(g, x, 24, w, 17);
  u8g2_DrawStr(g, x + 6, 36, toast_);
}

const char *MainApp::status() {
  snprintf(status_, sizeof status_, "%s · %s · %s · %d BPM · %s%s%s", VIEW_NAMES[view_],
           track_ == T_DRUM ? "drum" : "synth", MODE_LONG[mode_], m_.value[P_BPM],
           playing_ ? "running" : "stopped", rec_ ? " · rec" : "",
           shift_ ? " · shift" : "");
  if (track_ == T_SYNTH && (m_.value[P_CHRD] || m_.value[P_ARP])) {
    char t[48];
    snprintf(t, sizeof t, " · chord %s · arp %s %s", CHORD_NAMES[m_.value[P_CHRD]], ARP_NAMES[m_.value[P_ARP]],
             m_.value[P_ARP] ? ARP_RATE_NAMES[m_.value[P_ARPR]] : "");
    strncat(status_, t, sizeof status_ - strlen(status_) - 1);
  }
  if (view_ == V_SMP)
    strncat(status_, recState_ == R_REC ? " · recording" : recState_ == R_ARMED ? " · waiting for sound" : beeping_ ? " · beep" : " · sampling",
            sizeof status_ - strlen(status_) - 1);
  if (playing_) {
    char t[24];
    snprintf(t, sizeof t, " · step %d/%d", curStep_ + 1, m_.value[P_LEN] * 16);
    strncat(status_, t, sizeof status_ - strlen(status_) - 1);
  }
  return status_;
}

// ================================================================ audio core
// The synth and sampler live in engine.cpp; this part runs the event queue, the sequencer clock and
// the arpeggiator with sample accuracy, and mixes to the I2S block.

bool MainApp::post(uint8_t type, uint8_t a, uint8_t b) {
  const uint8_t next = (qHead_ + 1) & 63;
  if (next == qTail_) return false;            // full: drop
  q_[qHead_] = {type, a, b};
  qHead_ = next;
  return true;
}

void MainApp::hitDrum(int v, int vel, int semis) {
  sampler_.trigger(v, vel, m_, semis);
  drumHit_[v] = drumHit_[v] + 1;
}

bool MainApp::chance(int prob) {
  if (prob >= 100) return true;
  dsp::noise(rng_);
  return (int)(rng_ % 100) < prob;
}

void MainApp::stepTick() {
  const int len = m_.value[P_LEN] * 16;
  curStep_ = (curStep_ + 1) % len;
  const int s = curStep_;
  const float base = SAMPLE_RATE * 60.0f / m_.value[P_BPM] / 4.0f;
  const float sw = m_.value[P_SWNG] / 100.0f;
  stepLen_ = (int32_t)(base * 2 * ((s & 1) ? 1 - sw : sw));
  samplesLeft_ = stepLen_;
  if (s == 0 && !extSync_) clkLeft_ = 0;              // MIDI clock out lines up with step 1
  if (!m_.value[P_MUTD])
    for (int v = 0; v < 16; v++) {
      const Step &st = m_.drumSteps[v][s];
      if (st.on && chance(st.prob)) {
        hitDrum(v, st.vel, st.note);                     // drum step NOTE = pitch offset in semitones
        midiNoteTimed(m_.value[P_DCH] - 1, GM_DRUM[v], st.vel, (int32_t)(base * st.gate / 100.0f));
      }
    }
  const Step &st = m_.synthSteps[s];
  if (!m_.value[P_MUTS] && st.on && chance(st.prob)) {
    int semis[4];
    const int n = chordSemis(m_.value[P_SCALE], m_.value[P_CHRD], m_.value[P_INV], st.note, semis);
    const int32_t gate = (int32_t)(base * st.gate / 100.0f);
    for (int i = 0; i < n; i++) {
      const int note = synthNoteForPad(-1) + semis[i];
      synth_.noteOn(note, st.vel, gate, m_);
      if (note >= 0 && note <= 127) midiNoteTimed(m_.value[P_MCH] - 1, note, st.vel, gate);
    }
  }
  // keep the arp on the grid while the sequencer runs (straight rates of a 16th or longer)
  const int r = m_.value[P_ARPR];
  if (arpN_ && r <= 2) {
    const int every = (int)ARP_RATE_16THS[r];
    if (s % every == 0) arpLeft_ = 0;
  }
}

int32_t MainApp::arpInterval() const {
  return (int32_t)(SAMPLE_RATE * 60.0f / m_.value[P_BPM] / 4.0f * ARP_RATE_16THS[m_.value[P_ARPR]]);
}

// One arp note. The pool holds notes in the order played; UP/DOWN/UP-DN/RAND use it sorted.
void MainApp::arpStep() {
  uint8_t notes[48];
  int n = 0;
  const int mode = m_.value[P_ARP], octs = m_.value[P_ARPO];
  uint8_t pool[16];
  for (int i = 0; i < arpN_; i++) pool[i] = arpPool_[i];
  if (mode != 5)                                   // sort unless ORDER
    for (int i = 1; i < arpN_; i++)
      for (int j = i; j > 0 && pool[j] < pool[j - 1]; j--) { const uint8_t t = pool[j]; pool[j] = pool[j - 1]; pool[j - 1] = t; }
  for (int o = 0; o < octs; o++)
    for (int i = 0; i < arpN_; i++)
      if (pool[i] + 12 * o <= 127) notes[n++] = pool[i] + 12 * o;
  if (!n) return;
  int idx;
  switch (mode) {
    case 2: idx = n - 1 - (int)(arpPos_ % n); break;                     // down
    case 3: {                                                            // up/down, ends not repeated
      const int period = n > 1 ? 2 * n - 2 : 1;
      const int p = (int)(arpPos_ % period);
      idx = p < n ? p : period - p;
    } break;
    case 4: dsp::noise(rng_); idx = (int)(rng_ % n); break;              // random
    default: idx = (int)(arpPos_ % n); break;                            // up, order
  }
  arpPos_++;
  const int32_t gate = (int32_t)(arpInterval() * m_.value[P_ARPG] / 100.0f);
  synth_.noteOn(notes[idx], 100, gate, m_);
  midiNoteTimed(m_.value[P_MCH] - 1, notes[idx], 100, gate);
  arpNote_ = notes[idx];
}

void MainApp::render(int16_t *buf, int frames) {
  while (qTail_ != qHead_) {                    // events from the control core
    const Event e = q_[qTail_];
    qTail_ = (qTail_ + 1) & 63;
    switch (e.type) {
      case E_DRUM: hitDrum(e.a, e.b); break;
      case E_NOTE_ON: synth_.noteOn(e.a, e.b, -1, m_); break;
      case E_NOTE_OFF: synth_.noteOff(e.a); break;
      case E_ARP_ADD:
        if (arpN_ < 16) {
          if (arpN_ == 0) { arpPos_ = 0; arpLeft_ = 0; }    // first note sounds right away
          arpPool_[arpN_++] = e.a;
        }
        break;
      case E_ARP_DEL:
        for (int i = 0; i < arpN_; i++)
          if (arpPool_[i] == e.a) { for (int j = i; j < arpN_ - 1; j++) arpPool_[j] = arpPool_[j + 1]; arpN_--; break; }
        if (!arpN_) arpNote_ = -1;
        break;
      case E_ARP_CLEAR: arpN_ = 0; arpNote_ = -1; break;
      case E_EXT_STEP: if (playing_) stepTick(); break;
      case E_SMP_SET: {                         // a new user sample (or none); the old buffer goes back to be freed
        const int16_t *old = sampler_.setUser(e.a & 15, newData_[e.a & 15], newLen_[e.a & 15]);
        const uint8_t next = (retHead_ + 1) & 31;
        if (old && next != retTail_) { retired_[retHead_] = old; retHead_ = next; }
      } break;
      default: synth_.releaseAll(); break;
    }
  }
  if (!playing_) { curStep_ = -1; samplesLeft_ = 0; }   // next run starts on step 1 immediately

  static float L[AUDIO_FRAMES], R[AUDIO_FRAMES], S[AUDIO_FRAMES];
  if (frames > AUDIO_FRAMES) frames = AUDIO_FRAMES;
  memset(L, 0, frames * sizeof(float));
  memset(R, 0, frames * sizeof(float));
  memset(S, 0, frames * sizeof(float));

  // Render in slices that end exactly on sequencer / arp ticks, so every hit lands on its sample.
  int i = 0;
  while (i < frames) {
    if (playing_ && samplesLeft_ <= 0 && !extSync_) stepTick();
    const bool arp = arpN_ && m_.value[P_ARP];
    if (arp && arpLeft_ <= 0) { arpLeft_ = arpInterval(); arpStep(); }
    int k = frames - i;
    if (playing_ && !extSync_ && samplesLeft_ < k) k = samplesLeft_;
    if (arp && arpLeft_ < k) k = arpLeft_;
    if (k < 1) k = 1;
    sampler_.render(L + i, R + i, k, m_);
    synth_.render(S + i, k, m_);
    if (playing_ && samplesLeft_ > 0) samplesLeft_ = samplesLeft_ - k;
    if (arp) arpLeft_ -= k;
    i += k;
  }

  midiAudioBlock(frames);                         // timed note-offs + clock out
  for (int n = 0; n < frames; n++) { L[n] += S[n]; R[n] += S[n]; }   // synth is mono, centre
  fx_.process(L, R, frames, m_);                // drive, crush, lofi, delay, low cut
  const float mvol = m_.value[P_MVOL] / 100.0f * 1.6f;
  const bool mute = mode_ == M_CTL || recState_ == R_REC || recState_ == R_ARMED;   // a take hears only the mic
  const bool mon = monValid_, off = muteOut_;
  if (beepReq_) { beepReq_ = false; beepPos_ = 0; }
  for (int n = 0; n < frames; n++) {
    // tanh-shaped limiter: linear for normal levels, rounds off only when several voices pile up
    float l = dsp::tanhish(L[n] * mvol);
    float r = dsp::tanhish(R[n] * mvol);
    if (mute) l = r = 0;
    if (mon) { l += mon_[n] * 0.7f; r += mon_[n] * 0.7f; }   // monitor: use headphones, or it feeds back
    if (beepPos_ >= 0) {                          // the count-in beep: 1 kHz with 5 ms ramps, over everything
      const int len = SAMPLE_RATE * BEEP_MS / 1000, ramp = SAMPLE_RATE / 200;
      const int k = beepPos_++, e = k < ramp ? k : len - k < ramp ? len - k : ramp;
      const float b = 0.4f * e / ramp * sinf(6.2831853f * 1000.0f * k / SAMPLE_RATE);
      l += b;
      r += b;
      if (beepPos_ >= len) beepPos_ = -1;
    }
    if (off) l = r = 0;
    buf[2 * n] = (int16_t)(l * 32000.0f);
    buf[2 * n + 1] = (int16_t)(r * 32000.0f);
  }
}

}  // namespace hugbox
