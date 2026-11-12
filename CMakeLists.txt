cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/src/cmake/BuildVariants.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/src/cmake/MacOSDeploymentTarget.cmake")
# Application version in SemVer 2.0.0 form: MAJOR.MINOR.PATCH with an optional
# pre-release suffix, such as 1.2.0 or 1.2.0-beta.1.
set(SPEEDCRUNCH_VERSION "1.2.0-rc.1")
include("${CMAKE_CURRENT_LIST_DIR}/src/cmake/SpeedCrunchVersion.cmake")
project(speedcrunch VERSION ${speedcrunch_version_core})

# Put executables and app bundles in the top-level build directory by default.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

add_subdirectory(src)

# CTest looks for its entry point in the top-level build directory.
if(BUILD_TESTING)
    enable_testing()
endif()
