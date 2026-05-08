#include "metrics.h"
#include "../include/json.hpp"
#include <fstream>
#include <iostream>
#include <numeric>

using json = nlohmann::json;

void save_metrics(const std::vector<DetectionMetrics>& history, const std::string& filename) {
    if (history.empty()) return;

    double sum_inf = 0, sum_fps = 0;
    int    max_people = 0;
    json   j_frames   = json::array();

    for (const auto& m : history) {
        j_frames.push_back({
            {"inference_time_ms", m.inference_time_ms},
            {"fps",               m.fps},
            {"people_count",      m.people_count}
        });
        sum_inf    += m.inference_time_ms;
        sum_fps    += m.fps;
        max_people  = std::max(max_people, m.people_count);
    }

    size_t n = history.size();
    json j_out;
    j_out["frames"]  = j_frames;
    j_out["summary"] = {
        {"total_frames",              (int)n},
        {"average_inference_time_ms", sum_inf / n},
        {"average_fps",               sum_fps / n},
        {"max_people_detected",       max_people}
    };

    std::ofstream f(filename);
    if (f.is_open()) {
        f << j_out.dump(4);
        std::cout << "\n[Metrics] Salvo em '" << filename << "' ("
                  << n << " frames)." << std::endl;
        std::cout << "[Metrics] FPS médio: " << sum_fps / n
                  << " | Inferência média: " << sum_inf / n << "ms" << std::endl;
    } else {
        std::cerr << "[Metrics] Erro ao abrir arquivo." << std::endl;
    }
}