#!/bin/bash
# setup_dependencies.sh

echo "--- Instalando dependências básicas do sistema ---"
sudo apt update
sudo apt install -y build-essential cmake git libopencv-dev g++

echo "--- DICA IMPORTANTE ---"
echo "Este projeto precisa que a biblioteca NCNN esteja instalada."
echo "Se o CMake falhar dizendo que não achou o ncnn, siga os passos no arquivo DEPENDENCIAS.md"
echo "para compilar e instalar o ncnn via 'sudo make install'."
