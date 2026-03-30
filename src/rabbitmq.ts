import amqp, { Channel, ChannelModel, ConfirmChannel, ConsumeMessage } from "amqplib";
import { config } from "./config";
import { logger } from "./logger";
import { TelemetryMessage } from "./types";

const delay = (ms: number): Promise<void> => new Promise((resolve) => setTimeout(resolve, ms));

const connectWithRetry = async (): Promise<ChannelModel> => {
  let attempt = 0;

  while (attempt < config.MAX_RETRY_ATTEMPTS) {
    try {
      const connection = await amqp.connect(config.RABBITMQ_URL);
      logger.info("Conexao com RabbitMQ estabelecida", { attempt: attempt + 1 });
      return connection;
    } catch (error) {
      attempt += 1;
      logger.warn("Falha ao conectar no RabbitMQ", {
        attempt,
        maxAttempts: config.MAX_RETRY_ATTEMPTS,
        error: error instanceof Error ? error.message : "erro desconhecido"
      });
      await delay(config.RETRY_DELAY_MS);
    }
  }

  throw new Error("Nao foi possivel conectar ao RabbitMQ dentro do limite de tentativas");
};

const assertTopology = async (channel: Channel): Promise<void> => {
  await channel.assertExchange(config.RABBITMQ_EXCHANGE, "direct", { durable: true });
  await channel.assertExchange(config.RABBITMQ_DLX, "direct", { durable: true });

  await channel.assertQueue(config.RABBITMQ_QUEUE, {
    durable: true,
    deadLetterExchange: config.RABBITMQ_DLX,
    deadLetterRoutingKey: `${config.RABBITMQ_ROUTING_KEY}.dead`
  });

  await channel.assertQueue(config.RABBITMQ_DLQ, { durable: true });

  await channel.bindQueue(config.RABBITMQ_QUEUE, config.RABBITMQ_EXCHANGE, config.RABBITMQ_ROUTING_KEY);
  await channel.bindQueue(config.RABBITMQ_DLQ, config.RABBITMQ_DLX, `${config.RABBITMQ_ROUTING_KEY}.dead`);
};

export class TelemetryPublisher {
  private connection!: ChannelModel;
  private channel!: ConfirmChannel;

  static async create(): Promise<TelemetryPublisher> {
    const publisher = new TelemetryPublisher();
    await publisher.init();
    return publisher;
  }

  private async init(): Promise<void> {
    this.connection = await connectWithRetry();
    this.channel = await this.connection.createConfirmChannel();
    await assertTopology(this.channel);

    this.connection.on("error", (error) => {
      logger.error("Erro na conexao do RabbitMQ (publisher)", { error: error.message });
    });

    this.connection.on("close", () => {
      logger.warn("Conexao do RabbitMQ (publisher) encerrada");
    });
  }

  async publish(messages: TelemetryMessage[]): Promise<void> {
    for (const message of messages) {
      const ok = this.channel.publish(
        config.RABBITMQ_EXCHANGE,
        config.RABBITMQ_ROUTING_KEY,
        Buffer.from(JSON.stringify(message)),
        {
          contentType: "application/json",
          deliveryMode: 2,
          timestamp: Date.now(),
          messageId: message.messageId,
          type: "telemetry.reading"
        }
      );

      if (!ok) {
        await new Promise<void>((resolve) => this.channel.once("drain", () => resolve()));
      }
    }

    await this.channel.waitForConfirms();
  }

  async close(): Promise<void> {
    await this.channel.close();
    await this.connection.close();
  }
}

export const createConsumerChannel = async (): Promise<{ connection: ChannelModel; channel: Channel }> => {
  const connection = await connectWithRetry();
  const channel = await connection.createChannel();
  await assertTopology(channel);
  await channel.prefetch(config.RABBITMQ_PREFETCH);

  connection.on("error", (error) => {
    logger.error("Erro na conexao do RabbitMQ (consumer)", { error: error.message });
  });

  connection.on("close", () => {
    logger.warn("Conexao do RabbitMQ (consumer) encerrada");
  });

  return { connection, channel };
};

export const consumeTelemetry = async (
  channel: Channel,
  handler: (message: TelemetryMessage) => Promise<void>
): Promise<void> => {
  await channel.consume(
    config.RABBITMQ_QUEUE,
    async (rawMessage: ConsumeMessage | null) => {
      if (!rawMessage) {
        return;
      }

      try {
        const payload = JSON.parse(rawMessage.content.toString("utf-8")) as TelemetryMessage;
        await handler(payload);
        channel.ack(rawMessage);
      } catch (error) {
        logger.error("Falha ao processar mensagem da fila", {
          error: error instanceof Error ? error.message : "erro desconhecido"
        });
        channel.nack(rawMessage, false, false);
      }
    },
    {
      noAck: false
    }
  );
};