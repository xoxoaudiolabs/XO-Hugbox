import pcbnew, re, sys
from pcbnew import VECTOR2I
src, ses, out = sys.argv[1], sys.argv[2], sys.argv[3]
b = pcbnew.LoadBoard(src)
txt = open(ses).read()
# tokenizer for s-expressions
tok = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', txt)
def parse(i):
    lst = []
    while i < len(tok):
        t = tok[i]
        if t == '(':
            sub, i = parse(i+1); lst.append(sub)
        elif t == ')':
            return lst, i+1
        else:
            lst.append(t.strip('"')); i += 1
    return lst, i
tree, _ = parse(0)
root = tree[0]
def find(node, key):
    for n in node:
        if isinstance(n, list) and n and n[0] == key: yield n
routes = next(find(root, 'routes'))
res = next(find(routes, 'resolution')); unit, div = res[1], float(res[2])
scale = {'um': 1000.0, 'mm': 1e6, 'mil': 25400.0}[unit] / div   # -> nm
def nm(v): return int(round(float(v) * scale))
layers = {'F.Cu': pcbnew.F_Cu, 'B.Cu': pcbnew.B_Cu}
netout = next(find(routes, 'network_out'))
ds = b.GetDesignSettings()
ntr = nv = 0
for netn in find(netout, 'net'):
    name = netn[1]; ni = b.FindNet(name)
    for w in find(netn, 'wire'):
        for path in find(w, 'path'):
            lay, width = layers[path[1]], nm(path[2]); c = path[3:]
            pts = [(nm(c[k]), -nm(c[k+1])) for k in range(0, len(c), 2)]
            for a, bb in zip(pts, pts[1:]):
                t = pcbnew.PCB_TRACK(b); t.SetStart(VECTOR2I(*a)); t.SetEnd(VECTOR2I(*bb))
                t.SetWidth(width); t.SetLayer(lay); t.SetNet(ni); b.Add(t); ntr += 1
    for v in find(netn, 'via'):
        x, y = nm(v[2]), -nm(v[3])
        via = pcbnew.PCB_VIA(b); via.SetPosition(VECTOR2I(x, y)); via.SetWidth(pcbnew.FromMM(0.8))
        via.SetDrill(pcbnew.FromMM(0.4)); via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); via.SetNet(ni); b.Add(via); nv += 1
b.BuildConnectivity()
pcbnew.SaveBoard(out, b)
pcbnew.WriteDRCReport(b, 'drc.rpt', pcbnew.EDA_UNITS_MILLIMETRES, True)
print('tracks', ntr, 'vias', nv)
