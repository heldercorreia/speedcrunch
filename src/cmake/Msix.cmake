# Microsoft Store (MSIX) packaging for Windows.
#
# With SPEEDCRUNCH_MSIX=ON, the install tree also receives AppxManifest.xml and
# the Assets directory. The msix target installs into <build>/msix, indexes the
# assets with makepri and packs the directory with makeappx from the Windows SDK.
# The identity defaults match the product reserved in Partner Center.

option(SPEEDCRUNCH_MSIX "Install an MSIX manifest and assets and add the msix target" OFF)

if(NOT SPEEDCRUNCH_MSIX)
    return()
endif()
if(NOT WIN32)
    message(WARNING "SPEEDCRUNCH_MSIX only applies to Windows builds; ignoring it")
    return()
endif()

# Lets the About box and the diagnostics report name the Store build.
target_compile_definitions(${APPLICATION_NAME} PRIVATE SPEEDCRUNCH_MICROSOFT_STORE)

set(SPEEDCRUNCH_MSIX_IDENTITY_NAME "HelderCorreia.SpeedCrunch" CACHE STRING
    "MSIX Package/Identity/Name from Partner Center")
set(SPEEDCRUNCH_MSIX_PUBLISHER "CN=9777F554-01AD-423E-A920-2267606C7032" CACHE STRING
    "MSIX Package/Identity/Publisher from Partner Center")
set(SPEEDCRUNCH_MSIX_PUBLISHER_DISPLAY_NAME "Helder Correia" CACHE STRING
    "MSIX Package/Properties/PublisherDisplayName from Partner Center")
# MSIX versions are four numbers and the Store requires the last one to be 0.
# Only final releases go to the Store, so a pre-release package is for local
# testing: it would take the version number its final release needs.
set(speedcrunch_msix_version "${speedcrunch_version_core}.0")
if(speedcrunch_version_prerelease)
    message(WARNING "SpeedCrunch ${SPEEDCRUNCH_VERSION} is a pre-release. Its MSIX package "
        "is versioned ${speedcrunch_msix_version}, so do not submit it to the Store.")
endif()

if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(ARM64|arm64|aarch64)$"
        OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(speedcrunch_msix_arch arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(speedcrunch_msix_arch x64)
else()
    message(FATAL_ERROR "MSIX packages are only built for x64 and arm64")
endif()

message(STATUS "MSIX identity: ${SPEEDCRUNCH_MSIX_IDENTITY_NAME} ${speedcrunch_msix_version} ${speedcrunch_msix_arch}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/msix/AppxManifest.xml.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml" DESTINATION "${BIN_INSTALL_DIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/msix/Assets" DESTINATION "${BIN_INSTALL_DIR}")

# makeappx is on PATH in a Visual Studio developer prompt; otherwise look in the SDK.
set(speedcrunch_makeappx_hints)
if(CMAKE_WINDOWS_KITS_10_DIR AND CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    list(APPEND speedcrunch_makeappx_hints
        "${CMAKE_WINDOWS_KITS_10_DIR}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
endif()
file(GLOB speedcrunch_sdk_bin_dirs LIST_DIRECTORIES true
    "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/10.*/x64")
list(SORT speedcrunch_sdk_bin_dirs COMPARE NATURAL ORDER DESCENDING)
list(APPEND speedcrunch_makeappx_hints ${speedcrunch_sdk_bin_dirs})
find_program(SPEEDCRUNCH_MAKEAPPX makeappx HINTS ${speedcrunch_makeappx_hints})
find_program(SPEEDCRUNCH_MAKEPRI makepri HINTS ${speedcrunch_makeappx_hints})
if(TARGET Qt6::windeployqt)
    set(speedcrunch_windeployqt "$<TARGET_FILE:Qt6::windeployqt>")
else()
    find_program(SPEEDCRUNCH_WINDEPLOYQT windeployqt HINTS "${qtwin32_QT_LIBDIR}")
    set(speedcrunch_windeployqt "${SPEEDCRUNCH_WINDEPLOYQT}")
endif()

set(speedcrunch_msix_staging "${CMAKE_BINARY_DIR}/msix")
set(speedcrunch_msix_file
    "${CMAKE_BINARY_DIR}/SpeedCrunch-${SPEEDCRUNCH_VERSION}-${speedcrunch_msix_arch}.msix")
set(speedcrunch_priconfig "${CMAKE_CURRENT_SOURCE_DIR}/resources/msix/priconfig.xml")
# Index a copy of the logos alone; the license file names otherwise read as
# invalid resource qualifiers.
set(speedcrunch_msix_pri_root "${CMAKE_BINARY_DIR}/msix-pri")
if(SPEEDCRUNCH_MAKEAPPX AND SPEEDCRUNCH_MAKEPRI AND speedcrunch_windeployqt)
    # windeployqt uses the installer workflow options, and skips the DirectX
    # shader compiler it would otherwise copy from the Windows SDK, as the
    # installer does not ship it.
    # resources.pri lets Windows pick the scale- and targetsize-qualified logos.
    add_custom_target(msix
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${speedcrunch_msix_staging}"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --config $<CONFIG>
                --prefix "${speedcrunch_msix_staging}"
        COMMAND "${speedcrunch_windeployqt}" --release --no-compiler-runtime --no-translations
                --skip-plugin-types generic,networkinformation,imageformats
                --exclude-plugins qsqlodbc --no-system-dxc-compiler
                "${speedcrunch_msix_staging}/${BIN_INSTALL_DIR}/${APPLICATION_NAME}.exe"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${speedcrunch_msix_pri_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/msix/Assets"
                "${speedcrunch_msix_pri_root}/Assets"
        COMMAND "${SPEEDCRUNCH_MAKEPRI}" new /o /pr "${speedcrunch_msix_pri_root}"
                /cf "${speedcrunch_priconfig}"
                /mn "${speedcrunch_msix_staging}/AppxManifest.xml"
                /of "${speedcrunch_msix_staging}/resources.pri"
        COMMAND "${SPEEDCRUNCH_MAKEAPPX}" pack /o /d "${speedcrunch_msix_staging}"
                /p "${speedcrunch_msix_file}"
        DEPENDS ${APPLICATION_NAME}
        COMMENT "Packing ${speedcrunch_msix_file}"
        VERBATIM)
else()
    add_custom_target(msix
        COMMAND "${CMAKE_COMMAND}" -E echo
                "windeployqt, makeappx or makepri was not found; install the Windows SDK or set SPEEDCRUNCH_WINDEPLOYQT, SPEEDCRUNCH_MAKEAPPX and SPEEDCRUNCH_MAKEPRI"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM)
endif()
