# Shared layout (plate coordinates: x right, y up, origin = plate bottom-left), mm
P = 19.05; BORDER = 8.0; W = 4*P + 2*BORDER            # 92.2
H = 167.5
WALL = 2.0; INSET = 2.4                                 # PCB inset from plate/shell outer edge
def cx(i): return BORDER + P/2 + i*P
def cy(j): return BORDER + P/2 + j*P                    # j=0 bottom row
GRID_TOP = BORDER + 4*P                                 # 84.2
POT_Y = 117.0
POT_SLOT = (-14.5, -6.0)        # pin slot range from pot centre (pins face the keys)
POT_PITCH = 5.0
ROW2 = 128.5
# speaker: 31 wide (x) x 28.2 deep (y) body; ears stick out along y, about halfway down the body
SPK = dict(x0=2.6, y0=ROW2, w=31.4, h=28.6)
SPK_EAR_PITCH = 37.0            # hole-to-hole, estimated from listing photo (measure!)
SPK_EAR_DROP = 7.65             # speaker face -> middle of ear (listing: ears halfway down 15.3 body)
SPK_EAR_T = 1.5
SPK_C = (SPK['x0']+SPK['w']/2, SPK['y0']+SPK['h']/2)
NOTCH = (2.4, ROW2-0.4, 34.6, H-INSET)                    # PCB cut-out for speaker body
OLED = dict(x0=34.8, y0=ROW2, w=36.4, h=34.4)
OLED_C = (OLED['x0']+OLED['w']/2, OLED['y0']+OLED['h']/2)
OLED_HDR_FROM_EDGE = 2.6        # header row distance from module bottom edge (measure!)
OLED_WIN_OFF, OLED_GLASS_OFF = 0.7, 3.4
SHIFT = (80.5, ROW2+11.0)
PWR = (79.5, ROW2+26.0)             # 12D00G slide switch, actuator up through plate
HOLES = [(6,6),(W-6,6),(6,70),(W-6,70),(W-6,112.0)]
# Case screws: only the four corners (M3x12 countersunk from the top). The pot-strip position has NO screw:
# its shell boss + 1 mm spacer act as a support post under the knob strip (clamped by the corner screws),
# and the top plate is solid there.
SUPPORT_ONLY = [4]              # indices into HOLES: boss + spacer, no screw, no hole in the plate
XIAO_C = (12.5, 90.9)           # USB-C toward left wall
MAX_C = (34.5, 91.0)
PCF_ROW = (67.0, 99.5)          # P0..P7 row centre
# PCM5102: the jack opens out the module's LONG side (VIN side), at the end opposite the pins.
# Module runs front-to-back along the right wall, printed side down, jack end toward the front.
PCM_LEN = 33.0                  # module length (measure!)
PCM_W = 17.5                    # module width (measure!)
PCM_X1 = 88.0                   # VIN-side long edge, toward the right wall
PCM_Y0 = ROW2+3.3                  # jack end
PCM_ROW_Y = PCM_Y0 + PCM_LEN - 1.5            # SCK..VIN pin row near the back end
PCM_XC = PCM_X1 - PCM_W/2
JACK_FROM_END = 4.7             # barrel centre from the jack end of the module (measure!)
JACK_Y = PCM_Y0 + JACK_FROM_END
# vertical stack (shell z, floor bottom = 0)
FLOOR = 2.0; Z_PCB = 16.5; PCB_T = 1.6
Z_PLATE_BOT = Z_PCB + PCB_T + 1.0; PLATE_T = 4.0
POT_BODY_H = 7.0                # base-to-can-top height (measure!)
