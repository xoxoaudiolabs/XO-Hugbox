// XO-Hugbox master FX chain (audio core): drive -> bitcrush / sample-rate reduction -> ping-pong delay
// (tempo-synced, damped feedback, tape-style time glide) -> low cut. Reads its settings from the Model.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dsp.h"
#include "model.h"

namespace hugbox {

// Delay note values in 16th notes, index = P_DTIM value.
extern const float DELAY_DIV_16THS[10];
extern const char *const DELAY_DIV_NAMES[10];

class Fx {
 public:
  // Allocates the delay lines (~0.5 MB, PSRAM on the board). Without them the delay is skipped.
  bool init(void *(*alloc)(size_t));
  void clear();
  void process(float *l, float *r, int n, const Model &m);

 private:
  float *dl_ = nullptr, *dr_ = nullptr;
  int len_ = 0, w_ = 0;
  float time_ = -1;              // current delay in samples (glides toward the target)
  float dampL_ = 0, dampR_ = 0;  // feedback low-pass state
  float holdL_ = 0, holdR_ = 0, holdPh_ = 1;   // sample-rate reduction
  float hpL_ = 0, hpR_ = 0;      // low cut state
};

}  // namespace hugbox
