cmake_minimum_required(VERSION 3.31)

project(mlspace VERSION 0.0.0 LANGUAGES CXX)

option(ENABLE_STATIC_STDLIB "Link statically with libstdc++ and/or libgcc." ON)
option(ENABLE_TESTS "Build tests or not." OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;MinSize;Release;RelWithDebInfo" CACHE
    STRING "Available build configurations" FORCE)

message(STATUS "Available build configurations: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_SCAN_FOR_MODULES FALSE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (ENABLE_STATIC_STDLIB)
    message(STATUS "Link statically")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

include(FetchContent)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH
        SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)

FetchContent_MakeAvailable(json)

add_subdirectory(mlspace)
