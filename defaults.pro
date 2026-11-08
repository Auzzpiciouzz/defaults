QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = defaults
TEMPLATE = app

HEADERS += \
    src/desktopapps.h \
    src/mimeapps.h \
    src/associations.h \
    src/backend.h \
    src/cli.h \
    src/iconprovider.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/desktopapps.cpp \
    src/mimeapps.cpp \
    src/associations.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/iconprovider.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
