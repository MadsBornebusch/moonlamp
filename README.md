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

## Programming with the Pocket AVR Programmer

The board has a standard 6-pin AVR ISP header next to the display. Pin 1 (MISO) is the square pad: with the display above the buttons, it is the bottom-right pin.

```
   6 GND    5 RESET
   4 MOSI   3 SCK
   2 VCC   [1] MISO
```

1. Plug in the 6-pin connector with its triangle on the square pad.
2. Power the board from only one source: either set the programmer switch to "Power Target" with USB-C unplugged, or set it to "No Power" and power the board from USB-C.
3. Burn the fuses and bootloader and upload the sketch:

   ```
   pio run -e usbtiny -t bootloader -t upload
   ```

   This erases the chip, so always run both targets together. A plain `-t upload` on a chip that already has a sketch fails verification.

A new chip runs on its 1 MHz factory clock until the fuses are set. Add `upload_flags = -B32` to `[env:usbtiny]` for the first flash.

On Linux, avrdude needs permission to use the programmer. Add a udev rule, then replug the programmer:

```
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="1781", ATTR{idProduct}=="0c9f", MODE="0666"' | sudo tee /etc/udev/rules.d/99-usbtiny.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```
