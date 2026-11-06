// XO-Hugbox - pin map for the v1 board (Seeed XIAO ESP32-S3)
#pragma once

// Pots (WH148 10k, wiper on ADC1). Wired so clockwise = higher reading.
#define PIN_POT0   D0
#define PIN_POT1   D1
#define PIN_POT2   D2
#define PIN_POT3   D3

// I2C bus: OLED (SH1106 1.3", 0x3C) + PCF8574T key-matrix expander (0x20-0x27, set by A0-A2 bridges)
#define PIN_SDA    D4
#define PIN_SCL    D5
#define OLED_ADDR      0x3C
#define PCF8574_ADDR   0x20   // confirm with an I2C scan
// OLED header faces the knobs, so the screen is mounted upside down: use U8G2_R2.

// I2S out -> MAX98357A (speaker) and PCM5102A (line out) in parallel
#define PIN_I2S_BCK   D6
#define PIN_I2S_LRCK  D7
#define PIN_I2S_DOUT  D8
// INMP441 mic data: the XIAO's MTDI pad (GPIO41, underside) -> wire -> J_MTDI -> J_MIC SD. Not a header pin.
#define PIN_I2S_DIN   41

// Battery voltage: 100k/100k divider from the switched battery rail (Vbat = reading x 2)
#define PIN_VBAT_SENSE D9

// Shift key: MX switch to GND, use INPUT_PULLUP, active low
#define PIN_SHIFT  D10

// PCF8574 key matrix (16 pads). Diode per key, anode at switch, cathode to row.
// Scan: drive one row LOW (others high), read columns (PCF quasi-bidirectional = weak pull-up),
// a pressed key pulls its column LOW.
// P0..P3 = rows R0..R3 (R0 = top row, nearest the knobs)
// P4..P7 = columns C0..C3 (C0 = left)
#define PCF_ROW_MASK 0x0F
#define PCF_COL_MASK 0xF0
