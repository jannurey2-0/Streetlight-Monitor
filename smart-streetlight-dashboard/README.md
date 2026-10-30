# Smart Streetlight Dashboard

React + TypeScript + Vite dashboard for the Smart Streetlight Monitoring System. The dev server doubles as the ingest API: the ESP32 POSTs streetlight state to `/api/ingest`, and the UI polls `/api/streetlights` on the same origin — no CORS, no IP configuration.

Hardware wiring lives in **[WIRING.md](../WIRING.md)**; firmware flashing and end-to-end test instructions are in the **root [README](../README.md)**.

## Quick start

```bash
npm install
npm run dev
```

Open http://localhost:5173. No configuration files — the only address in the system is this PC's IP, which you give to the ESP32 once through its phone setup page (root README §3.2).

## Endpoints (dev-server plugin in `vite.config.ts`)

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/ingest` | POST | ESP32 pushes `{"led1":"ON","led2":"OFF","led3":"ON"}`; validated, then stored |
| `/api/streetlights` | GET | Latest state for the UI — 200 while fresh, 503 before the first push or 5 s after pushes stop |

## Scripts

| Command | Purpose |
|---|---|
| `npm run dev` | Start the dev server (LAN-exposed) |
| `npm run build` | Type-check and build for production |
| `npm run preview` | Preview the production build |

## Structure

```text
src/
├── components/
│   ├── Dashboard.tsx          Polling loop + page layout
│   ├── StreetlightCard.tsx    Per-light card with bulb icon and ON/OFF state
│   ├── SystemOverview.tsx     Status / counts / last-update tiles
│   └── ConnectionStatus.tsx   Online / Connecting / Offline pill
├── services/
│   └── streetlightApi.ts      Fetch layer: timeouts, friendly error messages
├── types/
│   └── streetlight.ts         Shared types
├── App.tsx
├── main.tsx
└── index.css                  Dark IoT theme, responsive
```
