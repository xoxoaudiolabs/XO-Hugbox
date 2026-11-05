#include "midi_io.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "USB.h"
#include "USBMIDI.h"

namespace midi_io {

using hugbox::MidiSource;

static USBMIDI usbMidi;

// ---------------------------------------------------------------- BLE MIDI
// Service / characteristic UUIDs from the BLE-MIDI spec.
static const char *const BLE_MIDI_SERVICE = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static const char *const BLE_MIDI_CHAR = "7772E5DB-3868-4112-A1A9-F2669D106BF3";
static NimBLEServer *server = nullptr;
static NimBLECharacteristic *chr = nullptr;
static volatile bool connected = false, advertising = false;

struct Msg { uint8_t b[3]; uint8_t len; };
static QueueHandle_t bleIn = nullptr;     // NimBLE host task -> UI task

static int msgLen(uint8_t status) {
  if (status >= 0xF8) return 1;                       // realtime
  switch (status & 0xF0) {
    case 0xC0: case 0xD0: return 2;
    case 0xF0: return status == 0xF2 ? 3 : (status == 0xF1 || status == 0xF3) ? 2 : 1;
    default: return 3;
  }
}

// BLE-MIDI packet: header (1tttttt), then [timestamp (1ttttttt)] status data... with running status.
static void parseBle(const uint8_t *p, size_t n) {
  if (n < 2 || !(p[0] & 0x80)) return;
  uint8_t running = 0;
  size_t i = 1;
  while (i < n) {
    if (p[i] & 0x80) {                                 // timestamp byte
      if (++i >= n) break;
      if (p[i] & 0x80) running = p[i++];               // status byte after the timestamp
      if (running == 0xF0) { while (i < n && p[i] != 0xF7) i++; i++; running = 0; continue; }   // skip sysex
    }
    if (!running) { i++; continue; }
    Msg m = {{running, 0, 0}, (uint8_t)msgLen(running)};
    for (int k = 1; k < m.len; k++) { if (i >= n || (p[i] & 0x80)) { m.len = 0; break; } m.b[k] = p[i++]; }
    if (m.len) xQueueSend(bleIn, &m, 0);
    if (running >= 0xF8) running = 0;                  // realtime doesn't set running status
  }
}

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &info) override {
    connected = true;
    advertising = false;
    // ask for a short connection interval (7.5-15 ms) for low latency
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override { connected = false; }
};

class CharCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    const NimBLEAttValue v = c->getValue();
    parseBle(v.data(), v.length());
  }
};

void begin(const char *bleName) {
  usbMidi.begin();
  bleIn = xQueueCreate(64, sizeof(Msg));
  NimBLEDevice::init(bleName);
  NimBLEDevice::setPower(3);                          // dBm
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  NimBLEService *svc = server->createService(BLE_MIDI_SERVICE);
  chr = svc->createCharacteristic(BLE_MIDI_CHAR,
                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  chr->setCallbacks(new CharCB());

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(bleName);
  adv->addServiceUUID(svc->getUUID());
  adv->enableScanResponse(true);
}

// Called about once a second: keeps advertising while BLE is wanted and nobody is connected
// (this also restarts it after a disconnect), and shuts BLE down when it is turned off.
void setBle(bool wanted) {
  if (wanted) {
    if (!connected && !advertising) advertising = NimBLEDevice::startAdvertising();
    return;
  }
  if (advertising) { NimBLEDevice::stopAdvertising(); advertising = false; }
  if (connected && server) for (uint16_t h : server->getPeerDevices()) server->disconnect(h);
}

bool bleConnected() { return connected; }

void send(const uint8_t *msg, int len) {
  if (len < 1 || len > 3) return;
  // USB: one 4-byte event packet, code index number = status high nibble (0xF for realtime single bytes)
  midiEventPacket_t pkt = {(uint8_t)(msg[0] >= 0xF8 ? 0x0F : msg[0] >> 4), msg[0], len > 1 ? msg[1] : (uint8_t)0,
                           len > 2 ? msg[2] : (uint8_t)0};
  usbMidi.writePacket(&pkt);
  // BLE: header + timestamp + message (13-bit millisecond timestamp)
  if (connected && chr) {
    const uint16_t ts = millis() & 0x1FFF;
    uint8_t b[5] = {(uint8_t)(0x80 | (ts >> 7)), (uint8_t)(0x80 | (ts & 0x7F)), msg[0], 0, 0};
    for (int i = 1; i < len; i++) b[2 + i] = msg[i];
    chr->setValue(b, 2 + len);
    chr->notify();
  }
}

void poll(void (*deliver)(MidiSource, const uint8_t *, int)) {
  midiEventPacket_t p;
  while (usbMidi.readPacket(&p)) {
    const uint8_t cin = p.header & 0x0F;
    const uint8_t m[3] = {p.byte1, p.byte2, p.byte3};
    if (cin == 0x0F) deliver(hugbox::MIDI_SRC_USB, m, 1);                    // single byte (realtime)
    else if (cin >= 0x08 && cin <= 0x0E) deliver(hugbox::MIDI_SRC_USB, m, msgLen(p.byte1));
  }
  Msg q;
  while (bleIn && xQueueReceive(bleIn, &q, 0) == pdTRUE) deliver(hugbox::MIDI_SRC_BLE, q.b, q.len);
}

}  // namespace midi_io
