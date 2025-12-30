// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLArcRingBuffer.h
// LED state buffer for a single monome arc ring with dirty tracking.

#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace ml
{

class ArcRingBuffer
{
 public:
  static constexpr int kLedCount = 64;
  static constexpr int kMaxLevel = 15;

  ArcRingBuffer() { levels_.fill(0); }

  // === Level access (0-15) ===

  void setLevel(int led, int level)
  {
    if (led < 0 || led >= kLedCount) return;
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    if (levels_[led] != clampedLevel)
    {
      levels_[led] = clampedLevel;
      dirty_ = true;
    }
  }

  int getLevel(int led) const
  {
    if (led < 0 || led >= kLedCount) return 0;
    return levels_[led];
  }

  // Fill all LEDs with same level
  void fill(int level)
  {
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    for (int i = 0; i < kLedCount; ++i)
    {
      if (levels_[i] != clampedLevel)
      {
        levels_[i] = clampedLevel;
        dirty_ = true;
      }
    }
  }

  // Fill range of LEDs (supports wrapping around 64)
  // If start > end, wraps around (e.g., start=60, end=4 fills 60-63 and 0-4)
  void fillRange(int start, int end, int level)
  {
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    start = ((start % kLedCount) + kLedCount) % kLedCount;
    end = ((end % kLedCount) + kLedCount) % kLedCount;

    if (start <= end)
    {
      for (int i = start; i <= end; ++i)
      {
        if (levels_[i] != clampedLevel)
        {
          levels_[i] = clampedLevel;
          dirty_ = true;
        }
      }
    }
    else
    {
      // Wrap around
      for (int i = start; i < kLedCount; ++i)
      {
        if (levels_[i] != clampedLevel)
        {
          levels_[i] = clampedLevel;
          dirty_ = true;
        }
      }
      for (int i = 0; i <= end; ++i)
      {
        if (levels_[i] != clampedLevel)
        {
          levels_[i] = clampedLevel;
          dirty_ = true;
        }
      }
    }
  }

  // === Dirty tracking ===

  bool isDirty() const { return dirty_; }
  void clearDirty() { dirty_ = false; }

  // === Data access ===

  // Get all 64 levels for ringMap command
  std::array<uint8_t, kLedCount> getAllLevels() const { return levels_; }

  // Get pointer to level data
  const uint8_t* data() const { return levels_.data(); }

  // === Convenience patterns ===

  // Set a position indicator - single bright LED at normalized position (0.0-1.0)
  // with optional falloff to adjacent LEDs
  void setPosition(float normalizedPosition, int brightness = kMaxLevel, int falloff = 2)
  {
    clear();
    // Wrap position to 0.0-1.0 range
    normalizedPosition = normalizedPosition - std::floor(normalizedPosition);
    int centerLed = static_cast<int>(normalizedPosition * kLedCount) % kLedCount;

    setLevel(centerLed, brightness);

    // Add falloff to adjacent LEDs
    for (int i = 1; i <= falloff; ++i)
    {
      int dimLevel = brightness * (falloff - i + 1) / (falloff + 2);
      int prevLed = (centerLed - i + kLedCount) % kLedCount;
      int nextLed = (centerLed + i) % kLedCount;
      setLevel(prevLed, dimLevel);
      setLevel(nextLed, dimLevel);
    }
  }

  // Set a range indicator - arc from startNorm to endNorm (both 0.0-1.0)
  void setRange(float startNorm, float endNorm, int level = kMaxLevel)
  {
    clear();
    // Wrap to 0.0-1.0 range
    startNorm = startNorm - std::floor(startNorm);
    endNorm = endNorm - std::floor(endNorm);

    int startLed = static_cast<int>(startNorm * kLedCount) % kLedCount;
    int endLed = static_cast<int>(endNorm * kLedCount) % kLedCount;

    fillRange(startLed, endLed, level);
  }

  // Clear all LEDs
  void clear() { fill(0); }

 private:
  std::array<uint8_t, kLedCount> levels_{};
  bool dirty_{false};
};

}  // namespace ml
