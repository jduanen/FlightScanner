#include "hardware/display_blanking.h"

#include <Preferences.h>
#include <cstdlib>

#include "hardware/display.h"
#include "hardware/display_brightness.h"

namespace hardware {
namespace {

constexpr char kStoreNs[] = "flightscnr";
constexpr char kBlankTimeoutKey[] = "blank_to_s";
constexpr uint16_t kDefaultTimeoutSec = 60;
constexpr uint16_t kValidTimeouts[] = {0, 30, 60, 120, 300, 600};
constexpr size_t kValidTimeoutCount = sizeof(kValidTimeouts) / sizeof(kValidTimeouts[0]);

uint16_t s_timeout_sec = kDefaultTimeoutSec;
bool s_awake = true;
unsigned long s_last_activity_ms = 0;

bool isValidTimeout(uint16_t sec) {
  for (size_t i = 0; i < kValidTimeoutCount; ++i) {
    if (kValidTimeouts[i] == sec) {
      return true;
    }
  }
  return false;
}

}  // namespace

void displayBlankingBootLoad() {
  Preferences prefs;
  if (!prefs.begin(kStoreNs, true)) {
    return;
  }
  const uint16_t stored = prefs.getUShort(kBlankTimeoutKey, kDefaultTimeoutSec);
  prefs.end();
  s_timeout_sec = isValidTimeout(stored) ? stored : kDefaultTimeoutSec;
}

void displayBlankingTick(unsigned long now_ms) {
  if (!s_awake || s_timeout_sec == 0 || s_last_activity_ms == 0) {
    return;
  }
  if (now_ms - s_last_activity_ms >= static_cast<unsigned long>(s_timeout_sec) * 1000UL) {
    displayBlank();
  }
}

void displayBlankingNotifyActivity(unsigned long now_ms) {
  s_last_activity_ms = now_ms;
  if (!s_awake) {
    displayWake();
  }
}

bool displayIsAwake() { return s_awake; }

void displayWake() {
  if (s_awake) {
    return;
  }
  s_awake = true;
  displayApplyBrightness();
  Serial.println("[blank] display on");
}

void displayBlank() {
  if (!s_awake) {
    return;
  }
  s_awake = false;
  Arduino_GFX* const panel = tft.raw();
  if (panel != nullptr) {
    panel->Display_Brightness(0);
  }
  Serial.println("[blank] display off");
}

uint16_t displayBlankingTimeoutSec() { return s_timeout_sec; }

void displayBlankingNotifyCharging() {
  displayBlank();
}

void displayBlankingNotifyUncharging(unsigned long now_ms) {
  displayBlankingNotifyActivity(now_ms);
}

void displayBlankingSaveTimeoutFromForm(const char* seconds_str) {
  if (seconds_str == nullptr || seconds_str[0] == '\0') {
    return;
  }
  char* end = nullptr;
  const long v = strtol(seconds_str, &end, 10);
  if (end == seconds_str || (end != nullptr && *end != '\0')) {
    return;
  }
  if (v < 0 || v > 65535) {
    return;
  }
  const uint16_t sec = static_cast<uint16_t>(v);
  if (!isValidTimeout(sec)) {
    return;
  }
  s_timeout_sec = sec;
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putUShort(kBlankTimeoutKey, s_timeout_sec);
    prefs.end();
  }
  Serial.printf("[blank] timeout: %us\n", static_cast<unsigned>(s_timeout_sec));
}

}  // namespace hardware
