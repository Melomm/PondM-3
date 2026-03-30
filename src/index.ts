import { buildApi } from "./api";
import { config } from "./config";
import { logger } from "./logger";

const start = async (): Promise<void> => {
  const app = await buildApi();

  const closeGracefully = async (signal: string) => {
    logger.warn(`Sinal ${signal} recebido. Encerrando API...`);
    await app.close();
    process.exit(0);
  };

  process.on("SIGINT", () => {
    void closeGracefully("SIGINT");
  });

  process.on("SIGTERM", () => {
    void closeGracefully("SIGTERM");
  });

  try {
    await app.listen({
      host: "0.0.0.0",
      port: config.PORT
    });

    logger.info("API de telemetria iniciada", { port: config.PORT });
  } catch (error) {
    logger.error("Falha ao iniciar API", {
      error: error instanceof Error ? error.message : "erro desconhecido"
    });
    process.exit(1);
  }
};

void start();