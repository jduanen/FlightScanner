#include "ui/radar_scale.h"

#include "ui/radar_theme.h"

#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui::radar {

namespace {

constexpr char kStoreNs[] = "flightscnr";
constexpr char kScaleSlotKey[] = "scale_slot";
constexpr char kDistMiKey[] = "dist_mi";
constexpr char kRoseKey[] = "rose_en";

constexpr char kLegacyScaleKey[] = "rangeIdx";
constexpr char kLegacyMilesKey[] = "useMiles";
constexpr char kLegacyRoseKey[] = "showCard";

constexpr uint8_t kDefaultScaleIndex = 1;
constexpr float kKmPerMile = 1.609344f;

uint8_t s_active_index = kDefaultScaleIndex;
bool s_distance_in_miles = false;
bool s_compass_rose = true;

bool formCheckboxOn(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  if ((value[0] == 'F' || value[0] == 'f') && value[1] == '\0') {
    return false;
  }
  if ((value[0] == 'T' || value[0] == 't') && value[1] == '\0') {
    return true;
  }
  return strcmp(value, "on") == 0;
}

void persistU8(const char* key, uint8_t v) {
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putUChar(key, v);
    prefs.end();
  }
}

void persistBool(const char* key, bool v) {
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putBool(key, v);
    prefs.end();
  }
}

}  // namespace

void scaleBootLoad() {
  Preferences prefs;
  if (!prefs.begin(kStoreNs, true)) {
    return;
  }

  uint8_t slot = prefs.getUChar(kScaleSlotKey, 255);
  if (slot == 255) {
    slot = prefs.getUChar(kLegacyScaleKey, kDefaultScaleIndex);
  }
  s_active_index = (slot < kScaleBandCount) ? slot : kDefaultScaleIndex;

  if (prefs.isKey(kDistMiKey)) {
    s_distance_in_miles = prefs.getBool(kDistMiKey, false);
  } else {
    s_distance_in_miles = prefs.getBool(kLegacyMilesKey, false);
  }

  if (prefs.isKey(kRoseKey)) {
    s_compass_rose = prefs.getBool(kRoseKey, true);
  } else {
    s_compass_rose = prefs.getBool(kLegacyRoseKey, true);
  }

  prefs.end();
}

void scaleIncrease() {
  s_active_index = static_cast<uint8_t>((s_active_index + 1) % kScaleBandCount);
  persistU8(kScaleSlotKey, s_active_index);
}

void scaleDecrease() {
  s_active_index = (s_active_index == 0)
                       ? static_cast<uint8_t>(kScaleBandCount - 1)
                       : static_cast<uint8_t>(s_active_index - 1);
  persistU8(kScaleSlotKey, s_active_index);
}

void scaleSelect(uint8_t index) {
  if (index >= kScaleBandCount) {
    return;
  }
  s_active_index = index;
  persistU8(kScaleSlotKey, s_active_index);
}

const ScaleBand& scaleActive() { return kScaleBands[s_active_index]; }

uint8_t scaleActiveIndex() { return s_active_index; }

float adsbQueryRadiusKm() {
  const float coverage_km = scaleActive().coverage_km;
  const float screen_r_px =
      static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return coverage_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

bool distanceInMiles() { return s_distance_in_miles; }

void toggleDistanceUnits() {
  s_distance_in_miles = !s_distance_in_miles;
  persistBool(kDistMiKey, s_distance_in_miles);
  Serial.printf("Distance units: %s\n", s_distance_in_miles ? "miles" : "km");
}

void saveDistanceUnitsFromForm(const char* checkbox_value) {
  s_distance_in_miles = formCheckboxOn(checkbox_value);
  persistBool(kDistMiKey, s_distance_in_miles);
  Serial.printf("Distance units: %s\n", s_distance_in_miles ? "miles" : "km");
}

bool showCompassRose() { return s_compass_rose; }

void toggleCompassRose() {
  s_compass_rose = !s_compass_rose;
  persistBool(kRoseKey, s_compass_rose);
  Serial.printf("Compass rose: %s\n", s_compass_rose ? "on" : "off");
}

void saveCompassRoseFromForm(const char* checkbox_value) {
  s_compass_rose = formCheckboxOn(checkbox_value);
  persistBool(kRoseKey, s_compass_rose);
  Serial.printf("Compass rose: %s\n", s_compass_rose ? "on" : "off");
}

void formatScaleTag(char* buf, size_t len, float label_km, bool use_miles) {
  if (use_miles) {
    const int mi = static_cast<int>(lroundf(label_km / kKmPerMile));
    snprintf(buf, len, "%dmi", mi);
  } else {
    const int km = static_cast<int>(lroundf(label_km));
    snprintf(buf, len, "%dkm", km);
  }
}

void formatActiveScaleTag(char* buf, size_t len) {
  formatScaleTag(buf, len, scaleActive().label_km, s_distance_in_miles);
}

}  // namespace ui::radar
