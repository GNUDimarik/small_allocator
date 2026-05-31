cmake_minimum_required(VERSION 3.16)
project(malloc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
set(ENABLE_TEST true)

if (DEFINED ENABLE_TEST)
    cmake_policy(SET CMP0135 NEW)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(
            allocator_test
            allocator_test.cpp
            memory.cpp
            memory.h
            logging.h
    )

    target_link_libraries(
            allocator_test
            GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(allocator_test)
endif ()

add_executable(${PROJECT_NAME} main.cpp
        memory.cpp
        memory.h
        logging.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})