cmake_minimum_required(VERSION 3.16)
project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# ── Dependências Principais ──────────────────────────────────────────────────
find_package(OpenCV REQUIRED)
find_package(CURL   REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    include   # json.hpp
    /usr/local/include/ncnn
)

# ── Gerenciamento Automático da Biblioteca Mosquitto (MQTT) ──────────────────
# 1. Tenta encontrar a libmosquitto instalada no sistema operacional
find_library(MOSQUITTO_LIB mosquitto)
find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)

if(MOSQUITTO_LIB AND MOSQUITTO_INCLUDE_DIR)
    message(STATUS "[MQTT] Usando libmosquitto encontrada no sistema: ${MOSQUITTO_LIB}")
    include_directories(${MOSQUITTO_INCLUDE_DIR})
    set(MQTT_TARGET ${MOSQUITTO_LIB})
else()
    # 2. Se não estiver instalada, baixa e compila automaticamente via FetchContent do GitHub
    message(STATUS "[MQTT] libmosquitto não encontrada no sistema. Baixando e compilando do GitHub via FetchContent...")
    include(FetchContent)
    
    # Desativa broker, clientes CLI e plugins para compilar somente a biblioteca cliente leve
    set(WITH_BROKER OFF CACHE BOOL "" FORCE)
    set(WITH_APPS OFF CACHE BOOL "" FORCE)
    set(WITH_CLIENTS OFF CACHE BOOL "" FORCE)
    set(WITH_PLUGINS OFF CACHE BOOL "" FORCE)
    set(DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(WITH_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
    set(WITH_PIC ON CACHE BOOL "" FORCE)
    set(WITH_TLS OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        mosquitto
        GIT_REPOSITORY https://github.com/eclipse/mosquitto.git
        GIT_TAG        v2.0.18
        GIT_SHALLOW    TRUE
    )
    
    FetchContent_MakeAvailable(mosquitto)
    
    include_directories(${mosquitto_SOURCE_DIR}/include)
    set(MQTT_TARGET libmosquitto)
endif()

# ── Executável Principal ─────────────────────────────────────────────────────
add_executable(main
    main.cpp
    fiware/mqtt_client.cpp
    metrics/metrics.cpp
    yolo-fastestv2.cpp
)

target_link_libraries(main
    ${OpenCV_LIBS}
    ${CURL_LIBRARIES}
    ${MQTT_TARGET}
    ncnn
    OpenMP::OpenMP_CXX
)