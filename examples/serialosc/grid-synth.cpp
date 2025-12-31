// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// grid-synth.cpp
// Polyphonic synthesizer controlled by monome grid.
// Uses madronalib's ADSR and EventsToSignals for voice management.

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <array>
#include <cmath>

#include "madronalib.h"
#include "mldsp.h"
#include "MLDSPFilters.h"
#include "MLSerialOscService.h"
#include "MLMonomeGrid.h"

using namespace ml;

// ============================================================================
// Constants
// ============================================================================

constexpr int kInputChannels = 0;
constexpr int kOutputChannels = 2;
constexpr int kSampleRate = 48000;
constexpr float kOutputGain = 0.15f;

constexpr int kNumVoices = 4;
constexpr int kGridWidth = 16;
constexpr int kGridHeight = 8;
constexpr int kBaseNote = 48;  // C3

// Signal handling for clean shutdown
std::atomic<bool> running{true};
void signalHandler(int signal)
{
  running = false;
}

// Simplex Noise (2D) - for basis function patterns
namespace noise
{

static const int perm[512] = {
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
  8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
  35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
  134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
  55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
  18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
  250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
  189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
  172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
  228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
  107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
  8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
  35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
  134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
  55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
  18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
  250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
  189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
  172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
  228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
  107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

inline float grad2(int hash, float x, float y)
{
  int h = hash & 7;
  float u = h < 4 ? x : y;
  float v = h < 4 ? y : x;
  return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

inline float simplex2D(float x, float y)
{
  constexpr float F2 = 0.366025403784439f;
  constexpr float G2 = 0.211324865405187f;

  float s = (x + y) * F2;
  int i = static_cast<int>(std::floor(x + s));
  int j = static_cast<int>(std::floor(y + s));

  float t = (i + j) * G2;
  float X0 = i - t;
  float Y0 = j - t;
  float x0 = x - X0;
  float y0 = y - Y0;

  int i1, j1;
  if (x0 > y0) { i1 = 1; j1 = 0; }
  else { i1 = 0; j1 = 1; }

  float x1 = x0 - i1 + G2;
  float y1 = y0 - j1 + G2;
  float x2 = x0 - 1.0f + 2.0f * G2;
  float y2 = y0 - 1.0f + 2.0f * G2;

  int ii = i & 255;
  int jj = j & 255;

  float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

  float t0 = 0.5f - x0 * x0 - y0 * y0;
  if (t0 >= 0.0f) {
    t0 *= t0;
    n0 = t0 * t0 * grad2(perm[ii + perm[jj]], x0, y0);
  }

  float t1 = 0.5f - x1 * x1 - y1 * y1;
  if (t1 >= 0.0f) {
    t1 *= t1;
    n1 = t1 * t1 * grad2(perm[ii + i1 + perm[jj + j1]], x1, y1);
  }

  float t2 = 0.5f - x2 * x2 - y2 * y2;
  if (t2 >= 0.0f) {
    t2 *= t2;
    n2 = t2 * t2 * grad2(perm[ii + 1 + perm[jj + 1]], x2, y2);
  }

  return 70.0f * (n0 + n1 + n2);
}

inline float fbm2D(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f)
{
  float sum = 0.0f;
  float amplitude = 1.0f;
  float frequency = 1.0f;
  float maxValue = 0.0f;

  for (int i = 0; i < octaves; ++i) {
    sum += amplitude * simplex2D(x * frequency, y * frequency);
    maxValue += amplitude;
    amplitude *= gain;
    frequency *= lacunarity;
  }

  return sum / maxValue;
}

}  // namespace noise


struct SynthVoice
{
  SineGen oscillator;
  ADSR envelope;

  void setEnvParams(float sr)
  {
    envelope.coeffs = ADSR::calcCoeffs(0.01f, 0.1f, 0.7f, 2.0f, sr);
  }

  DSPVector process(const DSPVector& pitch, const DSPVector& gate, float sr)
  {
    // Convert MIDI pitch to frequency (pitch is MIDI note number)
    DSPVector freq;
    for (int i = 0; i < kFloatsPerDSPVector; ++i) {
      freq[i] = 440.0f * std::pow(2.0f, (pitch[i] - 69.0f) / 12.0f);
    }
    DSPVector omega = freq / sr;

    DSPVector osc = oscillator(omega);
    DSPVector env = envelope(gate);
    return osc * env;
  }
};


struct GridSynthState
{
  std::array<SynthVoice, kNumVoices> voices;

  // Thread-safe event queue (grid thread -> audio thread)
  Queue<Event> eventQueue{64};

  // For LED feedback - envelope levels per voice
  std::array<float, kNumVoices> envelopeLevels{};

  // Animation time for basis function
  float animTime{0.0f};

  // Pointer to audio context (set by main)
  AudioContext* audioContext{nullptr};
};


void audioProcess(AudioContext* ctx, void* state)
{
  auto* appState = static_cast<GridSynthState*>(state);
  float sr = static_cast<float>(ctx->getSampleRate());

  // Process pending events from grid
  Event e;
  while (appState->eventQueue.pop(e)) {
    ctx->addInputEvent(e);
  }

  // Process events into voice signals
  ctx->processVector(0);

  // Mix all voices
  DSPVector mixL(0.0f);
  DSPVector mixR(0.0f);

  size_t polyphony = ctx->getInputPolyphony();
  for (size_t v = 0; v < polyphony; ++v) {
    const auto& voice = ctx->getInputVoice(static_cast<int>(v));

    // Get pitch and gate signals from EventsToSignals
    DSPVector pitch = voice.outputs.constRow(kPitch);
    DSPVector gate = voice.outputs.constRow(kGate);

    // Process voice
    DSPVector voiceOut = appState->voices[v].process(pitch, gate, sr);
    mixL += voiceOut;
    mixR += voiceOut;

    // Store envelope level for LED feedback (last sample)
    appState->envelopeLevels[v] = appState->voices[v].envelope.y;
  }

  ctx->outputs[0] = mixL * kOutputGain;
  ctx->outputs[1] = mixR * kOutputGain;
}


class GridSynthApp : public Actor
{
public:
  GridSynthState* state_;

  GridSynthApp(GridSynthState* state) : state_(state)
  {
    registerActor(Path("gridsynth"), this);
  }

  ~GridSynthApp()
  {
    removeActor(this);
    stop();
  }

  void setup()
  {
    auto& service = getSerialOscService();
    service.setListenerActor(Path("gridsynth"));

    service.setDeviceCallback([this](const MonomeDeviceInfo& info, bool connected) {
      if (connected) {
        std::cout << "Device connected: " << info.id.getText();
        if (info.isGrid()) {
          std::cout << " (grid " << info.width << "x" << info.height << ")";
          initializeGridDisplay();
        }
        std::cout << std::endl;
      } else {
        std::cout << "Device disconnected: " << info.id.getText() << std::endl;
      }
    });

    if (!service.start()) {
      std::cout << "Failed to start serialosc service" << std::endl;
      std::cout << "Make sure serialosc is running (serialoscd)" << std::endl;
      return;
    }

    std::cout << "serialosc service started, waiting for devices..." << std::endl;
    start();
  }

  void initializeGridDisplay()
  {
    updateGridLEDs();
  }

  void updateGridLEDs()
  {
    auto& service = getSerialOscService();
    MonomeGrid* grid = service.getFirstGrid();
    if (!grid) return;

    auto& buffer = grid->getLedBuffer();

    // 1. Draw animated background pattern
    state_->animTime += 0.02f;
    float scale = 0.3f;

    for (int y = 0; y < kGridHeight; ++y) {
      for (int x = 0; x < kGridWidth; ++x) {
        float nx = x * scale + state_->animTime;
        float ny = y * scale;
        float value = noise::fbm2D(nx, ny, 3, 2.0f, 0.5f);
        int level = static_cast<int>((value + 1.0f) * 2.0f);
        level = std::clamp(level, 0, 4);
        buffer.setLevel(x, y, level);
      }
    }

    // 2. Draw tonic markers (C notes) at brightness 8
    for (int y = 0; y < kGridHeight; ++y) {
      for (int x = 0; x < kGridWidth; ++x) {
        int musicalY = (kGridHeight - 1) - y;
        int midiNote = kBaseNote + (musicalY * 5) + x;
        if (midiNote % 12 == 0) {
          buffer.setLevel(x, y, 8);
        }
      }
    }

    // 3. Overlay active voice envelopes
    // Note: We don't have direct grid position -> voice mapping anymore
    // since EventsToSignals manages voices. Show envelope levels on newest voice.
    int newestVoice = state_->audioContext->getNewestInputVoice();
    if (newestVoice >= 0 && newestVoice < kNumVoices) {
      float envLevel = state_->envelopeLevels[newestVoice];
      if (envLevel > 0.01f) {
        const auto& voice = state_->audioContext->getInputVoice(newestVoice);
        float pitch = voice.currentPitch;
        // Convert pitch back to grid position
        int midiNote = static_cast<int>(pitch);
        int noteOffset = midiNote - kBaseNote;
        if (noteOffset >= 0) {
          int musicalY = noteOffset / 5;
          int x = noteOffset % 5;
          // This is approximate - fourths tuning makes this complex
          // For now, just show envelope on a calculated position
          if (musicalY < kGridHeight && x < kGridWidth) {
            int gridY = (kGridHeight - 1) - musicalY;
            int brightness = static_cast<int>(envLevel * 15.0f);
            brightness = std::clamp(brightness, 0, 15);
            buffer.setLevel(x, gridY, brightness);
          }
        }
      }
    }

    grid->flushLedBuffer();
  }

  void onMessage(Message m) override
  {
    if (!m.address) return;

    if (m.address.getSize() >= 3 && head(m.address) == Symbol("grid")) {
      Symbol eventType = nth(m.address, 2);
      if (eventType == Symbol("key")) {
        handleGridKey(m.value);
      }
    }
  }

private:
  void handleGridKey(const Value& value)
  {
    if (value.getType() != Value::kFloatArray) return;

    const float* arr = value.getFloatArrayPtr();
    size_t size = value.getFloatArraySize();
    if (size < 3) return;

    int x = static_cast<int>(arr[0]);
    int y = static_cast<int>(arr[1]);
    int keyState = static_cast<int>(arr[2]);

    // Flip y so bottom row is lowest notes
    int musicalY = (kGridHeight - 1) - y;

    // Calculate MIDI note
    int midiNote = kBaseNote + (musicalY * 5) + x;

    // Get amplitude from current LED brightness
    auto& service = getSerialOscService();
    MonomeGrid* grid = service.getFirstGrid();
    float velocity = 0.8f;
    if (grid) {
      int level = grid->getLedBuffer().getLevel(x, y);
      velocity = level <= 4 ? 0.3f : (0.3f + (level - 4) * 0.07f);
    }

    // Create event and queue for audio thread
    Event e;
    e.type = keyState ? kNoteOn : kNoteOff;
    e.channel = 1;
    e.sourceIdx = static_cast<uint16_t>(y * kGridWidth + x);  // Unique key ID
    e.time = 0;
    e.value1 = static_cast<float>(midiNote);
    e.value2 = keyState ? velocity : 0.0f;

    state_->eventQueue.push(e);

    if (keyState) {
      std::cout << "Note ON: (" << x << ", " << y << ") MIDI " << midiNote << std::endl;
    } else {
      std::cout << "Note OFF: (" << x << ", " << y << ")" << std::endl;
    }
  }
};


int main()
{
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Initialize state
  GridSynthState state;

  // Start timers (required for Actor message processing)
  SharedResourcePointer<Timers> timers;
  timers->start(false);

  // Create audio context with polyphony
  AudioContext ctx(kInputChannels, kOutputChannels, kSampleRate);
  ctx.setInputPolyphony(kNumVoices);
  state.audioContext = &ctx;

  // Initialize voice envelope params
  for (int v = 0; v < kNumVoices; ++v) {
    state.voices[v].setEnvParams(static_cast<float>(kSampleRate));
  }

  // Start audio
  AudioTask audioTask(&ctx, audioProcess, &state);

  if (audioTask.startAudio() == 0) {
    std::cout << "Failed to start audio" << std::endl;
    return 1;
  }

  std::cout << "=== Grid Synth ===" << std::endl;
  std::cout << "Chromatic layout: columns = semitones, rows = perfect fourths" << std::endl;
  std::cout << "C notes are marked at brightness 8" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;
  std::cout << std::endl;

  // Create and setup the app
  GridSynthApp app(&state);
  app.setup();

  // LED update timer (~30fps)
  Timer ledTimer;
  ledTimer.start([&app]() {
    app.updateGridLEDs();
  }, std::chrono::milliseconds(33));

  // Main loop
  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nShutting down..." << std::endl;
  ledTimer.stop();
  audioTask.stopAudio();

  return 0;
}
