project(mqtt-inspector)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)

file(GLOB SOURCES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC  
    paho-mqtt3c fmt pthread uuid)

