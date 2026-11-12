#ifndef SERVOFAULTCODES_H
#define SERVOFAULTCODES_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace ServoFaultCodes {

constexpr int kRegisterStart = 1000;
constexpr int kRegisterLast = 1025;
constexpr int kRegisterCount = kRegisterLast - kRegisterStart + 1;
constexpr int kAxisCount = kRegisterCount / 2;
static_assert(kRegisterCount % 2 == 0, "servo fault registers are high/low pairs");
static_assert(kAxisCount == 13, "1000~1025 maps to 13 axes");

struct AxisFault {
    int axisIndex = 0;
    quint32 rawCode = 0;
    QString displayCode;
    QString name;
    bool resettable = false;
    bool known = false;
};

QString displayCodeFromRaw(quint32 code);
QVector<AxisFault> decodeRegisterWords(const QVector<quint16> &words);
QStringList faultLines(const QVector<AxisFault> &faults);
QStringList historyLines(const QVector<AxisFault> &faults);
QString historySummary(const QVector<AxisFault> &faults);
QString faultSignature(const QVector<AxisFault> &faults);
bool hasKnownNonResettable(const QVector<AxisFault> &faults);
bool hasResettableFault(const QVector<AxisFault> &faults);

}

#endif
