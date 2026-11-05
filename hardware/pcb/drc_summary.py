# Run KiCad DRC on boards and print violation counts by type (KiCad 7 python, WSL):
#   python3 drc_summary.py xo_hugbox.kicad_pcb xo_hugbox_v11.kicad_pcb
import collections, re, sys, tempfile, os
import pcbnew

for path in sys.argv[1:]:
    b = pcbnew.LoadBoard(path)
    rpt = os.path.join(tempfile.gettempdir(), os.path.basename(path) + ".rpt")
    pcbnew.WriteDRCReport(b, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
    txt = open(rpt).read()
    kinds = collections.Counter(re.findall(r"^\[(\w+)\]", txt, re.M))
    unconnected = re.search(r"\*\* Found (\d+) unconnected pads \*\*", txt)
    print(f"{os.path.basename(path)}: unconnected {unconnected.group(1) if unconnected else '?'}")
    for k, n in sorted(kinds.items()):
        print(f"   {k:28} {n}")
