# Aluminium shell + clear acrylic faceplate (variant)

An alternative to the printed shell and plate: a CNC-machined, black anodized 6061 aluminium shell and a stack of four
laser-cut clear acrylic layers. It fits the same v1.0 PCB and parts. The keycap and knob heights don't change. The printed
parts in `../` are unchanged and still the default.

> **Status:** designed, not made or test-fitted. Check the [open questions](#open-questions) before ordering.

## Files

| File | For |
|---|---|
| `xo_hugbox_alu_shell.step` | CNC quote (PCBWay / JLCCNC CNC service). Threads are modelled as tap drills; give the table below as order notes. |
| `xo_hugbox_alu_shell.stl` | Preview only. |
| `xo_hugbox_acrylic_L1.dxf` … `_L4.dxf` | Laser cutting, one file per layer, mm. |
| `xo_hugbox_acrylic.scad` | Layer outlines (`layer=1..4`) and a 3D preview of the stack (`layer=0`, shell coordinates). |
| `gen_alu_shell.py`, `gen_acrylic.py` | Generators. All dimensions come from `hardware/layout.py` (the `ACR_*` / `ALU_*` block). |

Regenerate from this folder:

```bash
PYTHONPATH=../.. python gen_acrylic.py
for n in 1 2 3 4; do openscad -D layer=$n -o xo_hugbox_acrylic_L$n.dxf xo_hugbox_acrylic.scad; done
PYTHONPATH=../.. python gen_alu_shell.py      # needs CadQuery: pip install cadquery (Python 3.10-3.12)
```

`gen_acrylic.py` reads the v1.0 pad positions from `docs/build-guide/tools/pcb.json` (the header rows that come up through
the board).

## Acrylic stack

Heights are from the top of the PCB. MX switches put the top of the switch plate at +5.0. The pot cans reach +7.12.

| Layer | Sheet | Height | Cut-outs |
|---|---|---|---|
| Spacers | 1.5 mm | 0 → 1.5 | — (5 M3 spacers, was 1 mm) |
| **1 under-layer** | 2.0 mm clear | 1.5 → 3.5 | 16 mm switch pockets, pot cans + lug legs, OLED board, mic module, power switch, module header rows, speaker grille |
| **2 switch plate** | 1.5 mm clear | 3.5 → 5.0 | 14 mm switch holes, pot cans + lug legs, OLED glass window, 1.5 mm mic sound hole, power switch window, speaker grille |
| **3 knob spacer** | 2.0 mm clear | 5.0 → 7.0 | Pot cans + solder lugs (knob strip only) |
| **4 knob top** | 2.0 mm clear | 7.0 → 9.0 | 7.5 mm pot shaft holes (knob strip only); the pot nuts clamp it to the cans |

- Sheet thickness: if a vendor's sheet isn't exactly the size above, change `ACR_*_T` in `layout.py`. The spacer height
  then adjusts so the switch plate top stays at +5.0.
- Solvent-weld layers 3 and 4 to each other and to layer 2 along the knob strip. Layers 1 and 2 are held by the screws
  and switch clips.
- The speaker face seals against the underside of layer 1 (same height as the printed plate's locator).
- Clear acrylic shows the whole board. Smoked or frosted sheet for layer 2 hides more of it.

## Aluminium shell

6061-T6, CNC from solid, **Type II black anodize** (bead-blast first for a matte finish, if offered). 92.2 × 167.5 × 19.6 mm,
2 mm walls and floor, about 141 g. Machined from the top, plus one side setup for the USB-C and jack openings. All the
holes are blind, so the 2 mm floor stays solid.

| Holes | Where (plate coords, mm) | Thread | Depth |
|---|---|---|---|
| 5 | PCB bosses (6,6) (86.2,6) (6,70) (86.2,70) (86.2,112) | **M3** (2.5 drill) | 14.5 (to floor) |
| 2 | Speaker posts (18.3,124.3) (18.3,161.3) | **M2.5** (2.05 drill) | 9.2 (to floor) |

- Inside corners: 3.0 mm radius below the PCB, **0.8 mm radius** in the top 3.1 mm. The PCB's square corners are only
  0.4 mm from the wall, so tell the shop not to enlarge that radius.
- Posts that would leave a thin gap to the wall are joined to it.
- There's no battery cradle. Hold the LiPo down with foam tape in the same spot (under the key grid).
- The PCB mounting holes are unplated, so the shell has no electrical connection to the circuit.

## Assembly changes vs the printed version

- 1.5 mm spacers instead of 1 mm.
- Screws: 4× M3×12 **button head** (black) at the corners, plus 1× M3×16 button head through the knob strip into the
  fifth boss, replacing the countersunk ones. 2× M2.5×6 machine screws (not self-tapping) for the speaker.
- Mic: solder the INMP441 **1.5 mm above the board** (header plastic removed, use a shim) instead of on its 2.5 mm
  spacer. Its label face then sits 1.0 mm under layer 2 (0.5 mm above the solder). A 1 mm foam ring around the sound
  port seals it.

## Open questions

1. **Bluetooth range.** The aluminium blocks 2.4 GHz; only the acrylic top lets it out. Don't stick the antenna inside a
   wall. Stick it flat under layer 1, away from copper (next to the speaker), and test BLE MIDI range before ordering the
   shell. USB MIDI is unaffected.
2. **Power switch body height** (`PWR_BODY_H`, 3.7 estimated). It is above layer 2's underside, so both layers have a
   9.4 × 4.4 mm window for the body. If it measures ≤ 3.4 mm, layer 2 gets the narrow actuator slot instead.
3. **Speaker ear spacing** (`SPK_EAR_PITCH`, ~37 estimated) sets the post positions. Measure before ordering the shell.
4. **Laser kerf.** The DXFs are nominal. Check whether the vendor compensates for kerf; the 14.0 mm switch holes need
   to come out 14.0.
