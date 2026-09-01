#include "mqtt_client.h"
#include "../include/json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>
#include <thread>

using json = nlohmann::json;

// ── Callback interno do libcurl para descarte de dados recebidos ──────────────
static size_t curl_discard_cb(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

// ── Carregar Configuração ───────────────────────────────────────────────────
FiwareMqttConfig FiwareMqttConfig::load_from_file(const std::string& filepath) {
    FiwareMqttConfig cfg;
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "[Config] Arquivo '" << filepath << "' nao encontrado. Usando configuracao padrao." << std::endl;
        return cfg;
    }

    try {
        json j = json::parse(f, nullptr, true, true);
        if (j.contains("mqtt_host"))   cfg.mqtt_host   = j["mqtt_host"].get<std::string>();
        if (j.contains("mqtt_port"))   cfg.mqtt_port   = j["mqtt_port"].get<int>();
        if (j.contains("iota_host"))   cfg.iota_host   = j["iota_host"].get<std::string>();
        if (j.contains("iota_port"))   cfg.iota_port   = j["iota_port"].get<int>();
        if (j.contains("api_key"))     cfg.api_key     = j["api_key"].get<std::string>();
        if (j.contains("device_id"))   cfg.device_id   = j["device_id"].get<std::string>();
        if (j.contains("entity_name")) cfg.entity_name = j["entity_name"].get<std::string>();
        if (j.contains("cbroker_url")) cfg.cbroker_url = j["cbroker_url"].get<std::string>();
        if (j.contains("latitude"))    cfg.latitude    = j["latitude"].get<double>();
        if (j.contains("longitude"))   cfg.longitude   = j["longitude"].get<double>();
        if (j.contains("interval_s"))  cfg.interval_s  = j["interval_s"].get<int>();
        if (j.contains("keepalive_s")) cfg.keepalive_s = j["keepalive_s"].get<int>();
        std::cout << "[Config] Configuracoes carregadas de '" << filepath << "' com sucesso." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Erro ao ler '" << filepath << "': " << e.what() << std::endl;
    }

    return cfg;
}

// ── Callbacks Estáticos do Mosquitto ─────────────────────────────────────────
void FiwareMqttClient::on_connect_cb(struct mosquitto*, void* obj, int rc) {
    auto* self = static_cast<FiwareMqttClient*>(obj);
    if (rc == 0) {
        self->connected_ = true;
        std::cout << "[MQTT] Conectado ao Broker Mosquitto com sucesso! (RC=0)" << std::endl;
    } else {
        self->connected_ = false;
        std::cerr << "[MQTT] Falha ao conectar no Broker. Código de retorno: " << rc
                  << " (" << mosquitto_strerror(rc) << ")" << std::endl;
    }
}

void FiwareMqttClient::on_disconnect_cb(struct mosquitto*, void* obj, int rc) {
    auto* self = static_cast<FiwareMqttClient*>(obj);
    self->connected_ = false;
    if (rc == 0) {
        std::cout << "[MQTT] Desconectado do Broker limpo." << std::endl;
    } else {
        std::cerr << "[MQTT] Desconexao inesperada do Broker (RC=" << rc << "). Tentando reconectar automaticamente..." << std::endl;
    }
}

void FiwareMqttClient::on_publish_cb(struct mosquitto*, void*, int mid) {
    // Callback acionado quando a mensagem MQTT for entregue (útil para confirmação QoS 1/2)
}

// ── Construtor e Destrutor ──────────────────────────────────────────────────
FiwareMqttClient::FiwareMqttClient(const FiwareMqttConfig& cfg, const std::string& session_ts)
    : cfg_(cfg), session_ts_(session_ts) {

    if (session_ts_.empty()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
        session_ts_ = ss.str();
    }
    csv_filename_ = "metrics_envio_" + session_ts_ + ".csv";
    topic_attrs_  = "/" + cfg_.api_key + "/" + cfg_.device_id + "/attrs";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    mosquitto_lib_init();
}

FiwareMqttClient::~FiwareMqttClient() {
    if (mosq_) {
        mosquitto_disconnect(mosq_);
        mosquitto_loop_stop(mosq_, true);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }
    mosquitto_lib_cleanup();
    curl_global_cleanup();
}

// ── Helpers de Timestamp e Métricas ──────────────────────────────────────────
std::string FiwareMqttClient::iso8601_now() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

void FiwareMqttClient::log_metrics(double connect_ms, double pub_ms, double total_ms,
                                   int people_count, long status_code, const std::string& status) {
    if (csv_filename_.empty()) return;

    std::ifstream check(csv_filename_);
    bool novo = !check.good();
    check.close();

    std::ofstream f(csv_filename_, std::ios::app);
    if (!f.is_open()) return;

    if (novo) {
        f << "timestamp,pessoas,http_code,connect_ms,ttfb_ms,total_ms,status\n";
    }

    f << std::fixed << std::setprecision(3);
    f << iso8601_now()  << ","
      << people_count   << ","
      << status_code    << ","
      << connect_ms     << ","
      << pub_ms         << ","
      << total_ms       << ","
      << status         << "\n";
}

bool FiwareMqttClient::is_connected() const {
    return connected_;
}

// ── Fase 1: Auto-Provisionamento no IoT Agent (HTTP POST Único no Boot) ─────
bool FiwareMqttClient::provision_device() {
    try {
        // 1. Provisiona o Grupo de Serviços (Service Group)
        {
            std::string srv_url = "http://" + cfg_.iota_host + ":" + std::to_string(cfg_.iota_port) + "/iot/services";
            json srv_root;
            json srv_item;
            srv_item["apikey"]      = cfg_.api_key;
            srv_item["cbroker"]     = cfg_.cbroker_url;
            srv_item["entity_type"] = "ItemFlowObserved";
            srv_item["resource"]    = "/iot/d";
            srv_root["services"]    = json::array({srv_item});
            std::string srv_payload = srv_root.dump();

            CURL* curl = curl_easy_init();
            if (curl) {
                struct curl_slist* headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "fiware-service: openiot");
                headers = curl_slist_append(headers, "fiware-servicepath: /");

                curl_easy_setopt(curl, CURLOPT_URL,            srv_url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     srv_payload.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)srv_payload.size());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_discard_cb);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);

                long code = 0;
                if (curl_easy_perform(curl) == CURLE_OK) {
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
                    if (code == 201) {
                        std::cout << "[FIWARE IoT Agent] Grupo de servicos '" << cfg_.api_key << "' cadastrado (201)." << std::endl;
                    } else if (code == 409 || code == 200) {
                        std::cout << "[FIWARE IoT Agent] Grupo de servicos ja existia. OK." << std::endl;
                    }
                }
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
        }

        // 2. Provisiona o Dispositivo (Device)
        std::string url = "http://" + cfg_.iota_host + ":" + std::to_string(cfg_.iota_port) + "/iot/devices";
        std::cout << "[FIWARE IoT Agent] Auto-provisionando sensor em: " << url << std::endl;

        // Monta o payload de provisionamento UltraLight 2.0
        json root;
        json device;
        device["device_id"]   = cfg_.device_id;
        device["entity_name"] = cfg_.entity_name;
        device["entity_type"] = "ItemFlowObserved";
        device["transport"]   = "MQTT";
        device["apikey"]      = cfg_.api_key;

        // Atributos dinâmicos mapeados (p -> peopleCount, d -> dateObserved)
        device["attributes"] = json::array({
            {{"object_id", "p"}, {"name", "peopleCount"}, {"type", "Integer"}},
            {{"object_id", "d"}, {"name", "dateObserved"}, {"type", "DateTime"}}
        });

        // Atributos estáticos (localização geográfica e nome do dispositivo)
        std::stringstream loc_ss;
        loc_ss << std::fixed << std::setprecision(6) << cfg_.latitude << ", " << cfg_.longitude;

        device["static_attributes"] = json::array({
            {{"name", "location"}, {"type", "geo:point"}, {"value", loc_ss.str()}},
            {{"name", "name"},     {"type", "Text"},      {"value", cfg_.device_id}}
        });

        root["devices"] = json::array({device});
        std::string payload_str = root.dump();

        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "[FIWARE IoT Agent] Falha ao inicializar libcurl para provisionamento." << std::endl;
            return false;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "fiware-service: openiot");
        headers = curl_slist_append(headers, "fiware-servicepath: /");

        curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)payload_str.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_discard_cb);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);

        long http_code = 0;
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (http_code == 201) {
            std::cout << "[FIWARE IoT Agent] Dispositivo '" << cfg_.device_id << "' cadastrado com sucesso (201 Created)!" << std::endl;
            return true;
        } else if (http_code == 409) {
            std::cout << "[FIWARE IoT Agent] Dispositivo '" << cfg_.device_id << "' já estava provisionado (409 Conflict). Continuando." << std::endl;
            return true;
        } else {
            std::cerr << "[FIWARE IoT Agent] Aviso no provisionamento HTTP (" << http_code
                      << "). Prosseguindo para conexão MQTT..." << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FIWARE IoT Agent] Exceção no provisionamento: " << e.what() << std::endl;
        return false;
    }
}

// ── Fase 2: Conexão com o Broker MQTT Mosquitto ─────────────────────────────
bool FiwareMqttClient::connect_mqtt() {
    std::string client_id = "edge_" + cfg_.device_id + "_" + session_ts_;
    mosq_ = mosquitto_new(client_id.c_str(), true, this);
    if (!mosq_) {
        std::cerr << "[MQTT] Falha ao criar instancia do mosquitto." << std::endl;
        return false;
    }

    mosquitto_connect_callback_set(mosq_, on_connect_cb);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect_cb);
    mosquitto_publish_callback_set(mosq_, on_publish_cb);

    // Habilita reconexão automática com backoff exponencial suave
    mosquitto_reconnect_delay_set(mosq_, 1, 10, true);

    std::cout << "[MQTT] Conectando ao broker em " << cfg_.mqtt_host << ":" << cfg_.mqtt_port << "..." << std::endl;
    int rc = mosquitto_connect(mosq_, cfg_.mqtt_host.c_str(), cfg_.mqtt_port, cfg_.keepalive_s);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Falha inicial ao conectar no Broker (" << mosquitto_strerror(rc)
                  << "). A thread de loop tentara reconectar automaticamente." << std::endl;
    }

    // Inicia thread de background assíncrona do Mosquitto para gerenciar o socket e keep-alive
    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Falha ao iniciar loop assíncrono do Mosquitto." << std::endl;
        return false;
    }

    return true;
}

// ── Inicialização Geral (Boot) ──────────────────────────────────────────────
bool FiwareMqttClient::init() {
    provision_device();
    return connect_mqtt();
}

// ── Publicação Periódica de Dados (Runtime) ─────────────────────────────────
void FiwareMqttClient::update(int people_count) {
    auto t_inicio = std::chrono::steady_clock::now();

    // Monta o timestamp e o payload ultraleve (formato UltraLight 2.0)
    std::string ts_iso = iso8601_now();
    std::string payload = "p|" + std::to_string(people_count) + "|d|" + ts_iso;

    int rc = MOSQ_ERR_NO_CONN;
    if (mosq_) {
        rc = mosquitto_publish(mosq_, nullptr, topic_attrs_.c_str(),
                               (int)payload.size(), payload.c_str(), 0, false);
    }

    auto t_fim = std::chrono::steady_clock::now();
    double pub_ms = std::chrono::duration_cast<std::chrono::microseconds>(t_fim - t_inicio).count() / 1000.0;

    std::string status = (rc == MOSQ_ERR_SUCCESS) ? "OK" : ("MQTT Erro " + std::to_string(rc));
    long status_code   = (rc == MOSQ_ERR_SUCCESS) ? 200 : 0;

    // Registra métrica no arquivo CSV mantendo o formato compatível com scripts legados
    log_metrics(0.0, pub_ms, pub_ms, people_count, status_code, status);

    std::cout << "\n===== Metricas de Envio (MQTT UltraLight) =====" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Topico MQTT  : " << topic_attrs_ << std::endl;
    std::cout << "  Payload      : " << payload << " (" << payload.size() << " bytes)" << std::endl;
    std::cout << "  Latencia Pub : " << pub_ms << " ms" << std::endl;
    std::cout << "  Status       : " << status << " (Code: " << status_code << ")" << std::endl;
    std::cout << "  Pessoas      : " << people_count << " | Horario: " << ts_iso << std::endl;
    std::cout << "===============================================" << std::endl;
}
