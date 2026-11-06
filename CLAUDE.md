# XO-Hugbox: notes for developers and AI assistants

Battery-powered pocket groovebox and MIDI controller from XOXO Audio Labs: 4×4 MX pads, 4 knobs, shift, 1.3" OLED,
speaker + headphone out, USB-C and BLE MIDI, Seeed XIAO ESP32-S3. Companion to XOXO Studio and the FM-1 Companion Studio.

Read first: [README.md](README.md), [docs/hardware.md](docs/hardware.md), [docs/firmware.md](docs/firmware.md).

## Status

- Hardware design complete, nothing built yet. Board v1 gerbers are the order candidate; v1.1 (separate speaker data
  line) waits on the breadboard test in [docs/speaker-path-plan.md](docs/speaker-path-plan.md).
- Firmware stages done in code: UI, synth + sampler, projects/sequencer extras, FX, USB + BLE MIDI. None run on hardware.
- Both boards carry pads for an INMP441 mic (J_MIC, plus J_MTDI for a wire from the XIAO's GPIO41 pad); the plate has
  its pocket. Mic firmware is not written yet: see [docs/mic-plan.md](docs/mic-plan.md).
- Next: bring-up on real parts (check the "audio load" serial line first), the v1.1 + mic breadboard test, then
  mic recording and user samples from flash.

## Rules of the road

- `hardware/layout.py` is the single source of truth for positions and dimensions. Change it, then regenerate the
  board, plate/shell, build guide data and emulator layout (see docs/hardware.md).
- Never edit generated files by hand: `sim/index.html`, `sim/src/hugbox_core.js`, `sim/src/layout.js`,
  `docs/build-guide/index.html`, `docs/build-guide/src/data.js`, the `.scad`/`.stl` and gerber outputs.
- All instrument behaviour lives in `firmware/lib/hugbox_core` (plain C++, no Arduino calls). After changing it,
  rebuild both the firmware (`pio run -e hugbox`) and the emulator (`sim/build_wasm.sh`, emscripten) so they stay identical.
- The audio core must never call platform MIDI/USB/BLE functions: queue to the control core (see `io.cpp`).
- Keep the UI rules: holding Shift never changes the screen; knob pick-up; FX/SETTINGS + track shortcut jumps to PLAY.
- Board revisions may only use parts already in the parts list (no new BOM lines).
- Line endings are LF (`.gitattributes`); the build scripts run under Linux/WSL.

## Toolchain

- Firmware: PlatformIO Core >= 6.1.16, pioarduino 54.03.20 (Arduino-ESP32 3.2), U8g2, NimBLE-Arduino 2.x.
- Emulator: emscripten (tested with Ubuntu 24.04's package), Python 3 for the bundler, Node for headless tests.
- Hardware: KiCad 7.0 Python + `kicad-footprints`, Freerouting 2.1.0 (Java 21), OpenSCAD for STLs and renders.
