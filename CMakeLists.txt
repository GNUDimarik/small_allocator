cmake_minimum_required(VERSION 3.5)

project(c_allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(c_allocator main.cpp allocator.cpp allocator.h logging.h)
