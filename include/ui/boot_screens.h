#pragma once

/** Full-screen messages during WiFi setup and reconnect. */
void bootScreenShowPortalHint();
void bootScreenShowConnectFailed();
void bootScreenShowWifiCleared();

void bootScreenConnectingStart(const char* ssid);
void bootScreenConnectingPulse();
