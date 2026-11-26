#include <Arduino.h>
#include <Wire.h>
#include "display.h"
#include <math.h>

constexpr uint8_t ADDRESS = 0x0E;
bool ready = false;  // check that the compass is responding

// Inclusive neutral band: -5 through +5 degrees.
constexpr float kNeutralDegrees = 5.0f;
constexpr unsigned long kShiftPersistenceMs = 2000;

enum class ShiftStatus {
  OnReference,
  Pending,
  Lifted,
  Headed
};

// Stored separately from printing so a future display can use the same result.
ShiftStatus shiftStatus = ShiftStatus::OnReference;
ShiftStatus candidateShift = ShiftStatus::OnReference;
unsigned long candidateSince = 0;

void resetShiftTracking() {
  shiftStatus = ShiftStatus::OnReference;
  candidateShift = ShiftStatus::OnReference;
  candidateSince = 0;
}

void updateShiftStatus(float change, unsigned long now) {
  ShiftStatus requested = ShiftStatus::OnReference;
  if (change > kNeutralDegrees) requested = ShiftStatus::Lifted;
  else if (change < -kNeutralDegrees) requested = ShiftStatus::Headed;

  // Returning inside the neutral band immediately cancels a pending shift.
  if (requested == ShiftStatus::OnReference) {
    resetShiftTracking();
    return;
  }

  // A new direction must persist from its first qualifying measurement.
  if (requested != candidateShift) {
    candidateShift = requested;
    candidateSince = now;
  }

  shiftStatus = now - candidateSince >= kShiftPersistenceMs
                    ? requested : ShiftStatus::Pending;
}


float currentHeading = 0.0f;
bool headingAvailable = false;
unsigned long headingUpdatedAt = 0;

struct TackReference {
  float heading;       // Saved reference in degrees
  const char *name;    // Label for printed messages
  bool saved;          // Has a reference been captured?
};

TackReference portTack{0.0f, "Port", false};
TackReference starboardTack{0.0f, "Starboard", false};

// Selecting a tack does not change either saved reference.
TackReference *activeTack = nullptr;

// Each physical button has one explicit job.
enum class ButtonAction {
  SelectPort,
  SelectStarboard,
  SaveReference
};

// Accept a change only after it remains steady for 25 ms.
// This filters the rapid electrical changes caused by switch contact bounce.
struct Button {
  uint8_t pin;
  ButtonAction action; // Replaces the old TackReference *tack member.
  bool lastReading = HIGH;
  bool stableReading = HIGH;
  unsigned long changedAt = 0;
};

Button portButton{A0, ButtonAction::SelectPort};
Button starboardButton{D0, ButtonAction::SelectStarboard};
Button referenceButton{D3, ButtonAction::SaveReference};

// Decide what a confirmed press does; keep this separate from debounce.
void handleButtonPress(ButtonAction action) {
  switch (action) {
    case ButtonAction::SelectPort:
      activeTack = &portTack;
      resetShiftTracking();
      break;

    case ButtonAction::SelectStarboard:
      activeTack = &starboardTack;
      resetShiftTracking();
      break;

    case ButtonAction::SaveReference:
      if (activeTack == nullptr) {
        Serial.println("Reference not saved: select Port or Starboard first.");
        return;
      }
      if (!headingAvailable || millis() - headingUpdatedAt >= 1000) {
        Serial.println("Reference not saved: no recent compass reading.");
        return;
      }

      // Only the reference button writes a tack's saved heading.
      activeTack->heading = currentHeading;
      activeTack->saved = true;
      resetShiftTracking();
      Serial.printf("%s reference saved: %.1f degrees\n",
                    activeTack->name, activeTack->heading);
      return;
  }

  // Selection feedback; keep any previously saved reference for this tack.
  Serial.printf("%s selected\n", activeTack->name);
  if (!activeTack->saved) {
    Serial.println("SET REFERENCE: settle close-hauled, then press Reference.");
  }
}

void checkButton(Button &button) {
  const unsigned long now = millis();
  bool reading;
  if (button.pin == A0) {
    // A0: 10k to GND; button connects to 3.3V through another 10k.
    // Released is near 0; pressed is around half-scale. Separate thresholds
    // prevent small voltage changes from repeatedly changing the state.
    static unsigned long lastAnalogAt = 0;
    if (now - lastAnalogAt < 5) return;
    lastAnalogAt = now;
    const int value = analogRead(A0);
    reading = button.lastReading;
    if (value >= 350) reading = LOW;  // Pressed.
    else if (value <= 150) reading = HIGH; // Released.
  } else {
    reading = digitalRead(button.pin);
  }

  if (reading != button.lastReading) {
    button.lastReading = reading;
    button.changedAt = now;
  }

  if (reading != button.stableReading && now - button.changedAt >= 25) {
    button.stableReading = reading;
    if (reading == LOW) {
      handleButtonPress(button.action);
    }
  }
}

void checkButtons() {
  checkButton(portButton);
  checkButton(starboardButton);
  checkButton(referenceButton);
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  Wire.write(value);

  uint8_t error = Wire.endTransmission();
  // Serial.printf("Write register 0x%02X: result %u\n", reg, error);

  return error == 0;
}

bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDRESS, count) != count) return false;

  for (uint8_t i = 0; i < count; i++) {
    data[i] = Wire.read();
  }
  return true;
}

int16_t signedValue(uint8_t low, uint8_t high) {
  uint16_t value = uint16_t(low) | (uint16_t(high) << 8);
  return value < 32768 ? int16_t(value)
                       : int16_t(int32_t(value) - 65536);
}

void setup() {
  // Initialize buttons even if compass initialization later fails.
  // Port button uses analog input A0 and its external resistor divider.
  pinMode(D0, INPUT);        // Starboard: external 10 kohm pull-up to 3.3 V.
  pinMode(D3, INPUT_PULLUP); // Reference: button to GND; release during power-on/reset.
  Serial.begin(115200);
  delay(2000);

  beginDisplay();
  // Start GPS after TFT: SPI initialization also configures D6/MISO.
  // The TFT is write-only; D6 is now the GPS receive input.
  beginGps();

  Wire.begin(D2, D1); // SDA, SCL
  Wire.setClock(10000); // Slower speed for diagnosis

  // First check whether the address responds.
  Wire.beginTransmission(ADDRESS);
  uint8_t result = Wire.endTransmission();
  // Serial.printf("Address 0x%02X: result %u\n", ADDRESS, result);

  if (result != 0) {
    Serial.println("Address check failed.");
    return;
  }

  // Read identification without issuing a reset.
  uint8_t id;
  if (!readRegisters(0x00, &id, 1)) {
    Serial.println("Chip ID read failed.");
    return;
  }

  // Serial.printf("Chip ID: 0x%02X (expected 0x10)\n", id);

  if (id != 0x10) {
    Serial.println("Unexpected chip ID.");
    return;
  }

  ready = true;
  // Serial.println("IST8310 identified.");


}

float headingDifference(float current, const TackReference &reference) {
  float difference = current - reference.heading;

  if (difference > 180.0f) {
    difference -= 360.0f;
  } else if (difference < -180.0f) {
    difference += 360.0f;
  }

  return difference;
}

// Compass work is split into short steps rather than waiting in a while loop.
// millis() lets us ask "is it time yet?" without pausing button checks.
void updateCompass() {
  static bool waitingForMeasurement = false;
  static unsigned long requestedAt = 0;
  static unsigned long lastStatusCheck = 0;
  static unsigned long lastCycleAt = 0;
  static bool firstCycle = true;

  if (!ready) return;
  const unsigned long now = millis();

  if (!waitingForMeasurement) {
    if (!firstCycle && now - lastCycleAt < 500) return;
    firstCycle = false;
    lastCycleAt = now;

    if (!writeRegister(0x0A, 0x01)) {
      Serial.println("Measurement request failed.");
      headingAvailable = false;
      resetShiftTracking();
      return;
    }

    requestedAt = millis();
    lastStatusCheck = requestedAt;
    waitingForMeasurement = true;
    return; // Check buttons again instead of waiting for the compass.
  }

  if (now - requestedAt >= 100) {
    Serial.println("Measurement timed out.");
    headingAvailable = false;
    resetShiftTracking();
    waitingForMeasurement = false;
    return;
  }

  // Check status every 5 ms, leaving the bus idle between checks.
  if (now - lastStatusCheck < 5) return;
  lastStatusCheck = now;

  uint8_t status;
  if (!readRegisters(0x02, &status, 1)) {
    Serial.println("Compass status read failed.");
    headingAvailable = false;
    resetShiftTracking();
    waitingForMeasurement = false;
    return;
  }
  if (!(status & 0x01)) return; // Not ready; try on a later loop pass.

  waitingForMeasurement = false;
  uint8_t data[6];
  if (!readRegisters(0x03, data, 6)) {
    Serial.println("Compass measurement read failed.");
    headingAvailable = false;
    resetShiftTracking();
    return;
  }

  const int16_t x = signedValue(data[0], data[1]);
  const int16_t y = signedValue(data[2], data[3]);
  // const int16_t z = signedValue(data[4], data[5]); // Restore with X/Y/Z debug print.

  // Serial.printf("X: %d   Y: %d   Z: %d\n", x, y, z);
  if (x == 0 && y == 0) {
    Serial.println("Sensor angle unavailable: X and Y are zero.");
    headingAvailable = false;
    resetShiftTracking();
    return;
  }

  float degrees = float(atan2(y, x) * 180.0 / PI);
  if (degrees < 0) degrees += 360.0f;

  currentHeading = degrees;
  headingUpdatedAt = millis();
  headingAvailable = true;
  
  // Serial.printf("Heading: %.1f degrees\n", degrees);

  if (activeTack != nullptr && activeTack->saved) {
    float change = headingDifference(currentHeading, *activeTack);

    // With clockwise-increasing angles, positive means lifted on port.
    // Reverse the sign on starboard so the same label rules apply.
    if (activeTack == &starboardTack) {
      change = -change;
    }

    updateShiftStatus(change, headingUpdatedAt);

    const char *status;
    switch (shiftStatus) {
      case ShiftStatus::Lifted: status = "LIFTED"; break;
      case ShiftStatus::Headed: status = "HEADED"; break;
      case ShiftStatus::Pending: status = "CHANGE PENDING"; break;
      default: status = "ON REFERENCE"; break;
    }

    // The label supplies the direction; display only the positive magnitude.
    Serial.printf("%s: %s %.1f degrees\n",
                  activeTack->name, status, fabsf(change));
  }
}

// Build display data from the same compass/reference state used by Serial.
// GPS freshness is independent of compass availability.
DisplayState buildDisplayState(unsigned long now) {
  DisplayState screen;
  screen.gps = getGpsReading(now);
  screen.neutralDegrees = kNeutralDegrees;
  screen.tackName = activeTack == nullptr ? "SELECT TACK" : activeTack->name;
  screen.referenceSaved = activeTack != nullptr && activeTack->saved;
  screen.headingValid = headingAvailable && now - headingUpdatedAt < 1000;
  screen.heading = currentHeading;

  if (!ready || !screen.headingValid) {
    screen.mode = DisplayMode::CompassLost;
  } else if (activeTack == nullptr) {
    screen.mode = DisplayMode::SelectTack;
  } else if (!activeTack->saved) {
    screen.mode = DisplayMode::SetReference;
  } else {
    screen.reference = activeTack->heading;
    screen.change = headingDifference(currentHeading, *activeTack);
    if (activeTack == &starboardTack) screen.change = -screen.change;

    // A just-selected reference may not have had a new compass sample yet.
    // Show pending instead of falsely declaring a large change neutral.
    switch (shiftStatus) {
      case ShiftStatus::Lifted: screen.mode = DisplayMode::Lifted; break;
      case ShiftStatus::Headed: screen.mode = DisplayMode::Headed; break;
      case ShiftStatus::Pending: screen.mode = DisplayMode::Pending; break;
      default:
        screen.mode = fabsf(screen.change) <= kNeutralDegrees
                          ? DisplayMode::OnReference : DisplayMode::Pending;
        break;
    }
  }
  return screen;
}

void loop() {
  serviceGps();
  checkButtons();
  updateCompass();
  serviceGps();
  checkButtons();
  serviceDisplay(buildDisplayState(millis()));
  serviceGps();
  checkButtons();
  yield(); // Let the ESP8266 service its background work.
}
