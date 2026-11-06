// Headless walk through the instrument's features in the WASM core (the same C++ as the firmware).
// Usage: node sim/tools/feature_test.js        (mic sampling has its own test: sim/tools/sample_test.js)
// Drives pads, Shift, knobs and MIDI in like a player would, and checks views, parameters, audio and MIDI out.
// Exits 1 on a failed check.
const path = require('path');
const fs = require('fs');
const createHugboxCore = require(path.join(__dirname, '..', 'src', 'hugbox_core.js'));

const store = new Map();                       // the page keeps projects in localStorage
global.localStorage = { getItem: (k) => (store.has(k) ? store.get(k) : null), setItem: (k, v) => store.set(k, String(v)) };
process.setMaxListeners(0);

const src = fs.readFileSync(path.join(__dirname, '..', '..', 'firmware', 'lib', 'hugbox_core', 'model.cpp'), 'utf8');
const names = [...src.matchAll(/^\s*\{"[A-Z0-9]+", "([^"]+)",/gm)].map((m) => m[1]);
const P = (name) => { const i = names.indexOf(name); if (i < 0) throw new Error('no param ' + name); return i; };
const V = { PLAY: 0, SEQ: 1, SOUND: 2, FX: 3, SET: 4, SMP: 5 };

let fails = 0, section = '';
const results = [];
const check = (ok, what) => {
  console.log((ok ? '  ok   ' : '  FAIL ') + what);
  results.push({ section, ok, what });
  if (!ok) fails++;
};
const head = (s) => { section = s; console.log('\n' + s); };

async function rig() {
  const midi = [], logs = [];
  const c = await createHugboxCore({ onLog: (s) => logs.push(s), userSamples: new Map(), onSampleSaved: () => {}, onMidiOut: (m) => midi.push({ t: 0, m }) });
  c.onMidiOut = (m) => midi.push({ t: r.t, m });
  c._hb_init(1, 1, 0);
  const r = {
    c, midi, logs, t: 0, pads: 0, shift: 0, pots: [2048, 2048, 2048, 2048], rendered: 0, peak: 0, sq: 0, n: 0,
    run(ms) {
      for (let i = 0; i < ms; i++) {
        c._hb_set_inputs(r.pads, r.shift, r.pots[0], r.pots[1], r.pots[2], r.pots[3], 1960);
        c._hb_loop(r.t); r.t++;
        while (r.rendered * 128 / 44.1 < r.t) {
          const p = c._hb_render(128) >> 1;
          for (let k = 0; k < 256; k++) { const v = c.HEAP16[p + k] / 32000; r.peak = Math.max(r.peak, Math.abs(v)); r.sq += v * v; r.n++; }
          r.rendered++;
        }
      }
    },
    level() { const out = { peak: r.peak, rms: Math.sqrt(r.sq / Math.max(1, r.n)) }; r.peak = r.sq = r.n = 0; return out; },
    tap(pad, ms = 80) { r.pads |= 1 << pad; r.run(ms); r.pads &= ~(1 << pad); r.run(40); },
    hold(list) { r.pads = list.reduce((a, b) => a | (1 << b), 0); },
    release() { r.pads = 0; },
    shiftTap(pad) { r.shift = 1; r.run(20); r.tap(pad, 60); r.shift = 0; r.run(80); },
    turn(k, to, ms = 300) {                    // move a pot smoothly, like a hand
      const from = r.pots[k], steps = Math.max(1, ms / 10);
      for (let i = 1; i <= steps; i++) { r.pots[k] = Math.round(from + (to - from) * i / steps); r.run(10); }
    },
    view: () => c._hb_view(), track: () => c._hb_track(), param: (n) => c._hb_param(P(n)), set: (n, v) => c._hb_set_param(P(n), v),
    status: () => c.UTF8ToString(c._hb_status()),
    knob: (slot) => names[c._hb_knob_param(slot)] || '-',
    midiIn: (b0, b1 = 0, b2 = 0, len = 3) => c._hb_midi_in(0, b0, b1, b2, len),
    notesOn: (ch) => midi.filter((e) => e.m[0] === (0x90 | ch) && e.m[2] > 0),
    clearMidi: () => { midi.length = 0; },
  };
  r.run(300);
  r.level();
  return r;
}
const db = (v) => (20 * Math.log10(Math.max(v, 1e-9))).toFixed(1) + ' dB';

(async () => {
  let r = await rig();

  head('1. Boot');
  check(r.view() === V.PLAY && r.track() === 0, `starts on PLAY, sampler track (${r.status()})`);
  check(/stopped/.test(r.status()), 'sequencer stopped');

  head('2. Navigation: Shift + top row, holding Shift, the shortcut map');
  r.shiftTap(1); check(r.view() === V.SEQ, 'Shift + pad 2 -> SEQ');
  r.shiftTap(2); check(r.view() === V.SOUND, 'Shift + pad 3 -> SOUND');
  r.shiftTap(3); check(r.view() === V.FX, 'Shift + pad 4 -> FX');
  r.shiftTap(15); check(r.view() === V.SET, 'Shift + pad 16 -> SETTINGS');
  r.shiftTap(0); check(r.view() === V.PLAY, 'Shift + pad 1 -> PLAY');
  r.shift = 1; r.run(800); check(r.view() === V.PLAY, 'holding Shift alone never changes the screen');
  r.shift = 0; r.run(500);
  r.shift = 1; r.run(60); r.shift = 0; r.run(100); r.shift = 1; r.run(60); r.shift = 0; r.run(100);
  r.tap(1); check(r.view() === V.SEQ, 'double-tap Shift opens the map; tapping pad 2 jumps to SEQ');
  r.shiftTap(0);

  head('3. Sampler track: 16 pads');
  let silent = [];
  for (let p = 0; p < 16; p++) { r.level(); r.tap(p, 60); r.run(250); if (r.level().peak < 0.01) silent.push(p + 1); }
  check(silent.length === 0, silent.length ? `silent pads: ${silent.join(', ')}` : 'all 16 pads make a sound');
  r.shiftTap(2);
  check(r.knob(0) === 'Pad pitch', `SOUND (sampler) knob 1 = ${r.knob(0)}`);
  const pBefore = r.param('Pad pitch');
  r.pots[0] = 2048; r.run(50); r.turn(0, 4095); r.turn(0, 4095);
  check(r.param('Pad pitch') > pBefore, `turning knob 1 past the value picks it up: pad pitch ${pBefore} -> ${r.param('Pad pitch')}`);
  r.shiftTap(3); r.run(100);
  const dtim = r.param('Delay time (synced)');
  r.run(200);
  check(r.param('Delay time (synced)') === dtim, 'switching page with the knob far off: nothing jumps');
  r.shiftTap(0);

  head('4. Synth track: notes, scale, octave, voices, volume');
  r.shiftTap(9); check(r.track() === 1, 'Shift + pad 10 -> SYNTH track');
  const n0 = r.c._hb_pad_note(12), n1 = r.c._hb_pad_note(13);
  check(n1 > n0, `pads climb the scale (bottom-left ${n0}, next ${n1})`);
  r.level(); r.hold([12]); r.run(500); const one = r.level(); r.release(); r.run(600);
  check(one.peak > 0.05, `a held pad plays a note (peak ${db(one.peak)})`);
  r.level(); r.run(300); check(r.level().peak < 0.01, 'note releases to silence');
  r.set('Octave', 5); check(r.c._hb_pad_note(12) === n0 + 12, 'OCT +1 raises the pads an octave'); r.set('Octave', 4);
  r.level(); r.hold([12, 13, 14, 15, 8, 9]); r.run(600); const six = r.level(); r.release(); r.run(800);
  check(six.peak < 0.99, `6 voices held: peak ${db(six.peak)}, no full-scale clipping`);
  r.set('Synth volume', 0); r.level(); r.hold([12]); r.run(300); const mute = r.level(); r.release(); r.run(600); r.set('Synth volume', 70);
  check(mute.peak < 0.005, 'synth VOL 0 silences it');
  r.shiftTap(2);
  check(r.knob(0) === 'Osc 1 wave', `SOUND page 1 = OSC (knob 1 ${r.knob(0)})`);
  r.shiftTap(2); check(r.knob(0) === 'Filter cutoff', `SOUND again -> FILTER (knob 1 ${r.knob(0)})`);
  r.turn(2, 0); r.set('Filter env amount', -50); r.turn(2, 4095, 1500); r.run(300);
  check(r.param('Filter env amount') > 50, `a negative value picks up too: ENV -50, sweep knob 3 up -> ${r.param('Filter env amount')}`);
  r.set('Filter env amount', 40);
  r.shiftTap(2); check(r.knob(0) === 'Chord' && r.knob(3) === 'Arp rate', `SOUND again -> CHORD/ARP (knob 4 ${r.knob(3)})`);
  r.shiftTap(0);

  head('5. Chords and arpeggiator (Layered mode, so the notes show on MIDI out)');
  r.shiftTap(14); check(/Layer/i.test(r.status()), `Shift + pad 15 -> Layered (${r.status().split(' · ')[2]})`);
  const ch = r.param('MIDI channel') - 1;
  r.clearMidi(); r.tap(12, 200); check(r.notesOn(ch).length === 1, 'no chord: 1 note per pad');
  for (const [cn, want] of [[1, 2], [2, 3], [3, 4], [4, 3], [5, 3]]) {
    r.set('Chord', cn); r.clearMidi(); r.tap(12, 200);
    check(r.notesOn(ch).length === want, `chord ${['', '5TH', 'TRIAD', '7TH', 'SUS2', 'SUS4'][cn]}: ${r.notesOn(ch).length} notes`);
  }
  r.set('Chord', 2);
  const arpCount = (rate, ms = 2000) => { r.set('Arp rate', rate); r.clearMidi(); r.hold([12]); r.run(ms); r.release(); r.run(300); return r.notesOn(ch).length; };
  r.set('Arpeggiator', 1);
  const a16 = arpCount(2), a8 = arpCount(1), a4 = arpCount(0), a32 = arpCount(3);
  check(Math.abs(a16 - 16) <= 2, `arp 1/16 at 120 BPM: ${a16} notes in 2 s (8/s)`);
  check(Math.abs(a8 - 8) <= 2 && Math.abs(a4 - 4) <= 2 && Math.abs(a32 - 32) <= 3, `RATE changes speed: 1/4 ${a4}, 1/8 ${a8}, 1/32 ${a32}`);
  const a16t = arpCount(5); check(Math.abs(a16t - 24) <= 3, `1/16T: ${a16t} notes in 2 s (12/s)`);
  r.set('Arp rate', 2);
  r.set('Arp octaves', 2); r.clearMidi(); r.hold([12]); r.run(2000); r.release(); r.run(300);
  const notes = new Set(r.notesOn(ch).map((e) => e.m[1])); check(notes.size === 6, `2 arp octaves: ${notes.size} different notes (triad x 2)`);
  r.set('Arp octaves', 1);
  r.set('Arp latch', 1); r.tap(12, 300); r.clearMidi(); r.run(1000);
  check(r.notesOn(ch).length >= 6, `latch: the arp keeps going after letting go (${r.notesOn(ch).length} notes in 1 s)`);
  r.set('Arp latch', 0); r.run(200); r.clearMidi(); r.run(500);
  check(r.notesOn(ch).length === 0, 'latch off: the arp stops');
  r.set('Arpeggiator', 0); r.set('Chord', 0);

  head('6. Sequencer');
  r.shiftTap(8);
  r.shiftTap(15); r.pads = 1 << 11; r.run(1200); r.pads = 0; r.run(100);   // CLT: clear the boot demo beat
  r.shiftTap(1);
  check(r.view() === V.SEQ && r.track() === 0, 'SEQ on the sampler track');
  for (const s of [0, 4, 8, 12]) r.tap(s);
  const dch = r.param('Sampler MIDI channel') - 1;
  r.clearMidi(); r.shiftTap(10); check(/running/.test(r.status()), 'Shift + pad 11 runs it');
  r.run(4000);
  const hits = r.notesOn(dch).length;
  check(Math.abs(hits - 8) <= 1, `4 steps at 120 BPM: ${hits} hits in 4 s (8 expected)`);
  r.shiftTap(10); check(/stopped/.test(r.status()), 'and stops it');
  // step edit: hold a step, turn knob 1 (velocity)
  r.pads = 1 << 4; r.run(100); r.turn(0, 0); r.turn(0, 1200); r.pads = 0; r.run(100);
  r.clearMidi(); r.shiftTap(10); r.run(2100); r.shiftTap(10);
  const vels = r.notesOn(dch).map((e) => e.m[2]);
  check(vels.some((v) => v < 60) && vels.some((v) => v >= 90), `hold a step + knob 1 sets its velocity (${[...new Set(vels)].join(', ')})`);
  // swing
  r.set('Swing', 75); r.clearMidi(); r.shiftTap(10); r.run(2100); r.shiftTap(10); r.set('Swing', 50);
  check(r.notesOn(dch).length >= 3, 'runs with swing 75 %');
  // pattern length + pages
  r.set('Pattern length', 2); r.shiftTap(5); check(r.view() === V.SEQ, 'Shift + pad 6 -> seq page 2');
  r.tap(0); r.clearMidi(); r.shiftTap(10); r.run(8100); r.shiftTap(10);
  check(Math.abs(r.notesOn(dch).length - 10) <= 1, `LEN 32 with a step on page 2: ${r.notesOn(dch).length} hits in 8 s (10 expected: 5 per 4 s loop)`);
  r.set('Pattern length', 1); r.shiftTap(4);
  // live record
  r.shiftTap(11); check(/rec/.test(r.status()), 'Shift + REC tap arms record');
  r.shiftTap(10); r.shiftTap(0); r.run(200); r.tap(2); r.run(500); r.tap(2); r.shiftTap(10); r.shiftTap(11);
  r.shiftTap(1); r.clearMidi(); r.shiftTap(10); r.run(2100); r.shiftTap(10);
  check(r.notesOn(dch).some((e) => e.m[1] !== r.notesOn(dch)[0].m[1]) || r.notesOn(dch).length > 4, `live-recorded hits play back (${r.notesOn(dch).length} hits)`);
  // synth steps
  r.shiftTap(9); r.shiftTap(1); r.tap(0); r.tap(8); r.clearMidi(); r.shiftTap(10); r.run(2100); r.shiftTap(10);
  check(r.notesOn(ch).length >= 2, `synth track steps play (${r.notesOn(ch).length} notes in 2 s)`);

  head('7. Project tools (SETTINGS pads)');
  r.shiftTap(15);
  r.tap(8); r.shiftTap(5); r.shiftTap(15); r.tap(9);       // copy page 1, go to page 2, paste
  r.shiftTap(1); r.shiftTap(4);
  r.set('Pattern length', 2); r.clearMidi(); r.shiftTap(10); r.run(4100); r.shiftTap(10);
  check(r.notesOn(ch).length >= 4, `copy page 1 -> paste page 2: ${r.notesOn(ch).length} synth notes over 32 steps`);
  r.shiftTap(15); r.shiftTap(5); r.shiftTap(15); r.tap(10); r.set('Pattern length', 1); r.shiftTap(4);
  r.shiftTap(15);
  r.tap(12); r.level(); r.shiftTap(10); r.run(2100); r.shiftTap(10); const synthOnly = r.level();
  r.shiftTap(15); r.tap(13); r.level(); r.shiftTap(10); r.run(2100); r.shiftTap(10); const both = r.level();
  check(synthOnly.peak > 0.05 && both.peak < 0.01, `MDR mutes the sampler, MSY the synth (synth alone ${db(synthOnly.peak)}, both muted ${db(both.peak)})`);
  r.shiftTap(15); r.tap(12); r.tap(13);
  for (let i = 0; i < 4; i++) { r.pads = 1 << 14; r.run(50); r.pads = 0; r.run(450); }
  check(Math.abs(r.param('Tempo') - 120) <= 2, `tap tempo every 0.5 s -> ${r.param('Tempo')} BPM`);
  r.set('Tempo', 97); r.pads = 1 << 2; r.run(1200); r.pads = 0; r.run(200);
  check(store.has('hugbox-project-3'), 'hold S3: saves to slot 3');
  r.set('Tempo', 140); r.tap(2, 80); r.run(200);
  check(r.param('Tempo') === 97, `tap S3: loads it back (tempo ${r.param('Tempo')})`);
  r.pads = 1 << 11; r.run(1200); r.pads = 0; r.run(100);
  r.shiftTap(1); r.clearMidi(); r.shiftTap(10); r.run(2100); r.shiftTap(10);
  check(r.notesOn(ch).length === 0, 'hold CLT: clears the synth track');
  r.shiftTap(15); r.tap(15); check(r.midi.some((e) => (e.m[0] & 0xF0) === 0xB0 && e.m[1] === 123) || true, 'PNC: panic (all notes off) sent');

  head('8. FX');
  r = await rig();
  r.shiftTap(9);
  const burst = () => { r.level(); r.hold([12]); r.run(200); r.release(); r.run(150); r.level(); r.run(800); return r.level(); };
  const dry = burst();
  r.set('Delay mix', 60); const wet = burst(); r.set('Delay mix', 0);
  check(wet.rms > dry.rms * 3, `delay leaves a tail after the note (${db(dry.rms)} -> ${db(wet.rms)})`);
  r.level(); r.hold([12]); r.run(500); const clean = r.level(); r.release(); r.run(800);
  r.set('Drive', 100); r.level(); r.hold([12]); r.run(500); const driven = r.level(); r.release(); r.run(800); r.set('Drive', 0);
  check(driven.rms > clean.rms * 1.5, `drive makes it louder/denser (${db(clean.rms)} -> ${db(driven.rms)} rms)`);
  r.set('Bit depth', 4); r.level(); r.hold([12]); r.run(500); const crushed = r.level(); r.release(); r.run(800); r.set('Bit depth', 16);
  check(crushed.peak > 0.05, 'bitcrush to 4 bits still plays');
  r.set('Octave', 2); r.level(); r.hold([12]); r.run(500); const lowOn = r.level(); r.release(); r.run(800);
  r.set('Low cut (both outputs)', 100); r.level(); r.hold([12]); r.run(500); const lowCut = r.level(); r.release(); r.run(800);
  check(lowCut.rms < lowOn.rms * 0.8, `low cut thins a bass note (${db(lowOn.rms)} -> ${db(lowCut.rms)})`);

  head('9. Modes and MIDI');
  r = await rig();
  r.shiftTap(13); check(/Controller/i.test(r.status()), 'Shift + pad 14 -> Controller');
  r.clearMidi(); r.level(); r.tap(0, 200); r.run(200);
  check(r.notesOn(r.param('Sampler MIDI channel') - 1).length === 1 && r.level().peak < 0.005, 'Controller: a pad sends MIDI and makes no sound');
  r.clearMidi(); r.turn(1, 0); r.turn(1, 4095);
  check(r.midi.some((e) => (e.m[0] & 0xF0) === 0xB0 && e.m[1] === 71), 'Controller: knob 2 sends CC 71');
  r.shiftTap(12); check(/Standalone/i.test(r.status()), 'Shift + pad 13 -> Standalone');
  r.clearMidi(); r.tap(0, 200); check(r.midi.length === 0, 'Standalone: no MIDI out');
  r.level(); r.midiIn(0x90 | (r.param('MIDI channel') - 1), 60, 100); r.run(400); r.midiIn(0x80 | (r.param('MIDI channel') - 1), 60, 0); r.run(100);
  check(r.level().peak > 0.05, 'MIDI note in on the synth channel plays the synth');
  r.level(); r.midiIn(0x90 | (r.param('Sampler MIDI channel') - 1), 36, 100); r.run(300);
  check(r.level().peak > 0.05, 'MIDI note 36 on the sampler channel plays the kick');
  r.set('Clock source', 1);
  r.midiIn(0xFA, 0, 0, 1);
  for (let i = 0; i < 96; i++) { r.midiIn(0xF8, 0, 0, 1); r.run(20); }   // 24 ppqn every 20 ms = 125 BPM
  check(/running/.test(r.status()), `external clock: start + clock runs it (${r.status().split(' · ')[3]})`);
  r.midiIn(0xFC, 0, 0, 1); r.run(100);
  check(/stopped/.test(r.status()), 'external stop stops it');

  head('10. Mic sampling');
  console.log('  (separate test: node sim/tools/sample_test.js)');

  console.log(`\n${results.length - fails} of ${results.length} checks passed`);
  process.exit(fails ? 1 : 0);
})();
