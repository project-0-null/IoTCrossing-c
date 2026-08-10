#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <csignal>
#include <iomanip>
#include <ctime>
#include "yolo-fastestv2.h"
#include "metrics/metrics.h"
#include "fiware/fiware_client.h"


using namespace std;
using namespace cv;

bool run_loop = true;
void signalHandler(int) {
    cout << "\nFinalizando e salvando dados..." << endl;
    run_loop = false;
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

int main() {
    signal(SIGINT, signalHandler);

    // ── Configuração do broker (carregado via config.json) ───────────────
    FiwareConfig cfg = FiwareConfig::load_from_file("config.json");

    std::string session_ts = get_session_timestamp();
    FiwareClient fiware(cfg, session_ts);
    fiware.init_entity(); // POST inicial

    // ── YOLO + câmera ─────────────────────────────────────────────────────
    yoloFastestv2 yolo;
    yolo_init(yolo);

    VideoCapture cap(0, CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "[CAM] Câmera 0 não encontrada. Tentando câmera 1..." << endl;
        cap.open(1, CAP_V4L2);
    }

    if (!cap.isOpened()) {
        cerr << "[CAM] Erro FATAL: Nenhuma câmera encontrada." << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH,  640);//alterar via necessidade, atualmente usando (640x480)
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Espera a câmera estabilizar (warm-up)
    cout << "[CAM] Aquecendo a câmera..." << endl;
    for(int i = 0; i < 10; i++) {
        Mat temp;
        cap >> temp;
    }

    // ── Configuração do Gravador de Vídeo (.mp4) ──────────────────────────
    std::string video_filename = "video_output_" + session_ts + ".mp4";
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    double video_fps = 10.0; // FPS otimizado para vídeos longos
    cv::Size frame_size(640, 480);

    cv::VideoWriter video_writer(video_filename, fourcc, video_fps, frame_size);
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

    // ── Loop principal ────────────────────────────────────────────────────
    vector<DetectionMetrics> history;
    int  contador  = 0;
    auto ultimo_envio = chrono::steady_clock::now();

    while (run_loop) {
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
                // Desenhar retângulo ao redor da pessoa detectada
                cv::rectangle(frame, cv::Point(c.x1, c.y1), cv::Point(c.x2, c.y2), cv::Scalar(0, 255, 0), 2);
                
                // Rótulo com a confiança
                std::string label = "Pessoa " + std::to_string((int)(c.score * 100)) + "%";
                cv::putText(frame, label, cv::Point(c.x1, std::max(15, c.y1 - 5)),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            }
        }

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

        // ── Envio periódico ───────────────────────────────────────────────
        auto agora    = chrono::steady_clock::now();
        auto segundos = chrono::duration_cast<chrono::seconds>(agora - ultimo_envio).count();
        if (segundos >= cfg.interval_s) {
            fiware.update(pessoas);
            ultimo_envio = agora;
        }
    }

    cap.release();
    if (video_writer.isOpened()) {
        video_writer.release();
        cout << "[REC] Vídeo salvo com sucesso: " << video_filename << endl;
    }

    save_metrics(history, "metrics_output_" + session_ts + ".json");
    return 0;
}

