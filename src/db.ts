import { Pool } from "pg";
import { config } from "./config";
import { logger } from "./logger";
import { TelemetryMessage } from "./types";

const pool = new Pool({
  connectionString: config.DATABASE_URL,
  max: 20,
  idleTimeoutMillis: 10000,
  connectionTimeoutMillis: 5000
});

pool.on("error", (error) => {
  logger.error("Erro inesperado no pool PostgreSQL", { error: error.message });
});

const schemaSql = `
CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE TABLE IF NOT EXISTS telemetry_readings (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  message_id UUID NOT NULL UNIQUE,
  device_id TEXT NOT NULL,
  sensor_type TEXT NOT NULL,
  reading_type TEXT NOT NULL CHECK (reading_type IN ('analog', 'discrete')),
  analog_value DOUBLE PRECISION,
  discrete_value TEXT,
  unit TEXT,
  observed_at TIMESTAMPTZ NOT NULL,
  enqueued_at TIMESTAMPTZ NOT NULL,
  ingested_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  metadata JSONB NOT NULL DEFAULT '{}'::JSONB,
  CHECK (
    (reading_type = 'analog' AND analog_value IS NOT NULL AND discrete_value IS NULL) OR
    (reading_type = 'discrete' AND analog_value IS NULL AND discrete_value IS NOT NULL)
  )
);

CREATE INDEX IF NOT EXISTS idx_telemetry_device_time ON telemetry_readings (device_id, observed_at DESC);
CREATE INDEX IF NOT EXISTS idx_telemetry_sensor_time ON telemetry_readings (sensor_type, observed_at DESC);
CREATE INDEX IF NOT EXISTS idx_telemetry_ingested_time ON telemetry_readings (ingested_at DESC);
`;

export const ensureSchema = async (): Promise<void> => {
  await pool.query(schemaSql);
};

export const insertTelemetry = async (message: TelemetryMessage): Promise<void> => {
  const sql = `
  INSERT INTO telemetry_readings (
    message_id,
    device_id,
    sensor_type,
    reading_type,
    analog_value,
    discrete_value,
    unit,
    observed_at,
    enqueued_at,
    metadata
  ) VALUES (
    $1,$2,$3,$4,$5,$6,$7,$8,$9,$10
  )
  ON CONFLICT (message_id) DO NOTHING;
  `;

  const values = [
    message.messageId,
    message.deviceId,
    message.sensorType,
    message.readingType,
    message.analogValue,
    message.discreteValue,
    message.unit,
    message.observedAt,
    message.enqueuedAt,
    JSON.stringify(message.metadata)
  ];

  await pool.query(sql, values);
};

export const pingDatabase = async (): Promise<void> => {
  await pool.query("SELECT 1");
};

export const closeDatabase = async (): Promise<void> => {
  await pool.end();
};