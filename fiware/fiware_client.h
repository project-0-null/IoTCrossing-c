#pragma once
#include <string>

struct FiwareConfig {
    std::string broker_url  = "http://127.0.0.1:1026"; // Orion-LD port 1026
    std::string entity_id   = "urn:ngsi-ld:ItemFlowObserved:1";
    std::string entity_name = "RaspberryPi_3";//nome
    double      latitude    = -20.272594;//mudar
    double      longitude   = -40.306346;//mudar
    int         interval_s  = 60;//verificar mediante a teste
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