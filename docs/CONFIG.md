# Configuration reference

Sprout's settings are **layered** — each source overrides the one before it:

1. **Compiled defaults** — `#define`s in `src/config.h` (gitignored; copy from
   `src/config.example.h`). Baked in at build time.
2. **SD card** — a `/sprout.json` file on a FAT32 card, read at boot **before
   Wi-Fi** (so it can set the network itself).
3. **Web URL** — the JSON at `config_url`, fetched **after Wi-Fi connects** and
   merged on top.

Anything a layer doesn't specify keeps the value from the layer below. With no
SD card and no reachable URL, Sprout just runs on the compiled defaults.

```
config.h  ──►  /sprout.json (SD)  ──►  config_url (web)
 lowest                                     highest
```

## Keys

Each key can appear in the SD/URL JSON. The matching `config.h` macro sets the
compiled default.

| JSON key | `config.h` macro | Type | Default | Sources | Notes |
|---|---|---|---|---|---|
| `wifi_ssid` | `WIFI_SSID` | string | `""` | compiled, **SD only** | Wi-Fi network name |
| `wifi_pass` | `WIFI_PASS` | string | `""` | compiled, **SD only** | Wi-Fi password |
| `tz` | `TZ_INFO` | POSIX TZ string | `"HST10"` | all | DST-aware timezone |
| `hour24` | `CLOCK_24H` | bool | `false` | all | `true` = 24-hour clock, `false` = 12-hour am/pm |
| `lat` | `WX_LAT` | number (string) | `"21.3069"` | all | Weather latitude (validated numeric) |
| `lon` | `WX_LON` | number (string) | `"-157.8583"` | all | Weather longitude (validated numeric) |
| `demo` | `DEMO_DAYNIGHT` | bool | `false` | all | Fast simulated day/night (no Wi-Fi needed) |
| `day_ms` | `DEMO_DAY_MS` | uint (ms) | `120000` | all | Length of one simulated day in demo mode |
| `hack_after_s` | — | number (s) | `60` | all | Daytime idle seconds before he slips into hacking |
| `hn` | `HN_ENABLED` | bool | `true` | all | Hacker News headline ticker |
| `monitor_host` | `MONITOR_HOST` | string | `""` | all | TCP host to monitor; `""` disables |
| `monitor_port` | `MONITOR_PORT` | int | `443` | all | Port for the monitor connect check |
| `config_url` | `CONFIG_URL` | string | *(your URL)* | all | Remote config to fetch; `""` disables |
| — | `CONFIG_HMAC_KEY` | string | `""` | **compiled only** | Secret that signs the remote config (below) |

### Example `sprout.json`

```json
{
  "wifi_ssid": "your-network",
  "wifi_pass": "your-password",
  "tz": "HST10",
  "hour24": false,
  "lat": "21.3069",
  "lon": "-157.8583",
  "demo": false,
  "hack_after_s": 60,
  "hn": true,
  "monitor_host": "example.com",
  "monitor_port": 443,
  "config_url": ""
}
```

## Timezone (`tz`)

A POSIX TZ string, which handles daylight saving automatically:

| Zone | Value |
|---|---|
| Hawaii (no DST) | `HST10` |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0` |
| US Central | `CST6CDT,M3.2.0,M11.1.0` |
| US Mountain | `MST7MDT,M3.2.0,M11.1.0` |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0` |
| UK | `GMT0BST,M3.5.0/1,M10.5.0` |
| Central Europe | `CET-1CEST,M3.5.0,M10.5.0/3` |

## Location (`lat` / `lon`)

Decimal degrees for the weather lookup (Open-Meteo, no API key). Find yours on
any map. They're validated as plain numbers before use, so a malformed value is
ignored rather than trusted.

## Demo mode (`demo` / `day_ms`)

With `demo` true, Sprout ignores the real clock and runs a fast simulated day —
one full cycle every `day_ms` — so you can watch dawn → day → dusk → night (and
hacking mode) in seconds without Wi-Fi. Set `demo` false for real NTP time.

## SD card

Format a microSD as **FAT32** and put `sprout.json` at the **root** (`/sprout.json`).
The SD slot is on the board's own VSPI bus (CS 5), separate from the display, so
it never contends with the screen. A missing or unreadable card is fine — Sprout
falls back to the compiled defaults.

You can also drop a **`/splash.jpg`** (or `/splash.png`), 320×480, on the card to
replace the boot splash with your own image.

## Security

### Wi-Fi credentials are never read from the URL config

`wifi_ssid` / `wifi_pass` are honoured **only from local sources** (compiled
config or the SD card), never from the fetched URL:

- **It would leak your password** — a config served over the network puts your
  Wi-Fi password on a web server, and the fetch skips TLS certificate
  verification, so it's not a safe place for a secret.
- **It's pointless** — the URL config is fetched *after* the device is already
  online, so credentials in it couldn't connect anything this session.

Keep Wi-Fi creds in `config.h` or on the SD card; use the URL only to retune
non-secret things (timezone, location, feeds, monitor target, behaviour).

### Signed remote config (`CONFIG_HMAC_KEY`)

To make the remote config tamper-resistant, set `CONFIG_HMAC_KEY` to a secret
(compile-time only — it can't be set over the network). The device then fetches
`<config_url>.sig` and rejects the config unless its HMAC-SHA256 matches. An
empty key leaves the config unsigned (trusted as-is).

Sign the hosted file whenever you change it:

```bash
openssl dgst -sha256 -hmac "YOUR_SECRET" sprout.json | awk '{print $2}' > sprout.json.sig
```

Host `sprout.json` and `sprout.json.sig` together. On the device you'll see
`[cfg] signature ok` → `remote merged`, or `bad signature, rejected` on a
mismatch.
