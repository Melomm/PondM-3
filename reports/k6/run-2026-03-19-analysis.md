# Analise do Teste de Carga - 2026-03-19

## Cenario executado
- Ferramenta: k6 `tests/k6/telemetry-load.js`
- Duracao: 4 minutos de carga util (mais ramp-up/ramp-down)
- Perfil: `ramping-arrival-rate` de 100 ate 700 req/s
- Endpoint: `POST /telemetry`
- Infraestrutura: `api` + `worker` + `rabbitmq` + `postgres` via Docker Compose

## Principais metricas
- Iteracoes totais: **88.448**
- Throughput medio: **368,29 req/s**
- Latencia media: **14,89 ms**
- Latencia p90: **25,76 ms**
- Latencia p95: **29,11 ms**
- Latencia maxima: **811,62 ms**
- Taxa de erro HTTP (`http_req_failed.rate`): **0,00%**
- Checks `status 202`: **100% (88.448/88.448)**
- Iteracoes descartadas (`dropped_iterations`): **51**
- VUs maximos observados: **151**

## Evidencias adicionais de fila e persistencia
- `telemetry.ingest.queue`: `messages_ready=0`, `messages_unacknowledged=0`
- `telemetry.dead.queue`: `messages_ready=0`, `messages_unacknowledged=0`
- Linhas persistidas em banco ao final: **88.449**
  - 1 linha de teste funcional manual anterior + 88.448 do teste de carga

## Interpretacao
- O endpoint manteve latencia baixa mesmo com pico de carga, indicando que o desacoplamento via RabbitMQ protege o tempo de resposta da API.
- A taxa de erro em 0% e o escoamento das filas para 0 ao final do teste indicam estabilidade do consumidor e ausencia de perda aparente de mensagens nesse cenario.
- O `dropped_iterations` diferente de zero indica limite de geracao de carga no cliente k6 em alguns instantes do pico. Isso nao compromete o backend, mas mostra que o gerador chegou proximo do limite da configuracao de VUs no teste.
- O valor maximo de latencia (811 ms) aparece como outlier, enquanto p95 permaneceu em ~29 ms. O comportamento geral e consistente para ingestao assicrona.

## Possiveis gargalos e melhorias
1. Implementar metricas de observabilidade (Prometheus/Grafana) para fila, lag de consumo e tempo de persistencia por mensagem.
2. Adicionar particionamento temporal da tabela de telemetria quando o volume historico crescer.
3. Introduzir politica de retentativa com backoff para falhas transientes de banco, antes de enviar mensagens para DLQ.
4. Ajustar `RABBITMQ_PREFETCH` e pool do PostgreSQL em testes de carga mais agressivos (>1.000 req/s).