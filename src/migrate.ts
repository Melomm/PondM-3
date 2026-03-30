import { closeDatabase, ensureSchema } from "./db";
import { logger } from "./logger";

const run = async (): Promise<void> => {
  try {
    await ensureSchema();
    logger.info("Migracao concluida com sucesso");
  } catch (error) {
    logger.error("Falha na migracao", {
      error: error instanceof Error ? error.message : "erro desconhecido"
    });
    process.exitCode = 1;
  } finally {
    await closeDatabase();
  }
};

void run();