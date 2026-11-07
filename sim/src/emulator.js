// XO-Hugbox emulator - browser "platform layer".
// The app logic lives in C++ (sim/core, shared with the firmware) and runs as WASM (hugbox_core.js).
// This file only does what the board's platform code does: sample the controls, run loop(),
// stream audio blocks to the output, and show the SH1106 RAM on screen.
(() => {
  'use strict';
  const L = window.HUGBOX_LAYOUT;
  const SVGNS = 'http://www.w3.org/2000/svg';
  const $ = (id) => document.getElementById(id);
  const Y = (y) => L.H - y;   // plate y-up -> svg y-down

  // ---------------- hardware state (what the platform samples) ----------------
  const hw = {
    pads: 0,                       // bit n = pad n held, from mouse/touch
    keyPads: 0,                    // bit n = pad n held, from the keyboard
    shiftMouse: false, shiftKey: false,
    pots: [2600, 2048, 2048, 2048],   // raw ADC 0..4095
    vbat: 3.92,
    power: true,
    oledOk: true, pcfOk: true,
  };

  // ---------------- device drawing (top view, to scale, from hardware/layout.py) ----------------
  const svg = $('device');
  const M = 4;   // margin in mm for wall features
  svg.setAttribute('viewBox', `${-M} ${-M} ${L.W + 2 * M} ${L.H + 2 * M}`);
  const el = (tag, attrs, parent = svg) => {
    const e = document.createElementNS(SVGNS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    parent.appendChild(e);
    return e;
  };
  const defs = el('defs', {});
  defs.innerHTML = `
    <linearGradient id="plateG" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f6f3ea"/><stop offset="1" stop-color="#e2ddd0"/></linearGradient>
    <radialGradient id="knobG" cx=".4" cy=".35" r=".7"><stop offset="0" stop-color="#4a4a4a"/><stop offset="1" stop-color="#1c1c1c"/></radialGradient>
    <linearGradient id="capG" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fbfaf6"/><stop offset="1" stop-color="#dcd8cd"/></linearGradient>
    <linearGradient id="capS" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f39a55"/><stop offset="1" stop-color="#d0631c"/></linearGradient>
    <linearGradient id="acrG" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff" stop-opacity=".2"/><stop offset=".45" stop-color="#ffffff" stop-opacity=".05"/><stop offset="1" stop-color="#bfe3f5" stop-opacity=".12"/></linearGradient>`;

  // wall features: USB-C (XIAO) on the left wall, 3.5 mm jack (PCM5102A) on the right wall
  el('rect', { x: -1.6, y: Y(L.XIAO_C[1]) - 4.6, width: 2.2, height: 9.2, rx: 1, fill: '#3a3d42' });
  el('text', { x: -2.4, y: Y(L.XIAO_C[1]), 'font-size': 2.3, fill: '#8a8f96', 'text-anchor': 'middle',
    transform: `rotate(-90 -2.4 ${Y(L.XIAO_C[1])})` }).textContent = 'USB-C';
  el('rect', { x: L.W - 0.6, y: Y(L.JACK_Y) - 3, width: 2.2, height: 6, rx: 1, fill: '#3a3d42' });
  el('text', { x: L.W + 2.6, y: Y(L.JACK_Y), 'font-size': 2.3, fill: '#8a8f96', 'text-anchor': 'middle',
    transform: `rotate(90 ${L.W + 2.6} ${Y(L.JACK_Y)})` }).textContent = 'LINE';

  // Case: the 3D-printed plate (gPrinted) or the aluminium shell + clear acrylic faceplate (gAlu,
  // hardware/mechanical/alu_acrylic). Only one group shows; the Case menu in the Board card picks it.
  const gPrinted = el('g', {}), gAlu = el('g', {});
  const stripFront = L.POT_CAN_D / 2 + L.POT_LUG_L + 2.6;   // same as gen_scad.py: covers the pots' straight solder lugs
  const strip = { x: L.cx[0] - 10.4, y: Y(L.POT_Y + 10.4), width: L.W - 3 - (L.cx[0] - 10.4), height: 10.4 + stripFront, rx: 2 };
  const grille = (fn) => { for (let dx = -10; dx <= 10; dx += 4) for (let dy = -10; dy <= 10; dy += 4) if (dx * dx + dy * dy <= 100) fn(L.SPK_C[0] + dx, Y(L.SPK_C[1] + dy)); };
  const support = new Set(L.SUPPORT_ONLY || []);
  const [ox, oy] = L.OLED_C;
  const [px, py] = L.PWR;

  // ---- 3D-printed plate
  el('rect', { x: 0, y: 0, width: L.W, height: L.H, rx: 3, fill: 'url(#plateG)', stroke: '#bdb7a8', 'stroke-width': 0.3 }, gPrinted);
  el('rect', { ...strip, fill: '#ece7da', stroke: '#c9c2b2', 'stroke-width': 0.25 }, gPrinted);   // raised strip over the pots
  // screws (countersunk M3); SUPPORT_ONLY holes are hidden support posts under a solid plate
  for (const [i, [x, y]] of L.HOLES.entries()) {
    if (support.has(i)) continue;
    el('circle', { cx: x, cy: Y(y), r: 3.0, fill: '#b9bcc0', stroke: '#8d9095', 'stroke-width': 0.25 }, gPrinted);
    el('path', { d: `M${x - 1.6} ${Y(y)}h3.2M${x} ${Y(y) - 1.6}v3.2`, stroke: '#6f7277', 'stroke-width': 0.45 }, gPrinted);
  }
  grille((x, y) => el('circle', { cx: x, cy: y, r: 1, fill: '#2b2b2b' }, gPrinted));
  // OLED window in the plate (the glass and the active area are drawn on top)
  el('rect', { x: ox - L.OLED_WIN[0] / 2, y: Y(oy + L.OLED_WIN_OFF + L.OLED_WIN[1] / 2), width: L.OLED_WIN[0], height: L.OLED_WIN[1], rx: 0.6, fill: '#050608' }, gPrinted);
  el('rect', { x: px - 2.15, y: Y(py) - 1.05, width: 4.3, height: 2.1, rx: 0.4, fill: '#1a1a1a' }, gPrinted);   // power switch slot

  // ---- aluminium shell + clear acrylic: you see the board and the parts on its top side through the plate
  {
    const g = gAlu, I = L.INSET, [nx0, ny0, nx1] = L.NOTCH;
    el('rect', { x: 0, y: 0, width: L.W, height: L.H, rx: 3, fill: '#151517' }, g);                       // anodized shell
    const pcb = [[I, I], [L.W - I, I], [L.W - I, L.H - I], [nx1, L.H - I], [nx1, ny0], [nx0, ny0]];
    el('polygon', { points: pcb.map(([x, y]) => `${x},${Y(y)}`).join(' '), fill: '#1f6b3a' }, g);       // PCB (speaker notch cut out)
    for (const [x, y] of L.HOLES) el('circle', { cx: x, cy: Y(y), r: 3, fill: '#9da3a8' }, g);         // spacers
    const [sx, sy] = L.SPK_C;
    el('rect', { x: sx - 15.5, y: Y(sy + 14.1), width: 31, height: 28.2, rx: 1, fill: '#1d1d1d' }, g);   // speaker
    el('circle', { cx: sx, cy: Y(sy), r: 10.5, fill: '#2b2b2b' }, g);
    el('circle', { cx: sx, cy: Y(sy), r: 4, fill: '#3a3a3a' }, g);
    el('rect', { x: ox - L.OLED_MOD[0] / 2, y: Y(L.OLED_MOD_Y0 + L.OLED_MOD[1]), width: L.OLED_MOD[0], height: L.OLED_MOD[1], fill: '#1e5bd6' }, g);
    el('rect', { x: ox - L.OLED_GLASS[0] / 2, y: Y(L.OLED_GLASS_Y0 + L.OLED_GLASS[1]), width: L.OLED_GLASS[0], height: L.OLED_GLASS[1], fill: '#050608' }, g);
    el('circle', { cx: L.MIC_C[0], cy: Y(L.MIC_C[1]), r: L.MIC_D / 2, fill: '#1c1f26' }, g);              // INMP441 module
    el('rect', { x: px - 4.35, y: Y(py) - 1.85, width: 8.7, height: 3.7, fill: '#c8c8c8' }, g);            // 12D00G body
    el('rect', { x: px - 2.15, y: Y(py) - 1.05, width: 4.3, height: 2.1, rx: 0.3, fill: '#1a1a1a' }, g);
    for (let i = 0; i < 4; i++) el('circle', { cx: L.cx[i], cy: Y(L.POT_Y), r: L.POT_CAN_D / 2, fill: '#c8c8c8' }, g);   // pot cans
    // the acrylic: full sheet, knob strip, cut edges catching the light
    const edge = { stroke: '#d4eef9', 'stroke-opacity': 0.75, 'stroke-width': 0.35 };
    el('rect', { x: 0, y: 0, width: L.W, height: L.H, rx: 3, fill: 'url(#acrG)', ...edge, 'stroke-width': 0.6 }, g);
    el('rect', { ...strip, fill: 'url(#acrG)', ...edge }, g);
    el('rect', { x: ox - (L.OLED_GLASS[0] + 1) / 2, y: Y(oy + L.OLED_GLASS_OFF + (L.OLED_GLASS[1] + 1) / 2), width: L.OLED_GLASS[0] + 1, height: L.OLED_GLASS[1] + 1, fill: 'none', ...edge }, g);
    el('rect', { x: px - 4.7, y: Y(py) - 2.2, width: 9.4, height: 4.4, fill: 'none', ...edge }, g);
    grille((x, y) => el('circle', { cx: x, cy: y, r: 1, fill: '#0c0c0c', ...edge, 'stroke-width': 0.15 }, g));
    // black M3 button heads: four corners + the fifth through the knob strip
    for (const [x, y] of L.HOLES) {
      el('circle', { cx: x, cy: Y(y), r: 2.85, fill: '#1b1b1d', stroke: '#4a4a50', 'stroke-width': 0.3 }, g);
      el('polygon', { points: [0, 1, 2, 3, 4, 5].map((k) => `${x + Math.cos(k * Math.PI / 3)},${Y(y) + Math.sin(k * Math.PI / 3)}`).join(' '), fill: '#000' }, g);
    }
  }
  const setCase = (c) => { gPrinted.style.display = c === 'alu' ? 'none' : ''; gAlu.style.display = c === 'alu' ? '' : 'none'; };
  {
    let c = 'printed';
    try { c = localStorage.getItem('hugbox-sim-case') || 'printed'; } catch (e) {}
    $('caseSel').value = c; setCase(c);
    $('caseSel').addEventListener('change', (e) => {
      setCase(e.target.value);
      try { localStorage.setItem('hugbox-sim-case', e.target.value); } catch (err) {}
    });
  }
  // mic: the 1.5 mm sound hole over the INMP441, which sits label (port) side up in a pocket under the plate.
  // The ring and the MIC note are the emulator's (like the other grey notes); the printed plate only has the hole.
  const micG = el('g', {});
  el('title', {}, micG).textContent = 'INMP441 mic: sound hole. Shift + hold REC 1 s opens SAMPLE.';
  el('circle', { cx: L.MIC_C[0], cy: Y(L.MIC_C[1]), r: 2.2, fill: 'none', stroke: '#c9c2b2', 'stroke-width': 0.3, 'stroke-dasharray': '0.6 0.5' }, micG);
  el('circle', { cx: L.MIC_C[0], cy: Y(L.MIC_C[1]), r: L.MIC_HOLE / 2, fill: '#2b2b2b' }, micG);
  el('text', { x: L.MIC_C[0], y: Y(L.MIC_C[1]) + 4.8, 'font-size': 2.2, fill: '#7d7a72', 'text-anchor': 'middle' }, micG).textContent = 'MIC';

  // OLED: window in the plate, glass, then the 128x64 active area (29.42 x 14.7 mm on a 1.3" SH1106)
  const AW = 29.42, AH = 14.7;
  const fo = el('foreignObject', { x: ox - AW / 2, y: Y(oy + L.OLED_WIN_OFF + AH / 2), width: AW, height: AH });
  const oledCanvas = document.createElement('canvas');
  oledCanvas.width = 128; oledCanvas.height = 64;
  oledCanvas.style.cssText = 'width:100%;height:100%;display:block;image-rendering:pixelated;';
  fo.appendChild(oledCanvas);
  const octx = oledCanvas.getContext('2d');
  const zctx = $('oledZoom').getContext('2d');
  zctx.imageSmoothingEnabled = false;
  const mirrorZoom = () => zctx.drawImage(oledCanvas, 0, 0, 512, 256);

  // power slide switch (12D00G), actuator through the plate
  const pwrKnob = el('rect', { x: px - 0.75, y: Y(py) - 0.75, width: 1.5, height: 1.5, fill: '#d9d9d9', style: 'cursor:pointer' });
  const pwrHit = el('rect', { x: px - 5, y: Y(py) - 3, width: 10, height: 6, fill: 'transparent', style: 'cursor:pointer' });
  const pwrLabel = el('text', { x: px, y: Y(py) - 2.4, 'font-size': 2.2, fill: '#7d7a72', 'text-anchor': 'middle' });

  // knobs
  const knobs = [];
  for (let i = 0; i < 4; i++) {
    const g = el('g', { style: 'cursor:ns-resize' });
    const x = L.cx[i], y = Y(L.POT_Y);
    el('circle', { cx: x, cy: y, r: 5.5, fill: '#9a9a9a' }, g);           // nut
    // 15 x 17 mm black aluminium knob, seen from above: bright chamfer ring, black face, bright pointer line
    el('circle', { cx: x, cy: y, r: 7.5, fill: '#cfcfcf', stroke: '#111', 'stroke-width': 0.3 }, g);
    el('circle', { cx: x, cy: y, r: 6.9, fill: 'url(#knobG)' }, g);
    const ind = el('line', { x1: x, y1: y - 2.2, x2: x, y2: y - 6.9, stroke: '#dcdcdc', 'stroke-width': 0.5 }, g);
    const t = el('title', {}, g);
    // scale ticks
    for (let a = -150; a <= 150; a += 30) {
      const r = (a - 90) * Math.PI / 180;
      el('line', { x1: x + Math.cos(r) * 8.1, y1: y + Math.sin(r) * 8.1, x2: x + Math.cos(r) * 9.0, y2: y + Math.sin(r) * 9.0,
        stroke: '#a49d8c', 'stroke-width': 0.3 }, g);
    }
    knobs.push({ g, ind, t, x, y });
  }

  // keys: 16 pads (row 0 = top) + shift
  const KEYS = ['Digit1', 'Digit2', 'Digit3', 'Digit4', 'KeyQ', 'KeyW', 'KeyE', 'KeyR',
                'KeyA', 'KeyS', 'KeyD', 'KeyF', 'KeyZ', 'KeyX', 'KeyC', 'KeyV'];
  const KEYLABEL = ['1', '2', '3', '4', 'Q', 'W', 'E', 'R', 'A', 'S', 'D', 'F', 'Z', 'X', 'C', 'V'];
  const NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const noteName = (n) => NOTE[n % 12] + (4 + Math.floor(n / 12));
  const keyEls = [];
  const makeKey = (x, y, shift, label, sub) => {
    const g = el('g', { style: 'cursor:pointer' });
    el('rect', { x: x - 7.5, y: Y(y) - 7.5, width: 15, height: 15, fill: '#2a2a2a' }, g);         // switch hole shadow
    const cap = el('rect', { x: x - 9, y: Y(y) - 9, width: 18, height: 18, rx: 1.6,
      fill: shift ? 'url(#capS)' : 'url(#capG)', stroke: shift ? '#a04b13' : '#bcb6a6', 'stroke-width': 0.3 }, g);
    const top = el('rect', { x: x - 6.6, y: Y(y) - 7.4, width: 13.2, height: 13.2, rx: 1.4,
      fill: shift ? '#f0874a' : '#f7f5ef', opacity: 0.85 }, g);
    const labEl = el('text', { x: x - 5.4, y: Y(y) - 3.8, 'font-size': 2.6, fill: shift ? '#3a1b05' : '#8b8577', 'font-family': 'monospace',
                                class: 'kbdlabel' }, g);   // the computer key for it (hidden on phones)
    labEl.textContent = label;
    const subEl = el('text', { x: x + 5.6, y: Y(y) + 4.6, 'font-size': 2.4, fill: shift ? '#3a1b05' : '#b0a998', 'text-anchor': 'end' }, g);
    subEl.textContent = sub;
    return { g, cap, top, sub: subEl, lab: labEl };
  };
  for (let n = 0; n < 16; n++) {
    const r = n >> 2, c = n & 3;
    const k = makeKey(L.cx[c], L.cy[3 - r], false, KEYLABEL[n], noteName(n));
    k.n = n; keyEls.push(k);
  }
  const shiftEl = makeKey(L.SHIFT[0], L.SHIFT[1], true, '⇧', 'SHIFT');
  // firmware picker: 0 = main UI (sim/core/main_app), 1 = bring-up (port of firmware/src/bringup/main.cpp)
  const fw = () => +$('fwSel').value;
  // shift legends, as they would be printed on the keycap fronts
  const SHIFT_LEGEND = ['PLAY', 'SEQ', 'SOUND', 'FX', 'PG 1', 'PG 2', 'PG 3', 'PG 4',
                        'SMPL', 'SYNTH', 'RUN', 'REC/SMP', 'STD', 'CTL', 'LAYER', 'SET'];
  function setLegends() {
    const main = fw() === 0;
    keyEls.forEach((k) => {
      k.sub.textContent = main ? SHIFT_LEGEND[k.n] : noteName(k.n);
      k.sub.setAttribute('fill', main ? '#d0662a' : '#b0a998');
    });
    const km = $('keymap');
    km.innerHTML = '';
    for (let n = 0; n < 16; n++) {
      const sp = document.createElement('span');
      sp.innerHTML = `<b>${KEYLABEL[n]}</b> pad ${n} ${main ? '⇧ ' + SHIFT_LEGEND[n] : noteName(n)}`;
      km.appendChild(sp);
    }
    $('fwNote').style.display = main ? '' : 'none';
    $('bringNote').style.display = main ? 'none' : '';
  }

  // ---------------- phones: just the Hugbox, full screen ----------------
  const phoneMq = matchMedia('(max-width: 640px), (pointer: coarse) and (max-width: 940px)');
  const isPhone = () => document.body.classList.contains('phone');
  const setPhone = () => {
    document.body.classList.toggle('phone', phoneMq.matches);
    $('startBtn').textContent = phoneMq.matches ? 'Play' : 'Power on';
  };
  setPhone();
  phoneMq.addEventListener('change', setPhone);
  $('moreBtn').addEventListener('click', () => {
    const on = document.body.classList.toggle('showall');
    $('moreBtn').textContent = on ? 'Just the Hugbox' : 'Emulator panels';
    if (on) $('side').scrollIntoView({ behavior: 'smooth' }); else window.scrollTo({ top: 0, behavior: 'smooth' });
  });
  const buzz = () => { if (isPhone() && navigator.vibrate) navigator.vibrate(8); };   // a key click you can feel (Android)

  // ---------------- input handling ----------------
  const pointerPad = new Map();   // pointerId -> pad index (or 'shift')
  const setPointer = (id, what) => {
    pointerPad.set(id, what);
    recomputePointer();
  };
  const recomputePointer = () => {
    let bits = 0, sh = false;
    for (const v of pointerPad.values()) { if (v === 'shift') sh = true; else bits |= 1 << v; }
    hw.pads = bits; hw.shiftMouse = sh;
    paintKeys();
  };
  keyEls.forEach((k) => {
    k.g.addEventListener('pointerdown', (e) => { e.preventDefault(); k.g.setPointerCapture(e.pointerId); setPointer(e.pointerId, k.n); buzz(); });
  });
  shiftEl.g.addEventListener('pointerdown', (e) => { e.preventDefault(); shiftEl.g.setPointerCapture(e.pointerId); setPointer(e.pointerId, 'shift'); buzz(); });
  const release = (e) => { if (pointerPad.delete(e.pointerId)) recomputePointer(); };
  svg.addEventListener('pointerup', release);
  svg.addEventListener('pointercancel', release);

  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' && e.target.type !== 'checkbox' && e.target.type !== 'range') return;
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') { hw.shiftKey = true; paintKeys(); return; }
    const n = KEYS.indexOf(e.code);
    if (n >= 0 && !e.ctrlKey && !e.metaKey && !e.altKey) { e.preventDefault(); hw.keyPads |= 1 << n; paintKeys(); }
  });
  window.addEventListener('keyup', (e) => {
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') { hw.shiftKey = false; paintKeys(); return; }
    const n = KEYS.indexOf(e.code);
    if (n >= 0) { hw.keyPads &= ~(1 << n); paintKeys(); }
  });
  window.addEventListener('blur', () => { hw.keyPads = 0; hw.shiftKey = false; paintKeys(); });

  const padBits = () => hw.pads | hw.keyPads;
  const shiftHeld = () => hw.shiftMouse || hw.shiftKey;
  // Synth notes on the pads: keycaps take piano colours (sharps dark, naturals light) so you can see the
  // black and white keys. Emulator-only aid; the real keycaps don't change.
  let padNotes = new Array(16).fill(-1), padNotesKey = '';
  const isBlack = (n) => n >= 0 && [1, 3, 6, 8, 10].includes(n % 12);
  function updatePadNotes() {
    const notes = [];
    for (let n = 0; n < 16; n++) notes.push(core && hw.power && fw() === 0 ? core._hb_pad_note(n) : -1);
    const key = notes.join(',');
    if (key === padNotesKey) return;
    padNotesKey = key;
    padNotes = notes;
    paintKeys();
  }
  // ---------------- MIDI: monitor + optional WebMIDI ----------------
  const NOTE_N = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const nn = (n) => NOTE_N[n % 12] + (Math.floor(n / 12) - 1);
  const midiLines = [];
  let clocksOut = 0, clocksIn = 0, midiOutPort = null, midiAccess = null;
  function describe(m) {
    const s = m[0], ch = (s & 15) + 1;
    switch (s & 0xf0) {
      case 0x90: return m[2] ? `note on  ch${ch} ${nn(m[1])} (${m[1]}) vel ${m[2]}` : `note off ch${ch} ${nn(m[1])} (${m[1]})`;
      case 0x80: return `note off ch${ch} ${nn(m[1])} (${m[1]})`;
      case 0xb0: return `CC ${m[1]} = ${m[2]}  ch${ch}`;
    }
    return { 0xfa: 'START', 0xfb: 'CONTINUE', 0xfc: 'STOP' }[s] || m.map((x) => x.toString(16)).join(' ');
  }
  function midiLog(dir, m) {
    if (m[0] === 0xf8) { if (dir === 'out') clocksOut++; else clocksIn++; return; }   // clocks: counted, not listed
    midiLines.push(`${dir === 'out' ? '→' : '←'} ${describe(m)}`);
    if (midiLines.length > 60) midiLines.splice(0, midiLines.length - 60);
    const el = $('midiLog');
    el.textContent = midiLines.join('\n');
    el.scrollTop = el.scrollHeight;
  }
  function midiFromDevice(m) {                 // the Hugbox sends
    midiLog('out', m);
    if (midiOutPort) try { midiOutPort.send(m); } catch (e) {}
  }
  function midiToDevice(m) {                   // into the Hugbox (as USB)
    if (!core || !hw.power) return;
    midiLog('in', m);
    core._hb_midi_in(0, m[0], m[1] || 0, m[2] || 0, m.length);
  }
  setInterval(() => { $('midiClock').textContent = `clock out ${clocksOut} · in ${clocksIn}`; }, 250);
  $('midiTestNote').addEventListener('click', () => { midiToDevice([0x90, 60, 100]); setTimeout(() => midiToDevice([0x80, 60, 0]), 400); });
  $('midiTestDrum').addEventListener('click', () => { midiToDevice([0x99, 38, 110]); setTimeout(() => midiToDevice([0x89, 38, 0]), 100); });
  function fillPorts() {
    const fill = (sel, ports, none) => {
      const cur = sel.value;
      sel.innerHTML = `<option value="">${none}</option>` +
        [...ports.values()].map((p) => `<option value="${p.id}">${(p.name || p.id).replace(/</g, '&lt;')}</option>`).join('');
      sel.value = [...ports.keys()].includes(cur) ? cur : '';
    };
    fill($('midiOutSel'), midiAccess.outputs, '(monitor only)');
    fill($('midiInSel'), midiAccess.inputs, '(none)');
  }
  $('midiConnect').addEventListener('click', async () => {
    if (!navigator.requestMIDIAccess) { $('midiState').textContent = 'this browser has no Web MIDI (try Chrome or Edge)'; return; }
    try {
      midiAccess = await navigator.requestMIDIAccess();
      midiAccess.onstatechange = fillPorts;
      fillPorts();
      $('midiState').textContent = `${midiAccess.outputs.size} out · ${midiAccess.inputs.size} in`;
    } catch (e) { $('midiState').textContent = 'MIDI access was not allowed'; }
  });
  $('midiOutSel').addEventListener('change', () => { midiOutPort = midiAccess && midiAccess.outputs.get($('midiOutSel').value) || null; });
  let midiInPort = null;
  $('midiInSel').addEventListener('change', () => {
    if (midiInPort) midiInPort.onmidimessage = null;
    midiInPort = midiAccess && midiAccess.inputs.get($('midiInSel').value) || null;
    if (midiInPort) midiInPort.onmidimessage = (ev) => midiToDevice([...ev.data].slice(0, 3));
  });

  // ---------------- user samples (IndexedDB: localStorage is too small for audio) ----------------
  // The core reads Module.userSamples (slot -> Int16Array) synchronously, like the board's flash, so they are all
  // loaded before it starts; saves come back through onSampleSaved and go to IndexedDB in the background.
  const userSamples = new Map();
  let idb = null;
  const samplesReady = new Promise((resolve) => {
    try {
      const rq = indexedDB.open('hugbox', 1);
      rq.onupgradeneeded = () => rq.result.createObjectStore('samples');
      rq.onerror = () => resolve();
      rq.onsuccess = () => {
        idb = rq.result;
        const cur = idb.transaction('samples', 'readonly').objectStore('samples').openCursor();
        cur.onsuccess = () => {
          const c = cur.result;
          if (c) { userSamples.set(+c.key, new Int16Array(c.value)); c.continue(); } else resolve();
        };
        cur.onerror = () => resolve();
      };
    } catch (e) { resolve(); }
  });
  function sampleSaved(slot, a) {
    if (a) userSamples.set(slot, a); else userSamples.delete(slot);
    try {
      if (idb) { const st = idb.transaction('samples', 'readwrite').objectStore('samples'); if (a) st.put(a.buffer.slice(0), slot); else st.delete(slot); }
    } catch (e) { log('[emulator] could not store the sample in this browser\n'); }
    sampleState();
  }
  function sampleState() {
    let n = 0, f = 0;
    for (const a of userSamples.values()) { n++; f += a.length; }
    $('smpState').textContent = (n ? `${n} recorded pad${n > 1 ? 's' : ''} · ${(f / 44100).toFixed(1)} of 52 s used` : 'no recorded samples yet') +
      (idb ? '' : ' · this page can’t store them');
  }
  $('smpClear').addEventListener('click', () => {
    if (!confirm('Delete every recorded sample in this browser? The pads go back to the built-in kit.')) return;
    userSamples.clear();
    try { if (idb) idb.transaction('samples', 'readwrite').objectStore('samples').clear(); } catch (e) {}
    sampleState();
    if (hw.power) boot();
  });
  samplesReady.then(sampleState);

  // ---------------- mic (the computer's, standing in for the INMP441) ----------------
  // -20 dB first: a laptop mic after the browser is much hotter than the INMP441 (-26 dBFS at 94 dB SPL), so the
  // Hugbox's GAIN knob behaves about the same here as on the board.
  const MIC_SCALE = 0.1;
  let micOn = false, micStream = null, micSrc = null, micAsked = false;
  function micText() {
    const st = core && hw.power && fw() === 0 ? core._hb_mic_state() : 0;
    const what = ['', 'SAMPLE view open', 'waiting for sound (THRS)', '● recording'][st];
    // on a phone there is no panel to find the mic button in: SAMPLE turns the phone's mic on (asked once)
    if (st && isPhone() && !micOn && !micAsked) { micAsked = true; $('micBtn').click(); }
    $('micState').textContent = (micOn ? 'mic on' : 'mic off: takes record silence') + (what ? ' · ' + what : '');
  }
  $('micBtn').addEventListener('click', async () => {
    if (micOn) {
      micOn = false;
      if (micSrc) micSrc.disconnect();
      if (micStream) micStream.getTracks().forEach((t) => t.stop());
      micSrc = micStream = null;
      $('micBtn').textContent = 'Use mic';
      return;
    }
    start();
    try {
      micStream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    } catch (e) { $('micState').textContent = 'mic access was not allowed'; return; }
    micSrc = ac.createMediaStreamSource(micStream);
    micSrc.connect(sp);
    micOn = true;
    $('micBtn').textContent = 'Stop mic';
    log('[emulator] mic on: Shift + hold F (REC) 1 s opens SAMPLE; then tap or hold any pad to record\n');
  });

  // ---------------- projects (localStorage) ----------------
  function projectState() {
    let n = 0, auto = false;
    try { localStorage.getItem('hugbox-project-0'); } catch (e) {
      $('projState').textContent = 'this page can’t store data here (no origin) - open sim/index.html directly in Chrome/Edge to keep projects';
      return;
    }
    try { for (let s = 0; s <= 8; s++) if (localStorage.getItem('hugbox-project-' + s)) { if (s) n++; else auto = true; } } catch (e) {}
    $('projState').textContent = `${auto ? 'autosave on file' : 'no autosave yet'} · ${n} of 8 slots used`;
  }
  function projectSaved() { projectState(); }
  $('projClear').addEventListener('click', () => {
    if (!confirm('Delete the autosave and all 8 saved slots in this browser?')) return;
    try { for (let s = 0; s <= 8; s++) localStorage.removeItem('hugbox-project-' + s); } catch (e) {}
    projectState();
    if (hw.power) boot();
  });
  projectState();

  // ---------------- guide: live knob map ----------------
  const VIEW_NAMES = ['PLAY', 'SEQ', 'SOUND', 'FX', 'SETTINGS', 'SAMPLE'];
  let guideKey = '', guideT = 0;
  function updateGuide() {
    const now = performance.now();
    if (now - guideT < 150) return;
    guideT = now;
    const table = $('gKnobs'), tag = $('gViewTag');
    if (!core || !hw.power || fw() !== 0) {
      if (guideKey !== 'off') {
        guideKey = 'off'; tag.textContent = '';
        table.innerHTML = `<tr><td colspan="3" class="note">${fw() === 0 ? 'Power on to see the live knob map.' : 'Shown for the Main UI firmware.'}</td></tr>`;
      }
      return;
    }
    const s = (p) => core.UTF8ToString(p);
    const rows = [];
    for (let slot = 0; slot < 8; slot++) {
      const id = core._hb_knob_param(slot);
      if (id === 255) continue;
      rows.push([slot, s(core._hb_param_label(id)), s(core._hb_param_name(id)), s(core._hb_param_text(id))]);
    }
    const view = core._hb_view(), track = core._hb_track();
    const label = `${VIEW_NAMES[view] || ''} · ${track === 0 ? 'sampler' : 'synth'}`;
    const key = label + JSON.stringify(rows);
    if (key === guideKey) return;
    guideKey = key;
    tag.textContent = label;
    table.innerHTML = rows.map(([slot, lab, name, val]) =>
      `<tr class="${slot >= 4 ? 'shiftrow' : ''}"><td>${slot >= 4 ? 'Shift + ' : ''}knob ${(slot % 4) + 1}</td>` +
      `<td><span class="lbl">${lab}</span> ${name}</td><td class="v">${val}</td></tr>`).join('');
  }

  function paintKeys() {
    const b = padBits();
    keyEls.forEach((k) => {
      const on = (b >> k.n) & 1;
      const black = isBlack(padNotes[k.n]);
      k.top.setAttribute('fill', black ? (on ? '#4a4a4f' : '#26262a') : (on ? '#d9d3c3' : '#f7f5ef'));
      k.cap.setAttribute('fill', black ? '#151517' : 'url(#capG)');
      k.lab.setAttribute('fill', black ? '#c9c6bd' : '#8b8577');
      k.sub.setAttribute('fill', fw() === 0 ? (black ? '#f0874a' : '#d0662a') : (black ? '#c9c6bd' : '#b0a998'));
      k.g.setAttribute('transform', on ? `translate(0 0.5)` : '');
    });
    const s = shiftHeld();
    shiftEl.top.setAttribute('fill', s ? '#c75a16' : '#f0874a');
    shiftEl.g.setAttribute('transform', s ? 'translate(0 0.5)' : '');
  }

  // knobs: drag (up or right = higher) or scroll; 300 degree travel, clockwise = higher (as wired on the PCB)
  // Scrolling follows how far it moves, not how many events arrive: a trackpad sends dozens of tiny ones per swipe
  // (plus momentum), a mouse wheel one ~100 px event per notch (= 1/32 of the travel).
  // Up = higher, as the fingers or wheel move. Macs scroll "naturally" by default (content follows the fingers, so
  // fingers up = deltaY > 0): Safari says so (webkitDirectionInvertedFromDevice); elsewhere on a Mac assume it.
  const isMac = /Mac/.test(navigator.platform || navigator.userAgent);
  const setPot = (i, v) => { hw.pots[i] = Math.max(0, Math.min(4095, Math.round(v))); paintKnobs(); };
  knobs.forEach((k, i) => {
    let drag = null, acc = 0;
    k.g.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      k.g.setPointerCapture(e.pointerId);
      drag = { x: e.clientX, y: e.clientY, v: hw.pots[i] };
    });
    k.g.addEventListener('pointermove', (e) => {
      if (!drag) return;
      const fine = e.shiftKey ? 0.25 : 1;
      setPot(i, drag.v + ((drag.y - e.clientY) + (e.clientX - drag.x)) * 18 * fine);
    });
    const end = () => { drag = null; };
    k.g.addEventListener('pointerup', end);
    k.g.addEventListener('pointercancel', end);
    k.g.addEventListener('wheel', (e) => {
      e.preventDefault();                         // no page scroll, no pinch zoom over a knob
      if (e.ctrlKey) return;                      // pinch: not a turn
      const unit = e.deltaMode === 1 ? 33 : e.deltaMode === 2 ? 400 : 1;   // lines / pages -> px
      const natural = e.webkitDirectionInvertedFromDevice !== undefined ? e.webkitDirectionInvertedFromDevice : isMac;
      // vertical swipe, or a sideways one (right = higher)
      const d = Math.abs(e.deltaY) >= Math.abs(e.deltaX) ? (natural ? e.deltaY : -e.deltaY) : e.deltaX;
      acc += d * unit * 1.28 * (e.shiftKey ? 0.25 : 1);
      const step = Math.trunc(acc);
      if (!step) return;
      acc -= step;
      setPot(i, hw.pots[i] + step);
    }, { passive: false });
  });
  function paintKnobs() {
    knobs.forEach((k, i) => {
      const a = -150 + 300 * hw.pots[i] / 4095;
      k.ind.setAttribute('transform', `rotate(${a} ${k.x} ${k.y})`);
      k.t.textContent = `POT${i} (D${i}) raw ${hw.pots[i]}` + (i === 0 ? ' - volume' : '');
    });
  }

  pwrHit.addEventListener('click', () => setPower(!hw.power));

  // ---------------- WASM core ----------------
  let core = null, bootT0 = 0;
  const serial = $('serial');
  let serialBuf = '';
  const log = (s) => {
    serialBuf += s;
    if (serialBuf.length > 20000) serialBuf = serialBuf.slice(-15000);
    serial.textContent = serialBuf;
    serial.scrollTop = serial.scrollHeight;
  };

  async function boot() {
    // a reset re-instantiates the module: fresh statics, exactly like a power cycle
    core = null;
    await samplesReady;
    const c = await createHugboxCore({ onLog: log, onMidiOut: midiFromDevice, onProjectSaved: projectSaved,
      userSamples, onSampleSaved: sampleSaved });
    bootT0 = performance.now();
    hw.oledOk = !$('noOled').checked;
    hw.pcfOk = !$('noPcf').checked;
    // setup() output that lives in the firmware's platform code
    log(fw() === 0 ? '\n=== XO-Hugbox ===\n' : '\n=== XO-Hugbox bring-up ===\n');
    log('Chip ESP32-S3 rev 0, flash 8 MB, PSRAM 8192 KB   (emulated values)\n');
    log('I2C scan:\n');
    const found = [];
    if (hw.pcfOk) found.push(0x20);
    if (hw.oledOk) found.push(0x3C);
    for (const a of found) log('  0x' + a.toString(16).toUpperCase().padStart(2, '0') + '\n');
    if (!found.length) log('  nothing found - check SDA/SCL wires to the PCF I2C pads and OLED\n');
    log(`  OLED 0x3C: ${hw.oledOk ? 'OK' : 'MISSING'}   PCF8574 0x20: ${hw.pcfOk ? 'OK' : 'MISSING'}\n`);
    log('I2S running at 44.1 kHz\n');
    c._hb_init(hw.oledOk ? 1 : 0, hw.pcfOk ? 1 : 0, fw());
    core = c;
    if (!hw.oledOk) clearOled();
  }

  function pushInputs() {
    if (!core) return;
    const vsenseMv = Math.round(hw.vbat * 1000 / 2);   // 100k/100k divider
    core._hb_set_inputs(padBits(), shiftHeld() ? 1 : 0, hw.pots[0], hw.pots[1], hw.pots[2], hw.pots[3], vsenseMv);
  }

  // control loop: the firmware loop() spins freely; 2 ms here is plenty for its 5 ms / 100 ms timers
  setInterval(() => {
    if (!core || !hw.power) return;
    pushInputs();
    core._hb_loop((performance.now() - bootT0) >>> 0);
  }, 2);

  // ---------------- OLED ----------------
  let lastFrame = -1;
  const img = octx.createImageData(128, 64);
  const ON = [232, 244, 255], OFF = [6, 8, 12];
  function clearOled() {
    octx.fillStyle = 'rgb(6,8,12)'; octx.fillRect(0, 0, 128, 64); lastFrame = -1; mirrorZoom();
  }
  function drawOled() {
    if (!core || !hw.power || !hw.oledOk) return;
    const f = core._hb_frames();
    if (f === lastFrame && !drawOled.force) return;
    lastFrame = f; drawOled.force = false;
    const p = core._hb_panel();
    const ram = core.HEAPU8.subarray(p, p + 1024);
    const raw = $('rawPanel').checked;
    // brightness setting -> panel contrast; OLED contrast dims the lit pixels, it never lifts black
    const b = 0.35 + 0.65 * core._hb_brightness() / 100;
    const ONb = [ON[0] * b, ON[1] * b, ON[2] * b];
    for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
      // SH1106 RAM: page = y/8, bit = y%8, column = x (panel orientation).
      // The module is mounted upside down (header toward the knobs), so panel pixel (x, y)
      // appears at (127-x, 63-y) to the player. U8G2_R2 pre-rotates so the result reads upright.
      const on = (ram[(y >> 3) * 128 + x] >> (y & 7)) & 1;
      const sx = raw ? x : 127 - x, sy = raw ? y : 63 - y;
      const i = (sy * 128 + sx) * 4, c = on ? ONb : OFF;
      img.data[i] = c[0]; img.data[i + 1] = c[1]; img.data[i + 2] = c[2]; img.data[i + 3] = 255;
    }
    octx.putImageData(img, 0, 0);
    mirrorZoom();
  }
  $('rawPanel').addEventListener('change', () => { drawOled.force = true; });

  // ---------------- audio ----------------
  let audioBlocks = 0;
  let ac = null, sp = null, lineOut, spkIn, spkOut, master, analyser;
  function startAudio() {
    if (ac) return;
    try { ac = new AudioContext({ sampleRate: 44100 }); } catch (e) { ac = new AudioContext(); }
    // ScriptProcessor keeps everything on one thread with the WASM instance (works from file://).
    // 1024 frames = 8 firmware blocks of 128; latency ~23 ms on top of the browser's own.
    sp = ac.createScriptProcessor(1024, 1, 2);   // input: the mic, when it is on
    sp.onaudioprocess = (ev) => {
      audioBlocks++;
      const L0 = ev.outputBuffer.getChannelData(0), R0 = ev.outputBuffer.getChannelData(1);
      if (!core || !hw.power) { L0.fill(0); R0.fill(0); return; }
      const mp = core._hb_mic_buf() >> 2, H32 = core.HEAP32;   // mic block first, as the board's audio task
      if (micOn) {
        const inp = ev.inputBuffer.getChannelData(0);
        for (let i = 0; i < 1024; i++) H32[mp + i] = Math.round(Math.max(-1, Math.min(1, inp[i] * MIC_SCALE)) * 8388607);
      } else H32.fill(0, mp, mp + 1024);
      const p = core._hb_render(1024) >> 1;
      const h = core.HEAP16;
      for (let i = 0; i < 1024; i++) { L0[i] = h[p + 2 * i] / 32768; R0[i] = h[p + 2 * i + 1] / 32768; }
    };
    master = ac.createGain();
    lineOut = ac.createGain();
    // rough small-speaker model: the 28 x 31 mm box has little below ~350 Hz and rolls off above ~9 kHz
    spkIn = ac.createGain();
    const hp = ac.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 350; hp.Q.value = 0.9;
    const pk = ac.createBiquadFilter(); pk.type = 'peaking'; pk.frequency.value = 2500; pk.gain.value = 4; pk.Q.value = 1;
    const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 9000;
    spkIn.connect(hp); hp.connect(pk); pk.connect(lp); spkOut = lp;
    analyser = ac.createAnalyser(); analyser.fftSize = 2048;
    sp.connect(analyser);
    lineOut.connect(master); spkOut.connect(master);
    master.connect(ac.destination);
    routeAudio();
    setMonitor();
    if (sinkSel.value) applySink();
    if (ac.sampleRate !== 44100) log(`[emulator] audio device runs at ${ac.sampleRate} Hz; pitch is resampled by the browser\n`);
  }
  function routeAudio() {
    if (!ac) return;
    try { sp.disconnect(lineOut); } catch (e) {}
    try { sp.disconnect(spkIn); } catch (e) {}
    sp.connect($('outSel').value === 'spk' ? spkIn : lineOut);
  }
  function setMonitor() { if (master) master.gain.value = +$('mon').value; }
  $('outSel').addEventListener('change', routeAudio);
  $('mon').addEventListener('input', setMonitor);

  // scope + status
  const scope = $('scope'), sctx = scope.getContext('2d');
  const sbuf = new Float32Array(2048);
  function drawScope() {
    sctx.fillStyle = '#141414'; sctx.fillRect(0, 0, scope.width, scope.height);
    sctx.strokeStyle = '#2a2a28'; sctx.beginPath(); sctx.moveTo(0, 35); sctx.lineTo(scope.width, 35); sctx.stroke();
    if (!analyser) return;
    analyser.getFloatTimeDomainData(sbuf);
    let s = 0; while (s < 1000 && !(sbuf[s] <= 0 && sbuf[s + 1] > 0)) s++;   // trigger on rising zero crossing
    sctx.strokeStyle = '#f2f0eb'; sctx.lineWidth = 1.5; sctx.beginPath();
    for (let x = 0; x < scope.width; x++) {
      const v = sbuf[s + Math.floor(x * 1.6)] || 0;
      const y = 35 - v * 60;
      x ? sctx.lineTo(x, y) : sctx.moveTo(x, y);
    }
    sctx.stroke();
  }
  function drawStatus() {
    if (!core || !hw.power) { $('toneStat').textContent = hw.power ? 'booting' : 'power off'; return; }
    if (fw() === 0) {
      $('toneStat').textContent = core.UTF8ToString(core._hb_status()) + (ac ? '' : ' · audio not started');
      const ms0 = (performance.now() - bootT0) >>> 0;
      $('led').classList.toggle('on', !((ms0 / 500) & 1));
      return;
    }
    const hz = core._hb_tone_hz(), g = core._hb_tone_gain();
    if (hz > 0) {
      const b = padBits(), n = 31 - Math.clz32(b & -b);
      const what = hz === 1000 ? '1 kHz reference (shift)' : `pad ${n} = ${noteName(n)}`;
      $('toneStat').textContent = `${what} · ${hz.toFixed(2)} Hz · gain ${g.toFixed(3)}` + (ac ? '' : ' · audio not started');
    } else {
      $('toneStat').textContent = `silent · gain ${g.toFixed(3)} (pot 0)`;
    }
    const ms = (performance.now() - bootT0) >>> 0;
    $('led').classList.toggle('on', !((ms / 500) & 1));   // digitalWrite(LED, (now/500)&1), LED active low
  }
  function drawAudioStat() {
    $('audioStat').textContent = ac
      ? `audio ${ac.state} · ${ac.sampleRate} Hz · ${audioBlocks} blocks rendered`
      : 'audio not started - click Power on';
  }
  // plain WebAudio beep straight to the speakers: tells "browser/output problem" apart from "emulator problem"
  $('beep').addEventListener('click', () => {
    startAudio(); ac.resume();
    const o = ac.createOscillator(), g = ac.createGain();
    o.frequency.value = 660; g.gain.setValueAtTime(0.25, ac.currentTime); g.gain.exponentialRampToValueAtTime(0.001, ac.currentTime + 0.5);
    o.connect(g); g.connect(ac.destination); o.start(); o.stop(ac.currentTime + 0.5);
  });
  // output device picker: browsers play to the Windows default device unless told otherwise
  const sinkSel = $('sink');
  async function listSinks() {
    if (!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices) return;
    const outs = (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === 'audiooutput' && d.deviceId !== 'default');
    let saved = ''; try { saved = localStorage.getItem('hugbox-sink-label') || ''; } catch (e) {}
    const cur = sinkSel.value;
    sinkSel.innerHTML = '<option value="">System default</option>' + outs.map((d, i) =>
      `<option value="${d.deviceId}">${(d.label || 'Output ' + (i + 1)).replace(/</g, '&lt;')}</option>`).join('');
    const match = outs.find((d) => d.deviceId === cur) || outs.find((d) => d.label && d.label === saved);
    if (match) { sinkSel.value = match.deviceId; applySink(); }
  }
  async function applySink() {
    if (!ac || !ac.setSinkId) return;
    try {
      await ac.setSinkId(sinkSel.value);
      const opt = sinkSel.selectedOptions[0];
      try { localStorage.setItem('hugbox-sink-label', sinkSel.value ? opt.textContent : ''); } catch (e) {}
      log(`[emulator] audio output -> ${opt.textContent}\n`);
    } catch (e) { log(`[emulator] could not switch output: ${e.message}\n`); }
  }
  sinkSel.addEventListener('change', () => { startAudio(); applySink(); });
  $('sinkNames').addEventListener('click', async () => {
    try {
      const st = await navigator.mediaDevices.getUserMedia({ audio: true });   // only to unlock device names
      st.getTracks().forEach((t) => t.stop());
    } catch (e) { log('[emulator] device names need microphone permission (nothing is recorded)\n'); }
    listSinks();
  });
  listSinks();
  // browsers can suspend audio until a gesture; resume on any interaction
  const wake = () => { if (ac && ac.state !== 'running') ac.resume(); };
  window.addEventListener('pointerdown', wake, true);
  window.addEventListener('keydown', wake, true);
  function frame() {
    drawAudioStat();
    drawOled(); drawScope(); drawStatus(); updatePadNotes(); updateGuide(); micText();
    requestAnimationFrame(frame);
  }

  // ---------------- board controls ----------------
  function setBat() { hw.vbat = +$('bat').value; $('batV').textContent = hw.vbat.toFixed(2) + ' V'; }
  $('bat').addEventListener('input', setBat);
  $('reboot').addEventListener('click', () => { if (hw.power) boot(); });
  $('noOled').addEventListener('change', () => { if (hw.power) boot(); });
  $('noPcf').addEventListener('change', () => { if (hw.power) boot(); });
  function setPower(on) {
    hw.power = on;
    pwrKnob.setAttribute('x', L.PWR[0] - 0.75 + (on ? 1.2 : -1.2));
    pwrLabel.textContent = on ? 'POWER  ON' : 'POWER  OFF';
    if (on) { boot(); } else { core = null; clearOled(); log('\n[power off]\n'); }
  }

  $('fwSel').addEventListener('change', () => { setLegends(); if (hw.power) boot(); });
  setLegends();

  // ---------------- start ----------------
  clearOled(); paintKeys(); paintKnobs(); setBat();
  pwrKnob.setAttribute('x', L.PWR[0] - 0.75 + 1.2); pwrLabel.textContent = 'POWER  ON';
  boot();
  requestAnimationFrame(frame);
  const start = () => { startAudio(); if (ac.state === 'suspended') ac.resume(); $('start').style.display = 'none'; };
  $('startBtn').addEventListener('click', start);
  $('start').addEventListener('click', (e) => { if (e.target === $('start')) start(); });

  // ---------------- installable web app ----------------
  // sw.js caches the page for offline use; only over https or on this computer (not from file://).
  if ('serviceWorker' in navigator && (location.protocol === 'https:' || /^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname)))
    window.addEventListener('load', () => navigator.serviceWorker.register('sw.js').catch(() => {}));
  const installed = matchMedia('(display-mode: standalone), (display-mode: fullscreen)').matches || navigator.standalone === true;
  let installEvt = null;
  window.addEventListener('beforeinstallprompt', (e) => {   // Chrome, Edge, Android: our own Install button
    e.preventDefault();
    installEvt = e;
    $('installRow').hidden = $('installBtn').hidden = false;
  });
  $('installBtn').addEventListener('click', async () => {
    if (!installEvt) return;
    installEvt.prompt();
    await installEvt.userChoice;
    installEvt = null;
    $('installRow').hidden = true;
  });
  window.addEventListener('appinstalled', () => { $('installRow').hidden = true; });
  const ios = /iPhone|iPad|iPod/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  if (ios && !installed) $('installRow').hidden = $('iosNote').hidden = false;   // Safari has no install prompt

  window.hugboxEmu = { hw, get core() { return core; }, get ac() { return ac; }, get audioBlocks() { return audioBlocks; }, startAudio: start };   // for debugging from the console
})();
