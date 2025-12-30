// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLSerialOscService.h"
#include "MLMonomeGrid.h"
#include "MLMonomeArc.h"
#include "MLOSCBuilder.h"

#include "UdpSocket.h"
#include "IpEndpointName.h"

namespace ml
{

SerialOscService::SerialOscService()
{
  // Set up receiver callback for discovery responses
  receiver_.setMessageCallback([this](Path addr, std::vector<Value> args) {
    handleDiscoveryMessage(std::move(addr), std::move(args));
  });
}

SerialOscService::~SerialOscService()
{
  stop();
}

bool SerialOscService::start(const char* host)
{
  if (running_) return true;

  host_ = TextFragment(host);

  // Find an available port for receiving discovery responses
  localPort_ = findAvailablePort(13000);
  if (localPort_ == 0) return false;

  // Open receiver for discovery responses
  if (!receiver_.open(localPort_)) return false;

  // Open sender to serialosc daemon
  if (!sender_.open(host, kSerialOscPort))
  {
    receiver_.close();
    return false;
  }

  running_ = true;

  // Start Actor message processing
  Actor::start();

  // Subscribe to device notifications
  subscribeToNotifications();

  // Request current device list
  requestDeviceList();

  return true;
}

void SerialOscService::stop()
{
  if (!running_) return;

  // Stop all devices
  {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    devices_.clear();
  }

  // Close OSC connections
  sender_.close();
  receiver_.close();

  // Stop Actor
  Actor::stop();

  running_ = false;
  subscribed_ = false;
}

void SerialOscService::requestDeviceList()
{
  if (!sender_.isOpen()) return;

  // /serialosc/list si <host> <port>
  OSCMessageBuilder msg("/serialosc/list");
  msg.addString(host_.getText());
  msg.addInt(localPort_);
  msg.sendTo(sender_);
}

void SerialOscService::subscribeToNotifications()
{
  if (!sender_.isOpen()) return;

  // /serialosc/notify si <host> <port>
  OSCMessageBuilder msg("/serialosc/notify");
  msg.addString(host_.getText());
  msg.addInt(localPort_);
  msg.sendTo(sender_);

  subscribed_ = true;
}

MonomeDevice* SerialOscService::getDevice(const TextFragment& deviceId)
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  std::string key(deviceId.getText() ? deviceId.getText() : "");
  auto it = devices_.find(key);
  return it != devices_.end() ? it->second.get() : nullptr;
}

MonomeGrid* SerialOscService::getGrid(const TextFragment& deviceId)
{
  auto* device = getDevice(deviceId);
  if (device && device->getDeviceType() == MonomeDeviceType::Grid)
  {
    return static_cast<MonomeGrid*>(device);
  }
  return nullptr;
}

MonomeArc* SerialOscService::getArc(const TextFragment& deviceId)
{
  auto* device = getDevice(deviceId);
  if (device && device->getDeviceType() == MonomeDeviceType::Arc)
  {
    return static_cast<MonomeArc*>(device);
  }
  return nullptr;
}

MonomeGrid* SerialOscService::getFirstGrid()
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  for (auto& [id, device] : devices_)
  {
    if (device && device->getDeviceType() == MonomeDeviceType::Grid)
    {
      return static_cast<MonomeGrid*>(device.get());
    }
  }
  return nullptr;
}

MonomeArc* SerialOscService::getFirstArc()
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  for (auto& [id, device] : devices_)
  {
    if (device && device->getDeviceType() == MonomeDeviceType::Arc)
    {
      return static_cast<MonomeArc*>(device.get());
    }
  }
  return nullptr;
}

std::vector<TextFragment> SerialOscService::getDeviceIds() const
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  std::vector<TextFragment> ids;
  ids.reserve(devices_.size());
  for (const auto& [id, device] : devices_)
  {
    ids.push_back(TextFragment(id.c_str()));
  }
  return ids;
}

std::vector<TextFragment> SerialOscService::getGridIds() const
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  std::vector<TextFragment> ids;
  for (const auto& [id, device] : devices_)
  {
    if (device && device->getDeviceType() == MonomeDeviceType::Grid)
    {
      ids.push_back(TextFragment(id.c_str()));
    }
  }
  return ids;
}

std::vector<TextFragment> SerialOscService::getArcIds() const
{
  std::lock_guard<std::mutex> lock(devicesMutex_);
  std::vector<TextFragment> ids;
  for (const auto& [id, device] : devices_)
  {
    if (device && device->getDeviceType() == MonomeDeviceType::Arc)
    {
      ids.push_back(TextFragment(id.c_str()));
    }
  }
  return ids;
}

void SerialOscService::setListenerActor(const Path& actorPath)
{
  listenerActorPath_ = actorPath;

  // Update all existing devices
  std::lock_guard<std::mutex> lock(devicesMutex_);
  for (auto& [id, device] : devices_)
  {
    if (device)
    {
      device->setListenerActor(actorPath);
    }
  }
}

void SerialOscService::setDeviceCallback(DeviceCallback cb)
{
  deviceCallback_ = std::move(cb);
}

void SerialOscService::handleDiscoveryMessage(Path address, std::vector<Value> args)
{
  if (!address) return;

  Symbol msgType = head(address);

  // /serialosc/device ssi <id> <type> <port> - response to /serialosc/list
  if (msgType == Symbol("serialosc"))
  {
    if (address.getSize() >= 2)
    {
      Symbol subType = nth(address, 1);

      if (subType == Symbol("device") && args.size() >= 3)
      {
        TextFragment id = args[0].getTextValue();
        TextFragment type = args[1].getTextValue();
        int port = args[2].getIntValue();
        handleDeviceAdd(id, type, port);
      }
      else if (subType == Symbol("add") && args.size() >= 3)
      {
        // Device connected notification
        TextFragment id = args[0].getTextValue();
        TextFragment type = args[1].getTextValue();
        int port = args[2].getIntValue();
        handleDeviceAdd(id, type, port);
      }
      else if (subType == Symbol("remove") && args.size() >= 1)
      {
        // Device disconnected notification
        TextFragment id = args[0].getTextValue();
        handleDeviceRemove(id);
      }
    }
  }
}

void SerialOscService::handleDeviceAdd(const TextFragment& id, const TextFragment& type, int port)
{
  std::string idKey(id.getText() ? id.getText() : "");

  // Check if device already exists
  {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    if (devices_.find(idKey) != devices_.end())
    {
      return;  // Already have this device
    }
  }

  // Create device info
  MonomeDeviceInfo info;
  info.id = id;
  info.type = type;
  info.port = port;
  info.parseType();

  // Create the appropriate device
  auto device = createDevice(info);
  if (!device)
  {
    return;
  }

  // Set listener if we have one
  if (listenerActorPath_)
  {
    device->setListenerActor(listenerActorPath_);
  }

  // Connect the device
  int deviceLocalPort = findAvailablePort(nextDevicePort_);
  if (deviceLocalPort == 0)
  {
    return;
  }
  nextDevicePort_ = deviceLocalPort + 1;

  if (!device->connect(host_.getText(), deviceLocalPort))
  {
    return;
  }

  // Register the device actor
  Path deviceActorPath(runtimePath("serialosc"), runtimePath("devices"), runtimePath(id));
  registerActor(deviceActorPath, device.get());

  // Store the device
  MonomeDevice* rawPtr = device.get();
  {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    devices_[idKey] = std::move(device);
  }

  // Notify callback
  if (deviceCallback_)
  {
    deviceCallback_(info, true);
  }

  // Send notification message to listener
  if (listenerActorPath_)
  {
    Path msgPath(runtimePath("serialosc"), runtimePath("device"), runtimePath("add"));
    Message msg(msgPath, Value(id), kMsgFromSerialOsc);
    sendMessageToActor(listenerActorPath_, msg);
  }
}

void SerialOscService::handleDeviceRemove(const TextFragment& id)
{
  std::string idKey(id.getText() ? id.getText() : "");
  MonomeDeviceInfo info;

  {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    auto it = devices_.find(idKey);
    if (it == devices_.end())
    {
      return;  // Device not found
    }

    // Get info before removing
    if (it->second)
    {
      info.id = it->second->getId();
      info.type = it->second->getTypeString();
      info.deviceType = it->second->getDeviceType();

      // Unregister actor
      removeActor(it->second.get());
    }

    // Remove device
    devices_.erase(it);
  }

  // Notify callback
  if (deviceCallback_)
  {
    deviceCallback_(info, false);
  }

  // Send notification message to listener
  if (listenerActorPath_)
  {
    Path msgPath(runtimePath("serialosc"), runtimePath("device"), runtimePath("remove"));
    Message msg(msgPath, Value(id), kMsgFromSerialOsc);
    sendMessageToActor(listenerActorPath_, msg);
  }
}

std::unique_ptr<MonomeDevice> SerialOscService::createDevice(const MonomeDeviceInfo& info)
{
  if (info.isGrid())
  {
    return std::make_unique<MonomeGrid>(info);
  }
  else if (info.isArc())
  {
    return std::make_unique<MonomeArc>(info);
  }
  return nullptr;
}

int SerialOscService::findAvailablePort(int startPort)
{
  // Try to find an available port by attempting to bind a socket
  // We avoid using OSCReceiver here because it spawns a thread that can hang on close
  for (int port = startPort; port < startPort + 100; ++port)
  {
    try
    {
      // Try to create a listening socket - if it succeeds, the port is available
      // The socket will be destroyed immediately, freeing the port
      UdpListeningReceiveSocket testSocket(
          IpEndpointName(IpEndpointName::ANY_ADDRESS, port),
          nullptr);  // No listener needed for testing
      return port;
    }
    catch (const std::exception&)
    {
      // Port not available, try next
    }
  }
  return 0;  // No available port found
}

void SerialOscService::onMessage(Message m)
{
  // Handle any messages sent to the service actor
  // Currently not used, but could be extended for control messages
}

}  // namespace ml
