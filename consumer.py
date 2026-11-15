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

# Sensörün izlediği makineler için eksik teknisyen notlarını ekle (upsert: tekrar çalıştırmak güvenli)
EXTRA_NOTES = {
    "note-cnc02-temp": "CNC-02: Sıcaklık 90°C'yi geçerse soğutma sıvısı seviyesini ve fan kayışını kontrol et; seviye düşükse sızıntı ara.",
    "note-press03-vib": "PRESS-03: Titreşim 7 mm/s üzerine çıkarsa ana mil rulmanı aşınmıştır; makineyi durdur, rulmanı değiştir.",
    "note-press03-mount": "PRESS-03: Titreşim ani artıp sıcaklık da yükseliyorsa gövde bağlantı cıvatalarını ve hizalamayı kontrol et.",
    "note-press03-temp": "PRESS-03: Sıcaklık 85°C'yi geçerse hidrolik yağ seviyesini ve yağ soğutucusunu kontrol et.",
}
collection.upsert(ids=list(EXTRA_NOTES), documents=list(EXTRA_NOTES.values()))

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
    
    reason = data.get('reason', 'temperature')
    vib = data.get('vibration')
    print(f"\n🔔 [SENSOR] Alarm: {data['sensor_id']} - {data['status']} ({reason})")
    print(f"   Sıcaklık: {data['temperature']}°C" + (f" · Titreşim: {vib} mm/s" if vib is not None else ""))
    
    parts = []
    if 'temperature' in reason:
        parts.append(f"sıcaklık {data['temperature']}°C'ye ulaştı")
    if 'vibration' in reason and vib is not None:
        parts.append(f"titreşim {vib} mm/s'ye çıktı (sıcaklık {data['temperature']}°C)")
    alert_msg = f"{data['sensor_id']} makinesinde " + " ve ".join(parts) + "."
    
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
