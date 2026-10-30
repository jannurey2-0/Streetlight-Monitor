export type LightState = 'ON' | 'OFF';

export interface StreetlightsResponse {
  led1: LightState;
  led2: LightState;
  led3: LightState;
}

export type ConnectionStatus = 'connecting' | 'online' | 'offline';
