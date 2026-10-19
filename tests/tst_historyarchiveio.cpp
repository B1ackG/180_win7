#include "historyarchiveio.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray fileHash(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

QByteArray recordLine(const QString &timestamp)
{
    return QStringLiteral(
               "{\"timestamp\":\"%1\",\"pageName\":\"日志服务\",\"controlName\":\"日志接收\","
               "\"controlType\":\"网络服务\",\"operation\":\"开启监听\",\"oldValue\":\"\","
               "\"newValue\":\"已开始监听\",\"runtime_ms\":0}\n")
        .arg(timestamp)
        .toUtf8();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == static_cast<qint64>(data.size());
}

} // namespace

class TestHistoryArchiveIo : public QObject
{
    Q_OBJECT

private slots:
    void sampleJsonLoads();
    void sampleJsonlSkipsOrRepairsDamagedLine();
    void scanPrefersCleanJson();
    void sampleFilesStayUnchanged();
    void emptyAndOversizedFilesFail();
    void damageLimitsFail();
    void dateAndEnvelopeStayHardFailures();
    void damagedStructureWithinLimitIsSkipped();
};

void TestHistoryArchiveIo::sampleJsonLoads()
{
    const HistoryLoadResult result = HistoryArchiveIo::loadDateFile(
        QStringLiteral("/home/l/work/180/docs/operation_history_20261018.json"),
        QDate(2026, 10, 18));
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.records.size(), 13316);
    QVERIFY(result.warning.isEmpty());
    if (QDate::currentDate() > QDate(2026, 10, 18)) {
        const HistoryLoadResult opened = HistoryArchiveIo::loadFile(
            QStringLiteral("/home/l/work/180/docs/operation_history_20261018.json"));
        QVERIFY2(opened.ok(), qPrintable(opened.error));
        QCOMPARE(opened.records.size(), 13316);
    }
}

void TestHistoryArchiveIo::sampleJsonlSkipsOrRepairsDamagedLine()
{
    const HistoryLoadResult result = HistoryArchiveIo::loadDateFile(
        QStringLiteral("/home/l/work/180/docs/operation_history_20261018.jsonl"),
        QDate(2026, 10, 18));
    QVERIFY2(result.ok(), qPrintable(result.error));
    QVERIFY(result.repairedLineCount >= 1);
    QCOMPARE(result.records.size(), 13317);
    QVERIFY(!result.warning.isEmpty());
}

void TestHistoryArchiveIo::scanPrefersCleanJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString jsonPath = dir.filePath(QStringLiteral("operation_history_20261018.json"));
    const QString jsonlPath = dir.filePath(QStringLiteral("operation_history_20261018.jsonl"));
    QVERIFY(QFile::copy(QStringLiteral("/home/l/work/180/docs/operation_history_20261018.json"), jsonPath));
    QVERIFY(QFile::copy(QStringLiteral("/home/l/work/180/docs/operation_history_20261018.jsonl"), jsonlPath));

    const HistoryDateScanResult scan = HistoryArchiveIo::scanAvailableDates(dir.path());
    QVERIFY2(scan.ok(), qPrintable(scan.error));
    const HistoryDateEntry entry = scan.entries.value(QDate(2026, 10, 18));
    QVERIFY(entry.path.endsWith(QStringLiteral(".json")));
    QVERIFY(!entry.jsonLines);

    const HistoryLoadResult loaded = HistoryArchiveIo::loadDateFile(entry.path, QDate(2026, 10, 18));
    QVERIFY2(loaded.ok(), qPrintable(loaded.error));
    QCOMPARE(loaded.records.size(), 13316);
}

void TestHistoryArchiveIo::sampleFilesStayUnchanged()
{
    const QString jsonPath = QStringLiteral("/home/l/work/180/docs/operation_history_20261018.json");
    const QString jsonlPath = QStringLiteral("/home/l/work/180/docs/operation_history_20261018.jsonl");
    const QByteArray jsonBefore = fileHash(jsonPath);
    const QByteArray jsonlBefore = fileHash(jsonlPath);
    HistoryArchiveIo::loadDateFile(jsonPath, QDate(2026, 10, 18));
    HistoryArchiveIo::loadDateFile(jsonlPath, QDate(2026, 10, 18));
    QCOMPARE(fileHash(jsonPath), jsonBefore);
    QCOMPARE(fileHash(jsonlPath), jsonlBefore);
}

void TestHistoryArchiveIo::emptyAndOversizedFilesFail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString emptyPath = dir.filePath(QStringLiteral("empty.json"));
    QVERIFY(writeFile(emptyPath, QByteArray()));
    const HistoryLoadResult emptyResult = HistoryArchiveIo::loadFile(emptyPath);
    QVERIFY(!emptyResult.ok());
    QCOMPARE(emptyResult.error, QStringLiteral("历史记录文件是空的"));

    const QString hugePath = dir.filePath(QStringLiteral("huge.jsonl"));
    QFile huge(hugePath);
    QVERIFY(huge.open(QIODevice::WriteOnly));
    QVERIFY(huge.resize(50LL * 1024 * 1024 + 1));
    huge.close();
    const HistoryLoadResult hugeResult = HistoryArchiveIo::loadFile(hugePath);
    QVERIFY(!hugeResult.ok());
    QCOMPARE(hugeResult.error, QStringLiteral("历史记录文件过大，无法加载"));
}

void TestHistoryArchiveIo::damageLimitsFail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray allBad;
    for (int i = 0; i < 10; ++i) {
        allBad += "这不是记录\n";
    }
    const QString allBadPath = dir.filePath(QStringLiteral("all.jsonl"));
    QVERIFY(writeFile(allBadPath, allBad));
    const HistoryLoadResult allBadResult = HistoryArchiveIo::loadDateFile(allBadPath, QDate(2026, 10, 18));
    QVERIFY(!allBadResult.ok());
    QCOMPARE(allBadResult.error, QStringLiteral("历史文件损坏内容过多，无法安全读取"));

    QByteArray ratio;
    ratio += recordLine(QStringLiteral("2026-10-18T10:00:00"));
    ratio += "损坏\n损坏\n";
    const QString ratioPath = dir.filePath(QStringLiteral("ratio.jsonl"));
    QVERIFY(writeFile(ratioPath, ratio));
    const HistoryLoadResult ratioResult = HistoryArchiveIo::loadDateFile(ratioPath, QDate(2026, 10, 18));
    QVERIFY(!ratioResult.ok());
    QCOMPARE(ratioResult.error, QStringLiteral("历史文件损坏内容过多，无法安全读取"));

    QByteArray many;
    for (int i = 0; i < 200; ++i) {
        many += recordLine(QStringLiteral("2026-10-18T10:00:00"));
    }
    for (int i = 0; i < 101; ++i) {
        many += "损坏\n";
    }
    const QString manyPath = dir.filePath(QStringLiteral("many.jsonl"));
    QVERIFY(writeFile(manyPath, many));
    const HistoryLoadResult manyResult = HistoryArchiveIo::loadDateFile(manyPath, QDate(2026, 10, 18));
    QVERIFY(!manyResult.ok());
    QCOMPARE(manyResult.error, QStringLiteral("历史文件损坏内容过多，无法安全读取"));
}

void TestHistoryArchiveIo::dateAndEnvelopeStayHardFailures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datePath = dir.filePath(QStringLiteral("date.jsonl"));
    QVERIFY(writeFile(datePath, recordLine(QStringLiteral("2026-10-17T10:00:00"))));
    const HistoryLoadResult dateResult = HistoryArchiveIo::loadDateFile(datePath, QDate(2026, 10, 18));
    QVERIFY(!dateResult.ok());
    QVERIFY(dateResult.error.contains(QStringLiteral("不属于")));

    const QString envelopePath = dir.filePath(QStringLiteral("envelope.jsonl"));
    QVERIFY(writeFile(envelopePath, QByteArray("{\"payload\":\"e30=\",\"sig\":\"abc\"}\n")));
    const HistoryLoadResult envelopeResult = HistoryArchiveIo::loadDateFile(envelopePath, QDate(2026, 10, 18));
    QVERIFY(!envelopeResult.ok());
    QVERIFY(envelopeResult.error.contains(QStringLiteral("格式不正确")));
}

void TestHistoryArchiveIo::damagedStructureWithinLimitIsSkipped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray data;
    for (int i = 0; i < 200; ++i) {
        data += recordLine(QStringLiteral("2026-10-18T10:00:00"));
    }
    data += "{\"timestamp\":\"2026-10-18T10:00:00\"}\n";
    data += "{\"timestamp\":\"2026-10-18T10:00:00\",\"pageName\":1,\"controlName\":\"日志接收\","
            "\"controlType\":\"网络服务\",\"operation\":\"开启监听\",\"oldValue\":\"\","
            "\"newValue\":\"已开始监听\",\"runtime_ms\":0}\n";
    QByteArray repaired = recordLine(QStringLiteral("2026-10-18T10:00:01"));
    repaired.insert(10, '\0');
    data += repaired;

    const QString path = dir.filePath(QStringLiteral("mixed.jsonl"));
    QVERIFY(writeFile(path, data));
    const HistoryLoadResult result = HistoryArchiveIo::loadDateFile(path, QDate(2026, 10, 18));
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.records.size(), 201);
    QCOMPARE(result.skippedLineCount, 2);
    QCOMPARE(result.repairedLineCount, 1);
    QVERIFY(!result.warning.isEmpty());

    QByteArray nullOnly;
    for (int i = 0; i < 150; ++i) {
        nullOnly += recordLine(QStringLiteral("2026-10-18T12:00:00"));
    }
    nullOnly += QByteArray("\0\0\0\n", 4);
    nullOnly += "\n";
    const QString nullPath = dir.filePath(QStringLiteral("null-only.jsonl"));
    QVERIFY(writeFile(nullPath, nullOnly));
    const HistoryLoadResult nullResult = HistoryArchiveIo::loadDateFile(nullPath, QDate(2026, 10, 18));
    QVERIFY2(nullResult.ok(), qPrintable(nullResult.error));
    QCOMPARE(nullResult.records.size(), 150);
    QCOMPARE(nullResult.skippedLineCount, 1);
    QVERIFY(nullResult.skippedLocations.contains(QStringLiteral("151")));
}

QTEST_GUILESS_MAIN(TestHistoryArchiveIo)
#include "tst_historyarchiveio.moc"
