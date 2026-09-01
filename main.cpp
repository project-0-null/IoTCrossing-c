#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <csignal>
#include <iomanip>
#include <ctime>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "yolo-fastestv2.h"
#include "metrics/metrics.h"
#include "fiware/mqtt_client.h"

using namespace std;
using namespace cv;

// ── Controle de ciclo de vida e sincronização entre threads ─────────────────
std::atomic<bool> run_loop{true};
std::atomic<bool> worker_active{true};
std::atomic<int>  shared_people_count{0};
std::mutex        cv_mutex;
std::condition_variable cv_sleep;

void signalHandler(int) {
    cout << "\n[SISTEMA] Sinal de interrupção recebido. Finalizando e salvando dados..." << endl;
    run_loop = false;
    worker_active = false;
    cv_sleep.notify_all(); // Acorda a worker thread imediatamente para encerramento limpo
}

void yolo_init(yoloFastestv2& yolo) {
    yolo.loadModel("yolo-fastestv2-opt.param", "yolo-fastestv2-opt.bin");
    cout << "[YOLO] Modelo carregado." << endl;
}

static std::string get_session_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}

static std::string get_current_time_str() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%H:%M:%S");
    return ss.str();
}

// ── Worker Thread Dedicada para Publicação MQTT UltraLight ──────────────────
// Esta função roda 100% isolada em uma thread separada.
// Ela não bloqueia nem compete com a captura de vídeo e inferência YOLO.
void fiware_worker_thread(FiwareMqttClient* fiware, int interval_seconds) {
    cout << "[FIWARE MQTT Worker] Thread dedicada iniciada (Intervalo de envio: "
         << interval_seconds << "s)." << endl;

    while (worker_active.load(std::memory_order_relaxed)) {
        // Aguarda pelo intervalo configurado ou acorda imediatamente se o programa for encerrado
        std::unique_lock<std::mutex> lock(cv_mutex);
        bool encerrando = cv_sleep.wait_for(lock, std::chrono::seconds(interval_seconds), []() {
            return !worker_active.load(std::memory_order_relaxed);
        });

        if (encerrando) {
            // Se foi acordado para finalizar o programa, encerra o loop da thread
            break;
        }

        // Lê a contagem de pessoas mais recente de forma atômica (thread-safe, sem locks pesados)
        int pessoas_atual = shared_people_count.load(std::memory_order_relaxed);

        try {
            fiware->update(pessoas_atual);
        } catch (const std::exception& e) {
            cerr << "[FIWARE MQTT Worker] Erro no envio periódico: " << e.what() << " (ignorado)" << endl;
        } catch (...) {
            cerr << "[FIWARE MQTT Worker] Erro desconhecido no envio periódico (ignorado)" << endl;
        }
    }

    cout << "[FIWARE MQTT Worker] Thread de envio finalizada com sucesso." << endl;
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::string session_ts = get_session_timestamp();
    std::string video_filename = "video_output_" + session_ts + ".mp4";
    vector<DetectionMetrics> history;
    VideoCapture cap;
    VideoWriter video_writer;

    try {
        // ── Configuração MQTT / FIWARE IoT Agent (carregada de config.json) ──
        FiwareMqttConfig cfg = FiwareMqttConfig::load_from_file("config.json");

        FiwareMqttClient fiware(cfg, session_ts);
        try {
            // Auto-provisiona o sensor no IoT Agent e conecta no Broker MQTT
            fiware.init();
        } catch (const std::exception& e) {
            cerr << "[FIWARE] Falha ao tentar conectar no início: " << e.what() << " (ignorado)" << endl;
        } catch (...) {
            cerr << "[FIWARE] Falha desconhecida ao tentar conectar no início (ignorado)" << endl;
        }

        // ── Início da Worker Thread Dedicada para envio MQTT ─────────────────
        // A thread gerencia as publicações assíncronas em segundo plano
        std::thread fiware_thread(fiware_worker_thread, &fiware, cfg.interval_s);

        // ── YOLO + câmera ─────────────────────────────────────────────────────
        yoloFastestv2 yolo;
        yolo_init(yolo);

        cap.open(0, CAP_V4L2);
        if (!cap.isOpened()) {
            cerr << "[CAM] Câmera 0 não encontrada. Tentando câmera 1..." << endl;
            cap.open(1, CAP_V4L2);
        }

        if (!cap.isOpened()) {
            cerr << "[CAM] Erro FATAL: Nenhuma câmera encontrada." << endl;
            // Encerra a thread antes de sair
            worker_active = false;
            cv_sleep.notify_all();
            if (fiware_thread.joinable()) fiware_thread.join();
            save_metrics(history, "metrics_output_" + session_ts + ".json");
            return -1;
        }

        cap.set(CAP_PROP_FRAME_WIDTH,  640);
        cap.set(CAP_PROP_FRAME_HEIGHT, 480);

        // Espera a câmera estabilizar (warm-up)
        cout << "[CAM] Aquecendo a câmera..." << endl;
        for(int i = 0; i < 10; i++) {
            Mat temp;
            cap >> temp;
        }

        // ── Configuração do Gravador de Vídeo (.mp4) ──────────────────────────
        int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        double video_fps = 10.0;
        cv::Size frame_size(640, 480);

        video_writer.open(video_filename, fourcc, video_fps, frame_size);
        if (!video_writer.isOpened()) {
            cerr << "[REC] Erro: Não foi possível criar o arquivo de vídeo MP4 (" << video_filename << "). Tentando H264..." << endl;
            fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
            video_writer.open(video_filename, fourcc, video_fps, frame_size);
        }

        if (!video_writer.isOpened()) {
            cerr << "[REC] Erro: Falha ao abrir o gravador MP4." << endl;
        } else {
            cout << "[REC] Gravando vídeo otimizado (MP4) em: " << video_filename << endl;
        }

        // ── Loop principal de processamento de vídeo ─────────────────────────
        int contador = 0;

        while (run_loop.load(std::memory_order_relaxed)) {
            auto frame_inicio = chrono::steady_clock::now();

            Mat frame;
            cap >> frame;
            if (frame.empty()) {
                cerr << "[CAM] Frame vazio! Tentando continuar..." << endl;
                continue;
            }

            cv::flip(frame, frame, 0); // Flip vertical

            vector<TargetBox> caixas;
            auto inf_inicio = chrono::steady_clock::now();
            yolo.detection(frame, caixas);
            auto inf_fim = chrono::steady_clock::now();

            double inf_ms = chrono::duration_cast<chrono::microseconds>(inf_fim - inf_inicio).count() / 1000.0;

            int pessoas = 0;
            for (const auto& c : caixas) {
                if (c.cate == 0 && c.score > 0.4f) {
                    pessoas++;
                    cv::rectangle(frame, cv::Point(c.x1, c.y1), cv::Point(c.x2, c.y2), cv::Scalar(0, 255, 0), 2);
                    
                    std::string label = "Pessoa " + std::to_string((int)(c.score * 100)) + "%";
                    cv::putText(frame, label, cv::Point(c.x1, std::max(15, c.y1 - 5)),
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                }
            }

            // ── Atualização Atômica da Contagem Compartilhada (Custo: < 1 ns) ─────
            shared_people_count.store(pessoas, std::memory_order_relaxed);

            double frame_ms = chrono::duration_cast<chrono::microseconds>(
                chrono::steady_clock::now() - frame_inicio).count() / 1000.0;

            history.push_back({inf_ms, 1000.0 / frame_ms, pessoas});
            contador++;

            if (contador % 30 == 0)
                cout << "[Frame " << contador << "] " << inf_ms << "ms | "
                     << 1000.0 / frame_ms << " fps | Pessoas: " << pessoas << endl;

            // Overlay no vídeo (Horário + Qtd Pessoas)
            std::string osd_info = get_current_time_str() + " | Pessoas: " + std::to_string(pessoas);
            cv::putText(frame, osd_info, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);

            // Gravação do frame no vídeo
            if (video_writer.isOpened()) {
                video_writer.write(frame);
            }
        }

        // ── Encerramento Limpo da Worker Thread ──────────────────────────────
        cout << "[SISTEMA] Aguardando finalização da Worker Thread de rede..." << endl;
        worker_active = false;
        cv_sleep.notify_all();
        if (fiware_thread.joinable()) {
            fiware_thread.join();
        }

    } catch (const std::exception& e) {
        cerr << "\n[ERRO CRÍTICO] Exceção capturada no main: " << e.what() << endl;
    } catch (...) {
        cerr << "\n[ERRO CRÍTICO] Exceção desconhecida capturada no main." << endl;
    }

    if (cap.isOpened()) {
        cap.release();
    }
    if (video_writer.isOpened()) {
        video_writer.release();
        cout << "[REC] Vídeo salvo com sucesso: " << video_filename << endl;
    }

    save_metrics(history, "metrics_output_" + session_ts + ".json");
    return 0;
}
