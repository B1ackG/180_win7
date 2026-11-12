#include "servofaultcodes.h"

#include <QtTest>

class TestServoFaultCodes : public QObject
{
    Q_OBJECT
private slots:
    void displayCodeFormatsE108();
    void decodeHighLowWordOrder();
    void decodeSwappedWordOrder();
    void skipZeroAxesAndKeepUnknown();
    void typoAlias4B01MapsToCanonical();
    void unknownFaultIsResettable();
    void knownNonResettableIsNotResettable();
    void signatureJoinsAxisAndDisplayCode();
    void lastAxisUsesRegister1024();
};

void TestServoFaultCodes::displayCodeFormatsE108()
{
    QCOMPARE(ServoFaultCodes::displayCodeFromRaw(0x01080108u), QStringLiteral("E108.0"));
    QCOMPARE(ServoFaultCodes::displayCodeFromRaw(0x11080108u), QStringLiteral("E108.1"));
    QCOMPARE(ServoFaultCodes::displayCodeFromRaw(0x41080108u), QStringLiteral("E108.4"));
}

void TestServoFaultCodes::decodeHighLowWordOrder()
{
    QVector<quint16> words(ServoFaultCodes::kRegisterCount, 0);
    words[0] = 0x0108;
    words[1] = 0x0108;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QCOMPARE(faults.at(0).axisIndex, 1);
    QCOMPARE(faults.at(0).displayCode, QStringLiteral("E108.0"));
    QCOMPARE(faults.at(0).name, QStringLiteral("写入存储参数故障"));
    QVERIFY(faults.at(0).known);

    const QStringList lines = ServoFaultCodes::faultLines(faults);
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0), QStringLiteral("1轴发生写入存储参数故障（E108.0）"));
}

void TestServoFaultCodes::decodeSwappedWordOrder()
{
    QVector<quint16> words(ServoFaultCodes::kRegisterCount, 0);
    words[2] = 0x0108;
    words[3] = 0x1108;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QCOMPARE(faults.at(0).axisIndex, 2);
    QCOMPARE(faults.at(0).displayCode, QStringLiteral("E108.1"));
    QCOMPARE(faults.at(0).name, QStringLiteral("读取存储参数故障"));
}

void TestServoFaultCodes::skipZeroAxesAndKeepUnknown()
{
    QVector<quint16> words(4, 0);
    words[2] = 0xDEAD;
    words[3] = 0xBEEF;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QCOMPARE(faults.at(0).axisIndex, 2);
    QCOMPARE(faults.at(0).name, QStringLiteral("未知故障"));
    QVERIFY(!faults.at(0).known);
    QVERIFY(faults.at(0).resettable);
}

void TestServoFaultCodes::typoAlias4B01MapsToCanonical()
{
    QVector<quint16> words(2, 0);
    words[0] = 0x4B01;
    words[1] = 0x6B01;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QCOMPARE(faults.at(0).displayCode, QStringLiteral("EB01.4"));
    QCOMPARE(faults.at(0).name,
             QStringLiteral("单圈绝对值模式目标位置超过机械单圈位置的最大值"));
    QVERIFY(faults.at(0).resettable);
}

void TestServoFaultCodes::unknownFaultIsResettable()
{
    QVector<quint16> words{0xDEAD, 0xBEEF};
    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QVERIFY(ServoFaultCodes::hasResettableFault(faults));
    QVERIFY(!ServoFaultCodes::hasKnownNonResettable(faults));
}

void TestServoFaultCodes::knownNonResettableIsNotResettable()
{
    QVector<quint16> words{0x0101, 0x0101};
    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QVERIFY(faults.at(0).known);
    QVERIFY(!faults.at(0).resettable);
    QVERIFY(ServoFaultCodes::hasKnownNonResettable(faults));
    QVERIFY(!ServoFaultCodes::hasResettableFault(faults));
}

void TestServoFaultCodes::signatureJoinsAxisAndDisplayCode()
{
    QVector<quint16> words(4, 0);
    words[0] = 0x0108;
    words[1] = 0x0108;
    words[2] = 0x0101;
    words[3] = 0x0101;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(ServoFaultCodes::faultSignature(faults),
             QStringLiteral("1:E108.0|2:E101.0"));
    QCOMPARE(ServoFaultCodes::historySummary(faults),
             QStringLiteral("1轴发生写入存储参数故障，2轴发生H02及以上功能码参数异常"));
    QVERIFY(ServoFaultCodes::hasResettableFault(faults));
    QVERIFY(ServoFaultCodes::hasKnownNonResettable(faults));
}

void TestServoFaultCodes::lastAxisUsesRegister1024()
{
    QCOMPARE(ServoFaultCodes::kRegisterStart, 1000);
    QCOMPARE(ServoFaultCodes::kRegisterLast, 1025);
    QCOMPARE(ServoFaultCodes::kRegisterCount, 26);
    QCOMPARE(ServoFaultCodes::kAxisCount, 13);

    QVector<quint16> words(ServoFaultCodes::kRegisterCount, 0);
    words[24] = 0x0108;
    words[25] = 0x0108;

    const QVector<ServoFaultCodes::AxisFault> faults =
        ServoFaultCodes::decodeRegisterWords(words);
    QCOMPARE(faults.size(), 1);
    QCOMPARE(faults.at(0).axisIndex, 13);
    QCOMPARE(faults.at(0).displayCode, QStringLiteral("E108.0"));
}

QTEST_APPLESS_MAIN(TestServoFaultCodes)
#include "tst_servofaultcodes.moc"
