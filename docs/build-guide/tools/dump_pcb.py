# Dump footprints/pads/tracks/silk from the routed KiCad board to JSON (plate coords, mm, y up).
# Needs KiCad 7 pcbnew (WSL):  wsl -d Ubuntu -- bash -c "cd <this dir> && python3 dump_pcb.py"
import pcbnew, json, sys, os
here = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "../../../hardware/pcb/xo_hugbox.kicad_pcb")
dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "pcb.json")
b = pcbnew.LoadBoard(src)
OX, OY, H = 30.0, 30.0, 167.5          # same offsets as hardware/pcb/build.py
def P(v): return [round(pcbnew.ToMM(v.x) - OX, 3), round(H - (pcbnew.ToMM(v.y) - OY), 3)]
SILK = (pcbnew.F_SilkS, pcbnew.B_SilkS)
out = {"fps": [], "tracks": [], "vias": [], "edges": [], "silk": [], "silkText": []}
def shape(d):
    L = d.GetLayer()
    if L not in SILK: return
    back = L == pcbnew.B_SilkS
    st = d.GetShape(); w = round(pcbnew.ToMM(d.GetWidth()), 2)
    if st == pcbnew.SHAPE_T_SEGMENT:
        out["silk"].append({"k": "l", "a": P(d.GetStart()), "b": P(d.GetEnd()), "w": w, "back": back})
    elif st == pcbnew.SHAPE_T_CIRCLE:
        out["silk"].append({"k": "c", "a": P(d.GetCenter()), "r": round(pcbnew.ToMM(d.GetRadius()), 2), "w": w, "back": back})
    elif st == pcbnew.SHAPE_T_RECT:
        a, c = P(d.GetStart()), P(d.GetEnd())
        for p, q in [((a[0], a[1]), (c[0], a[1])), ((c[0], a[1]), (c[0], c[1])), ((c[0], c[1]), (a[0], c[1])), ((a[0], c[1]), (a[0], a[1]))]:
            out["silk"].append({"k": "l", "a": list(p), "b": list(q), "w": w, "back": back})
def text(t):
    if t.GetLayer() in SILK and t.IsVisible() and t.GetText() and "$" not in t.GetText():
        out["silkText"].append({"t": t.GetShownText() if hasattr(t, "GetShownText") else t.GetText(), "xy": P(t.GetPosition()),
                                "s": round(pcbnew.ToMM(t.GetTextSize().y), 2), "ang": round(t.GetTextAngleDegrees(), 1),
                                "back": t.GetLayer() == pcbnew.B_SilkS})
for fp in b.GetFootprints():
    pads = []
    for p in fp.Pads():
        sz = p.GetSize(); dr = p.GetDrillSize()
        pads.append({"n": p.GetNumber(), "xy": P(p.GetPosition()),
                     "sz": [round(pcbnew.ToMM(sz.x), 2), round(pcbnew.ToMM(sz.y), 2)],
                     "dr": [round(pcbnew.ToMM(dr.x), 2), round(pcbnew.ToMM(dr.y), 2)],
                     "sh": {pcbnew.PAD_SHAPE_RECT: "rect", pcbnew.PAD_SHAPE_OVAL: "oval"}.get(p.GetShape(), "circle"),
                     "ang": round(p.GetOrientationDegrees(), 1), "net": p.GetNetname(),
                     "np": p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH})
    out["fps"].append({"ref": fp.GetReference(), "lib": str(fp.GetFPID().GetLibItemName()),
                       "xy": P(fp.GetPosition()), "rot": round(fp.GetOrientationDegrees(), 1),
                       "back": fp.IsFlipped(), "pads": pads})
    for g in fp.GraphicalItems():
        if g.GetClass() in ("PCB_SHAPE", "FP_SHAPE", "MGRAPHIC"): shape(g)
    r = fp.Reference()
    if r.IsVisible() and r.GetLayer() in SILK: text(r)
for t in b.GetTracks():
    if t.GetClass() == "PCB_VIA":
        out["vias"].append({"xy": P(t.GetPosition()), "d": round(pcbnew.ToMM(t.GetWidth()), 2), "dr": round(pcbnew.ToMM(t.GetDrillValue()), 2)})
    else:
        out["tracks"].append([*P(t.GetStart()), *P(t.GetEnd()), round(pcbnew.ToMM(t.GetWidth()), 2), 0 if t.GetLayer() == pcbnew.F_Cu else 1])
for d in b.GetDrawings():
    if d.GetClass() == "PCB_SHAPE" and d.GetLayer() == pcbnew.Edge_Cuts:
        out["edges"].append([*P(d.GetStart()), *P(d.GetEnd())])
    elif d.GetClass() == "PCB_SHAPE": shape(d)
    elif d.GetClass() == "PCB_TEXT": text(d)
json.dump(out, open(dst, "w"))
print("fps", len(out["fps"]), "tracks", len(out["tracks"]), "vias", len(out["vias"]), "silk", len(out["silk"]), "text", len(out["silkText"]))
