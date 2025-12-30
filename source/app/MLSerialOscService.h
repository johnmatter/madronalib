// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLSerialOscService.h
// Service for discovering and managing monome devices via serialosc.

#pragma once

#include "MLActor.h"
#include "MLOSCReceiver.h"
#include "MLOSCSender.h"
#include "MLSerialOsc.h"
#include "MLSharedResource.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ml
{

class MonomeDevice;
class MonomeGrid;
class MonomeArc;

class SerialOscService : public Actor
{
 public:
  // Callback for device connect/disconnect notifications
  using DeviceCallback = std::function<void(const MonomeDeviceInfo& info, bool connected)>;

  SerialOscService();
  ~SerialOscService() override;

  // Non-copyable
  SerialOscService(const SerialOscService&) = delete;
  SerialOscService& operator=(const SerialOscService&) = delete;

  // Lifecycle
  bool start(const char* host = "127.0.0.1");
  void stop();
  bool isRunning() const { return running_; }

  // Device discovery
  void requestDeviceList();
  void subscribeToNotifications();

  // Get connected devices
  MonomeDevice* getDevice(const TextFragment& deviceId);
  MonomeGrid* getGrid(const TextFragment& deviceId);
  MonomeArc* getArc(const TextFragment& deviceId);

  // Get first available device of each type (convenience)
  MonomeGrid* getFirstGrid();
  MonomeArc* getFirstArc();

  // Get all device IDs
  std::vector<TextFragment> getDeviceIds() const;
  std::vector<TextFragment> getGridIds() const;
  std::vector<TextFragment> getArcIds() const;

  // Application registration
  void setListenerActor(const Path& actorPath);
  void setDeviceCallback(DeviceCallback cb);

  // Actor interface
  void onMessage(Message m) override;

 private:
  // OSC message handling (from serialosc daemon)
  void handleDiscoveryMessage(Path address, std::vector<Value> args);

  // Device lifecycle
  void handleDeviceAdd(const TextFragment& id, const TextFragment& type, int port);
  void handleDeviceRemove(const TextFragment& id);

  // Create the appropriate device type
  std::unique_ptr<MonomeDevice> createDevice(const MonomeDeviceInfo& info);

  // Find an available local port for receiving device messages
  int findAvailablePort(int startPort);

  // OSC communication with serialosc daemon
  OSCSender sender_;
  OSCReceiver receiver_;

  // Configuration
  TextFragment host_{"127.0.0.1"};
  int localPort_{0};

  // Device collection - keyed by device ID (string for map compatibility)
  std::map<std::string, std::unique_ptr<MonomeDevice>> devices_;
  mutable std::mutex devicesMutex_;

  // Next port to try for device receivers
  int nextDevicePort_{13001};

  // Listener registration
  Path listenerActorPath_;
  DeviceCallback deviceCallback_;

  // Running state
  bool running_{false};
  bool subscribed_{false};
};

// Global accessor using SharedResourcePointer pattern
inline SerialOscService& getSerialOscService()
{
  static SharedResourcePointer<SerialOscService> service;
  return *service;
}

}  // namespace ml
