// XO-Hugbox main firmware app: UI (views, knob pages, shift layer, sequencer grid, modes), chord mode,
// arpeggiator and the sequencer clock, driving the synth + sampler engine in engine.cpp.
//
// Controls
//   pads              play (drum voices or synth notes in a scale) / toggle steps in SEQ
//   knobs             4 params of the current page; with shift held, 4 more (holding shift never changes the screen)
//   shift double-tap  shortcut map on screen; tap a pad to run its shortcut (shift tap / knob / 5 s closes it)
//   shift + pad       top row:    PLAY  SEQ  SOUND  FX       (SOUND again = next sound page)
//                     row 2:      seq page 1..4
//                     row 3:      DRUM track, SYNTH track, run/stop, record
//                     bottom row: Standalone, Controller (MIDI only), Layered, Settings
//   hold a step+knob  per-step velocity / pitch / gate / probability
//   SETTINGS pads     S1-S8 tap = load, hold = save | copy / paste / clear page, clear track (hold) |
//                     mute drums, mute synth, tap tempo, MIDI panic  (io.cpp)
#pragma once
#include "hugbox_app.h"
#include "engine.h"
#include "fx.h"
#include "model.h"

namespace hugbox {

extern const uint8_t GM_DRUM[16];   // General MIDI note per drum voice (io.cpp)

class MainApp : public App {
 public:
  void begin(const Platform &p) override;
  void loop(uint32_t nowMs, const Inputs &in) override;
  void render(int16_t *stereo, int frames) override;

  uint32_t frames() const { return frames_; }
  const char *status();   // one line for the emulator side panel
  int brightness() const { return m_.value[P_BRT]; }
  // Knob page as the device shows it now: slot 0-3 = knobs, 4-7 = with shift. Returns a ParamId or P_NONE.
  int knobParam(int slot) const { return slot >= 0 && slot < 8 ? page().slot[slot] : P_NONE; }
  // Current value of a parameter, formatted exactly like the OLED readout.
  void paramText(int id, char *out) const { if (id >= 0 && id < P_COUNT) fmtVal((uint8_t)id, getVal((uint8_t)id), out); else out[0] = 0; }
  int view() const { return view_; }
  int track() const { return track_; }
  // MIDI note a pad plays right now, or -1 when pads aren't synth notes (drum track, SEQ view). For the emulator's keycaps.
  int padNote(int pad) const { return track_ == T_SYNTH && view_ != V_SEQ && !mapOpen_ ? synthNoteForPad(pad) : -1; }
  // direct parameter access (tests, emulator, presets later); id = ParamId, clamped to its range
  void setParam(int id, int v) { if (id >= 0 && id < P_COUNT) setVal((uint8_t)id, v); }
  int param(int id) const { return id >= 0 && id < P_COUNT ? getVal((uint8_t)id) : 0; }
  void midiIn(MidiSource src, const uint8_t *msg, int len) override;
  bool bleWanted() const { return m_.value[P_BLE] != 0; }

 private:
  // ---- control core ----
  void padDown(int pad, uint32_t now);
  void padUp(int pad, uint32_t now);
  void shortcut(int pad, uint32_t now);
  void pollKnobs(uint32_t now);
  const Page &page() const;
  int getVal(uint8_t id) const;
  void setVal(uint8_t id, int v);
  void fmtVal(uint8_t id, int v, char *out) const;
  Step *heldStepPtr();
  int quantizedStep() const;
  void toast(const char *msg, uint32_t now);
  void logf(const char *fmt, ...);
  int synthNoteForPad(int pad) const;

  // ---- display ----
  void draw(uint32_t now);
  void drawStatus(uint32_t now);
  void drawPadGrid(uint32_t now);
  void drawPlay(uint32_t now);
  void drawSeq(uint32_t now);
  void drawParams();
  void drawKnobBar();
  void drawReadout();
  void drawShiftMap();
  void drawToast();
  void drawSettings(uint32_t now);

  // ---- projects + SETTINGS grid (io.cpp) ----
  uint32_t modelHash() const;
  bool loadSlot(int slot, uint32_t now);
  void saveSlot(int slot, uint32_t now);
  void autosave(uint32_t now);
  void scanSlots();
  void settingsPadDown(int pad, uint32_t now);
  void settingsPadUp(int pad, uint32_t now);
  void settingsHold(uint32_t now);
  bool settingsCell(int pad, char *lab, bool &fill, bool &mark) const;
  bool slotUsed_[9] = {};
  int curSlot_ = 0;                // last slot loaded/saved, 0 = none
  uint32_t autosaveHash_ = 0, pendingHash_ = 0, changedMs_ = 0, lastHashMs_ = 0;
  int setPad_ = -1;
  uint32_t setPadMs_ = 0;
  bool setPadDone_ = false;
  Step clipDrum_[16][16], clipSynth_[16];
  int clipTrack_ = -1;
  uint32_t tapMs_[4] = {};
  int tapN_ = 0;

  // ---- MIDI (io.cpp) ----
  bool midiOutOn() const { return mode_ != M_STD; }
  void midiSend(uint8_t status, uint8_t d1, uint8_t d2);     // control core
  void midiQueue(uint8_t status, uint8_t d1, uint8_t d2);    // audio core
  void midiNoteTimed(uint8_t ch, uint8_t note, uint8_t vel, int32_t samples);
  void midiAudioBlock(int frames);
  void midiDrain();
  void knobCC(int k, uint32_t now);
  struct MidiEv { uint8_t s, d1, d2; };
  MidiEv mq_[128];
  volatile uint8_t mqHead_ = 0, mqTail_ = 0;
  struct PendOff { uint8_t status, note; int32_t left; };
  PendOff offs_[32] = {};
  int32_t clkLeft_ = 0;
  volatile bool extSync_ = false;
  int extClk_ = 0;
  uint32_t lastClkMs_ = 0, lastLoopMs_ = 0;
  float clkAvgMs_ = 0;
  int lastCC_[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
  int padDrum_[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   // GM note sent per pad

  // ---- audio core ----
  struct Event { uint8_t type, a, b; };
  enum { E_DRUM, E_NOTE_ON, E_NOTE_OFF, E_ALL_OFF, E_ARP_ADD, E_ARP_DEL, E_ARP_CLEAR, E_EXT_STEP };
  void arpStep();
  void post(uint8_t type, uint8_t a = 0, uint8_t b = 0);
  void hitDrum(int voice, int vel, int semis = 0);
  bool chance(int prob);
  void stepTick();
  int32_t arpInterval() const;

  Platform p_;
  Model m_;

  // UI state
  View view_ = V_PLAY;
  Track track_ = T_DRUM;
  Mode mode_ = M_STD;
  int soundPage_ = 0;          // synth SOUND sub-page
  int seqPage_ = 0;            // SEQ page shown (0..3)
  bool rec_ = false;
  uint16_t pads_ = 0;
  bool shift_ = false, shiftUsed_ = false;
  uint32_t shiftDownMs_ = 0;
  bool mapOpen_ = false;       // shortcut map on screen (double-tap shift)
  uint32_t lastTapMs_ = 0, mapMs_ = 0;
  int heldStep_ = -1;          // absolute step index held in SEQ, -1 = none
  bool heldStepWasOn_ = false, heldStepEdited_ = false;
  uint8_t padNotes_[16][4];    // synth notes (chord) each held pad started
  uint8_t padNoteN_[16] = {};  // how many, 0 = none
  bool padArp_[16] = {};       // those notes went to the arpeggiator instead of straight to voices
  int lastNote_ = -1;
  int lastArp_ = 0, lastLatch_ = 0;
  void releasePad(int pad);

  // knobs
  uint16_t pot_[4] = {}, accepted_[4] = {};
  uint32_t knobKey_[4] = {};
  bool latched_[4] = {};
  int prevTarget_[4] = {-1, -1, -1, -1};
  int touchKnob_ = -1;         // knob shown in the readout
  uint32_t touchMs_ = 0;
  bool potsInit_ = false;

  float vbat_ = 0;
  uint32_t lastScan_ = 0, lastPots_ = 0, lastDraw_ = 0, frames_ = 0;
  char toast_[24] = "";
  uint32_t toastMs_ = 0;
  char status_[96];
  int lastBrt_ = -1;

  // shared with the audio core
  volatile bool playing_ = false;
  volatile int curStep_ = -1;
  volatile int32_t samplesLeft_ = 0, stepLen_ = 1;
  volatile uint32_t drumHit_[16] = {};    // trigger counters, the UI flashes pads on change
  uint32_t seenHit_[16] = {}, flashMs_[16] = {};
  Event q_[64];
  volatile uint8_t qHead_ = 0, qTail_ = 0;

  // audio-core state
  Synth synth_;
  Sampler sampler_;
  Fx fx_;
  size_t kitBytes_ = 0;
  uint32_t lastFrameHash_ = 0;
  uint32_t rng_ = 0x1234567u;
  // arpeggiator (audio core): note pool in the order played
  uint8_t arpPool_[16];
  int arpN_ = 0;
  uint32_t arpPos_ = 0;
  int32_t arpLeft_ = 0;
  volatile int arpNote_ = -1;    // last arp note, for the display
  bool wasPlaying_ = false;
};

}  // namespace hugbox
