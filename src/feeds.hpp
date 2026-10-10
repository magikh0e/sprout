// Sprout — Hacker News headline reader.
//
// Pulls the hnrss.org front-page feed and extracts item <title>s with light
// string scanning (RSS is XML; a full parser is overkill here). Headlines
// scroll across a ticker line; see main.cpp.

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "settings.hpp"

namespace feeds {

constexpr int MAX_ITEMS = 8;
inline String items[MAX_ITEMS];
inline int    count = 0;

inline String decode(String s) {
  s.replace("<![CDATA[", "");
  s.replace("]]>", "");
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&#39;", "'");
  s.replace("&#x27;", "'");
  s.replace("&#x2F;", "/");
  s.trim();
  return s;
}

inline bool fetchHN() {
  if (!settings::hnEnabled || WiFi.status() != WL_CONNECTED) return false;

  Serial.printf("[hn] heap=%u\n", ESP.getFreeHeap());
  WiFiClientSecure client;
  client.setInsecure();                 // public feed; see settings.hpp TLS note
  client.setHandshakeTimeout(20);       // hnrss is slower than open-meteo
  HTTPClient http;
  http.setConnectTimeout(20000);
  if (!http.begin(client, "https://hnrss.org/frontpage?count=8")) return false;

  int fresh = 0;
  int code = http.GET();
  if (code != 200) { Serial.printf("[hn] GET=%d, retrying\n", code); delay(400); code = http.GET(); }
  if (code == 200) {
    String x = http.getString();
    int pos = 0;
    bool firstTitle = true;             // first <title> is the channel name
    while (fresh < MAX_ITEMS) {
      int ts = x.indexOf("<title>", pos);
      if (ts < 0) break;
      int te = x.indexOf("</title>", ts);
      if (te < 0) break;
      String t = x.substring(ts + 7, te);
      pos = te + 8;
      if (firstTitle) { firstTitle = false; continue; }
      t = decode(t);
      if (t.length() > 80) t = t.substring(0, 80);
      if (t.length()) items[fresh++] = t;
    }
  }
  http.end();
  if (fresh) count = fresh;
  Serial.printf("[hn] %d headlines\n", fresh);
  return fresh > 0;
}

}  // namespace feeds
