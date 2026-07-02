#include "response_time.h"

#include <curl/curl.h>
#include <iostream>
#include <iomanip>

// Callback que descarta o body da resposta (não precisamos do conteúdo)
static size_t discard_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    return size * nmemb;
}

ResponseMetrics send_and_measure(const std::string& url, const std::string& json) {
    ResponseMetrics metrics = {};

    CURL* curl = curl_easy_init();
    if (!curl) {
        metrics.success   = false;
        metrics.error_msg = "Falha ao inicializar curl";
        return metrics;
    }

    // Headers NGSI-LD
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/ld+json");
    headers = curl_slist_append(headers, "Accept: application/ld+json");

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    json.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback);

    // Timeout para não travar o loop caso o broker esteja fora
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);  // 3s para conectar
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        5L);  // 5s no total

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        metrics.success   = false;
        metrics.error_msg = curl_easy_strerror(res);
    } else {
        metrics.success = true;

        // Métricas internas do curl (em segundos — convertemos para ms)
        double connect_time      = 0;
        double starttransfer     = 0;
        double total_time        = 0;

        curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,       &connect_time);
        curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &starttransfer);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,         &total_time);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,      &metrics.http_code);

        metrics.connect_ms = connect_time  * 1000.0;
        metrics.ttfb_ms    = starttransfer * 1000.0;
        metrics.total_ms   = total_time    * 1000.0;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return metrics;
}

void print_metrics(const ResponseMetrics& metrics) {
    std::cout << "\n===== Metricas de Envio =====" << std::endl;

    if (!metrics.success) {
        std::cout << "  ERRO: " << metrics.error_msg << std::endl;
        std::cout << "=============================" << std::endl;
        return;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  HTTP Status : " << metrics.http_code  << std::endl;
    std::cout << "  Conexao TCP : " << metrics.connect_ms << " ms" << std::endl;
    std::cout << "  TTFB        : " << metrics.ttfb_ms    << " ms  <- latencia principal" << std::endl;
    std::cout << "  Total RTT   : " << metrics.total_ms   << " ms" << std::endl;
    std::cout << "=============================" << std::endl;
}