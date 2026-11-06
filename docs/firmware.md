# XO-Hugbox firmware

Arduino-ESP32 core 3.x on a Seeed XIAO ESP32-S3, built with PlatformIO (the
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform, which needs PlatformIO Core 6.1.16 or newer).

## Layout

```
firmware/
  lib/hugbox_core/   the instrument, plain C++ (no Arduino calls): shared with the browser emulator
  src/hugbox/        board layer: inputs, OLED, I2S, flash, USB + BLE MIDI, the two FreeRTOS tasks
  src/bringup/       hardware bring-up test
  src/duali2s/       v1.1 speaker-line breadboard test (docs/speaker-path-plan.md)
  include/pins.h     pin map
```

| Env | What it builds |
|---|---|
| `hugbox` (default) | The instrument. |
| `bringup` | I2C scan, pads, knobs, battery and a test tone. Flash this first on a new board. |
| `duali2s` | Board v1.1 test: separate speaker data line and shift on the battery divider. |

```bash
cd firmware
pio run -e hugbox -t upload
pio device monitor
```

The `hugbox` env runs USB in TinyUSB mode, so the USB-C port is a serial port and a class-compliant MIDI port at the
same time. If an upload can't find the port, hold **BOOT** while plugging the board in.

## How it runs

- **Core 1, audio task:** `App::render()` fills 128 frames (2.9 ms) at 44.1 kHz and writes them to I2S. The DAC and the
  speaker amp share the data line on board v1.
- **Core 0, UI task:** scans the PCF8574 key matrix, shift key, knobs and battery at 200 Hz, then runs `App::loop()`
  (pads, knobs, OLED, sequencer edits, autosave, MIDI). It prints the audio load every 5 s.
- The two cores share the model (`model.h`). Events go to the audio core through a lock-free queue; MIDI the audio core
  generates (sequencer, arp, clock) comes back through a second queue, so USB/BLE calls never run on the audio core.
- `Platform` (`hugbox_app.h`) is the only hardware interface: OLED handle, big-buffer allocator (PSRAM), project
  and sample storage and MIDI out. The mic comes in through `App::micIn()`, called by the audio task before each render. The emulator (`sim/wasm_main.cpp`) implements the same interface in the browser.

## The instrument (`lib/hugbox_core`)

| File | Contents |
|---|---|
| `main_app.*` | Views, status line, knob pages with pick-up, shift layer and shortcut map, sequencer clock, chord mode, arpeggiator, modes. The control map is at the top of `main_app.h`. |
| `model.*` | Parameter table, knob pages, scales, chord builder, steps, project file format. |
| `engine.*`, `dsp.h` | 6-voice synth (PolyBLEP saw/square, sine, noise, 2-op FM, TPT state-variable filter with envelope and keytracking, ADSR, drift, glide) and the 16-pad sample player (pitch, decay, filter, level, pan, choke groups). |
| `kit.cpp` | Built-in drum kit: 16 one-shots synthesized into PSRAM at boot (~0.8 MB). |
| `fx.*` | Drive, bitcrush, sample-rate reduction, tempo-synced ping-pong delay, low cut, then a tanh limiter. |
| `io.cpp` | Projects (autosave + 8 slots, SETTINGS pad grid) and MIDI (notes, knob CCs, clock in/out). |
| `mic.cpp` | Mic sampling: the recorder (audio core), the sample editor (a port of XOXO Studio's), user samples on flash, the SAMPLE view. See [mic-plan.md](mic-plan.md). |
| `bringup_app.*` | The bring-up test as an app, for the emulator. |

### Controls

- **Views:** PLAY, SEQ, SOUND, FX, SETTINGS, SAMPLE. **Tracks:** DRUM, SYNTH. **Modes:** Standalone, Controller (MIDI only), Layered.
- **Shift + pad:** top row PLAY · SEQ · SOUND (again = next page) · FX; row 2 sequencer pages 1-4; row 3 DRUM · SYNTH ·
  run/stop · record (held 2 s: beep, then a sample take); bottom row Standalone · Controller · Layered · SETTINGS. Holding Shift flips the knobs to their
  second four without changing the screen; double-tapping Shift shows the shortcut map.
- **Knob pick-up:** a value only follows the knob once the knob passes it, so values never jump when pages change.
- **Sequencer:** 64 steps (4 pages × 16), swing, pattern length, live recording, per-step velocity/pitch/gate/probability.
- **Chords and arp** (synth): chords built from the scale with inversions; arp up/down/up-down/random/as-played,
  1/4-1/32 with triplets, 1-3 octaves, gate, latch, locked to the sequencer grid.
- **SETTINGS pads:** S1-S8 (tap = load, hold = save), copy/paste/clear page, clear track (hold), mute drums/synth,
  tap tempo, MIDI panic. Knobs: synth MIDI channel, drum MIDI channel, clock source, Bluetooth; Shift: brightness,
  mic start threshold, mic monitor.
- **SAMPLE:** Shift + REC held 2 s (Shift can be let go) beeps, then records the mic onto the selected pad until REC is
  let go (up to 20 s). In SAMPLE a pad held 0.25 s does the same onto that pad; a tap selects and plays. The take is trimmed to the
  sound and opens in the sample editor: START, END, fade in, fade out | Shift: gain, normalise, reverse, USER/KIT.
  Recorded pads show as U01-U16. Saved to flash on leaving the view.

### Projects

Everything autosaves 3 s after the last change and is restored at power-on; 8 slots hold more. Files live on the
LittleFS data partition (`/p0.hug` autosave, `/p1.hug`-`/p8.hug`), written to a temp file and renamed so a power cut
can't corrupt one. The format is versioned and tolerant of parameters being added later. Loading a slot keeps the
device settings (MIDI channels, sync, Bluetooth, brightness, mic gain/threshold/monitor).

### Samples and flash

Recorded samples (`/s0.hus`-`/s15.hus`) are global, not per project: up to ~52 s in total. The custom partition
table (`partitions_hugbox.csv`: 3 MB app, 4.9 MB LittleFS) replaces the default one, so **the first flash of this
firmware erases the file system once** (saved projects too). Writing a sample pauses the audio, so the firmware only
saves when you leave SAMPLE, with the output muted.

### MIDI

- **Out** (Controller and Layered modes): synth notes on the synth channel, drums on the drum channel (default 10) with
  General MIDI note numbers, sequencer and arp notes with timed note-offs, clock (24 ppqn) and start/stop when the
  clock source is internal. In Controller mode the knobs send CC 70-73, or 74-77 with Shift.
- **In:** notes on the synth or drum channel play the engine. With the clock source set to USB or BLE it follows
  incoming clock (6 clocks per step), start and stop, and estimates the tempo.
- **Transports:** USB (TinyUSB MIDI) and Bluetooth LE MIDI (the standard BLE-MIDI service on NimBLE-Arduino,
  advertised as "XO-Hugbox"), both at once.

## Known limits

- Nothing has run on hardware yet.
- On board v1 the speaker and headphone outputs share one I2S data line, so the low cut affects both. Board v1.1
  fixes this; see [speaker-path-plan.md](speaker-path-plan.md).
- The mic path (full-duplex I2S on GPIO41) and the flash-write pause are untested until the v1.1 + mic breadboard
  test; see [mic-plan.md](mic-plan.md) section 5.
