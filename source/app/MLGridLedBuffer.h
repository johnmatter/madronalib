// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLGridLedBuffer.h
// LED state buffer for monome grid devices with dirty tracking for efficient updates.

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace ml
{

class GridLedBuffer
{
 public:
  static constexpr int kMaxWidth = 16;
  static constexpr int kMaxHeight = 16;
  static constexpr int kQuadrantSize = 8;
  static constexpr int kMaxLevel = 15;

  GridLedBuffer(int width = 16, int height = 8) : width_(width), height_(height)
  {
    levels_.fill(0);
  }

  // Dimensions
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }

  // === Level-based access (0-15) ===

  void setLevel(int x, int y, int level)
  {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int idx = y * kMaxWidth + x;
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    if (levels_[idx] != clampedLevel)
    {
      levels_[idx] = clampedLevel;
      markDirty(x, y);
    }
  }

  int getLevel(int x, int y) const
  {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return 0;
    return levels_[y * kMaxWidth + x];
  }

  // Fill all LEDs
  void fill(int level)
  {
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    for (int y = 0; y < height_; ++y)
    {
      for (int x = 0; x < width_; ++x)
      {
        int idx = y * kMaxWidth + x;
        if (levels_[idx] != clampedLevel)
        {
          levels_[idx] = clampedLevel;
          markDirty(x, y);
        }
      }
    }
  }

  // Fill rectangle
  void fillRect(int x0, int y0, int w, int h, int level)
  {
    uint8_t clampedLevel = static_cast<uint8_t>(std::clamp(level, 0, kMaxLevel));
    for (int y = y0; y < y0 + h && y < height_; ++y)
    {
      for (int x = x0; x < x0 + w && x < width_; ++x)
      {
        if (x >= 0 && y >= 0)
        {
          int idx = y * kMaxWidth + x;
          if (levels_[idx] != clampedLevel)
          {
            levels_[idx] = clampedLevel;
            markDirty(x, y);
          }
        }
      }
    }
  }

  // === Binary access (on/off, maps to level 0 or 15) ===

  void set(int x, int y, bool on) { setLevel(x, y, on ? kMaxLevel : 0); }
  bool get(int x, int y) const { return getLevel(x, y) > 0; }

  // Toggle a single LED
  void toggle(int x, int y) { set(x, y, !get(x, y)); }

  // === Dirty tracking ===

  bool isDirty() const { return dirtyMask_ != 0; }
  void clearDirty() { dirtyMask_ = 0; }

  // Check if specific quadrant is dirty (for 8x8 map optimization)
  // qx, qy are quadrant indices (0 or 1 for 16x16 grid)
  bool isQuadrantDirty(int qx, int qy) const
  {
    return (dirtyMask_ & (1 << quadrantIndex(qx, qy))) != 0;
  }

  // Get list of dirty quadrant coordinates
  std::vector<std::pair<int, int>> getDirtyQuadrants() const
  {
    std::vector<std::pair<int, int>> result;
    int numQuadsX = (width_ + kQuadrantSize - 1) / kQuadrantSize;
    int numQuadsY = (height_ + kQuadrantSize - 1) / kQuadrantSize;
    for (int qy = 0; qy < numQuadsY; ++qy)
    {
      for (int qx = 0; qx < numQuadsX; ++qx)
      {
        if (isQuadrantDirty(qx, qy))
        {
          result.emplace_back(qx, qy);
        }
      }
    }
    return result;
  }

  // === Data access for flush ===

  // Get 8x8 quadrant data for ledLevelMap command (64 bytes, row-major)
  std::array<uint8_t, 64> getQuadrantLevels(int qx, int qy) const
  {
    std::array<uint8_t, 64> result{};
    int xOffset = qx * kQuadrantSize;
    int yOffset = qy * kQuadrantSize;
    for (int row = 0; row < kQuadrantSize; ++row)
    {
      for (int col = 0; col < kQuadrantSize; ++col)
      {
        int x = xOffset + col;
        int y = yOffset + row;
        if (x < width_ && y < height_)
        {
          result[row * kQuadrantSize + col] = levels_[y * kMaxWidth + x];
        }
      }
    }
    return result;
  }

  // Get 8x8 quadrant as bitmask (8 bytes, one per row) for ledMap command
  std::array<uint8_t, 8> getQuadrantBitmask(int qx, int qy) const
  {
    std::array<uint8_t, 8> result{};
    int xOffset = qx * kQuadrantSize;
    int yOffset = qy * kQuadrantSize;
    for (int row = 0; row < kQuadrantSize; ++row)
    {
      uint8_t rowBits = 0;
      for (int col = 0; col < kQuadrantSize; ++col)
      {
        int x = xOffset + col;
        int y = yOffset + row;
        if (x < width_ && y < height_ && levels_[y * kMaxWidth + x] > 0)
        {
          rowBits |= (1 << col);
        }
      }
      result[row] = rowBits;
    }
    return result;
  }

  // Get row levels for ledLevelRow command
  void getRowLevels(int y, uint8_t* out, int count) const
  {
    if (y < 0 || y >= height_) return;
    for (int x = 0; x < count && x < width_; ++x)
    {
      out[x] = levels_[y * kMaxWidth + x];
    }
  }

  // Get column levels for ledLevelCol command
  void getColLevels(int x, uint8_t* out, int count) const
  {
    if (x < 0 || x >= width_) return;
    for (int y = 0; y < count && y < height_; ++y)
    {
      out[y] = levels_[y * kMaxWidth + x];
    }
  }

  // Clear all LEDs
  void clear() { fill(0); }

 private:
  int width_;
  int height_;

  // LED state: one byte per LED, row-major, max 16x16
  std::array<uint8_t, kMaxWidth * kMaxHeight> levels_{};

  // Dirty tracking: bitmask for 8x8 quadrants (up to 4 quadrants for 16x16)
  // Bit 0 = (0,0), Bit 1 = (1,0), Bit 2 = (0,1), Bit 3 = (1,1)
  uint8_t dirtyMask_{0};

  void markDirty(int x, int y)
  {
    int qx = x / kQuadrantSize;
    int qy = y / kQuadrantSize;
    dirtyMask_ |= (1 << quadrantIndex(qx, qy));
  }

  int quadrantIndex(int qx, int qy) const { return qy * 2 + qx; }
};

}  // namespace ml
