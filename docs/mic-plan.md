# Plan: built-in mic for recording samples (INMP441)

**Goal:** record your own sounds onto the pads with a built-in mic, keep them across power-offs, and play them
like the built-in kit (pitch, decay, filter, sequencer, chords all apply).
**Status:** board footprint and plate pocket done (v1 and v1.1), redone for the real module's 2×3 pins;
firmware done in code and in the emulator (section 4), not run on hardware yet; the pads on the back of the XIAO have been checked: MTCK, MTDO, MTDI and
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

## 4. Firmware (done in code; not run on hardware yet)

**Board layer (`src/hugbox/main.cpp`, `pins.h`, `partitions_hugbox.csv`):**
- `PIN_I2S_DIN 41`. I2S0 runs full duplex with 32-bit slots: the audio task reads a mic block (left slot, top 24 bits),
  calls `app.micIn()`, renders, shifts the 16-bit mix into the top of each 32-bit slot and writes it.
- v1.1's separate speaker stream (`duali2s` test) is not in the main firmware yet; when it is, it adds the RX side the
  same way.
- Partition table: one 3 MB app (no OTA; USB flashing) and **LittleFS of 4.9 MB**. User samples may use up to
  ~52 s (4.6 MB) in total, which leaves room for the projects and for rewriting a slot. **Changing partitions erases
  the saved projects once**: say so in the release notes.
- Sample files `/s<slot>.hus`: "HUGS", version 1, rate, frame count (16 bytes), then 16-bit mono at 44.1 kHz.
  Temp file + rename like the projects (or old file first when the flash is nearly full).

**Core (`lib/hugbox_core/mic.cpp`, shared with the emulator):**
- `App::micIn(const int32_t *mono24, int frames)`: the audio core, before `render()`, same block.
- Input chain: gain (`GAIN`, 0 to +40 dB, default +24; the INMP441 is quiet, -26 dBFS at 94 dB SPL)
  → 20 Hz DC-blocking high-pass → 16-bit.
- Recorder (audio core): one 20 s buffer in PSRAM (1.76 MB), allocated the first time SAMPLE opens, and a
  **300 ms pre-roll** ring. A pad press only arms a take after 250 ms (a shorter press is a tap: select and play);
  the pre-roll covers that delay, so the attack is never cut. With a threshold, the take starts when the input
  crosses it (or at once if the pre-roll already did).
- **Sample editor: a port of XOXO Studio's** (`fm1-bt-bridge/sampler.py` + `sampleui.py`), so a sample comes out
  the way the Studio would make it:
  - opening selection: the first sound to the last, "sound" being over -30 dB under the peak or -45 dBFS, from
    5 ms before to 50 ms after;
  - fade in (default 0) and fade out (default 10 ms), linear, 10 ms steps up to 2 s;
  - normalise to -6 dBFS (on by default, room for the pads to mix), reverse;
  - the screen: the take's waveform across the 128 columns, the selection between two markers, dotted outside it,
    a flag on the marker being moved. START and END move one column per step, like the Studio's arrow keys.
  - Not ported: the Studio's Note field (a root note for keyboard playback). Hugbox pads are tuned with PTCH.
- Editing a pad that already has a sample opens a copy of it (all of it selected, nothing applied); the raw take of
  another pad is then gone. Each change re-renders the pad's sample from the source, so nothing compounds.
- `Sampler`: a slot can hold a user sample instead of its built-in sound; the kit stays in memory, so SRC = KIT brings
  it back. New buffers reach the audio core through the event queue; replaced ones come back on a small queue and
  are freed on the control core.
- `Platform`: `sampleFrames(slot)`, `loadSample(slot, buf, max)`, `saveSample(slot, data, frames)`,
  `deleteSample(slot)`.
- Samples are global (a user kit), not per project; the project format is unchanged apart from the new settings.

**UI (keeps the rules: holding Shift never changes the screen, knob pick-up):**
- **Opening SAMPLE:** **Shift + REC held for 0.6 s** (the shortcut map shows REC/SMP). A quick tap is still sequencer
  record. Choosing DRUM/SYNTH from SAMPLE jumps to PLAY, like FX and SETTINGS.
- **Record:** **hold a pad**; release stops, 20 s at most. The take is trimmed, plays back once, and the pad shows
  `U01`–`U16` everywhere (PLAY, SEQ, SOUND, the pad grid). A tap selects a pad and plays it.
- **Screens:** the recorder (free time, pad, state, input meter with the threshold mark, pad grid with recorded pads
  filled) while nothing is selected or a pad is held, and while GAIN was turned in the last 1.5 s; otherwise the editor.
- **Knobs:** 1 STRT, 2 END, 3 FDIN, 4 FDOU | Shift: GAIN, NORM, REV, SRC (USER/KIT: turn to KIT to revert the pad; back to USER undoes it while you are still in the view).
  On SETTINGS, Shift: THRS (start threshold, OFF = start on the press) and MON (mic monitor).
- **Saving:** on leaving the view: the output mutes, "SAVING SAMPLES" shows, then the files are written.
- **Output while recording:** the instrument is muted during a take (so the speaker doesn't feed the mic); with MON on
  the mic is heard. On board v1 the speaker and jack share one stream, so use MON with headphones only.

**Emulator:** the computer's mic through `getUserMedia` ("Use mic" in the Mic · samples card), turned down 20 dB to
behave like the INMP441, fed to `micIn()` block by block like the board. Samples go in IndexedDB. The faceplate drawing
marks the mic hole. `node sim/tools/sample_test.js` runs the whole flow headless (open, record a tone, trim, save,
reload) and checks it.

## 5. Validate (with the v1.1 breadboard test, before the PCB order)

Add the INMP441 to the v1.1 breadboard and a `mictest` env: 1 kHz on the jack, mic → 3 s capture → playback,
and print the peak/RMS levels and the noise floor.
- [ ] Mic data on GPIO41 while I2S0 also drives the DAC (full duplex), clean at 32-bit slots.
- [ ] Together with the I2S1 speaker slave (v1.1), with no lost lock.
- [ ] Noise floor and level at a normal speaking distance; settles the gain range.
- [ ] Saving 1 MB to flash: how long the audio pauses (see risks).

## Risks / unknowns

- **Flash writes pause the audio.** On the ESP32-S3, writing flash briefly disables the cache that PSRAM also
  uses, so the audio task stalls. The firmware saves only on leaving the SAMPLE view, mutes first and shows
  "SAVING SAMPLES". Expect about 2 s for a 10 s sample. The test measures this.
- **Plate acoustics:** a small cavity behind a pinhole can ring. The foam ring and a short, wide hole keep it down.
  It's a reprint if it sounds boxy.
- **Pad noise:** key clicks travel through the plate into the mic. Starting on the threshold, plus the pre-roll,
  helps. Recording on release is an option if it's bad.
- **PSRAM budget:** kit 0.8 MB + recorder 1.76 MB + user samples up to 4.6 MB + delay = about 7.4 of 8 MB. The cap
  is the flash budget, so they can't overflow it.
- **Sound quality:** 61 dB SNR is fine for lo-fi sampling, not for hi-fi.
