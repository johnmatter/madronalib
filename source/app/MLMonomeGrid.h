// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLMonomeGrid.h
// Monome grid device support with LED buffer.

#pragma once

#include "MLMonomeDevice.h"
#include "MLGridLedBuffer.h"
#include <memory>
#include <array>

namespace ml
{

class MonomeGrid : public MonomeDevice
{
 public:
  explicit MonomeGrid(const MonomeDeviceInfo& info);
  ~MonomeGrid() override;

  // Grid dimensions (updated after /sys/size response)
  int getWidth() const { return info_.width > 0 ? info_.width : 16; }
  int getHeight() const { return info_.height > 0 ? info_.height : 8; }

  // Tilt sensor control
  void enableTilt(int sensor, bool enable);

  // === Raw LED Commands (immediate OSC send) ===

  // Single LED on/off
  void ledSet(int x, int y, bool state);

  // All LEDs on/off
  void ledAll(bool state);

  // 8x8 quadrant map using row bitmasks
  void ledMap(int xOffset, int yOffset, const std::array<uint8_t, 8>& rows);

  // Row bitmask (up to 8 LEDs)
  void ledRow(int xOffset, int y, uint8_t bitmask);

  // Row bitmask for 16-wide grids (two bytes)
  void ledRow(int xOffset, int y, uint8_t bitmask1, uint8_t bitmask2);

  // Column bitmask (up to 8 LEDs)
  void ledCol(int x, int yOffset, uint8_t bitmask);

  // Column bitmask for 16-tall grids (two bytes)
  void ledCol(int x, int yOffset, uint8_t bitmask1, uint8_t bitmask2);

  // === Level Commands (0-15 brightness, varibright devices) ===

  // Single LED level
  void ledLevelSet(int x, int y, int level);

  // All LEDs to same level
  void ledLevelAll(int level);

  // 8x8 quadrant with 64 individual levels
  void ledLevelMap(int xOffset, int yOffset, const std::array<uint8_t, 64>& levels);

  // Row of levels
  void ledLevelRow(int xOffset, int y, const uint8_t* levels, int count);

  // Column of levels
  void ledLevelCol(int x, int yOffset, const uint8_t* levels, int count);

  // === Buffer-based LED control ===

  // Get the LED buffer for batch updates
  GridLedBuffer& getLedBuffer() { return ledBuffer_; }
  const GridLedBuffer& getLedBuffer() const { return ledBuffer_; }

  // Flush dirty regions of buffer to device using optimal OSC commands
  void flushLedBuffer();

  // Flush using level commands (for varibright)
  void flushLedBufferLevels();

  // Flush using binary commands (for non-varibright)
  void flushLedBufferBinary();

  // Actor interface
  void onMessage(Message m) override;

 protected:
  void handleSysMessage(const Path& address, const std::vector<Value>& args) override;
  void handleDeviceInput(const Path& address, const std::vector<Value>& args) override;

 private:
  // Handle grid-specific input
  void handleGridKey(int x, int y, int state);
  void handleTilt(int sensor, int x, int y, int z);

  // Build the OSC address with current prefix
  TextFragment prefixedAddress(const char* suffix) const;

  // LED buffer
  GridLedBuffer ledBuffer_;
};

}  // namespace ml
