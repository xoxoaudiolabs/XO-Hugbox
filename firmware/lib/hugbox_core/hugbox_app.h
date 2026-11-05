// XO-Hugbox - platform-independent app interface.
//
// Everything in sim/core/ is plain C++ with no Arduino or browser calls, so the same source can be
// built into the ESP32 firmware and into the browser emulator (WASM). The platform layer owns the
// hardware: it scans the PCF8574 matrix, reads the ADC pins, owns the I2S/WebAudio output and the
// U8g2 display object, and calls the app.
//
//   firmware:  loop() -> app.loop(millis(), inputs);  audio task -> app.render(buf, 128) -> i2s.write
//   emulator:  setInterval -> hb_loop(now);           ScriptProcessor -> hb_render(128) -> WebAudio
//
// Later apps (synth engine, sequencer, Standalone/Controller/Layered modes) implement the same
// interface; MIDI in/out will be added to Platform as callbacks so USB/BLE (firmware) and WebMIDI
// (emulator) can sit behind it.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <clib/u8g2.h>

namespace hugbox {

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr int AUDIO_FRAMES = 128;   // frames per I2S DMA block / render call

// Raw hardware state, as the platform last sampled it.
struct Inputs {
  uint16_t padBits = 0;      // bit n = pad n held (n = row*4 + col, row 0 = top, nearest the knobs)
  bool shift = false;        // shift key held
  uint16_t potRaw[4] = {};   // analogRead() of POT0..POT3, 0..4095, clockwise = higher
  uint32_t vsenseMv = 0;     // analogReadMilliVolts(PIN_VBAT_SENSE): half the battery voltage
};

struct Platform {
  u8g2_t *oled = nullptr;                // nullptr = OLED missing on the I2C scan
  bool pcfOk = true;                     // PCF8574 found on the I2C scan
  void (*log)(const char *line) = nullptr;   // Serial.print on the board, serial pane in the emulator
  // big buffers (sample memory): ps_malloc on the board (8 MB PSRAM), malloc in the emulator
  void *(*alloc)(size_t bytes) = malloc;
  void (*release)(void *p) = free;
  // Project storage. Slot 0 = autosave, 1..8 = user slots. loadProject returns bytes read (0 = empty slot).
  // Flash (LittleFS) on the board, localStorage in the emulator. Called from the control core only.
  size_t (*loadProject)(int slot, uint8_t *buf, size_t max) = nullptr;
  bool (*saveProject)(int slot, const uint8_t *buf, size_t len) = nullptr;
  // MIDI out: one complete message (1-3 bytes). The board sends it on USB and BLE. Control core only.
  void (*midiOut)(const uint8_t *msg, int len) = nullptr;
};

enum MidiSource : uint8_t { MIDI_SRC_USB = 0, MIDI_SRC_BLE = 1 };

class App {
 public:
  virtual ~App() {}
  virtual void begin(const Platform &p) = 0;
  virtual void loop(uint32_t nowMs, const Inputs &in) = 0;   // control core, call as often as possible
  virtual void render(int16_t *stereo, int frames) = 0;      // audio core, interleaved L/R
  // One incoming MIDI message (control core). Realtime bytes (clock, start, stop) arrive as len 1.
  virtual void midiIn(MidiSource, const uint8_t *, int) {}
};

}  // namespace hugbox
