export type ReadingType = "analog" | "discrete";

export type TelemetryInput = {
  deviceId: string;
  timestamp: string;
  sensorType: string;
  readingType: ReadingType;
  value: number | string | boolean;
  unit?: string;
  metadata?: Record<string, unknown>;
};

export type TelemetryMessage = {
  messageId: string;
  deviceId: string;
  sensorType: string;
  readingType: ReadingType;
  analogValue: number | null;
  discreteValue: string | null;
  unit: string | null;
  observedAt: string;
  enqueuedAt: string;
  metadata: Record<string, unknown>;
};