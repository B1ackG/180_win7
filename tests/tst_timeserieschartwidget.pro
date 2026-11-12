QT += core gui widgets sql concurrent testlib
CONFIG += console c++17 testcase
TEMPLATE = app
TARGET = tst_timeserieschartwidget

INCLUDEPATH += $$PWD/..
HEADERS += \
    ../timeserieschartwidget.h \
    ../timeseriesdatazoom.h \
    ../timeseriesdialog.h \
    ../timeseriesfullscreendialog.h \
    ../timeseriesconditiondialog.h \
    ../timeseriestypes.h \
    ../timeseriesqueryservice.h \
    ../historycalendarpanel.h
SOURCES += \
    tst_timeserieschartwidget.cpp \
    ../timeserieschartwidget.cpp \
    ../timeseriesdatazoom.cpp \
    ../timeseriesdialog.cpp \
    ../timeseriesfullscreendialog.cpp \
    ../timeseriesconditiondialog.cpp \
    ../timeseriestypes.cpp \
    ../timeseriesqueryservice.cpp \
    ../historycalendarpanel.cpp
