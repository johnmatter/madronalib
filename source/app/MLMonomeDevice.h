// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLMonomeDevice.h
// Base class for monome devices (grid, arc).
// Handles OSC communication and device configuration.

#pragma once

#include "MLActor.h"
#include "MLOSCReceiver.h"
#include "MLOSCSender.h"
#include "MLSerialOsc.h"

namespace ml
{

class SerialOscService;

class MonomeDevice : public Actor
{
  friend class SerialOscService;

 public:
  explicit MonomeDevice(const MonomeDeviceInfo& info);
  ~MonomeDevice() override;

  // Non-copyable
  MonomeDevice(const MonomeDevice&) = delete;
  MonomeDevice& operator=(const MonomeDevice&) = delete;

  // Device properties
  const TextFragment& getId() const { return info_.id; }
  const TextFragment& getTypeString() const { return info_.type; }
  MonomeDeviceType getDeviceType() const { return info_.deviceType; }
  int getPort() const { return info_.port; }
  bool isConnected() const { return connected_; }

  // Configuration - these send OSC to device
  void setPrefix(const TextFragment& prefix);
  const TextFragment& getPrefix() const { return prefix_; }

  void setRotation(int rotation);  // 0, 90, 180, 270
  int getRotation() const { return rotation_; }

  // Set the Actor path that will receive input events from this device
  void setListenerActor(const Path& actorPath);
  const Path& getListenerActor() const { return listenerActorPath_; }

  // Query device info (async - responses come via onMessage)
  void queryInfo();

  // Actor interface
  void onMessage(Message m) override;

 protected:
  // For subclasses to send OSC messages to device
  OSCSender& getSender() { return sender_; }

  // Forward an input event to the listener actor
  void forwardInputEvent(const Path& eventPath, const Value& value);

  // Handle device system messages (/sys/*)
  virtual void handleSysMessage(const Path& address, const std::vector<Value>& args);

  // Handle device-specific input (override in subclasses)
  virtual void handleDeviceInput(const Path& address, const std::vector<Value>& args) {}

  // Device info
  MonomeDeviceInfo info_;

 private:
  // Connect to the device (called by SerialOscService)
  bool connect(const char* host, int localPort);
  void disconnect();

  // OSC message handler (called from receiver thread)
  void handleOSCMessage(Path address, std::vector<Value> args);

  // OSC communication
  OSCSender sender_;
  OSCReceiver receiver_;

  // Configuration
  TextFragment prefix_{kDefaultMonomePrefix};
  int rotation_{0};

  // State
  bool connected_{false};
  TextFragment host_;
  int localPort_{0};
  Path listenerActorPath_;
};

}  // namespace ml
