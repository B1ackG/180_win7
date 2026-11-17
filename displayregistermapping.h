#ifndef DISPLAYREGISTERMAPPING_H
#define DISPLAYREGISTERMAPPING_H

#include <QMap>
#include <QSettings>
#include <QString>
#include <QStringList>

/**
 * 功能控制台「显示控件读寄存器」：环形仪表与只读状态卡的起始地址、字数、编码。
 * 默认值与当前硬编码一致；config.ini [DisplayRegisterMapping] 可覆盖。
 */
namespace DisplayRegisterMapping {

enum class Encoding : quint8 {
    UInt16 = 0,
    Int16Div100 = 1,
    CdabFloat = 2,
    DcbaFehgDouble = 3
};

struct DisplaySpec {
    QString device = QStringLiteral("主控");
    int start = 0;
    int wordCount = 1;
    Encoding encoding = Encoding::UInt16;
    double scale = 1.0;
    int sumStart = -1;
    int secondStart = -1;
    int secondCount = 0;
    bool isConfigured() const {
        return device != QStringLiteral("无") && start >= 0 && wordCount > 0;
    }
};

QString encodingKey(Encoding encoding);
Encoding encodingFromKey(const QString &key, Encoding fallback);
QString encodingLabel(Encoding encoding);

DisplaySpec defaultSpec(const QString &objectName);
DisplaySpec resolvedSpec(const QString &objectName);
QMap<QString, DisplaySpec> resolvedAllSpecs();
QStringList knownObjectNames();
QStringList arcGaugeObjectNames();
QStringList statusObjectNames();
QString displayTitle(const QString &objectName);
void writeSpec(QSettings &settings, const QString &objectName, const DisplaySpec &spec);

bool containsAddress(const DisplaySpec &spec, int address);
int lastAddress(const DisplaySpec &spec);
bool hasSumStart(const DisplaySpec &spec);
bool hasSecondRing(const DisplaySpec &spec);

} // namespace DisplayRegisterMapping

#endif
