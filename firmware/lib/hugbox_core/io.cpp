// XO-Hugbox: projects (autosave + 8 slots), the SETTINGS pad grid (slots, copy/paste/clear, mutes, tap tempo,
// MIDI panic) and MIDI in/out (notes, knob CCs in Controller mode, clock in/out). Part of MainApp.
#include <stdio.h>
#include <string.h>
#include "main_app.h"

namespace hugbox {

// General MIDI drum notes for the 16 voices, in DRUM_NAMES order (KCK SNR CLP CHH OHH TML TMH RIM COW SHK CRS RID PC1 PC2 ZAP 808)
const uint8_t GM_DRUM[16] = {36, 38, 39, 42, 46, 45, 50, 37, 56, 70, 49, 51, 63, 75, 81, 35};

// SETTINGS pad grid (pad n = row*4 + col, top row first)
enum SetPad { SP_SLOT1 = 0, SP_SLOT8 = 7, SP_COPY = 8, SP_PASTE, SP_CLRPG, SP_CLRTRK, SP_MUTD, SP_MUTS, SP_TAP, SP_PANIC };
static const char *const SET_LABELS[16] = {"S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8",
                                           "CPY", "PST", "CLR", "CLT", "MDR", "MSY", "TAP", "PNC"};
static constexpr uint32_t HOLD_MS = 800;

// ================================================================ projects
uint32_t MainApp::modelHash() const {
  uint32_t h = 2166136261u;
  const uint8_t *p = (const uint8_t *)&m_;
  for (size_t i = 0; i < sizeof m_; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}

bool MainApp::loadSlot(int slot, uint32_t now) {
  if (!p_.loadProject) return false;
  static uint8_t buf[Model::PROJECT_MAX];
  const size_t n = p_.loadProject(slot, buf, sizeof buf);
  if (!n || !m_.load(buf, n, slot == 0)) return false;
  post(E_ALL_OFF);
  post(E_ARP_CLEAR);
  autosaveHash_ = modelHash();
  if (slot) { char t[24]; snprintf(t, sizeof t, "LOADED S%d", slot); toast(t, now); curSlot_ = slot; }
  logf("project %s loaded (%u bytes)\n", slot ? "slot" : "autosave", (unsigned)n);
  return true;
}

void MainApp::saveSlot(int slot, uint32_t now) {
  if (!p_.saveProject) return;
  static uint8_t buf[Model::PROJECT_MAX];
  const size_t n = m_.save(buf, sizeof buf);
  const bool ok = n && p_.saveProject(slot, buf, n);
  if (slot) {
    char t[24];
    snprintf(t, sizeof t, ok ? "SAVED S%d" : "SAVE FAILED", slot);
    toast(t, now);
    if (ok) { slotUsed_[slot] = true; curSlot_ = slot; }
  }
  if (ok && slot == 0) autosaveHash_ = modelHash();
}

// Autosave: 3 s after the last change, write slot 0. Checked once a second (hashing ~6 KB is cheap).
void MainApp::autosave(uint32_t now) {
  if (!p_.saveProject || now - lastHashMs_ < 1000) return;
  lastHashMs_ = now;
  const uint32_t h = modelHash();
  if (h != pendingHash_) { pendingHash_ = h; changedMs_ = now; return; }
  if (h != autosaveHash_ && now - changedMs_ >= 3000) saveSlot(0, now);
}

void MainApp::scanSlots() {
  if (!p_.loadProject) return;
  static uint8_t hdr[8];
  for (int s = 1; s <= 8; s++) slotUsed_[s] = p_.loadProject(s, hdr, sizeof hdr) > 0;
}

// ================================================================ SETTINGS pad grid
void MainApp::settingsPadDown(int pad, uint32_t now) {
  setPad_ = pad;
  setPadMs_ = now;
  setPadDone_ = false;
  char t[24];
  switch (pad) {
    case SP_COPY:
      for (int v = 0; v < 16; v++) for (int s = 0; s < 16; s++) clipDrum_[v][s] = m_.drumSteps[v][seqPage_ * 16 + s];
      for (int s = 0; s < 16; s++) clipSynth_[s] = m_.synthSteps[seqPage_ * 16 + s];
      clipTrack_ = track_;
      snprintf(t, sizeof t, "COPIED %s P%d", track_ == T_DRUM ? "DRUM" : "SYN", seqPage_ + 1);
      toast(t, now);
      break;
    case SP_PASTE:
      if (clipTrack_ < 0) { toast("NOTHING COPIED", now); break; }
      if (clipTrack_ == T_DRUM) for (int v = 0; v < 16; v++) for (int s = 0; s < 16; s++) m_.drumSteps[v][seqPage_ * 16 + s] = clipDrum_[v][s];
      else for (int s = 0; s < 16; s++) m_.synthSteps[seqPage_ * 16 + s] = clipSynth_[s];
      snprintf(t, sizeof t, "PASTED TO P%d", seqPage_ + 1);
      toast(t, now);
      break;
    case SP_CLRPG:
      if (track_ == T_DRUM) for (int v = 0; v < 16; v++) for (int s = 0; s < 16; s++) m_.drumSteps[v][seqPage_ * 16 + s] = Step();
      else for (int s = 0; s < 16; s++) m_.synthSteps[seqPage_ * 16 + s] = Step();
      snprintf(t, sizeof t, "CLEARED P%d", seqPage_ + 1);
      toast(t, now);
      break;
    case SP_CLRTRK: toast("HOLD TO CLEAR", now); break;
    case SP_MUTD: m_.value[P_MUTD] = !m_.value[P_MUTD]; toast(m_.value[P_MUTD] ? "DRUMS MUTED" : "DRUMS ON", now); break;
    case SP_MUTS: m_.value[P_MUTS] = !m_.value[P_MUTS]; toast(m_.value[P_MUTS] ? "SYNTH MUTED" : "SYNTH ON", now); break;
    case SP_TAP: {
      // tap tempo: average of the last taps that are 0.25-2 s apart
      if (tapN_ && (now - tapMs_[(tapN_ - 1) % 4] > 2000)) tapN_ = 0;
      tapMs_[tapN_ % 4] = now;
      tapN_++;
      if (tapN_ >= 2) {
        const int n = tapN_ < 4 ? tapN_ : 4;
        const uint32_t first = tapMs_[(tapN_ - n) % 4], last = tapMs_[(tapN_ - 1) % 4];
        const int bpm = (int)(60000.0f * (n - 1) / (last - first) + 0.5f);
        setVal(P_BPM, bpm);
        snprintf(t, sizeof t, "TAP %d BPM", m_.value[P_BPM]);
        toast(t, now);
      } else toast("TAP AGAIN", now);
    } break;
    case SP_PANIC:
      post(E_ALL_OFF);
      post(E_ARP_CLEAR);
      for (int ch = 0; ch < 16; ch++) { midiSend(0xB0 | ch, 123, 0); midiSend(0xB0 | ch, 120, 0); }
      toast("ALL NOTES OFF", now);
      break;
    default:                                         // slots: decided on release (tap) or after HOLD_MS (save)
      break;
  }
}

void MainApp::settingsPadUp(int pad, uint32_t now) {
  if (pad != setPad_) return;
  if (!setPadDone_ && pad >= SP_SLOT1 && pad <= SP_SLOT8) {
    const int slot = pad + 1;
    if (!slotUsed_[slot]) toast("EMPTY - HOLD TO SAVE", now);
    else if (!loadSlot(slot, now)) toast("LOAD FAILED", now);
  }
  setPad_ = -1;
}

// long presses: save a slot, clear the track
void MainApp::settingsHold(uint32_t now) {
  if (setPad_ < 0 || setPadDone_ || now - setPadMs_ < HOLD_MS) return;
  if (setPad_ >= SP_SLOT1 && setPad_ <= SP_SLOT8) { saveSlot(setPad_ + 1, now); setPadDone_ = true; }
  else if (setPad_ == SP_CLRTRK) {
    if (track_ == T_DRUM) for (int v = 0; v < 16; v++) for (int s = 0; s < STEPS; s++) m_.drumSteps[v][s] = Step();
    else for (int s = 0; s < STEPS; s++) m_.synthSteps[s] = Step();
    toast(track_ == T_DRUM ? "DRUM TRACK CLEARED" : "SYNTH TRACK CLEARED", now);
    setPadDone_ = true;
  }
}

bool MainApp::settingsCell(int pad, char *lab, bool &fill, bool &mark) const {
  strcpy(lab, SET_LABELS[pad]);
  fill = mark = false;
  if (pad <= SP_SLOT8) { fill = slotUsed_[pad + 1]; mark = curSlot_ == pad + 1; }
  else if (pad == SP_MUTD) fill = m_.value[P_MUTD];
  else if (pad == SP_MUTS) fill = m_.value[P_MUTS];
  else if (pad == SP_PASTE) fill = false, mark = clipTrack_ >= 0;
  return true;
}

void MainApp::drawSettings(uint32_t now) {
  u8g2_t *g = p_.oled;
  char s[24];
  u8g2_SetFont(g, u8g2_font_5x7_tr);
  u8g2_DrawStr(g, 0, 18, "PROJECT");
  u8g2_SetFont(g, u8g2_font_helvB10_tr);
  if (curSlot_) snprintf(s, sizeof s, "S%d", curSlot_); else strcpy(s, "NEW");
  u8g2_DrawStr(g, 0, 33, s);
  u8g2_SetFont(g, u8g2_font_4x6_tr);
  if (setPad_ >= SP_SLOT1 && setPad_ <= SP_SLOT8 && !setPadDone_) {
    const int w = (int)((now - setPadMs_) * 58 / HOLD_MS);          // hold-to-save progress
    u8g2_DrawFrame(g, 0, 37, 60, 5);
    u8g2_DrawBox(g, 1, 38, w > 58 ? 58 : w, 3);
    u8g2_DrawStr(g, 0, 50, "HOLD = SAVE");
  } else {
    u8g2_DrawStr(g, 0, 42, "TAP SLOT = LOAD");
    u8g2_DrawStr(g, 0, 50, "HOLD SLOT = SAVE");
  }
  drawPadGrid(now);
}

// ================================================================ MIDI
void MainApp::midiSend(uint8_t status, uint8_t d1, uint8_t d2) {
  if (!p_.midiOut) return;
  const uint8_t msg[3] = {status, d1, d2};
  const uint8_t hi = status & 0xF0;
  const int len = status >= 0xF8 ? 1 : (hi == 0xC0 || hi == 0xD0) ? 2 : 3;
  p_.midiOut(msg, len);
}

// audio core -> control core (the platform's MIDI send is not safe to call from the audio task)
void MainApp::midiQueue(uint8_t status, uint8_t d1, uint8_t d2) {
  const uint8_t next = (mqHead_ + 1) & 127;
  if (next == mqTail_) return;
  mq_[mqHead_] = {status, d1, d2};
  mqHead_ = next;
}

void MainApp::midiNoteTimed(uint8_t ch, uint8_t note, uint8_t vel, int32_t samples) {
  if (!midiOutOn()) return;
  midiQueue(0x90 | ch, note, vel);
  for (auto &o : offs_)
    if (o.left <= 0) { o.status = 0x80 | ch; o.note = note; o.left = samples > 64 ? samples : 64; return; }
  midiQueue(0x80 | ch, note, 0);                     // table full: end the note right away
}

void MainApp::midiAudioBlock(int frames) {
  for (auto &o : offs_)
    if (o.left > 0 && (o.left -= frames) <= 0) midiQueue(o.status, o.note, 0);
  // MIDI clock out (24 per quarter) while running on the internal clock
  if (playing_ && !extSync_ && midiOutOn()) {
    clkLeft_ -= frames;
    const int32_t iv = (int32_t)(SAMPLE_RATE * 60.0f / m_.value[P_BPM] / 24.0f);
    while (clkLeft_ <= 0) { midiQueue(0xF8, 0, 0); clkLeft_ += iv; }
  }
}

void MainApp::midiDrain() {
  while (mqTail_ != mqHead_) {
    const MidiEv e = mq_[mqTail_];
    mqTail_ = (mqTail_ + 1) & 127;
    midiSend(e.s, e.d1, e.d2);
  }
}

void MainApp::midiIn(MidiSource src, const uint8_t *msg, int len) {
  if (len < 1) return;
  const uint8_t st = msg[0];
  const bool clockSrc = (m_.value[P_SYNC] == 1 && src == MIDI_SRC_USB) || (m_.value[P_SYNC] == 2 && src == MIDI_SRC_BLE);
  if (st >= 0xF8) {                                  // realtime
    if (!clockSrc) return;
    const uint32_t now = lastLoopMs_;
    switch (st) {
      case 0xF8:                                     // clock: 6 per 16th step
        if (playing_ && extClk_ % 6 == 0) post(E_EXT_STEP);
        extClk_++;
        if (lastClkMs_) {
          const uint32_t dt = now - lastClkMs_;
          if (dt >= 2 && dt <= 120) clkAvgMs_ = clkAvgMs_ > 0 ? clkAvgMs_ * 0.92f + dt * 0.08f : (float)dt;
        }
        lastClkMs_ = now;
        if (extClk_ % 24 == 0 && clkAvgMs_ > 0) setVal(P_BPM, (int)(60000.0f / (clkAvgMs_ * 24.0f) + 0.5f));
        break;
      case 0xFA: extClk_ = 0; playing_ = true; break;     // start: the next clock is step 1
      case 0xFB: playing_ = true; break;                  // continue
      case 0xFC: playing_ = false; break;                 // stop
    }
    return;
  }
  if (len < 3) return;
  const uint8_t hi = st & 0xF0, ch = st & 0x0F;
  const bool on = hi == 0x90 && msg[2] > 0, off = hi == 0x80 || (hi == 0x90 && msg[2] == 0);
  if (!on && !off) return;
  if (ch == m_.value[P_MCH] - 1) {
    post(on ? E_NOTE_ON : E_NOTE_OFF, msg[1], msg[2]);
    if (on) lastNote_ = msg[1];
  } else if (ch == m_.value[P_DCH] - 1 && on) {
    for (int v = 0; v < 16; v++) if (GM_DRUM[v] == msg[1]) { post(E_DRUM, v, msg[2]); break; }
  }
}

// Controller mode: knobs send CC 70-73 (shift: 74-77), 0-127, instead of editing the internal sound.
void MainApp::knobCC(int k, uint32_t now) {
  const int slot = k + (shift_ ? 4 : 0);
  const int v = pot_[k] >> 5;
  if (v == lastCC_[slot]) return;
  lastCC_[slot] = v;
  midiSend(0xB0 | (m_.value[P_MCH] - 1), 70 + slot, v);
  char t[24];
  snprintf(t, sizeof t, "CC%d %d", 70 + slot, v);
  toast(t, now);
}

}  // namespace hugbox
