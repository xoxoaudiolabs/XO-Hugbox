// XO-Hugbox board MIDI: USB (TinyUSB class-compliant MIDI, next to the USB serial port) and BLE MIDI
// (the standard Apple/MMA BLE-MIDI service on NimBLE). Both directions, both at once.
#pragma once
#include <stdint.h>
#include "hugbox_app.h"

namespace midi_io {

void begin(const char *bleName);          // call before USB.begin()
void setBle(bool wanted);                 // start/stop advertising (and drop the link when turned off)
bool bleConnected();
// Send one message (1-3 bytes) on USB and, when connected, BLE. Call from the UI task.
void send(const uint8_t *msg, int len);
// Deliver everything received since the last call. Call from the UI task.
void poll(void (*deliver)(hugbox::MidiSource src, const uint8_t *msg, int len));

}  // namespace midi_io
