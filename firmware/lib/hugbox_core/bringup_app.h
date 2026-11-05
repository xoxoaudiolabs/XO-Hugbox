// XO-Hugbox bring-up behaviour as a portable App (port of firmware/src/bringup/main.cpp).
#pragma once
#include "hugbox_app.h"

namespace hugbox {

class BringupApp : public App {
 public:
  void begin(const Platform &p) override;
  void loop(uint32_t nowMs, const Inputs &in) override;
  void render(int16_t *stereo, int frames) override;

  // read-outs for the emulator UI
  float toneHz() const { return toneHz_; }
  float toneGain() const { return toneGain_; }
  uint32_t frames() const { return frames_; }   // OLED frames sent

 private:
  void drawOled();
  void logf(const char *fmt, ...);

  Platform p_;
  uint16_t padBits_ = 0, prevPads_ = 0;
  bool shiftHeld_ = false, prevShift_ = false;
  uint16_t pots_[4] = {}, reported_[4] = {};
  float vbat_ = 0;
  uint32_t lastScan_ = 0, lastSlow_ = 0, lastBat_ = 0, frames_ = 0;

  // shared with the audio core
  volatile float toneHz_ = 0;   // 0 = silent
  volatile float toneGain_ = 0;
  float phase_ = 0, gain_ = 0;  // audio-core state
};

}  // namespace hugbox
