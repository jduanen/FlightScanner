#include "ui/boot_screens.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"

namespace {

constexpr int kLineGap = 10;
const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;

constexpr int kSonarOuterRadius = 186;
constexpr int kSonarInnerRadius = 118;
constexpr int kSonarRingStepPx = 6;
constexpr uint16_t kSonarGuideColor = 0x0320;

char s_connecting_ssid[33];
char s_ssid_line[33];
char s_portal_ip_alt[32];
constexpr int kConnectingTextMaxWidthPx = 358;
int s_ping_radius = kSonarInnerRadius;
int s_ping_erase_radius = 0;
bool s_connecting_text_drawn = false;
bool s_sonar_guides_drawn = false;

struct TextLine {
  const char* text;
  UiTextStyle style;
};

void drawTextBlock(uint16_t bg, uint16_t fg, const TextLine* lines, size_t count) {
  tft.fillScreen(bg);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TextDatum::MiddleCenter);

  int total_h = 0;
  for (size_t i = 0; i < count; ++i) {
    total_h += displayFontHeight(tft, lines[i].style);
    if (i + 1 < count) {
      total_h += kLineGap;
    }
  }

  int y = (config::kDisplayHeight - total_h) / 2;
  for (size_t i = 0; i < count; ++i) {
    displayFontApply(tft, lines[i].style);
    const int h = displayFontHeight(tft, lines[i].style);
    tft.drawString(lines[i].text, kCenterX, y + h / 2);
    y += h + kLineGap;
  }
}

void fitSsidLine() {
  strncpy(s_ssid_line, s_connecting_ssid, sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
  displayFontApply(tft, displayFontDetail());
  if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
    return;
  }
  const size_t len = strlen(s_connecting_ssid);
  for (size_t n = len; n > 0; --n) {
    snprintf(s_ssid_line, sizeof(s_ssid_line), "%.*s…", static_cast<int>(n),
             s_connecting_ssid);
    if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
      return;
    }
  }
  strncpy(s_ssid_line, "…", sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
}

void drawConnectingText() {
  tft.fillScreen(config::kColorBlack);

  tft.setTextDatum(TextDatum::MiddleCenter);
  tft.setTextColor(config::kTextOnBlack, config::kColorBlack);

  displayFontApply(tft, displayFontDetail());
  const int detail_h = tft.fontHeight();
  const int total_h = detail_h * 2 + kLineGap;
  const int block_top = (config::kDisplayHeight - total_h) / 2;
  constexpr int kPanelPadY = 12;
  tft.fillRect(kCenterX - kConnectingTextMaxWidthPx / 2, block_top - kPanelPadY,
               kConnectingTextMaxWidthPx, total_h + kPanelPadY * 2, config::kColorBlack);

  int y = block_top;
  tft.drawString("Connecting to", kCenterX, y + detail_h / 2);
  y += detail_h + kLineGap;
  tft.drawString(s_ssid_line, kCenterX, y + detail_h / 2);

  s_connecting_text_drawn = true;
}

void drawSonarGuides() {
  tft.drawCircle(kCenterX, kCenterY, kSonarOuterRadius, kSonarGuideColor);
  tft.drawCircle(kCenterX, kCenterY, kSonarInnerRadius, kSonarGuideColor);
  s_sonar_guides_drawn = true;
}

void eraseSonarPing() {
  if (s_ping_erase_radius <= 0) {
    return;
  }
  tft.drawCircle(kCenterX, kCenterY, s_ping_erase_radius, config::kColorBlack);
  tft.drawCircle(kCenterX, kCenterY, s_ping_erase_radius - 1, config::kColorBlack);
  s_ping_erase_radius = 0;
}

void drawSonarPing() {
  const int span = kSonarOuterRadius - kSonarInnerRadius;
  const int phase = s_ping_radius - kSonarInnerRadius;
  const int fade = 96 + (159 * phase) / span;
  const uint16_t color = tft.color565(0, static_cast<uint8_t>(fade), 0);
  tft.drawCircle(kCenterX, kCenterY, s_ping_radius, color);
  s_ping_erase_radius = s_ping_radius;
}

void advanceSonarPing() {
  s_ping_radius += kSonarRingStepPx;
  if (s_ping_radius > kSonarOuterRadius) {
    s_ping_radius = kSonarInnerRadius;
  }
}

}  // namespace

void bootScreenConnectingStart(const char* ssid) {
  const char* name = (ssid != nullptr && ssid[0] != '\0') ? ssid : "network";
  strncpy(s_connecting_ssid, name, sizeof(s_connecting_ssid) - 1);
  s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
  fitSsidLine();
  s_ping_radius = kSonarInnerRadius;
  s_ping_erase_radius = 0;
  s_connecting_text_drawn = false;
  s_sonar_guides_drawn = false;
  drawConnectingText();
  drawSonarGuides();
  drawSonarPing();
}

void bootScreenConnectingPulse() {
  if (!s_connecting_text_drawn) {
    drawConnectingText();
    if (!s_sonar_guides_drawn) {
      drawSonarGuides();
    }
  }
  eraseSonarPing();
  advanceSonarPing();
  drawSonarPing();
}

void bootScreenShowPortalHint() {
  snprintf(s_portal_ip_alt, sizeof(s_portal_ip_alt), "or %s", config::kPortalIp);
  const TextLine lines[] = {
      {"Network setup", displayFontTitle()},
      {"1. Join network:", displayFontBody()},
      {config::kPortalApName, displayFontTitle()},
      {"2. Open in browser:", displayFontBody()},
      {config::kPortalHostUrl, displayFontTitle()},
      {s_portal_ip_alt, displayFontBody()},
  };
  drawTextBlock(config::kColorBlack, config::kTextOnBlack, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void bootScreenShowConnectFailed() {
  const TextLine lines[] = {
      {"Could not connect", displayFontTitle()},
      {"Check Wi-Fi password", displayFontBody()},
      {"and signal strength.", displayFontBody()},
      {"Hold knob 3 sec", displayFontBody()},
      {"to reset Wi-Fi", displayFontBody()},
  };
  drawTextBlock(config::kColorBlack, config::kTextOnBlack, lines,
                sizeof(lines) / sizeof(lines[0]));
}

void bootScreenShowWifiCleared() {
  const TextLine lines[] = {
      {"Resetting Wi-Fi", displayFontTitle()},
  };
  drawTextBlock(config::kColorBlack, config::kTextOnBlack, lines,
                sizeof(lines) / sizeof(lines[0]));
}
