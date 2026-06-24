#include "services/settings_web.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cmath>
#include <cstdio>

#include <esp_system.h>

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#include "config.h"
#include "hardware/buzzer.h"
#include "hardware/display_blanking.h"
#include "hardware/display_brightness.h"
#include "services/adsb_client.h"
#include "services/map_center.h"
#include "services/route_server.h"
#include "services/settings_apply.h"
#include "ui/display_prefs.h"
#include "ui/radar_scale.h"

namespace {

WebServer* s_server = nullptr;
bool s_active = false;

/** Static storage — must not live on loopTask stack (~8 KB). */
constexpr size_t kSettingsPageCap = 9216;
char s_settings_page[kSettingsPageCap];

const char kPageHead[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FlightScnr Settings</title>
<style>
body{font-family:system-ui,sans-serif;max-width:28rem;margin:1.5rem auto;padding:0 1rem;
background:#000;color:#e8f0ff;}
h1{font-size:1.25rem;margin:0 0 .5rem;}
p{color:#9ab;line-height:1.4;font-size:.9rem;}
label{display:block;margin:.75rem 0 .25rem;font-size:.85rem;}
input,select{width:100%;box-sizing:border-box;padding:.5rem;border-radius:6px;
border:1px solid #345;background:#555;color:#fff;font-size:1rem;}
.chk{margin:.75rem 0;display:flex;align-items:center;gap:.5rem;}
.chk input{width:auto;}
button{margin-top:1.25rem;width:100%;padding:.75rem;font-size:1rem;font-weight:600;
border:none;border-radius:8px;background:#1a9c3c;color:#fff;cursor:pointer;}
.note{margin-top:1rem;font-size:.8rem;color:#7a9;}
.gh{margin:.35rem 0 1rem;font-size:.85rem;text-align:center;}
.gh a{color:#6cf;}
</style></head><body>
<h1>FlightScnr</h1>
<p>Changes are saved to flash. The device reboots after you tap <strong>Save &amp; reboot</strong>.</p>
<form method="POST" action="/save">
)HTML";

const char kPageTail[] = R"HTML(
<button type="submit">Save &amp; reboot</button>
</form>
<p class="note">Wi‑Fi credentials are configured in the setup portal (hold knob 3&nbsp;s to reset).</p>
</body></html>
)HTML";

void appendGithubLink(char* page, size_t len, size_t* used) {
  const int n = snprintf(
      page + *used, len - *used,
      "<p class=\"gh\"><a href=\"%s\" target=\"_blank\" rel=\"noopener\">"
      "github.com/yashmulgaonkar/FlightScnr</a></p>",
      config::kGithubRepoUrl);
  if (n > 0) {
    *used += static_cast<size_t>(n);
  }
}

void sendRebootPage() {
  char page[640];
  snprintf(page, sizeof(page),
           "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>Rebooting</title></head>"
           "<body style=\"font-family:system-ui,sans-serif;text-align:center;padding:2rem;"
           "background:#000;color:#e8f0ff\">"
           "<h1>Saved</h1><p>Rebooting&hellip;</p>"
           "<p style=\"margin-top:1.5rem;font-size:.9rem\">"
           "<a href=\"%s\" style=\"color:#6cf\" target=\"_blank\" rel=\"noopener\">"
           "github.com/yashmulgaonkar/FlightScnr</a></p>"
           "</body></html>",
           config::kGithubRepoUrl);
  s_server->send(200, "text/html; charset=utf-8", page);
}

void sendLocationErrorPage() {
  char page[960];
  snprintf(page, sizeof(page),
           "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>Radar center not saved</title></head>"
           "<body style=\"font-family:system-ui,sans-serif;max-width:28rem;margin:1.5rem auto;"
           "padding:0 1rem;background:#000;color:#e8f0ff\">"
           "<h1 style=\"font-size:1.25rem;color:#f66\">Radar center not saved</h1>"
           "<p>Other settings were saved, but the <strong>Radar Center</strong> value could "
           "not be parsed. Use decimal degrees with a comma between latitude and longitude, "
           "for example:</p>"
           "<p style=\"font-family:monospace;background:#222;padding:.75rem;border-radius:6px\">"
           "51.507400, -0.127800</p>"
           "<p style=\"color:#9ab;font-size:.9rem\">Spaces around the comma are fine. "
           "Latitude must be between &minus;90 and 90; longitude between &minus;180 and 180.</p>"
           "<p><a href=\"/\" style=\"color:#6cf\">Back to settings</a></p>"
           "</body></html>");
  s_server->send(400, "text/html; charset=utf-8", page);
}

void appendRangeOptions(char* buf, size_t len, size_t* used) {
  for (uint8_t i = 0; i < ui::radar::kScaleBandCount; ++i) {
    const ui::radar::ScaleBand& p = ui::radar::kScaleBands[i];
    const int mi = static_cast<int>(lroundf(p.label_km / 1.609344f));
    const int n = snprintf(
        buf + *used, len - *used,
        "<option value=\"%u\"%s>%d km / %d mi</option>",
        static_cast<unsigned>(i),
        (i == ui::radar::scaleActiveIndex()) ? " selected" : "",
        static_cast<int>(lroundf(p.label_km)), mi);
    if (n > 0) {
      *used += static_cast<size_t>(n);
    }
  }
}

void handleSettingsPage() {
  services::route_server::load();
  char* const page = s_settings_page;
  size_t used = 0;

  const int head_n = snprintf(page, kSettingsPageCap, "%s", kPageHead);
  if (head_n > 0) {
    used = static_cast<size_t>(head_n);
  }

  char center_value[48];
  snprintf(center_value, sizeof(center_value), "%.6f, %.6f",
           services::map_center::latitude(), services::map_center::longitude());
  const int center_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<label for=\"radar_center\">Radar Center</label>"
      "<input id=\"radar_center\" name=\"radar_center\" type=\"text\" required "
      "autocomplete=\"off\" placeholder=\"37.636422, -122.365968\" value=\"%s\">",
      center_value);
  if (center_n > 0) {
    used += static_cast<size_t>(center_n);
  }

  const int miles_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<div class=\"chk\"><input id=\"use_miles\" name=\"use_miles\" type=\"checkbox\" "
      "value=\"T\"%s><label for=\"use_miles\">Display distances in miles</label></div>",
      ui::radar::distanceInMiles() ? " checked" : "");
  if (miles_n > 0) {
    used += static_cast<size_t>(miles_n);
  }

  const int card_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<div class=\"chk\"><input id=\"show_cardinals\" name=\"show_cardinals\" type=\"checkbox\" "
      "value=\"T\"%s><label for=\"show_cardinals\">Show Compass Rose</label></div>",
      ui::radar::showCompassRose() ? " checked" : "");
  if (card_n > 0) {
    used += static_cast<size_t>(card_n);
  }

  const int sweep_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<div class=\"chk\"><input id=\"show_sweep\" name=\"show_sweep\" type=\"checkbox\" "
      "value=\"T\"%s><label for=\"show_sweep\">Show radar sweep line</label></div>",
      ui::displayPrefsSweepLineEnabled() ? " checked" : "");
  if (sweep_n > 0) {
    used += static_cast<size_t>(sweep_n);
  }

  const unsigned long detail_ms = ui::displayPrefsFlightDetailTimeoutMs();
  const unsigned long detail_sec = detail_ms / 1000UL;
  const int detail_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<label for=\"detail_timeout\">Flight detail screen</label>"
      "<select id=\"detail_timeout\" name=\"detail_timeout\">"
      "<option value=\"0\"%s>Manual (swipe away)</option>"
      "<option value=\"10\"%s>10 seconds</option>"
      "<option value=\"20\"%s>20 seconds</option>"
      "<option value=\"30\"%s>30 seconds</option>"
      "</select>",
      detail_sec == 0 ? " selected" : "",
      detail_sec == 10 ? " selected" : "",
      detail_sec == 20 ? " selected" : "",
      detail_sec == 30 ? " selected" : "");
  if (detail_n > 0) {
    used += static_cast<size_t>(detail_n);
  }

  const uint8_t bright = hardware::displayBrightnessPercent();
  const int bright_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<label for=\"bright_pct\">Screen brightness</label>"
      "<select id=\"bright_pct\" name=\"bright_pct\">"
      "<option value=\"20\"%s>20%%</option>"
      "<option value=\"40\"%s>40%%</option>"
      "<option value=\"60\"%s>60%%</option>"
      "<option value=\"80\"%s>80%%</option>"
      "<option value=\"100\"%s>100%%</option>"
      "</select>",
      bright == 20 ? " selected" : "", bright == 40 ? " selected" : "",
      bright == 60 ? " selected" : "", bright == 80 ? " selected" : "",
      bright == 100 ? " selected" : "");
  if (bright_n > 0) {
    used += static_cast<size_t>(bright_n);
  }

  const uint16_t blank_to = hardware::displayBlankingTimeoutSec();
  const int blank_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<label for=\"blank_timeout\">Screen blanking</label>"
      "<select id=\"blank_timeout\" name=\"blank_timeout\">"
      "<option value=\"0\"%s>Never</option>"
      "<option value=\"30\"%s>30 seconds</option>"
      "<option value=\"60\"%s>1 minute</option>"
      "<option value=\"120\"%s>2 minutes</option>"
      "<option value=\"300\"%s>5 minutes</option>"
      "<option value=\"600\"%s>10 minutes</option>"
      "</select>",
      blank_to == 0 ? " selected" : "",
      blank_to == 30 ? " selected" : "",
      blank_to == 60 ? " selected" : "",
      blank_to == 120 ? " selected" : "",
      blank_to == 300 ? " selected" : "",
      blank_to == 600 ? " selected" : "");
  if (blank_n > 0) {
    used += static_cast<size_t>(blank_n);
  }

  const int beep_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<div class=\"chk\"><input id=\"ui_beep\" name=\"ui_beep\" type=\"checkbox\" "
      "value=\"T\"%s><label for=\"ui_beep\">UI beep on touch and knob</label></div>"
      "<label for=\"beep_tone\">Beep tone</label>"
      "<select id=\"beep_tone\" name=\"beep_tone\">"
      "<option value=\"A\"%s>A</option>"
      "<option value=\"B\"%s>B</option>"
      "<option value=\"C\"%s>C</option>"
      "<option value=\"D\"%s>D</option>"
      "<option value=\"E\"%s>E</option>"
      "</select>",
      hardware::buzzerEnabled() ? " checked" : "",
      hardware::buzzerToneLetter() == 'A' ? " selected" : "",
      hardware::buzzerToneLetter() == 'B' ? " selected" : "",
      hardware::buzzerToneLetter() == 'C' ? " selected" : "",
      hardware::buzzerToneLetter() == 'D' ? " selected" : "",
      hardware::buzzerToneLetter() == 'E' ? " selected" : "");
  if (beep_n > 0) {
    used += static_cast<size_t>(beep_n);
  }

  const int min_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<label for=\"min_height\">Min altitude (ft, 0 = off)</label>"
      "<input id=\"min_height\" name=\"min_height\" type=\"number\" min=\"0\" step=\"100\" "
      "value=\"%d\">",
      services::adsb::altitudeFloorFt());
  if (min_n > 0) {
    used += static_cast<size_t>(min_n);
  }

  const int range_lbl = snprintf(page + used, kSettingsPageCap - used,
                                 "<label for=\"range_idx\">Range preset</label>"
                                 "<select id=\"range_idx\" name=\"range_idx\">");
  if (range_lbl > 0) {
    used += static_cast<size_t>(range_lbl);
  }
  appendRangeOptions(page, kSettingsPageCap, &used);

  const int range_end = snprintf(page + used, kSettingsPageCap - used, "</select>");
  if (range_end > 0) {
    used += static_cast<size_t>(range_end);
  }

  const int svc_n = snprintf(
      page + used, kSettingsPageCap - used,
      "<h2 style=\"font-size:1rem;margin:1.25rem 0 .35rem\">Route Server</h2>"
      "<p>URL of the AircraftRoute service on your local network "
      "(e.g. <code>http://192.168.1.x:5000</code>). Leave blank to use prefix-only fallback.</p>"
      "<label for=\"route_server_url\">Route server URL</label>"
      "<input id=\"route_server_url\" name=\"route_server_url\" type=\"text\" "
      "autocomplete=\"off\" placeholder=\"http://192.168.1.x:5000\" value=\"%s\">",
      services::route_server::url());
  if (svc_n > 0) {
    used += static_cast<size_t>(svc_n);
  }

  const int tail_n = snprintf(page + used, kSettingsPageCap - used, "%s", kPageTail);
  if (tail_n > 0) {
    used += static_cast<size_t>(tail_n);
  }
  appendGithubLink(page, kSettingsPageCap, &used);

  s_server->send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  if (s_server->method() != HTTP_POST) {
    s_server->send(405, "text/plain", "Method Not Allowed");
    return;
  }

  const bool loc_ok = settingsApplyFromForm(
      s_server->arg("radar_center").c_str(), nullptr, nullptr,
      s_server->arg("use_miles").c_str(), s_server->arg("show_cardinals").c_str(),
      s_server->arg("min_height").c_str(),
      s_server->arg("range_idx").c_str(),
      s_server->arg("route_server_url").c_str(),
      s_server->arg("ui_beep").c_str(),
      s_server->arg("beep_tone").c_str(), s_server->arg("bright_pct").c_str(),
      s_server->arg("show_sweep").c_str(), s_server->arg("detail_timeout").c_str(),
      s_server->arg("blank_timeout").c_str());

  Serial.printf("Settings web save (lat/lon %s)\n", loc_ok ? "ok" : "invalid");

  if (!loc_ok) {
    sendLocationErrorPage();
    return;
  }

  sendRebootPage();
  s_server->client().flush();
  delay(400);
  esp_restart();
}

void handleNotFound() {
  s_server->sendHeader("Location", "/", true);
  s_server->send(302, "text/plain", "");
}

void apiSendCors() {
  s_server->sendHeader("Access-Control-Allow-Origin", "*");
  s_server->sendHeader("Access-Control-Allow-Methods", "GET, POST");
  s_server->sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleApiSettingsGet() {
  JsonDocument doc;
  char center[48];
  snprintf(center, sizeof(center), "%.6f, %.6f",
           services::map_center::latitude(), services::map_center::longitude());
  doc["radar_center"] = center;
  doc["use_miles"] = ui::radar::distanceInMiles();
  doc["show_cardinals"] = ui::radar::showCompassRose();
  doc["show_sweep"] = ui::displayPrefsSweepLineEnabled();
  doc["detail_timeout"] = static_cast<int>(ui::displayPrefsFlightDetailTimeoutMs() / 1000UL);
  doc["bright_pct"] = hardware::displayBrightnessPercent();
  doc["blank_timeout"] = hardware::displayBlankingTimeoutSec();
  doc["ui_beep"] = hardware::buzzerEnabled();
  char tone[2] = {hardware::buzzerToneLetter(), '\0'};
  doc["beep_tone"] = tone;
  doc["min_height"] = services::adsb::altitudeFloorFt();
  doc["range_idx"] = ui::radar::scaleActiveIndex();
  doc["route_server_url"] = services::route_server::url();

  String json;
  serializeJson(doc, json);
  apiSendCors();
  s_server->send(200, "application/json", json);
}

void handleApiSettingsPost() {
  JsonDocument doc;
  if (deserializeJson(doc, s_server->arg("plain"))) {
    apiSendCors();
    s_server->send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }

  if (!doc["radar_center"].isNull()) {
    services::map_center::applyRadarCenterFromForm(doc["radar_center"].as<const char*>());
  }
  if (!doc["use_miles"].isNull()) {
    ui::radar::saveDistanceUnitsFromForm(doc["use_miles"].as<bool>() ? "T" : "F");
  }
  if (!doc["show_cardinals"].isNull()) {
    ui::radar::saveCompassRoseFromForm(doc["show_cardinals"].as<bool>() ? "T" : "F");
  }
  if (!doc["show_sweep"].isNull()) {
    ui::displayPrefsSaveSweepLineFromForm(doc["show_sweep"].as<bool>() ? "T" : "F");
  }
  if (!doc["detail_timeout"].isNull()) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", doc["detail_timeout"].as<int>());
    ui::displayPrefsSaveFlightDetailTimeoutFromForm(buf);
  }
  if (!doc["bright_pct"].isNull()) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", doc["bright_pct"].as<int>());
    hardware::displayBrightnessSaveFromForm(buf);
    hardware::displayApplyBrightness();
  }
  if (!doc["blank_timeout"].isNull()) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", doc["blank_timeout"].as<int>());
    hardware::displayBlankingSaveTimeoutFromForm(buf);
  }
  if (!doc["ui_beep"].isNull()) {
    hardware::saveBeepEnabledFromForm(doc["ui_beep"].as<bool>() ? "T" : "F");
  }
  if (!doc["beep_tone"].isNull()) {
    hardware::saveBeepToneFromForm(doc["beep_tone"].as<const char*>());
  }
  if (!doc["min_height"].isNull()) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", doc["min_height"].as<int>());
    services::adsb::saveAltitudeFloorFromForm(buf);
  }
  if (!doc["range_idx"].isNull()) {
    const int idx = doc["range_idx"].as<int>();
    if (idx >= 0 && idx < static_cast<int>(ui::radar::kScaleBandCount)) {
      ui::radar::scaleSelect(static_cast<uint8_t>(idx));
    }
  }
  if (!doc["route_server_url"].isNull()) {
    services::route_server::saveFromForm(doc["route_server_url"].as<const char*>());
  }

  handleApiSettingsGet();
}

void handleApiSettings() {
  if (s_server->method() == HTTP_GET) {
    handleApiSettingsGet();
  } else if (s_server->method() == HTTP_POST) {
    handleApiSettingsPost();
  } else {
    apiSendCors();
    s_server->send(405, "application/json", "{\"error\":\"method not allowed\"}");
  }
}

void handleApiReboot() {
  if (s_server->method() != HTTP_POST) {
    apiSendCors();
    s_server->send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }
  apiSendCors();
  s_server->send(200, "application/json", "{\"status\":\"rebooting\"}");
  s_server->client().flush();
  delay(200);
  esp_restart();
}

void registerRoutes() {
  s_server->on("/", HTTP_GET, handleSettingsPage);
  s_server->on("/settings", HTTP_GET, handleSettingsPage);
  s_server->on("/save", HTTP_POST, handleSave);
  s_server->on("/api/settings", handleApiSettings);
  s_server->on("/api/reboot", handleApiReboot);
  s_server->onNotFound(handleNotFound);
}

}  // namespace

void settingsWebStart() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (s_active && s_server != nullptr) {
    return;
  }

  services::route_server::load();

  settingsWebStop();

  s_server = new WebServer(80);
  registerRoutes();
  s_server->begin();
  s_active = true;

  WiFi.setHostname(config::kPortalHostname);

#ifdef WM_MDNS
  MDNS.end();
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("Settings web: http://%s.local/  http://%s/\n",
                  config::kPortalHostname, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("Settings web: http://%s/  (mDNS unavailable)\n",
                  WiFi.localIP().toString().c_str());
  }
#else
  Serial.printf("Settings web: http://%s/\n", WiFi.localIP().toString().c_str());
#endif
}

void settingsWebStop() {
  if (s_server != nullptr) {
    s_server->stop();
    delete s_server;
    s_server = nullptr;
  }
  s_active = false;
#ifdef WM_MDNS
  MDNS.end();
#endif
}

void settingsWebPoll() {
  if (!s_active || s_server == nullptr) {
    return;
  }
  s_server->handleClient();
}

bool settingsWebActive() { return s_active; }
