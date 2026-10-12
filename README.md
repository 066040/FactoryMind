cat << 'EOF' > ~/FactoryMind-Docker/README.md
# FactoryMind

An Industry 4.0 predictive maintenance pipeline that captures machine sensor telemetry, queues critical alerts, and performs automated root-cause analysis using local RAG and LLM inference.

## Architecture

[ C++ Edge Sensor ] ──(AMQP)──> [ RabbitMQ Broker ] ──> [ Python Consumer ]
                                                             |
                                    +------------------------+------------------------+
                                    v                                                 v
                            [ ChromaDB (RAG) ]                             [ Local Ollama / LLM ]
                         (Technician Notes Context)                         (Root Cause Analysis)

1. Edge Sensor (C++): Collects telemetry (temperature, vibration, status) and sends critical JSON alarms.
2. Message Broker (RabbitMQ): Queues messages reliably without data loss.
3. RAG Vector Search (ChromaDB): Matches incoming alarm telemetry with historical technician repair manuals.
4. Local Brain (Ollama): Runs fully on-premise to diagnose root causes and recommend immediate mitigation steps.

## Tech Stack

- C++ (AMQP Publisher)
- Python 3.11 (Consumer & RAG)
- RabbitMQ (Docker)
- ChromaDB (Local Vector DB)
- Ollama (Local LLM Inference)
- Docker Compose

## Quick Start

1. Ensure Ollama is running on host with model:
   ollama pull qwen2.5:3b

2. Start broker and AI consumer:
   docker-compose up --build

3. In another terminal, trigger the edge sensor:
   ~/producer

## Example Output

[SENSOR] Alarm: CNC-01 - CRITICAL (Temperature: 92.0°C)
[RETRIEVED NOTES]: CNC-01: When temperature reaches 90°C, replace thermal relief valve.
[ROOT CAUSE & ACTION PLAN]: Operating temperature exceeded 90°C threshold. Verify coolant pump performance and replace thermal relief valve immediately.
