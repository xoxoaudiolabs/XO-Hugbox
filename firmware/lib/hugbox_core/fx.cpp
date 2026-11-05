#include "fx.h"
#include <string.h>
#include "hugbox_app.h"

namespace hugbox {

using namespace dsp;

const float DELAY_DIV_16THS[10] = {0.5f, 2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 8.0f / 3.0f, 3.0f, 4.0f, 8.0f};
const char *const DELAY_DIV_NAMES[10] = {"1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2"};

static constexpr float FS = (float)SAMPLE_RATE;
// longest delay: a half note at the slowest tempo (40 BPM) = 1.5 s
static constexpr int MAX_DELAY = (int)(FS * 1.5f) + 4;

bool Fx::init(void *(*alloc)(size_t)) {
  dl_ = (float *)alloc(MAX_DELAY * sizeof(float));
  dr_ = (float *)alloc(MAX_DELAY * sizeof(float));
  if (!dl_ || !dr_) { dl_ = dr_ = nullptr; return false; }
  len_ = MAX_DELAY;
  clear();
  return true;
}

void Fx::clear() {
  if (dl_) { memset(dl_, 0, len_ * sizeof(float)); memset(dr_, 0, len_ * sizeof(float)); }
  w_ = 0;
  dampL_ = dampR_ = 0;
}

void Fx::process(float *l, float *r, int n, const Model &m) {
  // ---- drive: tanh saturation with rough loudness compensation
  const float d = m.value[P_DRV] / 100.0f;
  const float drive = 1.0f + 11.0f * d * d;
  const float makeup = 1.0f / (1.0f + 0.3f * (drive - 1.0f));   // keeps full drive within ~+3 dB of dry
  // ---- bitcrush + sample-rate reduction
  const int bits = m.value[P_CRSH];
  const float q = (float)(1 << (bits - 1));
  const float lofi = m.value[P_RATE] / 100.0f;
  const float holdInc = 1.0f / (1.0f + lofi * lofi * 31.0f);   // 1 = every sample .. 1/32
  // ---- delay
  const float mix = m.value[P_DMIX] / 100.0f;
  const float fb = m.value[P_DFB] / 100.0f;
  const float target = FS * 60.0f / m.value[P_BPM] / 4.0f * DELAY_DIV_16THS[m.value[P_DTIM] % 10];
  const float tgt = target < len_ - 4 ? target : (float)(len_ - 4);
  if (time_ < 0) time_ = tgt;
  const float damp = 0.35f;            // one-pole low-pass in the feedback path (~3.5 kHz): repeats darken
  // ---- low cut (one-pole high-pass), 0 = off
  const int lc = m.value[P_SHPF];
  const float hpK = lc ? 1.0f - expf(-kTwoPi * (20.0f * powf(2.0f, lc / 100.0f * 4.32f)) / FS) : 0.0f;

  for (int i = 0; i < n; i++) {
    float a = l[i], b = r[i];
    if (d > 0) { a = tanhish(a * drive) * makeup; b = tanhish(b * drive) * makeup; }
    if (bits < 16) { a = floorf(a * q + 0.5f) / q; b = floorf(b * q + 0.5f) / q; }
    if (lofi > 0) {
      holdPh_ += holdInc;
      if (holdPh_ >= 1.0f) { holdPh_ -= 1.0f; holdL_ = a; holdR_ = b; }
      a = holdL_; b = holdR_;
    }
    if (dl_ && (mix > 0 || fb > 0)) {
      time_ += (tgt - time_) * 0.0002f;                 // glide to a new time like tape, no clicks
      float rp = w_ - time_;
      if (rp < 0) rp += len_;
      const int i0 = (int)rp;
      const int i1 = i0 + 1 < len_ ? i0 + 1 : 0;
      const float f = rp - i0;
      const float yl = dl_[i0] + (dl_[i1] - dl_[i0]) * f;
      const float yr = dr_[i0] + (dr_[i1] - dr_[i0]) * f;
      dampL_ += damp * (yl - dampL_);
      dampR_ += damp * (yr - dampR_);
      // ping-pong: the input enters the left line, each line feeds the other
      dl_[w_] = softclip((a + b) * 0.5f + dampR_ * fb);
      dr_[w_] = softclip(dampL_ * fb);
      if (++w_ >= len_) w_ = 0;
      a += yl * mix;
      b += yr * mix;
    }
    if (hpK > 0) {
      hpL_ += hpK * (a - hpL_); a -= hpL_;
      hpR_ += hpK * (b - hpR_); b -= hpR_;
    }
    l[i] = a;
    r[i] = b;
  }
}

}  // namespace hugbox
