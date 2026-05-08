#pragma once
#include <string>

struct FiwareConfig {
    std::string broker_url  = "http://127.0.0.1:31330/ngsi-ld/v1/entities/";//achar o ip do broker
    std::string entity_id   = "urn:ngsi-ld:CrowdFlowObserved:RaspberryPi_01";
    double      latitude    = -23.5505;
    double      longitude   = -46.6333;
    int         interval_s  = 60;
};

class FiwareClient {
public:
    explicit FiwareClient(const FiwareConfig& cfg);
    ~FiwareClient();

    // Chama no início: tenta POST, aceita 409 (já existe)
    bool init_entity();

    // Chama no loop: PATCH com peopleCount + dateObserved
    void update(int people_count);

private:
    FiwareConfig cfg_;
    long http_request(const std::string& method,
                      const std::string& url,
                      const std::string& body);
    std::string iso8601_now() const;
};