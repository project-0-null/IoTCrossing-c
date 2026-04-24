#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono> 
#include "yolo-fastestv2.h"
//futuramente podemos fazer uma quantização em INT8, mas por enquanto vamos usar o modelo em FP16 mesmo, que é mais fácil de lidar e ainda tem um desempenho decente na CPU.
//atualmente estamos usando o .param e .bin padrâo (que calcula com casas decimais - FP32 ou FP16). o NCNN permite convertes esse motelo apra INT8, (numeros inteiros). a precisão cai cair para a velocidae pode aumentar ( tradeoff entre precisão e velocidade). 
//para usar o modelo quantizado, basta substituir os arquivos .param e .bin pelos arquivos quantizados (geralmente com sufixo _int8.param e _int8.bin) e garantir que o código de carregamento do modelo esteja apontando para esses arquivos. 
//a detecção em si não precisa ser alterada, pois o NCNN lida com a diferença de precisão internamente.

using namespace std;// Evita ter que escrever "std::" antes de cada comando do C++
using namespace cv;// Evita ter que escrever "cv::" antes de cada comando do OpenCV

void yolo_init(yoloFastestv2 &yolo){
    // (Certifique-se de que esses arquivos estão na mesma pasta do seu executável)
    yolo.loadModel("yolo-fastestv2-opt.param", "yolo-fastestv2-opt.bin");
    cout<<"modelo carregado com sucesso!"<<endl;
}



int main() {
    cout << "Sucesso! O programa C++ compilou e está vivo!" << endl;
    yoloFastestv2 yolo;
    yolo_init(yolo);

    VideoCapture cap(0); 
    cap.set(CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CAP_PROP_FRAME_HEIGHT, 240);

    //nao vi ncessidade de um try catch aqui, se a camera nao abrir, o programa ja vai avisar e fechar, entao nao tem risco de dar erro
    if (!cap.isOpened()) {
        cerr << "Erro: Nenhuma câmera encontrada no índice 0!" << endl;
        return -1;
    }

    cout << "Câmera ligada. Pressione 'Ctrl + C' para parar." << endl;

    Mat frame;
    int contador_frames = 0;
    
    // Inicia o cronômetro do C++
    auto tempo_inicio = chrono::steady_clock::now();

    while (true) {
        cap >> frame; 
        
        if (frame.empty()) break; 

        std::vector<TargetBox> caixas_encontradas;//cria caixas vazias para receber os resultados do yolo
        yolo.detection(frame, caixas_encontradas);//manda o opencv dar o frma pro yolo analisar

        int pessoas_detectadas = 0;
        for( auto caixa : caixas_encontradas){
            if(caixa.cate == 0 && caixa.score>0.4){pessoas_detectadas++;}//se pessoa e conficança maior que 40%
        }

        contador_frames++;
        
        // A cada 30 frames, calculamos o tempo que passou
        if (contador_frames % 30 == 0) {
            auto tempo_agora = chrono::steady_clock::now();
            
            // Descobre quantos milissegundos se passaram desde a última medição
            double tempo_passado_ms = chrono::duration_cast<chrono::milliseconds>(tempo_agora - tempo_inicio).count();
            
            // Regra de três para calcular o FPS
            double fps_real = (30.0 / tempo_passado_ms) * 1000.0;

            cout << "Lidos 30 frames. Velocidade da câmera: " << fps_real << " FPS (Na CPU)" << endl;
            cout << "Pessoas detectadas: " << pessoas_detectadas << endl;
            
            // Reseta o cronômetro para os próximos 30 frames
            tempo_inicio = chrono::steady_clock::now(); 
        }
    }

    cap.release();
    return 0; 
}

