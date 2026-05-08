cmake_minimum_required(VERSION 3.16)
project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(OpenCV REQUIRED)
find_package(CURL    REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    include   # json.hpp
    /usr/local/include/ncnn

)

add_executable(main
    main.cpp
    fiware/fiware_client.cpp
    metrics/metrics.cpp
    yolo-fastestv2.cpp
)

target_link_libraries(main
    ${OpenCV_LIBS}
    ${CURL_LIBRARIES}
    ncnn
    OpenMP::OpenMP_CXX
)