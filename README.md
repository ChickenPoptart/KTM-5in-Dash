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
  (trip, moving time, average and top speed) without it. Calls and messages
  show here as a banner.
- **Brightness and look**: the sun icon at the top right dims the screen for
  night riding, and switches between the **Modern** dash and a **Retro**
  one.
- **Retro**: an 80s digital dash in KTM orange, after the C4 Corvette and
  Astra GTE. Curved bar-graph tach and speedo, seven-segment digits over
  their unlit "8"s, fourteen-segment scrolling text, dot-matrix turn arrows,
  a coolant bar, and a power-on self-test that lights every segment.
- **More pages**: swipe left for **Trips** (this ride, and Trip 1 and Trip 2
  kept until you reset them, like a car's) and **Charts** (speed, RPM,
  coolant and elevation over the whole ride). Double-tap to get back to the
  dash.

The bench build (`pio run -e waveshare5_demo -t upload`) adds a demo: hold
the dash for a second for a simulated ride, route, song, text and call, for
tuning the screen or recording video. The normal build leaves it out, so a
long press on the bike can never show fake data.

## Phone setup (Android)

1. Install **Chronos** from the Play Store and allow its permissions,
   including notification access. Give Google Maps notification access too.
2. Pair from inside Chronos (Watches → Pair new device → **KTM Dash**), not
   from Android's Bluetooth settings.
3. In Chronos, under the watch's **ESP32 Manager**, set **Navigation** to
   Enabled.

## Status

- **Done:** display, touch, CAN, GPS, SD logging, the main screen, the shift
  light, miles since gas fill, screen brightness, and the phone link
  (Android, through the [Chronos](https://chronos.ke/) app): Google Maps
  turn-by-turn with the dash's own arrows, music with working buttons, call
  and message banners, weather and phone battery.
- **Also done:** trip meters, ride charts, the retro look, and a watchdog
  that restarts the dash if it ever hangs.
- **Next:** testing on the bike with live CAN data, and a case for the 5"
  board.

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
pio run -e waveshare5 -t upload       # build and flash over USB-C
pio run -e waveshare5_demo -t upload  # the bench build, with the demo
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
- Fonts: Rajdhani and DSEG, both SIL Open Font License 1.1
  ([src/fonts/OFL.txt](src/fonts/OFL.txt),
  [src/fonts/DSEG-LICENSE.txt](src/fonts/DSEG-LICENSE.txt)).
