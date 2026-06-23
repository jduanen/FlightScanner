#pragma once

#include <cstddef>

namespace services::airport {

/** Normalize to 4-letter ICAO when possible (ICAO passthrough or IATA remap). */
bool normalizeRouteCode(const char* code, char* icao_out, size_t icao_len);

}  // namespace services::airport
