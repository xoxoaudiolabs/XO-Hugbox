// XO-Hugbox sound engine: 6-voice subtractive/FM synth + 16-pad sample player.
// Runs entirely on the audio core. Parameters are read from the shared Model once per block.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dsp.h"
#include "hugbox_app.h"   // SAMPLE_RATE, AUDIO_FRAMES
#include "model.h"

namespace hugbox {

constexpr int SYNTH_VOICES = 6;

class Synth {
 public:
  // gateSamples < 0 = held until noteOff (live pads); otherwise released automatically (seq/arp).
  void noteOn(int note, int vel, int32_t gateSamples, const Model &m);
  void noteOff(int note);
  void releaseAll();
  // Adds the synth into out (mono) for n frames.
  void render(float *out, int n, const Model &m);
  int active() const;

 private:
  struct Voice {
    bool on = false;
    uint8_t note = 0, stage = 0;     // stage 0 attack, 1 decay/sustain, 3 release
    float vel = 1, env = 0;
    float f = 440, target = 440;      // Hz, glides toward target
    float ph1 = 0, ph2 = 0, phm = 0;
    int32_t gate = -1;
    float drift = 0, driftTo = 0;
    int driftCount = 0;
    uint32_t age = 0;
    dsp::Svf flt;
  };
  Voice v_[SYNTH_VOICES];
  uint32_t clock_ = 0, rng_ = 0x9E3779B9u;
};

struct Sample {
  const int16_t *data = nullptr;
  uint32_t len = 0;
};

class Sampler {
 public:
  // Renders the built-in kit into memory from alloc (PSRAM on the board). Returns bytes used, 0 on failure.
  size_t init(void *(*alloc)(size_t), void (*release)(void *));
  void trigger(int pad, int vel, const Model &m, int semis = 0);
  void stopAll();
  // Adds the drums into l/r for n frames.
  void render(float *l, float *r, int n, const Model &m);
  const Sample &sample(int i) const { return s_[i]; }
  bool ready() const { return s_[0].data != nullptr; }
  // User sample in slot i (recorded with the mic), played instead of the built-in sound; data = nullptr goes back
  // to the kit. Audio core (or before audio starts). Voices still playing the old buffer stop; returns the old buffer
  // so the caller can free it outside the audio core.
  const int16_t *setUser(int i, const int16_t *data, uint32_t len);
  bool hasUser(int i) const { return u_[i].data != nullptr; }
  // Plays user slot i as it is (no pad settings) on its own voice: the sample editor's preview (slot 16).
  void preview(int i);

 private:
  struct Voice {
    bool on = false;
    const int16_t *data = nullptr;
    uint32_t len = 0;
    float pos = 0, rate = 1, gain = 0, env = 1, envK = 1, gl = 0.7f, gr = 0.7f;
    dsp::Svf flt;
    bool filtered = false;
  };
  Sample s_[16];
  Sample u_[17];                       // 16 user samples + the editor's preview
  Voice v_[17];
};

// Built-in kit: 16 synthesized one-shots (kit.cpp). Recorded user samples (Sampler::setUser) play in their place.
size_t renderKit(Sample out[16], void *(*alloc)(size_t), void (*release)(void *));

}  // namespace hugbox
