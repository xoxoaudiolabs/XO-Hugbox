# Drop dangling track stubs left by the autorouter, set the edge clearance and minimum text size, save, write DRC.
# One pass per process: KiCad 7's python bindings break the track list after Remove() in the same session, and
# removing one stub can expose the next, so repeat the script until it prints "removed 0":
#   HUGBOX_REV=1.1 PYTHONPATH=.. python3 finalize.py            # routed -> final
#   HUGBOX_REV=1.1 FINALIZE_AGAIN=1 PYTHONPATH=.. python3 finalize.py   # final -> final, until "removed 0"
import pcbnew, os
SUF = "_v11" if os.environ.get("HUGBOX_REV") == "1.1" else ""
from pcbnew import VECTOR2I, FromMM as M
src = f'xo_hugbox{SUF}.kicad_pcb' if os.environ.get("FINALIZE_AGAIN") else f'xo_hugbox{SUF}_routed.kicad_pcb'
b=pcbnew.LoadBoard(src)
b.BuildConnectivity(); conn=b.GetConnectivity()
dead=[t for t in b.GetTracks() if t.GetClass()=='PCB_TRACK' and conn.TestTrackEndpointDangling(t,False)]
for t in dead: b.Remove(t)
b.GetDesignSettings().m_CopperEdgeClearance=M(0.3)
for d in b.GetDrawings():
    if isinstance(d,pcbnew.PCB_TEXT) and pcbnew.ToMM(d.GetTextHeight())<0.8:
        d.SetTextSize(VECTOR2I(M(0.8),M(0.8)))
b.BuildConnectivity(); pcbnew.SaveBoard(f'xo_hugbox{SUF}.kicad_pcb',b)
pcbnew.WriteDRCReport(b,f'drcF{SUF}.rpt',pcbnew.EDA_UNITS_MILLIMETRES,True)
print('removed',len(dead))
