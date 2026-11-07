# XO-Hugbox hardware

One 2-layer PCB, a 3D-printed top plate and bottom shell, and off-the-shelf modules. Every position and dimension comes
from one file, [`hardware/layout.py`](../hardware/layout.py); the board, plate, shell, emulator drawing and build guide are
all generated from it.

> **Status:** prototype. The design is complete but not yet built. Measure the parts listed under
> [Check before ordering](#check-before-ordering) first.

## Parts

| Qty | Part | Notes |
|---|---|---|
| 1 | Seeed **XIAO ESP32-S3** | Must be the S3 (native USB MIDI, 8 MB PSRAM). U.FL antenna stuck to the inside of a wall. |
| 1 | **PCF8574T** I/O expander module | P0-P7 header into the board. Desolder the pass-through socket; replace the address jumper caps with solder bridges. The I2C pins go to the 2×2 "PCF I2C" pads with short wires. |
| 1 | **MAX98357A** I2S amp (generic 7-pin: LRC BCLK DIN GAIN SD GND VIN) | Header from the plain side, printed side facing the floor. GAIN floating = 9 dB, SD default (on, L+R mono). |
| 1 | **PCM5102A** I2S DAC (purple module) | Printed side down along the right wall, jack out of the right wall. Jumpers: FLT L, DEMP L, XSMT H, FMT L. SCK is tied to GND on the PCB. |
| 1 | 1.3" **SH1106** I2C OLED, 128×64 (35.4×33.5 mm, GND VCC SCK SDA) | Remove the header plastic; solder 0.8 mm above the board. |
| 4 | WH148 **B10K** 16 mm pots (straight lugs, 15 mm knurled shaft) + 15×17 mm black aluminium knobs (6 mm bore, set screw) | Lugs facing the keys, joined to the board with diode-lead wire legs (see the build guide). |
| 17 | MX switches + 17 1u keycaps | 16 pads + shift. |
| 16 | 1N4148 diodes | One per pad. |
| 2 | 100 kΩ resistors | Battery voltage divider (R1, R2). |
| 1 | **INMP441** I2S MEMS mic module (round ~14 mm, 2×3 pins, comes with a header strip) | Label (port) side up on its header straight into J_MIC, in the plate pocket under the sound hole; its data comes from the XIAO's MTDI pad (GPIO41) via J_MTDI. See [mic-plan.md](mic-plan.md). |
| 1 | 3 W 8 Ω box speaker, 28.2×31×15.3 mm | Screwed to two shell posts (M2.5 self-tapping), face against the plate grille. |
| 1 | LP102540 1100 mAh LiPo with protection board | In the shell cradle under the keys. |
| 1 | 12D00G-5mm slide switch | Power; actuator through the plate. |
| 4 | M3×12 countersunk screws | Corners. The fifth hole (knob strip) is a support post only: no screw, solid plate above. |
| 5 | 1 mm spacers | Between PCB and plate. |
| 2 | M2.5×6 self-tapping screws | Speaker. |
| - | Thin hook-up wire | PCF I2C jumpers, XIAO battery leads, MTDI lead. |

Target: under $50 in parts per unit.

## Check before ordering

These dimensions are estimates in `hardware/layout.py`. Measure your actual parts and update the file if they differ:

1. Pots: **measured** on the WH148 B10Ks in hand: can 16.27 Ø × 7.12 mm, lugs 5.28 mm up and 4.38 mm past the can,
   M7 collar 7.33 mm, shaft 5.95 Ø × 8.26 mm above the collar (`POT_*`). Lug pitch read 4.75 mm against the usual 5.0
   (`POT_PITCH`); re-check by measuring across the outer lugs, but the wire legs and long slots absorb either value.
2. OLED: **measured** module 35.56 × 33.66 (spec 35.4 × 33.5 used), glass 21.73 mm tall, 6.91 mm from the header edge
   and 5.02 mm from the far edge (`OLED_*`). The board footprint keeps the header 2.6 mm from the pocket edge; the
   module's own header distance (~2.1 mm, from a photo) only affects the printed plate, which clears both.
3. PCM5102A module 33 × 17.5 mm, jack barrel 4.7 mm from its end (`PCM_LEN`, `PCM_W`, `JACK_FROM_END`).
4. Speaker ear-hole spacing ~37 mm, ears about halfway down the 15.3 mm body (`SPK_EAR_PITCH`, `SPK_EAR_DROP`). Shell only.
5. INMP441 mic: **the pin-row spacing goes into the PCB**. The footprint (J_MIC) uses 2 rows of 3 at 2.54 mm pitch, rows
   **7.62 mm** apart centre to centre (read off the listing photo against the 2.54 pitch). To measure: the left edge of
   a hole in one row to the left edge of the hole straight across in the other row. Also check the diameter (13.9 used, listings say 14) and
   the board thickness (1.0 used); those two only change the printed plate (`MIC_*`).

## Ordering the PCB

- **v1.0** (the board being ordered, silkscreen "XO-HUGBOX v1.0"): [`hardware/pcb/xo_hugbox_gerbers.zip`](../hardware/pcb/xo_hugbox_gerbers.zip). 2 layers, 1.6 mm, HASL lead-free.
  **The oval pot-pin slots must stay plated:** say so in the order notes.
  **Cost (PCBWay, 2026-11-07):** 5 boards, **$92.70** delivered: about **$18.54 per board**, or $7.31 each for the boards alone.

  | Boards | Shipping | Coupon | Duty & tax | Duty-paid (DTP) fee | Card fee | Total |
  |---|---|---|---|---|---|---|
  | $41.55 | $31.16 | -$5.00 | $13.71 | $7.00 | $4.28 | $92.70 |

  Duty is paid up front through PCBWay, so nothing is due on delivery. Shipping, duty and fees are $56.15 of the
  $92.70, so a bigger order brings the per-board cost down quickly.
- **v1.1:** [`hardware/pcb/xo_hugbox_v11_gerbers.zip`](../hardware/pcb/xo_hugbox_v11_gerbers.zip). Same parts and outline; gives the
  speaker amp its own I2S data line so the speaker can be filtered separately. Pending a breadboard test; see
  [speaker-path-plan.md](speaker-path-plan.md). Until that passes, v1.0 is the safe choice.

The board has **no reverse-polarity protection** on the battery input: check the LiPo's polarity with a meter.

## Printing

- Plate: [`hardware/mechanical/xo_hugbox_top_plate.stl`](../hardware/mechanical/xo_hugbox_top_plate.stl). Use a layer height that
  divides 1.5 mm (0.15 / 0.25 / 0.3 mm) so the MX clip ledge comes out exact. Print one switch hole as a test first.
- Shell: [`hardware/mechanical/xo_hugbox_shell.stl`](../hardware/mechanical/xo_hugbox_shell.stl).
- Variant, not yet made: a CNC black anodized aluminium shell with a 4-layer laser-cut clear acrylic faceplate, for the
  same board. See [`hardware/mechanical/alu_acrylic/`](../hardware/mechanical/alu_acrylic/README.md).

## Assembly

Follow the interactive 3D guide, [`docs/build-guide/index.html`](build-guide/index.html). The order, in short:

1. Diodes and the divider resistors (back).
2. XIAO (back): first solder ~4 cm battery leads to the XIAO's own BAT+/BAT- pads (unreachable once mounted), then mount it
   and run the leads to J13.
3. MAX98357A (back).
4. INMP441 mic on top in J_MIC, soldered and clipped flush **before** the PCF8574T: its pins come through under that module.
5. PCF8574T module (back).
6. Switches snapped into the plate.
7. Pots, OLED and power switch on top **before** the plate: they sit under it, and the plate pockets set the OLED height.
8. Plate (with switches) onto the PCB over the spacers; solder the switches from below.
9. Clip the pins under the PCM5102A area flush (shift and power switch), **then** fit the PCM5102A.
10. Speaker onto the shell posts.
11. LiPo into the cradle, leads to J11 from the back, power switch OFF, polarity checked. Plug in the antenna.
12. Board and plate into the shell (4× M3×12), keycaps and knobs, then the first power-on with the bring-up firmware.

## First power-on

Flash the bring-up test (`pio run -e bringup -t upload` in `firmware/`) and open the serial monitor. It scans I2C
(expect `0x3C` OLED and `0x20` PCF8574), shows every pad, knob and the battery voltage, and plays a tone through both
outputs. Then flash the instrument (`pio run -e hugbox -t upload`); its serial output reports the audio load every 5 s.

## Regenerating the hardware files

Everything is scripted. The scripts import `hardware/layout.py`, so run them from their own folder with `PYTHONPATH=..`.

| Step | Tool | Command |
|---|---|---|
| Plate + shell `.scad` | Python | `cd hardware/mechanical && PYTHONPATH=.. python gen_scad.py` (then export STLs in OpenSCAD) |
| Assembly scene | Python | `cd hardware/mechanical && PYTHONPATH=.. python gen_assembly.py` |
| Place parts, nets, DSN | KiCad 7 Python (`pcbnew`) | `cd hardware/pcb && PYTHONPATH=.. python3 build.py` (`HUGBOX_REV=1.1` for v1.1) |
| Autoroute | [Freerouting](https://github.com/freerouting/freerouting) 2.1.0, Java 21 | `java -jar freerouting-2.1.0.jar --gui.enabled=false -de xo_hugbox.dsn -do xo_hugbox.ses -mp 100` |
| Import routes | KiCad 7 Python | `python3 import_ses.py xo_hugbox_placed.kicad_pcb xo_hugbox.ses xo_hugbox_routed.kicad_pcb` |
| Clean up stubs | KiCad 7 Python | `PYTHONPATH=.. python3 finalize.py`, then `FINALIZE_AGAIN=1 python3 finalize.py` until it prints `removed 0` |
| Check | KiCad 7 Python | `python3 drc_summary.py xo_hugbox.kicad_pcb` · `python3 compare_boards.py <a> <b>` |
| Gerbers | KiCad 7 Python | `python3 plot_gerbers.py [board] [zip]` |

Notes:
- KiCad 7 with its footprint library (`kicad`, `kicad-footprints` on Ubuntu 24.04) is enough; the scripts don't use the GUI.
  `kicad-cli` 7 has no DRC, so the scripts call `pcbnew.WriteDRCReport`.
- KiCad 7 can't import SES files from standalone Python, hence `import_ses.py`.
- A fresh DRC load uses KiCad's 0.5 mm copper-to-edge default, so v1 reports a few tracks at 0.45 mm from the top edge;
  that is well within normal fab limits.
- After any hardware change, regenerate the build guide data (`docs/build-guide/tools/`) and the emulator layout
  (`python sim/gen_layout.py`).
