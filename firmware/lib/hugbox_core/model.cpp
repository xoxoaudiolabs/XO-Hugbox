#include "model.h"
#include <stdio.h>
#include <string.h>

namespace hugbox {

// label, name, min, max, default, format
const ParamDef PARAMS[P_COUNT] = {
  {"WAV1", "Osc 1 wave", 0, 3, 1, F_WAVE},
  {"WAV2", "Osc 2 wave", 0, 3, 2, F_WAVE},
  {"DTUN", "Osc 2 detune", -24, 24, 0, F_SEMI},
  {"MIX", "Osc 1/2 mix", 0, 100, 30, F_PCT},
  {"FM", "FM amount", 0, 100, 0, F_PCT},
  {"RATI", "FM ratio", 1, 16, 2, F_RATIO},
  {"DRFT", "Analog drift", 0, 100, 15, F_PCT},
  {"GLID", "Glide", 0, 100, 0, F_PCT},
  {"CUT", "Filter cutoff", 0, 100, 60, F_PCT},
  {"RES", "Resonance", 0, 100, 20, F_PCT},
  {"ENV", "Filter env amount", -100, 100, 40, F_SIGNED},
  {"VOL", "Synth volume", 0, 100, 70, F_PCT},
  {"ATK", "Attack", 0, 100, 2, F_MS},
  {"DCY", "Decay", 0, 100, 30, F_MS},
  {"SUS", "Sustain", 0, 100, 60, F_PCT},
  {"REL", "Release", 0, 100, 25, F_MS},
  {"OCT", "Octave", 1, 7, 4, F_NUM},
  {"SCAL", "Scale", 0, 4, 2, F_SCALE},
  {"ROOT", "Root note", 0, 11, 0, F_NOTE},
  {"CHRD", "Chord", 0, 5, 0, F_CHORD},
  {"INV", "Inversion", 0, 3, 0, F_NUM},
  {"ARP", "Arpeggiator", 0, 5, 0, F_ARP},
  {"RATE", "Arp rate", 0, 5, 2, F_ARPRATE},
  {"OCTS", "Arp octaves", 1, 3, 1, F_NUM},
  {"AGAT", "Arp gate", 10, 100, 50, F_PCT},
  {"LTCH", "Arp latch", 0, 1, 0, F_ONOFF},
  {"PTCH", "Pad pitch", -24, 24, 0, F_SEMI},
  {"DCY", "Pad decay", 0, 100, 50, F_PCT},
  {"CUT", "Pad filter", 0, 100, 100, F_PCT},
  {"LVL", "Pad level", 0, 100, 80, F_PCT},
  {"SMPL", "Pad sample", 0, 15, 0, F_NUM},
  {"PAN", "Pad pan", -50, 50, 0, F_PAN},
  {"CHOK", "Choke group", 0, 4, 0, F_NUM},
  {"KIT", "Kit volume", 0, 100, 80, F_PCT},
  {"PAD", "Sequenced pad", 0, 15, 0, F_PAD},
  {"BPM", "Tempo", 40, 240, 120, F_NUM},
  {"SWNG", "Swing", 50, 75, 50, F_PCT},
  {"LEN", "Pattern length", 1, 4, 1, F_LEN},
  {"VEL", "Step velocity", 1, 127, 100, F_NUM},
  {"NOTE", "Step pitch", -12, 24, 0, F_SEMI},
  {"GATE", "Step gate", 5, 100, 50, F_PCT},
  {"PROB", "Step probability", 0, 100, 100, F_PCT},
  {"TIME", "Delay time (synced)", 0, 9, 5, F_DIV},
  {"FDBK", "Delay feedback", 0, 95, 35, F_PCT},
  {"DLY", "Delay mix", 0, 100, 0, F_PCT},
  {"DRV", "Drive", 0, 100, 0, F_PCT},
  {"CRSH", "Bit depth", 4, 16, 16, F_BITS},
  {"LOFI", "Sample rate reduction", 0, 100, 0, F_PCT},
  {"LCUT", "Low cut (both outputs)", 0, 100, 0, F_HZ},
  {"MVOL", "Master volume", 0, 100, 70, F_PCT},
  {"MCH", "MIDI channel", 1, 16, 1, F_NUM},
  {"SYNC", "Clock source", 0, 2, 0, F_SYNC},
  {"BLE", "Bluetooth MIDI", 0, 1, 1, F_ONOFF},
  {"BRT", "Screen brightness", 0, 100, 80, F_PCT},
  {"DCH", "Drum MIDI channel", 1, 16, 10, F_NUM},
  {"MUTD", "Mute drum track", 0, 1, 0, F_ONOFF},
  {"MUTS", "Mute synth track", 0, 1, 0, F_ONOFF},
};

const char *const DRUM_NAMES[16] = {"KCK", "SNR", "CLP", "CHH", "OHH", "TML", "TMH", "RIM",
                                    "COW", "SHK", "CRS", "RID", "PC1", "PC2", "ZAP", "808"};
const char *const SCALE_NAMES[5] = {"CHROM", "MAJOR", "MINOR", "PENTA", "DORIAN"};
const char *const CHORD_NAMES[6] = {"OFF", "5TH", "TRIAD", "7TH", "SUS2", "SUS4"};
const char *const ARP_NAMES[6] = {"OFF", "UP", "DOWN", "UP/DN", "RAND", "ORDER"};
const char *const ARP_RATE_NAMES[6] = {"1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"};
const float ARP_RATE_16THS[6] = {4.0f, 2.0f, 1.0f, 0.5f, 4.0f / 3.0f, 2.0f / 3.0f};
const uint8_t SCALE_LEN[5] = {12, 7, 7, 5, 7};
const uint8_t SCALES[5][12] = {
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
  {0, 2, 4, 5, 7, 9, 11},
  {0, 2, 3, 5, 7, 8, 10},
  {0, 3, 5, 7, 10},
  {0, 2, 3, 5, 7, 9, 10},
};

int degreeToSemis(int scale, int degree) {
  const int n = SCALE_LEN[scale];
  int oct = 0;
  while (degree < 0) { degree += n; oct--; }
  oct += degree / n;
  return oct * 12 + SCALES[scale][degree % n];
}

int chordSemis(int scale, int chord, int inversion, int degree, int out[4]) {
  // diatonic chords stack scale degrees; the chromatic "scale" uses fixed major-family intervals
  static const int8_t DEG[6][4] = {{0}, {0, 4}, {0, 2, 4}, {0, 2, 4, 6}, {0, 1, 4}, {0, 3, 4}};
  static const int8_t SEMI[6][4] = {{0}, {0, 7}, {0, 4, 7}, {0, 4, 7, 10}, {0, 2, 7}, {0, 5, 7}};
  static const uint8_t N[6] = {1, 2, 3, 4, 3, 3};
  const int n = N[chord];
  const int base = degreeToSemis(scale, degree);
  for (int i = 0; i < n; i++)
    out[i] = scale == 0 ? base + SEMI[chord][i] : degreeToSemis(scale, degree + DEG[chord][i]);
  for (int k = 0; k < inversion % n; k++) {           // inversion: lowest note up an octave
    const int lo = out[0];
    for (int i = 0; i < n - 1; i++) out[i] = out[i + 1];
    out[n - 1] = lo + 12;
  }
  return n;
}

void noteName(int midi, char *out) {
  static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  sprintf(out, "%s%d", N[((midi % 12) + 12) % 12], midi / 12 - 1);
}

void Model::reset() {
  for (int i = 0; i < P_COUNT; i++) value[i] = PARAMS[i].def;
  for (int v = 0; v < 16; v++) {
    for (int k = 0; k < DRUM_PARAMS; k++) drum[v][k] = PARAMS[P_D_PTCH + k].def;
    drum[v][P_D_SMPL - P_D_PTCH] = v;
    for (int s = 0; s < STEPS; s++) drumSteps[v][s] = Step();
  }
  drum[3][P_D_CHOK - P_D_PTCH] = 1;   // closed hat chokes open hat
  drum[4][P_D_CHOK - P_D_PTCH] = 1;
  for (int s = 0; s < STEPS; s++) synthSteps[s] = Step();
  // a starter beat so the sequencer does something on first press of play
  for (int s = 0; s < 16; s += 4) drumSteps[0][s].on = true;
  drumSteps[1][4].on = drumSteps[1][12].on = true;
  for (int s = 2; s < 16; s += 4) drumSteps[3][s].on = true;
}

// ---------------------------------------------------------------- project files
namespace {
struct Writer {
  uint8_t *b; size_t max, n = 0; bool ok = true;
  void u8(uint8_t v) { if (n < max) b[n++] = v; else ok = false; }
  void i16(int v) { u8((uint8_t)(v & 0xFF)); u8((uint8_t)((v >> 8) & 0xFF)); }
};
struct Reader {
  const uint8_t *b; size_t len, n = 0; bool ok = true;
  uint8_t u8() { if (n < len) return b[n++]; ok = false; return 0; }
  int i16() { const int lo = u8(), hi = u8(); return (int16_t)(lo | (hi << 8)); }
};
constexpr uint8_t VERSION = 1;
bool isSetting(int id) { return id == P_MCH || id == P_SYNC || id == P_BLE || id == P_BRT || id == P_DCH; }
int clampTo(int id, int v) { return v < PARAMS[id].min ? PARAMS[id].min : v > PARAMS[id].max ? PARAMS[id].max : v; }
}  // namespace

size_t Model::save(uint8_t *buf, size_t max) const {
  Writer w{buf, max};
  w.u8('H'); w.u8('U'); w.u8('G'); w.u8('B'); w.u8(VERSION);
  w.u8(P_COUNT);
  for (int i = 0; i < P_COUNT; i++) w.i16(value[i]);
  w.u8(DRUM_PARAMS);
  for (int v = 0; v < 16; v++) for (int k = 0; k < DRUM_PARAMS; k++) w.i16(drum[v][k]);
  w.u8(STEPS);
  auto step = [&](const Step &s) { w.u8(s.on); w.u8(s.vel); w.u8((uint8_t)s.note); w.u8(s.gate); w.u8(s.prob); };
  for (int v = 0; v < 16; v++) for (int s = 0; s < STEPS; s++) step(drumSteps[v][s]);
  for (int s = 0; s < STEPS; s++) step(synthSteps[s]);
  return w.ok ? w.n : 0;
}

bool Model::load(const uint8_t *buf, size_t len, bool includeSettings) {
  Reader r{buf, len};
  if (r.u8() != 'H' || r.u8() != 'U' || r.u8() != 'G' || r.u8() != 'B') return false;
  if (r.u8() != VERSION) return false;
  static Model m;                                    // ~6 KB: static, not on the UI task's stack
  m = *this;                                         // start from the current state, overwrite what the file has
  const int np = r.u8();
  for (int i = 0; i < np; i++) {
    const int v = r.i16();
    if (i < P_COUNT && (includeSettings || !isSetting(i))) m.value[i] = clampTo(i, v);
  }
  const int nd = r.u8();
  for (int v = 0; v < 16; v++)
    for (int k = 0; k < nd; k++) { const int x = r.i16(); if (k < DRUM_PARAMS) m.drum[v][k] = clampTo(P_D_PTCH + k, x); }
  const int ns = r.u8();
  auto step = [&](Step *s) {
    Step t;
    t.on = r.u8() != 0; t.vel = r.u8(); t.note = (int8_t)r.u8(); t.gate = r.u8(); t.prob = r.u8();
    if (!s) return;
    t.vel = clampTo(P_ST_VEL, t.vel); t.note = clampTo(P_ST_NOTE, t.note);
    t.gate = clampTo(P_ST_GATE, t.gate); t.prob = clampTo(P_ST_PROB, t.prob);
    *s = t;
  };
  for (int v = 0; v < 16; v++) for (int s = 0; s < ns; s++) step(s < STEPS ? &m.drumSteps[v][s] : nullptr);
  for (int s = 0; s < ns; s++) step(s < STEPS ? &m.synthSteps[s] : nullptr);
  if (!r.ok) return false;
  *this = m;
  return true;
}

}  // namespace hugbox
