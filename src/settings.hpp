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
#include "mbedtls/md.h"
#include "config.h"

namespace settings {

inline String   ssid        = WIFI_SSID;
inline String   pass        = WIFI_PASS;
inline String   tz          = TZ_INFO;
inline String   lat         = WX_LAT;
inline String   lon         = WX_LON;
inline bool     demo        = DEMO_DAYNIGHT;
inline bool     hour24      = CLOCK_24H;      // true = 24h clock, false = 12h am/pm
inline uint32_t dayMs       = DEMO_DAY_MS;
inline uint32_t hackAfterMs = 60000;
inline String   configUrl   = CONFIG_URL;
inline bool     hnEnabled   = HN_ENABLED;
inline String   monitorHost = MONITOR_HOST;
inline int      monitorPort = MONITOR_PORT;
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
  // Wi-Fi defaults to config.h. Only a local (SD) config that actually names
  // a network replaces it — and then ssid+pass move together, so you can't end
  // up with the SD ssid against the config.h password.
  if (allowSecrets && d.containsKey("wifi_ssid")) {
    ssid = d["wifi_ssid"].as<String>();
    pass = d.containsKey("wifi_pass") ? d["wifi_pass"].as<String>() : String("");
  }
  if (d.containsKey("tz"))           tz   = d["tz"].as<String>();
  if (d.containsKey("hour24"))       hour24 = d["hour24"].as<bool>();
  if (d.containsKey("lat")) { String v = d["lat"].as<String>(); if (validCoord(v)) lat = v; }
  if (d.containsKey("lon")) { String v = d["lon"].as<String>(); if (validCoord(v)) lon = v; }
  if (d.containsKey("demo"))         demo = d["demo"].as<bool>();
  if (d.containsKey("day_ms"))       dayMs = d["day_ms"].as<uint32_t>();
  if (d.containsKey("hack_after_s")) hackAfterMs = (uint32_t)(d["hack_after_s"].as<float>() * 1000.0f);
  if (d.containsKey("config_url"))   configUrl = d["config_url"].as<String>();
  if (d.containsKey("hn"))           hnEnabled = d["hn"].as<bool>();
  if (d.containsKey("monitor_host")) monitorHost = d["monitor_host"].as<String>();
  if (d.containsKey("monitor_port")) monitorPort = d["monitor_port"].as<int>();
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

inline bool httpGetBody(const String& url, String& out) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(20);
  HTTPClient http;
  http.setConnectTimeout(20000);
  if (!http.begin(client, url)) return false;
  bool ok = (http.GET() == 200);
  if (ok) out = http.getString();
  http.end();
  return ok;
}

inline void hmacSha256Hex(const String& key, const String& msg, char out[65]) {
  uint8_t mac[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info, (const uint8_t*)key.c_str(), key.length(),
                  (const uint8_t*)msg.c_str(), msg.length(), mac);
  for (int i = 0; i < 32; i++) sprintf(out + i * 2, "%02x", mac[i]);
  out[64] = '\0';
}

// Constant-time compare of a lowercase-hex MAC against an untrusted hex string.
inline bool hexEqualCT(const char* mac, String sig) {
  sig.trim();
  if (sig.length() != 64) return false;
  uint8_t diff = 0;
  for (int i = 0; i < 64; i++) {
    char c = sig[i];
    if (c >= 'A' && c <= 'F') c += 32;          // lower-case the server value
    diff |= (uint8_t)mac[i] ^ (uint8_t)c;
  }
  return diff == 0;
}

// Fetch a JSON config from a URL (needs Wi-Fi up). Never changes Wi-Fi creds.
// If CONFIG_HMAC_KEY is set, the body must carry a valid HMAC-SHA256 at
// <url>.sig or it is rejected.
inline bool loadFromUrl(const String& url) {
  if (url.isEmpty() || WiFi.status() != WL_CONNECTED) return false;

  String body;
  if (!httpGetBody(url, body)) { Serial.printf("[cfg] remote skipped: %s\n", url.c_str()); return false; }

  String key = CONFIG_HMAC_KEY;
  if (key.length()) {
    String sig;
    if (!httpGetBody(url + ".sig", sig)) { Serial.println("[cfg] no .sig, rejected"); return false; }
    char mac[65];
    hmacSha256Hex(key, body, mac);
    if (!hexEqualCT(mac, sig)) { Serial.println("[cfg] bad signature, rejected"); return false; }
    Serial.println("[cfg] signature ok");
  }

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) { Serial.println("[cfg] remote bad json"); return false; }
  merge(doc, false);
  source = "url";
  Serial.printf("[cfg] remote merged: %s\n", url.c_str());
  return true;
}

inline void begin() {
  if (loadFromSD()) Serial.println("[cfg] /sprout.json loaded from SD");
  else              Serial.println("[cfg] no SD config; compiled defaults");
}

}  // namespace settings
