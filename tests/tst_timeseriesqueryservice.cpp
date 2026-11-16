#include "timeseriesqueryservice.h"
#include "timeserieswriter.h"
#include "timeseriestypes.h"

#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTime>
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
    void conditionSearchMatchesRoundedWorkKeysAndReportsState();
    void conditionSearchRejectsDifferentPoseAndClustersDwell();
    void queriesNewChannelsAndSkipsMissingLegacyColumns();

private:
    TimeSeriesConfig testConfig(const QString &directory) const;
    TimeSeriesSample sampleAt(const QDateTime &local, TimeSeriesChannel channel, double value) const;
    TimeSeriesSample poseSample(const QDateTime &local,
                                double j1,
                                double j2,
                                double j3,
                                double load,
                                double armX,
                                double armY) const;
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

TimeSeriesSample TestTimeSeriesQueryService::poseSample(const QDateTime &local,
                                                        double j1,
                                                        double j2,
                                                        double j3,
                                                        double load,
                                                        double armX,
                                                        double armY) const
{
    TimeSeriesSample sample;
    sample.tsUtcMs = local.toUTC().toMSecsSinceEpoch();
    sample.localOffsetMinutes = local.offsetFromUtc() / 60;
    sample.sessionId = QStringLiteral("query-session");
    auto set = [&](TimeSeriesChannel channel, double value) {
        sample.values[static_cast<int>(channel)].value = value;
        sample.values[static_cast<int>(channel)].valid = true;
    };
    set(TimeSeriesChannel::J1AngleDeg, j1);
    set(TimeSeriesChannel::J2HeightMm, j2);
    set(TimeSeriesChannel::J3TotalLengthMm, j3);
    set(TimeSeriesChannel::J4AngleDeg, 5.2);
    set(TimeSeriesChannel::CounterweightHeightMm, 400);
    set(TimeSeriesChannel::PlaneHeightMm, 900);
    set(TimeSeriesChannel::LoadKg, load);
    set(TimeSeriesChannel::ColumnTorquePercent, 21.0);
    set(TimeSeriesChannel::CounterweightTorquePercent, 14.0);
    set(TimeSeriesChannel::ArmTorquePercent, 9.5);
    set(TimeSeriesChannel::ArmTiltXDeg, armX);
    set(TimeSeriesChannel::ArmTiltYDeg, armY);
    set(TimeSeriesChannel::TotalPowerW, 820);
    set(TimeSeriesChannel::ChassisTiltXDeg, 0.1);
    set(TimeSeriesChannel::ChassisTiltYDeg, -0.2);
    set(TimeSeriesChannel::FrontSupportForceKg, 320);
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
    QVERIFY(result.series.at(0).hasStats);
    QCOMPARE(result.series.at(0).minValue, 1.2);
    QCOMPARE(result.series.at(0).maxValue, 1.3);
    QVERIFY(!result.series.at(1).hasStats);
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
    QVERIFY(result.series.at(0).hasStats);
    QCOMPARE(result.series.at(0).minValue, -0.5);
    QCOMPARE(result.series.at(0).maxValue, 0.4);
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

void TestTimeSeriesQueryService::conditionSearchMatchesRoundedWorkKeysAndReportsState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime probe = QDateTime(QDate(2026, 11, 10), QTime(12, 0, 0));
    const QDateTime match = QDateTime(QDate(2026, 10, 20), QTime(9, 30, 0));
    writer.enqueueSample(poseSample(probe, 12.3, 805, 1204, 52, 0.4, -0.1));
    writer.enqueueSample(poseSample(match, 12.4, 812, 1196, 51, 1.8, 0.6));
    writer.enqueueSample(poseSample(QDateTime(QDate(2026, 10, 20), QTime(10, 0, 0)),
                                    18.0, 805, 1204, 52, 0.2, 0.1));
    writer.flush();
    writer.shutdown();

    const TimeSeriesConditionSearchResult result = TimeSeriesQueryService::searchSimilarConditions(
        dir.path(),
        probe.toUTC().toMSecsSinceEpoch(),
        QDate(2026, 11, 11));

    QVERIFY(result.ok);
    QCOMPARE(TimeSeriesMeta::conditionWorkChannels().size(), 7);
    QCOMPARE(TimeSeriesMeta::conditionStateChannels().size(), 9);
    QVERIFY(TimeSeriesMeta::conditionWorkChannels().contains(TimeSeriesChannel::CounterweightHeightMm));
    QVERIFY(TimeSeriesMeta::conditionWorkChannels().contains(TimeSeriesChannel::PlaneHeightMm));
    QVERIFY(TimeSeriesMeta::conditionStateChannels().contains(TimeSeriesChannel::TotalPowerW));
    QVERIFY(TimeSeriesMeta::conditionStateChannels().contains(TimeSeriesChannel::ColumnTorquePercent));
    QVERIFY(TimeSeriesMeta::conditionStateChannels().contains(TimeSeriesChannel::ArmTorquePercent));
    QVERIFY(TimeSeriesMeta::conditionStateChannels().contains(TimeSeriesChannel::FrontSupportForceKg));
    QCOMPARE(result.hits.size(), 1);
    QCOMPARE(result.hits.first().tsUtcMs, match.toUTC().toMSecsSinceEpoch());
    QCOMPARE(result.workChannels.size(), 7);
    QVERIFY(result.probeWork.first().valid);
    QCOMPARE(result.stateChannels.at(3), TimeSeriesChannel::ArmTiltXDeg);
    QCOMPARE(result.hits.first().stateValues.at(3).value, 1.8);
}

void TestTimeSeriesQueryService::conditionSearchRejectsDifferentPoseAndClustersDwell()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime probe = QDateTime(QDate(2026, 11, 10), QTime(12, 0, 0));
    const QDateTime dwell = QDateTime(QDate(2026, 11, 1), QTime(15, 0, 0));
    writer.enqueueSample(poseSample(probe, 12.3, 800, 1200, 50, 0.4, -0.1));
    writer.enqueueSample(poseSample(dwell, 12.2, 801, 1203, 51, 0.9, 0.2));
    writer.enqueueSample(poseSample(dwell.addSecs(1), 12.1, 802, 1201, 49, 0.8, 0.3));
    writer.enqueueSample(poseSample(dwell.addSecs(2), 12.4, 804, 1198, 52, 0.7, 0.1));
    writer.enqueueSample(poseSample(QDateTime(QDate(2026, 11, 1), QTime(16, 0, 0)),
                                    12.3, 830, 1200, 50, 0.1, 0.0));
    writer.flush();
    writer.shutdown();

    const TimeSeriesConditionSearchResult result = TimeSeriesQueryService::searchSimilarConditions(
        dir.path(),
        probe.toUTC().toMSecsSinceEpoch(),
        QDate(2026, 11, 11));

    QVERIFY(result.ok);
    QCOMPARE(result.hits.size(), 1);
    QCOMPARE(result.hits.first().tsUtcMs, dwell.addSecs(2).toUTC().toMSecsSinceEpoch());
    QVERIFY(result.rawHitCount >= 3);
}

void TestTimeSeriesQueryService::queriesNewChannelsAndSkipsMissingLegacyColumns()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TimeSeriesWriter writer;
    writer.initialize(testConfig(dir.path()));
    const QDateTime now = QDateTime(QDate(2026, 7, 1), QTime(8, 0, 0));
    TimeSeriesSample sample = sampleAt(now, TimeSeriesChannel::ArmTorquePercent, 11.5);
    sample.values[static_cast<int>(TimeSeriesChannel::FrontSupportForceKg)].value = 280;
    sample.values[static_cast<int>(TimeSeriesChannel::FrontSupportForceKg)].valid = true;
    writer.enqueueSample(sample);
    writer.flush();
    writer.shutdown();

    const TimeSeriesQueryResult result = TimeSeriesQueryService::query(
        dir.path(),
        now.addSecs(-2).toUTC().toMSecsSinceEpoch(),
        now.addSecs(2).toUTC().toMSecsSinceEpoch(),
        {TimeSeriesChannel::ArmTorquePercent, TimeSeriesChannel::FrontSupportForceKg});
    QVERIFY(result.ok);
    QCOMPARE(result.series.size(), 2);
    QCOMPARE(result.series.at(0).points.size(), 1);
    QCOMPARE(result.series.at(0).points.first().value, 11.5);
    QCOMPARE(result.series.at(1).points.size(), 1);
    QCOMPARE(result.series.at(1).points.first().value, 280.0);

    QTemporaryDir legacyDir;
    QVERIFY(legacyDir.isValid());
    const QString legacyPath = TimeSeriesMeta::dailyFilePath(legacyDir.path(), now.date());
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("tst_legacy_query"));
        db.setDatabaseName(legacyPath);
        QVERIFY(db.open());
        QVERIFY(QSqlQuery(db).exec(QStringLiteral(
            "CREATE TABLE metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)")));
        QVERIFY(QSqlQuery(db).exec(QStringLiteral(
            "INSERT INTO metadata(key, value) VALUES('schema_version','1')")));
        QVERIFY(QSqlQuery(db).exec(QStringLiteral(
            "CREATE TABLE samples("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "ts_utc_ms INTEGER NOT NULL,"
            "local_offset_min INTEGER NOT NULL,"
            "session_id TEXT NOT NULL,"
            "arm_tilt_x_deg REAL)")));
        QSqlQuery insert(db);
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO samples(ts_utc_ms, local_offset_min, session_id, arm_tilt_x_deg) "
            "VALUES(?,?,?,?)")));
        insert.addBindValue(now.toUTC().toMSecsSinceEpoch());
        insert.addBindValue(now.offsetFromUtc() / 60);
        insert.addBindValue(QStringLiteral("legacy"));
        insert.addBindValue(1.2);
        QVERIFY(insert.exec());
    }
    QSqlDatabase::removeDatabase(QStringLiteral("tst_legacy_query"));

    const TimeSeriesQueryResult legacy = TimeSeriesQueryService::query(
        legacyDir.path(),
        now.addSecs(-2).toUTC().toMSecsSinceEpoch(),
        now.addSecs(2).toUTC().toMSecsSinceEpoch(),
        {TimeSeriesChannel::ArmTiltXDeg, TimeSeriesChannel::ArmTorquePercent,
         TimeSeriesChannel::FrontSupportForceKg});
    QVERIFY(legacy.ok);
    QCOMPARE(legacy.series.size(), 3);
    QCOMPARE(legacy.series.at(0).points.size(), 1);
    QCOMPARE(legacy.series.at(0).points.first().value, 1.2);
    QCOMPARE(legacy.series.at(1).points.size(), 0);
    QCOMPARE(legacy.series.at(2).points.size(), 0);
}

QTEST_GUILESS_MAIN(TestTimeSeriesQueryService)
#include "tst_timeseriesqueryservice.moc"
