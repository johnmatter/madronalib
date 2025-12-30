// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLMonomeArc.h"
#include "MLOSCBuilder.h"

namespace ml
{

MonomeArc::MonomeArc(const MonomeDeviceInfo& info) : MonomeDevice(info)
{
}

TextFragment MonomeArc::prefixedAddress(const char* suffix) const
{
  return TextFragment(getPrefix(), TextFragment(suffix));
}

// === Raw Ring LED Commands ===

void MonomeArc::ringSet(int ring, int led, int level)
{
  OSCMessageBuilder msg(prefixedAddress("/ring/set"));
  msg.addInt(ring);
  msg.addInt(led);
  msg.addInt(level);
  msg.sendTo(getSender());
}

void MonomeArc::ringAll(int ring, int level)
{
  OSCMessageBuilder msg(prefixedAddress("/ring/all"));
  msg.addInt(ring);
  msg.addInt(level);
  msg.sendTo(getSender());
}

void MonomeArc::ringMap(int ring, const std::array<uint8_t, 64>& levels)
{
  OSCMessageBuilder msg(prefixedAddress("/ring/map"));
  msg.addInt(ring);
  for (int i = 0; i < 64; ++i)
  {
    msg.addInt(levels[i]);
  }
  msg.sendTo(getSender());
}

void MonomeArc::ringRange(int ring, int start, int end, int level)
{
  OSCMessageBuilder msg(prefixedAddress("/ring/range"));
  msg.addInt(ring);
  msg.addInt(start);
  msg.addInt(end);
  msg.addInt(level);
  msg.sendTo(getSender());
}

// === Buffer operations ===

ArcRingBuffer& MonomeArc::getRingBuffer(int ring)
{
  ring = std::clamp(ring, 0, kMaxEncoders - 1);
  return ringBuffers_[ring];
}

const ArcRingBuffer& MonomeArc::getRingBuffer(int ring) const
{
  ring = std::clamp(ring, 0, kMaxEncoders - 1);
  return ringBuffers_[ring];
}

void MonomeArc::flushRingBuffers()
{
  int count = getEncoderCount();
  for (int i = 0; i < count; ++i)
  {
    flushRingBuffer(i);
  }
}

void MonomeArc::flushRingBuffer(int ring)
{
  if (ring < 0 || ring >= kMaxEncoders) return;

  ArcRingBuffer& buffer = ringBuffers_[ring];
  if (!buffer.isDirty()) return;

  // Send all 64 LED levels
  ringMap(ring, buffer.getAllLevels());
  buffer.clearDirty();
}

// === Message handling ===

void MonomeArc::handleDeviceInput(const Path& address, const std::vector<Value>& args)
{
  if (!address || args.empty()) return;

  const Value& val = args[0];

  // Check for enc/delta or enc/key messages
  if (address.getSize() >= 2)
  {
    Symbol first = head(address);
    Symbol second = nth(address, 1);

    if (first == Symbol("enc"))
    {
      if (second == Symbol("delta"))
      {
        if (val.getType() == Value::kFloatArray)
        {
          const float* arr = val.getFloatArrayPtr();
          size_t arrSize = val.getFloatArraySize();
          if (arrSize >= 2)
          {
            handleEncoderDelta(static_cast<int>(arr[0]), static_cast<int>(arr[1]));
          }
        }
      }
      else if (second == Symbol("key"))
      {
        if (val.getType() == Value::kFloatArray)
        {
          const float* arr = val.getFloatArrayPtr();
          size_t arrSize = val.getFloatArraySize();
          if (arrSize >= 2)
          {
            handleEncoderKey(static_cast<int>(arr[0]), static_cast<int>(arr[1]));
          }
        }
      }
    }
  }
}

void MonomeArc::handleEncoderDelta(int encoder, int delta)
{
  // Forward to listener as: arc/{deviceId}/delta with value [encoder, delta]
  Path eventPath(runtimePath("arc"), runtimePath(info_.id), runtimePath("delta"));
  std::array<float, 2> data = {static_cast<float>(encoder), static_cast<float>(delta)};
  forwardInputEvent(eventPath, Value(data));
}

void MonomeArc::handleEncoderKey(int encoder, int state)
{
  // Forward to listener as: arc/{deviceId}/key with value [encoder, state]
  Path eventPath(runtimePath("arc"), runtimePath(info_.id), runtimePath("key"));
  std::array<float, 2> data = {static_cast<float>(encoder), static_cast<float>(state)};
  forwardInputEvent(eventPath, Value(data));
}

void MonomeArc::onMessage(Message m)
{
  // Let base class handle it
  MonomeDevice::onMessage(m);
}

}  // namespace ml
