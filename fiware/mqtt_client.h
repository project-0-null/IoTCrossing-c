#pragma once
#include <string>
#include <mosquitto.h>

struct FiwareMqttConfig {
    std::string mqtt_host   = "127.0.0.1";
    int         mqtt_port   = 1883;
    std::string iota_host   = "127.0.0.1";
    int         iota_port   = 4041;
    std::string api_key     = "smartcrossing";
    std::string device_id   = "rasp_crossing_01";
    std::string entity_name = "urn:ngsi-ld:ItemFlowObserved:1";
    std::string cbroker_url = "http://orion.fiware-kata.svc.cluster.local:1026";
    double      latitude    = 0.0;
    double      longitude   = 0.0;
    int         interval_s  = 1;
    int         keepalive_s = 60;

    static FiwareMqttConfig load_from_file(const std::string& filepath = "config.json");
};

class FiwareMqttClient {
public:
    explicit FiwareMqttClient(const FiwareMqttConfig& cfg, const std::string& session_ts = "");
    ~FiwareMqttClient();

    // Inicialização: Auto-provisiona o sensor no IoT Agent (HTTP POST) e conecta ao broker MQTT
    bool init();

    // Publicação periódica: envia "p|<qtd>|d|<timestamp_iso8601>" no tópico MQTT
    void update(int people_count);

    // Retorna se o cliente MQTT está atualmente conectado
    bool is_connected() const;

private:
    FiwareMqttConfig cfg_;
    std::string      session_ts_;
    std::string      csv_filename_;
    std::string      topic_attrs_;
    struct mosquitto* mosq_ = nullptr;
    bool             connected_ = false;

    // Métodos internos
    bool provision_device();
    bool connect_mqtt();
    std::string iso8601_now() const;
    void log_metrics(double connect_ms, double pub_ms, double total_ms,
                     int people_count, long status_code, const std::string& status);

    // Callbacks do Mosquitto
    static void on_connect_cb(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect_cb(struct mosquitto* mosq, void* obj, int rc);
    static void on_publish_cb(struct mosquitto* mosq, void* obj, int mid);
};
