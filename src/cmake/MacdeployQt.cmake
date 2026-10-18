# Macdeployqt.cmake
#
# Copyright (c) 2017 Samuel Gaist <samuel.gaist@edeltech.ch>
#

set(SPEEDCRUNCH_MACOS_DEPLOY_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/DeployMacOS.cmake")

function(macdeployqt target app_name)
    set(macdeployqt_signing_options)
    if(PORTABLE_SPEEDCRUNCH)
        set(macdeployqt_signing_options "-codesign=-")
    endif()
    install(CODE "
        set(APP_BUNDLE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${BIN_INSTALL_DIR}/${target}\")
        set(MACDEPLOYQT \"$<TARGET_FILE:Qt6::macdeployqt>\")
        set(MACDEPLOYQT_SIGNING_OPTIONS \"${macdeployqt_signing_options}\")
        include(\"${SPEEDCRUNCH_MACOS_DEPLOY_SCRIPT}\")
    ")
endfunction()
