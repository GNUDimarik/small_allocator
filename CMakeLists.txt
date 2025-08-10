cmake_minimum_required(VERSION 3.5)

project(c_allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if (ENABLE_TESTS)
    add_subdirectory(test)
endif ()

add_executable(${CMAKE_PROJECT_NAME} main.cpp logging.h free_list.h
        _MemoryBlock.h
        _Allocator.h
        _Allocator.cpp
        Utils.cpp
        Utils.h
        defs.h
        _FreeListManager.cpp
        _FreeListManager.h)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})