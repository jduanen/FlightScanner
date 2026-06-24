#pragma once

#include <cstdint>

namespace hardware {

struct BatteryState {
  bool present = false;
  uint8_t soc = 0;                  // State of charge, 0–100%
  uint16_t voltage_mv = 0;          // Battery voltage, mV
  int16_t current_ma = 0;           // Average current: positive=charging, negative=discharging
  uint16_t remain_mah = 0;          // Remaining capacity, mAh
  uint16_t full_mah = 0;            // Full charge capacity, mAh
  uint16_t time_to_empty_min = 0;   // Time to empty while discharging, 0 if unknown
  bool charging = false;
  bool full = false;
};

/** Probe the BQ27441-G1A on the Qwiic bus (Wire1, GPIO15/16). Call after panelBootResolve(). */
void batteryGaugeInit();

/** Read gauge state; throttled to once per 10 s. Call from main loop. */
void batteryGaugePoll();

const BatteryState& batteryGaugeState();

}  // namespace hardware
