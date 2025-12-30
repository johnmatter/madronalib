// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLMonomeGrid.h"
#include "MLOSCBuilder.h"

namespace ml
{

MonomeGrid::MonomeGrid(const MonomeDeviceInfo& info)
    : MonomeDevice(info), ledBuffer_(info.width > 0 ? info.width : 16, info.height > 0 ? info.height : 8)
{
}

MonomeGrid::~MonomeGrid()
{
  // Clear all LEDs before disconnecting
  if (isConnected())
  {
    ledAll(false);
  }
}

TextFragment MonomeGrid::prefixedAddress(const char* suffix) const
{
  return TextFragment(getPrefix(), TextFragment(suffix));
}

void MonomeGrid::enableTilt(int sensor, bool enable)
{
  OSCMessageBuilder msg(prefixedAddress("/tilt/set"));
  msg.addInt(sensor);
  msg.addInt(enable ? 1 : 0);
  msg.sendTo(getSender());
}

// === Raw LED Commands ===

void MonomeGrid::ledSet(int x, int y, bool state)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/set"));
  msg.addInt(x);
  msg.addInt(y);
  msg.addInt(state ? 1 : 0);
  msg.sendTo(getSender());
}

void MonomeGrid::ledAll(bool state)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/all"));
  msg.addInt(state ? 1 : 0);
  msg.sendTo(getSender());
}

void MonomeGrid::ledMap(int xOffset, int yOffset, const std::array<uint8_t, 8>& rows)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/map"));
  msg.addInt(xOffset);
  msg.addInt(yOffset);
  for (int i = 0; i < 8; ++i)
  {
    msg.addInt(rows[i]);
  }
  msg.sendTo(getSender());
}

void MonomeGrid::ledRow(int xOffset, int y, uint8_t bitmask)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/row"));
  msg.addInt(xOffset);
  msg.addInt(y);
  msg.addInt(bitmask);
  msg.sendTo(getSender());
}

void MonomeGrid::ledRow(int xOffset, int y, uint8_t bitmask1, uint8_t bitmask2)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/row"));
  msg.addInt(xOffset);
  msg.addInt(y);
  msg.addInt(bitmask1);
  msg.addInt(bitmask2);
  msg.sendTo(getSender());
}

void MonomeGrid::ledCol(int x, int yOffset, uint8_t bitmask)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/col"));
  msg.addInt(x);
  msg.addInt(yOffset);
  msg.addInt(bitmask);
  msg.sendTo(getSender());
}

void MonomeGrid::ledCol(int x, int yOffset, uint8_t bitmask1, uint8_t bitmask2)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/col"));
  msg.addInt(x);
  msg.addInt(yOffset);
  msg.addInt(bitmask1);
  msg.addInt(bitmask2);
  msg.sendTo(getSender());
}

// === Level Commands ===

void MonomeGrid::ledLevelSet(int x, int y, int level)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/level/set"));
  msg.addInt(x);
  msg.addInt(y);
  msg.addInt(level);
  msg.sendTo(getSender());
}

void MonomeGrid::ledLevelAll(int level)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/level/all"));
  msg.addInt(level);
  msg.sendTo(getSender());
}

void MonomeGrid::ledLevelMap(int xOffset, int yOffset, const std::array<uint8_t, 64>& levels)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/level/map"));
  msg.addInt(xOffset);
  msg.addInt(yOffset);
  for (int i = 0; i < 64; ++i)
  {
    msg.addInt(levels[i]);
  }
  msg.sendTo(getSender());
}

void MonomeGrid::ledLevelRow(int xOffset, int y, const uint8_t* levels, int count)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/level/row"));
  msg.addInt(xOffset);
  msg.addInt(y);
  for (int i = 0; i < count; ++i)
  {
    msg.addInt(levels[i]);
  }
  msg.sendTo(getSender());
}

void MonomeGrid::ledLevelCol(int x, int yOffset, const uint8_t* levels, int count)
{
  OSCMessageBuilder msg(prefixedAddress("/grid/led/level/col"));
  msg.addInt(x);
  msg.addInt(yOffset);
  for (int i = 0; i < count; ++i)
  {
    msg.addInt(levels[i]);
  }
  msg.sendTo(getSender());
}

// === Buffer operations ===

void MonomeGrid::flushLedBuffer()
{
  // Default to level-based flush for varibright support
  flushLedBufferLevels();
}

void MonomeGrid::flushLedBufferLevels()
{
  if (!ledBuffer_.isDirty()) return;

  auto dirtyQuadrants = ledBuffer_.getDirtyQuadrants();
  for (const auto& [qx, qy] : dirtyQuadrants)
  {
    auto levels = ledBuffer_.getQuadrantLevels(qx, qy);
    ledLevelMap(qx * 8, qy * 8, levels);
  }

  ledBuffer_.clearDirty();
}

void MonomeGrid::flushLedBufferBinary()
{
  if (!ledBuffer_.isDirty()) return;

  auto dirtyQuadrants = ledBuffer_.getDirtyQuadrants();
  for (const auto& [qx, qy] : dirtyQuadrants)
  {
    auto bitmask = ledBuffer_.getQuadrantBitmask(qx, qy);
    ledMap(qx * 8, qy * 8, bitmask);
  }

  ledBuffer_.clearDirty();
}

// === Message handling ===

void MonomeGrid::handleSysMessage(const Path& address, const std::vector<Value>& args)
{
  // Call base class handler
  MonomeDevice::handleSysMessage(address, args);

  // Update LED buffer size if grid size changed
  if (address.getSize() >= 2 && nth(address, 1) == Symbol("size") && args.size() >= 2)
  {
    int newWidth = args[0].getIntValue();
    int newHeight = args[1].getIntValue();
    if (newWidth > 0 && newHeight > 0)
    {
      // Recreate buffer with new size
      ledBuffer_ = GridLedBuffer(newWidth, newHeight);
    }
  }
}

void MonomeGrid::handleDeviceInput(const Path& address, const std::vector<Value>& args)
{
  if (!address) return;
  if (args.empty()) return;

  const Value& val = args[0];

  // Check for grid/key messages
  // Address pattern: grid/key with args [x, y, state]
  if (address.getSize() >= 2)
  {
    Symbol first = head(address);
    Symbol second = nth(address, 1);

    if (first == Symbol("grid") && second == Symbol("key"))
    {
      if (val.getType() == Value::kFloatArray)
      {
        const float* arr = val.getFloatArrayPtr();
        size_t arrSize = val.getFloatArraySize();
        if (arrSize >= 3)
        {
          handleGridKey(static_cast<int>(arr[0]), static_cast<int>(arr[1]),
                        static_cast<int>(arr[2]));
        }
      }
    }
  }
  else if (head(address) == Symbol("tilt"))
  {
    if (val.getType() == Value::kFloatArray)
    {
      const float* arr = val.getFloatArrayPtr();
      size_t arrSize = val.getFloatArraySize();
      if (arrSize >= 4)
      {
        handleTilt(static_cast<int>(arr[0]), static_cast<int>(arr[1]),
                   static_cast<int>(arr[2]), static_cast<int>(arr[3]));
      }
    }
  }
}

void MonomeGrid::handleGridKey(int x, int y, int state)
{
  // Forward to listener as: grid/{deviceId}/key with value [x, y, state]
  Path eventPath(runtimePath("grid"), runtimePath(info_.id), runtimePath("key"));
  std::array<float, 3> data = {static_cast<float>(x), static_cast<float>(y),
                                static_cast<float>(state)};
  forwardInputEvent(eventPath, Value(data));
}

void MonomeGrid::handleTilt(int sensor, int x, int y, int z)
{
  // Forward to listener as: grid/{deviceId}/tilt with value [sensor, x, y, z]
  Path eventPath(runtimePath("grid"), runtimePath(info_.id), runtimePath("tilt"));
  std::array<float, 4> data = {static_cast<float>(sensor), static_cast<float>(x),
                                static_cast<float>(y), static_cast<float>(z)};
  forwardInputEvent(eventPath, Value(data));
}

void MonomeGrid::onMessage(Message m)
{
  // First let base class handle it
  MonomeDevice::onMessage(m);
}

}  // namespace ml
