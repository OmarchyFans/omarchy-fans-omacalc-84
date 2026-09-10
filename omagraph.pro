QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = omagraph
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/engine.h \
    src/graphview.h \
    src/special.h \
    src/stats.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/engine.cpp \
    src/graphview.cpp \
    src/special.cpp \
    src/stats.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
