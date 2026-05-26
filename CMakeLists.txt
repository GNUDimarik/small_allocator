cmake_minimum_required(VERSION 3.16)
project(malloc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

add_executable(${PROJECT_NAME} main.cpp
        memory.cpp
        memory.h
        logging.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})