// Sprout — layered runtime config.
//
// Precedence (later overrides earlier):
//   1. compiled defaults (config.h)
//   2. SD card  /sprout.json   (read at boot, before Wi-Fi)
//   3. web URL  (the "config_url" field, fetched after Wi-Fi connects)
//
// Wi-Fi creds are only read from the local SD card, never from the URL — a
// remote config must not carry a password.
//
// So the device works with no SD card (compiled defaults), or you drop a
// /sprout.json on an SD card to change Wi-Fi / location / behaviour without
// reflashing, or point it at a URL to manage a fleet centrally.
//
// SD is on the board's own VSPI slot (CS 5, SCK 18, MISO 19, MOSI 23) —
// separate from the display/touch HSPI, so there's no bus contention.

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace settings {

inline String   ssid        = WIFI_SSID;
inline String   pass        = WIFI_PASS;
inline String   tz          = TZ_INFO;
inline String   lat         = WX_LAT;
inline String   lon         = WX_LON;
inline bool     demo        = DEMO_DAYNIGHT;
inline uint32_t dayMs       = DEMO_DAY_MS;
inline uint32_t hackAfterMs = 60000;
inline String   configUrl   = CONFIG_URL;
inline String   source      = "compiled";   // where config ended up coming from

inline SPIClass sdSPI(VSPI);

// A coordinate must parse as a plain number — rejects anything that could
// inject into the weather URL (lat/lon may come from untrusted SD/web config).
inline bool validCoord(const String& s) {
  if (s.isEmpty() || s.length() > 12) return false;
  char* end = nullptr;
  strtod(s.c_str(), &end);
  return end && *end == '\0';
}

// allowSecrets: Wi-Fi creds are only honoured from a local source (SD card),
// never from a remote URL — a URL config must not carry a password.
inline void merge(JsonDocument& d, bool allowSecrets) {
  if (allowSecrets) {
    if (d.containsKey("wifi_ssid")) ssid = d["wifi_ssid"].as<String>();
    if (d.containsKey("wifi_pass")) pass = d["wifi_pass"].as<String>();
  }
  if (d.containsKey("tz"))           tz   = d["tz"].as<String>();
  if (d.containsKey("lat")) { String v = d["lat"].as<String>(); if (validCoord(v)) lat = v; }
  if (d.containsKey("lon")) { String v = d["lon"].as<String>(); if (validCoord(v)) lon = v; }
  if (d.containsKey("demo"))         demo = d["demo"].as<bool>();
  if (d.containsKey("day_ms"))       dayMs = d["day_ms"].as<uint32_t>();
  if (d.containsKey("hack_after_s")) hackAfterMs = (uint32_t)(d["hack_after_s"].as<float>() * 1000.0f);
  if (d.containsKey("config_url"))   configUrl = d["config_url"].as<String>();
}

// Read /sprout.json off an SD card if one is present. Fails quietly.
inline bool loadFromSD() {
  sdSPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, sdSPI)) { SD.end(); return false; }
  bool ok = false;
  File f = SD.open("/sprout.json");
  if (f) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) { merge(doc, true); ok = true; source = "sd"; }
    f.close();
  }
  SD.end();
  return ok;
}

// Fetch a JSON config from a URL (needs Wi-Fi up). Won't change the Wi-Fi in
// use this session, but can retune tz / location / behaviour remotely.
inline bool loadFromUrl(const String& url) {
  if (url.isEmpty() || WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  bool ok = false;
  if (http.GET() == 200) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) { merge(doc, false); ok = true; source = "url"; }
  }
  http.end();
  Serial.printf("[cfg] remote %s: %s\n", ok ? "merged" : "skipped", url.c_str());
  return ok;
}

inline void begin() {
  if (loadFromSD()) Serial.println("[cfg] /sprout.json loaded from SD");
  else              Serial.println("[cfg] no SD config; compiled defaults");
}

}  // namespace settings
