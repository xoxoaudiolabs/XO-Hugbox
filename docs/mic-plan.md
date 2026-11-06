# Plan: built-in mic for recording samples (INMP441)

**Goal:** record your own sounds onto the pads with a built-in mic, keep them across power-offs, and play them
like the built-in kit (pitch, decay, filter, sequencer, chords all apply).
**Status:** board footprint and plate pocket done (v1 and v1.1), redone for the real module's 2×3 pins; firmware not written yet. The pads on the back of the XIAO have been checked: MTCK, MTDO, MTDI and
MTMS are there. The mic module is not ordered yet; it is a **new parts-list line** (INMP441 breakout, about $2.40 each in a 5-pack).

## 1. How it connects

The INMP441 is an I2S microphone. It listens to the same bit clock and word clock as the outputs, and sends its own
data line back. All 11 header pins on the XIAO are already used, so the data line comes from a spare pad on the
XIAO's underside.

| INMP441 pin | Goes to | Notes |
|---|---|---|
| SCK | `BCK` (D6) | shared with the DAC and amp |
| WS | `LRCK` (D7) | shared |
| SD | XIAO **MTDI pad, GPIO41** | through J_MIC to J_MTDI, then a wire soldered to the XIAO's pad before the XIAO is mounted, like the BAT leads |
| L/R | GND | mic sends on the left slot |
| VDD | 3V3 | 1.4 mA |
| GND | GND | |

The ESP32-S3 can transmit and receive on one I2S port with shared clocks (full duplex), so I2S0 sends to the
DAC/amp on D8 and receives the mic on GPIO41. On board v1.1 the separate speaker stream (I2S1, slave, D10) is
unaffected.

**One firmware requirement:** the INMP441 needs 64 bit-clocks per frame (32-bit slots). The output currently
uses 16-bit slots, so it moves to 32-bit. The PCM5102A and MAX98357A both accept that; the bit clock goes from
1.41 to 2.82 MHz.

## 2. Board: J_MIC footprint (done on v1 and v1.1, no new board parts)

The module (from the listing photos): round, ~14 mm, two notches; **2 rows of 3 pins**, 2.54 mm pitch, rows 7.62 mm
apart. One row is SCK WS L/R, the other SD VDD GND. The INMP441 is a **bottom-port** mic: the sound port is the hole
in the middle of the **label side**, and the mic can sits on the other side over it.

So the module mounts **label side up, straight on the main board** with its own header strip:
- **J_MIC:** a 2×3 footprint centred under the plate's sound hole (52, 94). Seen from the top: SCK WS L/R row toward
  the pots, SD VDD GND row toward the keys. Only that orientation matches the labels (turning the module round puts SD
  where L/R should be). SCK and WS join `BCK`/`LRCK`, L/R and GND go to GND, VDD to 3V3, SD to `MIC_SD`. A silk circle
  shows the module outline.
- **Height:** the header plastic (2.5 mm) sits on the board, so the module board spans 2.5–3.5 mm above the board and
  the can hangs below it with ~1.5 mm to spare. The plate's underside is 1.0 mm above the board.
- **J_MTDI:** 1 plated hole just below the XIAO (11.5, 80.5), on the `MIC_SD` net to J_MIC's SD hole. The fly-wire
  from the XIAO's MTDI pad lands here.

**Before ordering:** measure the row spacing on the real module (it is read off a photo). It is the one mic number that
is in copper; everything else only changes the plate.

## 3. Plate: mic pocket and sound hole (faceplate only)

- **Position:** between the key grid and the pot strip, at x 52, y 94 in plate coordinates (clear of the switch cut-outs
  and the pot strip, about 50 mm from the speaker).
- **Pocket** in the plate's underside: round, module diameter + 0.8 mm, **3.0 mm deep**. That is the label face (2.5 mm
  into the plate) plus 0.5 mm for the clipped pin joints and a foam ring. The roof is 1.0 mm.
- **Sound hole:** 1.5 mm through the roof, over the port. A small ring of thin foam tape around the port seals it
  to the roof, so the mic hears the hole and not the cavity.
- **Measure when the module arrives:** row spacing (PCB), diameter and board thickness (plate). Those go in
  `layout.py` as `MIC_*`.

## 4. Firmware

**Board layer (`src/hugbox/main.cpp`, `pins.h`):**
- `PIN_I2S_DIN 41`; `i2s.setPins(BCK, LRCK, DOUT, DIN)`; `I2S_DATA_BIT_WIDTH_32BIT`. The audio task converts
  `render()`'s 16-bit output to 32-bit (`<< 16`), and reads one block of mic frames each loop.
- v1.1 (`duali2s` path): add the RX channel to I2S0 in the same `i2s_std` setup.
- Custom partition table: one app slot of 3 MB (no OTA; USB flashing) and **LittleFS of about 4.9 MB**. That is
  about 55 s of 44.1 kHz/16-bit mono in total. **Changing partitions erases the saved projects once**: say so in
  the release notes.

**Core (`lib/hugbox_core`, shared with the emulator):**
- `App::micIn(const int16_t *mono, int frames)`: called by the audio task before `render()`, same block.
- Input chain: left slot, top 24 bits → gain (`MGN`, 0 to +30 dB; the INMP441 is quiet, -26 dBFS at 94 dB SPL)
  → 20 Hz DC-blocking high-pass → 16-bit.
- Recorder (audio core): one buffer in PSRAM, **20 s max** (1.76 MB), a 50 ms pre-roll ring so the attack is never
  cut. Ends with auto-trim: start at the threshold minus the pre-roll, cut trailing silence, 5 ms fades, peak-normalise
  to -1 dBFS (optional, on by default).
- `Sampler`: a pad can hold a user sample instead of its built-in kit sound. The kit stays in memory, so clearing
  a user sample brings the original back.
- `Platform`: `loadSample(pad, buf, max)`, `saveSample(pad, data, frames)`, `deleteSample(pad)`. File
  `/s<pad>.hus`: header "HUGS", version, frame count, name; then 16-bit mono at 44.1 kHz.
- Samples are global (a user kit), not per project. Projects keep pointing at pads, so the project format does not change.

**UI (keeps the rules: holding Shift never changes the screen, knob pick-up):**
- **Opening the SAMPLE view:** **Shift + REC held for 0.6 s**. A quick tap is still sequencer record, as now. The
  shortcut map shows "REC / hold: SAMPLE".
- **Screen:** an input level meter with the threshold line, the target pad, and the length used/free.
- **Knobs:** 1 input gain, 2 threshold (OFF = start on press), 3 trim start, 4 trim end (after a take).
  Shift-knobs: normalise on/off, monitor on/off.
- **Record:** **hold a pad**. Recording starts on press (or at the threshold) and stops on release, or at 20 s.
  The take plays back once at once. Hold the same pad again to retake.
- **Save and revert:** saving writes to flash when you leave the view, or with Shift + the pad. The CLR cell in the
  SAMPLE view reverts a pad to its built-in sound.
- **Speaker:** muted while recording to avoid feedback. Headphones keep monitoring.
- **Names:** the DRUM page shows user samples as `U01`–`U16` in place of the kit names.

**Emulator:** the browser mic through `getUserMedia` → `hb_mic_in`, with a "Use mic" button (it needs permission).
Samples go in IndexedDB, since localStorage is too small. You can try the whole feature before the module arrives.

## 5. Validate (with the v1.1 breadboard test, before the PCB order)

Add the INMP441 to the v1.1 breadboard and a `mictest` env: 1 kHz on the jack, mic → 3 s capture → playback,
and print the peak/RMS levels and the noise floor.
- [ ] Mic data on GPIO41 while I2S0 also drives the DAC (full duplex), clean at 32-bit slots.
- [ ] Together with the I2S1 speaker slave (v1.1), with no lost lock.
- [ ] Noise floor and level at a normal speaking distance; settles the gain range.
- [ ] Saving 1 MB to flash: how long the audio pauses (see risks).

## Risks / unknowns

- **Flash writes pause the audio.** On the ESP32-S3, writing flash briefly disables the cache that PSRAM also
  uses, so the audio task stalls. Plan: save only on leaving the SAMPLE view or on Shift + pad, mute for the
  write, and show "SAVING…". About 2 s for a 10 s sample. The test measures this.
- **Plate acoustics:** a small cavity behind a pinhole can ring. The foam ring and a short, wide hole keep it down.
  It's a reprint if it sounds boxy.
- **Pad noise:** key clicks travel through the plate into the mic. Starting on the threshold, plus the pre-roll,
  helps. Recording on release is an option if it's bad.
- **PSRAM budget:** kit 0.8 MB + recorder 1.76 MB + user samples up to 4.9 MB = 7.5 of 8 MB. The cap is the flash
  size, so they can't all overflow at once.
- **Sound quality:** 61 dB SNR is fine for lo-fi sampling, not for hi-fi.
