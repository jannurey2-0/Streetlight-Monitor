# Project Status & Roadmap to 50%

Last updated: 2026-10-24

## Where the project stands today

**Done and working:**

- ✅ Full software chain: Arduino UNO firmware → ESP32 push client (Wi-Fi and server address configured once from a phone via the ESP32's **Streetlight-Setup** hotspot) → dashboard dev server (built-in ingest API) → React UI. Status updates appear live within ~2 seconds, and the dashboard handles the ESP32 going offline without crashing. No more `.env.local` / ESP32 IP configuration.
- ✅ Power design finalized: each streetlight is self-powered — 5V solar panel → TP4056 charger → 18650 battery → IRLZ44N logic-level MOSFET switch → 3W warm-white COB strip. Fully documented in [WIRING.md](WIRING.md), the single source of truth for all wiring.
- ✅ Parts list complete; COB strips ordered. No code changes were needed for the solar upgrade — the firmware stays exactly as-is.

**Not done yet:**

- ❌ Physical build — no streetlight unit has been assembled on a breadboard yet
- ❌ Remaining parts to buy (TP4056 modules, battery holders, IRLZ44N MOSFETs, resistors — all cheap, see list below)
- ❌ Bulb-failure demo feature — designed and agreed, but not coded yet
- ❌ Physical stands/housings so the three lights actually *look* like streetlights
- ❌ Full end-to-end test with real hardware

## What "50% presentation-ready" means

A working demo you can show in front of people:

1. Three solar streetlights standing on their own, each lit by its own battery
2. The dashboard on screen showing the live ON/OFF state of all three
3. Press one button → one streetlight goes dark **and** the dashboard marks it as FAILED
4. Press again → light "repaired", dashboard back to normal

## Next steps, in order

1. **Buy the remaining parts**

   | Item | Qty |
   |---|---|
   | TP4056 USB charging module (with protection) | 3 |
   | 18650 battery (2000mAh+) | 3 |
   | 18650 battery holder | 3 |
   | IRLZ44N logic-level MOSFET | 3 (grab a few spares — **not** the IRFZ44N lookalike) |
   | 220 Ω resistors | 3 (a small pack is fine) |
   | 10 kΩ resistors | 3 (a small pack is fine) |
   | Push button (small tactile switch) | 1 — for the failure demo |

2. **Build and test ONE streetlight unit first.** Charge the battery by plugging any USB charger into the TP4056, then follow WIRING.md section 3 (streetlight unit circuit + assembly checklist). Test with a single jumper from 3.3V/5V before connecting the UNO.
3. **Build the other two units** — same circuit, three times.
4. **Wire all three to the UNO and ESP32** per WIRING.md, and confirm the dashboard mirrors the lights with the existing demo patterns.
5. **Add the bulb-failure feature** (the headline of the presentation):
   - UNO: a push button toggles a simulated failure on one light — the strip turns off and the frame sends `L2:FAIL` instead of `L2:ON`
   - ESP32: accept the `FAIL` value and add it to the JSON API
   - Dashboard: show a red "FAILED" state on that light's card
6. **Make simple stands.** Even cardboard, popsicle sticks, or a small box with a bottle-cap lampshade per light sells the illusion. Don't overbuild — the lights and dashboard carry the demo.
7. **Full rehearsal.** Power everything on, open the dashboard, walk through: normal operation → bulb failure → repair → offline handling (unplug the ESP32).

## After 50% (later phases — context only)

- Real failure detection using LDR light sensors instead of a simulated button
- Battery level monitoring shown on the dashboard
- Failure notifications (email/SMS via a small polling service)
- Data history / cloud logging

These are Phase 2+ items and are deliberately not needed for the first presentation.
