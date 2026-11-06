// Headless check of mic sampling in the WASM core (the same C++ as the firmware).
// Usage: node sim/tools/sample_test.js [out-dir]   (out-dir: also writes OLED frames as .pbm)
// Opens SAMPLE (Shift + REC held 1 s), records tone bursts (a held pad, then hands-free), trims one with the START
// knob, saves them with SAVE TO PAD? (a recorded pad needs a second tap), leaves (an unsaved take is dropped, the rest
// written), then boots a fresh core on the saved samples. Exits 1 on a failed check.
const path = require('path');
const fs = require('fs');
const createHugboxCore = require(path.join(__dirname, '..', 'src', 'hugbox_core.js'));

const out = process.argv[2];
const store = new Map();          // what IndexedDB holds in the page
let saved = 0, fails = 0;
const logs = [];
const check = (ok, what) => { console.log((ok ? 'ok   ' : 'FAIL ') + what); if (!ok) fails++; };

async function boot() {
  const c = await createHugboxCore({ onLog: (s) => logs.push(s), userSamples: store, onSampleSaved: () => saved++ });
  c._hb_init(1, 1, 0);
  return c;
}

(async () => {
  const c = await boot();
  let t = 0, pads = 0, shift = 0, tone = 0, ph = 0, rendered = 0, outPeak = 0;
  const pots = [0, 4095, 0, 0];
  const mic = c._hb_mic_buf() >> 2;
  const push = () => c._hb_set_inputs(pads, shift, pots[0], pots[1], pots[2], pots[3], 1960);
  const run = (ms) => {
    for (let i = 0; i < ms; i += 1) {
      push(); c._hb_loop(t); t += 1;
      while (rendered * 128 / 44.1 < t) {              // keep audio in step with the clock, 128-frame blocks
        for (let k = 0; k < 128; k++) {                // 440 Hz at -30 dBFS (24-bit, a voice at arm's length on the INMP441) while `tone`
          c.HEAP32[mic + k] = tone ? Math.round(Math.sin(ph) * 0.0316 * 8388607) : 0;
          ph += 2 * Math.PI * 440 / 44100;
        }
        const p = c._hb_render(128) >> 1;
        for (let k = 0; k < 256; k++) outPeak = Math.max(outPeak, Math.abs(c.HEAP16[p + k]));
        rendered++;
      }
    }
  };
  const snap = (name) => {
    if (!out) return;
    const p = c._hb_panel(), ram = c.HEAPU8.subarray(p, p + 1024);
    let pbm = 'P1\n128 64\n';
    for (let y = 0; y < 64; y++) {
      const row = [];
      for (let x = 0; x < 128; x++) { const px = 127 - x, py = 63 - y; row.push((ram[(py >> 3) * 128 + px] >> (py & 7)) & 1); }
      pbm += row.join(' ') + '\n';
    }
    fs.writeFileSync(path.join(out, name + '.pbm'), pbm);
  };

  run(300);
  const tap = (pad, ms = 60) => { pads = 1 << pad; run(ms); pads = 0; run(60); };
  const shiftTap = (pad, ms = 60) => { shift = 1; run(20); tap(pad, ms); shift = 0; run(100); };
  const count = (pre) => logs.filter((l) => l.startsWith(pre)).length;

  // Shift + REC (pad 11) held 1 s opens SAMPLE on READY
  shift = 1; run(20); pads = 1 << 11; run(600);
  check(c._hb_view() === 0, 'Shift + REC held 0.6 s: not yet');
  run(500);
  check(c._hb_view() === 5, 'Shift + REC held 1 s opens SAMPLE');
  pads = 0; run(20); shift = 0; run(200);
  check(c._hb_mic_state() === 1, 'READY: mic listening, not recording');
  snap('oled_sample_ready');

  // hold any pad: the beep, then it records while held
  pads = 1 << 12; outPeak = 0; run(150);
  check(outPeak > 8000, `pad down: the beep (output peak ${outPeak})`);
  check(c._hb_mic_state() === 1, 'not recording during the beep');
  run(150);
  check(c._hb_mic_state() === 3, 'recording right after the beep');
  tone = 1; run(150); snap('oled_sample_recording'); run(150); tone = 0; run(200);
  pads = 0; outPeak = 0; run(400);
  check(c._hb_mic_state() === 1, 'let go: stopped');
  const sel = parseFloat((logs.find((l) => l.startsWith('take:')) || '').split('selection ')[1]);
  check(sel > 0.3 && sel < 0.45, `take trimmed to the tone: ${sel} s (0.30 + 5 ms + 50 ms)`);
  check(outPeak > 3000, `the trimmed take plays once (output peak ${outPeak})`);
  check(c._hb_user_frames(0) === 0, 'nothing is on a pad yet');
  snap('oled_sample_editor');

  // in the editor a tap only plays it
  outPeak = 0; tap(3); run(200);
  check(outPeak > 3000 && c._hb_mic_state() === 1 && count('take:') === 1, 'editor: a pad tap plays the sound, no new take');

  // START knob (knob 1) from 0 to the middle: pick-up, then the selection shrinks (and plays)
  for (let v = 0; v <= 2048; v += 64) { pots[0] = v; run(12); }
  run(300);
  snap('oled_sample_editor_start');

  // Shift + REC: SAVE TO PAD?; tap pad 12 (bottom-left, slot U01)
  shiftTap(11); snap('oled_sample_save');
  check(c._hb_user_frames(0) === 0, 'SAVE TO PAD? waits for a pad');
  tap(12); run(200);
  const f2 = c._hb_user_frames(0);
  check(f2 > 0 && f2 < sel * 44100, `saved onto the pad tapped, trimmed by START: ${(f2 / 44100).toFixed(3)} s`);
  check(count('sample saved to pad 13') === 1, 'save logged');
  check(c._hb_mic_state() === 1, 'back to READY');

  // tap a pad: hands free until the next tap
  tap(13); run(300);
  check(c._hb_mic_state() === 3, 'tapped: still recording with no pad held');
  tone = 1; run(200); tone = 0; run(100);
  tap(5); run(400);
  check(c._hb_mic_state() === 1 && count('take:') === 2, 'any pad tap stops it');
  // save onto the recorded pad 12: asks for a second tap
  shiftTap(11); tap(12); run(100);
  check(count('sample saved') === 1 && c._hb_user_frames(0) === f2, 'a recorded pad: the first tap only asks');
  tap(12); run(200);
  check(count('sample saved') === 2, 'second tap replaces it');
  const f3 = c._hb_user_frames(0);

  // a take that's never saved is dropped on leaving; the saved ones are written
  pads = 1 << 0; run(600); tone = 1; run(200); tone = 0; pads = 0; run(400);
  shiftTap(0);
  check(c._hb_view() === 0, 'Shift + PLAY leaves SAMPLE');
  check(count('unsaved sample dropped') === 1, 'the unsaved take is dropped');
  check(store.has(0) && store.get(0).length === f3 && store.size === 1, `saved on leaving (${saved} save call(s))`);

  // a fresh boot loads it
  const c2 = await boot();
  check(c2._hb_user_frames(0) === f3, 'reboot: the sample is back from storage');
  check(logs.some((l) => l.startsWith('user samples: 1 loaded')), 'boot log: ' + (logs.find((l) => l.startsWith('user samples')) || '').trim());

  console.log(fails ? `${fails} check(s) FAILED` : 'all checks passed');
  process.exit(fails ? 1 : 0);
})();
