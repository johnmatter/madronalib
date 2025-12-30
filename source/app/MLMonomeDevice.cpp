// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLMonomeDevice.h"
#include "MLOSCBuilder.h"
#include "MLActor.h"

namespace ml
{

MonomeDevice::MonomeDevice(const MonomeDeviceInfo& info) : info_(info)
{
  // Set up OSC receiver callback
  receiver_.setMessageCallback([this](Path addr, std::vector<Value> args) {
    handleOSCMessage(std::move(addr), std::move(args));
  });
}

MonomeDevice::~MonomeDevice()
{
  disconnect();
  stop();  // Stop the Actor timer
}

bool MonomeDevice::connect(const char* host, int localPort)
{
  if (connected_) return true;

  host_ = TextFragment(host);
  localPort_ = localPort;

  // Open sender to device's port
  if (!sender_.open(host, info_.port)) return false;

  // Open receiver on our local port
  if (!receiver_.open(localPort))
  {
    sender_.close();
    return false;
  }

  // Tell the device where to send messages
  {
    OSCMessageBuilder msg("/sys/host");
    msg.addString(host);
    msg.sendTo(sender_);
  }
  {
    OSCMessageBuilder msg("/sys/port");
    msg.addInt(localPort);
    msg.sendTo(sender_);
  }

  // Set our prefix
  setPrefix(prefix_);

  // Query device info
  queryInfo();

  connected_ = true;

  // Start the Actor's message processing timer
  start();

  return true;
}

void MonomeDevice::disconnect()
{
  if (!connected_) return;

  receiver_.close();
  sender_.close();
  connected_ = false;
}

void MonomeDevice::setPrefix(const TextFragment& prefix)
{
  prefix_ = prefix;
  if (sender_.isOpen())
  {
    OSCMessageBuilder msg("/sys/prefix");
    msg.addString(prefix.getText());
    msg.sendTo(sender_);
  }
}

void MonomeDevice::setRotation(int rotation)
{
  // Clamp to valid values
  rotation = ((rotation / 90) % 4) * 90;
  rotation_ = rotation;
  if (sender_.isOpen())
  {
    OSCMessageBuilder msg("/sys/rotation");
    msg.addInt(rotation);
    msg.sendTo(sender_);
  }
}

void MonomeDevice::setListenerActor(const Path& actorPath)
{
  listenerActorPath_ = actorPath;
}

void MonomeDevice::queryInfo()
{
  if (sender_.isOpen())
  {
    // Query device info - responses will come to our receiver
    OSCMessageBuilder msg("/sys/info");
    msg.addString(host_.getText());
    msg.addInt(localPort_);
    msg.sendTo(sender_);
  }
}

void MonomeDevice::forwardInputEvent(const Path& eventPath, const Value& value)
{
  if (listenerActorPath_)
  {
    Message msg(eventPath, value, kMsgFromSerialOsc | kMsgDeviceEvent);
    sendMessageToActor(listenerActorPath_, msg);
  }
}

void MonomeDevice::handleOSCMessage(Path address, std::vector<Value> args)
{
  // Create a message to enqueue for Actor processing
  // We store the address and first arg (or empty) - subclasses may need more args
  // For now, pack args into the message

  // Check if this is a system message
  if (address && head(address) == Symbol("sys"))
  {
    // Handle system messages directly (they're simple responses)
    handleSysMessage(address, args);
  }
  else
  {
    // For device input, strip the prefix from the address
    // e.g., /monome/grid/key -> grid/key
    Path strippedAddress = address;
    if (address && address.getSize() > 0)
    {
      // Check if the first element matches our prefix (without the leading /)
      TextFragment prefixWithoutSlash = prefix_;
      if (prefix_.getText() && prefix_.getText()[0] == '/')
      {
        prefixWithoutSlash = TextFragment(prefix_.getText() + 1);
      }

      if (head(address) == Symbol(prefixWithoutSlash.getText()))
      {
        // Strip the prefix by taking the tail of the path
        strippedAddress = tail(address);
      }
    }

    // Pack multiple args into a float array if needed
    Value val;
    if (args.size() == 1)
    {
      val = args[0];
    }
    else if (args.size() > 1)
    {
      // Pack into float array
      std::vector<float> floats;
      floats.reserve(args.size());
      for (const auto& arg : args)
      {
        if (arg.getType() == Value::kFloat)
        {
          floats.push_back(arg.getFloatValue());
        }
        else if (arg.getType() == Value::kInt)
        {
          floats.push_back(static_cast<float>(arg.getIntValue()));
        }
      }
      val = Value(floats);
    }

    Message msg(strippedAddress, val, kMsgFromSerialOsc);
    enqueueMessage(msg);
  }
}

void MonomeDevice::handleSysMessage(const Path& address, const std::vector<Value>& args)
{
  // Parse system messages like /sys/id, /sys/size, /sys/prefix, etc.
  if (address.getSize() < 2) return;

  Symbol prop = nth(address, 1);

  if (prop == Symbol("id") && !args.empty())
  {
    // Device ID confirmation
    info_.id = args[0].getTextValue();
  }
  else if (prop == Symbol("size") && args.size() >= 2)
  {
    // Grid size
    info_.width = args[0].getIntValue();
    info_.height = args[1].getIntValue();
  }
  else if (prop == Symbol("prefix") && !args.empty())
  {
    // Prefix confirmation
    prefix_ = args[0].getTextValue();
  }
  else if (prop == Symbol("rotation") && !args.empty())
  {
    // Rotation confirmation
    rotation_ = args[0].getIntValue();
  }
  else if (prop == Symbol("host") && !args.empty())
  {
    // Host confirmation (usually our own host)
  }
  else if (prop == Symbol("port") && !args.empty())
  {
    // Port confirmation
  }
}

void MonomeDevice::onMessage(Message m)
{
  if (!m.address) return;

  // Route to device-specific handler
  handleDeviceInput(m.address, {m.value});
}

}  // namespace ml
