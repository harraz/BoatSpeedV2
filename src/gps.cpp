#include "gps.h"
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

namespace {
SoftwareSerial gpsSerial;
TinyGPSPlus gps;
// Receive only: FW10 TX -> D6. No commands are sent to the GPS.
constexpr unsigned long baudRates[] = {115200, 9600, 38400, 57600, 19200};
constexpr unsigned long freshMs = 3000;
constexpr unsigned long baudTrialMs = 5000;
constexpr unsigned long linkTimeoutMs = 10000;
unsigned int baudIndex = 0;
unsigned long baudStartedAt = 0;
unsigned long lastByteAt = 0;
unsigned long lastSentenceAt = 0;
unsigned long trialChecksums = 0;
unsigned long lastLogAt = 0;
unsigned long overflowCount = 0;
bool receivedBytes = false;
bool baudLocked = false;

void startBaud() {
  gpsSerial.end();
  // Buffer incoming edges while a screen strip is transferred over SPI.
  gpsSerial.begin(baudRates[baudIndex], SWSERIAL_8N1, D6, -1, false, 256, 2048);
  baudStartedAt = millis();
  trialChecksums = gps.passedChecksum();
  baudLocked = false;
  receivedBytes = false;
  Serial.printf("GPS: trying %lu baud on D6\n", baudRates[baudIndex]);
}
} // namespace

void beginGps() {
  startBaud(); // Call AFTER TFT initialization, which also configures D6.
}

GpsReading getGpsReading(unsigned long now) {
  GpsReading reading;
  if (!receivedBytes || now - lastByteAt >= freshMs) return reading;
  if (!baudLocked || now - lastSentenceAt >= freshMs) {
    reading.status = GpsStatus::CheckingLink;
    return reading;
  }
  reading.status = GpsStatus::WaitingForFix;
  // Valid position and speed must both be recent. Bytes alone are not a fix.
  if (gps.location.isValid() && gps.location.age() < freshMs &&
      gps.speed.isValid() && gps.speed.age() < freshMs) {
    const double knots = gps.speed.knots();
    if (isfinite(knots) && knots >= 0.0 && knots <= 99.9) {
      reading.speedValid = true;
      reading.knots = float(knots);
      reading.status = GpsStatus::Fix;
    }
  }
  return reading;
}

void serviceGps() {
  // Bound each pass so heavy GPS output cannot starve the buttons.
  unsigned int budget = 256;
  while (budget-- && gpsSerial.available()) {
    const int data = gpsSerial.read();
    if (data < 0) break;
    receivedBytes = true;
    lastByteAt = millis();
    // encode() returns true only for a complete sentence with a good checksum.
    if (gps.encode(char(data))) {
      lastSentenceAt = millis();
      // Require two valid sentences before accepting a baud rate.
      if (!baudLocked && gps.passedChecksum() - trialChecksums >= 2) {
        baudLocked = true;
        Serial.printf("GPS: NMEA detected at %lu baud\n", baudRates[baudIndex]);
      }
    }
  }
  if (gpsSerial.overflow()) ++overflowCount;

  const unsigned long now = millis();
  if ((!baudLocked && now - baudStartedAt >= baudTrialMs) ||
      (baudLocked && now - lastSentenceAt >= linkTimeoutMs)) {
    baudIndex = (baudIndex + 1) % (sizeof(baudRates) / sizeof(baudRates[0]));
    startBaud();
  }

  // One concise diagnostic per two seconds; no raw GPS stream on USB.
  if (now - lastLogAt >= 2000) {
    lastLogAt = now;
    const GpsReading reading = getGpsReading(now);
    const char *status = "no serial data";
    if (reading.status == GpsStatus::CheckingLink) status = "checking baud / NMEA output";
    if (reading.status == GpsStatus::WaitingForFix) status = "waiting for fix";
    if (reading.status == GpsStatus::Fix) status = "fix";
    Serial.printf("GPS: %s; baud=%lu; valid=%lu; errors=%lu; overflows=%lu",
                  status, baudRates[baudIndex],
                  static_cast<unsigned long>(gps.passedChecksum()),
                  static_cast<unsigned long>(gps.failedChecksum()), overflowCount);
    if (reading.speedValid) Serial.printf("; SOG=%.1f kn", reading.knots);
    Serial.println();
  }
}
