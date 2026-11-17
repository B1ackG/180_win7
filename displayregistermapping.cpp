#include "displayregistermapping.h"

#include <QSettings>

namespace DisplayRegisterMapping {
namespace {

DisplaySpec makeSpec(const QString &device,
                     int start,
                     int wordCount,
                     Encoding encoding,
                     double scale = 1.0,
                     int sumStart = -1,
                     int secondStart = -1,
                     int secondCount = 0)
{
    DisplaySpec spec;
    spec.device = device;
    spec.start = start;
    spec.wordCount = wordCount;
    spec.encoding = encoding;
    spec.scale = scale;
    spec.sumStart = sumStart;
    spec.secondStart = secondStart;
    spec.secondCount = secondCount;
    return spec;
}

const QMap<QString, DisplaySpec> &knownSpecs()
{
    static const QMap<QString, DisplaySpec> specs = {
        {QStringLiteral("robot_ArcGauge_J1Angle"),
         makeSpec(QStringLiteral("主控"), 0, 4, Encoding::DcbaFehgDouble, 1.0, -1, 36, 4)},
        {QStringLiteral("robot_ArcGauge_J2Height"),
         makeSpec(QStringLiteral("主控"), 4, 4, Encoding::DcbaFehgDouble, 1.0, -1, 40, 4)},
        {QStringLiteral("robot_ArcGauge_J3Length"),
         makeSpec(QStringLiteral("主控"), 12, 4, Encoding::DcbaFehgDouble, 1.0, 16, 48, 8)},
        {QStringLiteral("robot_ArcGauge_J4Angle"),
         makeSpec(QStringLiteral("主控"), 20, 4, Encoding::DcbaFehgDouble, 1.0, -1, 56, 4)},
        {QStringLiteral("robot_ArcGauge_CounterweightHeight"),
         makeSpec(QStringLiteral("主控"), 8, 4, Encoding::DcbaFehgDouble, 1.0, -1, 44, 4)},
        {QStringLiteral("robot_ArcGauge_SixAxis1"),
         makeSpec(QStringLiteral("主控"), 73, 2, Encoding::CdabFloat)},
        {QStringLiteral("robot_ArcGauge_SixAxis2"),
         makeSpec(QStringLiteral("主控"), 75, 2, Encoding::CdabFloat)},
        {QStringLiteral("robot_ArcGauge_SixAxis3"),
         makeSpec(QStringLiteral("主控"), 77, 2, Encoding::CdabFloat)},
        {QStringLiteral("robot_ArcGauge_SixAxis4"),
         makeSpec(QStringLiteral("主控"), 79, 2, Encoding::CdabFloat, 1000.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis5"),
         makeSpec(QStringLiteral("主控"), 81, 2, Encoding::CdabFloat, 1000.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis6"),
         makeSpec(QStringLiteral("主控"), 83, 2, Encoding::CdabFloat, 1000.0)},
        {QStringLiteral("weightCard"),
         makeSpec(QStringLiteral("主控"), 123, 1, Encoding::UInt16)},
        {QStringLiteral("frontSupportForceCard"),
         makeSpec(QStringLiteral("主控"), 139, 1, Encoding::UInt16)},
        {QStringLiteral("robotTotalPowerCard"),
         makeSpec(QStringLiteral("主控"), 134, 1, Encoding::UInt16)},
        {QStringLiteral("robotTotalTorqueCard"),
         makeSpec(QStringLiteral("主控"), 276, 4, Encoding::DcbaFehgDouble)},
        {QStringLiteral("robotCounterweightTorqueCard"),
         makeSpec(QStringLiteral("主控"), 280, 4, Encoding::DcbaFehgDouble)},
        {QStringLiteral("robotArmTorqueCard"),
         makeSpec(QStringLiteral("主控"), 296, 4, Encoding::DcbaFehgDouble)},
        {QStringLiteral("robotInclinometerX"),
         makeSpec(QStringLiteral("主控"), 135, 2, Encoding::CdabFloat)},
        {QStringLiteral("robotInclinometerY"),
         makeSpec(QStringLiteral("主控"), 137, 2, Encoding::CdabFloat)},
        {QStringLiteral("chassisInclinometerX"),
         makeSpec(QStringLiteral("AGV"), 151, 1, Encoding::Int16Div100)},
        {QStringLiteral("chassisInclinometerY"),
         makeSpec(QStringLiteral("AGV"), 152, 1, Encoding::Int16Div100)},
        {QStringLiteral("AGV_speedGauge"),
         makeSpec(QStringLiteral("AGV"), 104, 1, Encoding::UInt16)},
    };
    return specs;
}

const QMap<QString, QString> &titleMap()
{
    static const QMap<QString, QString> titles = {
        {QStringLiteral("robot_ArcGauge_J1Angle"), QStringLiteral("悬臂角度 (J1)")},
        {QStringLiteral("robot_ArcGauge_J2Height"), QStringLiteral("升降高度 (J2)")},
        {QStringLiteral("robot_ArcGauge_J3Length"), QStringLiteral("总伸展长度 (J3)")},
        {QStringLiteral("robot_ArcGauge_J4Angle"), QStringLiteral("末端角度 (J4)")},
        {QStringLiteral("robot_ArcGauge_CounterweightHeight"), QStringLiteral("配重缸升降")},
        {QStringLiteral("robot_ArcGauge_SixAxis1"), QStringLiteral("六轴 RX")},
        {QStringLiteral("robot_ArcGauge_SixAxis2"), QStringLiteral("六轴 RY")},
        {QStringLiteral("robot_ArcGauge_SixAxis3"), QStringLiteral("六轴 RZ")},
        {QStringLiteral("robot_ArcGauge_SixAxis4"), QStringLiteral("六轴 X")},
        {QStringLiteral("robot_ArcGauge_SixAxis5"), QStringLiteral("六轴 Y")},
        {QStringLiteral("robot_ArcGauge_SixAxis6"), QStringLiteral("六轴 Z")},
        {QStringLiteral("weightCard"), QStringLiteral("当前负载")},
        {QStringLiteral("frontSupportForceCard"), QStringLiteral("前支撑力")},
        {QStringLiteral("robotTotalPowerCard"), QStringLiteral("总功率")},
        {QStringLiteral("robotTotalTorqueCard"), QStringLiteral("立柱扭矩")},
        {QStringLiteral("robotCounterweightTorqueCard"), QStringLiteral("配重扭矩")},
        {QStringLiteral("robotArmTorqueCard"), QStringLiteral("悬臂扭矩")},
        {QStringLiteral("robotInclinometerX"), QStringLiteral("悬臂倾角 X")},
        {QStringLiteral("robotInclinometerY"), QStringLiteral("悬臂倾角 Y")},
        {QStringLiteral("chassisInclinometerX"), QStringLiteral("底盘倾角 X")},
        {QStringLiteral("chassisInclinometerY"), QStringLiteral("底盘倾角 Y")},
        {QStringLiteral("AGV_speedGauge"), QStringLiteral("行驶速度")},
    };
    return titles;
}

int boundedAddress(int value)
{
    return qBound(0, value, 65535);
}

int boundedWordCount(int value)
{
    if (value == 1 || value == 2 || value == 4 || value == 8) {
        return value;
    }
    return 1;
}

} // namespace

QString encodingKey(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Int16Div100:
        return QStringLiteral("int16_div100");
    case Encoding::CdabFloat:
        return QStringLiteral("cdab_float");
    case Encoding::DcbaFehgDouble:
        return QStringLiteral("dcba_fehg_double");
    case Encoding::UInt16:
    default:
        return QStringLiteral("uint16");
    }
}

Encoding encodingFromKey(const QString &key, Encoding fallback)
{
    const QString k = key.trimmed().toLower();
    if (k == QStringLiteral("uint16")) {
        return Encoding::UInt16;
    }
    if (k == QStringLiteral("int16_div100")) {
        return Encoding::Int16Div100;
    }
    if (k == QStringLiteral("cdab_float")) {
        return Encoding::CdabFloat;
    }
    if (k == QStringLiteral("dcba_fehg_double")) {
        return Encoding::DcbaFehgDouble;
    }
    return fallback;
}

QString encodingLabel(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Int16Div100:
        return QStringLiteral("有符号/100");
    case Encoding::CdabFloat:
        return QStringLiteral("CDAB float");
    case Encoding::DcbaFehgDouble:
        return QStringLiteral("DCBA FEHG double");
    case Encoding::UInt16:
    default:
        return QStringLiteral("uint16");
    }
}

DisplaySpec defaultSpec(const QString &objectName)
{
    if (!knownSpecs().contains(objectName)) {
        DisplaySpec empty;
        empty.device = QStringLiteral("无");
        empty.start = -1;
        empty.wordCount = 0;
        return empty;
    }
    return knownSpecs().value(objectName);
}

DisplaySpec resolvedSpec(const QString &objectName)
{
    const DisplaySpec fallback = defaultSpec(objectName);
    if (!knownSpecs().contains(objectName)) {
        return fallback;
    }

    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("DisplayRegisterMapping"));
    if (!settings.contains(objectName + QStringLiteral("_device"))
        && !settings.contains(objectName + QStringLiteral("_start"))) {
        settings.endGroup();
        return fallback;
    }

    DisplaySpec loaded = fallback;
    loaded.device = settings.value(objectName + QStringLiteral("_device"), fallback.device).toString();
    loaded.start = boundedAddress(
        settings.value(objectName + QStringLiteral("_start"), fallback.start).toInt());
    loaded.wordCount = boundedWordCount(
        settings.value(objectName + QStringLiteral("_count"), fallback.wordCount).toInt());
    loaded.encoding = encodingFromKey(
        settings.value(objectName + QStringLiteral("_encoding"), encodingKey(fallback.encoding)).toString(),
        fallback.encoding);
    loaded.scale = settings.value(objectName + QStringLiteral("_scale"), fallback.scale).toDouble();
    if (loaded.scale == 0.0) {
        loaded.scale = fallback.scale;
    }
    loaded.sumStart = settings.value(objectName + QStringLiteral("_sum_start"), fallback.sumStart).toInt();
    loaded.secondStart = settings.value(objectName + QStringLiteral("_second_start"), fallback.secondStart).toInt();
    loaded.secondCount = settings.value(objectName + QStringLiteral("_second_count"), fallback.secondCount).toInt();
    settings.endGroup();
    return loaded;
}

QMap<QString, DisplaySpec> resolvedAllSpecs()
{
    QMap<QString, DisplaySpec> out;
    const QStringList names = knownObjectNames();
    for (const QString &name : names) {
        out.insert(name, resolvedSpec(name));
    }
    return out;
}

QStringList knownObjectNames()
{
    return knownSpecs().keys();
}

QStringList arcGaugeObjectNames()
{
    static const QStringList names = {
        QStringLiteral("robot_ArcGauge_J1Angle"),
        QStringLiteral("robot_ArcGauge_J2Height"),
        QStringLiteral("robot_ArcGauge_J3Length"),
        QStringLiteral("robot_ArcGauge_J4Angle"),
        QStringLiteral("robot_ArcGauge_CounterweightHeight"),
        QStringLiteral("robot_ArcGauge_SixAxis1"),
        QStringLiteral("robot_ArcGauge_SixAxis2"),
        QStringLiteral("robot_ArcGauge_SixAxis3"),
        QStringLiteral("robot_ArcGauge_SixAxis4"),
        QStringLiteral("robot_ArcGauge_SixAxis5"),
        QStringLiteral("robot_ArcGauge_SixAxis6"),
    };
    return names;
}

QStringList statusObjectNames()
{
    static const QStringList names = {
        QStringLiteral("weightCard"),
        QStringLiteral("frontSupportForceCard"),
        QStringLiteral("robotTotalPowerCard"),
        QStringLiteral("robotTotalTorqueCard"),
        QStringLiteral("robotCounterweightTorqueCard"),
        QStringLiteral("robotArmTorqueCard"),
        QStringLiteral("robotInclinometerX"),
        QStringLiteral("robotInclinometerY"),
        QStringLiteral("chassisInclinometerX"),
        QStringLiteral("chassisInclinometerY"),
        QStringLiteral("AGV_speedGauge"),
    };
    return names;
}

QString displayTitle(const QString &objectName)
{
    return titleMap().value(objectName, objectName);
}

void writeSpec(QSettings &settings, const QString &objectName, const DisplaySpec &spec)
{
    settings.setValue(objectName + QStringLiteral("_device"), spec.device);
    settings.setValue(objectName + QStringLiteral("_start"), spec.start);
    settings.setValue(objectName + QStringLiteral("_count"), spec.wordCount);
    settings.setValue(objectName + QStringLiteral("_encoding"), encodingKey(spec.encoding));
    settings.setValue(objectName + QStringLiteral("_scale"), spec.scale);
    settings.setValue(objectName + QStringLiteral("_sum_start"), spec.sumStart);
    settings.setValue(objectName + QStringLiteral("_second_start"), spec.secondStart);
    settings.setValue(objectName + QStringLiteral("_second_count"), spec.secondCount);
}

int lastAddress(const DisplaySpec &spec)
{
    if (!spec.isConfigured()) {
        return -1;
    }
    return spec.start + spec.wordCount - 1;
}

bool containsAddress(const DisplaySpec &spec, int address)
{
    if (!spec.isConfigured()) {
        return false;
    }
    if (address >= spec.start && address <= lastAddress(spec)) {
        return true;
    }
    if (hasSumStart(spec)
        && address >= spec.sumStart && address < spec.sumStart + spec.wordCount) {
        return true;
    }
    if (hasSecondRing(spec)
        && address >= spec.secondStart && address < spec.secondStart + spec.secondCount) {
        return true;
    }
    return false;
}

bool hasSumStart(const DisplaySpec &spec)
{
    return spec.sumStart >= 0 && spec.wordCount > 0;
}

bool hasSecondRing(const DisplaySpec &spec)
{
    return spec.secondStart >= 0 && spec.secondCount > 0;
}

} // namespace DisplayRegisterMapping
