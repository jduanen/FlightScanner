#pragma once

#include <cstdint>

namespace hardware {

/** Load screen-blank timeout from NVS (call after displayInit). */
void displayBlankingBootLoad();

/** Check if the blank timeout has elapsed; blank the display if so. */
void displayBlankingTick(unsigned long now_ms);

/** Reset the blank timer; wake the display if it was blanked. */
void displayBlankingNotifyActivity(unsigned long now_ms);

/** True while the display is on (not blanked). */
bool displayIsAwake();

/** Turn the display on and restore saved brightness. */
void displayWake();

/** Turn the display off (brightness = 0). */
void displayBlank();

/** Current blank timeout in seconds; 0 = never blank. */
uint16_t displayBlankingTimeoutSec();

/** Blank the display immediately (e.g. device placed on charger). */
void displayBlankingNotifyCharging();

/** Wake the display and reset the timer (e.g. device removed from charger). */
void displayBlankingNotifyUncharging(unsigned long now_ms);

/** Persist blank timeout from web form (0, 30, 60, 120, 300, or 600 seconds). */
void displayBlankingSaveTimeoutFromForm(const char* seconds_str);

}  // namespace hardware
