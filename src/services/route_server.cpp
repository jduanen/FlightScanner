#include "services/route_server.h"

#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

namespace services::route_server {

namespace {

constexpr char kNs[] = "flightscanner";
constexpr char kUrlKey[] = "rs_url";

char s_url[kMaxUrlLen + 1] = "";

}  // namespace

void load() {
  Preferences prefs;
  if (!prefs.begin(kNs, true)) {
    return;
  }
  if (prefs.isKey(kUrlKey)) {
    strncpy(s_url, prefs.getString(kUrlKey).c_str(), kMaxUrlLen);
    s_url[kMaxUrlLen] = '\0';
  }
  prefs.end();
}

void saveFromForm(const char* url) {
  if (url == nullptr || url[0] == '\0') {
    return;
  }
  strncpy(s_url, url, kMaxUrlLen);
  s_url[kMaxUrlLen] = '\0';

  size_t n = strlen(s_url);
  while (n > 0 && s_url[n - 1] == '/') {
    s_url[--n] = '\0';
  }

  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    return;
  }
  prefs.putString(kUrlKey, s_url);
  prefs.end();
}

bool hasUrl() { return s_url[0] != '\0'; }

const char* url() { return s_url; }

}  // namespace services::route_server
