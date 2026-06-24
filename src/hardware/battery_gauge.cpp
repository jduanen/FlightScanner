#include "hardware/battery_gauge.h"

#include <Arduino.h>
#include <Wire.h>

#include "hardware/display_blanking.h"
#include "hardware/pin_config.h"

namespace hardware {
namespace {

constexpr uint8_t kAddr = 0x55;
constexpr unsigned long kPollIntervalMs = 10000;

// BQ27441-G1A standard command registers (16-bit, little-endian)
constexpr uint8_t kCmdVoltage     = 0x04;  // mV
constexpr uint8_t kCmdFlags       = 0x06;  // Status flags
constexpr uint8_t kCmdRemainCap   = 0x0C;  // Remaining capacity, mAh
constexpr uint8_t kCmdFullCap     = 0x0E;  // Full charge capacity, mAh
constexpr uint8_t kCmdAvgCurrent  = 0x10;  // Average current, mA (signed)
constexpr uint8_t kCmdTimeToEmpty = 0x16;  // Average time to empty, minutes
constexpr uint8_t kCmdSoc         = 0x1C;  // State of charge, %

constexpr uint16_t kFlagDSG = 0x0001;   // Set when discharging
constexpr uint16_t kFlagFC  = 0x0200;   // Set when fully charged

BatteryState s_state;
unsigned long s_last_poll_ms = 0;
bool s_was_charging = false;

bool readU16(uint8_t reg, uint16_t* out) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(kAddr, static_cast<uint8_t>(2)) != 2) {
    return false;
  }
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *out = static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
  return true;
}

void readState() {
  uint16_t v = 0;
  if (readU16(kCmdSoc, &v)) {
    s_state.soc = static_cast<uint8_t>(v);
  }
  if (readU16(kCmdVoltage, &v)) {
    s_state.voltage_mv = v;
  }
  if (readU16(kCmdAvgCurrent, &v)) {
    s_state.current_ma = static_cast<int16_t>(v);
  }
  if (readU16(kCmdRemainCap, &v)) {
    s_state.remain_mah = v;
  }
  if (readU16(kCmdFullCap, &v)) {
    s_state.full_mah = v;
  }
  uint16_t flags = 0;
  if (readU16(kCmdFlags, &flags)) {
    s_state.charging = !(flags & kFlagDSG);
    s_state.full = !!(flags & kFlagFC);
  }
  if (!s_state.charging && !s_state.full) {
    uint16_t tte = 0;
    if (readU16(kCmdTimeToEmpty, &tte) && tte != 0xFFFF) {
      s_state.time_to_empty_min = tte;
    } else {
      s_state.time_to_empty_min = 0;
    }
  } else {
    s_state.time_to_empty_min = 0;
  }
}

}  // namespace

void batteryGaugeInit() {
  Wire.begin(IIC_SDA, IIC_SCL);

  Serial.println("[battery] I2C scan:");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[battery]   0x%02X\n", addr);
    }
  }

  Wire.beginTransmission(kAddr);
  if (Wire.endTransmission() != 0) {
    Serial.println("[battery] BQ27441 not found");
    return;
  }

  s_state.present = true;
  Serial.println("[battery] BQ27441 found");
  readState();
  s_was_charging = s_state.charging;
  s_last_poll_ms = millis();

  if (s_state.charging) {
    displayBlankingNotifyCharging();
  }
}

void batteryGaugePoll() {
  if (!s_state.present) {
    return;
  }
  const unsigned long now = millis();
  if (now - s_last_poll_ms < kPollIntervalMs) {
    return;
  }
  s_last_poll_ms = now;
  readState();

  Serial.printf("[battery] SOC=%u%% V=%umV I=%dmA remain=%umAh\n",
                s_state.soc, s_state.voltage_mv,
                static_cast<int>(s_state.current_ma), s_state.remain_mah);

  if (s_state.charging != s_was_charging) {
    s_was_charging = s_state.charging;
    if (s_state.charging) {
      displayBlankingNotifyCharging();
    } else {
      displayBlankingNotifyUncharging(now);
    }
  }
}

const BatteryState& batteryGaugeState() {
  return s_state;
}

}  // namespace hardware
