QT       += core gui widgets network quickwidgets concurrent sql

CONFIG   += c++17
TEMPLATE = app
TARGET   = 180_win7

# 静态库方案：modbus_backend_c.cpp + libmodbus.a 链进可执行文件。
# 工控机只需 dist/（exe + Qt/MinGW 运行库），不需要安装 Qt，也不需要 libmodbus.dll。

unix {
    INCLUDEPATH += $$PWD/third_party/libmodbus-local/include
    DEPENDPATH  += $$PWD/third_party/libmodbus-local/include
    LIBS += $$PWD/third_party/libmodbus-local/lib/libmodbus.a
    PRE_TARGETDEPS += $$PWD/third_party/libmodbus-local/lib/libmodbus.a
}

win32 {
    # 先运行 build_libmodbus_win32.bat（与 Qt 同一套 MinGW，只产出 .a）
    INCLUDEPATH += $$PWD/third_party/libmodbus-win32/include
    DEPENDPATH  += $$PWD/third_party/libmodbus-win32/include
    LIBS += $$PWD/third_party/libmodbus-win32/lib/libmodbus.a
    LIBS += -lws2_32
    PRE_TARGETDEPS += $$PWD/third_party/libmodbus-win32/lib/libmodbus.a
}

SOURCES += \
    agvmodbusmanager.cpp \
    modbus_backend_c.cpp \
    animationmanager.cpp \
    batterywidget.cpp \
    buttonmodbusmapping.cpp \
    displayregistermapping.cpp \
    devicecoordpanel.cpp \
    enablebuttonworker.cpp \
    featureswitchmanager.cpp \
    featureswitchwidget.cpp \
    historyarchiveio.cpp \
    historycalendarpanel.cpp \
    inclinometercard.cpp \
    metricvaluecard.cpp \
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
    navigationicon.cpp \
    modbustcpclient.cpp \
    modbusthreadmanager.cpp \
    modbusvariables.cpp \
    operationrecorder.cpp \
    poseprovider.cpp \
    robottotalpowercard.cpp \
    robottotaltorquecard.cpp \
    servofaultcodes.cpp \
    speedmodeselector.cpp \
    steeringmodeselector.cpp \
    techarcgauge.cpp \
    techchamfertoolbutton.cpp \
    techpushbutton.cpp \
    techshapes.cpp \
    techzonepanel.cpp \
    techslideredit.cpp \
    techsliderlabel.cpp \
    techspeedgauge.cpp \
    techvirtualkeyboard.cpp \
    timeseriestypes.cpp \
    timeseriesmanager.cpp \
    timeserieswriter.cpp \
    timeseriesqueryservice.cpp \
    timeserieschartwidget.cpp \
    timeseriesdatazoom.cpp \
    timeseriesdialog.cpp \
    timeseriesfullscreendialog.cpp \
    timeseriesconditiondialog.cpp

HEADERS += \
    agvmodbusmanager.h \
    animationmanager.h \
    batterywidget.h \
    buttonmodbusmapping.h \
    displayregistermapping.h \
    devicecoordpanel.h \
    debug.h \
    enablebuttonworker.h \
    featureswitchmanager.h \
    featureswitchwidget.h \
    historyarchiveio.h \
    historycalendarpanel.h \
    inclinometercard.h \
    metricvaluecard.h \
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
    navigationicon.h \
    modbustcpclient.h \
    modbusthreadmanager.h \
    modbusvariables.h \
    operationrecorder.h \
    poseprovider.h \
    robottotalpowercard.h \
    robottotaltorquecard.h \
    servofaultcodes.h \
    speedmodeselector.h \
    steeringmodeselector.h \
    techarcgauge.h \
    techchamfertoolbutton.h \
    techpushbutton.h \
    techshapes.h \
    techzonepanel.h \
    techslideredit.h \
    techsliderlabel.h \
    techspeeddialsimple.h \
    techspeedgauge.h \
    techvirtualkeyboard.h \
    timeseriestypes.h \
    timeseriesmanager.h \
    timeserieswriter.h \
    timeseriesqueryservice.h \
    timeserieschartwidget.h \
    timeseriesdatazoom.h \
    timeseriesdialog.h \
    timeseriesfullscreendialog.h \
    timeseriesconditiondialog.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    res.qrc

DISTFILES += \
    config.ini
