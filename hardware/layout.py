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
OLED_HDR_FROM_EDGE = 2.6        # board footprint: header row 2.6 mm above OLED y0 (baked into the v1/v1.1 gerbers)
OLED_HDR_Y = OLED['y0'] + OLED_HDR_FROM_EDGE
# The module itself (header side = low y, toward the pots; ribbon/ledge side = high y). Published 1.3" SH1106 specs where
# the calipers agreed (board 35.56 x 33.66 measured vs 35.4 x 33.5), measured glass where they did not (21.73 vs 23.0).
OLED_MOD = (35.4, 33.5)         # module board (spec)
OLED_MOD_HDR = 2.1              # module edge -> header pin centres (photo estimate; board pocket also clears 2.6)
OLED_MOD_Y0 = OLED_HDR_Y - OLED_MOD_HDR
OLED_GLASS = (34.5, 21.73)      # glass incl. ribbon ledge: spec width, measured height
OLED_GLASS_Y0 = OLED_MOD_Y0 + 6.91   # measured: header edge -> glass (and 5.02 from the far edge)
OLED_AA = (29.42, 14.7)         # active area (spec)
OLED_AA_CY = OLED_MOD_Y0 + 16.3      # active-area centre (photo estimate, centred in the black area)
OLED_WIN = (31.0, 16.0)         # plate window: active area + ~0.7 mm each side
OLED_PCB_POCKET = (OLED_HDR_Y - 2.6 - 0.5, OLED_MOD_Y0 + OLED_MOD[1] + 0.5)   # y range, header distance 2.1..2.6
OLED_WIN_OFF = OLED_AA_CY - OLED_C[1]
OLED_GLASS_OFF = OLED_GLASS_Y0 + OLED_GLASS[1] / 2 - OLED_C[1]
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
POT_BODY_H = 7.12               # base-to-can-top height (measured, WH148 B10K)
POT_CAN_D = 16.27               # can diameter (measured)
# The WH148s in hand are the straight-lug type: the solder lugs leave the side of the can near the top and point
# toward the keys. They are too short to bend down into the board, so a short wire leg (diode lead offcut) runs
# from each lug down into the oval slot below it; the plate carries a relief over the lugs.
POT_LUG_Z = 5.28                # PCB top -> underside of the lugs (measured)
POT_LUG_L = 4.38                # lug reach past the can (measured)
POT_LUG_T = 0.5                 # lug thickness (assumed)
