// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLMonomeArc.h
// Monome arc device support with ring LED buffers.

#pragma once

#include "MLMonomeDevice.h"
#include "MLArcRingBuffer.h"
#include <memory>
#include <vector>
#include <array>

namespace ml
{

class MonomeArc : public MonomeDevice
{
 public:
  static constexpr int kMaxEncoders = 4;
  static constexpr int kLedsPerRing = 64;

  explicit MonomeArc(const MonomeDeviceInfo& info);
  ~MonomeArc() override;

  // Arc properties
  int getEncoderCount() const { return info_.encoderCount > 0 ? info_.encoderCount : 4; }

  // === Raw Ring LED Commands ===

  // Single LED on ring n, position x (0-63), level 0-15
  void ringSet(int ring, int led, int level);

  // All LEDs on ring to same level
  void ringAll(int ring, int level);

  // Set all 64 LEDs on a ring via array
  void ringMap(int ring, const std::array<uint8_t, 64>& levels);

  // Range of LEDs (supports wrapping around 64)
  void ringRange(int ring, int start, int end, int level);

  // === Buffer-based Ring control ===

  // Get ring buffer for batch updates
  ArcRingBuffer& getRingBuffer(int ring);
  const ArcRingBuffer& getRingBuffer(int ring) const;

  // Flush all dirty ring buffers to device
  void flushRingBuffers();

  // Flush a specific ring buffer
  void flushRingBuffer(int ring);

  // Actor interface
  void onMessage(Message m) override;

 protected:
  void handleDeviceInput(const Path& address, const std::vector<Value>& args) override;

 private:
  // Handle arc-specific input
  void handleEncoderDelta(int encoder, int delta);
  void handleEncoderKey(int encoder, int state);

  // Build the OSC address with current prefix
  TextFragment prefixedAddress(const char* suffix) const;

  // Ring buffers (one per encoder)
  std::array<ArcRingBuffer, kMaxEncoders> ringBuffers_;
};

}  // namespace ml
