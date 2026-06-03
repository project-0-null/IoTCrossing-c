#include "fiware_client.h"
#include "../include/json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

static const std::string CTX_ETSI = "https://uri.etsi.org/ngsi-ld/v1/ngsi-ld-core-context-v1.8.jsonld";
static const std::string URI_NAME = "https://uri.etsi.org/ngsi-ld/name";
static const std::string URI_PPC  = "https://uri.fiware.org/ns/data-models#peopleCount";

// ── Callback interno do libcurl ──────────────────────────────────────────────
static size_t curl_discard(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

// ── Construtor / Destrutor ───────────────────────────────────────────────────
FiwareClient::FiwareClient(const FiwareConfig& cfg) : cfg_(cfg) {
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
        std::cerr << "[FIWARE] curl erro: " << curl_easy_strerror(res) << std::endl;
    else
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return http_code;
}

// ── POST: cria entidade ──────────────────────────────────────────────────────
bool FiwareClient::init_entity() {
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
    std::cerr << "[FIWARE] Falha no POST em " << url << ". HTTP " << code << std::endl;
    return false;
}

// ── PATCH: atualiza atributos ────────────────────────────────────────────────
void FiwareClient::update(int people_count) {
    json attrs;
    attrs["@context"]  = CTX_ETSI;
    attrs["timestamp"] = {{"type", "Property"}, {"value", iso8601_now()}};
    attrs[URI_PPC]     = {{"type", "Property"}, {"value", people_count}};

    std::string url = cfg_.broker_url + "/ngsi-ld/v1/entities/" + cfg_.entity_id + "/attrs";
    
    std::cout << "[FIWARE] Enviando PATCH para: " << url << std::endl;
    std::cout << "[FIWARE] Dados (JSON): " << attrs.dump() << std::endl;

    long code = http_request("PATCH", url, attrs.dump());

    if (code == 204)
        std::cout << "[FIWARE] PATCH OK. Pessoas: " << people_count << std::endl;
    else
        std::cerr << "[FIWARE] PATCH falhou. HTTP " << code << " | URL: " << url << std::endl;
}