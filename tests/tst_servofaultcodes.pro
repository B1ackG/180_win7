QT += core testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_servofaultcodes

INCLUDEPATH += $$PWD/..
SOURCES += tst_servofaultcodes.cpp ../servofaultcodes.cpp
HEADERS += ../servofaultcodes.h
