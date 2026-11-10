#ifndef TIMESERIESTYPES_H
#define TIMESERIESTYPES_H

#include <QDate>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <cmath>

enum class TimeSeriesChannel {
    J1AngleDeg = 0,
    J2HeightMm,
    J3TotalLengthMm,
    J4AngleDeg,
    CounterweightHeightMm,
    ColumnTorquePercent,
    CounterweightTorquePercent,
    ArmTiltXDeg,
    ArmTiltYDeg,
    PlaneHeightMm,
    LoadKg,
    TotalPowerW,
    ChassisTiltXDeg,
    ChassisTiltYDeg,
    Count
};

enum class TimeSeriesSource {
    Main = 0,
    Agv
};

struct TimeSeriesValue {
    double value = 0.0;
    bool valid = false;
};

struct TimeSeriesSample {
    qint64 tsUtcMs = 0;
    int localOffsetMinutes = 0;
    QString sessionId;
    QVector<TimeSeriesValue> values;

    TimeSeriesSample()
    {
        values.resize(static_cast<int>(TimeSeriesChannel::Count));
    }

    bool hasAnyValid() const
    {
        for (const TimeSeriesValue &item : values) {
            if (item.valid) {
                return true;
            }
        }
        return false;
    }

    QDate localDate() const
    {
        const QDateTime utc = QDateTime::fromMSecsSinceEpoch(tsUtcMs, Qt::UTC);
        return utc.toOffsetFromUtc(localOffsetMinutes * 60).date();
    }
};

struct TimeSeriesPoint {
    qint64 tsUtcMs = 0;
    qint64 id = 0;
    double value = 0.0;
};

struct TimeSeriesSeries {
    TimeSeriesChannel channel = TimeSeriesChannel::J1AngleDeg;
    QVector<TimeSeriesPoint> points;
};

struct TimeSeriesQueryResult {
    bool ok = true;
    QString error;
    QVector<TimeSeriesSeries> series;
    qint64 startUtcMs = 0;
    qint64 endUtcMs = 0;
    int sampleIntervalMs = 1000;
};

struct TimeSeriesConfig {
    bool enabled = true;
    int sampleIntervalMs = 1000;
    int flushIntervalMs = 5000;
    int retentionDays = 730;
    int reserveFreeMb = 2048;
    int maxQueueRows = 300;
    QString databaseDirectory;
    QString sessionId;
    QString appVersion = QStringLiteral("180_win7");

    static constexpr int kMinSampleIntervalMs = 200;
    static constexpr int kMaxSampleIntervalMs = 60000;
    static constexpr int kMinFlushIntervalMs = 1000;
    static constexpr int kMaxFlushIntervalMs = 60000;
    static constexpr int kMinRetentionDays = 366;
    static constexpr int kMaxRetentionDays = 3650;
    static constexpr int kMinReserveFreeMb = 512;
    static constexpr int kMinQueueRows = 30;
    static constexpr int kMaxQueueRows = 3600;
    static constexpr int kDefaultSampleIntervalMs = 1000;
    static constexpr int kDefaultFlushIntervalMs = 5000;
    static constexpr int kDefaultRetentionDays = 730;
    static constexpr int kDefaultReserveFreeMb = 2048;
    static constexpr int kDefaultMaxQueueRows = 300;
    static constexpr int kSupportedSchemaVersion = 1;
    static constexpr int kInsertBatchTriggerRows = 20;
    static constexpr int kMaxQueryPointsPerChannel = 4000;

    static TimeSeriesConfig fromIni(const QString &iniPath = QStringLiteral("config.ini"));
    static QString defaultDirectory();
    QString resolvedDirectory() const;
    TimeSeriesConfig normalized() const;
};

namespace TimeSeriesMeta {

int channelCount();
TimeSeriesChannel channelAt(int index);
QString columnName(TimeSeriesChannel channel);
QString displayName(TimeSeriesChannel channel);
QString unit(TimeSeriesChannel channel);
TimeSeriesSource source(TimeSeriesChannel channel);
bool isFiniteNumber(double value);
int displayDecimals(TimeSeriesChannel channel);
QVector<TimeSeriesChannel> allChannels();
QString dailyFileName(const QDate &date);
QString dailyFilePath(const QString &directory, const QDate &date);

} // namespace TimeSeriesMeta

inline uint qHash(TimeSeriesChannel channel, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(channel), seed);
}

Q_DECLARE_METATYPE(TimeSeriesSample)
Q_DECLARE_METATYPE(TimeSeriesConfig)

#endif // TIMESERIESTYPES_H
