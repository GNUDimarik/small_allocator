cmake_minimum_required(VERSION 3.28)

set(BINARY allocator_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TEST_LOCATION "${PROJECT_SOURCE_DIR}/test")

include(ExternalProject)
ExternalProject_Add(googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG main
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TEST_LOCATION}
)

enable_testing()

add_executable(${BINARY} allocator_test.cpp free_list_test.cpp ../allocator.cpp)
target_include_directories(${BINARY} PUBLIC ${TEST_LOCATION}/include)
target_link_directories(${BINARY} PUBLIC ${TEST_LOCATION}/lib)

add_test(NAME ${BINARY} COMMAND ${BINARY})
target_link_libraries(${BINARY} PUBLIC c gtest pthread)
add_dependencies(${BINARY} googletest)
