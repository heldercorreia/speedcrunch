cmake_minimum_required(VERSION 3.16)
project(speedcrunch)

# Put executables and app bundles in the top-level build directory by default.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

add_subdirectory(src)

# CTest looks for its entry point in the top-level build directory.
if(BUILD_TESTING)
    enable_testing()
endif()
