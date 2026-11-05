// Port of firmware/src/bringup/main.cpp: same timing, same maths, same OLED page, same log lines.
// Keep the two in step until the firmware itself builds on this file (see sim/README.md).
#include "bringup_app.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

namespace hugbox {

void BringupApp::logf(const char *fmt, ...) {
  if (!p_.log) return;
  char line[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  p_.log(line);
}

void BringupApp::begin(const Platform &p) {
  p_ = p;
  logf("Hold a pad for a note, shift+pad for 1 kHz. Pot 0 = volume.\n");
}

// ---------- audio (core 1) ----------
void BringupApp::render(int16_t *buf, int frames) {
  const float hz = toneHz_, target = hz > 0 ? toneGain_ : 0.0f;
  const float inc = hz / SAMPLE_RATE;
  for (int i = 0; i < frames; i++) {
    gain_ += (target - gain_) * 0.002f;            // click-free fade in/out
    const float s = sinf(phase_ * 2.0f * PI) * gain_;
    phase_ += inc;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    const int16_t v = (int16_t)(s * 32767.0f);
    buf[2 * i] = v;
    buf[2 * i + 1] = v;
  }
}

// ---------- display ----------
void BringupApp::drawOled() {
  u8g2_t *g = p_.oled;
  if (!g) return;
  u8g2_ClearBuffer(g);
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  u8g2_DrawStr(g, 0, 9, "XO-Hugbox bring-up");
  char line[32];
  snprintf(line, sizeof line, "BAT %.2fV %s", vbat_, shiftHeld_ ? "SHIFT" : "");
  u8g2_DrawStr(g, 0, 20, line);
  for (int k = 0; k < 4; k++) {                       // pot bars
    const int y = 25 + k * 6;
    u8g2_DrawFrame(g, 0, y, 62, 5);
    u8g2_DrawBox(g, 1, y + 1, (int)(pots_[k] * 60 / 4095), 3);
  }
  for (int r = 0; r < 4; r++)                         // pad grid, row 0 at the top
    for (int c = 0; c < 4; c++) {
      const int x = 74 + c * 13, y = 14 + r * 12;
      if (padBits_ & (1u << (r * 4 + c))) u8g2_DrawBox(g, x, y, 11, 10);
      else u8g2_DrawFrame(g, x, y, 11, 10);
    }
  snprintf(line, sizeof line, "%s", toneHz_ > 0 ? "tone" : "");
  u8g2_DrawStr(g, 0, 63, line);
  u8g2_SendBuffer(g);
  frames_++;
}

// ---------- loop (core 0) ----------
void BringupApp::loop(uint32_t now, const Inputs &in) {
  if (now - lastScan_ >= 5) {                          // 200 Hz pad scan
    lastScan_ = now;
    padBits_ = p_.pcfOk ? in.padBits : 0;
    shiftHeld_ = in.shift;
    if (shiftHeld_ != prevShift_) logf("shift %s\n", shiftHeld_ ? "down" : "up");
    const uint16_t changed = padBits_ ^ prevPads_;
    for (int n = 0; n < 16; n++)
      if (changed & (1u << n))
        logf("pad %2d (row %d col %d) %s\n", n, n / 4, n % 4, (padBits_ & (1u << n)) ? "down" : "up");
    prevPads_ = padBits_;
    prevShift_ = shiftHeld_;

    // lowest held pad picks the note
    float hz = 0;
    if (padBits_) {
      const int n = __builtin_ctz(padBits_);
      hz = shiftHeld_ ? 1000.0f : 261.63f * powf(2.0f, n / 12.0f);
    }
    toneGain_ = 0.05f + 0.45f * pots_[0] / 4095.0f;
    toneHz_ = hz;
  }

  if (now - lastSlow_ >= 100) {                        // 10 Hz: pots, battery, display
    lastSlow_ = now;
    for (int k = 0; k < 4; k++) {
      pots_[k] = (pots_[k] * 3 + in.potRaw[k]) / 4;
      if (abs((int)pots_[k] - (int)reported_[k]) > 64) {
        logf("pot %d = %4u\n", k, pots_[k]);
        reported_[k] = pots_[k];
      }
    }
    vbat_ = in.vsenseMv * 2 / 1000.0f;
    drawOled();
    if (now - lastBat_ >= 5000) { lastBat_ = now; logf("battery %.2f V\n", vbat_); }
  }
}

}  // namespace hugbox
