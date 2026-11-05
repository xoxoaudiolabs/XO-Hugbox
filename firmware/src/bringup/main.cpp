// XO-Hugbox hardware bring-up
//  - USB serial log + LED heartbeat
//  - I2C scan (expect 0x3C OLED, 0x20 PCF8574)
//  - OLED status page (SH1106, upside down -> U8G2_R2)
//  - 4x4 pad matrix via PCF8574, shift key, 4 pots, battery voltage
//  - I2S test tone to MAX98357A + PCM5102A: hold a pad = note (pad 0 = C4 ... pad 15 = D#5),
//    pot 0 = volume, shift + pad = 1 kHz reference sine.
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP_I2S.h>
#include "pins.h"

static constexpr uint32_t SAMPLE_RATE = 44100;
static constexpr int AUDIO_FRAMES = 128;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R2, U8X8_PIN_NONE);
I2SClass i2s;

static bool oledOk = false, pcfOk = false;
static uint16_t padBits = 0;         // bit n = pad n held (n = row*4 + col, row 0 = top)
static bool shiftHeld = false;
static uint16_t pots[4];
static float vbat = 0;

// shared with the audio task
static volatile float toneHz = 0;    // 0 = silent
static volatile float toneGain = 0;

// ---------- I2C ----------
static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void i2cScan() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2cPresent(a)) { Serial.printf("  0x%02X\n", a); found++; }
  }
  if (!found) Serial.println("  nothing found - check SDA/SCL wires to the PCF I2C pads and OLED");
  oledOk = i2cPresent(OLED_ADDR);
  pcfOk = i2cPresent(PCF8574_ADDR);
  Serial.printf("  OLED 0x%02X: %s   PCF8574 0x%02X: %s\n", OLED_ADDR, oledOk ? "OK" : "MISSING",
                PCF8574_ADDR, pcfOk ? "OK" : "MISSING");
}

static void pcfWrite(uint8_t v) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(v);
  Wire.endTransmission();
}

static uint8_t pcfRead() {
  Wire.requestFrom((int)PCF8574_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// Rows P0..P3 driven low one at a time; columns P4..P7 held high (weak pull-up) and read back.
static uint16_t scanMatrix() {
  if (!pcfOk) return 0;
  uint16_t bits = 0;
  for (int r = 0; r < 4; r++) {
    pcfWrite(PCF_COL_MASK | (uint8_t)(~(1u << r) & PCF_ROW_MASK));
    uint8_t cols = (uint8_t)(~pcfRead() & PCF_COL_MASK) >> 4;
    for (int c = 0; c < 4; c++)
      if (cols & (1u << c)) bits |= 1u << (r * 4 + c);
  }
  pcfWrite(0xFF);
  return bits;
}

// ---------- audio (core 1) ----------
static void audioTask(void *) {
  static int16_t buf[AUDIO_FRAMES * 2];
  float phase = 0, gain = 0;
  for (;;) {
    const float hz = toneHz, target = hz > 0 ? toneGain : 0.0f;
    const float inc = hz / SAMPLE_RATE;
    for (int i = 0; i < AUDIO_FRAMES; i++) {
      gain += (target - gain) * 0.002f;            // click-free fade in/out
      const float s = sinf(phase * 2.0f * PI) * gain;
      phase += inc;
      if (phase >= 1.0f) phase -= 1.0f;
      const int16_t v = (int16_t)(s * 32767.0f);
      buf[2 * i] = v;
      buf[2 * i + 1] = v;
    }
    i2s.write((uint8_t *)buf, sizeof(buf));        // blocks on DMA -> paces the loop
  }
}

static bool audioBegin() {
  i2s.setPins(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S begin FAILED");
    return false;
  }
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  return true;
}

// ---------- display ----------
static void drawOled() {
  if (!oledOk) return;
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 9, "XO-Hugbox bring-up");
  char line[32];
  snprintf(line, sizeof line, "BAT %.2fV %s", vbat, shiftHeld ? "SHIFT" : "");
  oled.drawStr(0, 20, line);
  for (int k = 0; k < 4; k++) {                       // pot bars
    const int y = 25 + k * 6;
    oled.drawFrame(0, y, 62, 5);
    oled.drawBox(1, y + 1, (int)(pots[k] * 60 / 4095), 3);
  }
  for (int r = 0; r < 4; r++)                         // pad grid, row 0 at the top
    for (int c = 0; c < 4; c++) {
      const int x = 74 + c * 13, y = 14 + r * 12;
      if (padBits & (1u << (r * 4 + c))) oled.drawBox(x, y, 11, 10);
      else oled.drawFrame(x, y, 11, 10);
    }
  snprintf(line, sizeof line, "%s", toneHz > 0 ? "tone" : "");
  oled.drawStr(0, 63, line);
  oled.sendBuffer();
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_SHIFT, INPUT_PULLUP);
  analogReadResolution(12);

  Serial.println("\n=== XO-Hugbox bring-up ===");
  Serial.printf("Chip %s rev %d, flash %u MB, PSRAM %u KB\n", ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getFlashChipSize() >> 20, ESP.getPsramSize() >> 10);

  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  i2cScan();
  if (pcfOk) pcfWrite(0xFF);
  if (oledOk) {
    oled.setI2CAddress(OLED_ADDR << 1);
    oled.begin();
  }
  Serial.println(audioBegin() ? "I2S running at 44.1 kHz" : "no audio");
  Serial.println("Hold a pad for a note, shift+pad for 1 kHz. Pot 0 = volume.");
}

void loop() {
  static uint32_t lastScan = 0, lastSlow = 0;
  static uint16_t prevPads = 0;
  static bool prevShift = false;
  const uint32_t now = millis();

  if (now - lastScan >= 5) {                          // 200 Hz pad scan
    lastScan = now;
    padBits = scanMatrix();
    shiftHeld = digitalRead(PIN_SHIFT) == LOW;
    if (shiftHeld != prevShift) Serial.printf("shift %s\n", shiftHeld ? "down" : "up");
    const uint16_t changed = padBits ^ prevPads;
    for (int n = 0; n < 16; n++)
      if (changed & (1u << n))
        Serial.printf("pad %2d (row %d col %d) %s\n", n, n / 4, n % 4, (padBits & (1u << n)) ? "down" : "up");
    prevPads = padBits;
    prevShift = shiftHeld;

    // lowest held pad picks the note
    float hz = 0;
    if (padBits) {
      const int n = __builtin_ctz(padBits);
      hz = shiftHeld ? 1000.0f : 261.63f * powf(2.0f, n / 12.0f);
    }
    toneGain = 0.05f + 0.45f * pots[0] / 4095.0f;
    toneHz = hz;
  }

  if (now - lastSlow >= 100) {                        // 10 Hz: pots, battery, display
    lastSlow = now;
    static uint16_t reported[4];
    const uint8_t potPins[4] = {PIN_POT0, PIN_POT1, PIN_POT2, PIN_POT3};
    for (int k = 0; k < 4; k++) {
      pots[k] = (pots[k] * 3 + analogRead(potPins[k])) / 4;
      if (abs((int)pots[k] - (int)reported[k]) > 64) {
        Serial.printf("pot %d = %4u\n", k, pots[k]);
        reported[k] = pots[k];
      }
    }
    vbat = analogReadMilliVolts(PIN_VBAT_SENSE) * 2 / 1000.0f;
    drawOled();
    digitalWrite(LED_BUILTIN, (now / 500) & 1);       // heartbeat (LED is active low)
    static uint32_t lastBat = 0;
    if (now - lastBat >= 5000) { lastBat = now; Serial.printf("battery %.2f V\n", vbat); }
  }
}
