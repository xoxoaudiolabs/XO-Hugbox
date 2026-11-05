# Turn the 128x64 PBM frames from oled_snapshot.js into OLED-looking PNGs (x4, light pixels on near-black).
# Usage: python sim/tools/pbm_to_png.py <dir>
import glob, os, sys
from PIL import Image

d = sys.argv[1] if len(sys.argv) > 1 else "."
ON, OFF, S = (232, 244, 255), (6, 8, 12), 4
for f in sorted(glob.glob(os.path.join(d, "*.pbm"))):
    tok = open(f).read().split()
    w, h = int(tok[1]), int(tok[2])
    bits = tok[3:]
    im = Image.new("RGB", (w * S + 16, h * S + 16), OFF)
    px = im.load()
    for y in range(h):
        for x in range(w):
            if bits[y * w + x] == "1":
                for dy in range(S - 1):
                    for dx in range(S - 1):
                        px[8 + x * S + dx, 8 + y * S + dy] = ON
    png = f[:-4] + ".png"
    im.save(png)
    os.remove(f)
    print("wrote", png)
