import type { LightState } from '../types/streetlight';

interface StreetlightCardProps {
  id: number;
  state: LightState;
  live: boolean;
}

export function StreetlightCard({ id, state, live }: StreetlightCardProps) {
  const isOn = state === 'ON';
  const label = `Streetlight ${String(id).padStart(2, '0')}`;

  return (
    <article className={`light-card ${isOn ? 'light-card--on' : 'light-card--off'}`} aria-label={label}>
      <span className={`live-chip ${live ? 'live-chip--live' : ''}`}>
        {live ? 'Live' : 'No link'}
      </span>

      <BulbIcon on={isOn} />

      <h3 className="light-card__title">{label}</h3>

      <p className={`light-card__state ${isOn ? 'is-on' : 'is-off'}`}>
        <span className="state-dot" aria-hidden="true" />
        {state}
      </p>

      <p className="light-card__note">{live ? 'Operational' : 'Awaiting data'}</p>
    </article>
  );
}

function BulbIcon({ on }: { on: boolean }) {
  return (
    <svg className={`bulb ${on ? 'bulb--on' : 'bulb--off'}`} viewBox="0 0 64 64" aria-hidden="true">
      <path
        className="bulb__glass"
        d="M32 6a17 17 0 0 0-10 30.7c2 1.5 3.2 3.6 3.6 5.8h12.8c.4-2.2 1.6-4.3 3.6-5.8A17 17 0 0 0 32 6Z"
      />
      <path className="bulb__filament" d="M26 34l3.2-6.5 2.3 4.5 2.4-4.5L37 34" fill="none" />
      <rect className="bulb__base" x="25" y="45" width="14" height="4.5" rx="2" />
      <rect className="bulb__base" x="26.5" y="51" width="11" height="4" rx="1.8" />
      <rect className="bulb__base" x="28.5" y="56.5" width="7" height="3" rx="1.4" />
    </svg>
  );
}
