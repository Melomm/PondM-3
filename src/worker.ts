import { closeDatabase, ensureSchema, insertTelemetry, pingDatabase } from "./db";
import { logger } from "./logger";
import { consumeTelemetry, createConsumerChannel } from "./rabbitmq";
import { queueMessageSchema } from "./validation";

const startWorker = async (): Promise<void> => {
  await pingDatabase();
  await ensureSchema();

  const { connection, channel } = await createConsumerChannel();

  await consumeTelemetry(channel, async (message) => {
    const validated = queueMessageSchema.parse(message);
    await insertTelemetry(validated);
  });

  logger.info("Worker iniciado e consumindo fila de telemetria");

  const closeGracefully = async (signal: string): Promise<void> => {
    logger.warn(`Sinal ${signal} recebido. Encerrando worker...`);
    await channel.close();
    await connection.close();
    await closeDatabase();
    process.exit(0);
  };

  process.on("SIGINT", () => {
    void closeGracefully("SIGINT");
  });

  process.on("SIGTERM", () => {
    void closeGracefully("SIGTERM");
  });
};

startWorker().catch(async (error) => {
  logger.error("Falha ao iniciar worker", {
    error: error instanceof Error ? error.message : "erro desconhecido"
  });
  await closeDatabase();
  process.exit(1);
});