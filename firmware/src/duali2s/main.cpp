// XO-Hugbox v1.1 breadboard test: separate speaker data line (docs/speaker-path-plan.md section 3).
//
// Wiring (v1.1): BCK D6 + LRCK D7 to BOTH modules; PCM5102A DIN <- D8; MAX98357A DIN <- D10;
// R1/R2 100k divider from the battery (+) to GND with its midpoint on D9; the shift switch from D9 to GND.
//
// What it does
//   I2S0 = master TX on D6/D7/D8  -> PCM5102A jack: 1 kHz sine
//   I2S1 = slave  TX, data on D10 -> MAX98357A speaker: 200 Hz sine, clocked by I2S0's BCK/WS pins
//          (the pads stay I2S0 outputs; their input buffers feed I2S1's clock inputs through the GPIO matrix)
// Serial commands (115200):
//   a = dual mode (default)      f = fallback: D10 carries I2S0's data (same 1 kHz as the jack)
//   m = mute/unmute speaker      s = swap the two tones       l = louder/quieter toggle
// Every 2 s it prints: blocks written per channel, slave write timeouts (should stay 0 = locked),
// D9 millivolts, shift state, battery estimate.
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_rom_gpio.h"
#include "soc/i2s_periph.h"
#include "pins.h"

static constexpr uint32_t FS = 44100;
static constexpr int FRAMES = 128;
static const gpio_num_t G_BCK = (gpio_num_t)PIN_I2S_BCK;
static const gpio_num_t G_WS = (gpio_num_t)PIN_I2S_LRCK;
static const gpio_num_t G_DOUT = (gpio_num_t)PIN_I2S_DOUT;
static const gpio_num_t G_SPK = (gpio_num_t)D10;          // v1.1: speaker data line
static const int PIN_SENSE = PIN_VBAT_SENSE;               // v1.1: battery divider + shift switch
static constexpr int SHIFT_ON_MV = 600, SHIFT_OFF_MV = 900;

static i2s_chan_handle_t tx0 = nullptr, tx1 = nullptr;
static volatile bool fallback = false, spkMute = false, swapTones = false, loud = false;
static volatile uint32_t blocks0 = 0, blocks1 = 0, timeouts1 = 0;

static bool check(esp_err_t e, const char *what) {
  if (e != ESP_OK) Serial.printf("  %s FAILED: %s\n", what, esp_err_to_name(e));
  return e == ESP_OK;
}

static i2s_std_config_t stdConfig(gpio_num_t dout) {
  i2s_std_config_t c = {};
  c.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(FS);
  c.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  c.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  c.gpio_cfg.bclk = G_BCK;
  c.gpio_cfg.ws = G_WS;
  c.gpio_cfg.dout = dout;
  c.gpio_cfg.din = I2S_GPIO_UNUSED;
  return c;
}

static bool startAudio() {
  // slave first: its init claims BCK/WS as inputs; the master init then turns them into outputs
  i2s_chan_config_t c1 = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
  c1.dma_frame_num = FRAMES;
  c1.auto_clear = true;
  i2s_chan_config_t c0 = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  c0.dma_frame_num = FRAMES;
  c0.auto_clear = true;
  bool ok = check(i2s_new_channel(&c1, &tx1, nullptr), "new slave channel");
  ok &= check(i2s_new_channel(&c0, &tx0, nullptr), "new master channel");
  if (!ok) return false;
  i2s_std_config_t s1 = stdConfig(G_SPK), s0 = stdConfig(G_DOUT);
  ok &= check(i2s_channel_init_std_mode(tx1, &s1), "slave init");
  ok &= check(i2s_channel_init_std_mode(tx0, &s0), "master init");
  if (!ok) return false;
  // The master init left BCK/WS output-only. Turn their input buffers back on and re-attach them to I2S1's
  // TX clock inputs, so the slave sees the master's clocks on the very same pads.
  gpio_input_enable(G_BCK);
  gpio_input_enable(G_WS);
  esp_rom_gpio_connect_in_signal(G_BCK, i2s_periph_signal[1].s_tx_bck_sig, false);
  esp_rom_gpio_connect_in_signal(G_WS, i2s_periph_signal[1].s_tx_ws_sig, false);
  // pre-fill both DMA queues equally so they start in step
  static int16_t zero[FRAMES * 2] = {};
  size_t w;
  for (int i = 0; i < 4; i++) {
    i2s_channel_preload_data(tx1, zero, sizeof zero, &w);
    i2s_channel_preload_data(tx0, zero, sizeof zero, &w);
  }
  ok &= check(i2s_channel_enable(tx1), "slave enable");
  ok &= check(i2s_channel_enable(tx0), "master enable");
  return ok;
}

// D10 output selector: either I2S1's data (dual) or a copy of I2S0's data (fallback).
static void routeSpeaker(bool useFallback) {
  esp_rom_gpio_connect_out_signal(G_SPK, useFallback ? i2s_periph_signal[0].data_out_sig : i2s_periph_signal[1].data_out_sig,
                                  false, false);
  Serial.printf("speaker line D10 <- %s\n", useFallback ? "I2S0 data (fallback: same as the jack)" : "I2S1 data (own stream)");
}

static void audioTask(void *) {
  static int16_t b0[FRAMES * 2], b1[FRAMES * 2];
  float p0 = 0, p1 = 0;
  for (;;) {
    const float f0 = swapTones ? 200.0f : 1000.0f, f1 = swapTones ? 1000.0f : 200.0f;
    const float g = loud ? 0.5f : 0.15f;
    for (int i = 0; i < FRAMES; i++) {
      const int16_t a = (int16_t)(sinf(p0 * 6.2831853f) * g * 32767), b = (int16_t)(sinf(p1 * 6.2831853f) * g * 32767);
      p0 += f0 / FS; if (p0 >= 1) p0 -= 1;
      p1 += f1 / FS; if (p1 >= 1) p1 -= 1;
      b0[2 * i] = b0[2 * i + 1] = a;
      b1[2 * i] = b1[2 * i + 1] = spkMute ? 0 : b;
    }
    size_t w;
    i2s_channel_write(tx0, b0, sizeof b0, &w, portMAX_DELAY);   // paced by the master clock
    blocks0 = blocks0 + 1;
    // the slave only drains if it receives the master's clocks: a timeout here means it is not locked
    if (i2s_channel_write(tx1, b1, sizeof b1, &w, pdMS_TO_TICKS(20)) == ESP_OK && w == sizeof b1) blocks1 = blocks1 + 1;
    else timeouts1 = timeouts1 + 1;
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  analogReadResolution(12);
  Serial.println("\n=== XO-Hugbox v1.1 dual-I2S breadboard test ===");
  Serial.println("jack: 1 kHz   speaker: 200 Hz   (a=dual f=fallback m=mute spk s=swap l=level)");
  if (!startAudio()) { Serial.println("audio start failed - see above"); return; }
  routeSpeaker(false);
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 1);
}

void loop() {
  static bool shift = false;
  static uint32_t lastScan = 0, lastReport = 0, vbatMv = 0;
  static int confirm = 0;
  const uint32_t now = millis();
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'a': fallback = false; routeSpeaker(false); break;
      case 'f': fallback = true; routeSpeaker(true); break;
      case 'm': spkMute = !spkMute; Serial.printf("speaker stream %s\n", spkMute ? "muted" : "on"); break;
      case 's': swapTones = !swapTones; Serial.println(swapTones ? "jack 200 Hz, speaker 1 kHz" : "jack 1 kHz, speaker 200 Hz"); break;
      case 'l': loud = !loud; Serial.printf("level %s\n", loud ? "-6 dBFS" : "-16 dBFS"); break;
    }
  }
  if (now - lastScan >= 5) {                       // shift on the divider node, 2-scan confirm + hysteresis
    lastScan = now;
    const uint32_t mv = analogReadMilliVolts(PIN_SENSE);
    const bool raw = shift ? mv < SHIFT_OFF_MV : mv < SHIFT_ON_MV;
    if (raw != shift) {
      if (++confirm >= 2) { shift = raw; confirm = 0; Serial.printf("shift %s (D9 %u mV)\n", shift ? "DOWN" : "up", mv); }
    } else {
      confirm = 0;
      if (!shift) vbatMv = mv * 2;                 // battery only while shift is up
    }
  }
  if (now - lastReport >= 2000) {
    lastReport = now;
    Serial.printf("%s  I2S0 blocks %u  I2S1 blocks %u  slave timeouts %u  |  D9 %u mV  shift %s  battery %.2f V\n",
                  fallback ? "[fallback]" : "[dual]", (unsigned)blocks0, (unsigned)blocks1, (unsigned)timeouts1,
                  (unsigned)analogReadMilliVolts(PIN_SENSE), shift ? "DOWN" : "up", vbatMv / 1000.0f);
  }
  delay(1);
}
