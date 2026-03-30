import Fastify from "fastify";
import { config } from "./config";
import { logger } from "./logger";
import { TelemetryPublisher } from "./rabbitmq";
import { normalizeTelemetry, parseTelemetryBatch } from "./validation";

export const buildApi = async () => {
  const app = Fastify({
    logger: false,
    bodyLimit: 1024 * 1024
  });

  const publisher = await TelemetryPublisher.create();

  app.post("/telemetry", async (request, reply) => {
    try {
      const batch = parseTelemetryBatch(request.body);
      const normalized = batch.map((item) => normalizeTelemetry(item));

      await publisher.publish(normalized);

      return reply.code(202).send({
        status: "accepted",
        acceptedCount: normalized.length,
        queue: config.RABBITMQ_QUEUE,
        enqueuedAt: new Date().toISOString()
      });
    } catch (error) {
      logger.warn("Payload invalido recebido", {
        error: error instanceof Error ? error.message : "erro desconhecido"
      });

      return reply.code(400).send({
        error: "invalid_payload",
        message: error instanceof Error ? error.message : "Erro ao validar payload"
      });
    }
  });

  app.get("/health", async () => {
    return {
      status: "ok",
      service: "telemetry-api",
      uptimeSeconds: Math.round(process.uptime()),
      timestamp: new Date().toISOString()
    };
  });

  app.addHook("onClose", async () => {
    await publisher.close();
  });

  return app;
};