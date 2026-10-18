cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/BuildVariants.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/MacOSDeploymentTarget.cmake")
project(speedcrunch)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message("Using C++${CMAKE_CXX_STANDARD}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
message("Using C${CMAKE_C_STANDARD}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

option(BUILD_TESTING "Build the test executables and enable CTest" OFF)
option(ENABLE_UPDATE_CHECKS "Enable launch-time and manual update checks" OFF)
option(ENABLE_STARTUP_DIAGNOSTICS "Record startup timings and detailed appearance diagnostics" OFF)
if(ENABLE_UPDATE_CHECKS)
    add_compile_definitions(SPEEDCRUNCH_ENABLE_UPDATE_CHECKS)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Help Network LinguistTools)
message(STATUS "Found Qt version: ${Qt6_VERSION}")

if(BUILD_TESTING)
    find_package(Qt6 COMPONENTS Test REQUIRED)
endif()

if (MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
    # MSVC's /Wall also enables normally disabled padding and inlining reports.
    add_compile_options(/W4)
    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        # MSBuild parallelizes projects. /MP also parallelizes files within each project.
        add_compile_options(
            "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MP>"
            "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MP>"
        )
    endif()
else()
    add_compile_options(-Wno-shift-negative-value -Wall -pedantic)
endif()

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(speedcrunch_VERSION "1.0")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MACOSX_RPATH ON)

ADD_DEFINITIONS("-DSPEEDCRUNCH_VERSION=\"${speedcrunch_VERSION}\"")
ADD_DEFINITIONS(-DQT_USE_QSTRINGBUILDER)

include(SourceFiles.cmake)

if(BUILD_TESTING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(tests/testfloatnum.c PROPERTIES
        COMPILE_OPTIONS "-Wno-strict-prototypes")
endif()

if(APPLE)
    set(APP_ICON_FILE speedcrunch.icns)
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/${APP_ICON_FILE}")
    set(APPLICATION_NAME SpeedCrunch)
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
    set(MACOSX_BUNDLE_ICON_FILE ${APP_ICON_FILE})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${speedcrunch_VERSION})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${speedcrunch_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING Version ${speedcrunch_VERSION})
    set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
elseif(HAIKU)
    set(APPLICATION_NAME SpeedCrunch)
else()
    set(APPLICATION_NAME speedcrunch)
endif()

if(WIN32)
    set(WIN32_RES_FILE ${CMAKE_CURRENT_BINARY_DIR}/speedcrunch.rc.obj)
    if(MINGW)
        add_custom_command(OUTPUT ${WIN32_RES_FILE}
                           COMMAND windres.exe ${CMAKE_CURRENT_SOURCE_DIR}/resources/speedcrunch.rc ${WIN32_RES_FILE})
    else()
        add_custom_command(OUTPUT ${WIN32_RES_FILE}
                           COMMAND rc.exe /fo ${WIN32_RES_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/resources/speedcrunch.rc)
    endif()
endif()

# Embedded manual
set(HTML_DOCS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../doc/build_html_embedded"
    CACHE PATH
    "Path to the HTML manual. Defaults to the bundled prebuilt copy.")
set(speedcrunch_RESOURCES ${speedcrunch_RESOURCES} ${HTML_DOCS_DIR}/manual.qrc)

if(WIN32)
    add_executable(${APPLICATION_NAME} WIN32 ${speedcrunch_SOURCES} ${speedcrunch_RESOURCES} ${WIN32_RES_FILE})
elseif(APPLE)
    add_executable(${APPLICATION_NAME} MACOSX_BUNDLE ${speedcrunch_SOURCES} ${speedcrunch_RESOURCES} ${APP_ICON})
    set_target_properties(${APPLICATION_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${MACOSX_BUNDLE_INFO_PLIST}")
else()
    add_executable(${APPLICATION_NAME} ${speedcrunch_SOURCES} ${speedcrunch_RESOURCES})
endif()

# lupdate needs an explicit source include path to resolve translation contexts.
target_include_directories(${APPLICATION_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(APPLE)
    if(NOT CMAKE_STRIP)
        message(FATAL_ERROR "The macOS toolchain must provide CMAKE_STRIP for Release builds")
    endif()
    # Native strip preserves the symbols needed by the dynamic loader.
    add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
        COMMAND "$<$<CONFIG:Release>:${CMAKE_STRIP}>"
                "$<$<CONFIG:Release>:$<TARGET_FILE:${APPLICATION_NAME}>>"
        VERBATIM
    )
elseif(MSVC)
    target_link_options(${APPLICATION_NAME} PRIVATE "$<$<CONFIG:Release>:/DEBUG:NONE>")
else()
    target_link_options(${APPLICATION_NAME} PRIVATE "$<$<CONFIG:Release>:LINKER:-s>")
endif()

file(GLOB speedcrunch_TS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/locale/*.ts")
qt_add_translations(${APPLICATION_NAME}
    TS_FILES ${speedcrunch_TS_SOURCES}
    LUPDATE_OPTIONS -no-obsolete
    QM_FILES_OUTPUT_VARIABLE speedcrunch_QM_FILES
)
set(speedcrunch_TRANSLATION_TARGETS ${APPLICATION_NAME})

target_compile_definitions(${APPLICATION_NAME}
    PRIVATE
        $<$<BOOL:${ENABLE_STARTUP_DIAGNOSTICS}>:SPEEDCRUNCH_ENABLE_STARTUP_DIAGNOSTICS>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_INFO_OUTPUT>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_WARNING_OUTPUT>
)

add_custom_target(confclean
    COMMAND rm -rf Makefile CMakeFiles/ CMakeCache.txt cmake_install.cmake DartTestfile.txt install_manifest.txt
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

set(QT_LIBRARIES Qt6::Widgets Qt6::Help Qt6::Network Qt6::Core)
target_link_libraries(${APPLICATION_NAME} PRIVATE ${QT_LIBRARIES})

if(BUILD_TESTING)
    enable_testing()

    qt_add_executable(testtranslations tests/testtranslations.cpp)
    target_link_libraries(testtranslations PRIVATE Qt6::Test)
    target_compile_definitions(testtranslations PRIVATE
        SPEEDCRUNCH_TS_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/resources/locale")
    add_test(NAME testtranslations COMMAND testtranslations)
    list(APPEND speedcrunch_TRANSLATION_TARGETS testtranslations)

    add_executable(testhmath ${testhmath_SOURCES})
    target_link_libraries(testhmath ${QT_LIBRARIES})
    add_test(NAME testhmath COMMAND testhmath)

    add_executable(testevaluator ${testevaluator_SOURCES} ${testevaluator_HEADERS_MOC})
    target_link_libraries(testevaluator ${QT_LIBRARIES})
    add_test(NAME testevaluator COMMAND testevaluator)

    add_executable(testfloatnum ${testfloatnum_SOURCES})
    set_property(TARGET testfloatnum APPEND PROPERTY COMPILE_DEFINITIONS _FLOATNUMTEST)
    add_test(NAME testfloatnum COMMAND testfloatnum)

    add_executable(testcmath ${testcmath_SOURCES})
    target_link_libraries(testcmath ${QT_LIBRARIES})
    add_test(NAME testcmath COMMAND testcmath)

    add_executable(testdmath ${testdmath_SOURCES})
    target_link_libraries(testdmath ${QT_LIBRARIES})
    add_test(NAME testdmath COMMAND testdmath)

    add_executable(testserial ${testserial_SOURCES})
    target_link_libraries(testserial ${QT_LIBRARIES})
    add_test(NAME testserial COMMAND testserial)

    add_executable(testeditorui ${testeditorui_SOURCES})
    target_link_libraries(testeditorui ${QT_LIBRARIES} Qt6::Test)
    add_test(NAME testeditorui COMMAND testeditorui)

    add_executable(testoklch ${testoklch_SOURCES})
    target_link_libraries(testoklch ${QT_LIBRARIES} Qt6::Test)
    add_test(NAME testoklch COMMAND testoklch)

    add_executable(testdocksui ${testdocksui_SOURCES})
    target_link_libraries(testdocksui ${QT_LIBRARIES} Qt6::Test)
    add_test(NAME testdocksui COMMAND testdocksui)

    # Compile the application code and resources once for the component suites.
    # An object library also keeps the Qt resource objects in every executable.
    add_library(mainwindowuitest OBJECT ${mainwindowuitest_SOURCES})
    target_link_libraries(mainwindowuitest PUBLIC ${QT_LIBRARIES} Qt6::Test)
    target_compile_definitions(mainwindowuitest PUBLIC
        SPEEDCRUNCH_UI_TEST
        SPEEDCRUNCH_ENABLE_STARTUP_DIAGNOSTICS
    )
    # Start the longer suites first on a build without CTest timing history.
    set(mainwindow_ui_tests
        testfocusui
        testdockappearanceui
        testdocklayoutui
        testprivatesessionsui
        testthemesui
        testkeypadui
        testeditorappearanceui
        testsettingsui
        teststatusbarui
        testmenusui
        testbitfieldui
        testsessionsui
        testdisplayui
    )
    foreach(ui_test IN LISTS mainwindow_ui_tests)
        add_executable(${ui_test} tests/${ui_test}.cpp)
        target_link_libraries(${ui_test} PRIVATE mainwindowuitest)
        add_test(NAME ${ui_test} COMMAND ${ui_test})
        set_tests_properties(${ui_test} PROPERTIES LABELS "ui")
    endforeach()
    foreach(ui_test IN ITEMS testeditorui testdocksui)
        target_compile_definitions(${ui_test} PRIVATE SPEEDCRUNCH_UI_TEST)
        set_tests_properties(${ui_test} PROPERTIES LABELS "ui")
    endforeach()

    if(WIN32)
        get_property(speedcrunch_tests DIRECTORY PROPERTY TESTS)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.22")
            set_property(TEST ${speedcrunch_tests} APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
        else()
            # Older CTest versions require the full PATH as one escaped list entry.
            string(REPLACE ";" "\\;" speedcrunch_test_path "$ENV{PATH}")
            set_property(TEST ${speedcrunch_tests} APPEND PROPERTY ENVIRONMENT
                "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;${speedcrunch_test_path}")
        endif()
    endif()
endif()

# Embed the same generated catalogs in the application and test executables.
foreach(translation_target IN LISTS speedcrunch_TRANSLATION_TARGETS)
    qt_add_resources(${translation_target} "${translation_target}_translations"
        PREFIX "/locale"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${speedcrunch_QM_FILES}
    )
endforeach()

include_directories("${CMAKE_CURRENT_BINARY_DIR}" thirdparty core gui math)

################################# INSTALL ######################################

if(UNIX AND NOT APPLE AND NOT HAIKU)
    include(GNUInstallDirs)
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
    set(DOC_INSTALL_DIR "${CMAKE_INSTALL_DOCDIR}")

    set(MENU_DIR "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    set(APP_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
    set(ICON_DIR "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor")
elseif(APPLE)
    set(BIN_INSTALL_DIR ".")
    set(DOC_INSTALL_DIR ${APPLICATION_NAME}.app/Contents/Resources)
elseif(HAIKU)
    set(BIN_INSTALL_DIR "apps")
    set(DOC_INSTALL_DIR "documentation/SpeedCrunch")
else()
    set(BIN_INSTALL_DIR ".")
    set(DOC_INSTALL_DIR ".")
endif()

option(PORTABLE_SPEEDCRUNCH "Enable to build in portable mode" OFF)
if(PORTABLE_SPEEDCRUNCH)
    add_definitions(-DSPEEDCRUNCH_PORTABLE)
endif(PORTABLE_SPEEDCRUNCH)

install(TARGETS ${APPLICATION_NAME} DESTINATION "${BIN_INSTALL_DIR}")

if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES ../pkg/org.speedcrunch.SpeedCrunch.desktop DESTINATION "${MENU_DIR}")
    install(FILES ../gfx/speedcrunch.svg
        DESTINATION "${ICON_DIR}/scalable/apps"
        RENAME org.speedcrunch.SpeedCrunch.svg)
    foreach(ICON_SIZE IN ITEMS 16 22 24 32 48 64 128 256)
        install(FILES "../gfx/icons/${ICON_SIZE}x${ICON_SIZE}/org.speedcrunch.SpeedCrunch.png"
            DESTINATION "${ICON_DIR}/${ICON_SIZE}x${ICON_SIZE}/apps")
    endforeach()
    install(FILES ../pkg/org.speedcrunch.SpeedCrunch.metainfo.xml DESTINATION "${APP_DATA_DIR}")
elseif(WIN32)
    install(FILES ../pkg/COPYING.rtf DESTINATION ${DOC_INSTALL_DIR})
endif()

if(WIN32)
    include(QtWin32Deploy)
endif()

include(InstallLicenses)

if(WIN32)
    qtwin32_deploy_modules(${BIN_INSTALL_DIR} qtwin32_Qt6Widgets qtwin32_Qt6Help qtwin32_Qt6Network)
    qtwin32_deploy_default_qt_conf(${BIN_INSTALL_DIR})
elseif(APPLE)
    include(MacdeployQt)
    macdeployqt(${APPLICATION_NAME}.app ${APPLICATION_NAME})
endif(WIN32)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${BIN_INSTALL_DIR})
include(InstallRequiredSystemLibraries)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

include(SpeedCrunchCPack.cmake)

################################ UNINSTALL #####################################

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
                IMMEDIATE @ONLY)

add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
