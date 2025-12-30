// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// serialosc unit tests using the Catch framework.
// Tests for LED buffers and monome device types.

#include "catch.hpp"
#include "MLGridLedBuffer.h"
#include "MLArcRingBuffer.h"
#include "MLSerialOsc.h"

using namespace ml;

// ============================================================================
// GridLedBuffer Tests
// ============================================================================

TEST_CASE("madronalib/serialosc/grid_led_buffer", "[serialosc]")
{
  SECTION("basic dimensions")
  {
    GridLedBuffer buffer(16, 8);
    REQUIRE(buffer.getWidth() == 16);
    REQUIRE(buffer.getHeight() == 8);
  }

  SECTION("default dimensions")
  {
    GridLedBuffer buffer;
    REQUIRE(buffer.getWidth() == 16);
    REQUIRE(buffer.getHeight() == 8);
  }

  SECTION("set and get level")
  {
    GridLedBuffer buffer(8, 8);

    // Initially all LEDs are off
    REQUIRE(buffer.getLevel(0, 0) == 0);
    REQUIRE(buffer.getLevel(7, 7) == 0);

    // Set a level
    buffer.setLevel(3, 4, 10);
    REQUIRE(buffer.getLevel(3, 4) == 10);

    // Other LEDs unaffected
    REQUIRE(buffer.getLevel(0, 0) == 0);
  }

  SECTION("level clamping")
  {
    GridLedBuffer buffer(8, 8);

    // Test clamping to max level
    buffer.setLevel(0, 0, 20);
    REQUIRE(buffer.getLevel(0, 0) == 15);  // kMaxLevel

    // Test clamping to min level
    buffer.setLevel(1, 1, -5);
    REQUIRE(buffer.getLevel(1, 1) == 0);
  }

  SECTION("bounds checking")
  {
    GridLedBuffer buffer(8, 8);

    // Out of bounds access should return 0
    REQUIRE(buffer.getLevel(-1, 0) == 0);
    REQUIRE(buffer.getLevel(0, -1) == 0);
    REQUIRE(buffer.getLevel(8, 0) == 0);
    REQUIRE(buffer.getLevel(0, 8) == 0);

    // Out of bounds set should be ignored
    buffer.setLevel(-1, 0, 15);
    buffer.setLevel(100, 100, 15);
    // No crash is success
  }

  SECTION("binary set/get")
  {
    GridLedBuffer buffer(8, 8);

    buffer.set(2, 3, true);
    REQUIRE(buffer.get(2, 3) == true);
    REQUIRE(buffer.getLevel(2, 3) == 15);

    buffer.set(2, 3, false);
    REQUIRE(buffer.get(2, 3) == false);
    REQUIRE(buffer.getLevel(2, 3) == 0);
  }

  SECTION("toggle")
  {
    GridLedBuffer buffer(8, 8);

    REQUIRE(buffer.get(0, 0) == false);
    buffer.toggle(0, 0);
    REQUIRE(buffer.get(0, 0) == true);
    buffer.toggle(0, 0);
    REQUIRE(buffer.get(0, 0) == false);
  }

  SECTION("fill")
  {
    GridLedBuffer buffer(8, 8);

    buffer.fill(7);
    for (int y = 0; y < 8; ++y)
    {
      for (int x = 0; x < 8; ++x)
      {
        REQUIRE(buffer.getLevel(x, y) == 7);
      }
    }

    buffer.clear();
    REQUIRE(buffer.getLevel(0, 0) == 0);
    REQUIRE(buffer.getLevel(7, 7) == 0);
  }

  SECTION("fill rect")
  {
    GridLedBuffer buffer(16, 8);

    buffer.fillRect(2, 2, 4, 3, 10);

    // Inside rect
    REQUIRE(buffer.getLevel(2, 2) == 10);
    REQUIRE(buffer.getLevel(5, 4) == 10);

    // Outside rect
    REQUIRE(buffer.getLevel(0, 0) == 0);
    REQUIRE(buffer.getLevel(6, 2) == 0);
    REQUIRE(buffer.getLevel(2, 5) == 0);
  }

  SECTION("dirty tracking")
  {
    GridLedBuffer buffer(16, 8);

    // Initially not dirty
    REQUIRE(buffer.isDirty() == false);

    // Setting a level marks it dirty
    buffer.setLevel(0, 0, 5);
    REQUIRE(buffer.isDirty() == true);

    // Clear dirty flag
    buffer.clearDirty();
    REQUIRE(buffer.isDirty() == false);

    // Setting same value doesn't mark dirty
    buffer.setLevel(0, 0, 5);
    REQUIRE(buffer.isDirty() == false);

    // Setting different value marks dirty
    buffer.setLevel(0, 0, 10);
    REQUIRE(buffer.isDirty() == true);
  }

  SECTION("quadrant dirty tracking")
  {
    GridLedBuffer buffer(16, 16);

    buffer.clearDirty();

    // Modify quadrant (0,0)
    buffer.setLevel(3, 3, 5);
    REQUIRE(buffer.isQuadrantDirty(0, 0) == true);
    REQUIRE(buffer.isQuadrantDirty(1, 0) == false);
    REQUIRE(buffer.isQuadrantDirty(0, 1) == false);
    REQUIRE(buffer.isQuadrantDirty(1, 1) == false);

    buffer.clearDirty();

    // Modify quadrant (1,1)
    buffer.setLevel(10, 10, 5);
    REQUIRE(buffer.isQuadrantDirty(0, 0) == false);
    REQUIRE(buffer.isQuadrantDirty(1, 0) == false);
    REQUIRE(buffer.isQuadrantDirty(0, 1) == false);
    REQUIRE(buffer.isQuadrantDirty(1, 1) == true);
  }

  SECTION("get dirty quadrants")
  {
    GridLedBuffer buffer(16, 16);

    buffer.setLevel(1, 1, 5);    // Quadrant (0,0)
    buffer.setLevel(12, 12, 5);  // Quadrant (1,1)

    auto dirty = buffer.getDirtyQuadrants();
    REQUIRE(dirty.size() == 2);
  }

  SECTION("get quadrant levels")
  {
    GridLedBuffer buffer(16, 16);

    // Set some values in quadrant (0,0)
    buffer.setLevel(0, 0, 15);
    buffer.setLevel(7, 7, 10);

    auto levels = buffer.getQuadrantLevels(0, 0);
    REQUIRE(levels[0] == 15);        // (0,0) -> index 0
    REQUIRE(levels[63] == 10);       // (7,7) -> index 7*8+7 = 63
  }

  SECTION("get quadrant bitmask")
  {
    GridLedBuffer buffer(16, 16);

    // Set first LED of each row in quadrant (0,0)
    for (int y = 0; y < 8; ++y)
    {
      buffer.setLevel(0, y, 15);
    }

    auto bitmask = buffer.getQuadrantBitmask(0, 0);
    for (int row = 0; row < 8; ++row)
    {
      REQUIRE(bitmask[row] == 0x01);  // Only first bit set
    }
  }
}

// ============================================================================
// ArcRingBuffer Tests
// ============================================================================

TEST_CASE("madronalib/serialosc/arc_ring_buffer", "[serialosc]")
{
  SECTION("basic properties")
  {
    ArcRingBuffer buffer;
    REQUIRE(ArcRingBuffer::kLedCount == 64);
    REQUIRE(ArcRingBuffer::kMaxLevel == 15);
  }

  SECTION("set and get level")
  {
    ArcRingBuffer buffer;

    // Initially all LEDs are off
    REQUIRE(buffer.getLevel(0) == 0);
    REQUIRE(buffer.getLevel(63) == 0);

    // Set a level
    buffer.setLevel(10, 12);
    REQUIRE(buffer.getLevel(10) == 12);

    // Other LEDs unaffected
    REQUIRE(buffer.getLevel(0) == 0);
  }

  SECTION("level clamping")
  {
    ArcRingBuffer buffer;

    buffer.setLevel(0, 20);
    REQUIRE(buffer.getLevel(0) == 15);

    buffer.setLevel(1, -5);
    REQUIRE(buffer.getLevel(1) == 0);
  }

  SECTION("bounds checking")
  {
    ArcRingBuffer buffer;

    REQUIRE(buffer.getLevel(-1) == 0);
    REQUIRE(buffer.getLevel(64) == 0);
    REQUIRE(buffer.getLevel(100) == 0);

    // Out of bounds set should be ignored
    buffer.setLevel(-1, 15);
    buffer.setLevel(64, 15);
    // No crash is success
  }

  SECTION("fill")
  {
    ArcRingBuffer buffer;

    buffer.fill(8);
    for (int i = 0; i < 64; ++i)
    {
      REQUIRE(buffer.getLevel(i) == 8);
    }

    buffer.clear();
    REQUIRE(buffer.getLevel(0) == 0);
    REQUIRE(buffer.getLevel(63) == 0);
  }

  SECTION("fill range no wrap")
  {
    ArcRingBuffer buffer;

    buffer.fillRange(10, 20, 12);

    REQUIRE(buffer.getLevel(9) == 0);
    REQUIRE(buffer.getLevel(10) == 12);
    REQUIRE(buffer.getLevel(15) == 12);
    REQUIRE(buffer.getLevel(20) == 12);
    REQUIRE(buffer.getLevel(21) == 0);
  }

  SECTION("fill range with wrap")
  {
    ArcRingBuffer buffer;

    // Range wraps around: 60-63 and 0-4
    buffer.fillRange(60, 4, 10);

    REQUIRE(buffer.getLevel(59) == 0);
    REQUIRE(buffer.getLevel(60) == 10);
    REQUIRE(buffer.getLevel(63) == 10);
    REQUIRE(buffer.getLevel(0) == 10);
    REQUIRE(buffer.getLevel(4) == 10);
    REQUIRE(buffer.getLevel(5) == 0);
  }

  SECTION("dirty tracking")
  {
    ArcRingBuffer buffer;

    REQUIRE(buffer.isDirty() == false);

    buffer.setLevel(0, 5);
    REQUIRE(buffer.isDirty() == true);

    buffer.clearDirty();
    REQUIRE(buffer.isDirty() == false);

    // Same value doesn't mark dirty
    buffer.setLevel(0, 5);
    REQUIRE(buffer.isDirty() == false);

    // Different value marks dirty
    buffer.setLevel(0, 10);
    REQUIRE(buffer.isDirty() == true);
  }

  SECTION("get all levels")
  {
    ArcRingBuffer buffer;

    buffer.setLevel(0, 15);
    buffer.setLevel(32, 8);
    buffer.setLevel(63, 1);

    auto levels = buffer.getAllLevels();
    REQUIRE(levels[0] == 15);
    REQUIRE(levels[32] == 8);
    REQUIRE(levels[63] == 1);
    REQUIRE(levels[16] == 0);
  }

  SECTION("set position")
  {
    ArcRingBuffer buffer;

    // Position at 0.5 should light up LED 32 (middle)
    buffer.setPosition(0.5f, 15, 0);
    REQUIRE(buffer.getLevel(32) == 15);

    // With falloff
    buffer.clear();
    buffer.setPosition(0.5f, 15, 2);
    REQUIRE(buffer.getLevel(32) == 15);  // Center
    REQUIRE(buffer.getLevel(31) > 0);    // Adjacent
    REQUIRE(buffer.getLevel(33) > 0);    // Adjacent
  }

  SECTION("set range")
  {
    ArcRingBuffer buffer;

    // Range from 0.0 to 0.5 should fill LEDs 0-32
    buffer.setRange(0.0f, 0.5f, 10);
    REQUIRE(buffer.getLevel(0) == 10);
    REQUIRE(buffer.getLevel(16) == 10);
    REQUIRE(buffer.getLevel(32) == 10);
    REQUIRE(buffer.getLevel(48) == 0);
  }
}

// ============================================================================
// MonomeDeviceInfo Tests
// ============================================================================

TEST_CASE("madronalib/serialosc/device_info", "[serialosc]")
{
  SECTION("parse grid type")
  {
    MonomeDeviceInfo info;
    info.type = TextFragment("monome 128");
    info.parseType();

    REQUIRE(info.isGrid() == true);
    REQUIRE(info.isArc() == false);
    REQUIRE(info.deviceType == MonomeDeviceType::Grid);
  }

  SECTION("parse arc type")
  {
    MonomeDeviceInfo info;
    info.type = TextFragment("monome arc 4");
    info.parseType();

    REQUIRE(info.isGrid() == false);
    REQUIRE(info.isArc() == true);
    REQUIRE(info.deviceType == MonomeDeviceType::Arc);
    REQUIRE(info.encoderCount == 4);
  }

  SECTION("parse arc 2 type")
  {
    MonomeDeviceInfo info;
    info.type = TextFragment("monome arc 2");
    info.parseType();

    REQUIRE(info.isArc() == true);
    REQUIRE(info.encoderCount == 2);
  }

  SECTION("unknown type")
  {
    MonomeDeviceInfo info;
    info.type = TextFragment("unknown device");
    info.parseType();

    REQUIRE(info.isGrid() == false);
    REQUIRE(info.isArc() == false);
    REQUIRE(info.deviceType == MonomeDeviceType::Unknown);
  }
}
