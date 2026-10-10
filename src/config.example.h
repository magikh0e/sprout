// Copy this to config.h and fill it in. config.h is gitignored so your
// Wi-Fi password never gets committed.
#pragma once

// --- Wi-Fi (used only when DEMO_DAYNIGHT is 0) ---
#define WIFI_SSID   "your-network"
#define WIFI_PASS   "your-password"

// POSIX TZ string — handles DST automatically. Common ones:
//   US Eastern  "EST5EDT,M3.2.0,M11.1.0"
//   US Central  "CST6CDT,M3.2.0,M11.1.0"
//   US Mountain "MST7MDT,M3.2.0,M11.1.0"
//   US Pacific  "PST8PDT,M3.2.0,M11.1.0"
//   UK          "GMT0BST,M3.5.0/1,M10.5.0"
//   Central EU  "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO     "EST5EDT,M3.2.0,M11.1.0"
#define CLOCK_24H   0         // 1 = 24-hour clock, 0 = 12-hour am/pm

// Location for weather (Open-Meteo, no API key). Find yours on any map.
#define WX_LAT      "21.3069"
#define WX_LON      "-157.8583"

// Remote config fetched after Wi-Fi ("" to disable). Merged over compiled + SD.
#define CONFIG_URL  ""

// Hacker News headline ticker.
#define HN_ENABLED   1

// Host up/down monitor via TCP connect ("" to disable).
#define MONITOR_HOST ""
#define MONITOR_PORT 443

// --- Demo day/night ---
// 1 = fast simulated day (no Wi-Fi needed). 0 = real time via NTP.
#define DEMO_DAYNIGHT 0
#define DEMO_DAY_MS   120000UL
