cmake_minimum_required(VERSION 3.10)
project(teste)

# Configurações de C++
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#----------------------------------------
# Buscando as ferramentas externas
#----------------------------------------

# 1. OpenCV
find_package(OpenCV REQUIRED)

# 2. ncnn (Busca inteligente)
find_package(ncnn REQUIRED)

# 3. OpenMP (Para processamento paralelo)
find_package(OpenMP REQUIRED)

# Inclui os diretórios de cabeçalho
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

#----------------------------------------
# Construindo o programa
#----------------------------------------

# Define o executável e seus fontes
add_executable(main main.cpp yolo-fastestv2.cpp)

# Linkagem inteligente (Sem caminhos absolutos)
target_link_libraries(main 
    ${OpenCV_LIBS} 
    ncnn 
    OpenMP::OpenMP_CXX
)
