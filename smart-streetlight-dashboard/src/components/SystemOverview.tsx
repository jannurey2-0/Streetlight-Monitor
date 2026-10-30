import type { ConnectionStatus, LightState } from '../types/streetlight';

const STATUS_TEXT: Record<ConnectionStatus, string> = {
  online: 'Online',
  connecting: 'Connecting',
  offline: 'Offline',
};

interface SystemOverviewProps {
  connection: ConnectionStatus;
  states: LightState[];
  lastUpdated: Date | null;
}

interface StatTile {
  label: string;
  value: string;
  modifier?: string;
  dot?: ConnectionStatus;
}

export function SystemOverview({ connection, states, lastUpdated }: SystemOverviewProps) {
  const lightsOn = states.filter((state) => state === 'ON').length;
  const lightsOff = states.length - lightsOn;

  const tiles: StatTile[] = [
    {
      label: 'System status',
      value: STATUS_TEXT[connection],
      dot: connection,
      modifier: `status--${connection}`,
    },
    { label: 'Streetlights', value: String(states.length) },
    { label: 'Lights ON', value: String(lightsOn), modifier: 'on' },
    { label: 'Lights OFF', value: String(lightsOff), modifier: 'off' },
    {
      label: 'Last update',
      value: lastUpdated ? lastUpdated.toLocaleTimeString() : '—',
      modifier: 'time',
    },
  ];

  return (
    <section className="overview-grid" aria-label="System overview">
      {tiles.map((tile) => (
        <article key={tile.label} className="stat-tile">
          <span className="stat-tile__label">{tile.label}</span>
          <span
            className={
              tile.modifier ? `stat-tile__value stat-tile__value--${tile.modifier}` : 'stat-tile__value'
            }
          >
            {tile.dot ? (
              <span className={`stat-tile__dot stat-tile__dot--${tile.dot}`} aria-hidden="true" />
            ) : null}
            {tile.value}
          </span>
        </article>
      ))}
    </section>
  );
}
