QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_timeseriesdatazoom

INCLUDEPATH += $$PWD/..
HEADERS += ../timeseriesdatazoom.h
SOURCES += \
    tst_timeseriesdatazoom.cpp \
    ../timeseriesdatazoom.cpp
