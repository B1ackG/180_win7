#include "timeseriesqueryservice.h"
#include "timeserieswriter.h"
#include "timeseriestypes.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QtTest>

class TestTimeSeriesQueryService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void queriesSingleDayAndKeepsNullChannelsEmpty();
    void queriesAcrossLocalMidnight();
    void downsampleKeepsMinAndMax();

private:
    TimeSeriesConfig testConfig(const QString &directory) const;
    TimeSeriesSample sampleAt(const QDateTime &local, TimeSeriesChannel channel, double value) const;
};

void TestTimeSeriesQueryService::initTestCase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        QSKIP("QSQLITE driver not available");
    }
}

TimeSeriesConfig TestTimeSeriesQueryService::testConfig(const QString &directory) const
{
    TimeSeriesConfig config;
    config.databaseDirectory = directory;
    config.sessionId = QStringLiteral("query-session");
    config.retentionDays = 366;
    config.reserveFreeMb = 512;
    return config.normalized();
}

TimeSeriesSample TestTimeSeriesQueryService::sampleAt(const QDateTime &local,
                                                     TimeSeriesChannel channel,
                                                     double value) const
{
    TimeSeriesSample sample;
    sample.tsUtcMs = local.toUTC().toMSecsSinceEpoch();
    sample.localOffsetMinutes = local.offsetFromUtc() / 60;
    sample.sessionId = QStringLiteral("query-session");
    sample.values[static_cast<int>(channel)].value = value;
    sample.values[static_cast<int>(channel)].valid = true;
    return sample;
}

void TestTimeSeriesQueryService::queriesSingleDayAndKeepsNullChannelsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime now = QDateTime(QDate(2026, 5, 1), QTime(12, 0, 0));
    writer.enqueueSample(sampleAt(now, TimeSeriesChannel::ArmTiltXDeg, 1.2));
    writer.enqueueSample(sampleAt(now.addSecs(1), TimeSeriesChannel::ArmTiltXDeg, 1.3));
    writer.flush();
    writer.shutdown();

    const QVector<TimeSeriesChannel> channels = {
        TimeSeriesChannel::ArmTiltXDeg,
        TimeSeriesChannel::LoadKg
    };
    const TimeSeriesQueryResult result = TimeSeriesQueryService::query(
        dir.path(),
        now.toUTC().toMSecsSinceEpoch() - 1000,
        now.addSecs(2).toUTC().toMSecsSinceEpoch(),
        channels);

    QVERIFY(result.ok);
    QCOMPARE(result.series.size(), 2);
    QCOMPARE(result.series.at(0).points.size(), 2);
    QCOMPARE(result.series.at(1).points.size(), 0);
    QVERIFY(result.series.at(0).points.at(0).tsUtcMs <= result.series.at(0).points.at(1).tsUtcMs);
}

void TestTimeSeriesQueryService::queriesAcrossLocalMidnight()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime before = QDateTime(QDate(2026, 6, 1), QTime(23, 59, 50));
    const QDateTime after = QDateTime(QDate(2026, 6, 2), QTime(0, 0, 10));
    writer.enqueueSample(sampleAt(before, TimeSeriesChannel::ChassisTiltXDeg, -0.5));
    writer.enqueueSample(sampleAt(after, TimeSeriesChannel::ChassisTiltXDeg, 0.4));
    writer.flush();
    writer.shutdown();

    const TimeSeriesQueryResult result = TimeSeriesQueryService::query(
        dir.path(),
        before.addSecs(-5).toUTC().toMSecsSinceEpoch(),
        after.addSecs(5).toUTC().toMSecsSinceEpoch(),
        {TimeSeriesChannel::ChassisTiltXDeg});

    QVERIFY(result.ok);
    QCOMPARE(result.series.size(), 1);
    QCOMPARE(result.series.at(0).points.size(), 2);
    QVERIFY(result.series.at(0).points.at(0).tsUtcMs < result.series.at(0).points.at(1).tsUtcMs);
}

void TestTimeSeriesQueryService::downsampleKeepsMinAndMax()
{
    QVector<TimeSeriesPoint> points;
    for (int i = 0; i < 20; ++i) {
        TimeSeriesPoint point;
        point.tsUtcMs = 1000 * i;
        point.id = i + 1;
        point.value = (i == 7) ? -50.0 : ((i == 13) ? 80.0 : static_cast<double>(i));
        points.append(point);
    }

    const QVector<TimeSeriesPoint> reduced = TimeSeriesQueryService::downsampleMinMax(points, 8);
    QVERIFY(reduced.size() <= 8);
    QVERIFY(reduced.size() >= 2);

    bool sawMin = false;
    bool sawMax = false;
    for (const TimeSeriesPoint &point : reduced) {
        if (qAbs(point.value - (-50.0)) < 0.001) {
            sawMin = true;
        }
        if (qAbs(point.value - 80.0) < 0.001) {
            sawMax = true;
        }
    }
    QVERIFY(sawMin);
    QVERIFY(sawMax);
}

QTEST_GUILESS_MAIN(TestTimeSeriesQueryService)
#include "tst_timeseriesqueryservice.moc"
