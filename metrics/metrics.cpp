#include "metrics.h"
#include "../include/json.hpp"
#include <fstream>
#include <iostream>
#include <numeric>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

void save_metrics(const std::vector<DetectionMetrics>& history, const std::string& filename) {
    std::string target_file = filename;
    if (target_file.empty()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "metrics_output_" << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S") << ".json";
        target_file = ss.str();
    }

    try {
        if (history.empty()) {
            json j_out;
            j_out["frames"]  = json::array();
            j_out["summary"] = {
                {"total_frames",              0},
                {"average_inference_time_ms", 0.0},
                {"average_fps",               0.0},
                {"max_people_detected",       0}
            };
            std::ofstream f(target_file);
            if (f.is_open()) {
                f << j_out.dump(4);
                std::cout << "\n[Metrics] Salvo em '" << target_file << "' (0 frames)." << std::endl;
            }
            return;
        }

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

        std::ofstream f(target_file);
        if (f.is_open()) {
            f << j_out.dump(4);
            std::cout << "\n[Metrics] Salvo em '" << target_file << "' ("
                      << n << " frames)." << std::endl;
            std::cout << "[Metrics] FPS médio: " << sum_fps / n
                      << " | Inferência média: " << sum_inf / n << "ms" << std::endl;
        } else {
            std::cerr << "[Metrics] Erro ao abrir arquivo: " << target_file << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Metrics] Exceção ao salvar métricas: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[Metrics] Exceção desconhecida ao salvar métricas." << std::endl;
    }
}