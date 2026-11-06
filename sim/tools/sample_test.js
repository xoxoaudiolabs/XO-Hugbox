// Headless check of mic sampling in the WASM core (the same C++ as the firmware).
// Usage: node sim/tools/sample_test.js [out-dir]   (out-dir: also writes OLED frames as .pbm)
// Holds Shift + REC 2 s (beep, then a take of a tone burst onto the selected pad), checks the take, trims it with the START knob,
// leaves the view (save), then boots a fresh core on the saved samples. Exits 1 on a failed check.
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
  // Shift down, REC (pad 11) down, Shift let go, REC held: at 2 s a beep, then the take runs until REC is let go
  shift = 1; run(20); pads = 1 << 11; run(100); shift = 0; run(1700);
  check(c._hb_view() === 0, 'REC held 1.8 s: still in PLAY, no beep yet');
  outPeak = 0; run(300);
  check(c._hb_view() === 5, 'REC held 2 s opens SAMPLE');
  check(outPeak > 8000, `the beep plays (output peak ${outPeak})`);
  check(c._hb_mic_state() === 1, 'not recording during the beep');
  snap('oled_sample_beep');
  run(200);
  check(c._hb_mic_state() === 3, 'recording starts right after the beep');
  tone = 1; run(150); snap('oled_sample_recording'); run(150); tone = 0; run(200);
  pads = 0; outPeak = 0; run(400);
  const f = c._hb_user_frames(0);
  check(f > 0.3 * 44100 && f < 0.45 * 44100, `take trimmed to the tone: ${(f / 44100).toFixed(3)} s (0.30 + 5 ms + 50 ms)`);
  check(outPeak > 3000, `the take plays back once (output peak ${outPeak})`);
  check(logs.some((l) => l.startsWith('take:')), 'take logged: ' + (logs.find((l) => l.startsWith('take:')) || '').trim());
  snap('oled_sample_editor');

  // in SAMPLE, a pad held past 250 ms beeps too; let go during the beep and nothing is recorded
  pads = 1 << 13; run(300); pads = 0; run(300);
  check(c._hb_user_frames(1) === 0 && c._hb_mic_state() === 1, 'pad let go during the beep: nothing recorded');
  pads = 1 << 12; run(50); pads = 0; run(100);    // back to pad 12 (voice 0) for the editor checks

  // START knob (knob 0) from 0 to the middle: pick-up, then the selection shrinks
  for (let v = 0; v <= 2048; v += 64) { pots[0] = v; run(12); }
  run(300);
  const f2 = c._hb_user_frames(0);
  check(f2 > 0 && f2 < f, `START trims it: ${(f2 / 44100).toFixed(3)} s`);
  snap('oled_sample_editor_start');

  // leave with Shift + PLAY: saved to the store
  shift = 1; run(20); pads = 1 << 0; run(30); pads = 0; run(20); shift = 0; run(300);
  check(c._hb_view() === 0, 'Shift + PLAY leaves SAMPLE');
  check(store.has(0) && store.get(0).length === f2, `saved on leaving (${saved} save call(s))`);

  // a fresh boot loads it, and pad 0 plays it
  const c2 = await boot();
  check(c2._hb_user_frames(0) === f2, 'reboot: the sample is back from storage');
  check(logs.some((l) => l.startsWith('user samples: 1 loaded')), 'boot log: ' + (logs.find((l) => l.startsWith('user samples')) || '').trim());

  console.log(fails ? `${fails} check(s) FAILED` : 'all checks passed');
  process.exit(fails ? 1 : 0);
})();
