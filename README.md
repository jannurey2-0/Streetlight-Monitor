# Smart Streetlight Monitoring System

Phase 1 — monitor the ON/OFF state of 3 LED streetlights driven by an Arduino UNO, relayed over UART to an ESP32, and visualized in a separate React dashboard. No sensors, no database, no cloud.

```text
3 solar streetlights   (each: 5V panel + TP4056 charger + 18650 battery + COB LED strip)
       |
  Arduino UNO          (switches the lights on D5/D6/D7 via MOSFETs, sends state frames)
       | UART @ 9600 baud (voltage divider on TX line)
      ESP32             (parses frames, pushes the state over Wi-Fi to your PC)
       | HTTP POST to the dashboard's /api/ingest (PC address entered once from your phone)
  React Dashboard      (Vite dev server on your PC receives the pushes, UI polls every 2 s)
```

Each streetlight is self-powered: its solar panel charges its own 18650 through a TP4056 charging module, and the battery — not the UNO — powers its LED. The UNO only opens/closes the path through a MOSFET.

Data flows **push**: the ESP32 sends the current state to the dashboard server running on your PC — the Vite dev server includes a small ingest API. The React app polls that same server, so everything is same-origin: no CORS, no IP address to configure in the app. The ESP32 learns your PC's address through a phone setup page (section 3.2).

```text
smart-streetlight-monitor/
├── arduino/
│   ├── uno_streetlight_control/     UNO firmware (LEDs + UART output)
│   └── esp32_streetlight_client/    ESP32 firmware (setup hotspot + push client)
└── smart-streetlight-dashboard/     React + TypeScript + Vite frontend (dev server hosts the ingest API)
```

---

## 1. Hardware required

| Item | Qty | Notes |
|---|---|---|
| Arduino UNO | 1 | Controls the lights, still USB-powered |
| ESP32 dev board | 1 | Wi-Fi push client (hotspot setup), still USB-powered |
| Solar panel 5V / 500mA / 2.5W | 3 | One per streetlight — you already have these |
| 18650 battery | 3 | One per streetlight, 2000mAh or more is plenty |
| 18650 battery holder | 3 | Much easier and safer than soldering to the battery |
| TP4056 USB charging module | 3 | Charges the 18650 from the solar panel — get the version marked **"with protection"** |
| COB LED strip, 3V 3W, warm white | 3 | The streetlight heads — 60mm × 8mm strips (CKFR-style); choose the 3000K warm white variant |
| IRLZ44N logic-level MOSFET | 3 | Acts as a switch so the UNO can control each battery-powered strip — **not** the IRFZ44N lookalike |
| 220 Ω resistors | 3 | MOSFET gate series resistors |
| 10 kΩ resistors | 3 | MOSFET gate pulldowns — stop the strip glowing dimly at UNO boot |
| 1 kΩ + 2 kΩ resistors | 1 + 1 | Voltage divider for UNO → ESP32 |
| Red LED | 1 | ESP32 internet-unavailable indicator |
| Green LED | 1 | ESP32 internet-available indicator |
| 220 Ω resistors | 2 | ESP32 indicator LED series resistors |
| 3.3 V passive piezo buzzer | 1 | ESP32 startup and internet-connected sounds |
| Breadboard + jumper wires | — | |

Everything except the panels, batteries, and boards is pennies each.

## 2. Wiring

All wiring — the streetlight unit circuit, the UNO ↔ ESP32 UART link, pin maps, an assembly checklist, and a changelog — lives in **[WIRING.md](WIRING.md)**.

WIRING.md is the single source of truth for wiring. Whenever the wiring changes, edit that file (and its changelog) — do not re-document wiring here or anywhere else.

## 3. Firmware setup

### 3.1 Arduino UNO

1. Open `arduino/uno_streetlight_control/uno_streetlight_control.ino` in the Arduino IDE.
2. Select **Tools → Board → Arduino UNO** and the correct port.
3. Click **Upload**. No libraries are required. The sketch itself is unchanged from the old version — D5/D6/D7 now feed MOSFET gates instead of LEDs directly.
4. Optional: open **Serial Monitor at 9600 baud** — you will see the raw frames (`L1:ON,L2:OFF,L3:ON`).

The sketch ships with `DEMO_MODE = true`, which cycles through light patterns every 5 s so you can watch the dashboard change. Set it to `false` for a fixed pattern.

### 3.2 ESP32

1. Install ESP32 support in the Arduino IDE: **Tools → Board → Boards Manager → search "esp32" → install "esp32 by Espressif Systems"**.
2. Open `arduino/esp32_streetlight_client/esp32_streetlight_client.ino`. There are **no credentials to edit** — Wi-Fi and the server address are configured from your phone.
3. Select **Tools → Board → ESP32 Dev Module**, choose the port, click **Upload**.
4. Find your PC's IPv4 address: run `ipconfig` and note the **IPv4 Address** of the adapter you use on the same network as the ESP32 will be (e.g. `192.168.1.50`). The dashboard must be running for pushes to arrive (section 4).
5. Power the ESP32. On first boot it opens an open hotspot named **Streetlight-Setup**. Connect your phone to it — the setup page should open automatically (captive portal), or browse to `http://192.168.4.1`.
6. On the setup page: choose your Wi-Fi network from the detected-network dropdown (or enter its SSID manually if the scan is empty), type its password, and enter your PC's address — a bare IP like `192.168.1.50` (port 5173 is assumed) or `IP:port` if you run the dev server elsewhere. Tap **Save and connect**.
7. The hotspot closes and the ESP32 starts pushing. Open **Serial Monitor at 115200 baud** to see the push URL and any errors. The on-board LED tells the story: **solid** = Wi-Fi up and pushes accepted, **slow blink** = Wi-Fi down or pushes failing, **fast blink** = setup hotspot active.
   The external **red LED** is on when the ESP32 cannot reach the internet; the **green LED** is on when it can. The ESP32 checks internet reachability every 10 seconds.
   The passive buzzer plays a short two-tone sound during boot and a confirmation tone when internet access is detected.
8. Wi-Fi must be **2.4 GHz** — the ESP32 will not join 5 GHz-only networks.
9. To change the Wi-Fi or server address later: hold the **BOOT** button while powering on, or type `reset` in the serial monitor — the setup hotspot comes back.
10. **Windows Firewall:** the first time Node.js opens a port, Windows may ask — allow Node.js on **private networks**, or the ESP32's pushes will never arrive.

## 4. React dashboard setup

Requires Node.js 18+.

```bash
cd smart-streetlight-dashboard
npm install
npm run dev
```

Open http://localhost:5173.

- **Nothing to configure** — there is no `.env` file anymore. The only address in the system is your PC's IP, which you give to the ESP32 once via its phone setup page (step 3.2.6).
- `host: true` in `vite.config.ts` makes the dev server listen on all interfaces — that is what lets the ESP32 reach it. It also means the dashboard is open on your LAN at `http://<your-pc-ip>:5173`.
- The ingest API (`POST /api/ingest`, `GET /api/streetlights`) is a dev-server plugin inside `vite.config.ts`. Note that it exists only under `npm run dev` / `npm run preview`-style dev usage — for this phase the dashboard is always run with the dev server.

## 5. Testing

### 5.1 Test the ingest API directly

The API lives on the dashboard's own dev server (it must be running):

```bash
curl -X POST http://localhost:5173/api/ingest \
  -H "Content-Type: application/json" \
  -d '{"led1":"ON","led2":"OFF","led3":"ON"}'
# {"status":"ok"}

curl http://localhost:5173/api/streetlights
# {"led1":"ON","led2":"OFF","led3":"ON"}
```

`/api/streetlights` returns **503** until the first push arrives, and again 5 s after pushes stop — that is the ESP32-offline detection.

### 5.2 Test the full chain UNO → ESP32 → dashboard

1. Start the dashboard (`npm run dev`) and note your PC's IP (`ipconfig`).
2. Power the UNO and ESP32 over USB, batteries in the holders, everything wired per [WIRING.md](WIRING.md). If the ESP32 isn't configured yet, do the hotspot setup once (step 3.2.5–6).
3. Open http://localhost:5173 — the header pill shows **Online** within a couple of seconds and the three cards mirror the strips (default startup: ON, OFF, ON).
4. With `DEMO_MODE = true` the pattern changes every 5 s; the dashboard follows within ~2 s (poll interval).
5. Unplug the ESP32 (or kill its Wi-Fi): pushes stop → within ~7 s (5 s staleness + poll) the dashboard flips to **Offline** with an error banner and keeps retrying — it returns to **Online** on its own when the ESP32 comes back. The page never crashes.
6. Unplug just the UNO: the ESP32 keeps pushing the last known state every 2 s (heartbeat), so the dashboard stays **Online** — check the ESP32 serial monitor for the "no data from UNO" warning.

### 5.3 Developing the frontend without hardware

Simulate the ESP32 with a push loop against the running dev server — this one-liner cycles the pattern every second so you can watch the dashboard change:

```bash
node -e "const s=['ON','OFF','ON'];let i=0;setInterval(()=>{fetch('http://localhost:5173/api/ingest',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({led1:s[i%3],led2:s[(i+1)%3],led3:s[(i+2)%3]})}).catch(()=>{});i++},1000)"
```

## 6. Troubleshooting

| Symptom | Check |
|---|---|
| Dashboard stuck on Offline | Dev server running? Read the ESP32 serial monitor — it prints exactly why pushes fail (unreachable, rejected, Wi-Fi down) |
| ESP32 says "Server unreachable" | Wrong PC address in the setup page (re-check `ipconfig`); PC and ESP32 on different networks; Windows Firewall blocking Node.js |
| Setup page doesn't open on the phone | Browse to `http://192.168.4.1` manually; turn off mobile data so the phone actually uses the hotspot |
| Want to change Wi-Fi or the server address | Hold **BOOT** while powering on, or type `reset` in the ESP32 serial monitor |
| ESP32 gets empty/garbled serial | Common GND missing; divider wiring swapped; baud must be 9600 on both sides |
| ESP32 serial monitor unreadable | Its debug port runs at **115200**, the UNO monitor at **9600** |
| UNO upload fails | Unplug the TX wire to the divider, upload, replug |
| Wi-Fi never connects | Network must be 2.4 GHz; captive-portal networks won't work; re-enter the password in the setup page |
| One streetlight never lights | Battery polarity at B+/B−; strip +/− pads swapped; MOSFET G-D-S legs wrong way round; 10 kΩ pulldown wired to the gate instead of between gate and source |
| MOSFET too hot to touch | Wrong part — IRFZ44N instead of IRLZ44N (check the printed part number); or drain/source swapped. A real IRLZ44N runs cold at 800 mA |
| Strip dimmer than when demo started | Battery draining — normal; top up via USB on the TP4056 |
| Lights off and dashboard offline | The UNO/ESP32 still need USB power — the batteries only run the LEDs |
| A light gets dim or dies mid-demo | Battery is low; plug a USB charger into that TP4056 to top up |

## 7. Phase 1 scope and the road to Phase 2

Included: LED control (UNO), UART state reporting, hotspot setup + JSON push over Wi-Fi, live dashboard with offline handling.

Deliberately excluded: LDR/current sensors, real bulb-failure detection, database, auth, cloud, notifications, MQTT.

The architecture is ready for failure detection without a rewrite:

- **UNO**: extend the frame with per-light health, e.g. `L1:ON,L1:OK,L2:ON,L2:FAIL,...` — the ESP32's token parser (`applyToken`) accepts new `X:Y` pairs without structural change.
- **ESP32**: add fields to the pushed JSON (`"health1":"OK"`) and teach the ingest endpoint in `vite.config.ts` to pass them through.
- **React**: add `health` to `StreetlightsResponse`, render a warning state in `StreetlightCard`. The polling service, offline handling, and layout all stay as-is.
- **Notifications** (Phase 2+): the ingest endpoint already sees every state change, so a small notifier can hook in there — or the ESP32 can push to a second URL — no frontend changes required.
