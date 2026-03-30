import http from "k6/http";
import { check } from "k6";

const BASE_URL = __ENV.BASE_URL || "http://localhost:3000";

const analogSensors = [
  { type: "temperature", unit: "C", min: 18, max: 95 },
  { type: "humidity", unit: "%", min: 20, max: 95 },
  { type: "vibration", unit: "mm/s", min: 0.1, max: 20 },
  { type: "luminosity", unit: "lux", min: 10, max: 1000 },
  { type: "tank_level", unit: "%", min: 0, max: 100 }
];

const discreteSensors = [
  { type: "presence", values: ["present", "absent"] },
  { type: "pump_status", values: ["on", "off"] },
  { type: "door_state", values: ["open", "closed"] }
];

function randomInt(maxExclusive) {
  return Math.floor(Math.random() * maxExclusive);
}

function randomFloat(min, max) {
  return Number((Math.random() * (max - min) + min).toFixed(3));
}

function buildPayload() {
  const nowIso = new Date().toISOString();
  const isAnalog = Math.random() > 0.35;

  if (isAnalog) {
    const sensor = analogSensors[randomInt(analogSensors.length)];

    return {
      deviceId: `dev-${randomInt(5000)}`,
      timestamp: nowIso,
      sensorType: sensor.type,
      readingType: "analog",
      value: randomFloat(sensor.min, sensor.max),
      unit: sensor.unit,
      metadata: {
        line: `line-${1 + randomInt(12)}`,
        gateway: `gw-${1 + randomInt(30)}`
      }
    };
  }

  const sensor = discreteSensors[randomInt(discreteSensors.length)];

  return {
    deviceId: `dev-${randomInt(5000)}`,
    timestamp: nowIso,
    sensorType: sensor.type,
    readingType: "discrete",
    value: sensor.values[randomInt(sensor.values.length)],
    metadata: {
      line: `line-${1 + randomInt(12)}`,
      gateway: `gw-${1 + randomInt(30)}`
    }
  };
}

export const options = {
  scenarios: {
    telemetry_ingest: {
      executor: "ramping-arrival-rate",
      startRate: 100,
      timeUnit: "1s",
      preAllocatedVUs: 100,
      maxVUs: 1200,
      stages: [
        { target: 100, duration: "30s" },
        { target: 350, duration: "1m" },
        { target: 700, duration: "90s" },
        { target: 300, duration: "45s" },
        { target: 0, duration: "15s" }
      ]
    }
  },
  thresholds: {
    http_req_failed: ["rate<0.01"],
    http_req_duration: ["p(95)<250", "p(99)<400"],
    checks: ["rate>0.99"]
  }
};

export default function () {
  const payload = buildPayload();

  const response = http.post(`${BASE_URL}/telemetry`, JSON.stringify(payload), {
    headers: {
      "Content-Type": "application/json"
    },
    timeout: "10s"
  });

  check(response, {
    "status 202": (r) => r.status === 202
  });
}

export function handleSummary(data) {
  const summaryPath = __ENV.K6_SUMMARY_PATH || "reports/k6/latest-summary.json";

  return {
    [summaryPath]: JSON.stringify(data, null, 2),
    stdout: [
      "\\nResumo k6:",
      `- Iteracoes: ${data.metrics.iterations.values.count}`,
      `- Erro HTTP: ${(data.metrics.http_req_failed.values.rate * 100).toFixed(3)}%`,
      `- Latencia p95: ${data.metrics.http_req_duration.values["p(95)"]?.toFixed(2)} ms`,
      `- Throughput: ${data.metrics.http_reqs.values.rate.toFixed(2)} req/s\\n`
    ].join("\\n")
  };
}