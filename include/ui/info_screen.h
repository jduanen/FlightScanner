#pragma once

#include <cstdint>

namespace ui {

enum class InfoSettingsPage : uint8_t { Main, Display, Colors };

void infoScreenResetToMain();
InfoSettingsPage infoScreenPage();
void infoScreenSetPage(InfoSettingsPage page);

/** Settings / status screens (two pages). */
void infoScreenDraw();

/** Rotate knob on display page adjusts the highlighted row. */
void infoScreenHandleKnob(int8_t delta);

/** Short knob press on display page cycles highlighted setting row. */
void infoScreenCycleDisplayFocus();

/** Default display-page selection when opening page 2. */
void infoScreenResetDisplayFocus();

}  // namespace ui
