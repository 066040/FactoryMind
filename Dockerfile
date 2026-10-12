FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY consumer.py .
COPY factory_mind_db ./factory_mind_db

CMD ["python3", "consumer.py"]
