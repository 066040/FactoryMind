import pika
import ollama
import chromadb
import json
import time

# 1. Ollama artık doğrudan localhost'ta (network_mode: host sayesinde)
client = ollama.Client(host='http://host.docker.internal:11434')

# 2. ChromaDB'ye bağlan
db_client = chromadb.PersistentClient(path="./factory_mind_db")
collection = db_client.get_collection("technician_notes")

def get_rabbit_connection():
    while True:
        try:
            print("🔄 RabbitMQ'ya bağlanmaya çalışılıyor...")
            # 3. RabbitMQ de localhost'ta (port 5672 host'a açıldı)
            connection = pika.BlockingConnection(
                pika.ConnectionParameters(
                    host='rabbitmq', 
                    port=5672,
                    credentials=pika.PlainCredentials('admin', 'mercedes2026')
                )
            )
            print("✅ RabbitMQ'ya başarıyla bağlanıldı!")
            return connection
        except Exception as e:
            print(f"⚠️ Bağlantı hatası: {e}. 2 saniye sonra tekrar denenecek...")
            time.sleep(2)

def callback(ch, method, properties, body):
    data = json.loads(body.decode())
    
    print(f"\n🔔 [SENSOR] Alarm: {data['sensor_id']} - {data['status']}")
    print(f"   Sıcaklık: {data['temperature']}°C")
    
    alert_msg = f"{data['sensor_id']} makinesinde sıcaklık {data['temperature']}°C'ye ulaştı."
    
    results = collection.query(query_texts=[alert_msg], n_results=2)
    context = "\n".join(results['documents'][0])
    
    print(f"\n📚 [BULUNAN NOTLAR]:\n{context}\n")
    print("🤖 [AI] Notları okuyup analiz ediyor...")
    
    prompt = f"""Sen bir Mercedes fabrika mühendisisin. 
    Aşağıdaki teknisyen notlarını kullanarak bu alarmın kök nedenini ve acil müdahale planını bul.
    
    TEKNİSYEN NOTLARI:
    {context}
    
    ALARM: {alert_msg}
    
    Kök nedeni ve yapılması gerekeni 2 cümle ile yaz."""
    
    response = client.chat(model='qwen2.5:3b', messages=[{'role': 'user', 'content': prompt}])
    
    print(f"\n🧠 [KÖK NEDEN VE MÜDAHALE]:\n{response['message']['content']}\n")
    print("="*60)

if __name__ == "__main__":
    connection = get_rabbit_connection()
    channel = connection.channel()
    channel.queue_declare(queue='factory.sensors', durable=False)
    channel.basic_consume(queue='factory.sensors', on_message_callback=callback, auto_ack=True)
    
    print("--- Sensör verileri dinleniyor (Hazır!) ---")
    channel.start_consuming()
