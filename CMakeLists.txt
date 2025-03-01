cmake_minimum_required(VERSION 3.31)

project(mlspace VERSION 0.0.0 LANGUAGES CXX)

option(ENABLE_TESTS "Build tests or not." OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;MinSize;Release;RelWithDebInfo" CACHE
    STRING "Available build configurations" FORCE)

message(STATUS "Available build configurations: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(mlspace STATIC)

target_sources(mlspace
    PUBLIC
        base64.h
    PRIVATE
        base64.cc
)

if (ENABLE_TESTS)
    find_package(GTest REQUIRED)

    add_executable(mlspace_test base64_test.cc)

    target_link_libraries(mlspace_test PRIVATE GTest::gtest_main mlspace)
    target_include_directories(mlspace_test PRIVATE ${PROJECT_SOURCE_DIR})
endif()
