# Referencia Parte 1

Este firmware (Parte 2) integra com o backend da Parte 1 via endpoint HTTP `POST /telemetry`.

- Referencia local neste workspace: `../`
- Endpoint esperado: `http://<host>:3000/telemetry`
- Contrato de payload: `deviceId`, `timestamp`, `sensorType`, `readingType`, `value`, `unit?`, `metadata?`
