import type { LightState, StreetlightsResponse } from '../types/streetlight';

const REQUEST_TIMEOUT_MS = 4000;

async function extractError(response: Response): Promise<string> {
  const fallback = `HTTP ${response.status} ${response.statusText}`;
  try {
    const body = (await response.json()) as { error?: string };
    return body.error ?? fallback;
  } catch {
    return fallback;
  }
}

async function getJson<T>(path: string): Promise<T> {
  const controller = new AbortController();
  const timeoutId = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  try {
    const response = await fetch(path, {
      signal: controller.signal,
      cache: 'no-store',
    });
    if (!response.ok) {
      throw new Error(await extractError(response));
    }
    return (await response.json()) as T;
  } catch (error) {
    if (error instanceof DOMException && error.name === 'AbortError') {
      throw new Error(`request timed out after ${REQUEST_TIMEOUT_MS / 1000}s`);
    }
    throw error;
  } finally {
    window.clearTimeout(timeoutId);
  }
}

export function fetchStreetlights(): Promise<StreetlightsResponse> {
  return getJson<StreetlightsResponse>('/api/streetlights');
}

export function normalizeStates(response: StreetlightsResponse): LightState[] {
  const toState = (value: unknown): LightState => (value === 'ON' ? 'ON' : 'OFF');
  return [toState(response.led1), toState(response.led2), toState(response.led3)];
}
