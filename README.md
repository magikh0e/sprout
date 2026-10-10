# Sprout

An ambient desk companion for the 3.5" Cheap Yellow Display
(Sunton **ESP32-3248S035R**, ST7796 320×480, resistive XPT2046 touch).

Not a stat-tending Tamagotchi — a little creature that *lives* on your desk:
breathes, blinks, reacts when you touch it, and knows day from night (real
clock + the onboard light sensor), dozing when the room goes dark.

## Status

MVP complete and running on hardware. The creature breathes, blinks, reacts to
touch, chats in IRC style, and has a day/night rhythm with a hacking mode.

- **Day:** idle (breathe + blink). Tap him → happy hop + chirp + green pop.
  Left alone a minute → slips into hacking mode (focused squint, leans at the
  laptop, code scrolls, amber LED).
- **Night:** always hacking (night owl). Disturb his session → annoyed:
  startled eyes, red flash, low buzz, a cranky line.
- Touch is restricted to his body. Random chatter is throttled to ~1 line / 5 min.

Time is demo-fast by default (one day per `DEMO_DAY_MS`); set `DEMO_DAYNIGHT 0`
and fill the Wi-Fi/timezone fields in `src/config.h` for real NTP time.

## Configuration

Settings layer, later overriding earlier:

1. **Compiled defaults** — `src/config.h` (gitignored; copy from
   `src/config.example.h`).
2. **SD card** — drop a `/sprout.json` on a FAT32 card (see
   `sprout.example.json`). Read at boot, before Wi-Fi, so it can set the
   network itself. Change Wi-Fi, location, timezone, or behaviour with no
   reflash.
3. **Web URL** — if the config has a `config_url`, that JSON is fetched after
   Wi-Fi connects and merged on top (retune timezone / location / behaviour
   remotely; Wi-Fi itself applies next boot).

Keys: `wifi_ssid`, `wifi_pass`, `tz` (POSIX), `lat`, `lon`, `demo` (bool),
`day_ms`, `hack_after_s`, `config_url`. All optional — anything omitted keeps
the compiled default. The SD slot is on the board's own VSPI (CS 5), separate
from the display, so it never fights the screen for the bus.

Built in five steps: bring-up → static scene + IRC log → breathe/blink →
touch reactions → day/night + hacking mode.

Note: the 3.5" 035 board has **no onboard LDR**, so day/night runs off the
clock, not a light sensor.

## Pin map (bare CYD — no radios, nothing contends for SPI)

| Function | Pin(s) | Notes |
|---|---|---|
| Display ST7796 (HSPI) | SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST −1 | BGR, inversion off |
| Backlight | **27** | active HIGH, PWM = brightness (pueo proved it's 27, not 21) |
| Touch XPT2046 | CS 33 (shares display SPI) | single-point resistive |
| Speaker | 26 | SPEAK1; unused until step 4 |
| RGB LED | R 4, G 16, B 17 | **common-anode = active LOW**; G/B order to confirm in step 1 |
| LDR (light) | **34** `[verify]` | analog; cover it and watch the value move |

Pins are from the pueo project's on-hardware measurements. The two marked for
confirmation (LDR pin, RGB channel order) are exactly what the bring-up sketch
exists to settle.

## Build & flash

```bash
pio run -t upload && pio device monitor
```

Then on the board:

- **Tap the screen** — a cyan dot appears where you pressed; coords print to serial.
- **LDR** — the yellow bar and number track room light. Cover the sensor with
  a thumb: the value should swing a lot. If it barely moves, the LDR isn't on 34.
- **RGB LED** — cycles red → green → blue → off, once a second, with the pin
  it's driving labelled on screen. Confirm the physical colour matches the label.
