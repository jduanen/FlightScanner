#pragma once

/**
 * Apply portal/web form fields and persist to NVS.
 * Web: pass radar_center as "lat, lon". Leave lat_str/lon_str null.
 * Captive portal (4.3.2.1) is Wi‑Fi only and does not call this function.
 * Returns false if coordinates invalid.
 */
bool settingsApplyFromForm(const char* radar_center_str, const char* lat_str,
                           const char* lon_str, const char* miles_checkbox,
                           const char* cardinals_checkbox,
                           const char* min_height_str, const char* range_index_str,
                           const char* route_server_url,
                           const char* ui_beep_checkbox, const char* beep_tone_str,
                           const char* bright_pct_str, const char* sweep_line_checkbox,
                           const char* detail_timeout_str,
                           const char* blank_timeout_str);
