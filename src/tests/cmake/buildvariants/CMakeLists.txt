cmake_minimum_required(VERSION 3.16)

if(NOT TEST_BUILD_VARIANT STREQUAL "DEFAULT")
    set("BUILD_VARIANT_${TEST_BUILD_VARIANT}" ON CACHE BOOL "Build variant under test" FORCE)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/BuildVariants.cmake")
project(testbuildvariants NONE)

if(NOT BUILD_TESTING STREQUAL EXPECTED_BUILD_TESTING)
    message(FATAL_ERROR
        "${TEST_BUILD_VARIANT}: expected BUILD_TESTING=${EXPECTED_BUILD_TESTING}, got ${BUILD_TESTING}")
endif()
