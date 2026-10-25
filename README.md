# Moonlamp

Arduino-based moon lamp with an RTC (real-time clock) and NeoPixel LEDs.

## Features

- Timed lighting with programmable times for orange/green light
- 14 NeoPixel LEDs with several colour modes
- 4-digit 7-segment display
- 3 control buttons
- RV3032 RTC for accurate timekeeping

## User guide

An interactive user guide is available at:
**[https://MadsBornebusch.github.io/moonlamp/](https://MadsBornebusch.github.io/moonlamp/)**


## Hardware

- Arduino (ATmega328P compatible)
- RV3032 RTC module
- 14x NeoPixel LEDs (WS2812B)
- 4-digit 7-segment display (common cathode)
- 3x push buttons

## Software

- Arduino IDE
- Adafruit NeoPixel library
- RV3032 RTC library (included)
- PlatformIO (optional): see `platformio.ini`. `pio run -e uart -t upload` uploads through the bootloader (press reset when the upload starts).
