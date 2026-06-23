#include "services/clock_time.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "config.h"

namespace services::clock {

namespace {

constexpr char kNs[] = "flightscnr";
constexpr char kTzOffsetKey[] = "clk_tz_sec";
constexpr char kUse24hKey[] = "clk_24h";

constexpr int32_t kMinOffsetSec = -12 * 3600;
constexpr int32_t kMaxOffsetSec = 14 * 3600;
constexpr time_t kMinValidEpoch = 1600000000;

int32_t s_tz_offset_sec = 0;
bool s_use_24h = false;

void persistOffset() {
  Preferences prefs;
  if (prefs.begin(kNs, false)) {
    prefs.putInt(kTzOffsetKey, s_tz_offset_sec);
    prefs.end();
  }
}

void persistFormat() {
  Preferences prefs;
  if (prefs.begin(kNs, false)) {
    prefs.putBool(kUse24hKey, s_use_24h);
    prefs.end();
  }
}

void applyNtpConfig() {
  configTime(s_tz_offset_sec, 0, config::kNtpServer1, config::kNtpServer2);
}

bool timeValid() {
  const time_t now = time(nullptr);
  return now >= kMinValidEpoch;
}

void setTimezoneOffsetSec(int32_t offset_sec) {
  if (offset_sec < kMinOffsetSec) {
    offset_sec = kMinOffsetSec;
  } else if (offset_sec > kMaxOffsetSec) {
    offset_sec = kMaxOffsetSec;
  }
  s_tz_offset_sec = offset_sec;
  persistOffset();
  applyNtpConfig();
}

void setUse24Hour(bool use_24h) {
  s_use_24h = use_24h;
  persistFormat();
}

}  // namespace

void bootLoad() {
  Preferences prefs;
  if (!prefs.begin(kNs, true)) {
    s_tz_offset_sec = 0;
    s_use_24h = false;
    return;
  }
  s_tz_offset_sec = prefs.getInt(kTzOffsetKey, 0);
  s_use_24h = prefs.getBool(kUse24hKey, false);
  prefs.end();

  if (s_tz_offset_sec < kMinOffsetSec) {
    s_tz_offset_sec = kMinOffsetSec;
  } else if (s_tz_offset_sec > kMaxOffsetSec) {
    s_tz_offset_sec = kMaxOffsetSec;
  }
}

void startNtp() {
  applyNtpConfig();
  Serial.printf("Clock: NTP %s / %s (UTC%+d)\n", config::kNtpServer1,
                  config::kNtpServer2, static_cast<int>(s_tz_offset_sec / 3600));
}

uint32_t localMinuteStamp() {
  if (!timeValid()) {
    return UINT32_MAX;
  }
  return static_cast<uint32_t>(time(nullptr) / 60);
}

int32_t timezoneOffsetSec() { return s_tz_offset_sec; }

void stepTimezoneHours(int8_t delta) {
  if (delta == 0) {
    return;
  }
  setTimezoneOffsetSec(s_tz_offset_sec + static_cast<int32_t>(delta) * 3600);
}

bool use24Hour() { return s_use_24h; }

void toggleHourFormat() {
  setUse24Hour(!s_use_24h);
}

void formatTimezoneLabel(char* out, size_t len) {
  if (out == nullptr || len == 0) {
    return;
  }
  const int32_t off = s_tz_offset_sec;
  const int sign = off >= 0 ? 1 : -1;
  const int32_t abs_sec = off >= 0 ? off : -off;
  const int h = abs_sec / 3600;
  const int m = (abs_sec % 3600) / 60;
  if (m == 0) {
    snprintf(out, len, "UTC%+d", sign * h);
  } else {
    snprintf(out, len, "UTC%+d:%02d", sign * h, m);
  }
}

void formatTimeOfDay(char* out, size_t len) {
  if (out == nullptr || len == 0) {
    return;
  }
  out[0] = '\0';
  if (!timeValid()) {
    strncpy(out, "--:--", len - 1);
    out[len - 1] = '\0';
    return;
  }

  struct tm local {};
  const time_t now = time(nullptr);
  localtime_r(&now, &local);
  if (s_use_24h) {
    strftime(out, len, "%H:%M", &local);
  } else {
    strftime(out, len, "%I:%M", &local);
  }
}

void formatDateLine(char* out, size_t len) {
  if (out == nullptr || len == 0) {
    return;
  }
  out[0] = '\0';
  if (!timeValid()) {
    strncpy(out, "Syncing time…", len - 1);
    out[len - 1] = '\0';
    return;
  }

  struct tm local {};
  const time_t now = time(nullptr);
  localtime_r(&now, &local);
  strftime(out, len, "%a %b %d", &local);
}

void formatAmPm(char* out, size_t len) {
  if (out == nullptr || len == 0) {
    return;
  }
  out[0] = '\0';
  if (s_use_24h || !timeValid()) {
    return;
  }

  struct tm local {};
  const time_t now = time(nullptr);
  localtime_r(&now, &local);
  strftime(out, len, "%p", &local);
}

}  // namespace services::clock
