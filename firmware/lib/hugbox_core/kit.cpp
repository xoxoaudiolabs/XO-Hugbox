// Built-in drum kit, synthesized once at boot into memory (PSRAM on the board, ~0.8 MB).
// Classic analog-machine recipes: swept sines, metallic square clusters, filtered noise.
// The sampler plays these exactly like loaded samples, so the pad pitch/decay/filter controls behave
// the same when user samples arrive later.
#include <string.h>
#include "engine.h"

namespace hugbox {

using namespace dsp;

static constexpr float FS = (float)SAMPLE_RATE;
// seconds per sound, in DRUM_NAMES order
static const float LEN_S[16] = {0.6f, 0.35f, 0.45f, 0.15f, 0.7f, 0.55f, 0.45f, 0.08f,
                                0.4f, 0.22f, 2.0f, 1.6f, 0.35f, 0.12f, 0.3f, 1.4f};

// six detuned squares at the classic hi-hat ratios -> metallic noise
static float metal(float t, float scale) {
  static const float HZ[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
  float s = 0;
  for (int k = 0; k < 6; k++) {
    float ph = t * HZ[k] * scale;
    ph -= (int)ph;
    s += ph < 0.5f ? 1.0f : -1.0f;
  }
  return s / 6.0f;
}

static void generate(int type, float *b, int len) {
  uint32_t rng = 0xC0FFEEu + type * 7919u;
  Svf f1, f2;
  float ph = 0, ph2 = 0;
  auto bp = [&](Svf &f, float x) { float b0; f.process(x, &b0); return b0; };
  auto hp = [&](Svf &f, float x) { float h; f.process(x, nullptr, &h); return h; };
  switch (type) {
    case 3: case 4: f1.set(7500, 0.2f, FS); f2.set(9000, 0.3f, FS); break;   // hats
    case 1: f1.set(3200, 0.25f, FS); break;                                   // snare noise
    case 2: f1.set(1250, 0.55f, FS); break;                                   // clap
    case 7: f1.set(2200, 0.6f, FS); break;                                    // rim
    case 8: f1.set(2600, 0.5f, FS); break;                                    // cowbell
    case 9: f1.set(5500, 0.1f, FS); break;                                    // shaker
    case 10: f1.set(5200, 0.15f, FS); f2.set(8000, 0.2f, FS); break;          // crash
    case 11: f1.set(6500, 0.3f, FS); break;                                   // ride
  }
  for (int i = 0; i < len; i++) {
    const float t = i / FS;
    const float n = noise(rng);
    float x = 0;
    switch (type) {
      case 0: {                                             // kick: 160 -> 48 Hz sweep, click, drive
        const float hz = 48.0f + 112.0f * expf(-t / 0.035f);
        ph += hz / FS;
        x = sine(ph) * expf(-t / 0.28f);
        if (t < 0.004f) x += n * 0.5f * (1 - t / 0.004f);
        x = tanhish(x * 1.6f);
      } break;
      case 1: {                                             // snare: two tones + bright noise
        ph += 185.0f / FS; ph2 += 330.0f / FS;
        const float tone = (sine(ph) * 0.6f + sine(ph2) * 0.4f) * expf(-t / 0.07f);
        x = tone * 0.8f + bp(f1, n) * 1.6f * expf(-t / 0.13f);
      } break;
      case 2: {                                             // clap: 3 bursts then a tail
        float env;
        if (t < 0.03f) env = expf(-fmodf(t, 0.01f) / 0.003f);
        else env = expf(-(t - 0.03f) / 0.16f);
        x = bp(f1, n) * 2.2f * env;
      } break;
      case 3: x = hp(f2, bp(f1, metal(t, 1.0f) + 0.3f * n)) * expf(-t / 0.035f) * 2.0f; break;   // closed hat
      case 4: x = hp(f2, bp(f1, metal(t, 1.0f) + 0.3f * n)) * expf(-t / 0.22f) * 2.0f; break;    // open hat
      case 5: case 6: {                                     // toms
        const float base = type == 5 ? 95.0f : 145.0f;
        ph += base * (1.0f + 0.35f * expf(-t / 0.05f)) / FS;
        x = sine(ph) * expf(-t / (type == 5 ? 0.22f : 0.17f)) + n * 0.05f * expf(-t / 0.01f);
      } break;
      case 7: ph += 1700.0f / FS; x = (sine(ph) * 0.7f + bp(f1, n)) * expf(-t / 0.012f); break;   // rim
      case 8: {                                             // cowbell: two squares through a bandpass
        ph += 540.0f / FS; ph2 += 800.0f / FS;
        const float sq = ((ph - (int)ph) < 0.5f ? 1.0f : -1.0f) + ((ph2 - (int)ph2) < 0.5f ? 1.0f : -1.0f);
        x = bp(f1, sq) * 1.4f * (t < 0.01f ? 1.0f : expf(-(t - 0.01f) / 0.11f));
      } break;
      case 9: x = hp(f1, n) * (t < 0.012f ? t / 0.012f : expf(-(t - 0.012f) / 0.05f)); break;   // shaker
      case 10: x = hp(f2, bp(f1, metal(t, 1.17f) * 0.6f + n * 0.7f)) * expf(-t / 0.6f) * 1.8f; break;   // crash
      case 11: {                                            // ride: metal + bell partial
        ph += 3150.0f / FS;
        x = (bp(f1, metal(t, 1.6f) + 0.15f * n) * 1.5f + sine(ph) * 0.18f) * expf(-t / 0.5f);
      } break;
      case 12: ph += 330.0f * (1.0f + 0.25f * expf(-t / 0.02f)) / FS; x = sine(ph) * expf(-t / 0.09f); break;   // conga-ish
      case 13: ph += 2500.0f / FS; x = sine(ph) * expf(-t / 0.018f); break;                                       // clave
      case 14: ph += (80.0f + 2900.0f * expf(-t / 0.03f)) / FS; x = sine(ph) * expf(-t / 0.09f); break;           // zap
      default: {                                            // 808-style boom, slight drop, saturated
        ph += 55.0f * (1.0f + 0.5f * expf(-t / 0.03f)) / FS;
        x = tanhish(sine(ph) * 1.8f * expf(-t / 0.55f));
      } break;
    }
    // 2 ms fade at the very end so a sample never clicks off
    const int tail = len - i;
    if (tail < 88) x *= tail / 88.0f;
    b[i] = x;
  }
}

size_t renderKit(Sample out[16], void *(*alloc)(size_t), void (*release)(void *)) {
  int maxLen = 0;
  for (int k = 0; k < 16; k++) { const int l = (int)(LEN_S[k] * FS); if (l > maxLen) maxLen = l; }
  float *tmp = (float *)alloc(maxLen * sizeof(float));
  if (!tmp) return 0;
  size_t used = 0;
  for (int k = 0; k < 16; k++) {
    const int len = (int)(LEN_S[k] * FS);
    int16_t *d = (int16_t *)alloc(len * sizeof(int16_t));
    if (!d) { release(tmp); return used; }
    generate(k, tmp, len);
    float peak = 1e-6f;
    for (int i = 0; i < len; i++) { const float a = fabsf(tmp[i]); if (a > peak) peak = a; }
    const float g = 0.89f * 32767.0f / peak;          // normalise to -1 dBFS
    for (int i = 0; i < len; i++) d[i] = (int16_t)(tmp[i] * g);
    out[k].data = d;
    out[k].len = len;
    used += len * sizeof(int16_t);
  }
  release(tmp);
  return used;
}

}  // namespace hugbox
