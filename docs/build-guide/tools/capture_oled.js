// Capture the bring-up OLED page from the shared WASM core (sim/src/hugbox_core.js) as an upright
// 128x64 1-bit image for the 3D render. Run: node capture_oled.js
const path = require('path'), fs = require('fs');
const create = require(path.resolve(__dirname, '../../../sim/src/hugbox_core.js'));
create({}).then((M) => {
  M._hb_init(1, 1);
  M._hb_set_inputs((1 << 5) | (1 << 10), 0, 2600, 1500, 3000, 900, 1960);   // two pads held, some knob positions, 3.92 V
  for (let t = 0; t <= 3000; t += 5) M._hb_loop(t);
  const p = M._hb_panel(), ram = M.HEAPU8.subarray(p, p + 1024);
  const out = new Uint8Array(1024);   // row-major, 16 bytes per row, MSB = left pixel
  for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
    const px = 127 - x, py = 63 - y;                       // panel mounted upside down
    if ((ram[(py >> 3) * 128 + px] >> (py & 7)) & 1) out[y * 16 + (x >> 3)] |= 0x80 >> (x & 7);
  }
  fs.writeFileSync(path.join(__dirname, 'oled_frame.json'), JSON.stringify({ b64: Buffer.from(out).toString('base64') }));
  console.log('ok');
});
