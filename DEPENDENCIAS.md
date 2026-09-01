# Como rodar este projeto em uma nova máquina

### 1. Requisitos do Sistema (Ubuntu/Debian/Raspberry Pi)
Abra o terminal e instale as ferramentas de compilação, OpenCV, cURL e Mosquitto:

```bash
sudo apt update
sudo apt install -y build-essential cmake git libopencv-dev libcurl4-openssl-dev libmosquitto-dev g++
```

### 2. Instalar o ncnn (Obrigatório)
O projeto depende da biblioteca **ncnn**. Caso ainda não tenha instalado no sistema:

```bash
git clone https://github.com/Tencent/ncnn.git
cd ncnn
mkdir build && cd build
cmake -DNCNN_VULKAN=OFF -DNCNN_BUILD_EXAMPLES=OFF ..
make -j$(nproc)
sudo make install
```

### 3. Como Compilar este Projeto
Na pasta raiz deste repositório:

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### 4. Arquivos do Modelo e Configuração
Certifique-se de que os arquivos `yolo-fastestv2-opt.param`, `yolo-fastestv2-opt.bin` e `config.json` estão na pasta de execução.

### 5. Executar
```bash
./main
```
