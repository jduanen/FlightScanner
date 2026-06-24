# FlightScanner for LilyGO T-Encoder Pro

**WIP**

> This is a modification of the work in https://github.com/yashmulgaonkar/FlightScnr.
> 
> Go there for all information about the original project and buy him a coffee:
<p align="center">
  <a href="https://buymeacoffee.com/yashmulgaonkar" target="_blank">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" style="height: 35px;">
  </a>
</p>

Firmware is released under **[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)** (see [LICENSE](LICENSE)) instead of MIT. Permissive MIT licensing on similar projects has made it easy for vendors to ship closed derivatives without keeping firmware open to the community. This license keeps the source shareable for hobbyists and open-source builders while discouraging proprietary takeaways.

## Modifications to the original project and their motivations

I attempted to leave as much as possible of the excellent original project intact, while adding support for my local services and implementation choices.

One of the bigger changes to the project is to make the device battery-powered so that it can be carried around the house with me. To this end, I added a battery with a Qi-based wireless charging capability.

I want the device to stay powered on at all times so that it will provide instant information. I also want the battery and display to last as long as possible, so I want to implement a screen blanking function that triggers after some period of time (or whenever the device is placed on a wireless charger). Whenever the display is blanked, tapping the touchscreen (or removing it from the charger) with result in the display being instantly turned back on.

I added a screen-blanking function that turns the display on whenever the screen is touched (or the device is removed from the charger) and starts a timer that is reset every time the device is touched again. When this timer expires, the display is turned off. The display is turned on at boot time, when the screen is touched, or when the device is removed from the charging pad. The display remains on until a given time interval after the last time it was touched.

#### Screen Blanking Implementation

The blanking timeout is configurable via the settings web page ("Screen blanking" dropdown). Available options are Never, 30 seconds, 1 minute, 2 minutes, 5 minutes, and 10 minutes. The default is 1 minute. The setting is stored in NVS under the key `blank_to_s` and survives reboots.

Behavior:
- The idle timer starts when setup completes (after Wi-Fi connects and the boot splash appears).
- Any touch, swipe, knob press, or encoder rotation resets the timer. If the display was off, the triggering input wakes the display but is otherwise discarded — no accidental navigation occurs on wake.
- The radar sweep animation is paused while the display is off to avoid unnecessary SPI writes.
- `hardware::displayBlankingNotifyCharging()` and `hardware::displayBlankingNotifyUncharging()` are provided as hooks for future Battery Babysitter integration (blank on charger placement, wake on removal). These are not yet called because the Battery Babysitter I2C driver has not been added.

I added a Sparkfun Electronics Battery Babysitter board which allows the LilyGO device to remain powered on while the battery is being charged. This battery management board has a LiPo fuel gauge function that is available over the I2C bus, and I want to add a page with information about the current state of the battery's charge.

### Settings REST API

All settings on the `flightscanner.local` page are also accessible via a REST API served on port 80. Unlike the HTML form, API writes take effect immediately without a reboot (brightness is applied live; route server URL takes effect on the next lookup).

#### GET /api/settings

Returns all current settings as JSON.

```
GET http://flightscanner.local/api/settings
```

```bash
curl -X GET http://flightscanner.local/api/settings | jq '.'
```

```json
{
  "radar_center": "37.619770, -122.372270",
  "use_miles": false,
  "show_cardinals": true,
  "show_sweep": true,
  "detail_timeout": 10,
  "bright_pct": 80,
  "blank_timeout": 60,
  "ui_beep": true,
  "beep_tone": "B",
  "min_height": 500,
  "range_idx": 2,
  "route_server_url": "http://192.168.1.x:5000"
}
```

#### POST /api/settings

Updates one or more settings. Send a JSON body with only the fields you want to change. Returns the full settings object after applying changes.

```
POST http://flightscanner.local/api/settings
Content-Type: application/json

{"bright_pct": 85, "blank_timeout": 120}
```

```bash
curl -X POST http://flightscanner.local/api/settings -H 'Content-Type: application/json' -d '{"bright_pct": 85, "blank_timeout": 120}'

```

| Field | Type | Values |
|---|---|---|
| `radar_center` | string | `"lat, lon"` decimal degrees |
| `use_miles` | bool | |
| `show_cardinals` | bool | |
| `show_sweep` | bool | |
| `detail_timeout` | int | `0` (manual), `10`, `20`, `30` |
| `bright_pct` | int | `20`, `40`, `60`, `80`, `100` |
| `blank_timeout` | int | `0` (never), `30`, `60`, `120`, `300`, `600` |
| `ui_beep` | bool | |
| `beep_tone` | string | `"A"` – `"E"` |
| `min_height` | int | feet, `0` = off |
| `range_idx` | int | `0` – `N-1` (see radar scale bands) |
| `route_server_url` | string | base URL, e.g. `"http://192.168.1.x:5000"` |

Invalid or out-of-range values for a field are silently ignored (the field is left unchanged).

#### POST /api/reboot

Triggers a clean reboot. Use this after changes that require it (e.g. Wi-Fi settings).

```
POST http://flightscanner.local/api/reboot
```

```json
{"status": "rebooting"}
```

### Use local ADS-B and Route Service

I have my own (unfiltered, 1080MHz and 980MHz) ADS-B receiver as well as a LAN-based service that provides additional information on flights (such as airline, route, airport, etc.) that I would like to use instead of the built in web-based services in the original project. The information server handles caching as well as falling back to different cloud-based APIs for this, as well as other applications in my home.

### Add support for Sparkfun Battery Babysitter

The Sparkfun Battery Babysitter board has an I2C interface to it's battery management IC. By connecting this interface to the T-Encoder's 3.3V Qwiic port, I will enable a screen to show the current amount of charge in the battery. Furthermore, with this interface I can also determine when the device has been placed on a Qi charger, and when the device has been removed from the charger (and use this event to reset the screen-blanking timeout and enable the display if it was turned off). In this way, I can have the device remain active, but with the display blanked until it is picked up off of the charger (or when the screen is touched).

## Hardware

The hardware that I'm using a LilyGO T-Encoder Pro with a Sparkfun Battery Babysitter, a Qi Wireless Charging Receiver, and a LiPo battery. I also 3D modeled and printed an enclosure for these components.

### LilyGO T-Encoder Pro

* Key Features
  - ESP32-S3 R8
  - Flash: 16 MB
  - PSRAM: 8 MB
  - WiFi and BTLE5
  - 2x Qwiic 4x pin
  - USB-C
  - 1.2" 390x390 AMOLED Display (SH8601A)
    * QSPI interface
  - CHSC5816 Touchscreen Controller and Rotary Encoder
  - Buzzer
  - 5V @ 500mA

**image here**

### Power Supply

#### Sparkfun Battery Babysitter

* Key Features
  - allows charging of battery and suppling the output load with 5V
  - ????

**image here**

#### Qi Wireless Charging Receiver

The wireless charging receiver unit consists of a coil and a small PCB. Most models also provide a self-adhesive-backed sheet of magnetic material to put on the back of the coil to improve the energy transfer and reduce the field effects on the colocated electronics.

**image here**


## License

### Firmware license

Original application code, tools, and documentation in this repository, as well as all modifications to it, are licensed under **[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/)** ([LICENSE](LICENSE)).

- **Attribution:** credit the author and link to the license when you share or adapt this work.
- **NonCommercial:** you may not use this material for commercial purposes without separate permission.
- **ShareAlike:** adaptations must be released under the same license.

Vendored libraries (`lib/Arduino_GFX`, `lib/SensorLib`, and PlatformIO registry dependencies) remain under **their own licenses** (GPL, MIT, etc.). Combining them into a binary does not re-license those components. Comply with each upstream license when you distribute builds.

---
