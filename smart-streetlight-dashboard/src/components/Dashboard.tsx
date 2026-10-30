import { useEffect, useRef, useState } from 'react';
import { ConnectionStatus } from './ConnectionStatus';
import { StreetlightCard } from './StreetlightCard';
import { SystemOverview } from './SystemOverview';
import { fetchStreetlights, normalizeStates } from '../services/streetlightApi';
import type { ConnectionStatus as ConnectionStatusType, LightState } from '../types/streetlight';

const LIGHT_COUNT = 3;
const POLL_INTERVAL_MS = 2000;

export function Dashboard() {
  const [states, setStates] = useState<LightState[]>(() =>
    Array<LightState>(LIGHT_COUNT).fill('OFF'),
  );
  const [connection, setConnection] = useState<ConnectionStatusType>('connecting');
  const [error, setError] = useState<string | null>(null);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);
  const timerRef = useRef<number | undefined>(undefined);

  useEffect(() => {
    let cancelled = false;

    // Sequential self-scheduling poll: the next request starts only after
    // the previous one settles, so slow/unreachable devices can never pile up.
    const poll = async () => {
      try {
        const data = await fetchStreetlights();
        if (cancelled) return;
        setStates(normalizeStates(data));
        setConnection('online');
        setError(null);
        setLastUpdated(new Date());
      } catch (err) {
        if (cancelled) return;
        setConnection('offline');
        setError(err instanceof Error ? err.message : 'Unknown error');
      } finally {
        if (!cancelled) {
          timerRef.current = window.setTimeout(poll, POLL_INTERVAL_MS);
        }
      }
    };

    void poll();

    return () => {
      cancelled = true;
      window.clearTimeout(timerRef.current);
    };
  }, []);

  return (
    <div className="dashboard">
      <header className="dashboard__header">
        <div className="dashboard__brand">
          <span className="dashboard__logo" aria-hidden="true">
            <svg
              viewBox="0 0 24 24"
              width="22"
              height="22"
              fill="none"
              stroke="currentColor"
              strokeWidth="1.8"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <path d="M9 18h6" />
              <path d="M10 21h4" />
              <path d="M12 3a6 6 0 0 0-3.6 10.8c.7.55 1.1 1.3 1.25 2.2h4.7c.15-.9.55-1.65 1.25-2.2A6 6 0 0 0 12 3Z" />
            </svg>
          </span>
          <div>
            <h1>Smart Streetlight Monitoring System</h1>
            <p className="dashboard__subtitle">Live streetlight status &middot; Phase 1</p>
          </div>
        </div>
        <ConnectionStatus status={connection} />
      </header>

      <SystemOverview connection={connection} states={states} lastUpdated={lastUpdated} />

      <section className="dashboard__section" aria-label="Streetlights">
        <h2 className="dashboard__section-title">Streetlights</h2>
        <div className="cards-grid">
          {states.map((state, index) => (
            <StreetlightCard
              key={index + 1}
              id={index + 1}
              state={state}
              live={connection === 'online'}
            />
          ))}
        </div>
      </section>

      {connection === 'offline' && (
        <div className="error-banner" role="alert">
          <strong>Connection lost.</strong>{' '}
          {error ?? 'Cannot reach the dashboard server'}. Retrying automatically every{' '}
          {POLL_INTERVAL_MS / 1000} seconds.
        </div>
      )}

      <footer className="dashboard__footer">
        Data source: ESP32 pushes to this server&rsquo;s <code>/api/ingest</code> endpoint
      </footer>
    </div>
  );
}
