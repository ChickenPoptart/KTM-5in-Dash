# KTM 5" Dash

A 5" touchscreen dash and data logger for a **2020 KTM 690 Enduro R**, a
bike that comes with no instrument cluster. It reads the bike's CAN bus
(listen-only, never transmits), adds GPS, records everything to a MicroSD
card, and shows it on a Waveshare ESP32-S3 5" display.

It follows on from [KTMComputer](https://github.com/ChickenPoptart/KTMComputer),
the same idea on a 2.8" "Cheap Yellow Display". That's where the CAN
decoding was worked out from ride logs.

## What it shows

- **RPM dial** with the gear in the middle and a **two-stage shift light**
  on its ring: steady green when it's nearly time to shift (7,200 rpm),
  flashing red when it's time (7,800 rpm).
- **Speed dial**, with **miles since gas fill** underneath like a trip meter.
  Tap it to reset; the count is kept through key-off.
- **Centre panel**: turn-by-turn directions with ETA from the phone, or a
  **compass** (GPS heading) when there's no route.
- **Top strip**: coolant temperature, elevation, time, phone battery, and
  GPS satellites.
- **Bottom bar**: music controls with the phone connected, or ride stats
  (trip, moving time, average and top speed) without it.

With no bike data coming in, it plays a simulated ride so the screen can be
tuned on the bench. Tapping the screen in the demo steps through the phone
states: route and music, music only, no phone.

## Status

- **Done:** display, touch, CAN, GPS, SD logging, the main screen, the shift
  light and miles since gas fill.
- **Next:** the Bluetooth link to an Android phone (navigation, music, calls,
  texts, weather). Until then, the phone data on screen is the demo's
  sample data.

## Hardware

| Part | Notes |
|---|---|
| Waveshare ESP32-S3-Touch-LCD-5 | 800x480, capacitive touch, CAN transceiver, MicroSD, 6–36 V input |
| u-blox NEO-M8N GPS | Factory default, 9600 baud |
| SC16IS750 I²C-to-UART bridge | The board has no spare UART pins, so the GPS is read over I²C |
| MicroSD card | Every CAN frame and GPS sentence is logged |

Wiring to the bike and the GPS: [docs/wiring.md](docs/wiring.md).

## Building

[PlatformIO](https://platformio.org/):

```
pio run -e waveshare5 -t upload     # build and flash over USB-C
pio test -e native                  # unit tests for the dash logic
```

| Path | What |
|---|---|
| `src/` | Firmware: display, gauges, icons, touch, CAN, GPS, SD logging |
| `lib/dash_logic/` | Pure logic with no hardware (CAN decoding, trip and fuel counts, shift stages…), tested on the computer |
| `test/` | Unit tests for `lib/dash_logic` |
| `tools/make_vlw_font.py` | Turns a TrueType font into the smooth fonts in `src/fonts/` |
| `docs/` | Wiring, CAN signals, log format |

## Safety

The CAN connection is **listen-only**. The firmware never transmits on the
bike's bus. The board's CAN transceiver can also be put into hardware
silent mode (see [docs/wiring.md](docs/wiring.md)).

Ride logs contain GPS positions, so `logs/` is never committed.

## License

- Code: [PolyForm Noncommercial 1.0.0](LICENSE). Free for personal and
  other noncommercial use. For commercial use, open an issue with the
  commercial license template.
- Docs and images: [CC BY-NC-SA 4.0](LICENSE-docs.md).
- Rajdhani font: SIL Open Font License 1.1 ([src/fonts/OFL.txt](src/fonts/OFL.txt)).
