#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <csignal>
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

int main() {
    signal(SIGINT, signalHandler);

    // ── Configuração do broker ────────────────────────────────────────────
    FiwareConfig cfg;
    cfg.broker_url  = "http://127.0.0.1:31330";// Porta correta NodePort
    cfg.entity_id   = "urn:ngsi-ld:ItemFlowObserved:1";
    cfg.entity_name = "RaspberryPi_3";
    cfg.latitude    = -20.272594;
    cfg.longitude   = -40.306346;
    cfg.interval_s  = 10; // 10 segundos para teste


    FiwareClient fiware(cfg);
    fiware.init_entity(); // POST inicial

    // ── YOLO + câmera ─────────────────────────────────────────────────────
    yoloFastestv2 yolo;
    yolo_init(yolo);

    VideoCapture cap(0, CAP_V4L2);
    cap.set(CAP_PROP_FRAME_WIDTH,  320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);

    if (!cap.isOpened()) {
        cerr << "[CAM] Câmera não encontrada." << endl;
        return -1;
    }

    // ── Loop principal ────────────────────────────────────────────────────
    vector<DetectionMetrics> history;
    int  contador  = 0;
    auto ultimo_envio = chrono::steady_clock::now();

    while (run_loop) {
        auto frame_inicio = chrono::steady_clock::now();

        Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        vector<TargetBox> caixas;
        auto inf_inicio = chrono::steady_clock::now();
        yolo.detection(frame, caixas);
        auto inf_fim = chrono::steady_clock::now();

        double inf_ms = chrono::duration_cast<chrono::microseconds>(inf_fim - inf_inicio).count() / 1000.0;

        int pessoas = 0;
        for (const auto& c : caixas)
            if (c.cate == 0 && c.score > 0.4f) pessoas++;

        double frame_ms = chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now() - frame_inicio).count() / 1000.0;

        history.push_back({inf_ms, 1000.0 / frame_ms, pessoas});
        contador++;

        if (contador % 30 == 0)
            cout << "[Frame " << contador << "] " << inf_ms << "ms | "
                 << 1000.0 / frame_ms << " fps | Pessoas: " << pessoas << endl;

        // ── Envio periódico ───────────────────────────────────────────────
        auto agora    = chrono::steady_clock::now();
        auto segundos = chrono::duration_cast<chrono::seconds>(agora - ultimo_envio).count();
        if (segundos >= cfg.interval_s) {
            fiware.update(pessoas);
            ultimo_envio = agora;
        }
    }

    cap.release();
    save_metrics(history);
    return 0;
}