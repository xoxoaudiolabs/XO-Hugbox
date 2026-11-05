import pcbnew, math, os
# Board revision: HUGBOX_REV=1.1 builds the separate-speaker-line variant (docs/speaker-path-plan.md):
# shift switch on the D9 divider node, D10 = speaker data, MAX98357A DIN on D10. Output files get a _v11 suffix.
V11 = os.environ.get("HUGBOX_REV") == "1.1"
SUF = "_v11" if V11 else ""
from pcbnew import VECTOR2I, FromMM as mm
import layout as L
FP="/usr/share/kicad/footprints/"
OX,OY=30.0,30.0
board=pcbnew.BOARD(); board.GetDesignSettings().SetCopperLayerCount(2)
nc=board.GetDesignSettings().m_NetSettings.m_DefaultNetClass
nc.SetTrackWidth(mm(0.3)); nc.SetClearance(mm(0.2)); nc.SetViaDiameter(mm(0.8)); nc.SetViaDrill(mm(0.4))
def K(x,y): return VECTOR2I(mm(OX+x),mm(OY+L.H-y))     # plate coords -> board
nets={}
def net(n):
    if n not in nets:
        ni=pcbnew.NETINFO_ITEM(board,n); board.Add(ni); nets[n]=ni
    return nets[n]
def text(s,x,y,size=0.9,back=True):
    t=pcbnew.PCB_TEXT(board); t.SetText(s); t.SetLayer(pcbnew.B_SilkS if back else pcbnew.F_SilkS)
    t.SetPosition(K(x,y)); t.SetTextSize(VECTOR2I(mm(size),mm(size))); t.SetTextThickness(mm(0.15))
    if back: t.SetMirrored(True)
    board.Add(t)
def seg(a,b,layer,w=0.15):
    s=pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetStart(K(*a)); s.SetEnd(K(*b))
    s.SetLayer(layer); s.SetWidth(mm(w)); board.Add(s)
def rect(x0,y0,x1,y1,layer):
    for a,b in [((x0,y0),(x1,y0)),((x1,y0),(x1,y1)),((x1,y1),(x0,y1)),((x0,y1),(x0,y0))]: seg(a,b,layer)
def custom(ref,pads,size=1.7,drill=1.0,labels=True,lab_dy=1.9,back=True,stagger=False,slot=None):
    fp=pcbnew.FOOTPRINT(board); fp.SetReference(ref); fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
    fp.SetFPID(pcbnew.LIB_ID('custom',ref)); x0,y0=pads[0][1],pads[0][2]; fp.SetPosition(K(x0,y0)); board.Add(fp)
    for i,(name,x,y,n) in enumerate(pads):
        pd=pcbnew.PAD(fp); pd.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        if slot:
            pd.SetShape(pcbnew.PAD_SHAPE_OVAL); pd.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_OBLONG)
            pd.SetSize(VECTOR2I(mm(slot[0]+0.8),mm(slot[1]+0.8))); pd.SetDrillSize(VECTOR2I(mm(slot[0]),mm(slot[1])))
        else:
            pd.SetShape(pcbnew.PAD_SHAPE_RECT if i==0 else pcbnew.PAD_SHAPE_CIRCLE)
            pd.SetSize(VECTOR2I(mm(size),mm(size))); pd.SetDrillSize(VECTOR2I(mm(drill),mm(drill)))
        pd.SetLayerSet(pd.PTHMask()); pd.SetNumber(str(i+1)); fp.Add(pd)
        dx,dy=K(x,y).x-K(x0,y0).x, K(x,y).y-K(x0,y0).y
        pd.SetPos0(VECTOR2I(dx,dy)); pd.SetPosition(K(x,y))
        if n: pd.SetNet(net(n))
        if labels and name:
            d = lab_dy*(1.8 if (stagger and i%2) else 1.0)
            text(name,x,y+d,0.8,back)
    return fp
def lib(libname,name,ref,x,y,rot=0,back=False,refvis=True):
    fp=pcbnew.FootprintLoad(FP+libname+".pretty",name); fp.SetReference(ref)
    fp.SetPosition(K(x,y))
    if rot: fp.SetOrientationDegrees(rot)
    board.Add(fp)
    if back: fp.Flip(fp.GetPosition(),True)
    if not refvis: fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    return fp
def padxy(p):  # board pad -> plate coords
    v=p.GetPosition(); return (pcbnew.ToMM(v.x)-OX, L.H-(pcbnew.ToMM(v.y)-OY))

# ---- outline with speaker notch ----
I=L.INSET; nx0,ny0,nx1,ny1=L.NOTCH
poly=[(I,I),(L.W-I,I),(L.W-I,L.H-I),(nx1,L.H-I),(nx1,ny0),(nx0,ny0)]
for a,b in zip(poly,poly[1:]+poly[:1]): seg(a,b,pcbnew.Edge_Cuts,0.1)
for i,(x,y) in enumerate(L.HOLES):
    lib("MountingHole","MountingHole_3.2mm_M3",f"H{i+1}",x,y,refvis=False)

# ---- key matrix (rows R0 top .. R3 bottom), diodes on the back ----
k=1
for jr in range(4):            # jr: 0 = top row
    for c in range(4):
        X,Y=L.cx(c),L.cy(3-jr)
        sw=lib("Button_Switch_Keyboard","SW_Cherry_MX_1.00u_PCB",f"SW{k}",X+2.54,Y+5.08)
        d=lib("Diode_THT","D_DO-35_SOD27_P7.62mm_Horizontal",f"D{k}",X+3.81,Y-9.5,back=True)
        an=net(f"SWD{k}")
        for p in sw.Pads():
            if p.GetNumber()=="1": p.SetNet(net(f"C{c}"))
            if p.GetNumber()=="2": p.SetNet(an)
        for p in d.Pads():
            p.SetNet(net(f"R{jr}") if p.GetNumber()=="1" else an)
        k+=1
# shift key (direct to D10)
sx,sy=L.SHIFT
sw=lib("Button_Switch_Keyboard","SW_Cherry_MX_1.00u_PCB","SW17",sx+2.54,sy+5.08)
for p in sw.Pads(): p.SetNet(net(("VSENSE" if V11 else "SHIFT")) if p.GetNumber()=="1" else net("GND"))
text("SHIFT",sx,sy-9.6,1.0,back=False)

# ---- pots (WH148, pins in slots so exact pin offset doesn't matter) ----
smid=L.POT_Y+sum(L.POT_SLOT)/2; slen=L.POT_SLOT[1]-L.POT_SLOT[0]
for i in range(4):
    x=L.cx(i)
    custom(f"RV{i+1}",[("GND",x-L.POT_PITCH,smid,"GND"),("W",x,smid,f"POT{i}"),("3V3",x+L.POT_PITCH,smid,"3V3")],
           slot=(1.3,slen),labels=False)
    text(f"POT{i}",x,L.POT_Y-1.0,1.0,back=False)
    s=pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_CIRCLE); s.SetCenter(K(x,L.POT_Y)); s.SetEnd(K(x+8.4,L.POT_Y))
    s.SetLayer(pcbnew.F_SilkS); s.SetWidth(mm(0.15)); board.Add(s)

# ---- OLED (header toward the pots, so pin order reads SDA SCL VCC GND left->right) ----
ox,oy=L.OLED_C; hy=L.OLED['y0']+L.OLED_HDR_FROM_EDGE
custom("U5",[(n,ox-3.81+2.54*i,hy,nn) for i,(n,nn) in enumerate([("SDA","SDA"),("SCL","SCL"),("VCC","3V3"),("GND","GND")])],
       back=False,lab_dy=-1.9,stagger=True)
rect(L.OLED['x0'],L.OLED['y0'],L.OLED['x0']+L.OLED['w'],L.OLED['y0']+L.OLED['h'],pcbnew.F_SilkS)
text("OLED 1.3in (sits 0.8mm above board)",ox,oy+4,1.0,back=False)

# ---- power slide switch 12D00G (top side) ----
px,py=L.PWR
custom("SW18",[("RAW",px-2.5,py,"BAT_RAW"),("ON",px,py,"BAT_SW"),("",px+2.5,py,None)],size=1.5,drill=0.9,back=False,lab_dy=2.6)
rect(px-4.4,py-1.9,px+4.4,py+1.9,pcbnew.F_SilkS); text("POWER",px,py-3.2,1.0,back=False)

# ---- XIAO ESP32-S3 (bottom, component side down, USB-C out the left wall) ----
X0,Y0=L.XIAO_C
left=["POT0","POT1","POT2","POT3","SDA","SCL","BCK"]
right=[None,"GND","3V3","SPK_DIN" if V11 else "SHIFT","VSENSE","DIN","LRCK"]
ln=["D0","D1","D2","D3","D4","D5","D6"]; rn=["5V","GND","3V3","D10","D9","D8","D7"]
xp=[]
for i in range(7):
    v=-7.62+2.54*i
    xp.append((ln[i],X0+v,Y0+7.62,left[i]))
for i in range(7):
    v=-7.62+2.54*i
    xp.append((rn[i],X0+v,Y0-7.62,right[i]))
custom("U1",xp,labels=False)
for (n,x,y,_) in xp:
    kk=round((x-(X0-7.62))/2.54); off=1.9 if kk%2==0 else 3.3
    text(n,x,y+(off if y>Y0 else -off),0.8)
rect(X0-10.5,Y0-8.9,X0+10.5,Y0+8.9,pcbnew.B_SilkS); text("XIAO ESP32-S3",X0,Y0,1.0)

# ---- MAX98357A (bottom, printed side down): x increasing = LRC..VIN ----
mx=["LRC","BCLK","DIN","GAIN","SD","GND","VIN"]; mn=["LRCK","BCK","SPK_DIN" if V11 else "DIN",None,None,"GND","3V3"]
Mx,My=L.MAX_C
custom("U3",[(mx[i],Mx-7.62+2.54*i,My+8.0,mn[i]) for i in range(7)],lab_dy=-1.9,stagger=True)
rect(Mx-9.7,My-9.7,Mx+9.7,My+8.9,pcbnew.B_SilkS); text("MAX98357A",Mx,My-2,1.0); text("speaker -> + / -",Mx,My-4,0.8)

# ---- PCF8574 module P0..P7 (bottom, mounted upside down) ----
rx,ry=L.PCF_ROW
pn=["R0","R1","R2","R3","C0","C1","C2","C3"]
custom("U2",[(f"P{i}",rx-8.89+2.54*i,ry,pn[i]) for i in range(8)],lab_dy=-1.9,stagger=True)
text("PCF8574: P0 on P0",rx,ry-6.5,1.0)
custom("J9",[(n,63.6+2.54*(i%2),104.6+2.54*(i//2),nn) for i,(n,nn) in enumerate([("VCC","3V3"),("GND","GND"),("SDA","SDA"),("SCL","SCL")])],
       size=1.6,labels=False)
text("PCF I2C",64.9,102.6,0.8); text("V G  D C",64.9,109.6,0.7)

# ---- PCM5102A (bottom, printed side down, jack out the right wall). SCK..VIN x increasing ----
pc=[("SCK","GND"),("BCK","BCK"),("DIN","DIN"),("LCK","LRCK"),("GND","GND"),("VIN","3V3")]
custom("U4",[(n,L.PCM_XC-6.35+2.54*i,L.PCM_ROW_Y,nn) for i,(n,nn) in enumerate(pc)],lab_dy=-1.9,stagger=True)
rect(L.PCM_X1-L.PCM_W,L.PCM_Y0,L.PCM_X1,L.PCM_Y0+L.PCM_LEN,pcbnew.B_SilkS)
text("PCM5102A",L.PCM_XC,L.PCM_Y0+16,1.0); text("jack -> right wall",L.PCM_XC,L.PCM_Y0+13.6,0.8)

# pre-route: tie PCM SCK (pin 1) to its GND pin (pin 5) underneath pins 2-4 on the back layer
_gy=L.PCM_ROW_Y-2.0; _x1=L.PCM_XC-6.35; _x5=L.PCM_XC-6.35+4*2.54
for a,c in [((_x1,L.PCM_ROW_Y),(_x1,_gy)),((_x1,_gy),(_x5,_gy)),((_x5,_gy),(_x5,L.PCM_ROW_Y))]:
    t=pcbnew.PCB_TRACK(board); t.SetStart(K(*a)); t.SetEnd(K(*c)); t.SetWidth(mm(0.3)); t.SetLayer(pcbnew.B_Cu); t.SetNet(net("GND")); board.Add(t)
# ---- battery + divider ----
custom("J11",[("BAT-",5.2,46,"GND"),("BAT+",5.2,49.5,"BAT_RAW")],lab_dy=0,labels=False)
text("BAT-",5.2,43.6,0.8); text("BAT+",5.2,52.0,0.8); text("LiPo",5.2,55,0.9)
custom("J13",[("+",4.5,79.5,"BAT_SW"),("-",7.04,79.5,"GND")],size=1.6,lab_dy=-1.9)
text("to XIAO BAT pads",9,76.5,0.8)
r1=lib("Resistor_THT","R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal","R1",27.0,101.6,rot=90,back=True)
r2=lib("Resistor_THT","R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal","R2",46.1,101.6,rot=90,back=True)
for r,(a,b) in [(r1,("BAT_SW","VSENSE")),(r2,("VSENSE","GND"))]:
    for p in r.Pads(): p.SetNet(net(a if p.GetNumber()=="1" else b))
text("R1 R2 100k (+shift)" if V11 else "R1 R2 100k",36.5,111.2,0.8)
text("XO-HUGBOX v1.1" if V11 else "XO-HUGBOX v1",46,4.4,1.2)
board.BuildConnectivity()
pcbnew.SaveBoard(f"xo_hugbox{SUF}_placed.kicad_pcb",board)
pcbnew.ExportSpecctraDSN(board, f"xo_hugbox{SUF}.dsn")
for r in (r1,r2): print(r.GetReference(),[ (p.GetNumber(),tuple(round(c,2) for c in padxy(p))) for p in r.Pads()])
sw=board.FindFootprintByReference("SW1"); print("SW1",[(p.GetNumber(),tuple(round(c,2) for c in padxy(p))) for p in sw.Pads() if p.GetNumber()])
d=board.FindFootprintByReference("D1"); print("D1",[(p.GetNumber(),tuple(round(c,2) for c in padxy(p))) for p in d.Pads()])
