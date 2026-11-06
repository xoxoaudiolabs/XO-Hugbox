// Headless output-level check of the WASM core (the same C++ as the firmware).
// Usage: node sim/tools/level_test.js
// Plays the synth (and sampler) in a range of settings and reports, per case, the output peak and how hard the
// output limiter works. The limiter is out = tanhish(in) (dsp.h), so `in` can be recovered from each output sample:
// in <= 0.5 is untouched, 0.5..2 is being rounded off, >= 2 is flat-topped (full scale).
const path = require('path');
const fs = require('fs');
const createHugboxCore = require(path.join(__dirname, '..', 'src', 'hugbox_core.js'));

// parameter ids = order of PARAMS[] in model.cpp
const src = fs.readFileSync(path.join(__dirname, '..', '..', 'firmware', 'lib', 'hugbox_core', 'model.cpp'), 'utf8');
const names = [...src.matchAll(/^\s*\{"[A-Z0-9]+", "([^"]+)",/gm)].map((m) => m[1]);
const P = (name) => { const i = names.indexOf(name); if (i < 0) throw new Error('no param ' + name); return i; };

const knee = 0.5;
const tanhish0 = (x) => (x >= 3 ? 1 : (x * (27 + x * x)) / (27 + 9 * x * x));
const tanhish = (a) => (a <= knee ? a : knee + (1 - knee) * tanhish0((a - knee) / (1 - knee)));   // dsp::limit
const unLimit = (o) => {                       // inverse of tanhish on 0..1
  if (o >= 0.99999) return 2;
  let lo = 0, hi = 2;
  for (let k = 0; k < 40; k++) { const m = (lo + hi) / 2; if (tanhish(m) < o) lo = m; else hi = m; }
  return (lo + hi) / 2;
};

async function boot() {
  const c = await createHugboxCore({ onLog: () => {}, userSamples: new Map(), onSampleSaved: () => {} });
  c._hb_init(1, 1, 0);
  return c;
}

async function measure(name, setup) {
  const c = await boot();
  let t = 0, pads = 0, shift = 0, rendered = 0, rec = null;
  const pots = [0, 0, 0, 0];
  const run = (ms) => {
    for (let i = 0; i < ms; i++) {
      c._hb_set_inputs(pads, shift, pots[0], pots[1], pots[2], pots[3], 1960);
      c._hb_loop(t); t++;
      while (rendered * 128 / 44.1 < t) {
        const p = c._hb_render(128) >> 1;
        if (rec) for (let k = 0; k < 256; k++) rec.push(c.HEAP16[p + k] / 32000);
        rendered++;
      }
    }
  };
  const io = {
    c, run, set: (n, v) => c._hb_set_param(P(n), v),
    hold: (list) => { pads = list.reduce((a, b) => a | (1 << b), 0); },
    release: () => { pads = 0; },
    shiftTap: (pad) => { shift = 1; run(20); pads = 1 << pad; run(60); pads = 0; run(40); shift = 0; run(100); },
    record: (on) => { rec = on ? [] : rec; },
  };
  run(300);
  io.shiftTap(9);                              // SYNTH track
  await setup(io);
  io.record(true);
  io.run(1500);
  io.release();
  io.run(500);
  const s = rec;
  let peak = 0, sq = 0, over = 0, flat = 0, inPeak = 0, xx = 0, xo = 0;
  const xs = new Float64Array(s.length);
  s.forEach((v, i) => {
    const a = Math.abs(v), x = Math.sign(v) * unLimit(Math.min(a, 1));
    xs[i] = x; xx += x * x; xo += x * v;
    peak = Math.max(peak, a); sq += v * v; inPeak = Math.max(inPeak, Math.abs(x));
    if (Math.abs(x) > knee) over++;
    if (Math.abs(x) >= 1.9) flat++;
  });
  const k = xo / xx;                            // best linear gain; what is left over is the limiter's distortion
  let err = 0;
  s.forEach((v, i) => { err += (v - k * xs[i]) ** 2; });
  const thd = 100 * Math.sqrt(err / sq);
  const db = (v) => (20 * Math.log10(Math.max(v, 1e-9))).toFixed(1).padStart(6);
  console.log(`${name.padEnd(44)} peak ${db(peak)} dBFS  rms ${db(Math.sqrt(sq / s.length))}  ` +
    `limiter in ${inPeak.toFixed(2).padStart(4)}x  squashed ${(100 * over / s.length).toFixed(1).padStart(5)} %  ` +
    `flat ${(100 * flat / s.length).toFixed(1).padStart(5)} %  limiter distortion ${thd.toFixed(1).padStart(4)} %`);
}

const NOTES = [0, 1, 2, 3, 4, 5];
process.setMaxListeners(0);
(async () => {
  console.log('Synth track, defaults unless noted (VOL 70, MVOL 70, CUT 60, RES 20, saw+square, MIX 30)\n');
  await measure('1 note', (io) => io.hold([0]));
  await measure('2 notes', (io) => io.hold([0, 4]));
  await measure('4 notes', (io) => io.hold([0, 2, 4, 6]));
  await measure('6 notes (all voices)', (io) => io.hold(NOTES));
  await measure('chord TRIAD, 1 pad', (io) => { io.set('Chord', 2); io.hold([0]); });
  await measure('chord 7TH, 2 pads (6 voices)', (io) => { io.set('Chord', 3); io.hold([0, 4]); });
  await measure('1 note, RES 100', (io) => { io.set('Resonance', 100); io.hold([0]); });
  await measure('4 notes, RES 100', (io) => { io.set('Resonance', 100); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, CUT 100, ENV 100', (io) => { io.set('Filter cutoff', 100); io.set('Filter env amount', 100); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, saw+saw unison MIX 50', (io) => { io.set('Osc 2 wave', 1); io.set('Osc 1/2 mix', 50); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, FM 100', (io) => { io.set('FM amount', 100); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, octave 2 (bass)', (io) => { io.set('Octave', 2); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, DRV 100', (io) => { io.set('Drive', 100); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, delay mix 100, feedback 95', (io) => { io.set('Delay mix', 100); io.set('Delay feedback', 95); io.hold([0, 2, 4, 6]); });
  await measure('4 notes, MVOL 100', (io) => { io.set('Master volume', 100); io.hold([0, 2, 4, 6]); });
  await measure('6 notes, VOL 100, MVOL 100, RES 100', (io) => { io.set('Synth volume', 100); io.set('Master volume', 100); io.set('Resonance', 100); io.hold(NOTES); });
  await measure('arp UP 1/16, 7TH chord, 1 pad', (io) => { io.set('Chord', 3); io.set('Arpeggiator', 1); io.hold([0]); });
  console.log('\nSampler track for comparison');
  await measure('sampler: kick + snare + hat', (io) => { io.shiftTap(8); io.hold([0, 1, 3]); });
})();
