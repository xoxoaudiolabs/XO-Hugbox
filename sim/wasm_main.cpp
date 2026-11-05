// Browser platform layer: plain C exports around the shared apps, called from sim/index.html.
// The OLED is a real U8g2 SH1106 128x64 full-buffer object with U8G2_R2, exactly like the firmware;
// only the byte transport is a no-op. JS reads the frame buffer (panel orientation) and draws it.
//
// hb_init(oledOk, pcfOk, app): app 0 = main UI firmware (main_app), 1 = hardware bring-up (bringup_app).
#include <emscripten/emscripten.h>
#include <string.h>
#include "bringup_app.h"
#include "main_app.h"

using namespace hugbox;

EM_JS(void, js_log, (const char *s), { if (Module.onLog) Module.onLog(UTF8ToString(s)); });

// Projects live in localStorage ("hugbox-project-<slot>", base64), the emulator's stand-in for the board's flash.
EM_JS(int, js_load_project, (int slot, uint8_t *buf, int max), {
  try {
    const s = localStorage.getItem("hugbox-project-" + slot);
    if (!s) return 0;
    const bin = atob(s);
    const n = Math.min(bin.length, max);
    for (let i = 0; i < n; i++) HEAPU8[buf + i] = bin.charCodeAt(i);
    return n;
  } catch (e) { return 0; }
});
EM_JS(int, js_save_project, (int slot, const uint8_t *buf, int len), {
  try {
    let bin = "";
    for (let i = 0; i < len; i++) bin += String.fromCharCode(HEAPU8[buf + i]);
    localStorage.setItem("hugbox-project-" + slot, btoa(bin));
    if (Module.onProjectSaved) Module.onProjectSaved(slot, len);
    return 1;
  } catch (e) { return 0; }
});
EM_JS(void, js_midi_out, (int b0, int b1, int b2, int len), {
  if (Module.onMidiOut) Module.onMidiOut(len === 1 ? [b0] : len === 2 ? [b0, b1] : [b0, b1, b2]);
});

static size_t loadProject(int slot, uint8_t *buf, size_t max) { return (size_t)js_load_project(slot, buf, (int)max); }
static bool saveProject(int slot, const uint8_t *buf, size_t len) { return js_save_project(slot, buf, (int)len) != 0; }
static void midiOut(const uint8_t *m, int len) { js_midi_out(m[0], len > 1 ? m[1] : 0, len > 2 ? m[2] : 0, len); }

static uint8_t byteCb(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }
static uint8_t gpioCb(u8x8_t *, uint8_t, uint8_t, void *) { return 1; }

static u8g2_t oled;
static MainApp mainApp;
static BringupApp bringup;
static App *app = &mainApp;
static int appIndex = 0;
static Inputs in;
static int16_t audioBuf[AUDIO_FRAMES * 2 * 8];
static uint8_t panel[1024];   // what the SH1106 RAM holds after the last SendBuffer
static uint32_t panelFrame = ~0u;

static uint32_t appFrames() { return appIndex == 0 ? mainApp.frames() : bringup.frames(); }

extern "C" {

EMSCRIPTEN_KEEPALIVE void hb_init(int oledOk, int pcfOk, int which) {
  u8g2_Setup_sh1106_i2c_128x64_noname_f(&oled, U8G2_R2, byteCb, gpioCb);
  u8g2_SetI2CAddress(&oled, 0x3C << 1);
  u8g2_InitDisplay(&oled);
  u8g2_SetPowerSave(&oled, 0);
  appIndex = which == 1 ? 1 : 0;
  app = appIndex == 0 ? (App *)&mainApp : (App *)&bringup;
  Platform p;
  p.oled = oledOk ? &oled : nullptr;
  p.pcfOk = pcfOk != 0;
  p.log = js_log;
  p.loadProject = loadProject;
  p.saveProject = saveProject;
  p.midiOut = midiOut;
  app->begin(p);
}

// MIDI from the page (WebMIDI input or the test buttons). src 0 = USB, 1 = BLE (for the SYNC setting).
EMSCRIPTEN_KEEPALIVE void hb_midi_in(int src, int b0, int b1, int b2, int len) {
  const uint8_t m[3] = {(uint8_t)b0, (uint8_t)b1, (uint8_t)b2};
  if (appIndex == 0) mainApp.midiIn(src ? MIDI_SRC_BLE : MIDI_SRC_USB, m, len);
}

EMSCRIPTEN_KEEPALIVE void hb_set_inputs(int padBits, int shift, int p0, int p1, int p2, int p3, int vsenseMv) {
  in.padBits = (uint16_t)padBits;
  in.shift = shift != 0;
  in.potRaw[0] = p0; in.potRaw[1] = p1; in.potRaw[2] = p2; in.potRaw[3] = p3;
  in.vsenseMv = vsenseMv;
}

EMSCRIPTEN_KEEPALIVE void hb_loop(uint32_t nowMs) { app->loop(nowMs, in); }

// Renders frames (multiple of AUDIO_FRAMES, at most 8 blocks) in AUDIO_FRAMES blocks like the I2S task.
EMSCRIPTEN_KEEPALIVE int16_t *hb_render(int frames) {
  if (frames > AUDIO_FRAMES * 8) frames = AUDIO_FRAMES * 8;
  for (int f = 0; f < frames; f += AUDIO_FRAMES) app->render(audioBuf + 2 * f, AUDIO_FRAMES);
  return audioBuf;
}

// SH1106 RAM image: 8 pages x 128 columns, bit 0 = top pixel of the page, in panel orientation.
EMSCRIPTEN_KEEPALIVE uint8_t *hb_panel() {
  if (appFrames() != panelFrame) {
    panelFrame = appFrames();
    memcpy(panel, u8g2_GetBufferPtr(&oled), sizeof panel);
  }
  return panel;
}
EMSCRIPTEN_KEEPALIVE uint32_t hb_frames() { return appFrames(); }
EMSCRIPTEN_KEEPALIVE float hb_tone_hz() { return appIndex == 1 ? bringup.toneHz() : 0; }
EMSCRIPTEN_KEEPALIVE float hb_tone_gain() { return appIndex == 1 ? bringup.toneGain() : 0; }
EMSCRIPTEN_KEEPALIVE int hb_app() { return appIndex; }
EMSCRIPTEN_KEEPALIVE const char *hb_status() { return appIndex == 0 ? mainApp.status() : ""; }
// Screen brightness setting 0..100 (the firmware sends it to the panel with u8g2_SetContrast; the emulator dims pixels).
EMSCRIPTEN_KEEPALIVE void hb_set_param(int id, int v) { mainApp.setParam(id, v); }
// Live knob map for the emulator's guide panel.
EMSCRIPTEN_KEEPALIVE int hb_knob_param(int slot) { return appIndex == 0 ? mainApp.knobParam(slot) : 255; }
EMSCRIPTEN_KEEPALIVE const char *hb_param_label(int id) { return id >= 0 && id < P_COUNT ? PARAMS[id].label : ""; }
EMSCRIPTEN_KEEPALIVE const char *hb_param_name(int id) { return id >= 0 && id < P_COUNT ? PARAMS[id].name : ""; }
EMSCRIPTEN_KEEPALIVE const char *hb_param_text(int id) { static char s[24]; mainApp.paramText(id, s); return s; }
EMSCRIPTEN_KEEPALIVE int hb_view() { return appIndex == 0 ? mainApp.view() : -1; }
EMSCRIPTEN_KEEPALIVE int hb_track() { return appIndex == 0 ? mainApp.track() : -1; }
EMSCRIPTEN_KEEPALIVE int hb_pad_note(int pad) { return appIndex == 0 ? mainApp.padNote(pad) : -1; }
EMSCRIPTEN_KEEPALIVE int hb_param(int id) { return mainApp.param(id); }
EMSCRIPTEN_KEEPALIVE int hb_brightness() { return appIndex == 0 ? mainApp.brightness() : 100; }

}  // extern "C"
