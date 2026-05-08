#pragma once
#include <vector>
#include <string>

struct DetectionMetrics {
    double inference_time_ms;
    double fps;
    int    people_count;
};

// Salva o JSON de métricas em disco
void save_metrics(const std::vector<DetectionMetrics>& history, const std::string& filename = "metrics.json");