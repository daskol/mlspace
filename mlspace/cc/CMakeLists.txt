add_library(mlspace STATIC)

target_sources(mlspace
    PUBLIC
        base64.h
        cli.h
        job.h
    PRIVATE
        base64.cc
        cli.cc
        job.cc
)

target_include_directories(mlspace PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(mlspace PRIVATE nlohmann_json::nlohmann_json)

if (ENABLE_TESTS)
    find_package(GTest REQUIRED)

    add_executable(mlspace_cc_test base64_test.cc)

    target_include_directories(mlspace_cc_test PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(mlspace_cc_test PRIVATE GTest::gtest_main mlspace)
endif()
