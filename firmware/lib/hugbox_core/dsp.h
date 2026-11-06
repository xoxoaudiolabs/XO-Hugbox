// XO-Hugbox - small DSP building blocks shared by the synth, the sampler and the kit generator.
// Single-precision float throughout (the ESP32-S3 has a single-precision FPU; doubles are slow).
#pragma once
#include <math.h>
#include <stdint.h>

namespace hugbox {
namespace dsp {

constexpr float kTwoPi = 6.283185307179586f;

// 1024-point sine table with linear interpolation; phase in cycles (0..1).
extern float SINE[1025];
void init();
inline float sine(float ph) {
  ph -= (float)(int)ph;
  if (ph < 0) ph += 1.0f;
  const float x = ph * 1024.0f;
  const int i = (int)x;
  const float f = x - i;
  return SINE[i] + (SINE[i + 1] - SINE[i]) * f;
}

// PolyBLEP residual for band-limited saw/square (t = phase 0..1, dt = phase increment).
inline float polyblep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

inline float saw(float t, float dt) { return 2.0f * t - 1.0f - polyblep(t, dt); }
inline float square(float t, float dt) {
  float t2 = t + 0.5f;
  if (t2 >= 1.0f) t2 -= 1.0f;
  return (t < 0.5f ? 1.0f : -1.0f) + polyblep(t, dt) - polyblep(t2, dt);
}

// xorshift32 white noise in -1..1
inline float noise(uint32_t &s) {
  s ^= s << 13; s ^= s >> 17; s ^= s << 5;
  return (int32_t)s * (1.0f / 2147483648.0f);
}

// Soft saturation, |out| < 1 for any input.
inline float softclip(float x) { return x / (1.0f + fabsf(x)); }
// tanh-like with unity slope at 0 (rational approximation, clamped)
inline float tanhish(float x) {
  if (x > 3.0f) return 1.0f;
  if (x < -3.0f) return -1.0f;
  const float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
// Output limiter: exactly linear up to `knee` (-6 dBFS), then a tanh-shaped shoulder that meets full scale at 2.
// Unity slope at the knee, so normal levels pass untouched and only peaks are rounded off.
inline float limit(float x, float knee = 0.5f) {
  const float a = fabsf(x);
  if (a <= knee) return x;
  const float y = knee + (1.0f - knee) * tanhish((a - knee) / (1.0f - knee));
  return x < 0 ? -y : y;
}

// Topology-preserving state-variable filter (Zavalishin). Set coefficients at control rate.
struct Svf {
  float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 2;
  // fc in Hz, res 0..1 (1 = near self-oscillation)
  void set(float fc, float res, float fs) {
    if (fc > fs * 0.45f) fc = fs * 0.45f;
    if (fc < 10.0f) fc = 10.0f;
    const float g = tanf(3.14159265f * fc / fs);
    k = 2.0f - 1.96f * res;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
  }
  // returns lowpass; bp/hp available through the out-params when needed
  inline float process(float v0, float *bp = nullptr, float *hp = nullptr) {
    const float v3 = v0 - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    if (bp) *bp = v1;
    if (hp) *hp = v0 - k * v1 - v2;
    return v2;
  }
  void reset() { ic1 = ic2 = 0; }
};

inline float midiHz(float note) { return 440.0f * powf(2.0f, (note - 69.0f) * (1.0f / 12.0f)); }

}  // namespace dsp
}  // namespace hugbox
