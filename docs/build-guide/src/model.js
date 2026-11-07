// XO-Hugbox 3D model: every part built from hardware/layout.py, the routed KiCad board and the STLs
// (all packed into data.js). Coordinates are plate coordinates in mm, x right, y up (toward the
// screen end), z up from the shell floor - the same frame as layout.py and assembly.scad.
// Classic script: defines window.buildHugboxModel(THREE, CASE) so the bundled page needs only one ES module.
// CASE: 'printed' (3D-printed shell + plate) or 'alu' (aluminium shell + 4-layer acrylic, hardware/mechanical/alu_acrylic).
window.buildHugboxModel = function (THREE, CASE) {
'use strict';
const ALU = CASE === 'alu';

const D = window.HUGBOX_DATA;
const L = D.layout;
const PCB = D.pcb;
const FP = Object.fromEntries(PCB.fps.map((f) => [f.ref, f]));

const Z = {};
Z.pcb = L.Z_PCB;                       // PCB underside
Z.pcbTop = L.Z_PCB + L.PCB_T;          // PCB top
Z.plate = ALU ? Z.pcbTop + L.ACR_SPACER : L.Z_PLATE_BOT;          // plate / acrylic layer 1 underside
Z.plateTop = ALU ? Z.pcbTop + L.ACR_SW_TOP : L.Z_PLATE_BOT + L.PLATE_T;   // the same 5.0 mm above the PCB both ways
Z.hdr = 2.5;                           // header plastic between PCB and the bottom modules
Z.modTop = Z.pcb - Z.hdr;              // bottom modules: face toward the PCB
Z.strip = ALU ? Z.plateTop + 2 * L.ACR_KNOB_T                                   // acrylic layers 3 + 4
  : Z.plate + ((L.Z_PCB + L.PCB_T + L.POT_BODY_H) - L.Z_PLATE_BOT) + 2.0;   // raised pot strip top

// pad names printed on the board silk for the custom footprints (hardware/pcb/build.py)
const PADNAMES = {
  U1: ['D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', '5V', 'GND', '3V3', 'D10', 'D9', 'D8', 'D7'],
  U2: ['P0', 'P1', 'P2', 'P3', 'P4', 'P5', 'P6', 'P7'],
  U3: ['LRC', 'BCLK', 'DIN', 'GAIN', 'SD', 'GND', 'VIN'],
  U4: ['SCK', 'BCK', 'DIN', 'LCK', 'GND', 'VIN'],
  U5: ['SDA', 'SCL', 'VCC', 'GND'],
  J9: ['VCC', 'GND', 'SDA', 'SCL'],
  J11: ['BAT-', 'BAT+'],
  J13: ['+', '-'],
  SW18: ['RAW', 'ON', '(nc)'],
  RV1: ['GND', 'W', '3V3'], RV2: ['GND', 'W', '3V3'], RV3: ['GND', 'W', '3V3'], RV4: ['GND', 'W', '3V3'],
};

// ---------------------------------------------------------------- part registry
const parts = new Map();
const root = new THREE.Group();

class Part {
  constructor(id, o) {
    Object.assign(this, { name: id, step: 0, explode: 0, info: '', refs: [], qty: 1, xray: false }, o);
    this.id = id;
    this.group = new THREE.Group();
    this.group.name = id;
    this.mats = new Map();
    this.state = 'normal';
    root.add(this.group);
    parts.set(id, this);
  }
  mat(color, o = {}) {
    const key = color + JSON.stringify(o);
    if (!this.mats.has(key)) {
      const m = new THREE.MeshStandardMaterial({ color, roughness: 0.55, metalness: 0.05, ...o });
      m.userData.base = { opacity: m.opacity, transparent: m.transparent, emissive: m.emissive.getHex() };
      this.mats.set(key, m);
    }
    return this.mats.get(key);
  }
  add(geom, color, x = 0, y = 0, z = 0, o) {
    const m = new THREE.Mesh(geom, color instanceof THREE.Material ? color : this.mat(color, o));
    if (color instanceof THREE.Material && !this.mats.has(color.uuid)) {
      color.userData.base = { opacity: color.opacity, transparent: color.transparent, emissive: color.emissive ? color.emissive.getHex() : 0 };
      this.mats.set(color.uuid, color);
    }
    m.position.set(x, y, z);
    m.userData.part = this.id;
    this.group.add(m);
    return m;
  }
  box(w, d, h, color, x, y, z0, o) { return this.add(new THREE.BoxGeometry(w, d, h), color, x, y, z0 + h / 2, o); }
  cylZ(r, h, color, x, y, z0, seg = 24, o) {
    const g = new THREE.CylinderGeometry(r, r, h, seg); g.rotateX(Math.PI / 2);
    return this.add(g, color, x, y, z0 + h / 2, o);
  }
  cylAxis(r, a, b, color, seg = 16, o) {   // cylinder between two points
    const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b);
    const g = new THREE.CylinderGeometry(r, r, A.distanceTo(B), seg);
    const m = this.add(g, color, ...A.clone().add(B).multiplyScalar(0.5).toArray(), o);
    m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), B.clone().sub(A).normalize());
    return m;
  }
  wire(points, color, r = 0.45) {
    const c = new THREE.CatmullRomCurve3(points.map((p) => new THREE.Vector3(...p)));
    return this.add(new THREE.TubeGeometry(c, 40, r, 8), color, 0, 0, 0, { roughness: 0.4 });
  }
}
const part = (id, o) => new Part(id, o);

// shared geometry helpers
const GOLD = '#d4b04c', SILVER = '#c8c9cc', BLACK = '#161616';
function headerPins(p, ref, zBottom, zTop, plastic = true) {
  const pads = FP[ref].pads;
  for (const pd of pads) {
    p.cylZ(0.32, zTop - zBottom, GOLD, pd.xy[0], pd.xy[1], zBottom, 8, { metalness: 0.8, roughness: 0.3 });
    if (plastic) p.box(2.5, 2.5, Z.hdr, BLACK, pd.xy[0], pd.xy[1], Z.modTop);
  }
}
function padCenter(ref) {
  const ps = FP[ref].pads.filter((q) => !q.np);
  return [ps.reduce((s, q) => s + q.xy[0], 0) / ps.length, ps.reduce((s, q) => s + q.xy[1], 0) / ps.length];
}
const pad = (ref, n) => FP[ref].pads.find((q) => q.n === String(n)).xy;

// ---------------------------------------------------------------- PCB with plotted copper + silk
function pcbTexture(side) {
  const S = 10, W = L.W, H = L.H;
  const cv = document.createElement('canvas');
  cv.width = Math.round(W * S); cv.height = Math.round(H * S);
  const g = cv.getContext('2d');
  const X = (x) => x * S, Yc = (y) => (H - y) * S;
  g.fillStyle = '#1f6b3a'; g.fillRect(0, 0, cv.width, cv.height);
  // copper under mask
  g.lineCap = 'round';
  g.strokeStyle = '#2f8a4c';
  for (const [x1, y1, x2, y2, w, layer] of PCB.tracks) {
    if ((layer === 0) !== (side === 'top')) continue;
    g.lineWidth = w * S; g.beginPath(); g.moveTo(X(x1), Yc(y1)); g.lineTo(X(x2), Yc(y2)); g.stroke();
  }
  // pads (HASL), drills
  for (const f of PCB.fps) for (const q of f.pads) {
    const [x, y] = q.xy, [sx, sy] = q.sz;
    if (!q.np) {
      g.fillStyle = '#cfd0c8';
      g.save(); g.translate(X(x), Yc(y)); g.rotate(-(q.ang || 0) * Math.PI / 180);
      if (q.sh === 'rect') g.fillRect(-sx * S / 2, -sy * S / 2, sx * S, sy * S);
      else { g.beginPath(); g.roundRect(-sx * S / 2, -sy * S / 2, sx * S, sy * S, Math.min(sx, sy) * S / 2); g.fill(); }
      g.restore();
    }
    g.fillStyle = '#0b0d0c';
    const [dx, dy] = q.dr;
    g.save(); g.translate(X(x), Yc(y)); g.rotate(-(q.ang || 0) * Math.PI / 180);
    g.beginPath(); g.roundRect(-dx * S / 2, -(dy || dx) * S / 2, dx * S, (dy || dx) * S, Math.min(dx, dy || dx) * S / 2); g.fill();
    g.restore();
  }
  for (const v of PCB.vias) {
    g.fillStyle = '#cfd0c8'; g.beginPath(); g.arc(X(v.xy[0]), Yc(v.xy[1]), v.d * S / 2, 0, 7); g.fill();
    g.fillStyle = '#0b0d0c'; g.beginPath(); g.arc(X(v.xy[0]), Yc(v.xy[1]), v.dr * S / 2, 0, 7); g.fill();
  }
  // silk
  g.strokeStyle = g.fillStyle = '#f2f2ea';
  for (const s of PCB.silk) {
    if (s.back !== (side === 'bottom')) continue;
    g.lineWidth = Math.max(s.w, 0.12) * S;
    g.beginPath();
    if (s.k === 'l') { g.moveTo(X(s.a[0]), Yc(s.a[1])); g.lineTo(X(s.b[0]), Yc(s.b[1])); }
    else g.arc(X(s.a[0]), Yc(s.a[1]), s.r * S, 0, 7);
    g.stroke();
  }
  g.fillStyle = '#f2f2ea'; g.textAlign = 'center'; g.textBaseline = 'middle';
  for (const t of PCB.silkText) {
    if (t.back !== (side === 'bottom')) continue;
    g.save(); g.translate(X(t.xy[0]), Yc(t.xy[1]));
    g.rotate(-(t.ang || 0) * Math.PI / 180);
    if (t.back) g.scale(-1, 1);         // back silk is drawn mirrored so it reads correctly from below
    g.font = `600 ${t.s * S * 1.32}px "Consolas", "DejaVu Sans Mono", monospace`;
    g.fillText(t.t, 0, 0);
    g.restore();
  }
  const tex = new THREE.CanvasTexture(cv);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 8;
  return tex;
}

function pcbShape() {
  const I = L.INSET, [nx0, ny0, nx1] = L.NOTCH;
  const pts = [[I, I], [L.W - I, I], [L.W - I, L.H - I], [nx1, L.H - I], [nx1, ny0], [nx0, ny0]];
  const s = new THREE.Shape(pts.map(([x, y]) => new THREE.Vector2(x, y)));
  for (const f of PCB.fps) if (f.ref.startsWith('H')) {
    const h = new THREE.Path(); h.absarc(f.xy[0], f.xy[1], 1.6, 0, Math.PI * 2, true); s.holes.push(h);
  }
  return s;
}
function uvToPlate(geom) {
  const pos = geom.attributes.position, uv = geom.attributes.uv;
  for (let i = 0; i < pos.count; i++) uv.setXY(i, pos.getX(i) / L.W, pos.getY(i) / L.H);
  uv.needsUpdate = true;
}

// ---------------------------------------------------------------- STL meshes (int16, 0.01 mm)
function stlGeometry(b64) {
  const bin = atob(b64), n = bin.length / 2;
  const view = new DataView(new ArrayBuffer(bin.length));
  for (let i = 0; i < bin.length; i++) view.setUint8(i, bin.charCodeAt(i));
  const pos = new Float32Array(n);
  for (let i = 0; i < n; i++) pos[i] = view.getInt16(i * 2, true) * D.stl.scale;
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.computeVertexNormals();
  return g;
}

function keycapGeometry() {
  const g = new THREE.CylinderGeometry(12.7 / Math.SQRT2, 18 / Math.SQRT2, 7.4, 4, 1);
  g.rotateY(Math.PI / 4); g.rotateX(Math.PI / 2);
  return g.toNonIndexed();
}

function oledTexture() {
  const bits = atob(D.oled);
  const cv = document.createElement('canvas'); cv.width = 128; cv.height = 64;
  const g = cv.getContext('2d'); const img = g.createImageData(128, 64);
  for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
    const on = (bits.charCodeAt(y * 16 + (x >> 3)) >> (7 - (x & 7))) & 1, i = (y * 128 + x) * 4;
    img.data[i] = on ? 225 : 4; img.data[i + 1] = on ? 240 : 6; img.data[i + 2] = on ? 255 : 10; img.data[i + 3] = 255;
  }
  g.putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(cv); t.magFilter = THREE.NearestFilter; t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

// ---------------------------------------------------------------- build everything
function buildModel(STEP) {
  const P = L.P;
  const keyXY = [];
  for (let n = 0; n < 16; n++) keyXY.push([L.cx[n & 3], L.cy[3 - (n >> 2)]]);

  // ---- PCB
  {
    const p = part('pcb', { name: 'XO-Hugbox v1 PCB', step: 0, explode: 45, refs: [],
      info: '2-layer, 1.6 mm, HASL. Top = component side for keys, pots, OLED, power switch; the four modules, diodes and resistors go on the back.' });
    const shape = pcbShape();
    const body = new THREE.ExtrudeGeometry(shape, { depth: L.PCB_T, bevelEnabled: false, curveSegments: 16 });
    p.add(body, '#16502b', 0, 0, Z.pcb, { roughness: 0.7 });
    const top = new THREE.ShapeGeometry(shape, 16); uvToPlate(top);
    const topMat = new THREE.MeshStandardMaterial({ map: pcbTexture('top'), roughness: 0.6, metalness: 0.0 });
    p.add(top, topMat, 0, 0, Z.pcbTop + 0.01);
    const bot = new THREE.ShapeGeometry(shape, 16); uvToPlate(bot);
    const botMat = new THREE.MeshStandardMaterial({ map: pcbTexture('bottom'), roughness: 0.6, side: THREE.BackSide });
    p.add(bot, botMat, 0, 0, Z.pcb - 0.01);
  }

  // ---- diodes (back)
  {
    const p = part('diodes', { name: '1N4148 diodes D1-D16', qty: 16, step: STEP.diodes, explode: 45,
      refs: Array.from({ length: 16 }, (_, i) => 'D' + (i + 1)),
      info: 'One per pad, on the BACK. Black band (cathode) to the SQUARE pad - that pad is the row line R0-R3.' });
    for (let i = 1; i <= 16; i++) {
      const a = pad('D' + i, 1), b = pad('D' + i, 2);        // 1 = cathode (square pad)
      const zc = Z.pcb - 1.1, mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2, dir = Math.sign(a[0] - b[0]);
      p.cylAxis(0.95, [mx - dir * 1.9, my, zc], [mx + dir * 1.9, my, zc], '#d24a1c', 16, { transparent: true, opacity: 0.85, roughness: 0.15 });
      p.cylAxis(0.98, [mx + dir * 1.1, my, zc], [mx + dir * 1.7, my, zc], '#111', 16);
      for (const q of [a, b]) {
        const end = [mx + Math.sign(q[0] - mx) * 1.9, my, zc];
        p.cylAxis(0.25, [q[0], q[1], zc], end, SILVER, 6, { metalness: 0.8 });
        p.cylAxis(0.25, [q[0], q[1], zc], [q[0], q[1], Z.pcbTop + 0.6], SILVER, 6, { metalness: 0.8 });
      }
    }
  }

  // ---- divider resistors (back)
  {
    const p = part('resistors', { name: '100 kΩ divider R1, R2', qty: 2, step: STEP.diodes, explode: 45, refs: ['R1', 'R2'],
      info: 'Battery sense divider (Vbat/2 into D9). 100k = brown-black-yellow-gold. No polarity. Back side.' });
    for (const r of ['R1', 'R2']) {
      const a = pad(r, 1), b = pad(r, 2), zc = Z.pcb - 1.4, mx = a[0], my = (a[1] + b[1]) / 2;
      p.cylAxis(1.25, [mx, my - 3.15, zc], [mx, my + 3.15, zc], '#d8c39a', 16);
      [['#6b3b1a', -2.0], ['#111', -1.0], ['#e3c21a', 0.0], ['#c9a44a', 2.1]].forEach(([c, o]) =>
        p.cylAxis(1.3, [mx, my + o - 0.3, zc], [mx, my + o + 0.3, zc], c, 16));
      for (const q of [a, b]) {
        p.cylAxis(0.28, [q[0], q[1], zc], [q[0], my + Math.sign(q[1] - my) * 3.15, zc], SILVER, 6, { metalness: 0.8 });
        p.cylAxis(0.28, [q[0], q[1], zc], [q[0], q[1], Z.pcbTop + 0.6], SILVER, 6, { metalness: 0.8 });
      }
    }
  }

  // ---- XIAO ESP32-S3 (bottom, component side down, USB-C to the left wall)
  {
    const p = part('xiao', { name: 'Seeed XIAO ESP32-S3', step: STEP.xiao, explode: 30, refs: ['U1', 'J13'],
      info: 'Must be the S3 (native USB MIDI + 8 MB PSRAM). Shield can faces the floor, USB-C toward the left wall.' });
    const [x0, y0] = L.XIAO_C, zb = Z.modTop - 1.2;
    p.box(21, 17.8, 1.2, '#1b2238', x0, y0, zb);
    p.box(12.5, 14, 2.6, SILVER, x0 + 2.4, y0, zb - 2.6, { metalness: 0.85, roughness: 0.3 });
    p.box(7.4, 9, 3.2, '#d8d8d8', x0 - 10.5 + 3.0, y0, zb - 3.2, { metalness: 0.8, roughness: 0.3 });   // USB-C, out the left end
    p.box(1.2, 1.2, 0.4, '#e0e0e0', x0 + 6.5, y0 - 5.5, zb - 0.4);                                     // U.FL
    headerPins(p, 'U1', zb - 0.6, Z.pcbTop + 1.2);
    // battery leads soldered to the BAT pads (on the face toward the PCB) -> J13 "+ -"
    const j1 = pad('J13', 1), j2 = pad('J13', 2);
    p.wire([[x0 - 7, y0 - 6.5, Z.modTop + 0.2], [x0 - 7, y0 - 11, Z.modTop - 0.5], [j1[0], j1[1] + 1.5, Z.pcb - 2], [j1[0], j1[1], Z.pcb]], '#d03030', 0.4);
    p.wire([[x0 - 4.5, y0 - 6.5, Z.modTop + 0.2], [x0 - 4.5, y0 - 11, Z.modTop - 0.8], [j2[0], j2[1] + 1.5, Z.pcb - 2.2], [j2[0], j2[1], Z.pcb]], '#222', 0.4);
  }

  // ---- MAX98357A (bottom, printed side to the floor)
  {
    const p = part('max', { name: 'MAX98357A I2S amp', step: STEP.max, explode: 30, refs: ['U3'],
      info: 'Header from the plain side, printed side faces the floor. GAIN floating = 9 dB, SD left default (on, L+R mono). Speaker leads to + / -.' });
    const [x0, y0] = L.MAX_C, zb = Z.modTop - 1.6;
    p.box(19.4, 18.6, 1.6, '#1e5bd6', x0, y0 - 0.4, zb);
    p.box(3, 3, 0.9, BLACK, x0, y0 - 1, zb - 0.9);
    p.box(7.5, 4, 1.2, '#2a8a3a', x0, y0 - 7.6, zb - 1.2);                          // speaker +/- pads/terminal
    headerPins(p, 'U3', zb - 0.6, Z.pcbTop + 1.2);
  }

  // ---- PCF8574T module (bottom, upside down, P0 on P0) + I2C wires to J9
  {
    const p = part('pcf', { name: 'PCF8574T I/O expander module', step: STEP.pcf, explode: 30, refs: ['U2', 'J9'],
      info: 'Key-matrix expander at I2C 0x20. Socket removed, A0-A2 jumper caps replaced by solder bridges (all low = 0x20). P0 pin on the P0 pad. I2C pins wired to the 2x2 "PCF I2C" pads.' });
    const [rx, ry] = L.PCF_ROW, zb = Z.modTop - 1.6;
    p.box(40, 15, 1.6, '#1e5bd6', rx, ry - 6.5, zb);
    p.box(7.5, 5.2, 1.6, BLACK, rx + 4, ry - 8, zb - 1.6);                                    // the PCF8574T chip (SOIC)
    for (let i = 0; i < 3; i++) {                                                               // address bridges
      p.box(1.4, 3.2, 0.3, '#c8c8c8', rx - 9 + i * 2.6, ry - 9.5, zb - 0.3, { metalness: 0.8 });
    }
    headerPins(p, 'U2', zb - 0.6, Z.pcbTop + 1.2);
    // right-angle I2C pins at the left end, short wires to J9
    const names = ['VCC', 'GND', 'SDA', 'SCL'], cols = ['#d03030', '#222', '#2f7de1', '#e2c21a'];
    for (let i = 0; i < 4; i++) {
      const sx = rx - 20 - 3, sy = ry - 11 + i * 2.54;
      p.cylAxis(0.32, [sx + 3, sy, zb + 0.8], [sx - 1, sy, zb + 0.8], GOLD, 8, { metalness: 0.8 });
      const t = pad('J9', i + 1);
      p.wire([[sx - 1, sy, zb + 0.8], [sx - 5, sy + 4, zb - 2], [t[0] - 6, t[1] - 2, zb - 2], [t[0], t[1], Z.pcb - 2], [t[0], t[1], Z.pcb]], cols[i], 0.32);
    }
  }

  // ---- PCM5102A (bottom, printed side down, right wall, jack out the right wall)
  {
    const p = part('pcm', { name: 'PCM5102A DAC (purple)', step: STEP.pcm, explode: 30, refs: ['U4'],
      info: 'Printed side down, along the right wall, jack end toward the keys, jack opens out the long VIN-side edge into the right wall. Jumpers FLT L, DEMP L, XSMT H, FMT L. SCK is tied to GND on the PCB.' });
    const zb = Z.modTop - 1.6;
    p.box(L.PCM_W, L.PCM_LEN, 1.6, '#6a2a9a', L.PCM_XC, L.PCM_Y0 + L.PCM_LEN / 2, zb);
    p.box(6, 12, 5.2, BLACK, L.PCM_X1 - 3.4, L.PCM_Y0 + 6.5, zb - 5.2);                    // 3.5 mm jack body
    p.cylAxis(3, [L.PCM_X1 - 0.5, L.JACK_Y, zb - 2.8], [L.PCM_X1 + 1.6, L.JACK_Y, zb - 2.8], BLACK, 24);
    p.box(5, 6.4, 1.4, BLACK, L.PCM_XC - 2, L.PCM_Y0 + 20, zb - 1.4);                      // the DAC chip
    for (let i = 0; i < 4; i++) p.box(1.4, 2.6, 0.3, '#c8c8c8', L.PCM_XC - 5 + i * 2.4, L.PCM_Y0 + 26.5, zb + 1.6, { metalness: 0.8 });
    headerPins(p, 'U4', zb - 0.6, Z.pcbTop + 0.3);
  }

  // ---- MX switches (snap into the plate, solder to the PCB) + 1 mm spacers
  {
    const p = part('switches', { name: 'MX switches (16 pads + shift)', qty: 17, step: STEP.switches, explode: 75,
      refs: Array.from({ length: 17 }, (_, i) => 'SW' + (i + 1)),
      info: ALU ? 'Snap all 17 into acrylic layer 2 (the 1.5 mm switch plate) from the top, slide layer 1 on from below, then fit the stack onto the PCB and solder. Pins point toward the knobs.'
        : 'Snap all 17 into the printed plate first (from the top), then fit plate + switches onto the PCB and solder. Pins point toward the knobs.' });
    const all = [...keyXY, L.SHIFT];
    all.forEach(([x, y], i) => {
      p.box(14, 14, (ALU ? Z.plateTop - L.ACR_SW_T : Z.plate) - Z.pcbTop + 0.01, '#2b2b2b', x, y, Z.pcbTop);   // lower housing, in the plate
      const g = new THREE.CylinderGeometry(11 / Math.SQRT2, 15.6 / Math.SQRT2, 5.6, 4, 1); g.rotateY(Math.PI / 4); g.rotateX(Math.PI / 2);
      p.add(g.toNonIndexed(), '#353535', x, y, Z.plateTop + 2.8);
      p.box(4, 1.2, 3.6, i === 16 ? '#c8331d' : '#a32b23', x, y, Z.plateTop + 5.6);         // stem
      const sw = FP['SW' + (i + 1)];
      for (const q of sw.pads.filter((q) => !q.np)) p.cylZ(0.5, 4.2, SILVER, q.xy[0], q.xy[1], Z.pcb - 1.2, 8, { metalness: 0.8 });
      p.cylZ(1.9, 2.2, '#2b2b2b', sw.pads[1].xy[0], sw.pads[1].xy[1], Z.pcb - 0.6, 16);    // centre post
    });
  }
  {
    const p = part('spacers', { name: ALU ? '1.5 mm spacers (PCB to acrylic)' : '1 mm spacers (PCB to plate)', qty: 5, step: STEP.plate, explode: 60,
      info: 'Between the PCB and the plate at the four corner screws and the support post by the knobs, so the MX switches seat at the right height (plate top 5.0 mm above the PCB).' });
    for (const [x, y] of L.HOLES) {
      p.cylZ(3.0, Z.plate - Z.pcbTop, '#9da3a8', x, y, Z.pcbTop, 24, { metalness: 0.5, transparent: true, opacity: 0.9 });
    }
  }

  // ---- pots (WH148 B10K, straight lugs toward the keys, wire legs down into the oval slots)
  {
    const p = part('pots', { name: 'WH148 B10K 16 mm pots', qty: 4, step: STEP.underplate, explode: 50, refs: ['RV1', 'RV2', 'RV3', 'RV4'],
      info: 'Straight-lug WH148s: the solder lugs leave the can about 5.3 mm above the board, pointing at the keys. A short wire leg (diode lead offcut) runs from each lug straight down into the plated oval slot under it (GND, wiper, 3V3 left to right). The nut on the plate holds the pot, so the legs carry no load.' });
    const R = L.POT_CAN_D / 2, MET = { metalness: 0.7, roughness: 0.35 };
    for (let i = 0; i < 4; i++) {
      const x = L.cx[i], y = L.POT_Y;
      p.cylZ(R, L.POT_BODY_H, '#c8c8c8', x, y, Z.pcbTop, 48, MET);
      p.cylZ(6.65 / 2, 7.33, '#d0d0d0', x, y, Z.pcbTop + L.POT_BODY_H, 24, { metalness: 0.8, roughness: 0.3 });   // M7 collar
      p.cylZ(5.95 / 2, 8.26, '#e3e3e3', x, y, Z.pcbTop + L.POT_BODY_H + 7.33, 24, { metalness: 0.6, roughness: 0.4 });
      const lz = Z.pcbTop + L.POT_LUG_Z, legY = y - (R + L.POT_LUG_L - 1.2);
      p.box(13, 2.2, L.POT_LUG_T, '#b8b8b8', x, y - R - 0.6, lz, MET);                                            // frame tab
      for (let k = -1; k <= 1; k++) {
        p.box(2.0, L.POT_LUG_L, L.POT_LUG_T, '#d8c27a', x + k * L.POT_PITCH, y - R - L.POT_LUG_L / 2, lz, { metalness: 0.8, roughness: 0.3 });
        p.cylZ(0.3, lz - (Z.pcb - 1.2), SILVER, x + k * L.POT_PITCH, legY, Z.pcb - 1.2, 8, { metalness: 0.8, roughness: 0.3 });
      }
    }
  }

  // ---- OLED (header toward the pots, so it is upside down; 0.8 mm above the board)
  let screenMat;
  {
    const p = part('oled', { name: '1.3" SH1106 I2C OLED', step: STEP.underplate, explode: 50, refs: ['U5'],
      info: 'Header plastic removed, module soldered 0.8 mm above the board. Header toward the pots, so the module is upside down - the firmware uses U8G2_R2 to turn the picture back. Board order SDA SCL VCC GND left to right.' });
    const [ox, oy] = L.OLED_C;
    p.box(L.OLED_MOD[0], L.OLED_MOD[1], 1.2, '#1e5bd6', ox, L.OLED_MOD_Y0 + L.OLED_MOD[1] / 2, Z.pcbTop + 0.8);
    p.box(L.OLED_GLASS[0], L.OLED_GLASS[1], 1.6, '#050505', ox, oy + L.OLED_GLASS_OFF, Z.pcbTop + 2.0, { roughness: 0.15, metalness: 0.2 });
    screenMat = new THREE.MeshBasicMaterial({ map: oledTexture(), toneMapped: false });
    const scr = p.add(new THREE.PlaneGeometry(29.42, 14.7), screenMat, ox, oy + L.OLED_WIN_OFF, Z.pcbTop + 3.62);
    scr.rotation.z = 0;
    for (const q of FP.U5.pads) p.cylZ(0.32, 4.0, GOLD, q.xy[0], q.xy[1], Z.pcb - 1.0, 8, { metalness: 0.8 });
  }

  // ---- power switch 12D00G (top, actuator through the plate)
  {
    const p = part('power', { name: '12D00G slide switch (power)', step: STEP.underplate, explode: 50, refs: ['SW18'],
      info: 'On the top side, actuator up through the plate slot. RAW (battery) - ON (switched rail to the XIAO BAT pads and divider).' });
    const [x, y] = L.PWR;
    p.box(8.7, 3.7, 3.7, '#c8c8c8', x, y, Z.pcbTop, { metalness: 0.8, roughness: 0.3 });
    p.box(1.5, 1.5, 5, BLACK, x - 0.75, y, Z.pcbTop + 3.7);
    for (const q of FP.SW18.pads) p.box(0.6, 0.3, 4.5, SILVER, q.xy[0], q.xy[1], Z.pcb - 1.2, { metalness: 0.8 });
  }

  // ---- top plate and bottom shell (STLs): printed, or the aluminium + acrylic variant
  if (ALU) {
    const p = part('plate', { name: 'Clear acrylic faceplate (4 layers)', step: STEP.switches, explode: 75, xray: true,
      info: 'Laser-cut clear acrylic: layer 1 (2 mm, pockets), layer 2 (1.5 mm switch plate), layers 3 + 4 (2 mm each, the knob strip, solvent-welded to layer 2). The pot nuts clamp layer 4 to the cans. DXFs in hardware/mechanical/alu_acrylic.' });
    p.add(stlGeometry(D.stl.acrylic), '#d6ecf7', 0, 0, 0, { roughness: 0.05, metalness: 0.0, transparent: true, opacity: 0.38 });   // STL is in shell z
  } else {
    const p = part('plate', { name: '3D-printed top plate', step: STEP.switches, explode: 75, xray: true,
      info: 'Print with a layer height that divides 1.5 mm (0.15 / 0.25 / 0.3) so the MX clip ledge is exact. Test one switch hole first.' });
    p.add(stlGeometry(D.stl.plate), '#f2efe6', 0, 0, Z.plate, { roughness: 0.8 });
  }
  if (ALU) {
    const p = part('shell', { name: 'Aluminium shell (black anodized)', step: STEP.speaker, explode: 0, xray: true,
      info: 'CNC 6061, Type II black anodize. PCB rests on the five bosses at z = 16.5 mm (four tapped M3, one plain support by the knobs); acrylic layer 1 rests on the wall top. Tapped M2.5 speaker posts, USB-C and jack openings; no battery cradle (foam tape).' });
    p.add(stlGeometry(D.stl.aluShell), '#1c1c1f', 0, 0, 0, { roughness: 0.42, metalness: 0.65 });
  } else {
    const p = part('shell', { name: '3D-printed bottom shell', step: STEP.speaker, explode: 0, xray: true,
      info: 'PCB rests on the five bosses at z = 16.5 mm; plate rests on the wall top. Speaker posts, battery cradle, USB-C and jack openings are built in.' });
    p.add(stlGeometry(D.stl.shell), '#d9d4c7', 0, 0, 0, { roughness: 0.85 });
  }
  const spkTop = ALU ? Z.plate : Z.plate + 0.8;     // speaker face: under acrylic layer 1, or in the printed plate's 0.8 mm locator

  // ---- speaker + M2.5 screws + leads from the MAX98357A
  {
    const p = part('speaker', { name: '3 W 8 Ω box speaker 28.2×31×15.3', step: STEP.speaker, explode: 18,
      info: (ALU ? 'Face up against the grille in acrylic layer 1, ears screwed to the two tapped shell posts with M2.5×6 machine screws.'
        : 'Face up against the plate grille (sits in the 0.8 mm locator), ears screwed to the two shell posts with M2.5×6 self-tappers.') + ' Ear spacing ~37 mm is an estimate - measure yours.' });
    const [sx, sy] = L.SPK_C, top = spkTop, h = 15.3;
    p.box(31, 28.2, h, '#1b1b1b', sx, sy, top - h);
    p.cylZ(10.5, 0.4, '#2c2c2c', sx, sy, top - 0.2, 40);
    p.cylZ(4, 0.6, '#3a3a3a', sx, sy, top - 0.1, 24);
    const ez = top - L.SPK_EAR_DROP - L.SPK_EAR_T / 2;
    for (const s of [-1, 1]) {
      p.box(11, L.SPK_EAR_PITCH / 2 - 14.1 + 3.2, L.SPK_EAR_T, '#1b1b1b', sx, sy + s * (14.1 + (L.SPK_EAR_PITCH / 2 - 14.1 + 3.2) / 2), ez);
    }
    // leads from the MAX98357A +/- to the speaker tabs
    const mx = L.MAX_C[0], my = L.MAX_C[1] - 7.6;
    p.wire([[mx - 1.5, my, Z.modTop - 2.9], [mx - 6, my + 8, 6], [sx + 10, sy - 20, 6], [sx + 6, sy - 14.1, top - 6]], '#d03030', 0.35);
    p.wire([[mx + 1.5, my, Z.modTop - 2.9], [mx - 3, my + 9, 5], [sx + 12, sy - 19, 5], [sx + 8, sy - 14.1, top - 6]], '#222', 0.35);
  }
  {
    const p = part('spkScrews', { name: ALU ? 'M2.5×6 machine screws (speaker)' : 'M2.5×6 self-tapping screws (speaker)', qty: 2, step: STEP.speaker, explode: 26,
      info: 'Through the speaker ears into the two shell posts.' });
    const ez = spkTop - L.SPK_EAR_DROP + L.SPK_EAR_T / 2;
    for (const s of [-1, 1]) {
      const y = L.SPK_C[1] + s * L.SPK_EAR_PITCH / 2;
      p.cylZ(2.2, 1.6, SILVER, L.SPK_C[0], y, ez, 20, { metalness: 0.6, roughness: 0.35 });
      p.cylZ(1.25, 6, SILVER, L.SPK_C[0], y, ez - 6, 12, { metalness: 0.6, roughness: 0.35 });
    }
  }

  // ---- LiPo in the cradle + leads to J11, U.FL antenna on the left wall
  {
    const p = part('battery', { name: 'LP102540 1100 mAh LiPo', step: STEP.battery, explode: 8, refs: ['J11'],
      info: (ALU ? 'On the shell floor under the keys, held with foam tape (the aluminium shell has no cradle).' : 'In the shell cradle under the keys.') + ' Leads to the BAT+ / BAT- pads (J11). Check polarity against the pads with a meter - lead colours are not always right.' });
    p.box(25, 40, 10, '#aeb6c0', 46, 40, 2.2, { metalness: 0.5, roughness: 0.35 });
    p.box(23, 5, 3, '#2b6a3b', 46, 62.2, 5.5);    // protection board
    const a = pad('J11', 1), b = pad('J11', 2);
    p.wire([[42, 63.5, 7], [30, 62, 8], [10, b[1] + 2, 11], [b[0], b[1], Z.pcb - 1.5], [b[0], b[1], Z.pcb]], '#d03030', 0.45);
    p.wire([[46, 63.5, 7], [30, 60, 7.5], [10, a[1] - 2, 10.5], [a[0], a[1], Z.pcb - 1.5], [a[0], a[1], Z.pcb]], '#222', 0.45);
  }
  // ---- INMP441 mic: label (port) side up on its own header, straight above J_MIC; the plate pocket closes over it.
  //      The can is underneath, facing the board; the header plastic is the 2.5 mm spacer. MTDI wire on the back.
  {
    const p = part('mic', { name: 'INMP441 I2S mic module', step: STEP.mic, explode: 50, refs: ['J_MIC'],
      info: 'Label side up (the sound port is the hole in the middle of that side), on a 2×3 header into J_MIC: SCK WS L/R toward the knobs, SD VDD GND toward the keys. L/R is tied to GND, so the mic sends on the left slot.' });
    const spacer = ALU ? L.ACR_MIC_SPACER : L.MIC_SPACER;   // acrylic: header plastic off, 1.5 mm above the board
    const [mx, my] = L.MIC_C, R = L.MIC_D / 2, z0 = Z.pcbTop + spacer;
    p.cylZ(R, L.MIC_PCB_T, '#1c1f26', mx, my, z0, 48);                                   // module board
    p.cylZ(1.1, 0.05, '#c89a50', mx, my, z0 + L.MIC_PCB_T, 24);                           // port ring
    p.cylZ(0.45, 0.06, '#111', mx, my, z0 + L.MIC_PCB_T + 0.01, 16);                      // port
    p.box(4.7, 3.8, 1.0, '#c0c0c0', mx + 0.6, my, z0 - 1.0, { metalness: 0.7, roughness: 0.3 });   // mic can (underside)
    for (const j of [-1, 1]) {
      const py = my + j * L.MIC_ROW_DY;
      if (!ALU) p.box(3 * L.MIC_PITCH, 2.5, L.MIC_SPACER, '#222', mx, py, Z.pcbTop);      // header plastic
      for (const i of [-1, 0, 1])
        p.box(0.64, 0.64, spacer + L.MIC_PCB_T + Z.pcbTop - Z.pcb + 1.2, '#d4af37', mx + i * L.MIC_PITCH, py, Z.pcb - 1.0,
          { metalness: 0.8, roughness: 0.3 });
    }
  }
  {
    const p = part('mtdi', { name: 'MTDI wire (mic data)', step: STEP.xiao, explode: 8, refs: ['J_MTDI'],
      info: 'Solder it to the MTDI pad (GPIO41) on the XIAO\'s underside BEFORE mounting the XIAO, like the battery leads; it lands in J_MTDI next to the XIAO.' });
    const [tx, ty] = L.MTDI_PAD;
    p.wire([[L.XIAO_C[0] + 2, L.XIAO_C[1] - 4, Z.modTop + 0.6], [tx + 1, ty + 1.5, Z.modTop + 0.8], [tx, ty, Z.pcb - 1.2], [tx, ty, Z.pcb]], '#e0b040', 0.3);
  }
  {
    const p = part('antenna', { name: 'U.FL antenna (XIAO S3)', step: STEP.battery, explode: 8,
      info: ALU ? 'Flat antenna stuck to the underside of acrylic layer 1 beside the top of the speaker, where no copper or metal is above it: the aluminium shell blocks Bluetooth everywhere else. Test BLE MIDI range.'
        : 'Flat antenna stuck to the inside of a wall, cable to the U.FL socket on the XIAO. Keep it clear of the speaker magnet and the battery.' });
    if (ALU) {
      const ay = (L.SPK_C[1] + 14.1 + L.NOTCH[3]) / 2;   // between the speaker body and the top wall, under the acrylic
      p.box(20, 6, 0.3, '#b88a2a', 22, ay, Z.plate - 0.3, { metalness: 0.6 });
      p.wire([[12, ay, Z.plate - 0.5], [4.5, ay - 6, Z.plate - 3], [4.5, L.NOTCH[1] - 2, 12], [L.XIAO_C[0] + 6.5, L.XIAO_C[1] - 5.5, 11.6]], '#333', 0.3);
    } else {
      p.box(0.3, 20, 6, '#b88a2a', L.WALL + 0.3, 112, 6, { metalness: 0.6 });
      p.wire([[L.WALL + 0.6, 104, 8], [5, 98, 9], [L.XIAO_C[0] + 6.5, L.XIAO_C[1] - 5.5, 11.6]], '#333', 0.3);
    }
  }

  // ---- case screws: 4x M3x12 countersunk in the corners. The pot-strip position is a screwless support post.
  //      Aluminium + acrylic: black button heads, still corners only.
  if (ALU) {
    const support = new Set(L.SUPPORT_ONLY || []);
    const p = part('screws', { name: 'M3×12 button-head screws (corners)', qty: L.HOLES.length - support.size, step: STEP.close, explode: 120,
      info: 'In the four corners through acrylic layers 1 + 2, the spacers and the PCB, into the tapped M3 bosses - snug only, acrylic cracks if overtightened. No screw by the knobs.' });
    const MS = { metalness: 0.5, roughness: 0.4 };
    L.HOLES.forEach(([x, y], i) => {
      if (support.has(i)) return;                    // support post only
      const top = Z.plateTop, len = 12;
      const head = new THREE.SphereGeometry(2.85, 24, 8, 0, Math.PI * 2, 0, Math.PI / 2); head.scale(1, 1, 1.65 / 2.85); head.rotateX(Math.PI / 2);
      p.add(head, '#1b1b1d', x, y, top, MS);
      p.cylZ(1.45, len, '#1b1b1d', x, y, top - len, 12, MS);
    });
  } else {
    const support = new Set(L.SUPPORT_ONLY || []);
    const p = part('screws', { name: 'M3×12 countersunk screws (corners)', qty: L.HOLES.length - support.size, step: STEP.close, explode: 120,
      info: 'In the four corners, through the plate and PCB, self-tapping into the shell bosses - snug, not tight.' });
    const MS = { metalness: 0.6, roughness: 0.35 };
    L.HOLES.forEach(([x, y], i) => {
      if (support.has(i)) return;                    // no screw: boss + spacer only, plate solid on top
      const top = Z.plateTop, len = 12;
      const g = new THREE.CylinderGeometry(3.2, 1.6, 1.75, 24); g.rotateX(Math.PI / 2);
      p.add(g, SILVER, x, y, top - 0.875, MS);
      p.cylZ(1.45, len - 1.75, SILVER, x, y, top - len, 12, MS);
    });
  }

  // ---- keycaps, knobs, nuts
  {
    const p = part('keycaps', { name: 'Keycaps (16 pads + orange shift)', qty: 17, step: STEP.caps, explode: 100,
      info: '1u MX keycaps. Last thing on, after the case is closed.' });
    const g = keycapGeometry();
    [...keyXY, L.SHIFT].forEach(([x, y], i) => p.add(g, i === 16 ? '#e8792b' : '#ece9e1', x, y, Z.plateTop + 6.6 + 3.7, { roughness: 0.7 }));
  }
  {
    // 15 x 17 mm black anodised aluminium knob (set screw, 6 mm bore): knurled grip band at the base,
    // smooth upper body, black top face with a bright diamond-cut chamfer and pointer line.
    const p = part('knobs', { name: '15×17 mm aluminium knobs + pot nuts', qty: 4, step: STEP.caps, explode: 100,
      info: 'Nut and washer clamp the pot to the raised strip, then fit the 15×17 mm knobs and tighten the set screws.' });
    const KR = 7.5, KH = 17, KNURL_H = 13, CHAMF = 0.6;
    const BLACK = { metalness: 0.55, roughness: 0.38 }, BRIGHT = { metalness: 0.95, roughness: 0.2 };
    const knurl = new THREE.CylinderGeometry(KR, KR, KNURL_H, 160, 1);
    {
      const pos = knurl.attributes.position;
      for (let v = 0; v < pos.count; v++) {
        const x = pos.getX(v), z = pos.getZ(v), r = Math.hypot(x, z);
        if (r < KR * 0.9) continue;                          // cap centre vertices
        const k = 1 - 0.035 * (0.5 + 0.5 * Math.cos(80 * Math.atan2(z, x)));
        pos.setX(v, x * k); pos.setZ(v, z * k);
      }
      knurl.computeVertexNormals(); knurl.rotateX(Math.PI / 2);
    }
    const chamfer = new THREE.CylinderGeometry(KR - CHAMF, KR, CHAMF, 64, 1, true); chamfer.rotateX(Math.PI / 2);
    for (let i = 0; i < 4; i++) {
      const x = L.cx[i], z0 = Z.strip + 2;
      const nut = new THREE.CylinderGeometry(5.5, 5.5, 2, 6); nut.rotateX(Math.PI / 2);
      p.add(nut, '#9a9a9a', x, L.POT_Y, Z.strip + 1, { metalness: 0.85, roughness: 0.3 });
      p.add(knurl, '#1a1a1a', x, L.POT_Y, z0 + KNURL_H / 2, BLACK);
      p.cylZ(KR, KH - KNURL_H - CHAMF, '#161616', x, L.POT_Y, z0 + KNURL_H, 64, BLACK);
      p.add(chamfer, '#d6d6d6', x, L.POT_Y, z0 + KH - CHAMF / 2, BRIGHT);
      p.cylZ(KR - CHAMF, CHAMF, '#0c0c0c', x, L.POT_Y, z0 + KH - CHAMF, 64, { metalness: 0.3, roughness: 0.5 });
      p.box(0.45, 4.6, 0.05, '#d6d6d6', x, L.POT_Y + (KR - CHAMF) - 2.3, z0 + KH, BRIGHT);
    }
  }
  return { screenMat };
}

return { L, PCB, FP, Z, PADNAMES, parts, root, buildModel };
};
