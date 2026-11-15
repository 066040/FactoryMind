
# FactoryMind

An Industry 4.0 predictive maintenance pipeline that captures machine sensor telemetry, queues critical alerts, and performs automated root-cause analysis using local RAG and LLM inference.

## Architecture

[ C++ Edge Sensor ] ──(AMQP)──> [ RabbitMQ Broker ] ──> [ Python Consumer ]
                                                             |
                                    +------------------------+------------------------+
                                    v                                                 v
                            [ ChromaDB (RAG) ]                             [ Local Ollama / LLM ]
                         (Technician Notes Context)                         (Root Cause Analysis)

1. Edge Sensor (C++, `sensor/main.cpp`): Simulates temperature and vibration for three machines, checks them against limits (90 °C, 7.1 mm/s RMS) with hysteresis, and publishes a JSON alarm over AMQP only when a limit is crossed.
2. Message Broker (RabbitMQ): Buffers alarms between the edge and the AI worker, so neither side waits for the other.
3. RAG Vector Search (ChromaDB): Matches incoming alarm telemetry with historical technician repair manuals.
4. Local Brain (Ollama): Runs fully on-premise to diagnose root causes and recommend immediate mitigation steps.

## Tech Stack

- C++17 + rabbitmq-c (AMQP publisher)
- Python 3.11 (Consumer & RAG)
- RabbitMQ (Docker)
- ChromaDB (Local Vector DB)
- Ollama (Local LLM Inference)
- Docker Compose

## Quick Start

1. Ensure Ollama is running on host with model:
   ollama pull qwen2.5:3b

2. Start everything — broker, AI consumer and the C++ edge sensor:
   docker compose up --build

3. Watch the alarms and the AI's answers:
   docker compose logs -f sensor consumer

   RabbitMQ management UI: http://localhost:15672

Run the sensor outside Docker (Linux / WSL):
   sudo apt install g++ librabbitmq-dev
   g++ -std=c++17 -O2 sensor/main.cpp -lrabbitmq -o sensor/sensor
   ./sensor/sensor          # RABBIT_HOST, RABBIT_PORT, RABBIT_USER, RABBIT_PASS, SAMPLE_MS

## Example Output

[SENSOR] Alarm: CNC-01 - CRITICAL (Temperature: 92.0°C)
[RETRIEVED NOTES]: CNC-01: When temperature reaches 90°C, replace thermal relief valve.
[ROOT CAUSE & ACTION PLAN]: Operating temperature exceeded 90°C threshold. Verify coolant pump performance and replace thermal relief valve immediately.
