# Web-app icons for the emulator: the Hugbox faceplate seen from above (plate, speaker, OLED, Shift, knobs, pads),
# drawn from the same layout as the emulator (sim/src/layout.js, generated from hardware/layout.py).
# Usage: python3 sim/tools/make_icons.py      (needs Pillow; writes sim/icons/*.png)
import json, os, re
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
L = json.loads(re.search(r"=\s*(\{.*\})", open(os.path.join(SIM, "src", "layout.js"), encoding="utf-8").read()).group(1))
OUT = os.path.join(SIM, "icons")
SS = 4   # supersampling

BG = (11, 11, 11)
PLATE, PLATE_EDGE = (236, 231, 218), (189, 183, 168)
HOLE = (42, 42, 42)
CAP, CAP_EDGE = (247, 245, 239), (188, 182, 166)
SHIFT, SHIFT_EDGE = (240, 135, 74), (160, 75, 19)
KNOB, KNOB_RING = (17, 17, 17), (207, 207, 207)
OLED, PIXEL = (5, 6, 8), (232, 244, 255)
DOT = (43, 43, 43)


def draw(size, plate_h):
    """size: canvas px; plate_h: plate height as a fraction of the canvas."""
    S = size * SS
    im = Image.new("RGB", (S, S), BG)
    d = ImageDraw.Draw(im)
    k = plate_h * S / L["H"]                       # px per mm
    ox, oy = (S - L["W"] * k) / 2, (S - L["H"] * k) / 2
    X = lambda x: ox + x * k
    Y = lambda y: oy + (L["H"] - y) * k            # plate y-up -> image y-down

    def box(cx, cy, w, h, r, fill, edge=None, ew=0.3):
        d.rounded_rectangle([X(cx - w / 2), Y(cy + h / 2), X(cx + w / 2), Y(cy - h / 2)], radius=r * k,
                            fill=fill, outline=edge, width=max(1, int(ew * k)) if edge else 0)

    d.rounded_rectangle([X(0), Y(L["H"]), X(L["W"]), Y(0)], radius=3 * k, fill=PLATE, outline=PLATE_EDGE,
                        width=max(1, int(0.5 * k)))
    # speaker grille
    sx, sy = L["SPK_C"]
    for i in range(-3, 4):
        for j in range(-3, 4):
            dx, dy = i * 3.2, j * 3.2
            if dx * dx + dy * dy <= 100:
                d.ellipse([X(sx + dx - 1), Y(sy + dy + 1), X(sx + dx + 1), Y(sy + dy - 1)], fill=DOT)
    # OLED window with a few lit rows
    cx, cy = L["OLED_C"][0], L["OLED_AA_CY"]
    w, h = L["OLED_WIN"]
    box(cx, cy, w, h, 0.6, OLED)
    for frac, length in ((0.30, 0.55), (0.50, 0.80), (0.70, 0.40)):
        y = cy + h / 2 - frac * h
        d.rectangle([X(cx - w / 2 + 2.5), Y(y + 0.8), X(cx - w / 2 + 2.5 + length * (w - 5)), Y(y - 0.8)], fill=PIXEL)
    # Shift
    box(L["SHIFT"][0], L["SHIFT"][1], 16, 16, 0.4, HOLE)
    box(L["SHIFT"][0], L["SHIFT"][1], 14, 14, 1.2, SHIFT, SHIFT_EDGE, 0.5)
    # knobs
    for x in L["cx"]:
        y = L["POT_Y"]
        d.ellipse([X(x - 7.5), Y(y + 7.5), X(x + 7.5), Y(y - 7.5)], fill=KNOB_RING)
        d.ellipse([X(x - 6.6), Y(y + 6.6), X(x + 6.6), Y(y - 6.6)], fill=KNOB)
        d.line([X(x), Y(y + 2.2), X(x), Y(y + 6.0)], fill=KNOB_RING, width=max(1, int(0.9 * k)))
    # 4 x 4 pads
    for x in L["cx"]:
        for y in L["cy"]:
            box(x, y, 16, 16, 0.4, HOLE)
            box(x, y, 14, 14, 1.2, CAP, CAP_EDGE, 0.5)
    return im.resize((size, size), Image.LANCZOS)


os.makedirs(OUT, exist_ok=True)
# plain icons: the plate nearly fills the square; maskable: the whole plate inside the middle 80 % circle
for name, size, h in (("icon-192.png", 192, 0.90), ("icon-512.png", 512, 0.90),
                      ("icon-maskable-512.png", 512, 0.70), ("apple-touch-icon.png", 180, 0.84)):
    draw(size, h).save(os.path.join(OUT, name), optimize=True)
    print("wrote", os.path.join("sim", "icons", name))
