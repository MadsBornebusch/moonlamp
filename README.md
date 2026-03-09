# Moonlamp (Månelampe)

Arduino-basert månelampe med RTC (Real-Time Clock) og NeoPixel LED-lys.

## Funksjoner

- Tidsstyrt belysning med programmerbare tider for oransje/grønt lys
- 14 NeoPixel LEDs med flere fargemodi
- 4-sifret 7-segment display
- 3 knapper for kontroll
- RV3032 RTC for nøyaktig tidsstyring

## Brukerveiledning

En interaktiv brukerveiledning er tilgjengelig på:
**[https://mads-[your-github-username].github.io/moonlamp/](https://mads-[your-github-username].github.io/moonlamp/)**

*(Oppdater lenken med ditt GitHub-brukernavn etter at GitHub Pages er aktivert)*

## Oppsett av GitHub Pages

1. Gå til repository-innstillingene på GitHub
2. Naviger til "Pages" i venstre meny
3. Under "Source", velg "Deploy from a branch"
4. Velg `main` branch og `/ (root)` som mappe
5. Klikk "Save"

Siden vil være tilgjengelig på `https://[your-username].github.io/[repository-name]/` etter noen minutter.

## Hardware

- Arduino (kompatibel med ATmega328P)
- RV3032 RTC-modul
- 14x NeoPixel LEDs (WS2812B)
- 4-sifret 7-segment display (felles katode)
- 3x trykknopper

## Programvare

- Arduino IDE
- Adafruit NeoPixel-bibliotek
- RV3032 RTC-bibliotek (inkludert)
