#pragma once
#include <Arduino.h>

enum class GpsStatus { NoData, CheckingLink, WaitingForFix, Fix };
struct GpsReading {
  GpsStatus status = GpsStatus::NoData;
  bool speedValid = false;
  float knots = 0.0f;
};

void beginGps();
void serviceGps();
GpsReading getGpsReading(unsigned long now);
