#pragma once

#include <cstdint>

class PlaneGfx;

namespace ui::aircraft_symbol {

/** Top-down aircraft glyph radius in pixels (for layout / dirty rects). */
int radiusPx();

/** Full-size symbol on the radar grid, rotated so nose points along heading_deg. */
void draw(PlaneGfx& gfx, int cx, int cy, float heading_deg, uint16_t color);

/** Smaller symbol for beyond-range rim markers. */
void drawCompact(PlaneGfx& gfx, int cx, int cy, float heading_deg, uint16_t color);

}  // namespace ui::aircraft_symbol
