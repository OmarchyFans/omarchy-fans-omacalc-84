QT += core gui testlib dbus
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omagraph

INCLUDEPATH += ../src
SOURCES += \
    tst_omagraph.cpp \
    ../src/backend.cpp \
    ../src/engine.cpp \
    ../src/special.cpp \
    ../src/stats.cpp
HEADERS += \
    ../src/backend.h \
    ../src/engine.h \
    ../src/special.h \
    ../src/stats.h
