add_subdirectory(cc)

add_executable(launch)

target_sources(launch PRIVATE launch.cc)
target_include_directories(launch PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(launch PRIVATE mlspace)
