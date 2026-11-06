#include "engine.h"
#include <string.h>

namespace hugbox {

using namespace dsp;

float dsp::SINE[1025];
void dsp::init() {
  static bool done = false;
  if (done) return;
  for (int i = 0; i <= 1024; i++) SINE[i] = sinf(kTwoPi * i / 1024.0f);
  done = true;
}

static constexpr float FS = (float)SAMPLE_RATE;
static constexpr int CTRL = 16;   // control-rate interval (samples) for filter coefficients and drift

static float msFrom(int v) { const float x = v / 100.0f; return 1.0f + x * x * 3000.0f; }

// ================================================================ synth
void Synth::noteOn(int note, int vel, int32_t gate, const Model &m) {
  dsp::init();
  int best = -1;
  for (int i = 0; i < SYNTH_VOICES; i++)                    // same note: retrigger that voice
    if (v_[i].on && v_[i].note == note) { best = i; break; }
  if (best < 0)
    for (int i = 0; i < SYNTH_VOICES; i++) if (!v_[i].on) { best = i; break; }
  if (best < 0) {                                           // steal: oldest released, else oldest
    uint32_t oldest = 0xFFFFFFFFu;
    for (int pass = 0; pass < 2 && best < 0; pass++)
      for (int i = 0; i < SYNTH_VOICES; i++) {
        if (pass == 0 && v_[i].stage != 3) continue;
        if (v_[i].age < oldest) { oldest = v_[i].age; best = i; }
      }
  }
  Voice &v = v_[best];
  const float hz = midiHz((float)note);
  const bool legato = v.on && m.value[P_GLID] > 0;
  if (!legato) {
    // pick up a glide from the most recent voice when glide is on
    float from = hz;
    if (m.value[P_GLID] > 0) {
      uint32_t newest = 0;
      for (int i = 0; i < SYNTH_VOICES; i++)
        if (v_[i].on && v_[i].age >= newest) { newest = v_[i].age; from = v_[i].f; }
    }
    v.f = from;
    if (!v.on) { v.flt.reset(); v.env = 0; v.ph1 = v.ph2 = v.phm = 0; }
  }
  v.on = true;
  v.note = note;
  v.target = hz;
  v.stage = 0;
  v.gate = gate;
  v.vel = 0.35f + 0.65f * vel / 127.0f;
  v.age = ++clock_;
}

void Synth::noteOff(int note) {
  for (auto &v : v_)
    if (v.on && v.note == note && v.gate < 0 && v.stage < 3) v.stage = 3;
}

void Synth::releaseAll() {
  for (auto &v : v_) if (v.on && v.stage < 3) v.stage = 3;
}

int Synth::active() const {
  int n = 0;
  for (auto &v : v_) n += v.on;
  return n;
}

void Synth::render(float *out, int n, const Model &m) {
  const int w1 = m.value[P_W1], w2 = m.value[P_W2];
  const float dtune = powf(2.0f, m.value[P_DTUN] / 12.0f) * 1.0029f;   // +5 cents so unison beats
  const float mix = m.value[P_MIX] / 100.0f;
  const float fmIdx = (m.value[P_FM] / 100.0f) * (m.value[P_FM] / 100.0f) * 2.5f;   // phase-mod depth, cycles
  const float fmRatio = m.value[P_FMR] * 0.5f;
  const float driftAmt = m.value[P_DRFT] / 100.0f;
  const float glideK = m.value[P_GLID] ? expf(-1.0f / (m.value[P_GLID] * 0.004f * FS)) : 0.0f;
  const float cutOct = m.value[P_CUT] / 100.0f * 9.8f;        // 20 Hz .. ~18 kHz
  const float res = m.value[P_RES] / 100.0f;
  const float envOct = m.value[P_ENV] / 100.0f * 6.0f;
  const float atk = 1.0f / (msFrom(m.value[P_ATK]) * 0.001f * FS);
  const float dcyK = expf(-4.6f / (msFrom(m.value[P_DCY]) * 0.001f * FS));
  const float relK = expf(-4.6f / (msFrom(m.value[P_REL]) * 0.001f * FS));
  const float sus = m.value[P_SUS] / 100.0f;
  const float vol = m.value[P_SVOL] / 100.0f * 0.32f;

  for (auto &v : v_) {
    if (!v.on) continue;
    for (int i = 0; i < n; i++) {
      if (((i & (CTRL - 1)) == 0)) {
        if (--v.driftCount <= 0) { v.driftTo = noise(rng_) * driftAmt; v.driftCount = 300; }   // ~0.1 s
        v.drift += (v.driftTo - v.drift) * 0.02f;
        const float key = (v.note - 60) / 12.0f * 0.5f;                  // 50 % keytrack
        const float oct = cutOct + envOct * v.env + key + v.drift * 0.3f;
        v.flt.set(20.0f * powf(2.0f, oct), res, FS);
      }
      if (v.gate > 0 && --v.gate == 0 && v.stage < 3) v.stage = 3;
      switch (v.stage) {
        case 0: v.env += atk; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; } break;
        case 1: v.env = sus + (v.env - sus) * dcyK; break;
        default: v.env *= relK; break;
      }
      if (v.stage == 3 && v.env < 0.0003f) { v.on = false; break; }
      v.f = v.target + (v.f - v.target) * glideK;
      const float f = v.f * (1.0f + v.drift * 0.007f);                  // up to ~12 cents of drift
      const float dt1 = f / FS, dt2 = dt1 * dtune;
      // operator 2 (sine) phase-modulates oscillator 1
      float p1 = v.ph1;
      if (fmIdx > 0) {
        v.phm += dt1 * fmRatio;
        if (v.phm >= 1.0f) v.phm -= 1.0f;
        p1 += fmIdx * sine(v.phm);
        p1 -= (float)(int)p1;
        if (p1 < 0) p1 += 1.0f;
      }
      float o1, o2;
      switch (w1) { case 0: o1 = sine(p1); break; case 1: o1 = saw(p1, dt1); break; case 2: o1 = square(p1, dt1); break; default: o1 = noise(rng_); }
      switch (w2) { case 0: o2 = sine(v.ph2); break; case 1: o2 = saw(v.ph2, dt2); break; case 2: o2 = square(v.ph2, dt2); break; default: o2 = noise(rng_); }
      v.ph1 += dt1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
      v.ph2 += dt2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
      const float x = o1 * (1.0f - mix) + o2 * mix;
      out[i] += v.flt.process(x) * v.env * v.vel * vol;
    }
  }
}

// ================================================================ sampler
size_t Sampler::init(void *(*alloc)(size_t), void (*release)(void *)) {
  dsp::init();
  return renderKit(s_, alloc, release);
}

void Sampler::trigger(int pad, int vel, const Model &m, int semis) {
  const int16_t *p = m.drum[pad];
  auto P = [&](int id) { return (int)p[id - P_D_PTCH]; };
  const int choke = P(P_D_CHOK);
  if (choke)
    for (int o = 0; o < 16; o++)
      if (o != pad && m.drum[o][P_D_CHOK - P_D_PTCH] == choke) v_[o].on = false;
  const int smp = P(P_D_SMPL) & 15;
  const Sample &src = u_[smp].data ? u_[smp] : s_[smp];
  if (!src.data || src.len < 2) return;
  Voice &v = v_[pad];
  v.on = true;
  v.data = src.data;
  v.len = src.len;
  v.pos = 0;
  v.rate = powf(2.0f, (P(P_D_PTCH) + semis) / 12.0f);
  v.gain = (vel / 127.0f) * (P(P_D_LVL) / 100.0f);
  v.env = 1.0f;
  const int d = P(P_D_DCY);
  if (d >= 100) v.envK = 1.0f;
  else {                                       // shorten the natural tail: -60 dB at T
    const float dur = src.len / FS / v.rate;
    const float x = d / 100.0f;
    const float T = 0.015f + x * x * dur;
    v.envK = expf(-6.9f / (T * FS));
  }
  const int c = P(P_D_CUT);
  v.filtered = c < 100;
  if (v.filtered) { v.flt.reset(); v.flt.set(40.0f * powf(2.0f, c / 100.0f * 8.9f), 0.15f, FS); }
  const float pan = P(P_D_PAN) / 100.0f + 0.5f;   // 0..1
  v.gl = sqrtf(1.0f - pan);
  v.gr = sqrtf(pan);
}

void Sampler::stopAll() { for (auto &v : v_) v.on = false; }

const int16_t *Sampler::setUser(int i, const int16_t *data, uint32_t len) {
  const int16_t *old = u_[i].data;
  if (old)
    for (auto &v : v_) if (v.on && v.data == old) v.on = false;
  u_[i].data = data;
  u_[i].len = data ? len : 0;
  return old;
}

void Sampler::render(float *l, float *r, int n, const Model &m) {
  const float kit = m.value[P_KITV] / 100.0f * 0.8f;
  for (auto &v : v_) {
    if (!v.on) continue;
    const int16_t *d = v.data;
    const float g = v.gain * kit;
    for (int i = 0; i < n; i++) {
      const uint32_t ip = (uint32_t)v.pos;
      if (ip + 1 >= v.len) { v.on = false; break; }
      const float f = v.pos - ip;
      float x = (d[ip] + (d[ip + 1] - d[ip]) * f) * (1.0f / 32768.0f);
      if (v.filtered) x = v.flt.process(x);
      x *= g * v.env;
      v.env *= v.envK;
      if (v.env < 0.0005f) { v.on = false; break; }
      l[i] += x * v.gl;
      r[i] += x * v.gr;
      v.pos += v.rate;
    }
  }
}

}  // namespace hugbox
