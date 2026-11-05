# List dangling tracks (KiCad 7 python, WSL): python3 list_dangling.py board.kicad_pcb
import sys
import pcbnew

b = pcbnew.LoadBoard(sys.argv[1])
b.BuildConnectivity()
conn = b.GetConnectivity()
mm = pcbnew.ToMM
for t in b.GetTracks():
    if t.GetClass() == 'PCB_TRACK' and conn.TestTrackEndpointDangling(t, False):
        s, e = t.GetStart(), t.GetEnd()
        print(f"{t.GetNetname():8} {b.GetLayerName(t.GetLayer()):5} ({mm(s.x):.3f},{mm(s.y):.3f}) -> ({mm(e.x):.3f},{mm(e.y):.3f}) "
              f"w={mm(t.GetWidth()):.2f} uuid={t.m_Uuid.AsString()}")
