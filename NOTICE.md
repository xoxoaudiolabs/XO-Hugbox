# Third-party components

XO-Hugbox builds on these projects. Their licenses apply to their code wherever it appears in builds of this repository.

| Component | Used in | License |
|---|---|---|
| [U8g2](https://github.com/olikraus/u8g2) by olikraus (code + fonts) | Firmware (library dependency); **compiled into** the emulator (`sim/src/hugbox_core.js`, `sim/index.html`) | BSD-2-Clause; fonts under their own terms (the X11 Adobe/DEC notice applies to the Helvetica font used on screen). Full text: [`LICENSES/U8g2-LICENSE.txt`](LICENSES/U8g2-LICENSE.txt) |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) core and [ESP-IDF](https://github.com/espressif/esp-idf) | Firmware (framework, linked into the binary) | LGPL-2.1-or-later (Arduino core) and Apache-2.0 (ESP-IDF) |
| [pioarduino platform-espressif32](https://github.com/pioarduino/platform-espressif32) | Firmware build | Apache-2.0 |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Firmware (BLE MIDI) | Apache-2.0 |
| [Emscripten](https://emscripten.org/) runtime | Emulator (`sim/src/hugbox_core.js`) | MIT / University of Illinois/NCSA |
| [three.js](https://threejs.org/) | Build guide (loaded from the jsDelivr CDN, not included) | MIT |
| [Anton](https://fonts.google.com/specimen/Anton) and [IBM Plex Mono](https://fonts.google.com/specimen/IBM+Plex+Mono) | Emulator and build guide (loaded from Google Fonts, not included) | SIL Open Font License 1.1 |
| [KiCad](https://www.kicad.org/) footprints | PCB (footprint geometry in `hardware/pcb/*.kicad_pcb`) | CC-BY-SA-4.0 with the KiCad library exception (footprints may be used in designs under any license) |

Tools used to produce the design files but not included: KiCad 7, Freerouting, OpenSCAD, PlatformIO.
