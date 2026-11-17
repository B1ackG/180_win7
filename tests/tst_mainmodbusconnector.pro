QT += core testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_mainmodbusconnector

INCLUDEPATH += $$PWD/..
SOURCES += tst_mainmodbusconnector.cpp
HEADERS += ../mainmodbusconnector.h
