cmake_minimum_required(VERSION 2.8.4)
project(CN_http_server)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(CN_http_server
        defs.h
        server.cpp)

target_link_libraries(CN_http_server ws2_32)
