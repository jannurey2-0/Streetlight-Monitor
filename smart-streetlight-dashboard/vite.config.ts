import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import type { ServerResponse } from 'node:http';

// host: true exposes the dev server on the LAN so the ESP32 can reach it.
// The ESP32 pushes streetlight state to POST /api/ingest; the UI polls
// GET /api/streetlights on the same server, so there is no CORS and no
// configuration - the ESP32 only needs to know this PC's IP address.

const STALE_MS = 5000;
const MAX_BODY_BYTES = 1024;

type LightValue = 'ON' | 'OFF';

interface StreetlightsPayload {
  led1: LightValue;
  led2: LightValue;
  led3: LightValue;
}

let latest: StreetlightsPayload | null = null;
let lastUpdateMs = 0;

const isFresh = () => latest !== null && Date.now() - lastUpdateMs < STALE_MS;

function sendJson(res: ServerResponse, statusCode: number, body: unknown): void {
  res.statusCode = statusCode;
  res.setHeader('Content-Type', 'application/json');
  res.end(JSON.stringify(body));
}

function parseLight(value: unknown, name: string): LightValue {
  if (value !== 'ON' && value !== 'OFF') {
    throw new Error(`"${name}" must be "ON" or "OFF"`);
  }
  return value;
}

export default defineConfig({
  plugins: [
    react(),
    {
      name: 'streetlight-ingest-server',
      configureServer(server) {
        // ESP32 push: {"led1":"ON","led2":"OFF","led3":"ON"}
        server.middlewares.use('/api/ingest', (req, res) => {
          if (req.method !== 'POST') {
            sendJson(res, 405, { error: 'POST only' });
            return;
          }
          let body = '';
          let tooLarge = false;
          req.on('data', (chunk) => {
            if (tooLarge) return;
            body += chunk.toString();
            if (body.length > MAX_BODY_BYTES) tooLarge = true;
          });
          req.on('end', () => {
            if (tooLarge) {
              sendJson(res, 400, { error: 'payload too large' });
              return;
            }
            try {
              const data = JSON.parse(body) as Record<string, unknown>;
              latest = {
                led1: parseLight(data.led1, 'led1'),
                led2: parseLight(data.led2, 'led2'),
                led3: parseLight(data.led3, 'led3'),
              };
              lastUpdateMs = Date.now();
              sendJson(res, 200, { status: 'ok' });
            } catch (error) {
              sendJson(res, 400, {
                error: error instanceof Error ? error.message : 'invalid payload',
              });
            }
          });
          req.on('error', () => sendJson(res, 400, { error: 'invalid payload' }));
        });

        // UI poll: 200 while pushes are fresh, 503 when the ESP32 goes silent
        server.middlewares.use('/api/streetlights', (_req, res) => {
          if (latest === null) {
            sendJson(res, 503, {
              error: 'No data from the ESP32 yet - is it powered on and set up?',
            });
            return;
          }
          if (!isFresh()) {
            sendJson(res, 503, {
              error: 'No fresh data from the ESP32 - check its power, Wi-Fi, or the server address in its setup',
            });
            return;
          }
          sendJson(res, 200, latest);
        });
      },
    },
  ],
  server: {
    host: true,
    port: 5173,
  },
});
