// FactoryMind edge sensor
// Simulates machine telemetry (temperature, vibration) for a few CNC
// machines and publishes a JSON alarm to RabbitMQ (AMQP 0-9-1) whenever a
// reading crosses its limit. Normal readings stay on the edge device.
//
// Build: g++ -std=c++17 -O2 main.cpp -lrabbitmq -o sensor

#if __has_include(<rabbitmq-c/amqp.h>)
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#else
#include <amqp.h>
#include <amqp_tcp_socket.h>
#endif

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string env(const char* key, const char* fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? v : fallback;
}

struct Limits {
    double temp_c = 90.0;       // °C
    double vibration = 7.1;     // mm/s RMS (ISO 10816 zone C/D border, class II)
};

struct Machine {
    std::string id;
    double base_temp;     // normal operating temperature
    double temp;          // current temperature
    double vibration;     // current vibration, mm/s
    bool in_alarm = false;
    int fault_ticks = 0;  // >0 while a simulated fault is developing
};

// --- AMQP -------------------------------------------------------------------

class Publisher {
public:
    bool connect(const std::string& host, int port, const std::string& user, const std::string& pass) {
        conn_ = amqp_new_connection();
        amqp_socket_t* sock = amqp_tcp_socket_new(conn_);
        if (!sock || amqp_socket_open(sock, host.c_str(), port) != AMQP_STATUS_OK) return fail("socket open");
        if (amqp_login(conn_, "/", 0, 131072, 30, AMQP_SASL_METHOD_PLAIN, user.c_str(), pass.c_str()).reply_type !=
            AMQP_RESPONSE_NORMAL)
            return fail("login");
        amqp_channel_open(conn_, 1);
        if (amqp_get_rpc_reply(conn_).reply_type != AMQP_RESPONSE_NORMAL) return fail("channel open");
        // Same declaration as the consumer: non-durable queue "factory.sensors".
        amqp_queue_declare(conn_, 1, amqp_cstring_bytes(queue_), 0, 0, 0, 0, amqp_empty_table);
        if (amqp_get_rpc_reply(conn_).reply_type != AMQP_RESPONSE_NORMAL) return fail("queue declare");
        return true;
    }

    bool publish(const std::string& json) {
        amqp_basic_properties_t props{};
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
        props.content_type = amqp_cstring_bytes("application/json");
        int rc = amqp_basic_publish(conn_, 1, amqp_empty_bytes, amqp_cstring_bytes(queue_), 0, 0, &props,
                                    amqp_cstring_bytes(json.c_str()));
        return rc == AMQP_STATUS_OK;
    }

    ~Publisher() {
        if (!conn_) return;
        amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
        amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(conn_);
    }

private:
    bool fail(const char* what) {
        std::fprintf(stderr, "[AMQP] %s failed\n", what);
        if (conn_) amqp_destroy_connection(conn_);
        conn_ = nullptr;
        return false;
    }
    amqp_connection_state_t conn_ = nullptr;
    const char* queue_ = "factory.sensors";
};

// --- Simulation -------------------------------------------------------------

std::string iso_now() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

std::string alarm_json(const Machine& m, const char* status, const char* reason) {
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "{\"sensor_id\":\"%s\",\"status\":\"%s\",\"reason\":\"%s\","
                  "\"temperature\":%.1f,\"vibration\":%.2f,\"timestamp\":\"%s\"}",
                  m.id.c_str(), status, reason, m.temp, m.vibration, iso_now().c_str());
    return buf;
}

}  // namespace

int main() {
    const std::string host = env("RABBIT_HOST", "localhost");
    const int port = std::atoi(env("RABBIT_PORT", "5672").c_str());
    const std::string user = env("RABBIT_USER", "admin");
    const std::string pass = env("RABBIT_PASS", "mercedes2026");
    const int period_ms = std::atoi(env("SAMPLE_MS", "1000").c_str());

    std::setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered so `docker logs` shows output live

    Publisher pub;
    for (int attempt = 1; !pub.connect(host, port, user, pass); ++attempt) {
        std::fprintf(stderr, "[SENSOR] RabbitMQ not ready (try %d), retrying in 2 s...\n", attempt);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::printf("[SENSOR] Connected to %s:%d, publishing to factory.sensors\n", host.c_str(), port);

    std::vector<Machine> machines = {
        {"CNC-01", 68.0, 68.0, 2.4}, {"CNC-02", 72.0, 72.0, 3.1}, {"PRESS-03", 61.0, 61.0, 4.0}};
    const Limits lim;
    std::mt19937 rng{std::random_device{}()};
    std::normal_distribution<double> noise(0.0, 0.35);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    for (unsigned long tick = 0;; ++tick) {
        for (auto& m : machines) {
            // Occasionally start a fault: coolant loss heats the machine, a worn
            // bearing raises vibration. Faults develop over ~20-40 samples.
            if (m.fault_ticks == 0 && !m.in_alarm && uni(rng) < 0.01) m.fault_ticks = 20 + int(uni(rng) * 20);

            double target_t = m.base_temp + (m.fault_ticks > 0 ? 30.0 : 0.0);
            double target_v = (m.fault_ticks > 0 && m.id == "PRESS-03") ? 9.0 : 2.5;
            m.temp += (target_t - m.temp) * 0.12 + noise(rng);
            m.vibration = std::max(0.0, m.vibration + (target_v - m.vibration) * 0.15 + noise(rng) * 0.2);
            if (m.fault_ticks > 0) --m.fault_ticks;

            const bool hot = m.temp >= lim.temp_c;
            const bool shaking = m.vibration >= lim.vibration;

            if ((hot || shaking) && !m.in_alarm) {
                m.in_alarm = true;
                const char* reason = hot && shaking ? "temperature+vibration" : hot ? "temperature" : "vibration";
                std::string msg = alarm_json(m, "CRITICAL", reason);
                bool ok = pub.publish(msg);
                std::printf("[SENSOR] ALARM %s -> %s %s\n", m.id.c_str(), ok ? "sent" : "SEND FAILED", msg.c_str());
            } else if (m.in_alarm && m.temp < lim.temp_c - 5.0 && m.vibration < lim.vibration - 1.0) {
                // Hysteresis: only clear once well below the limits, so one noisy
                // reading can't fire the same alarm twice.
                m.in_alarm = false;
                std::printf("[SENSOR] %s back to normal (%.1f C, %.2f mm/s)\n", m.id.c_str(), m.temp, m.vibration);
            }
        }
        if (tick % 10 == 0) {
            for (const auto& m : machines)
                std::printf("  %-8s %5.1f C  %4.2f mm/s%s\n", m.id.c_str(), m.temp, m.vibration, m.in_alarm ? "  [ALARM]" : "");
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}
