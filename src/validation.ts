import { randomUUID } from "node:crypto";
import { z } from "zod";
import { TelemetryInput, TelemetryMessage } from "./types";

const sensorTypes = [
  "temperature",
  "humidity",
  "presence",
  "vibration",
  "luminosity",
  "tank_level"
] as const;

const baseTelemetrySchema = z.object({
  deviceId: z.string().min(1).max(128),
  timestamp: z.iso.datetime(),
  sensorType: z.union([z.enum(sensorTypes), z.string().min(2).max(64)]),
  unit: z.string().min(1).max(24).optional(),
  metadata: z.record(z.string(), z.unknown()).optional()
});

const analogReadingSchema = baseTelemetrySchema.extend({
  readingType: z.literal("analog"),
  value: z.number().finite()
});

const discreteReadingSchema = baseTelemetrySchema.extend({
  readingType: z.literal("discrete"),
  value: z.union([
    z.boolean(),
    z.string().min(1).max(64),
    z.number().int()
  ])
});

export const telemetrySchema = z.discriminatedUnion("readingType", [
  analogReadingSchema,
  discreteReadingSchema
]);

const telemetryBatchSchema = z.union([
  telemetrySchema,
  z.array(telemetrySchema).min(1).max(1000)
]);

export const queueMessageSchema = z.object({
  messageId: z.string().uuid(),
  deviceId: z.string().min(1),
  sensorType: z.string().min(1),
  readingType: z.enum(["analog", "discrete"]),
  analogValue: z.number().nullable(),
  discreteValue: z.string().nullable(),
  unit: z.string().nullable(),
  observedAt: z.iso.datetime(),
  enqueuedAt: z.iso.datetime(),
  metadata: z.record(z.string(), z.unknown())
});

export const parseTelemetryBatch = (payload: unknown): TelemetryInput[] => {
  const parsed = telemetryBatchSchema.parse(payload);
  return Array.isArray(parsed) ? parsed : [parsed];
};

export const normalizeTelemetry = (input: TelemetryInput): TelemetryMessage => {
  const observedDate = new Date(input.timestamp);

  if (Number.isNaN(observedDate.getTime())) {
    throw new Error("timestamp invalido");
  }

  if (input.readingType === "analog") {
    if (typeof input.value !== "number") {
      throw new Error("Leitura analog exige value numerico");
    }

    return {
      messageId: randomUUID(),
      deviceId: input.deviceId,
      sensorType: input.sensorType,
      readingType: "analog",
      analogValue: input.value,
      discreteValue: null,
      unit: input.unit ?? null,
      observedAt: observedDate.toISOString(),
      enqueuedAt: new Date().toISOString(),
      metadata: input.metadata ?? {}
    };
  }

  if (typeof input.value === "number" && !Number.isInteger(input.value)) {
    throw new Error("Leitura discrete numerica deve ser inteira");
  }

  return {
    messageId: randomUUID(),
    deviceId: input.deviceId,
    sensorType: input.sensorType,
    readingType: "discrete",
    analogValue: null,
    discreteValue: String(input.value),
    unit: input.unit ?? null,
    observedAt: observedDate.toISOString(),
    enqueuedAt: new Date().toISOString(),
    metadata: input.metadata ?? {}
  };
};