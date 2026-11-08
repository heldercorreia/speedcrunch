# Parses the application version declared in the root CMakeLists.txt.
#
# SPEEDCRUNCH_VERSION holds the full SemVer 2.0.0 string, MAJOR.MINOR.PATCH with
# an optional pre-release suffix (build metadata is not supported). It is used
# wherever the version is displayed or names a package. Formats that only accept
# numbers use speedcrunch_version_core (MAJOR.MINOR.PATCH), which is also what
# project() receives.
#
# Outputs: SPEEDCRUNCH_VERSION, speedcrunch_version_core,
# speedcrunch_version_prerelease (empty for final releases).

get_filename_component(speedcrunch_root_dir "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(speedcrunch_top_source_dir "${CMAKE_SOURCE_DIR}" REALPATH)
if(NOT speedcrunch_top_source_dir STREQUAL speedcrunch_root_dir)
    # Direct src builds read the version declared at the repository root.
    set(speedcrunch_root_cmake "${speedcrunch_root_dir}/CMakeLists.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${speedcrunch_root_cmake}")
    file(STRINGS "${speedcrunch_root_cmake}" speedcrunch_version_declaration
        REGEX "^set\\(SPEEDCRUNCH_VERSION \"[^\"]*\"\\)")
    string(REGEX REPLACE "^set\\(SPEEDCRUNCH_VERSION \"([^\"]*)\"\\)$" "\\1"
        SPEEDCRUNCH_VERSION "${speedcrunch_version_declaration}")
endif()

set(speedcrunch_semver_number "(0|[1-9][0-9]*)")
set(speedcrunch_semver_identifier "(0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)")
set(speedcrunch_semver_core
    "${speedcrunch_semver_number}\\.${speedcrunch_semver_number}\\.${speedcrunch_semver_number}")
set(speedcrunch_semver_prerelease
    "${speedcrunch_semver_identifier}(\\.${speedcrunch_semver_identifier})*")
if(NOT SPEEDCRUNCH_VERSION MATCHES
        "^${speedcrunch_semver_core}(-${speedcrunch_semver_prerelease})?$")
    message(FATAL_ERROR "SPEEDCRUNCH_VERSION \"${SPEEDCRUNCH_VERSION}\" in the root CMakeLists.txt "
        "must be MAJOR.MINOR.PATCH with an optional SemVer pre-release, such as 1.2.0 or 1.2.0-beta.1")
endif()

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" speedcrunch_version_core "${SPEEDCRUNCH_VERSION}")
string(LENGTH "${speedcrunch_version_core}" speedcrunch_version_core_length)
string(SUBSTRING "${SPEEDCRUNCH_VERSION}" ${speedcrunch_version_core_length} -1
    speedcrunch_version_prerelease)
string(REGEX REPLACE "^-" "" speedcrunch_version_prerelease "${speedcrunch_version_prerelease}")
