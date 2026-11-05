# Plan: separate speaker audio path (board v1 -> v1.1)

**Goal:** give the MAX98357A speaker amp its own I2S data stream, so the firmware can high-pass (~250 Hz) and tune
the speaker without touching the headphone/line output. **Constraint:** only parts already ordered. No new BOM lines.

**Status:** prepared. Board v1.1 is routed (`hardware/pcb/xo_hugbox_v11.kicad_pcb`, `xo_hugbox_v11_gerbers.zip`) and the
breadboard test firmware builds (`pio run -e duali2s`). Validate on a breadboard when the parts arrive, *before* the
PCB is ordered. Section 2's main-firmware change (two output streams) is not written yet; it follows a passing test.

## Today (v1)
| Net | XIAO pin | Goes to |
|---|---|---|
| `SHIFT` | D10 (GPIO9) | SW17 pad 1 (switch to GND, internal pull-up) |
| `VSENSE` | D9 (GPIO8, ADC1) | R1/R2 100k/100k midpoint (battery / 2) |
| `DIN` | D8 (GPIO7) | PCM5102A DIN **and** MAX98357A DIN |
| `BCK`, `LRCK` | D6, D7 | both modules |

All 11 XIAO pins are used, so a second data line needs a pin freed.

## 1. Board change (3 nets in `hardware/pcb/build.py`, no new parts)
1. **Shift moves onto the battery divider.** SW17 pad 1: `SHIFT` -> `VSENSE`. Pressing shift shorts R2, pulling
   D9 to 0 V. Released, D9 reads Vbat/2 = 1.5-2.1 V. Current while pressed is Vbat / 100k <= 42 uA, and the pin
   never sees more than Vbat/2. D9 is on ADC1, which works alongside the radio (ADC2 does not).
2. **D10 becomes the speaker data line.** XIAO D10: `SHIFT` -> `SPK_DIN`.
3. **The amp listens to D10.** MAX98357A DIN: `DIN` -> `SPK_DIN`. The PCM5102A stays on `DIN` (D8).
4. Silkscreen: "R1 R2 100k" -> "R1 R2 100k (+shift)", and bump the board text to "XO-HUGBOX v1.1".
5. Re-run the pipeline in [hardware.md](hardware.md#regenerating-the-hardware-files): `build.py` -> DSN -> Freerouting -> `import_ses.py` -> `finalize.py` -> DRC
   (0 unconnected, no clearance errors) -> `plot_gerbers.py` -> archive the v1 zip -> rebuild the build-guide data
   (`docs/build-guide/tools/*`). No part moves, so `layout.py`, the plate and the shell do not change.

## 2. Firmware change
- **`pins.h`:** drop `PIN_SHIFT`; add `PIN_I2S_DOUT_SPK D10` and `SHIFT_ON_MV 600` / `SHIFT_OFF_MV 900` (hysteresis).
- **Shift detection** (board layer, `src/hugbox/main.cpp`): shift = `analogReadMilliVolts(D9)` below the threshold,
  confirmed on 2 consecutive 5 ms scans. The battery reading updates only while shift is up, otherwise it holds the last value.
- **Two output streams:** `App::render(line, spk, frames)`. The speaker stream is line -> mono sum -> 2nd-order HPF
  (the `SHPF` param returns as the speaker cutoff, default 250 Hz) -> small-speaker limiter. `LCUT` stays as a global
  low cut. The emulator's "Listen through: Speaker" plays the real speaker stream.
- **I2S, primary path:** use the ESP-IDF `i2s_std` driver directly (`ESP_I2S` handles only one port).
  - I2S0 is master TX: BCK D6, WS D7, DOUT D8, feeding the PCM5102A as today.
  - I2S1 is slave TX: DOUT D10. Its BCK/WS *inputs* are routed through the GPIO matrix from the D6/D7 pads, which
    stay outputs of I2S0 with their input buffers enabled.
  - Init order: create both, enable the slave first, then the master; re-enable output on D6/D7 if the slave init cleared it.
  - Pre-fill both DMA queues with the same number of zero frames, then write both every block from the one audio task.
- **Fallback (firmware only, no board change):** if the slave will not lock, route I2S0's data-out signal to D10
  as well, through D10's own output selector in the GPIO matrix. The amp then gets the same stream as v1 and
  `LCUT` remains the stopgap. The board change therefore can't leave you worse off than v1.

## 3. Validate before ordering the PCB (when the parts arrive)
Breadboard the XIAO, MAX98357A, PCM5102A, 2x 100k and one MX switch with jumper wires, wired as v1.1.
Flash a test env (`env:duali2s`): a 1 kHz tone on the jack and 200 Hz on the speaker.
- [ ] Each output carries only its own tone, with no crosstalk (listen; scope if you have one).
- [ ] 10 minutes with no crackle, dropout or drifting phase (the slave never loses lock).
- [ ] Shift reads reliably from 3.0 V (flat) to 4.2 V (full); battery reading is correct when released.
- [ ] Fallback mode (same signal on D8 + D10) also works, so the board is safe either way.

If all pass, apply section 1 and order. If the slave path fails, still apply section 1 (the fallback keeps v1
behaviour) or keep v1. Decide then.

## Risks / unknowns
- **Shared clock pins:** I found no Espressif documentation for two controllers sharing BCK/WS pins (master out,
  slave in). The GPIO matrix should allow it, because each pad and each peripheral input has its own selector.
  The IDF driver may still fight it during init. That's why the breadboard test comes first.
- **Phase offset:** the two streams may sit a constant 0-1 DMA block apart. That's harmless; drift would not be.
- **Cost:** one extra HPF per sample (negligible) and about 4 KB more DMA buffer RAM.
- **Shift through the divider:** shift reads only through the ADC, about 20 us per read at 200 Hz. That's fine;
  the battery value pauses while shift is held.
