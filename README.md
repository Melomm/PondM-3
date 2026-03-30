# Backend de Telemetria Industrial

Este projeto implementa uma arquitetura de ingestao assicrona para dispositivos embarcados, com foco em escalabilidade, resiliencia e baixo tempo de resposta do endpoint HTTP.

## Objetivo
Receber leituras de sensores por `POST /telemetry`, enfileirar no RabbitMQ e processar em background para persistir em banco relacional (PostgreSQL), evitando gargalos de processamento sincrono na borda HTTP.

## Arquitetura
1. **API (`src/index.ts`)** recebe payloads de telemetria.
2. **Publisher (`src/rabbitmq.ts`)** publica mensagens persistentes em fila duravel no RabbitMQ.
3. **Worker (`src/worker.ts`)** consome a fila com `prefetch` configuravel.
4. **Persistencia (`src/db.ts`)** grava no PostgreSQL com controle de consistencia e idempotencia (`message_id` unico).
5. **DLQ** captura mensagens rejeitadas permanentemente.

## Decisoes tecnicas
- **TypeScript + Fastify**: bom desempenho para alta taxa de requisicoes.
- **RabbitMQ com fila duravel + mensagem persistente**: protege contra perda em reinicios.
- **Canal de confirmacao no publisher** (`waitForConfirms`): garante recebimento no broker antes da resposta 202.
- **Processamento assicrono por worker dedicado**: desacopla ingestao de persistencia.
- **PostgreSQL** com restricoes de integridade:
  - `reading_type` em `analog|discrete`.
  - `CHECK` para garantir apenas `analog_value` ou `discrete_value`, nunca ambos.
- **Idempotencia**: `message_id` unico com `ON CONFLICT DO NOTHING`.

## Modelo de dados
Tabela `telemetry_readings`:
- `id` (UUID PK)
- `message_id` (UUID unico)
- `device_id` (TEXT)
- `sensor_type` (TEXT)
- `reading_type` (TEXT: `analog`/`discrete`)
- `analog_value` (DOUBLE PRECISION, nullable)
- `discrete_value` (TEXT, nullable)
- `unit` (TEXT, nullable)
- `observed_at`, `enqueued_at`, `ingested_at` (TIMESTAMPTZ)
- `metadata` (JSONB)

## Estrutura do repositorio
- `src/` codigo da API, worker, validacoes e persistencia.
- `sql/init.sql` bootstrap do banco.
- `tests/k6/telemetry-load.js` script de carga.
- `reports/k6/` artefatos de resultados e analise.
- `docker-compose.yml` orquestracao de servicos.
- `Dockerfile` build da aplicacao TypeScript.

## Payload aceito
### Leitura analogica
```json
{
  "deviceId": "dev-01",
  "timestamp": "2026-03-19T13:00:00.000Z",
  "sensorType": "temperature",
  "readingType": "analog",
  "value": 42.5,
  "unit": "C",
  "metadata": {
    "line": "line-1"
  }
}
```

### Leitura discreta
```json
{
  "deviceId": "dev-02",
  "timestamp": "2026-03-19T13:00:00.000Z",
  "sensorType": "presence",
  "readingType": "discrete",
  "value": "present",
  "metadata": {
    "line": "line-2"
  }
}
```

## Como executar
### 1) Preparar ambiente
```bash
cp .env.example .env
```
No Windows PowerShell:
```powershell
Copy-Item .env.example .env
```

### 2) Subir toda a stack
```bash
docker compose up -d --build
```

Servicos:
- API: `http://localhost:3000`
- RabbitMQ Management: `http://localhost:15672` (guest/guest)
- PostgreSQL: `localhost:5432` (postgres/postgres)

### 3) Teste funcional rapido
```bash
curl -X POST http://localhost:3000/telemetry \
  -H "Content-Type: application/json" \
  -d '{"deviceId":"dev-01","timestamp":"2026-03-19T13:00:00.000Z","sensorType":"temperature","readingType":"analog","value":42.5,"unit":"C"}'
```
Resposta esperada: HTTP `202 Accepted`.

## Teste de carga com k6
O projeto inclui servico `k6` no compose (profile `loadtest`).

```bash
docker compose --profile loadtest run --rm k6
```

Relatorio JSON gerado em:
- `reports/k6/run-2026-03-19-summary.json`

Analise interpretativa:
- `reports/k6/run-2026-03-19-analysis.md`

## Resultados obtidos (execucao em 2026-03-19)
- Iteracoes: **88.448**
- Throughput medio: **368,29 req/s**
- Latencia media: **14,89 ms**
- Latencia p95: **29,11 ms**
- Erros HTTP: **0,00%**
- Filas ao final: `telemetry.ingest.queue=0`, `telemetry.dead.queue=0`
- Linhas persistidas: **88.449** (1 teste manual + 88.448 do k6)

## Encerramento
Para parar e remover os containers:
```bash
docker compose down
```
Para remover tambem os volumes:
```bash
docker compose down -v
```

## Possiveis Melhorias futuras
1. Observabilidade com Prometheus/Grafana e dashboard de lag da fila.
2. Particionamento da tabela por tempo para historico de alto volume.
3. Retry inteligente para falhas transientes de banco antes de DLQ.
4. Autenticacao/autorizacao dos dispositivos no endpoint de ingestao.

# Parte 2 - Firmware Pico W
A implementacao da segunda parte (toolchain nativo Pico SDK) esta em `pico-w-firmware/`.

Acesse o [README](pico-w-firmware/README.md) da segunda parte (Dentro de `pico-w-firmware`)

Consulte tambem:
- `pico-w-firmware/src/`
- `pico-w-firmware/simulation/wokwi/`
- `pico-w-firmware/evidence/`
