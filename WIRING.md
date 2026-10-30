# Wiring — Smart Streetlight Monitoring System

**This file is the single source of truth for all wiring.** Whenever the wiring changes — a different pin, a new component, a redesign — edit this file and add a changelog entry. Do not document wiring anywhere else.

Last updated: 2026-10-26

## Changelog

| Date | Change |
|---|---|
| 2026-10-26 | Add passive piezo buzzer on ESP32 GPIO23: two-tone startup sound and confirmation sound when internet becomes reachable. |
| 2026-10-26 | Add external red/green ESP32 LEDs for internet reachability on GPIO18/GPIO19, each with a 220 Ω series resistor. |
| 2026-10-24 | Switch device changed from 2N2222 NPN to IRLZ44N logic-level MOSFET (runs cold at 800 mA, no base-current limit, full strip brightness). Per unit: 220 Ω gate series + 10 kΩ gate pulldown; 1 kΩ base resistors no longer used. |
| 2026-10-23 | ESP32 firmware rewritten from on-device REST API to push client with phone setup hotspot. **No wiring changes** — pins identical. |
| 2026-10-23 | v1 — initial design, extracted from the old README §2. Not built yet. |

## 1. System overview

```text
   Streetlight 1 ─┐
   Streetlight 2 ─┤  each self-powered:
   Streetlight 3 ─┘  5V solar panel → TP4056 charger → 18650 battery
                     → IRLZ44N MOSFET switch → 3W warm-white COB strip
        │ 220 Ω gate resistor from UNO D5 / D6 / D7 (+ 10 kΩ gate pulldown)
        │ one GND wire from each unit back to UNO GND
   Arduino UNO (USB powered) — switches the lights, sends state frames
        │ D1 (TX) → 1 kΩ/2 kΩ divider → ESP32 GPIO16 (RX2), 9600 baud
        │ GND ─────────────────────────────► ESP32 GND
   ESP32 (USB powered) — parses frames, pushes state over Wi-Fi
        │ HTTP POST to the dashboard's /api/ingest (PC address entered via phone setup)
   React dashboard (PC) — dev server receives the pushes, UI polls every 2 s
```

## 2. Pin map

### 2.1 Arduino UNO

| UNO pin | Goes to | Purpose |
|---|---|---|
| D5 | 220 Ω → IRLZ44N gate, unit 1 (10 kΩ gate pulldown to GND) | Switch streetlight 1 |
| D6 | 220 Ω → IRLZ44N gate, unit 2 (10 kΩ gate pulldown to GND) | Switch streetlight 2 |
| D7 | 220 Ω → IRLZ44N gate, unit 3 (10 kΩ gate pulldown to GND) | Switch streetlight 3 |
| D1 (TX) | 1 kΩ → divider node → ESP32 GPIO16 | State frames to ESP32, 9600 baud |
| GND | ESP32 GND **and** TP4056 OUT− of all 3 units | Common ground — mandatory |
| USB | Laptop / charger / power bank | UNO power |

### 2.2 ESP32

| ESP32 pin | Goes to | Purpose |
|---|---|---|
| GPIO16 (RX2) | Divider output from UNO TX | State frames in, 9600 baud — **3.3 V max, never wire UNO TX directly** |
| GND | UNO GND | UART common reference |
| GPIO18 | 220 Ω resistor → red LED anode; LED cathode → GND | Internet unavailable indicator |
| GPIO19 | 220 Ω resistor → green LED anode; LED cathode → GND | Internet available indicator |
| GPIO23 | Positive terminal of 3.3 V passive piezo buzzer; negative terminal → GND | Startup and internet-connected sounds |
| USB | Laptop / charger / power bank | ESP32 power |

### 2.3 Passive components

| Component | Qty | Where |
|---|---|---|
| 220 Ω resistor | 3 | MOSFET gate series (D5/D6/D7) |
| 10 kΩ resistor | 3 | MOSFET gate pulldown to GND, one per unit |
| 1 kΩ resistor | 1 | Divider top (UNO TX) |
| 2 kΩ resistor | 1 | Divider bottom (to GND) |
| 220 Ω resistor | 2 | ESP32 red/green indicator LED series resistors |
| Red LED | 1 | ESP32 internet-unavailable indicator |
| Green LED | 1 | ESP32 internet-available indicator |
| 3.3 V passive piezo buzzer | 1 | ESP32 startup and internet-connected sounds |

## 3. Streetlight unit circuit (build ×3, identical)

Each streetlight is self-powered. The battery — not the UNO — powers the COB strip; the UNO only opens/closes the path through the IRLZ44N MOSFET (required because the strip runs on ~3.7 V battery power, not the UNO's 5 V pin).

```text
Solar panel +  ──────────────► TP4056 IN+
Solar panel −  ──────────────► TP4056 IN−
18650 + (via holder) ────────► TP4056 B+
18650 − (via holder) ────────► TP4056 B−

TP4056 OUT+ ─────────────────► COB strip + pad
COB strip − pad ─────────────► IRLZ44N drain
IRLZ44N source ──────────────► TP4056 OUT−

UNO D5 ──[220 Ω]──●──────────► IRLZ44N gate   (D6 for unit 2, D7 for unit 3)
                   │
                 [10 kΩ]
                   │
UNO GND ───────────┴─────────► TP4056 OUT−    (one ground wire per unit)
```

**No resistor on the strip** — it is made for 3 V / 3.7 V, exactly what one 18650 provides, so it connects straight to the charger output. The strip pads are marked **+** and **−**; get them right and don't bend the strip sharply.

### 3.1 Assembly checklist (per unit)

1. [ ] Panel + → TP4056 IN+, panel − → IN−
2. [ ] Battery holder + → B+, holder − → B−
3. [ ] TP4056 OUT+ → COB strip + pad
4. [ ] Strip − pad → IRLZ44N drain (middle leg)
5. [ ] IRLZ44N source → TP4056 OUT−
6. [ ] 10 kΩ gate→source pulldown, then smoke test **before connecting the UNO**: battery in → strip must stay dark (gate held low by the 10 kΩ); briefly touch the gate to 5 V → strip lights
7. [ ] UNO D5/D6/D7 → 220 Ω → gate
8. [ ] UNO GND → TP4056 OUT−

### 3.2 Build notes

- **Battery polarity matters** — B+ to the battery's positive (nub) end. Wiring it backwards can kill the charger board.
- **MOSFET legs (TO-220)** — flat face toward you, legs down: **gate, drain, source** left to right. Always fit the 10 kΩ gate–source pulldown — with the gate floating, the UNO's boot-time pin states can make the strip glow dimly.
- **IRLZ44N, not IRFZ44N** — the IRFZ44N lookalike is not logic-level and only partly turns on from a 5 V gate, so the strip runs dim and the MOSFET heats up. Check the part number printed on the tab.
- Strip draws ~800 mA (3 W); a full 18650 runs one light ~3 hours. Brightest on a fresh battery, dims as it drains — normal, not a fault.
- Three 2.5 W panels can't sustain three 3 W strips nonstop — top up between demos (sun, or any USB phone charger into the TP4056).
- The IRLZ44N runs cold at 800 mA — on-resistance is ~25 mΩ at a 5 V gate, so it dissipates only tens of milliwatts. If it gets hot, the wiring is wrong or it's an IRFZ44N (see above).
- Panels come with bare wires — solder, or twist firmly and tape so the strands can't touch.

## 4. UNO → ESP32 UART link (voltage divider — required)

UNO logic is 5 V; ESP32 GPIO pins are **3.3 V and not 5 V tolerant**. The divider drops the level safely:

```text
UNO TX (D1) ──[ 1 kΩ ]──●──────► ESP32 GPIO16 (RX2)
                        │
                      [ 2 kΩ ]
                        │
UNO GND ────────────────┴──────► ESP32 GND
```

With 1 kΩ / 2 kΩ: `5 V × 2k / (1k + 2k) = 3.33 V` — safe for the ESP32.

- **Common ground is mandatory** — UNO GND to ESP32 GND, or no data arrives.
- Only the TX line needs the divider. There is no UNO ← ESP32 RX path in this phase.
- If UNO uploads fail, temporarily unplug the wire leaving D1 (TX) — the serial hardware is shared with USB.
- Baud rate on this link is 9600. (The ESP32's own USB debug monitor runs at 115200 — unrelated to this link.)

## 5. Power distribution

| Powered by | What |
|---|---|
| 18650 battery (via TP4056 OUT) | COB LED strips only |
| USB (laptop / charger / power bank) | UNO and ESP32 |

The batteries never power the boards. For a fully unplugged demo, one USB power bank feeding both boards works.

## 6. Planned — not wired yet

| Feature | Wiring plan | Status |
|---|---|---|
| Bulb-failure demo push button | Push button on the UNO (pin TBD when the feature is coded); toggles one light to `FAIL` | Designed, not coded or wired |
| LDR real-failure detection | Light sensors per unit | Phase 2+, deliberately out of scope |
