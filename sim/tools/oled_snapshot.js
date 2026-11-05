// Drive the WASM core headless and dump OLED frames (as the player sees them) for docs.
// Usage: node sim/tools/oled_snapshot.js <out-dir>
// Writes <name>.pbm files (128x64, 1 = lit) for a few screens; tools/pbm_to_png.py turns them into PNGs.
const path = require('path');
const fs = require('fs');
const createHugboxCore = require(path.join(__dirname, '..', 'src', 'hugbox_core.js'));

(async () => {
  const out = process.argv[2] || '.';
  const c = await createHugboxCore({ onLog: () => {} });
  c._hb_init(1, 1, 0);
  let t = 0, pads = 0, shift = 0;
  const pots = [2048, 2048, 2048, 2048];
  const push = () => c._hb_set_inputs(pads, shift, pots[0], pots[1], pots[2], pots[3], 1960);
  const run = (ms) => { for (let i = 0; i < ms; i += 2) { push(); c._hb_loop(t); t += 2; if (i % 6 === 0) c._hb_render(128); } };
  const shortcut = (n) => { shift = 1; run(20); pads = 1 << n; run(30); pads = 0; run(20); shift = 0; run(40); };
  const snap = (name) => {
    run(900);                                        // let toasts/readouts clear, playhead move
    const p = c._hb_panel();
    const ram = c.HEAPU8.subarray(p, p + 1024);
    let pbm = 'P1\n128 64\n';
    for (let y = 0; y < 64; y++) {
      const row = [];
      for (let x = 0; x < 128; x++) {
        // module is mounted upside down: panel (x,y) shows at (127-x, 63-y)
        const px = 127 - x, py = 63 - y;
        row.push((ram[(py >> 3) * 128 + px] >> (py & 7)) & 1);
      }
      pbm += row.join(' ') + '\n';
    }
    fs.writeFileSync(path.join(out, name + '.pbm'), pbm);
    console.log('wrote', name);
  };

  run(200);
  snap('oled_play_drum');
  shortcut(1); shortcut(10); snap('oled_seq_running');      // SEQ, run
  shortcut(10);
  shortcut(9); shortcut(0);                                   // synth PLAY
  c._hb_set_param(19, 2); c._hb_set_param(21, 1);             // chord TRIAD, arp UP
  pads = 1 << 12; run(600); snap('oled_play_synth_arp'); pads = 0; run(100);
  shortcut(3); snap('oled_fx');
})();
