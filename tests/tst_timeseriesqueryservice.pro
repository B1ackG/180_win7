QT += core sql testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_timeseriesqueryservice

INCLUDEPATH += $$PWD/..
HEADERS += ../timeserieswriter.h ../timeseriestypes.h ../timeseriesqueryservice.h
SOURCES += \
    tst_timeseriesqueryservice.cpp \
    ../timeseriestypes.cpp \
    ../timeserieswriter.cpp \
    ../timeseriesqueryservice.cpp
