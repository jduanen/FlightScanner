#pragma once

#include <cstdint>

namespace ui::radar {

/** Accent for grid, compass rose, range labels, and sweep line. */
enum class RadarAccentColor : uint8_t {
  Red = 0,
  Yellow,
  Orange,
  Green,
  White,
};

constexpr uint8_t kRadarAccentCount = 5;

struct AccentRgb {
  uint8_t grid_r;
  uint8_t grid_g;
  uint8_t grid_b;
  uint8_t sweep_r;
  uint8_t sweep_g;
  uint8_t sweep_b;
  uint8_t trail_r;
  uint8_t trail_g;
  uint8_t trail_b;
  uint8_t label_r;
  uint8_t label_g;
  uint8_t label_b;
};

void accentBootLoad();
RadarAccentColor accentColor();
const char* accentColorName();
AccentRgb accentPalette();
/** Bright sweep RGB for settings UI highlights. */
void accentHighlightRgb(uint8_t* r, uint8_t* g, uint8_t* b);

void accentStep(int8_t delta);

}  // namespace ui::radar
