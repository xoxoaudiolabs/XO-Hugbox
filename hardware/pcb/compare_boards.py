# Compare two boards pad by pad: positions and nets. Run with KiCad 7 python (WSL):
#   python3 compare_boards.py xo_hugbox.kicad_pcb xo_hugbox_v11_placed.kicad_pcb
import sys
import pcbnew


def pads(path):
    d = {}
    for fp in pcbnew.LoadBoard(path).GetFootprints():
        for i, p in enumerate(fp.Pads()):
            pos = p.GetPosition()
            d[(fp.GetReference(), p.GetNumber(), i)] = (round(pcbnew.ToMM(pos.x), 3), round(pcbnew.ToMM(pos.y), 3), p.GetNetname())
    return d


a, b = pads(sys.argv[1]), pads(sys.argv[2])
moved = [k for k in a if k in b and a[k][:2] != b[k][:2]]
print(f"pads: {len(a)} vs {len(b)}, only in one: {len(set(a) ^ set(b))}, moved: {len(moved)}")
for k in moved[:10]:
    print("  MOVED", k, a[k][:2], "->", b[k][:2])
for k in sorted(a):
    if k in b and a[k][2] != b[k][2]:
        print("  NET", k[0], "pad", k[1], ":", a[k][2], "->", b[k][2])
