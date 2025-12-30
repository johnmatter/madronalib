// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// arc-example.cpp
// Simple monome arc example - tracks encoder positions and displays on ring LEDs.

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <array>
#include <cmath>

#include "MLSerialOscService.h"
#include "MLMonomeArc.h"
#include "MLTimer.h"
#include "MLSharedResource.h"
#include "MLActor.h"

using namespace ml;

// Signal handling for clean shutdown
std::atomic<bool> running{true};

void signalHandler(int signal)
{
  running = false;
}

// Simple application Actor that receives arc events
class ArcApp : public Actor
{
 public:
  static constexpr float kSensitivity = 1.0f / 256.0f;

  ArcApp()
  {
    // Register this actor so it can receive messages
    registerActor(Path("arcapp"), this);

    // Initialize encoder positions
    encoderPositions_.fill(0.0f);
  }

  ~ArcApp()
  {
    removeActor(this);
    stop();
  }

  void setup()
  {
    // Get the serialosc service
    auto& service = getSerialOscService();

    // Set this actor as the listener for device events
    service.setListenerActor(Path("arcapp"));

    // Set callback for device connect/disconnect
    service.setDeviceCallback([this](const MonomeDeviceInfo& info, bool connected) {
      if (connected)
      {
        std::cout << "Device connected: " << info.id.getText();
        if (info.isArc())
        {
          std::cout << " (arc " << info.encoderCount << " encoders)";
          initializeAllRingDisplays();
        }
        else if (info.isGrid())
        {
          std::cout << " (grid " << info.width << "x" << info.height << ")";
        }
        std::cout << std::endl;
      }
      else
      {
        std::cout << "Device disconnected: " << info.id.getText() << std::endl;
      }
    });

    // Start the service
    if (!service.start())
    {
      std::cout << "Failed to start serialosc service" << std::endl;
      std::cout << "Make sure serialosc is running (serialoscd)" << std::endl;
      std::cout.flush();
      return;
    }

    std::cout << "serialosc service started, waiting for devices..." << std::endl;

    // Start this actor's message processing
    start();
  }

  void onMessage(Message m) override
  {
    if (!m.address) return;

    // Check for arc events
    // Path format: arc/{deviceId}/delta or arc/{deviceId}/key
    if (m.address.getSize() >= 3 && head(m.address) == Symbol("arc"))
    {
      Symbol eventType = nth(m.address, 2);
      if (eventType == Symbol("delta"))
      {
        handleEncoderDelta(m.value);
      }
      else if (eventType == Symbol("key"))
      {
        handleEncoderKey(m.value);
      }
    }
  }

 private:
  void handleEncoderDelta(const Value& value)
  {
    if (value.getType() != Value::kFloatArray) return;

    const float* arr = value.getFloatArrayPtr();
    size_t size = value.getFloatArraySize();
    if (size < 2) return;

    int encoder = static_cast<int>(arr[0]);
    int delta = static_cast<int>(arr[1]);

    if (encoder < 0 || encoder >= 4) return;

    // Accumulate position
    encoderPositions_[encoder] += delta * kSensitivity;

    // Wrap to 0.0-1.0 range
    encoderPositions_[encoder] -= std::floor(encoderPositions_[encoder]);

    std::cout << "Encoder " << encoder << ": delta=" << delta
              << " pos=" << encoderPositions_[encoder] << std::endl;

    updateRingDisplay(encoder);
  }

  void handleEncoderKey(const Value& value)
  {
    if (value.getType() != Value::kFloatArray) return;

    const float* arr = value.getFloatArrayPtr();
    size_t size = value.getFloatArraySize();
    if (size < 2) return;

    int encoder = static_cast<int>(arr[0]);
    int state = static_cast<int>(arr[1]);

    if (encoder < 0 || encoder >= 4) return;

    std::cout << "Encoder " << encoder << " " << (state ? "pressed" : "released") << std::endl;

    // Reset all positions when encoder 0 is pressed
    // (some arc models only have a button on encoder 0)
    if (state == 1 && encoder == 0)
    {
      for (int i = 0; i < 4; ++i)
      {
        encoderPositions_[i] = 0.0f;
        updateRingDisplay(i);
      }
    }
  }

  void initializeAllRingDisplays()
  {
    auto& service = getSerialOscService();
    if (MonomeArc* arc = service.getFirstArc())
    {
      for (int i = 0; i < 4; ++i)
      {
        updateRingDisplay(i);
      }
    }
  }

  void updateRingDisplay(int encoder)
  {
    auto& service = getSerialOscService();
    if (MonomeArc* arc = service.getFirstArc())
    {
      auto& buffer = arc->getRingBuffer(encoder);
      buffer.setPosition(encoderPositions_[encoder]);
      arc->flushRingBuffer(encoder);
    }
  }

  std::array<float, 4> encoderPositions_;
};

int main(int argc, char* argv[])
{
  // Install signal handlers for clean shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Start the global timer system (required for Actor message processing)
  SharedResourcePointer<Timers> timers;
  timers->start(false);  // false = use background thread

  std::cout << "=== Monome Arc Example ===" << std::endl;
  std::cout << "Turn encoders to move position indicator" << std::endl;
  std::cout << "Press encoder 0 to reset all positions to zero" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;
  std::cout << std::endl;

  ArcApp app;
  app.setup();

  // Keep running until interrupted
  while (running)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nShutting down..." << std::endl;
  return 0;
}
