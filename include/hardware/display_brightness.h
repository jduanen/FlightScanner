#pragma once

#include <cstdint>

namespace hardware {

/** Load saved brightness from flash (call after displayInit). */
void displayBrightnessBootLoad();

/** Current UI brightness level: 20, 40, 60, 80, or 100 (percent). */
uint8_t displayBrightnessPercent();

/** Step brightness by one 20% level; +1 brighter, -1 dimmer (wraps). */
void displayBrightnessStep(int8_t delta);

/** Persist brightness from web form (20, 40, 60, 80, or 100). */
void displayBrightnessSaveFromForm(const char* percent_str);

/** Apply current level to the panel. */
void displayApplyBrightness();

}  // namespace hardware
