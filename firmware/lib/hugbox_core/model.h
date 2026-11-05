// XO-Hugbox - shared device state: parameters, knob pages, sequencer data.
// Plain data, no platform calls. The UI (control core) writes it; the audio engine (audio core) reads it.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hugbox {

enum View : uint8_t { V_PLAY, V_SEQ, V_SOUND, V_FX, V_SET, V_COUNT };
enum Track : uint8_t { T_DRUM, T_SYNTH };
enum Mode : uint8_t { M_STD, M_CTL, M_LAY };   // Standalone, Controller (MIDI only), Layered (both)

enum Fmt : uint8_t { F_NUM, F_PCT, F_WAVE, F_SCALE, F_NOTE, F_ONOFF, F_SYNC, F_SIGNED, F_PAN, F_LEN, F_PAD, F_BITS,
                     F_SEMI, F_MS, F_RATIO, F_CHORD, F_ARP, F_ARPRATE, F_DIV, F_HZ };

// ---- parameter ids ----
enum ParamId : uint8_t {
  // synth patch
  P_W1, P_W2, P_DTUN, P_MIX, P_FM, P_FMR, P_DRFT, P_GLID,
  P_CUT, P_RES, P_ENV, P_SVOL, P_ATK, P_DCY, P_SUS, P_REL,
  P_OCT, P_SCALE, P_ROOT,
  // chord + arpeggiator (synth track)
  P_CHRD, P_INV, P_ARP, P_ARPR, P_ARPO, P_ARPG, P_LTCH,
  // drum pad (value lives per pad, see Model::drum)
  P_D_PTCH, P_D_DCY, P_D_CUT, P_D_LVL, P_D_SMPL, P_D_PAN, P_D_CHOK,
  P_KITV,
  // sequencer / transport
  P_SPAD, P_BPM, P_SWNG, P_LEN,
  // per step (value lives in the held step)
  P_ST_VEL, P_ST_NOTE, P_ST_GATE, P_ST_PROB,
  // fx / master
  P_DTIM, P_DFB, P_DMIX, P_DRV, P_CRSH, P_RATE, P_SHPF, P_MVOL,
  // settings
  P_MCH, P_SYNC, P_BLE, P_BRT,
  P_DCH,                         // drum MIDI channel
  P_MUTD, P_MUTS,                // sequencer track mutes (no knob; SETTINGS pad grid)
  P_COUNT,
  P_NONE = 0xFF
};

struct ParamDef {
  const char *label;   // 4 chars max, knob label
  const char *name;    // long name for the readout
  int16_t min, max, def;
  Fmt fmt;
};

extern const ParamDef PARAMS[P_COUNT];

// A knob page: 4 params on the knobs, 4 more with shift held.
struct Page {
  const char *title;
  uint8_t slot[8];
};

constexpr int DRUM_PARAMS = 7;     // P_D_PTCH .. P_D_CHOK
constexpr int STEPS = 64;          // 4 pages x 16

struct Step {
  bool on = false;
  uint8_t vel = 100;    // 1..127
  int8_t note = 0;      // drum: pitch offset in semitones; synth: scale degree from root
  uint8_t gate = 50;    // % of a step
  uint8_t prob = 100;   // %
};

struct Model {
  int16_t value[P_COUNT];                 // global params (drum and step params are stored below)
  int16_t drum[16][DRUM_PARAMS];          // per drum voice
  Step drumSteps[16][STEPS];              // per drum voice
  Step synthSteps[STEPS];

  void reset();

  // Project file: "HUGB" magic, version, param count, then values, drum pads and all steps (little-endian, packed).
  // load() copies what both sides know (older/newer param counts are fine) and clamps every value to its range.
  // includeSettings = false keeps the device settings (MIDI channels, sync, BLE, brightness) as they are.
  static constexpr size_t PROJECT_MAX = 8192;
  size_t save(uint8_t *buf, size_t max) const;
  bool load(const uint8_t *buf, size_t len, bool includeSettings);
};

extern const char *const DRUM_NAMES[16];   // 3-letter placeholder kit names
extern const char *const SCALE_NAMES[5];
extern const char *const CHORD_NAMES[6];
extern const char *const ARP_NAMES[6];
extern const char *const ARP_RATE_NAMES[6];
extern const float ARP_RATE_16THS[6];   // arp interval in 16th notes

// Notes of a chord on scale degree `degree` (semitones above the root note), sorted low to high.
// Returns the count (1 when chords are off).
int chordSemis(int scale, int chord, int inversion, int degree, int out[4]);
extern const uint8_t SCALE_LEN[5];
extern const uint8_t SCALES[5][12];

// Scale degree (0 = root) -> semitones above root.
int degreeToSemis(int scale, int degree);
// "C#4" style note name for a MIDI note.
void noteName(int midi, char *out);

}  // namespace hugbox
