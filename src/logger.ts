import { config } from "./config";

type LogMethod = "debug" | "info" | "warn" | "error";

const levelPriority: Record<string, number> = {
  trace: 10,
  debug: 20,
  info: 30,
  warn: 40,
  error: 50,
  fatal: 60
};

const currentLevel = levelPriority[config.LOG_LEVEL] ?? levelPriority.info;

const shouldLog = (method: LogMethod): boolean => {
  const methodLevel = levelPriority[method] ?? levelPriority.info;
  return methodLevel >= currentLevel;
};

const write = (method: LogMethod, message: string, context?: unknown): void => {
  if (!shouldLog(method)) {
    return;
  }

  const payload = {
    time: new Date().toISOString(),
    level: method,
    message,
    ...(context ? { context } : {})
  };

  const line = JSON.stringify(payload);

  if (method === "error") {
    console.error(line);
    return;
  }

  if (method === "warn") {
    console.warn(line);
    return;
  }

  console.log(line);
};

export const logger = {
  debug: (message: string, context?: unknown) => write("debug", message, context),
  info: (message: string, context?: unknown) => write("info", message, context),
  warn: (message: string, context?: unknown) => write("warn", message, context),
  error: (message: string, context?: unknown) => write("error", message, context)
};