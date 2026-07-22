#pragma once
#include <string>

struct FiwareConfig {
    std::string broker_url  = "http://127.0.0.1:1026";
    std::string entity_id   = "urn:ngsi-ld:ItemFlowObserved:1";
    std::string entity_name = "RaspberryPi_Device";
    double      latitude    = 0.0;
    double      longitude   = 0.0;
    int         interval_s  = 10;

    static FiwareConfig load_from_file(const std::string& filepath = "config.json");
};

class FiwareClient {
public:
    explicit FiwareClient(const FiwareConfig& cfg, const std::string& session_ts = "");
    ~FiwareClient();

    // Chama no início: tenta POST, aceita 409 (já existe)
    bool init_entity();

    // Chama no loop: PATCH com peopleCount + dateObserved
    void update(int people_count);

private:
    FiwareConfig cfg_;
    std::string  csv_filename_;
    long http_request(const std::string& method,
                      const std::string& url,
                      const std::string& body);
    std::string iso8601_now() const;
};