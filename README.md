# FlightScanner for LilyGO T-Encoder Pro

**WIP**

> This is a modification of the work in https://github.com/yashmulgaonkar/FlightScnr
> Go there for all information about the original project.

Buy him a coffee:
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

I added a Sparkfun Electronics Battery Babysitter board which allows the LilyGO device to remain powered on while the battery is being charged. This battery management board has a LiPo fuel gauge function that is available over the I2C bus, and I want to add a page with information about the current state of the battery's charge.

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
