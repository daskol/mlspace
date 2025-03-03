add_subdirectory(cc)

add_executable(launch)

target_sources(launch PRIVATE launch.cc)
target_include_directories(launch PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(launch PRIVATE mlspace)

# NOTE If scikit-learn-build run with experimental feature that allows
# installation to platforma directories, then we use it to install into package
# data during wheel builing.
#
# https://packaging.python.org/en/latest/specifications/binary-distribution-format/
if (DEFINED SKBUILD_DATA_DIR)
    install(TARGETS launch RUNTIME DESTINATION ${SKBUILD_DATA_DIR}/bin)
else()
    install(TARGETS launch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
