// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// grid-example.cpp
// Simple monome grid example - cycles LED brightness on button press.

#include <iostream>
#include <thread>
#include <chrono>

#include "MLSerialOscService.h"
#include "MLMonomeGrid.h"
#include "MLTimer.h"
#include "MLSharedResource.h"
#include "MLActor.h"

using namespace ml;

// Simple application Actor that receives grid events
class GridApp : public Actor
{
 public:
  GridApp()
  {
    // Register this actor so it can receive messages
    registerActor(Path("gridapp"), this);
  }

  ~GridApp()
  {
    removeActor(this);
    stop();
  }

  void setup()
  {
    // Get the serialosc service
    auto& service = getSerialOscService();

    // Set this actor as the listener for device events
    service.setListenerActor(Path("gridapp"));

    // Set callback for device connect/disconnect
    service.setDeviceCallback([this](const MonomeDeviceInfo& info, bool connected) {
      if (connected)
      {
        std::cout << "Device connected: " << info.id.getText();
        if (info.isGrid())
        {
          std::cout << " (grid " << info.width << "x" << info.height << ")";
        }
        else if (info.isArc())
        {
          std::cout << " (arc " << info.encoderCount << " encoders)";
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

    // Check for grid key events
    // Path format: grid/{deviceId}/key
    if (m.address.getSize() >= 3 && head(m.address) == Symbol("grid"))
    {
      Symbol eventType = nth(m.address, 2);
      if (eventType == Symbol("key"))
      {
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
    int state = static_cast<int>(arr[2]);

    std::cout << "Key: (" << x << ", " << y << ") " << (state ? "down" : "up") << std::endl;

    // Cycle through 4 brightness levels on press
    if (state == 1)
    {
      auto& service = getSerialOscService();
      if (MonomeGrid* grid = service.getFirstGrid())
      {
        auto& buffer = grid->getLedBuffer();

        // Get current level and cycle to next (0 → 5 → 10 → 15 → 0)
        int currentLevel = buffer.getLevel(x, y);
        int nextLevel;
        if (currentLevel == 0)
          nextLevel = 5;
        else if (currentLevel < 8)
          nextLevel = 10;
        else if (currentLevel < 13)
          nextLevel = 15;
        else
          nextLevel = 0;

        buffer.setLevel(x, y, nextLevel);
        grid->flushLedBuffer();
      }
    }
  }
};

int main(int argc, char* argv[])
{
  // Start the global timer system (required for Actor message processing)
  SharedResourcePointer<Timers> timers;
  timers->start(false);  // false = use background thread

  std::cout << "=== Monome Grid Example ===" << std::endl;
  std::cout << "Press keys on the grid to cycle brightness through 4 levels" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;
  std::cout << std::endl;

  GridApp app;
  app.setup();

  // Keep running until interrupted
  while (true)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return 0;
}
