// Sprout config. Local, not secret-safe for sharing — don't commit real
// Wi-Fi creds to a public remote.
#pragma once

// --- Real time over NTP (used only when DEMO_DAYNIGHT is 0) ---
// Leave WIFI_SSID empty to run an offline clock (starts at noon, real speed).
#define WIFI_SSID        ""
#define WIFI_PASS        ""
#define GMT_OFFSET_SEC   0        // your UTC offset in seconds, e.g. -18000 = EST
#define DST_OFFSET_SEC   0        // add 3600 while DST is in effect

// --- Demo day/night ---
// 1 = accelerate time so one full day passes every DEMO_DAY_MS (watch the
//     dawn/day/dusk/night behaviour in seconds, no Wi-Fi needed).
// 0 = real time (NTP if creds set above, else offline clock).
#define DEMO_DAYNIGHT    1
#define DEMO_DAY_MS      120000UL  // one simulated day = 2 min (watchable)
