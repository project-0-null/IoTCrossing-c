#include "fiware_client.h"
#include "../include/json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>


using json = nlohmann::json;

static const std::string CTX_ETSI = "https://uri.etsi.org/ngsi-ld/v1/ngsi-ld-core-context-v1.8.jsonld";
static const std::string URI_NAME = "https://uri.etsi.org/ngsi-ld/name";
static const std::string URI_PPC  = "https://uri.fiware.org/ns/data-models#peopleCount";


static void save_metrics(double connect_ms, double ttfb_ms, double total_ms,
                         int people_count, long http_code, const std::string& arquivo,
                         const std::string& status = "OK") {
    if (arquivo.empty()) return;
    
    // Verifica se o arquivo já existe para escrever o cabeçalho só uma vez
    std::ifstream check(arquivo);
    bool novo = !check.good();
    check.close();

    std::ofstream f(arquivo, std::ios::app);  // append — não sobrescreve
    if (!f.is_open()) return;

    if (novo)
        f << "timestamp,pessoas,http_code,connect_ms,ttfb_ms,total_ms,status\n";

    // Timestamp atual
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ts;
    ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S");
    ts << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";

    f << std::fixed << std::setprecision(3);
    f << ts.str()      << ","
      << people_count  << ","
      << http_code     << ","
      << connect_ms    << ","
      << ttfb_ms       << ","
      << total_ms      << ","
      << status        << "\n";
}

// ── Callback interno do libcurl ──────────────────────────────────────────────
static size_t curl_discard(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

// ── Carregar Configuração ───────────────────────────────────────────────────
FiwareConfig FiwareConfig::load_from_file(const std::string& filepath) {
    FiwareConfig cfg;
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "[Config] Arquivo '" << filepath << "' nao encontrado. Usando configuracao padrao." << std::endl;
        return cfg;
    }

    try {
        json j = json::parse(f, nullptr, true, true);
        if (j.contains("broker_url"))  cfg.broker_url  = j["broker_url"].get<std::string>();
        if (j.contains("entity_id"))   cfg.entity_id   = j["entity_id"].get<std::string>();
        if (j.contains("entity_name")) cfg.entity_name = j["entity_name"].get<std::string>();
        if (j.contains("latitude"))    cfg.latitude    = j["latitude"].get<double>();
        if (j.contains("longitude"))   cfg.longitude   = j["longitude"].get<double>();
        if (j.contains("interval_s"))  cfg.interval_s  = j["interval_s"].get<int>();
        std::cout << "[Config] Configuracoes carregadas de '" << filepath << "'." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Erro ao ler '" << filepath << "': " << e.what() << std::endl;
    }

    return cfg;
}

// ── Construtor / Destrutor ───────────────────────────────────────────────────
FiwareClient::FiwareClient(const FiwareConfig& cfg, const std::string& session_ts) : cfg_(cfg) {
    std::string ts = session_ts;
    if (ts.empty()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
        ts = ss.str();
    }
    csv_filename_ = "metrics_envio_" + ts + ".csv";
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

FiwareClient::~FiwareClient() {
    curl_global_cleanup();
}

// ── Helpers ──────────────────────────────────────────────────────────────────
std::string FiwareClient::iso8601_now() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

long FiwareClient::http_request(const std::string& method,
                                const std::string& url,
                                const std::string& body) {
    try {
        CURL* curl = curl_easy_init();
        if (!curl) return -1;

        long http_code = -1;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/ld+json");
        headers = curl_slist_append(headers, "Accept: application/json");

        curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_discard);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,       10L);

        if (method == "PATCH")
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "[FIWARE] curl erro: " << curl_easy_strerror(res) << " (erro de conexao ignorar)" << std::endl;
        else
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return http_code;
    } catch (const std::exception& e) {
        std::cerr << "[FIWARE] Exceção em http_request: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "[FIWARE] Exceção desconhecida em http_request." << std::endl;
        return -1;
    }
}

// ── POST: cria entidade ──────────────────────────────────────────────────────
bool FiwareClient::init_entity() {
    try {
        json payload;
        payload["@context"] = CTX_ETSI;
        payload["id"]       = cfg_.entity_id;
        payload["type"]     = "ItemFlowObserved";
        
        payload["timestamp"] = {{"type", "Property"}, {"value", iso8601_now()}};
        payload[URI_NAME]    = {{"type", "Property"}, {"value", cfg_.entity_name}};
        payload[URI_PPC]     = {{"type", "Property"}, {"value", 0}};
        
        payload["location"]  = {
            {"type",  "GeoProperty"},
            {"value", {{"type", "Point"}, {"coordinates", {cfg_.longitude, cfg_.latitude}}}}
        };

        std::string url  = cfg_.broker_url + "/ngsi-ld/v1/entities";
        std::cout << "[FIWARE] Tentando POST em: " << url << std::endl;

        long code = http_request("POST", url, payload.dump());

        if (code == 201) {
            std::cout << "[FIWARE] Entidade criada (201)." << std::endl;
            return true;
        } else if (code == 409) {
            std::cout << "[FIWARE] Entidade já existe (409). Continuando." << std::endl;
            return true;
        }
        std::cerr << "[FIWARE] Falha no POST em " << url << ". HTTP " << code << " (erro de conexao ignorar)" << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[FIWARE] Exceção em init_entity: " << e.what() << " (erro de conexao ignorar)" << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[FIWARE] Exceção desconhecida em init_entity (erro de conexao ignorar)" << std::endl;
        return false;
    }
}

// ── PATCH: atualiza atributos ────────────────────────────────────────────────
void FiwareClient::update(int people_count) {
    try {
        json attrs;
        attrs["@context"]  = CTX_ETSI;
        attrs["timestamp"] = {{"type", "Property"}, {"value", iso8601_now()}};
        attrs[URI_PPC]     = {{"type", "Property"}, {"value", people_count}};

        std::string url = cfg_.broker_url + "/ngsi-ld/v1/entities/" + cfg_.entity_id + "/attrs";

        //────────────────────────/time-response/───────────────────────────────────
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "[FIWARE] Falha ao inicializar libcurl." << std::endl;
            save_metrics(0.0, 0.0, 0.0, people_count, 0, csv_filename_, "erro de conexao ignorar");
            return;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/ld+json");
        headers = curl_slist_append(headers, "Accept: application/json");

        std::string body = attrs.dump();
        curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,  "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_discard);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "[FIWARE] curl erro: " << curl_easy_strerror(res) << " (erro de conexao ignorar)" << std::endl;
            // Salva registro no relatório CSV informando o erro de conexão
            save_metrics(0.0, 0.0, 0.0, people_count, 0, csv_filename_, "erro de conexao ignorar");
        } else {
            long   http_code   = 0;
            double connect_ms  = 0;
            double ttfb_ms     = 0;
            double total_ms    = 0;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,      &http_code);
            curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,       &connect_ms);
            curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &ttfb_ms);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,         &total_ms);

            std::string status = (http_code == 204 || http_code == 200 || http_code == 201)
                                 ? "OK"
                                 : "erro de conexao ignorar (HTTP " + std::to_string(http_code) + ")";

            // Salva no CSV
            save_metrics(connect_ms * 1000, ttfb_ms * 1000, total_ms * 1000, people_count, http_code, csv_filename_, status);

            std::cout << "\n===== Metricas de Envio =====" << std::endl;
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "  HTTP Status : " << http_code         << std::endl;
            std::cout << "  Conexao TCP : " << connect_ms * 1000 << " ms" << std::endl;
            std::cout << "  TTFB        : " << ttfb_ms    * 1000 << " ms  <- latencia principal" << std::endl;
            std::cout << "  Total RTT   : " << total_ms   * 1000 << " ms" << std::endl;
            std::cout << "  Status      : " << status            << std::endl;
            std::cout << "=============================" << std::endl;

            if (http_code == 204)
                std::cout << "[FIWARE] PATCH OK. Pessoas: " << people_count << std::endl;
            else
                std::cerr << "[FIWARE] PATCH falhou. HTTP " << http_code << " (erro de conexao ignorar)" << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } catch (const std::exception& e) {
        std::cerr << "[FIWARE] Exceção em update(): " << e.what() << " (erro de conexao ignorar)" << std::endl;
        save_metrics(0.0, 0.0, 0.0, people_count, 0, csv_filename_, "erro de conexao ignorar");
    } catch (...) {
        std::cerr << "[FIWARE] Exceção desconhecida em update() (erro de conexao ignorar)" << std::endl;
        save_metrics(0.0, 0.0, 0.0, people_count, 0, csv_filename_, "erro de conexao ignorar");
    }
}

