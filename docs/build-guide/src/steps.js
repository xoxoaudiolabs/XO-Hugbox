// XO-Hugbox build guide content: parts list and steps.
// Order follows the assembly order in docs/hardware.md. Two steps explain why parts go in earlier or later
// than you might expect (pots/OLED/power switch before the plate; PCM5102A after the switch pins are clipped).
window.HUGBOX_GUIDE = (() => {
  const STEP = { diodes: 1, xiao: 2, max: 3, pcf: 4, switches: 5, underplate: 6, plate: 7, pcm: 8,
    speaker: 9, battery: 10, close: 11, caps: 12, poweron: 13 };

  // camera presets: [position], [target] in plate mm (z up)
  const V = {
    hero: [[150, -95, 150], [46, 84, 18]],
    bottom: [[60, -70, -150], [46, 80, 14]],
    keysBack: [[50, -25, -115], [46, 46, 14]],
    xiao: [[-5, 35, -70], [16, 88, 12]],
    max: [[38, 30, -75], [34, 90, 12]],
    pcf: [[75, 30, -80], [66, 94, 12]],
    pcm: [[125, 105, -55], [78, 148, 13]],
    plateAbove: [[150, -75, 140], [46, 84, 45]],
    underplate: [[95, 40, 115], [50, 128, 20]],
    plate: [[150, -40, 120], [46, 90, 20]],
    shellTop: [[40, 85, 70], [20, 138, 8]],
    battery: [[115, -35, 105], [35, 55, 6]],
    close: [[150, -80, 130], [46, 90, 18]],
    caps: [[60, -125, 135], [46, 80, 22]],
    screen: [[53, 95, 115], [53, 140, 24]],
  };
  const BOARD = ['pcb', 'diodes', 'resistors', 'xiao', 'max', 'pcf', 'pcm', 'switches', 'spacers', 'pots', 'oled', 'power', 'plate'];

  const BOM = [
    ['XO-Hugbox v1 PCB', 1, '2-layer 1.6 mm, ordered 5 from PCBWay'],
    ['Top plate (3D-printed)', 1, 'xo_hugbox_top_plate.stl, layer height dividing 1.5 mm'],
    ['Bottom shell (3D-printed)', 1, 'xo_hugbox_shell.stl'],
    ['Seeed XIAO ESP32-S3', 1, 'must be S3; comes with the U.FL antenna'],
    ['PCF8574T I/O expander module', 1, 'from a 5-pack; socket removed, address bridged to 0x20'],
    ['MAX98357A I2S amp module', 1, 'generic 7-pin (LRC BCLK DIN GAIN SD GND VIN)'],
    ['PCM5102A I2S DAC module (purple)', 1, 'jumpers FLT L, DEMP L, XSMT H, FMT L'],
    ['1.3" SH1106 I2C OLED 128×64', 1, '35.4×33.5 mm, pins GND VCC SCK SDA'],
    ['WH148 B10K 16 mm pot (straight lugs)', 4, 'with nut + washer; 12 diode lead offcuts as wire legs'],
    ['15×17 mm black aluminium knob, 6 mm set screw', 4, 'silver chamfer + pointer line'],
    ['MX switch', 17, '16 pads + shift'],
    ['1u MX keycap', 17, '16 + one for shift (orange in the render)'],
    ['1N4148 diode', 16, 'one per pad'],
    ['100 kΩ resistor', 2, 'battery divider R1, R2'],
    ['12D00G-5mm slide switch', 1, 'power'],
    ['3 W 8 Ω box speaker 28.2×31×15.3 mm', 1, 'with two screw ears'],
    ['LP102540 1100 mAh LiPo (with protection)', 1, ''],
    ['M3×12 countersunk screw', 4, 'corners only - no screw by the knobs'],
    ['1 mm spacer (PCB to plate)', 5, 'qty assumed: 4 corners + the support post by the knobs'],
    ['M2.5×6 self-tapping screw', 2, 'speaker ears'],
    ['Hook-up wire', '-', 'XIAO BAT leads (2), PCF I2C (4), speaker leads (2)'],
  ];

  const parts = (ids) => ids;
  const steps = [
    {
      title: 'Overview and parts', view: V.hero, explode: 0.55, all: true, focus: [],
      body: `
        <p>XO-Hugbox is a 4×4-pad MIDI controller and standalone synth. One 2-layer PCB carries everything:
        keys, pots, OLED and power switch on the <b>top</b>; the XIAO ESP32-S3, PCF8574T, MAX98357A and PCM5102A
        modules plus the diodes and divider on the <b>back</b>. A 3D-printed plate holds the switches, a printed shell
        holds the speaker and battery.</p>
        <p>Drag to orbit, scroll to zoom, right-drag to pan. Hover any part for its name, click it for details and pins.
        Use the <b>Explode</b> slider, or step through the build with <b>Next</b> (or the → key).</p>
        <h3>Parts list</h3>
        <table class="bom"><tr><th>Part</th><th>Qty</th><th>Notes</th></tr>
        ${BOM.map(([a, q, n]) => `<tr><td>${a}</td><td class="q">${q}</td><td>${n}</td></tr>`).join('')}</table>
        <div class="callout fix"><b>Two things to know about the order.</b> The pots, OLED and power switch sit <i>under</i> the plate,
        so they go in before the plate (step 6). The shift and power switch pins are <i>under</i> the PCM5102A, so that module goes on
        after them (step 8).</div>`,
    },
    {
      title: 'Diodes and divider resistors (back)', view: V.keysBack, focus: ['diodes', 'resistors'],
      body: `
        <p>Start with the lowest parts. Both go on the <b>back</b> of the board (the side with the module outlines).</p>
        <ol>
          <li><b>16× 1N4148</b> at D1–D16, one under each pad. Bend the leads to 7.62 mm.</li>
          <li><b>2× 100 kΩ</b> at R1 and R2 (brown-black-yellow), the battery-sense divider into D9.</li>
          <li>Solder on the top side and clip the leads <b>flush</b> - the plate sits only 1 mm above the top.
          <b>Keep 12 of the clipped leads</b>: they become the pot wire legs in step 6.</li>
        </ol>
        <div class="callout warn"><b>Diode polarity:</b> the black band (cathode) goes to the <b>square pad</b>.
        The square pad is the row line (R0–R3); the round pad goes to the switch. A reversed diode makes that pad dead.</div>`,
    },
    {
      title: 'XIAO ESP32-S3 (back)', view: V.xiao, focus: ['xiao'],
      body: `
        <div class="callout warn"><b>Battery leads first.</b> Before the XIAO goes anywhere, solder two short wires (red +, black −, ~4 cm)
        to the <b>BAT+ / BAT− pads</b> on the XIAO. Once it is on the board those pads face the PCB and you cannot reach them.
        They go to the <b>J13 "+ −, to XIAO BAT pads"</b> pair just below the XIAO (switched battery, after the power switch).</div>
        <ol>
          <li>Must be the <b>ESP32-S3</b> (native USB MIDI and 8 MB PSRAM), not the C3 or plain ESP32.</li>
          <li>Component side (shield can) faces the <b>floor</b>, USB-C toward the <b>left wall</b>.</li>
          <li>Use the header strips with their plastic between the boards (2.5 mm). Pins D0–D6 are the row nearest the pots,
          5V/GND/3V3/D10–D7 the row nearest the keys - match the silk.</li>
          <li>Solder the BAT leads to J13 (+ to +). Keep the U.FL antenna for step 10.</li>
        </ol>`,
    },
    {
      title: 'MAX98357A amp (back)', view: V.max, focus: ['max'],
      body: `
        <ol>
          <li>7-pin header <b>LRC BCLK DIN GAIN SD GND VIN</b>, pushed in from the module's <b>plain side</b>,
          so the <b>printed side faces the floor</b>.</li>
          <li><b>GAIN floating</b> = 9 dB. <b>SD</b> left at its default (on, L+R mono). Nothing to wire on those two pins.</li>
          <li>Solder ~8 cm speaker leads to the module's <b>+ / −</b> now; the speaker sits in the top-left corner of the shell.</li>
        </ol>`,
    },
    {
      title: 'PCF8574T key-matrix expander (back)', view: V.pcf, focus: ['pcf'],
      body: `
        <p>Prepare the module first:</p>
        <ol>
          <li><b>Desolder the pass-through socket</b> (the female header at the output end).</li>
          <li><b>Replace the A0–A2 jumper caps with solder bridges</b>, all to the low/GND side → I2C address <b>0x20</b>,
          which is what <code>pins.h</code> expects.</li>
          <li>P0–P7 header into the board, module upside down: <b>P0 pin on the P0 pad</b> ("PCF8574: P0 on P0" on the silk).
          P0–P3 are rows R0–R3, P4–P7 columns C0–C3.</li>
          <li>The module's right-angle I2C pins go to the 2×2 <b>"PCF I2C"</b> pads (V G / D C) with <b>four short wires</b>:
          VCC→V, GND→G, SDA→D, SCL→C. Check the labels on your module - pin order varies between batches.</li>
        </ol>
        <div class="callout fix"><b>Not the PCM5102A yet.</b> Leave it off for now: the shift and power-switch pins come through
        the board right where it sits (step 8).</div>`,
    },
    {
      title: 'Switches into the plate', view: V.plateAbove, focus: ['plate', 'switches'], offsets: { plate: 45, switches: 45 },
      body: `
        <ol>
          <li>Print the plate with a layer height that divides 1.5 mm (<b>0.15, 0.25 or 0.3 mm</b>) so the MX clip ledge is exact.
          <b>Test one switch in one hole first.</b></li>
          <li>Snap all <b>17 MX switches</b> in from the top (16 pads + shift), pins toward the knobs to match the PCB holes.</li>
          <li>Don't put the plate on the PCB yet.</li>
        </ol>`,
    },
    {
      title: 'Pots, OLED and power switch (top, before the plate)', view: V.underplate, focus: ['pots', 'oled', 'power'],
      ghost: ['plate', 'switches'], offsets: { plate: 45, switches: 45 },
      body: `
        <div class="callout fix"><b>Before the plate:</b> all three sit <i>under</i> the plate (pot cans and lugs in the strip pockets,
        OLED in its window pocket, switch body in its slot). Place them now, solder them in step 7.</div>
        <ol>
          <li><b>4× WH148 B10K</b>, straight-lug type: the lugs leave the side of the can about 5.3 mm up and are too short to
          bend down to the board. First, on the bench, solder a <b>diode lead offcut</b> to the tip of each lug, pointing straight
          down past the bottom of the can (about 9 mm sticking out below the lug).</li>
          <li>Set each pot on the board with its <b>lugs facing the keys</b> and the three legs in the plated oval slots
          (GND, wiper, 3V3 left to right). The slots are long, so the exact leg position does not matter.</li>
          <li><b>OLED</b>: pull the black plastic off its header. Header toward the pots, so the module is <b>upside down</b>
          (the firmware's <code>U8G2_R2</code> turns the picture back). Board pads read SDA SCL VCC GND left to right.
          It must end up <b>0.8 mm above the board</b> - the plate pocket sets that height in the next step.</li>
          <li><b>12D00G power switch</b>: three pins in, actuator up.</li>
        </ol>`,
    },
    {
      title: 'Plate onto the PCB, solder from below', view: V.plate, focus: ['plate', 'switches', 'spacers'],
      body: `
        <ol>
          <li>Put a <b>1 mm spacer</b> on each of the five screw holes.</li>
          <li>Lower plate + switches onto the board: every switch's pins through, pot shafts up through their holes,
          OLED glass into its window, slide actuator in its slot. Thread the pot nuts on loosely to hold the stack.</li>
          <li>Flip it and solder: <b>17 switches</b>, then the <b>pot legs</b> (clip them flush afterwards), the <b>OLED</b> (check it is level and 0.8 mm up) and the <b>power switch</b>.</li>
        </ol>
        <div class="callout tip">Press each switch while it cools so it sits flat in the plate.</div>`,
    },
    {
      title: 'Clip PCM-area pins, then the PCM5102A (back)', view: V.pcm, focus: ['pcm'],
      body: `
        <ol>
          <li><b>Clip flush</b> the pins under the PCM5102A outline: the <b>shift switch</b> (2 pins) and the <b>power switch</b> (3 pins).
          A strip of Kapton over them is cheap insurance.</li>
          <li>Set the jumpers <b>before</b> mounting (one face ends up against the board):
          <b>FLT L, DEMP L, XSMT H, FMT L</b>. <b>SCK</b> needs no jumper - it is tied to GND on the PCB.</li>
          <li>Module <b>printed side down</b>, along the <b>right wall</b>, jack end toward the keys. The jack opens out the module's
          long <b>VIN-side edge</b> and lines up with the hole in the right wall. Header SCK BCK DIN LCK GND VIN.</li>
        </ol>
        <div class="callout fix"><b>Why here:</b> SW17 and SW18 pins are inside the PCM5102A outline; with the module already on they
        could not be soldered or clipped.</div>`,
    },
    {
      title: 'Speaker into the shell', view: V.shellTop, focus: ['speaker', 'spkScrews'], hide: BOARD,
      body: `
        <ol>
          <li>Speaker <b>face up</b> - it presses against the plate grille when the case closes.</li>
          <li>Screw both ears to the two shell posts with <b>2× M2.5×6</b> self-tappers.</li>
          <li>Ear spacing in the design is ~37 mm, estimated from a listing photo: <b>measure your speaker</b> before printing the shell.</li>
          <li>Solder the leads from the MAX98357A + / − to the speaker tabs.</li>
        </ol>`,
    },
    {
      title: 'Battery and antenna', view: V.battery, focus: ['battery', 'antenna'], hide: BOARD,
      body: `
        <ol>
          <li>Power switch <b>OFF</b>.</li>
          <li><b>LP102540 LiPo</b> (with protection board) into the cradle under the keys.</li>
          <li>Leads to the <b>BAT+ / BAT−</b> pads (J11, left edge under the keys), soldered from the back.</li>
          <li>Plug the <b>U.FL antenna</b> into the XIAO and stick it to the inside of a wall, away from the speaker magnet and the battery.</li>
        </ol>
        <div class="callout warn"><b>Polarity:</b> check the battery's red/black against the BAT+ / BAT− pads <b>with a meter</b>
        before soldering. LiPo lead colours and JST pinouts are not reliable, and there is no reverse protection on the board.</div>
        <div class="callout note">This is the last point where both the battery pads and the cradle are easy to reach.</div>`,
    },
    {
      title: 'Board and plate into the shell', view: V.close, focus: ['screws'], ghost: ['shell'],
      body: `
        <ol>
          <li>Lower the board + plate into the shell: USB-C into the left-wall opening, the jack into the right-wall hole.</li>
          <li><b>4× M3×12 countersunk</b> in the corners, from the top. They self-tap into the shell bosses -
          snug, don't strip the plastic.</li>
          <li>There is <b>no fifth screw</b>: by the right-hand knob the shell boss and its 1 mm spacer just support the board,
          and the corner screws clamp it. The plate is solid there.</li>
          <li>Tighten the pot nuts on the strip.</li>
        </ol>`,
    },
    {
      title: 'Keycaps and knobs', view: V.caps, focus: ['keycaps', 'knobs'],
      body: `<p>Keycaps on all 17 switches (shift is the orange one, top right), then the four <b>15×17 mm knobs</b>: line them up at the same position and tighten the set screws.</p>`,
    },
    {
      title: 'First power-on: bring-up', view: V.screen, focus: [], screen: true,
      body: `
        <p>Flash the bring-up sketch from <code>firmware/</code>:</p>
        <pre>~/.platformio/penv/Scripts/pio.exe run -e bringup -t upload
pio device monitor</pre>
        <ol>
          <li>Serial should list <b>0x20</b> (PCF8574) and <b>0x3C</b> (OLED).</li>
          <li>The OLED shows the bring-up page: title, battery voltage, four pot bars and the 4×4 pad grid.</li>
          <li>Hold a pad for a tone (pad 0 = C4 … pad 15 = D#5); shift + pad = 1 kHz; the left knob is volume.</li>
        </ol>
        <p>Try it before the parts arrive: open the <b>emulator</b> at <code>sim/index.html</code> - it runs the same bring-up code.</p>
        <div class="callout tip">Missing 0x20 → check the four PCF I2C wires and the address bridges. Missing 0x3C → OLED pins/order.
        A dead pad → that pad's diode is reversed.</div>`,
    },
  ];
  return { STEP, steps, BOM, BOARD };
})();
