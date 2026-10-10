// Sprout — persistent state in NVS (survives reboots), so he has continuity.
//
// Tracks how many times he's been powered on and how many pokes he's gotten.
// Pokes are flushed at most once a minute (and on demand) to spare flash wear.

#pragma once
#include <Arduino.h>
#include <Preferences.h>

namespace store {

inline Preferences prefs;
inline uint32_t boots = 0, pokes = 0;
inline bool     dirty = false;
inline uint32_t lastSave = 0;

inline void begin() {
  prefs.begin("sprout", false);
  boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);     // boot count: one write per power-on
  pokes = prefs.getUInt("pokes", 0);
  lastSave = millis();
}

inline void addPoke() { pokes++; dirty = true; }

inline void flush(uint32_t now, bool force = false) {
  if (dirty && (force || now - lastSave >= 60000)) {
    prefs.putUInt("pokes", pokes);
    dirty = false;
    lastSave = now;
  }
}

}  // namespace store
