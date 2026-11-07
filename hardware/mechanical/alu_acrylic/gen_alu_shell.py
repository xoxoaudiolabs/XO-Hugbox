# XO-Hugbox variant: CNC-machined 6061 aluminium shell for the acrylic faceplate (generated from layout.py).
# Needs CadQuery (pip install cadquery; Python 3.10-3.12). Run from this folder:
#   PYTHONPATH=../.. python gen_alu_shell.py
# Writes xo_hugbox_alu_shell.step (for the CNC quote) and .stl (preview). Threads are modelled as tap drills;
# README.md lists them for the drawing / order notes.
import os
import cadquery as cq
import layout as L

here = os.path.dirname(os.path.abspath(__file__))
W, H, wall, floor_t, top = L.W, L.H, L.ALU_WALL, L.ALU_FLOOR, L.ALU_TOP
boss_top = L.Z_PCB
spk_face = top                                          # speaker face seals against the underside of layer 1
ear_post_top = spk_face - L.SPK_EAR_DROP - L.SPK_EAR_T / 2
usb_y, usb_z = L.XIAO_C[1], L.Z_PCB - 2.5 - 1.2 - 1.6   # same openings as the printed shell
jack_y, jack_z = L.JACK_Y, L.Z_PCB - 2.5 - 1.6 - 3.0
M3_TAP, M25_TAP = 2.5, 2.05
BOSS_D, POST_D = 7.0, 6.5


def rrect(w, h, r, z0, z1, x0=0.0, y0=0.0):
    return (cq.Workplane("XY").workplane(offset=z0).center(x0 + w / 2, y0 + h / 2)
            .rect(w, h).extrude(z1 - z0).edges("|Z").fillet(r))


def cyl(x, y, d, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(d / 2).extrude(z1 - z0)


def box(x0, y0, x1, y1, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).center((x0 + x1) / 2, (y0 + y1) / 2).rect(x1 - x0, y1 - y0).extrude(z1 - z0)


shell = rrect(W, H, 3.0, 0, top)
# cavity: big corner radii below the PCB, tight ones in the band the PCB sits in (its corners are square)
shell = shell.cut(rrect(W - 2 * wall, H - 2 * wall, L.ALU_R_LOW, floor_t, boss_top, wall, wall))
shell = shell.cut(rrect(W - 2 * wall, H - 2 * wall, L.ALU_R_IN, boss_top, top + 1, wall, wall))


def wall_rib(x, y, d, z1):
    """Join a post to the nearest wall(s) so there's no unmachinable sliver between them."""
    r = d / 2
    parts = []
    if x - r - wall < 3: parts.append(box(0, y - r, x, y + r, 0, z1))
    if W - wall - (x + r) < 3: parts.append(box(x, y - r, W, y + r, 0, z1))
    if y - r - wall < 3: parts.append(box(x - r, 0, x + r, y, 0, z1))
    if H - wall - (y + r) < 3: parts.append(box(x - r, y, x + r, H, 0, z1))
    return parts


solids = []
for (x, y) in L.HOLES:                                   # PCB bosses: four corners tapped M3, the fifth a plain support post
    solids += [cyl(x, y, BOSS_D, 0, boss_top)] + wall_rib(x, y, BOSS_D, boss_top)
ears = [(L.SPK_C[0], L.SPK_C[1] + s * L.SPK_EAR_PITCH / 2) for s in (-1, 1)]
for (x, y) in ears:                                      # speaker ear posts, tapped M2.5
    solids += [cyl(x, y, POST_D, 0, ear_post_top)] + wall_rib(x, y, POST_D, ear_post_top)
for s in solids:
    shell = shell.union(s)
# keep everything inside the outer outline (ribs run into the walls)
shell = shell.intersect(rrect(W, H, 3.0, 0, top))

for i, (x, y) in enumerate(L.HOLES):
    if i in L.SUPPORT_ONLY: continue                     # support post under the knob strip: no screw, no hole
    shell = shell.cut(cyl(x, y, M3_TAP, floor_t, boss_top + 1))       # blind: 2 mm floor stays solid
for (x, y) in ears:
    shell = shell.cut(cyl(x, y, M25_TAP, floor_t, ear_post_top + 1))
# USB-C through the left wall (12.5 along y x 7.5 tall, r2), jack through the right wall
usb = (cq.Workplane("YZ").workplane(offset=-1).center(usb_y, usb_z).rect(12.5, 7.5).extrude(wall + 2).edges("|X").fillet(2))
shell = shell.cut(usb)
shell = shell.cut(cq.Workplane("YZ").workplane(offset=W - wall - 1).center(jack_y, jack_z).circle(4.0).extrude(wall + 2))
for (x, y) in [(10, 10), (W - 10, 10), (10, H - 10), (W - 10, H - 10)]:   # rubber-foot recesses
    shell = shell.cut(cyl(x, y, 8.5, -0.01, 0.8))

cq.exporters.export(shell, os.path.join(here, "xo_hugbox_alu_shell.step"))
cq.exporters.export(shell, os.path.join(here, "xo_hugbox_alu_shell.stl"), tolerance=0.02, angularTolerance=0.1)
bb = shell.val().BoundingBox()
print(f"shell {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, volume {shell.val().Volume()/1000:.1f} cm3"
      f" (~{shell.val().Volume()*2.70e-3:.0f} g in 6061); ear posts top z={ear_post_top:.2f}, usb z={usb_z:.2f}, jack z={jack_z:.2f}")
