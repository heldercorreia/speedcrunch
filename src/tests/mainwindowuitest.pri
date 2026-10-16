include(common.pri)
QT += testlib network
CONFIG += c++17
DEFINES += SPEEDCRUNCH_UI_TEST
contains(CONFIG, enable_update_checks) {
    DEFINES += SPEEDCRUNCH_ENABLE_UPDATE_CHECKS
    HEADERS += ../gui/versioncheck.h
    SOURCES += ../gui/versioncheck.cpp
}

SOURCES += ../gui/menustyleutils.cpp \
           ../core/userdefinitions.cpp \
           ../gui/aboutbox.cpp \
           ../gui/diagnosticsdialog.cpp \
           ../gui/bitfieldwidget.cpp \
           ../gui/bookdock.cpp \
           ../gui/constantswidget.cpp \
           ../gui/customkeypaddialog.cpp \
           ../gui/dockcomboboxchevron.cpp \
           ../gui/dockliststyle.cpp \
           ../gui/editor.cpp \
           ../gui/functionswidget.cpp \
           ../gui/historywidget.cpp \
           ../gui/keypad.cpp \
           ../gui/mainwindow.cpp \
           ../gui/numberformatdialog.cpp \
           ../gui/notationandprecisiondialog.cpp \
           ../gui/oklchutils.cpp \
           ../gui/resultdisplay.cpp \
           ../gui/splittertreeutils.cpp \
           ../gui/syntaxhighlighter.cpp \
           ../gui/themedlineedit.cpp \
           ../gui/userfunctionlistwidget.cpp \
           ../gui/userunitlistwidget.cpp \
           mainwindowtesthelpers.cpp

HEADERS += ../core/userdefinitions.h \
           ../gui/aboutbox.h \
           ../gui/diagnosticsdialog.h \
           ../gui/diagnosticapplication.h \
           ../core/startupdiagnostics.h \
           ../gui/bitfieldwidget.h \
           ../gui/bookdock.h \
           ../gui/constantbrowserutils.h \
           ../gui/constantswidget.h \
           ../gui/customkeypaddialog.h \
           ../gui/dockcomboboxchevron.h \
           ../gui/dockliststyle.h \
           ../gui/editor.h \
           ../gui/functionswidget.h \
           ../gui/historywidget.h \
           ../gui/keypad.h \
           ../gui/lineeditstyleutils.h \
           ../gui/mainwindow.h \
           ../gui/numberformatdialog.h \
           ../gui/notationandprecisiondialog.h \
           ../gui/oklchutils.h \
           ../gui/resultdisplay.h \
           ../gui/splittertreeutils.h \
           ../gui/syntaxhighlighter.h \
           ../gui/themedlineedit.h \
           ../gui/userfunctionlistwidget.h \
           ../gui/userunitlistwidget.h

RESOURCES += ../resources/speedcrunch.qrc

HEADERS += mainwindowtesthelpers.h
