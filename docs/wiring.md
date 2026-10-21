# Wiring

Everything connects to the Waveshare board through its 16-pin screw
terminal. Use crimped ferrules on every wire going into a terminal: bare
stranded wire frays and works loose with vibration.

## Screw terminal

| Pin | Signal | Used for |
|---|---|---|
| 1 | VIN (6–36 V) | Bike switched 12 V, through a 1–2 A inline fuse |
| 2 | GND | Bike ground |
| 3 | VOUT (I²C power, 3.3 V) | Bridge VIN and GPS VCC |
| 4 | GND | Bridge GND and GPS GND |
| 5 | SDA | Bridge SDA |
| 6 | SCL | Bridge SCL |
| 7 | CAN-L | Bike CAN-L |
| 8 | CAN-H | Bike CAN-H |
| 9–16 | RS485, digital in/out | Not used |

Check pin 3 with a meter before connecting the GPS: it should read about
3.3 V. Waveshare can set it to 5 V with a resistor on the board.

## The bike (2020 690 Enduro R)

The dash taps the wires of the offroad ABS dongle connector:

| Bike wire | Signal | Terminal pin |
|---|---|---|
| Tan | Switched 12 V | 1, through the fuse |
| Brown | Ground | 2 |
| Blue/black | CAN-H | 8 |
| Blue/white | CAN-L | 7 |

**Leave CAN termination off** (switch SW2 on the board). The bike's bus is
already terminated: it measures about 60 Ω between CAN-H and CAN-L with
the key off. Adding another 120 Ω would load it down.

The ECU starts sending about 15 s after key-on; until then the dash shows
`--` for engine values.

## GPS through the SC16IS750 bridge

```
Terminal pin 3 (3.3 V) ──┬──► bridge VIN
                         └──► GPS VCC
Terminal pin 4 (GND) ────┬──► bridge GND
                         └──► GPS GND
Terminal pin 5 (SDA) ───────► bridge SDA
Terminal pin 6 (SCL) ───────► bridge SCL

bridge TX ──► GPS RX
bridge RX ◄── GPS TX
```

Jumpers on the bridge board:

| Pad | To | Why |
|---|---|---|
| I2C/SPI | VIN | I²C mode |
| RESET | VIN | Keeps it out of reset |
| A0 | GND | With A1, sets address 0x4D |
| A1 | GND | |

Where two wires share a terminal (pins 3 and 4), twist them together into
one larger ferrule (0.75 mm²). Keep the I²C wires short, under about 30 cm.

The I²C bus is shared with the board's own chips: GT911 touch (0x5D), the
CH422G expander (0x20–0x3F) and the RTC (0x51). The bridge at 0x4D doesn't
clash with any of them.

## Optional: hardware listen-only

The firmware already runs the CAN controller in listen-only mode. To make
it impossible in hardware for the board to drive the bus, put the TJA1051
transceiver in silent mode: its S pin (pin 8) is tied to GND through R96
(0 Ω). Moving it to 3.3 V makes the transceiver receive-only.
