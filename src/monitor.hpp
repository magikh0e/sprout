// Sprout — host up/down monitor via TCP connect.
//
// Opens a TCP connection to settings::monitorHost:monitorPort and treats a
// successful handshake as "up". Proves the service answers, not just that the
// box pings. main.cpp reacts to state changes (annoyed on down, happy on
// recovery).

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "settings.hpp"

namespace monitor {

inline bool enabled = false;
inline bool up      = true;
inline bool known   = false;   // have we taken a first reading yet

inline bool check() {
  WiFiClient c;
  bool ok = c.connect(settings::monitorHost.c_str(), settings::monitorPort, 2500);
  c.stop();
  return ok;
}

}  // namespace monitor
