# Plot fab gerbers + drill for PCBWay from the routed board, then zip them.
# Usage (KiCad 7 python, e.g. WSL Ubuntu 24.04 `apt install kicad`):
#   python3 plot_gerbers.py [board.kicad_pcb] [out.zip]
import os, sys, glob, shutil, tempfile, zipfile
import pcbnew

src = sys.argv[1] if len(sys.argv) > 1 else "xo_hugbox.kicad_pcb"
out_zip = sys.argv[2] if len(sys.argv) > 2 else "xo_hugbox_gerbers.zip"
board = pcbnew.LoadBoard(src)
tmp = tempfile.mkdtemp()

pc = pcbnew.PLOT_CONTROLLER(board)
job = pcbnew.GERBER_JOBFILE_WRITER(board)
po = pc.GetPlotOptions()
po.SetOutputDirectory(tmp)
po.SetFormat(pcbnew.PLOT_FORMAT_GERBER)
po.SetUseGerberProtelExtensions(True)     # .gtl/.gbl/.gto/... (PCBWay friendly)
po.SetUseGerberX2format(True)
po.SetIncludeGerberNetlistInfo(True)
po.SetGerberPrecision(6)                  # 4.6 format, mm
po.SetSubtractMaskFromSilk(False)
po.SetPlotFrameRef(False)
po.SetPlotValue(False)
po.SetPlotReference(True)
po.SetPlotInvisibleText(False)
po.SetSketchPadsOnFabLayers(False)
po.SetDrillMarksType(pcbnew.DRILL_MARKS_NO_DRILL_SHAPE)
po.SetScale(1)
po.SetMirror(False)

layers = [
    ("F_Cu", pcbnew.F_Cu), ("B_Cu", pcbnew.B_Cu),
    ("F_Silkscreen", pcbnew.F_SilkS), ("B_Silkscreen", pcbnew.B_SilkS),
    ("F_Mask", pcbnew.F_Mask), ("B_Mask", pcbnew.B_Mask),
    ("Edge_Cuts", pcbnew.Edge_Cuts),
]
for name, layer in layers:
    pc.SetLayer(layer)
    pc.OpenPlotfile(name, pcbnew.PLOT_FORMAT_GERBER, name)
    pc.PlotLayer()
    job.AddGbrFile(layer, os.path.basename(pc.GetPlotFileName()))
pc.ClosePlot()
job.CreateJobFile(os.path.join(tmp, os.path.splitext(os.path.basename(src))[0] + "-job.gbrjob"))

# Drill: one Excellon file, PTH + NPTH merged (MixedPlating), metric, decimal, plated slots kept as G85.
dw = pcbnew.EXCELLON_WRITER(board)
dw.SetOptions(False, False, board.GetDesignSettings().GetAuxOrigin(), True)  # mirror, minimalHeader, offset, mergeNPTH
dw.SetFormat(True, pcbnew.EXCELLON_WRITER.DECIMAL_FORMAT, 3, 3)
dw.SetRouteModeForOvalHoles(False)        # G85 slots, same as the previous order files
dw.CreateDrillandMapFilesSet(tmp, True, False)

files = sorted(f for f in glob.glob(os.path.join(tmp, "*")) if os.path.isfile(f))
with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as z:
    for f in files:
        z.write(f, os.path.basename(f))
        print(os.path.basename(f), os.path.getsize(f))
shutil.rmtree(tmp)
print("wrote", out_zip)
