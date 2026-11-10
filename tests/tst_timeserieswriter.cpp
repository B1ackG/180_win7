#include "timeserieswriter.h"
#include "timeseriestypes.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class TestTimeSeriesWriter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createsSchemaAndInsertsBatch();
    void splitsSamplesAcrossLocalDates();
    void deletesExpiredDailyFiles();
    void quarantinesCorruptDatabase();
    void flushOnShutdownPersistsQueue();

private:
    TimeSeriesConfig testConfig(const QString &directory) const;
    TimeSeriesSample sampleAt(const QDateTime &local, double j1) const;
    int rowCount(const QString &path) const;
};

void TestTimeSeriesWriter::initTestCase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        QSKIP("QSQLITE driver not available");
    }
}

TimeSeriesConfig TestTimeSeriesWriter::testConfig(const QString &directory) const
{
    TimeSeriesConfig config;
    config.enabled = true;
    config.sampleIntervalMs = 1000;
    config.flushIntervalMs = 5000;
    config.retentionDays = 366;
    config.reserveFreeMb = 512;
    config.maxQueueRows = 300;
    config.databaseDirectory = directory;
    config.sessionId = QStringLiteral("test-session");
    return config.normalized();
}

TimeSeriesSample TestTimeSeriesWriter::sampleAt(const QDateTime &local, double j1) const
{
    TimeSeriesSample sample;
    sample.tsUtcMs = local.toUTC().toMSecsSinceEpoch();
    sample.localOffsetMinutes = local.offsetFromUtc() / 60;
    sample.sessionId = QStringLiteral("test-session");
    sample.values[static_cast<int>(TimeSeriesChannel::J1AngleDeg)].value = j1;
    sample.values[static_cast<int>(TimeSeriesChannel::J1AngleDeg)].valid = true;
    return sample;
}

int TestTimeSeriesWriter::rowCount(const QString &path) const
{
    const QString name = QStringLiteral("tst_count_%1").arg(path.size());
    int count = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM samples")) && query.next()) {
                count = query.value(0).toInt();
            }
        }
    }
    QSqlDatabase::removeDatabase(name);
    return count;
}

void TestTimeSeriesWriter::createsSchemaAndInsertsBatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));

    const QDateTime now = QDateTime::currentDateTime();
    writer.enqueueSample(sampleAt(now, 1.5));
    writer.enqueueSample(sampleAt(now.addSecs(1), 0.0));
    writer.flush();
    writer.shutdown();

    const QString path = TimeSeriesMeta::dailyFilePath(dir.path(), now.date());
    QVERIFY(QFile::exists(path));
    QCOMPARE(rowCount(path), 2);
}

void TestTimeSeriesWriter::splitsSamplesAcrossLocalDates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));

    const QDateTime day1 = QDateTime(QDate(2026, 3, 1), QTime(23, 59, 50));
    const QDateTime day2 = QDateTime(QDate(2026, 3, 2), QTime(0, 0, 10));
    writer.enqueueSample(sampleAt(day1, 10.0));
    writer.enqueueSample(sampleAt(day2, 11.0));
    writer.flush();
    writer.shutdown();

    QCOMPARE(rowCount(TimeSeriesMeta::dailyFilePath(dir.path(), day1.date())), 1);
    QCOMPARE(rowCount(TimeSeriesMeta::dailyFilePath(dir.path(), day2.date())), 1);
}

void TestTimeSeriesWriter::deletesExpiredDailyFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString oldPath = TimeSeriesMeta::dailyFilePath(dir.path(), QDate(2000, 1, 1));
    QFile oldFile(oldPath);
    QVERIFY(oldFile.open(QIODevice::WriteOnly));
    oldFile.write("placeholder");
    oldFile.close();

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    writer.shutdown();

    QVERIFY(!QFile::exists(oldPath));
}

void TestTimeSeriesWriter::quarantinesCorruptDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = TimeSeriesMeta::dailyFilePath(dir.path(), QDate::currentDate());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a sqlite database");
    file.close();

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    writer.enqueueSample(sampleAt(QDateTime::currentDateTime(), 3.0));
    writer.flush();
    writer.shutdown();

    QVERIFY(QFile::exists(path));
    QVERIFY(rowCount(path) >= 1);
    const QStringList leftovers = QDir(dir.path()).entryList(QStringList() << QStringLiteral("*.corrupt_*"),
                                                             QDir::Files);
    QVERIFY(!leftovers.isEmpty());
}

void TestTimeSeriesWriter::flushOnShutdownPersistsQueue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime now = QDateTime::currentDateTime();
    writer.enqueueSample(sampleAt(now, 8.0));
    writer.shutdown();

    QCOMPARE(rowCount(TimeSeriesMeta::dailyFilePath(dir.path(), now.date())), 1);
}

QTEST_GUILESS_MAIN(TestTimeSeriesWriter)
#include "tst_timeserieswriter.moc"
