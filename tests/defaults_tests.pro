QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = defaults_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/desktopapps.h ../src/mimeapps.h ../src/associations.h ../src/backend.h \
           ../src/theme.h ../src/translator.h
SOURCES += defaults_tests.cpp ../src/desktopapps.cpp ../src/mimeapps.cpp ../src/associations.cpp \
           ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
DEFINES += SRCDIR=\\\"$$PWD\\\"
