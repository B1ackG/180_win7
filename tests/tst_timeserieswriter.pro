QT += core sql testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_timeserieswriter

INCLUDEPATH += $$PWD/..
HEADERS += ../timeserieswriter.h ../timeseriestypes.h
SOURCES += \
    tst_timeserieswriter.cpp \
    ../timeseriestypes.cpp \
    ../timeserieswriter.cpp
