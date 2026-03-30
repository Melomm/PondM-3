import "dotenv/config";
import { z } from "zod";

const envSchema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().int().positive().default(3000),
  LOG_LEVEL: z.enum(["trace", "debug", "info", "warn", "error", "fatal"]).default("info"),
  RABBITMQ_URL: z.string().url().default("amqp://guest:guest@localhost:5672"),
  RABBITMQ_EXCHANGE: z.string().min(1).default("telemetry.exchange"),
  RABBITMQ_ROUTING_KEY: z.string().min(1).default("telemetry.ingest"),
  RABBITMQ_QUEUE: z.string().min(1).default("telemetry.ingest.queue"),
  RABBITMQ_DLX: z.string().min(1).default("telemetry.dlx"),
  RABBITMQ_DLQ: z.string().min(1).default("telemetry.dead.queue"),
  RABBITMQ_PREFETCH: z.coerce.number().int().positive().default(100),
  DATABASE_URL: z.string().url().default("postgres://postgres:postgres@localhost:5432/telemetry"),
  MAX_RETRY_ATTEMPTS: z.coerce.number().int().positive().default(20),
  RETRY_DELAY_MS: z.coerce.number().int().positive().default(2000)
});

const parsed = envSchema.safeParse(process.env);

if (!parsed.success) {
  throw new Error(`Falha ao validar variaveis de ambiente: ${parsed.error.message}`);
}

export const config = parsed.data;
