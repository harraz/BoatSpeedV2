#pragma once
#include <Arduino.h>
#include "gps.h"

// Data passed to the screen; drawing does not alter sailing calculations.
enum class DisplayMode {
  SelectTack, SetReference, CompassLost, OnReference, Pending, Lifted, Headed
};

struct DisplayState {
  const char *tackName = "SELECT TACK";
  DisplayMode mode = DisplayMode::CompassLost;
  bool headingValid = false;
  bool referenceSaved = false;
  float heading = 0.0f;
  float reference = 0.0f;
  float change = 0.0f; // Positive = lifted, independent of tack.
  float neutralDegrees = 5.0f;
  GpsReading gps;
};

bool beginDisplay();
void serviceDisplay(const DisplayState &state);
