// madronalib: a C++ framework for DSP applications.
// Copyright (c) 2020-2025 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

// MLSerialOsc.h
// Main include header for monome serialosc integration.
// Provides communication with monome grid and arc devices via OSC.

#pragma once

#include "MLText.h"

namespace ml
{

// serialosc daemon default port
constexpr int kSerialOscPort = 12002;

// Default prefix for device messages
constexpr const char* kDefaultMonomePrefix = "/monome";

// Message flags for serialosc events
enum SerialOscFlags
{
  kMsgFromSerialOsc = 1 << 6,  // Message originated from serialosc/device
  kMsgDeviceEvent = 1 << 7     // Message is a device input event (key, encoder, etc.)
};

// Device type enumeration
enum class MonomeDeviceType
{
  Unknown,
  Grid,
  Arc
};

// Information about a connected monome device
struct MonomeDeviceInfo
{
  TextFragment id;           // Device serial number (e.g., "m0000123")
  TextFragment type;         // Device type string (e.g., "monome 128", "monome arc 4")
  int port{0};               // UDP port for device communication
  int width{0};              // Grid width (0 for arc)
  int height{0};             // Grid height (0 for arc)
  int encoderCount{0};       // Number of encoders (0 for grid, typically 2 or 4 for arc)
  MonomeDeviceType deviceType{MonomeDeviceType::Unknown};

  bool isGrid() const { return deviceType == MonomeDeviceType::Grid; }
  bool isArc() const { return deviceType == MonomeDeviceType::Arc; }

  // Parse device type from type string
  void parseType()
  {
    // Arc types: "monome arc 2", "monome arc 4"
    if (type.getText() && strstr(type.getText(), "arc"))
    {
      deviceType = MonomeDeviceType::Arc;
      // Try to parse encoder count from type string
      const char* arcStr = strstr(type.getText(), "arc");
      if (arcStr)
      {
        int count = 0;
        if (sscanf(arcStr, "arc %d", &count) == 1)
        {
          encoderCount = count;
        }
        else
        {
          encoderCount = 4;  // Default to 4 encoders
        }
      }
    }
    // Grid types: "monome 64", "monome 128", "monome 256", "monome grid"
    else if (type.getText() && strstr(type.getText(), "monome"))
    {
      deviceType = MonomeDeviceType::Grid;
      // Size will be determined by /sys/size response
    }
  }
};

// Forward declarations
class MonomeDevice;
class MonomeGrid;
class MonomeArc;
class SerialOscService;

}  // namespace ml

// Include the component headers
#include "MLGridLedBuffer.h"
#include "MLArcRingBuffer.h"
