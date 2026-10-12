# 🏭 FactoryMind: Edge-to-AI Predictive Maintenance Architecture

An end-to-end **Industry 4.0** predictive maintenance pipeline designed for smart factory telemetry. 

The system captures high-frequency CNC telemetry via an edge-level C++ producer, queues mission-critical alerts through RabbitMQ, retrieves contextual technician troubleshooting procedures using vector search (ChromaDB), and performs automated root-cause failure analysis (RCFA) locally using an on-premise Large Language Model (Ollama).

---

## 🏗️ System Architecture

```text
[ CNC Machine / Edge ] 
        │
   (C++ AMQP Producer)
        │  Sends Telemetry (JSON: Temp, Vibration, Status)
        ▼
[ RabbitMQ Message Broker (Docker) ]
        │
        │  Persistent Queue Stream
        ▼
[ AI Brain Consumer (Docker / Python) ]
   ├── Step 1: Ingests alert payload
   ├── Step 2: Vector Search in ChromaDB (Historical Technician Notes / RAG)
   └── Step 3: Local LLM Inference via Ollama API
        │
        ▼
[ Root Cause & Corrective Action Report ]
