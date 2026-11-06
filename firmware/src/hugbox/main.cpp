// XO-Hugbox firmware - board platform layer.
// All behaviour lives in lib/hugbox_core (the same C++ the browser emulator runs). This file only does
// hardware: PCF8574 key matrix, ADC pots/battery, shift key, the OLED object, I2S out + the INMP441 mic in,
// project and sample files on flash (LittleFS), USB + BLE MIDI (midi_io.cpp), and the two tasks.
//   core 0: UI task  - scan inputs, app.loop() (pads, knobs, OLED, sequencer edits)
//   core 1: audio task - i2s read (mic) -> app.micIn() -> app.render() 128 frames -> i2s.write
//           (blocks on DMA, paces itself at 44.1 kHz)
// I2S0 runs full duplex with 32-bit slots (BCK 2.82 MHz): the INMP441 needs 64 clocks per frame. The DAC and amp
// take the 16-bit mix in the top half of each slot; the mic sends 24 bits on the left slot (its L/R pin is on GND).
#include <Arduino.h>
#include <ESP_I2S.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "USB.h"
#include "main_app.h"
#include "midi_io.h"
#include "pins.h"

using namespace hugbox;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R2, U8X8_PIN_NONE);
I2SClass i2s;
MainApp app;
static bool pcfOk = false;

static void logLine(const char *s) { Serial.print(s); }
static void *psAlloc(size_t n) { return ps_malloc(n); }

// ---- project files: /p0.hug = autosave, /p1.hug .. /p8.hug = slots. Written to a temp file, then renamed,
// so a power cut mid-save never leaves a broken project.
static bool fsOk = false;
static size_t loadProject(int slot, uint8_t *buf, size_t max) {
  if (!fsOk) return 0;
  char path[16];
  snprintf(path, sizeof path, "/p%d.hug", slot);
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  const size_t n = f.read(buf, max);
  f.close();
  return n;
}
static bool saveProject(int slot, const uint8_t *buf, size_t len) {
  if (!fsOk) return false;
  char path[16], tmp[16];
  snprintf(path, sizeof path, "/p%d.hug", slot);
  snprintf(tmp, sizeof tmp, "/p%d.tmp", slot);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  const bool ok = f.write(buf, len) == len;
  f.close();
  if (!ok) { LittleFS.remove(tmp); return false; }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

// ---- user samples: /s0.hus .. /s15.hus = "HUGS", version, rate, frames, then 16-bit mono. Temp file + rename
// like the projects. Writing stalls the audio task (flash writes pause PSRAM); the app mutes the output first.
struct HusHeader { char magic[4]; uint16_t version, reserved; uint32_t rate, frames; };
static void samplePath(char *out, int slot, const char *ext) { snprintf(out, 16, "/s%d.%s", slot, ext); }
static uint32_t sampleFrames(int slot) {
  if (!fsOk) return 0;
  char path[16];
  samplePath(path, slot, "hus");
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  HusHeader h;
  const bool ok = f.read((uint8_t *)&h, sizeof h) == sizeof h && !memcmp(h.magic, "HUGS", 4) && h.version == 1 &&
                  h.rate == SAMPLE_RATE && f.size() >= sizeof h + h.frames * 2;
  f.close();
  return ok ? h.frames : 0;
}
static uint32_t loadSample(int slot, int16_t *buf, uint32_t max) {
  const uint32_t n = sampleFrames(slot);
  if (!n) return 0;
  char path[16];
  samplePath(path, slot, "hus");
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  f.seek(sizeof(HusHeader));
  const uint32_t want = n < max ? n : max;
  const uint32_t got = f.read((uint8_t *)buf, want * 2) / 2;
  f.close();
  return got;
}
static bool saveSample(int slot, const int16_t *data, uint32_t frames) {
  if (!fsOk) return false;
  char path[16], tmp[16];
  samplePath(path, slot, "hus");
  samplePath(tmp, slot, "tmp");
  // temp file + rename needs room for the old and the new copy; when the flash is that full, drop the old one first
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < sizeof(HusHeader) + frames * 2 + 16384) LittleFS.remove(path);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  HusHeader h = {{'H', 'U', 'G', 'S'}, 1, 0, SAMPLE_RATE, frames};
  bool ok = f.write((const uint8_t *)&h, sizeof h) == sizeof h;
  for (uint32_t i = 0; ok && i < frames; i += 4096) {   // in chunks: PSRAM -> flash
    const uint32_t k = frames - i < 4096 ? frames - i : 4096;
    ok = f.write((const uint8_t *)(data + i), k * 2) == k * 2;
  }
  f.close();
  if (!ok) { LittleFS.remove(tmp); return false; }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}
static void deleteSample(int slot) {
  if (!fsOk) return;
  char path[16];
  samplePath(path, slot, "hus");
  LittleFS.remove(path);
}

static void midiOut(const uint8_t *msg, int len) { midi_io::send(msg, len); }
static void midiDeliver(MidiSource src, const uint8_t *msg, int len) { app.midiIn(src, msg, len); }

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
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

// Rows P0..P3 driven low one at a time; columns P4..P7 read back (pressed = low).
static uint16_t scanMatrix() {
  if (!pcfOk) return 0;
  uint16_t bits = 0;
  for (int r = 0; r < 4; r++) {
    pcfWrite(PCF_COL_MASK | (uint8_t)(~(1u << r) & PCF_ROW_MASK));
    const uint8_t cols = (uint8_t)(~pcfRead() & PCF_COL_MASK) >> 4;
    for (int c = 0; c < 4; c++)
      if (cols & (1u << c)) bits |= 1u << (r * 4 + c);
  }
  pcfWrite(0xFF);
  return bits;
}

// audio load: render time as a share of the 2.9 ms block (128 frames at 44.1 kHz)
static volatile uint32_t renderUsSum = 0, renderUsMax = 0, renderBlocks = 0;

static void audioTask(void *) {
  static int16_t buf[AUDIO_FRAMES * 2];
  static int32_t rx[AUDIO_FRAMES * 2], tx[AUDIO_FRAMES * 2], mic[AUDIO_FRAMES];
  for (;;) {
    // the mic block that arrived with the last one out (same clocks, so this paces like the write)
    if (i2s.readBytes((char *)rx, sizeof rx) == sizeof rx) {
      for (int n = 0; n < AUDIO_FRAMES; n++) mic[n] = rx[2 * n] >> 8;   // left slot, top 24 bits
      app.micIn(mic, AUDIO_FRAMES);
    }
    const int64_t t = esp_timer_get_time();
    app.render(buf, AUDIO_FRAMES);
    const uint32_t us = (uint32_t)(esp_timer_get_time() - t);
    renderUsSum = renderUsSum + us;
    if (us > renderUsMax) renderUsMax = us;
    renderBlocks = renderBlocks + 1;
    for (int i = 0; i < AUDIO_FRAMES * 2; i++) tx[i] = (int32_t)buf[i] << 16;
    i2s.write((uint8_t *)tx, sizeof(tx));
  }
}

static void reportLoad(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 5000 || !renderBlocks) return;
  last = now;
  const float blockUs = AUDIO_FRAMES * 1e6f / SAMPLE_RATE;
  Serial.printf("audio load avg %.0f%% peak %.0f%%  (%u blocks)\n", renderUsSum / (float)renderBlocks / blockUs * 100,
                renderUsMax / blockUs * 100, (unsigned)renderBlocks);
  renderUsSum = 0; renderUsMax = 0; renderBlocks = 0;
}

static void uiTask(void *) {
  const uint8_t potPins[4] = {PIN_POT0, PIN_POT1, PIN_POT2, PIN_POT3};
  Inputs in;
  uint32_t lastScan = 0, lastBle = 0;
  for (;;) {
    const uint32_t now = millis();
    if (now - lastScan >= 5) {                    // 200 Hz: matrix + shift + pots
      lastScan = now;
      in.padBits = scanMatrix();
      in.shift = digitalRead(PIN_SHIFT) == LOW;
      for (int k = 0; k < 4; k++) in.potRaw[k] = analogRead(potPins[k]);
      in.vsenseMv = analogReadMilliVolts(PIN_VBAT_SENSE);
    }
    midi_io::poll(midiDeliver);
    app.loop(now, in);
    if (now - lastBle >= 1000) { lastBle = now; midi_io::setBle(app.bleWanted()); }
    reportLoad(now);
    digitalWrite(LED_BUILTIN, (now / 500) & 1);   // heartbeat (active low)
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_SHIFT, INPUT_PULLUP);
  analogReadResolution(12);
  fsOk = LittleFS.begin(true);                   // formats the data partition on first boot
  Serial.printf("\n=== XO-Hugbox ===\nPSRAM %u KB free\n", ESP.getFreePsram() >> 10);

  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  pcfOk = i2cPresent(PCF8574_ADDR);
  const bool oledOk = i2cPresent(OLED_ADDR);
  Serial.printf("OLED 0x%02X: %s   PCF8574 0x%02X: %s\n", OLED_ADDR, oledOk ? "OK" : "MISSING", PCF8574_ADDR,
                pcfOk ? "OK" : "MISSING");
  if (pcfOk) pcfWrite(0xFF);
  if (oledOk) {
    oled.setI2CAddress(OLED_ADDR << 1);
    oled.begin();
  }

  Platform p;
  p.oled = oledOk ? oled.getU8g2() : nullptr;
  p.pcfOk = pcfOk;
  p.log = logLine;
  p.loadProject = loadProject;
  p.saveProject = saveProject;
  p.midiOut = midiOut;
  p.sampleFrames = sampleFrames;
  p.loadSample = loadSample;
  p.saveSample = saveSample;
  p.deleteSample = deleteSample;
  if (ESP.getPsramSize()) p.alloc = psAlloc;     // sample memory in PSRAM
  if (!fsOk) Serial.println("flash storage FAILED: projects and samples will not be saved");
  else Serial.printf("flash storage: %u KB used of %u KB\n", (unsigned)(LittleFS.usedBytes() >> 10), (unsigned)(LittleFS.totalBytes() >> 10));
  app.begin(p);                                  // renders the built-in kit (~1 s), restores the autosave
  midi_io::begin("XO-Hugbox");                   // USB MIDI class + BLE MIDI service
  USB.productName("XO-Hugbox");
  USB.manufacturerName("XOXO Audio Labs");
  USB.begin();                                   // TinyUSB: serial port + MIDI port

  i2s.setPins(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_DIN);   // full duplex: DAC + amp out, mic in
  i2s.setTimeout(20);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO))
    Serial.println("I2S begin FAILED");
  xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
  xTaskCreatePinnedToCore(uiTask, "ui", 8192, nullptr, 2, nullptr, 0);
}

void loop() { vTaskDelay(portMAX_DELAY); }   // everything runs in the two tasks
