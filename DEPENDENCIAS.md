# Como rodar este projeto em uma nova máquina

### 1. Requisitos do Sistema (Ubuntu/Debian/Raspberry Pi)
Abra o terminal e instale as ferramentas de compilação e o OpenCV:

```bash
sudo apt update
sudo apt install -build-essential cmake git libopencv-dev g++
```

### 2. Instalar o ncnn (Obrigatório)
O projeto depende da biblioteca **ncnn**. Você precisa compilá-la e instalá-la no sistema:

```bash
git clone https://github.com/Tencent/ncnn.git
cd ncnn
mkdir build && cd build
cmake -DNCNN_VULKAN=OFF -DNCNN_BUILD_EXAMPLES=OFF ..
make -j$(nproc)
sudo make install
```

### 3. Como Compilar este Projeto
Na pasta raiz deste código:

```bash
mkdir build
cd build
cmake ..
make -j4
```

### 4. Arquivos do Modelo
Certifique-se de que os arquivos `yolo-fastestv2-opt.param` e `yolo-fastestv2-opt.bin` estão na mesma pasta do executável `main` gerado.

### 5. Executar
```bash
./main
```
