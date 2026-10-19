QT += core testlib network
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_historyarchiveio

INCLUDEPATH += $$PWD/..
SOURCES += tst_historyarchiveio.cpp ../historyarchiveio.cpp
