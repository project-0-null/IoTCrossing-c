#pragma once

#include <string>

// Estrutura com todas as métricas de tempo da requisição
struct ResponseMetrics {
    double ttfb_ms;         // Time To First Byte — tempo até o Orion começar a responder
    double total_ms;        // Tempo total da requisição (ida + volta completa)
    double connect_ms;      // Tempo só para estabelecer conexão TCP
    long   http_code;       // HTTP status code (201, 200, 400, etc)
    bool   success;         // false se houve erro de rede
    std::string error_msg;  // mensagem de erro, se houver
};

// Envia o JSON para o broker e retorna as métricas de tempo
// url:     endpoint completo, ex: "http://127.0.0.1:31330/ngsi-ld/v1/entities"
// json:    payload em formato NGSI-LD
// returns: ResponseMetrics com os tempos medidos
ResponseMetrics send_and_measure(const std::string& url, const std::string& json);

// Imprime as métricas no terminal de forma legível
void print_metrics(const ResponseMetrics& metrics);