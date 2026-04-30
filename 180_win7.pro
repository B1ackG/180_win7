QT       += core gui widgets network

CONFIG   += c++17
TEMPLATE = app
TARGET   = 180_win7

SOURCES += \
    agvmodbusmanager.cpp \
    animationmanager.cpp \
    batterywidget.cpp \
    enablebuttonworker.cpp \
    featureswitchmanager.cpp \
    featureswitchwidget.cpp \
    main.cpp \
    maindevicemodbusapi.cpp \
    mainmodbusconnector.cpp \
    mainmodbuslabelmapper.cpp \
    mainmodbuspoller.cpp \
    mainmodbusstatus.cpp \
    mainwindow.cpp \
    mainwindow_lifecycle.cpp \
    matrixkeymonitor.cpp \
    matrixkeythreadmanager.cpp \
    mappingconfig.cpp \
    modebuttonstyler.cpp \
    modbustcpclient.cpp \
    modbusthreadmanager.cpp \
    modbusvariables.cpp \
    operationrecorder.cpp \
    poseprovider.cpp \
    robottotalpowercard.cpp \
    speedmodeselector.cpp \
    steeringmodeselector.cpp \
    techarcgauge.cpp \
    techpushbutton.cpp \
    techslideredit.cpp \
    techsliderlabel.cpp \
    techspeedgauge.cpp \
    techvirtualkeyboard.cpp

HEADERS += \
    agvmodbusmanager.h \
    animationmanager.h \
    batterywidget.h \
    debug.h \
    enablebuttonworker.h \
    featureswitchmanager.h \
    featureswitchwidget.h \
    maindevicemodbusapi.h \
    mainmodbusconnector.h \
    matrixkeymonitor.h \
    matrixkeythreadmanager.h \
    mainmodbuslabelmapper.h \
    mainmodbuspoller.h \
    mainmodbusstatus.h \
    mainwindow.h \
    mappingconfig.h \
    modebuttonstyler.h \
    modbustcpclient.h \
    modbusthreadmanager.h \
    modbusvariables.h \
    operationrecorder.h \
    poseprovider.h \
    robottotalpowercard.h \
    speedmodeselector.h \
    steeringmodeselector.h \
    techarcgauge.h \
    techpushbutton.h \
    techslideredit.h \
    techsliderlabel.h \
    techspeeddialsimple.h \
    techspeedgauge.h \
    techvirtualkeyboard.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    res.qrc

DISTFILES += \
    config.ini
