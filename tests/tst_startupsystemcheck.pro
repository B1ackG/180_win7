QT += core widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_startupsystemcheck

INCLUDEPATH += $$PWD/..
SOURCES += tst_startupsystemcheck.cpp \
    ../startupsystemcheck.cpp \
    ../featureswitchmanager.cpp
HEADERS += ../startupsystemcheck.h \
    ../mainmodbusconnector.h \
    ../featureswitchmanager.h
