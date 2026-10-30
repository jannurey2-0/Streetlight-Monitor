import type { ConnectionStatus as ConnectionStatusType } from '../types/streetlight';

const STATUS_LABELS: Record<ConnectionStatusType, string> = {
  online: 'Online',
  connecting: 'Connecting',
  offline: 'Offline',
};

interface ConnectionStatusProps {
  status: ConnectionStatusType;
}

export function ConnectionStatus({ status }: ConnectionStatusProps) {
  return (
    <div className={`connection-pill connection-pill--${status}`} role="status">
      <span className="connection-pill__dot" aria-hidden="true" />
      {STATUS_LABELS[status]}
    </div>
  );
}
