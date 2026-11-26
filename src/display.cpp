#include "display.h"
#include <TFT_eSPI.h>
// #include <math.h>
#include <string.h>

namespace {
TFT_eSPI tft;
TFT_eSprite strip(&tft);
constexpr int width = 480;
constexpr int height = 320;
constexpr int stripHeight = 16;
constexpr int centerX = 165;
constexpr int centerY = 222;
constexpr int radius = 116;
constexpr float gaugeLimit = 30.0f;
constexpr uint16_t background = TFT_WHITE;
constexpr uint16_t ink = TFT_BLACK;
constexpr uint16_t muted = 0x52EC;
constexpr uint16_t track = 0xDEFB;
constexpr uint16_t neutral = 0xADB7;
constexpr uint16_t lifted = 0x03A9;
constexpr uint16_t headed = 0xB0E5;
constexpr uint16_t pending = 0x9BE0;

bool available = false;
bool painting = false;
bool havePrevious = false;
int stripY = 0;
unsigned long lastFrameAt = 0;
DisplayState frame;
DisplayState previous;

bool sameState(const DisplayState &a, const DisplayState &b) {
  return strcmp(a.tackName, b.tackName) == 0 && a.mode == b.mode &&
         a.headingValid == b.headingValid && a.referenceSaved == b.referenceSaved &&
         a.heading == b.heading && a.reference == b.reference &&
         a.change == b.change && a.neutralDegrees == b.neutralDegrees &&
         a.gps.status == b.gps.status && a.gps.speedValid == b.gps.speedValid &&
         a.gps.knots == b.gps.knots;
}

float angle(float change) {
  return (-90.0f + change * 90.0f / gaugeLimit) * PI / 180.0f;
}

// Coordinates are full-screen coordinates, shifted into the current strip.
void line(int x1, int y1, int x2, int y2, uint16_t color) {
  if (max(y1, y2) < stripY || min(y1, y2) >= stripY + stripHeight) return;
  strip.drawLine(x1, y1 - stripY, x2, y2 - stripY, color);
}

void triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t color) {
  if (max(y1, max(y2, y3)) < stripY ||
      min(y1, min(y2, y3)) >= stripY + stripHeight) return;
  strip.fillTriangle(x1, y1 - stripY, x2, y2 - stripY, x3, y3 - stripY, color);
}

void text(const char *value, int center, int y, int font, int size,
          uint16_t color = ink) {
  strip.setTextFont(font);
  strip.setTextSize(size);
  if (y + strip.fontHeight() <= stripY || y >= stripY + stripHeight) return;
  strip.setTextColor(color, background);
  strip.drawCentreString(value, center, y - stripY, font);
}

void degreeText(const char *value, int center, int y, int font, int size,
                uint16_t color = ink) {
  strip.setTextFont(font);
  strip.setTextSize(size);
  const int textWidth = strip.textWidth(value, font);
  const int dotRadius = size == 3 ? 5 : 3;
  const int extra = dotRadius * 2 + 5;
  const int left = center - (textWidth + extra) / 2;
  if (y + strip.fontHeight() <= stripY || y >= stripY + stripHeight) return;
  strip.setTextColor(color, background);
  strip.drawString(value, left, y - stripY, font);
  strip.drawCircle(left + textWidth + 4 + dotRadius,
                   y + dotRadius + 3 - stripY, dotRadius, color);
}

uint16_t statusColor(DisplayMode mode) {
  if (mode == DisplayMode::Lifted) return lifted;
  if (mode == DisplayMode::Headed || mode == DisplayMode::CompassLost) return headed;
  if (mode == DisplayMode::Pending || mode == DisplayMode::SetReference) return pending;
  return ink;
}

void gauge() {
  // Two filled triangles form each small piece of the semicircular band.
  for (int step = -30; step < 30; ++step) {
    const float a = angle(step);
    const float b = angle(step + 1);
    const uint16_t color = fabsf(step + 0.5f) <= frame.neutralDegrees ? neutral : track;
    const int x1 = centerX + lroundf(cosf(a) * radius);
    const int y1 = centerY + lroundf(sinf(a) * radius);
    const int x2 = centerX + lroundf(cosf(b) * radius);
    const int y2 = centerY + lroundf(sinf(b) * radius);
    const int x3 = centerX + lroundf(cosf(b) * (radius - 12));
    const int y3 = centerY + lroundf(sinf(b) * (radius - 12));
    const int x4 = centerX + lroundf(cosf(a) * (radius - 12));
    const int y4 = centerY + lroundf(sinf(a) * (radius - 12));
    triangle(x1, y1, x2, y2, x3, y3, color);
    triangle(x1, y1, x3, y3, x4, y4, color);
  }
  for (int tick = -30; tick <= 30; tick += 5) {
    const float a = angle(tick);
    const int outer = radius + (tick % 10 == 0 ? 10 : 5);
    const int x1 = centerX + lroundf(cosf(a) * (radius - 12));
    const int y1 = centerY + lroundf(sinf(a) * (radius - 12));
    const int x2 = centerX + lroundf(cosf(a) * outer);
    const int y2 = centerY + lroundf(sinf(a) * outer);
    line(x1, y1, x2, y2, ink);
    if (tick == 0) line(x1 + 1, y1, x2 + 1, y2, ink);
  }
  degreeText("0", centerX, 79, 2, 1);
  degreeText("30", 35, 235, 2, 1, muted);
  degreeText("30", 295, 235, 2, 1, muted);
}

void pointer(uint16_t color) {
  // The gauge stops at +/-30, while the numeric field retains the full amount.
  const float bounded = fmaxf(-gaugeLimit, fminf(gaugeLimit, frame.change));
  const float a = angle(bounded);
  const float c = cosf(a), s = sinf(a);
  triangle(centerX + lroundf(c * 135), centerY + lroundf(s * 135),
           centerX + lroundf(c * 97 + s * 9), centerY + lroundf(s * 97 - c * 9),
           centerX + lroundf(c * 97 - s * 9), centerY + lroundf(s * 97 + c * 9), color);
}

void paintStrip() {
  strip.fillSprite(background);
  char label[48];
  text(frame.tackName, 95, 13, 4, 1);
  if (frame.headingValid) {
    const int roundedHeading = int(lroundf(frame.heading)) % 360;
    snprintf(label, sizeof(label), "HDG %03d M", roundedHeading);
  } else {
    snprintf(label, sizeof(label), "HDG --- M");
  }
  text(label, 366, 13, 4, 1);
  text("SHIFT FROM REFERENCE", centerX, 55, 2, 1, muted);
  line(331, 58, 331, 294, track);
  gauge();

  text("SOG", 405, 90, 4, 1, muted);
  if (frame.gps.speedValid) snprintf(label, sizeof(label), "%.1f", frame.gps.knots);
  else snprintf(label, sizeof(label), "--.-");
  text(label, 405, 137, 4, 2);
  text("kn", 405, 197, 4, 1, muted);
  const char *gpsLabel = "NO GPS DATA";
  switch (frame.gps.status) {
    case GpsStatus::CheckingLink: gpsLabel = "CHECK GPS LINK"; break;
    case GpsStatus::WaitingForFix: gpsLabel = "WAITING FIX"; break;
    case GpsStatus::Fix: gpsLabel = "GPS FIX"; break;
    default: break;
  }
  text(gpsLabel, 405, 235, 2, 1, muted);
  snprintf(label, sizeof(label), "+/- %.0f NEUTRAL", frame.neutralDegrees);
  text(label, 405, 268, 2, 1, muted);

  const uint16_t color = statusColor(frame.mode);
  const bool comparison = frame.mode == DisplayMode::OnReference ||
      frame.mode == DisplayMode::Pending || frame.mode == DisplayMode::Lifted ||
      frame.mode == DisplayMode::Headed;

  const char *status;
  const char *hint = "";
  switch (frame.mode) {
    case DisplayMode::SelectTack:
      status = "SELECT TACK"; hint = "Press Port or Starboard"; break;
    case DisplayMode::SetReference:
      status = "SET REFERENCE"; hint = "Close-hauled? Press Reference"; break;
    case DisplayMode::CompassLost:
      status = "COMPASS LOST"; hint = "Check compass power and connections"; break;
    case DisplayMode::Pending:
      status = "CHANGE PENDING"; break;
    case DisplayMode::Lifted: status = "LIFTED"; break;
    case DisplayMode::Headed: status = "HEADED"; break;
    default: status = "ON REFERENCE"; break;
  }

  if (comparison) {
    pointer(color);
    snprintf(label, sizeof(label), "%.1f", fabsf(frame.change));
    degreeText(label, centerX, 157, 4, 3);
    if (fabsf(frame.change) > gaugeLimit) text("OFF SCALE", centerX, 242, 2, 1, muted);
  } else {
    text("--", centerX, 157, 4, 3, muted);
  }
  text(status, centerX, 269, 4, 1, color);
  text(hint, width / 2, 303, 2, 1, muted);
}
} // namespace

bool beginDisplay() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(background);
  strip.setColorDepth(16);
  available = strip.createSprite(width, stripHeight) != nullptr;
  if (!available) {
    tft.setTextColor(TFT_RED, background);
    tft.drawCentreString("Display buffer unavailable", width / 2, 140, 2);
    Serial.println("Display buffer allocation failed.");
  }
  return available;
}

void serviceDisplay(const DisplayState &state) {
  if (!available) return;
  if (!painting) {
    if (havePrevious && sameState(state, previous)) return;
    if (havePrevious && millis() - lastFrameAt < 100) return;
    frame = state; // Snapshot stays consistent while the strips are drawn.
    stripY = 0;
    painting = true;
    lastFrameAt = millis();
  }

  // Just 480 x 16 x 2 = 15360 bytes, rather than a 307200-byte framebuffer.
  // One strip per loop: buttons and compass run between SPI transfers.
  paintStrip();
  strip.pushSprite(0, stripY);
  stripY += stripHeight;
  if (stripY >= height) {
    previous = frame;
    havePrevious = true;
    painting = false;
  }
}
