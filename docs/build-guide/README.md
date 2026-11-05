# XO-Hugbox interactive build guide

Double-click `index.html`. It is one self-contained file except for three.js, which loads from jsDelivr
(`three@0.160.0`), so it needs an internet connection the first time.

- Orbit (drag), zoom (scroll), pan (right-drag). Hover a part for its name; click it for notes and its pins/nets
  from the KiCad board. Explode slider, X-ray toggle for shell + plate, 3/4 / top / bottom views.
- 14 pages: overview + parts list, then 13 build steps. Each step shows only what is installed so far, highlights
  and labels the new parts, moves the camera there, and lists the orientation, jumper and polarity notes.
- Arrow keys step through; the last step is remembered.

## Where the geometry comes from

| Source | Used for |
|---|---|
| `hardware/layout.py` | every position and height (board, modules, pots, OLED, speaker, holes, z stack) |
| `hardware/pcb/xo_hugbox.kicad_pcb` | board outline, every footprint and pad, tracks, vias, silkscreen (drawn onto the PCB texture) |
| `hardware/mechanical/*.stl` | shell and top plate meshes |
| `sim/` WASM core | the bring-up OLED picture shown on the 3D screen |

Parts that are not in the CAD (module PCBs, chips, speaker, battery, wires) are simple boxes and cylinders at
the CAD positions, sized like `hardware/mechanical/assembly.scad`. The PCF8574T I2C-pin end and the wire routes are illustrative.

## Rebuild after a hardware change

In a Linux/WSL shell with KiCad 7's Python (`pcbnew`), from `docs/build-guide/tools/`:
```bash
python3 dump_pcb.py
```
then, from the same folder (any OS):
```bash
node capture_oled.js
```
```bash
python gen_data.py
```
```bash
python bundle.py ../src/index.html ../index.html
```
`dump_pcb.py` needs KiCad 7's pcbnew (WSL). Sources live in `src/` (`model.js` = parts, `steps.js` = text and
camera views, `guide.js` = scene and UI). `src/index.html` itself needs a local web server because browsers
block ES modules from `file://`; the bundled `index.html` does not.
