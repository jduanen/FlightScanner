#pragma once

#include <cstddef>
#include <cstdint>

namespace services::adsb {
struct Aircraft;
}

namespace services::route {

/** Which source supplied the airline/route data (serial log tag). */
enum class ApiSource : uint8_t {
  kNone = 0,
  kService = 1,  // AircraftRoute local service
};

void init();

/** Apply RAM cache during ADS-B polls (no live service calls). */
void enrichAircraft(services::adsb::Aircraft* planes, size_t count, double center_lat,
                    double center_lon);

/**
 * Flight-detail view opened or encoder moved to another aircraft.
 * immediate=true on open/tap; false on encoder steps (debounced before service call).
 */
void onFlightDetailSelected(const char* callsign, bool immediate = false);

/** Fire debounced detail enrich after encoder settles (call from main loop). */
void tickDetailEnrichDebounce(unsigned long now_ms);

/** Clear detail enrichment state when leaving flight detail. */
void cancelDetailEnrichment();

/** True when a background detail enrichment finished. */
bool detailEnrichmentReady();

/** Apply enrichment result to the aircraft list; returns true if UI should refresh. */
bool detailEnrichmentConsume();

const char* sourceTag(ApiSource s);

}  // namespace services::route
