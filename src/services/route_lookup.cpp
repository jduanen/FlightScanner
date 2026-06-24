#include "services/route_lookup.h"

#include <Arduino.h>

#include "services/adsb_client.h"
#include <HTTPClient.h>

#include <ArduinoJson.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cctype>
#include <cstring>
#include <ctime>

#include "services/route_server.h"

namespace services::route {

namespace {

constexpr size_t kCacheSize = 64;

struct RouteInfo {
  char airline[28];
  char origin[5];
  char dest[5];
  char origin_iata[4];
  char dest_iata[4];
  char origin_name[48];
  char dest_name[48];
};

struct CacheSlot {
  char callsign[9];
  RouteInfo route;
  ApiSource source;
  bool api_done;  // true once the service has been tried (hit or miss)
};

CacheSlot s_cache[kCacheSize];

TaskHandle_t s_detail_task = nullptr;
char s_detail_selection_callsign[9] = "";
char s_detail_worker_callsign[9] = "";
volatile bool s_detail_requested = false;
volatile bool s_detail_busy = false;
volatile bool s_detail_ready = false;
char s_detail_pending_callsign[9] = "";
volatile bool s_detail_has_pending = false;
char s_detail_debounce_callsign[9] = "";
unsigned long s_detail_debounce_deadline_ms = 0;
bool s_detail_debounce_pending = false;
constexpr unsigned long kDetailEnrichDebounceMs = 400;
RouteInfo s_detail_result = {};
ApiSource s_detail_result_src = ApiSource::kNone;

bool apiAvailable();
bool lookupFromApis(const char* callsign, RouteInfo* route, ApiSource* source_out);

void routeClear(RouteInfo* r) {
  if (r == nullptr) {
    return;
  }
  r->airline[0] = '\0';
  r->origin[0] = '\0';
  r->dest[0] = '\0';
  r->origin_iata[0] = '\0';
  r->dest_iata[0] = '\0';
  r->origin_name[0] = '\0';
  r->dest_name[0] = '\0';
}

bool routeHasData(const RouteInfo& r) {
  return r.airline[0] != '\0' || r.origin[0] != '\0' || r.dest[0] != '\0';
}

bool routeEndpointsComplete(const RouteInfo& r) {
  return r.origin[0] != '\0' && r.dest[0] != '\0';
}

bool slotNeedsApiRouteUpgrade(const CacheSlot& slot) {
  if (slot.callsign[0] == '\0') {
    return false;
  }
  if (routeEndpointsComplete(slot.route)) {
    return false;
  }
  if (slot.api_done && slot.source == ApiSource::kNone) {
    return false;
  }
  return true;
}

void copyAirportCode(const char* s, char* out, size_t out_len) {
  if (out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (s == nullptr || s[0] == '\0') {
    return;
  }
  size_t n = 0;
  while (s[n] != '\0' && n + 1 < out_len && n < 4) {
    const unsigned char c = static_cast<unsigned char>(s[n]);
    out[n] = static_cast<char>(islower(c) ? toupper(c) : s[n]);
    ++n;
  }
  out[n] = '\0';
}

bool isIcaoRadioCallsign(const char* cs) {
  if (cs == nullptr || strlen(cs) < 4) {
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    if (!isupper(static_cast<unsigned char>(cs[i]))) {
      return false;
    }
  }
  for (size_t i = 3; cs[i] != '\0'; ++i) {
    const unsigned char c = static_cast<unsigned char>(cs[i]);
    if (!isupper(c) && !isdigit(c)) {
      return false;
    }
  }
  return true;
}

int findCache(const char* callsign) {
  for (size_t i = 0; i < kCacheSize; ++i) {
    if (s_cache[i].callsign[0] != '\0' && strcmp(s_cache[i].callsign, callsign) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void storeCache(const char* callsign, const RouteInfo& info, ApiSource src, bool api_done) {
  int slot = findCache(callsign);
  if (slot < 0) {
    slot = -1;
    for (size_t i = 0; i < kCacheSize; ++i) {
      if (s_cache[i].callsign[0] == '\0') {
        slot = static_cast<int>(i);
        break;
      }
    }
    if (slot < 0) {
      slot = 0;
      for (size_t i = 1; i < kCacheSize; ++i) {
        if (!s_cache[i].api_done) {
          continue;
        }
        if (!s_cache[slot].api_done) {
          slot = static_cast<int>(i);
          break;
        }
      }
    }
  }
  strncpy(s_cache[slot].callsign, callsign, sizeof(s_cache[slot].callsign) - 1);
  s_cache[slot].callsign[sizeof(s_cache[slot].callsign) - 1] = '\0';
  s_cache[slot].route = info;
  s_cache[slot].source = src;
  s_cache[slot].api_done = api_done;
}

bool cacheResolve(const char* callsign, RouteInfo* out, ApiSource* src_out) {
  const int idx = findCache(callsign);
  if (idx < 0) {
    return false;
  }

  const CacheSlot& slot = s_cache[idx];
  if (out != nullptr) {
    *out = slot.route;
  }
  if (src_out != nullptr) {
    *src_out = slot.source;
  }

  if (apiAvailable()) {
    return !slotNeedsApiRouteUpgrade(slot);
  }

  return slot.api_done || routeHasData(slot.route);
}

bool apiLookupAlreadyDone(const char* callsign) {
  if (!apiAvailable()) {
    return true;
  }
  const int idx = findCache(callsign);
  if (idx >= 0) {
    return !slotNeedsApiRouteUpgrade(s_cache[idx]);
  }
  return false;
}

bool apiAvailable() {
  return route_server::hasUrl();
}

void applyRouteToAircraft(services::adsb::Aircraft& ac, const RouteInfo& info) {
  if (info.airline[0] != '\0') {
    strncpy(ac.airline, info.airline, sizeof(ac.airline) - 1);
    ac.airline[sizeof(ac.airline) - 1] = '\0';
  }
  if (info.origin[0] != '\0') {
    strncpy(ac.route_origin, info.origin, sizeof(ac.route_origin) - 1);
    ac.route_origin[sizeof(ac.route_origin) - 1] = '\0';
  }
  if (info.dest[0] != '\0') {
    strncpy(ac.route_dest, info.dest, sizeof(ac.route_dest) - 1);
    ac.route_dest[sizeof(ac.route_dest) - 1] = '\0';
  }
  if (info.origin_iata[0] != '\0') {
    strncpy(ac.origin_iata, info.origin_iata, sizeof(ac.origin_iata) - 1);
    ac.origin_iata[sizeof(ac.origin_iata) - 1] = '\0';
  }
  if (info.dest_iata[0] != '\0') {
    strncpy(ac.dest_iata, info.dest_iata, sizeof(ac.dest_iata) - 1);
    ac.dest_iata[sizeof(ac.dest_iata) - 1] = '\0';
  }
  if (info.origin_name[0] != '\0') {
    strncpy(ac.origin_name, info.origin_name, sizeof(ac.origin_name) - 1);
    ac.origin_name[sizeof(ac.origin_name) - 1] = '\0';
  }
  if (info.dest_name[0] != '\0') {
    strncpy(ac.dest_name, info.dest_name, sizeof(ac.dest_name) - 1);
    ac.dest_name[sizeof(ac.dest_name) - 1] = '\0';
  }
}

void logRouteLine(const char* callsign, const RouteInfo& route, const char* tag) {
  char leg[12];
  leg[0] = '\0';
  if (route.origin[0] != '\0' && route.dest[0] != '\0') {
    snprintf(leg, sizeof(leg), "%s→%s", route.origin, route.dest);
  } else if (route.origin[0] != '\0') {
    snprintf(leg, sizeof(leg), "%s→?", route.origin);
  } else if (route.dest[0] != '\0') {
    snprintf(leg, sizeof(leg), "?→%s", route.dest);
  } else {
    strncpy(leg, "—", sizeof(leg) - 1);
    leg[sizeof(leg) - 1] = '\0';
  }
  Serial.printf("[route] %s -> %s %s [%s]\n", callsign,
                route.airline[0] != '\0' ? route.airline : "(no airline)", leg, tag);
}

void applyRouteToCallsign(const char* callsign, const RouteInfo& info) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return;
  }
  services::adsb::applyRouteFieldsByCallsign(
      callsign, info.airline, info.origin, info.dest,
      info.origin_iata, info.dest_iata, info.origin_name, info.dest_name);
}

bool isCurrentDetailSelection(const char* callsign) {
  return callsign != nullptr && callsign[0] != '\0' &&
         strcmp(callsign, s_detail_selection_callsign) == 0;
}

void signalDetailReadyIfSelected(const char* callsign) {
  if (isCurrentDetailSelection(callsign)) {
    s_detail_ready = true;
  }
}

void clearDetailDebounce() {
  s_detail_debounce_pending = false;
  s_detail_debounce_deadline_ms = 0;
  s_detail_debounce_callsign[0] = '\0';
}

void scheduleDetailEnrichDebounce(const char* callsign, unsigned long now_ms) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return;
  }
  strncpy(s_detail_debounce_callsign, callsign, sizeof(s_detail_debounce_callsign) - 1);
  s_detail_debounce_callsign[sizeof(s_detail_debounce_callsign) - 1] = '\0';
  s_detail_debounce_deadline_ms = now_ms + kDetailEnrichDebounceMs;
  s_detail_debounce_pending = true;
}

bool tryDetailCacheOnly(const char* callsign) {
  RouteInfo cached;
  routeClear(&cached);
  ApiSource cached_src = ApiSource::kNone;
  const bool cache_complete = cacheResolve(callsign, &cached, &cached_src);
  if (routeHasData(cached)) {
    applyRouteToCallsign(callsign, cached);
  }
  if (cache_complete || apiLookupAlreadyDone(callsign)) {
    if (routeHasData(cached) && isCurrentDetailSelection(callsign)) {
      logRouteLine(callsign, cached, "cache");
    }
    signalDetailReadyIfSelected(callsign);
    return true;
  }
  return false;
}

void setDetailPending(const char* callsign) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return;
  }
  strncpy(s_detail_pending_callsign, callsign, sizeof(s_detail_pending_callsign) - 1);
  s_detail_pending_callsign[sizeof(s_detail_pending_callsign) - 1] = '\0';
  s_detail_has_pending = true;
}

void startDetailWorker(const char* callsign) {
  strncpy(s_detail_worker_callsign, callsign, sizeof(s_detail_worker_callsign) - 1);
  s_detail_worker_callsign[sizeof(s_detail_worker_callsign) - 1] = '\0';
  s_detail_ready = false;
  s_detail_requested = true;
  Serial.printf("Route lookup: detail enrich %s\n", callsign);
}

void drainDetailPending() {
  if (!s_detail_has_pending || s_detail_pending_callsign[0] == '\0') {
    return;
  }

  if (!isCurrentDetailSelection(s_detail_pending_callsign)) {
    s_detail_has_pending = false;
    s_detail_pending_callsign[0] = '\0';
    return;
  }

  char callsign[sizeof(s_detail_pending_callsign)];
  strncpy(callsign, s_detail_pending_callsign, sizeof(callsign));
  callsign[sizeof(callsign) - 1] = '\0';
  s_detail_has_pending = false;
  s_detail_pending_callsign[0] = '\0';

  if (tryDetailCacheOnly(callsign)) {
    return;
  }
  if (s_detail_busy || s_detail_requested) {
    setDetailPending(callsign);
    return;
  }
  startDetailWorker(callsign);
}

void enrichDetailBlocking(const char* callsign) {
  routeClear(&s_detail_result);
  s_detail_result_src = ApiSource::kNone;

  RouteInfo cached;
  routeClear(&cached);
  ApiSource cached_src = ApiSource::kNone;
  const bool cache_complete = cacheResolve(callsign, &cached, &cached_src);
  if (routeHasData(cached)) {
    s_detail_result = cached;
    s_detail_result_src = cached_src;
  }

  if (cache_complete || apiLookupAlreadyDone(callsign)) {
    if (routeHasData(s_detail_result) && isCurrentDetailSelection(callsign)) {
      logRouteLine(callsign, s_detail_result, "cache");
    }
    return;
  }

  if (!isCurrentDetailSelection(callsign)) {
    return;
  }

  ApiSource live_src = ApiSource::kNone;
  if (lookupFromApis(callsign, &s_detail_result, &live_src)) {
    s_detail_result_src = live_src;
    if (isCurrentDetailSelection(callsign)) {
      logRouteLine(callsign, s_detail_result, sourceTag(live_src));
    }
    return;
  }

  RouteInfo miss;
  routeClear(&miss);
  storeCache(callsign, miss, ApiSource::kNone, true);
}

void detailWorkerTask(void* /*arg*/) {
  for (;;) {
    if (s_detail_requested) {
      s_detail_busy = true;
      char callsign[sizeof(s_detail_worker_callsign)];
      strncpy(callsign, s_detail_worker_callsign, sizeof(callsign) - 1);
      callsign[sizeof(callsign) - 1] = '\0';
      enrichDetailBlocking(callsign);
      s_detail_requested = false;
      s_detail_busy = false;
      if (isCurrentDetailSelection(callsign)) {
        s_detail_ready = true;
      }
      drainDetailPending();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void ensureDetailWorker() {
  if (s_detail_task != nullptr) {
    return;
  }
  xTaskCreatePinnedToCore(detailWorkerTask, "route_detail", 16384, nullptr, 1,
                          &s_detail_task, 0);
}

void queueDetailEnrichment(const char* callsign) {
  if (callsign == nullptr || callsign[0] == '\0' || !isIcaoRadioCallsign(callsign)) {
    return;
  }

  if (tryDetailCacheOnly(callsign)) {
    return;
  }

  ensureDetailWorker();
  if (s_detail_busy || s_detail_requested) {
    setDetailPending(callsign);
    return;
  }

  startDetailWorker(callsign);
}

void onFlightDetailSelectedImpl(const char* callsign, const bool immediate) {
  if (callsign == nullptr || callsign[0] == '\0') {
    s_detail_selection_callsign[0] = '\0';
    clearDetailDebounce();
    return;
  }

  const bool selection_changed = strcmp(callsign, s_detail_selection_callsign) != 0;
  if (selection_changed) {
    strncpy(s_detail_selection_callsign, callsign, sizeof(s_detail_selection_callsign) - 1);
    s_detail_selection_callsign[sizeof(s_detail_selection_callsign) - 1] = '\0';
  } else if (!immediate) {
    return;
  }

  if (immediate) {
    clearDetailDebounce();
    queueDetailEnrichment(callsign);
    return;
  }

  scheduleDetailEnrichDebounce(callsign, millis());
}

void tickDetailEnrichDebounceImpl(unsigned long now_ms) {
  if (!s_detail_debounce_pending || s_detail_debounce_callsign[0] == '\0') {
    return;
  }
  if (now_ms < s_detail_debounce_deadline_ms) {
    return;
  }
  if (!isCurrentDetailSelection(s_detail_debounce_callsign)) {
    clearDetailDebounce();
    return;
  }

  char callsign[sizeof(s_detail_debounce_callsign)];
  strncpy(callsign, s_detail_debounce_callsign, sizeof(callsign));
  callsign[sizeof(callsign) - 1] = '\0';
  clearDetailDebounce();
  queueDetailEnrichment(callsign);
}

void cancelDetailEnrichmentImpl() {
  s_detail_selection_callsign[0] = '\0';
  s_detail_ready = false;
  s_detail_has_pending = false;
  s_detail_pending_callsign[0] = '\0';
  clearDetailDebounce();
}

bool detailEnrichmentConsumeImpl() {
  if (!s_detail_ready) {
    return false;
  }

  const char* callsign = s_detail_selection_callsign;
  if (callsign[0] == '\0') {
    s_detail_ready = false;
    return false;
  }

  if (strcmp(callsign, s_detail_worker_callsign) != 0) {
    s_detail_ready = false;
    return false;
  }

  if (routeHasData(s_detail_result)) {
    applyRouteToCallsign(callsign, s_detail_result);
  }

  s_detail_ready = false;
  return true;
}

bool lookupFromService(const char* callsign, RouteInfo* route) {
  const char* base = route_server::url();
  if (base == nullptr || base[0] == '\0' || route == nullptr) {
    return false;
  }
  routeClear(route);

  String url = base;
  url += "/callsign/";
  url += callsign;

  HTTPClient http;
  if (!http.begin(url)) {
    return false;
  }
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setReuse(false);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    return false;
  }
  if (!doc["found"].as<bool>()) {
    return false;
  }

  const char* airline = doc["airline"].as<const char*>();
  if (airline != nullptr && airline[0] != '\0') {
    strncpy(route->airline, airline, sizeof(route->airline) - 1);
    route->airline[sizeof(route->airline) - 1] = '\0';
  }

  JsonObject origin = doc["origin"].as<JsonObject>();
  if (!origin.isNull()) {
    copyAirportCode(origin["icao"].as<const char*>(), route->origin, sizeof(route->origin));
    copyAirportCode(origin["iata"].as<const char*>(), route->origin_iata,
                    sizeof(route->origin_iata));
    const char* name = origin["name"].as<const char*>();
    if (name != nullptr && name[0] != '\0') {
      strncpy(route->origin_name, name, sizeof(route->origin_name) - 1);
      route->origin_name[sizeof(route->origin_name) - 1] = '\0';
    }
  }

  JsonObject dest = doc["destination"].as<JsonObject>();
  if (!dest.isNull()) {
    copyAirportCode(dest["icao"].as<const char*>(), route->dest, sizeof(route->dest));
    copyAirportCode(dest["iata"].as<const char*>(), route->dest_iata, sizeof(route->dest_iata));
    const char* name = dest["name"].as<const char*>();
    if (name != nullptr && name[0] != '\0') {
      strncpy(route->dest_name, name, sizeof(route->dest_name) - 1);
      route->dest_name[sizeof(route->dest_name) - 1] = '\0';
    }
  }

  return routeHasData(*route);
}

bool lookupFromApis(const char* callsign, RouteInfo* route, ApiSource* source_out) {
  if (route == nullptr) {
    return false;
  }
  routeClear(route);

  if (!apiAvailable() || apiLookupAlreadyDone(callsign)) {
    return false;
  }

  if (lookupFromService(callsign, route)) {
    storeCache(callsign, *route, ApiSource::kService, true);
    if (source_out != nullptr) {
      *source_out = ApiSource::kService;
    }
    return true;
  }

  RouteInfo miss;
  routeClear(&miss);
  storeCache(callsign, miss, ApiSource::kNone, true);
  return false;
}

}  // namespace

void init() {
  route_server::load();
  memset(s_cache, 0, sizeof(s_cache));
  if (apiAvailable()) {
    Serial.printf("Route lookup: route server %s\n", route_server::url());
  } else {
    Serial.println("Route lookup: no route server configured");
  }
}

const char* sourceTag(ApiSource s) {
  switch (s) {
    case ApiSource::kService:
      return "svc";
    default:
      return "";
  }
}

void enrichAircraft(services::adsb::Aircraft* planes, size_t count, double center_lat,
                    double center_lon) {
  (void)center_lat;
  (void)center_lon;

  for (size_t i = 0; i < count; ++i) {
    services::adsb::Aircraft& ac = planes[i];
    if (ac.callsign[0] == '\0' || !isIcaoRadioCallsign(ac.callsign)) {
      continue;
    }
    if (ac.airline[0] != '\0' && ac.route_origin[0] != '\0' && ac.route_dest[0] != '\0') {
      continue;
    }

    RouteInfo info;
    routeClear(&info);
    ApiSource src = ApiSource::kNone;
    if (cacheResolve(ac.callsign, &info, &src)) {
      applyRouteToAircraft(ac, info);
    }
  }
}

void onFlightDetailSelected(const char* callsign, const bool immediate) {
  onFlightDetailSelectedImpl(callsign, immediate);
}

void tickDetailEnrichDebounce(unsigned long now_ms) {
  tickDetailEnrichDebounceImpl(now_ms);
}

void cancelDetailEnrichment() { cancelDetailEnrichmentImpl(); }

bool detailEnrichmentReady() { return s_detail_ready; }

bool detailEnrichmentPending() {
  if (s_detail_selection_callsign[0] == '\0' || s_detail_ready) {
    return false;
  }
  if ((s_detail_busy || s_detail_requested) &&
      strcmp(s_detail_worker_callsign, s_detail_selection_callsign) == 0) {
    return true;
  }
  if (s_detail_has_pending &&
      strcmp(s_detail_pending_callsign, s_detail_selection_callsign) == 0) {
    return true;
  }
  return false;
}

bool detailEnrichmentConsume() { return detailEnrichmentConsumeImpl(); }

}  // namespace services::route
