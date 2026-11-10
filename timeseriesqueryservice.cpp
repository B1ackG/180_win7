#include "timeseriesqueryservice.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {

QList<QDate> datesForRange(qint64 startUtcMs, qint64 endUtcMs)
{
    QList<QDate> dates;
    const QDate startDate = QDateTime::fromMSecsSinceEpoch(startUtcMs, Qt::LocalTime).date();
    const QDate endDate = QDateTime::fromMSecsSinceEpoch(endUtcMs, Qt::LocalTime).date();
    if (!startDate.isValid() || !endDate.isValid()) {
        return dates;
    }
    for (QDate day = qMin(startDate, endDate); day <= qMax(startDate, endDate); day = day.addDays(1)) {
        dates.append(day);
    }
    return dates;
}

QString uniqueConnectionName()
{
    return QStringLiteral("timeseries_query_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void closeAndRemove(const QString &connectionName)
{
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

int readSampleIntervalMs(QSqlDatabase &db)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value FROM metadata WHERE key = ?"));
    query.addBindValue(QStringLiteral("sample_interval_ms"));
    if (query.exec() && query.next()) {
        bool ok = false;
        const int value = query.value(0).toInt(&ok);
        if (ok && value > 0) {
            return value;
        }
    }
    return TimeSeriesConfig::kDefaultSampleIntervalMs;
}

bool readSchemaVersion(QSqlDatabase &db, int *version, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT value FROM metadata WHERE key = 'schema_version'"))) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    if (!query.next()) {
        *version = TimeSeriesConfig::kSupportedSchemaVersion;
        return true;
    }
    bool ok = false;
    *version = query.value(0).toInt(&ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("不支持的数据库版本");
        }
        return false;
    }
    return true;
}

} // namespace

QList<QDate> TimeSeriesQueryService::availableDates(const QString &directory)
{
    QList<QDate> dates;
    QDir dir(directory);
    if (!dir.exists()) {
        return dates;
    }
    const QRegularExpression pattern(QStringLiteral("^metrics_(\\d{8})\\.db$"));
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("metrics_*.db"),
                                                  QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &info : files) {
        const QRegularExpressionMatch match = pattern.match(info.fileName());
        if (!match.hasMatch() || info.size() <= 0) {
            continue;
        }
        const QDate date = QDate::fromString(match.captured(1), QStringLiteral("yyyyMMdd"));
        if (date.isValid()) {
            dates.append(date);
        }
    }
    std::sort(dates.begin(), dates.end());
    return dates;
}

QVector<TimeSeriesPoint> TimeSeriesQueryService::downsampleMinMax(const QVector<TimeSeriesPoint> &points,
                                                                  int maxPoints)
{
    if (points.size() <= maxPoints || maxPoints < 2 || points.size() < 3) {
        return points;
    }

    const int bucketCount = qMax(1, maxPoints / 2);
    const qint64 start = points.first().tsUtcMs;
    const qint64 end = points.last().tsUtcMs;
    if (end <= start) {
        return points.mid(0, maxPoints);
    }

    QVector<TimeSeriesPoint> out;
    out.reserve(qMin(maxPoints, points.size()));
    int index = 0;
    for (int bucket = 0; bucket < bucketCount && index < points.size(); ++bucket) {
        const qint64 bucketStart = start + (end - start) * bucket / bucketCount;
        const qint64 bucketEnd = (bucket + 1 == bucketCount)
            ? (end + 1)
            : (start + (end - start) * (bucket + 1) / bucketCount);
        int minIndex = -1;
        int maxIndex = -1;
        while (index < points.size() && points.at(index).tsUtcMs < bucketEnd) {
            if (points.at(index).tsUtcMs >= bucketStart) {
                if (minIndex < 0 || points.at(index).value < points.at(minIndex).value) {
                    minIndex = index;
                }
                if (maxIndex < 0 || points.at(index).value > points.at(maxIndex).value) {
                    maxIndex = index;
                }
            }
            ++index;
        }
        if (minIndex < 0) {
            continue;
        }
        if (minIndex == maxIndex) {
            out.append(points.at(minIndex));
        } else if (points.at(minIndex).tsUtcMs < points.at(maxIndex).tsUtcMs
                   || (points.at(minIndex).tsUtcMs == points.at(maxIndex).tsUtcMs
                       && points.at(minIndex).id <= points.at(maxIndex).id)) {
            out.append(points.at(minIndex));
            out.append(points.at(maxIndex));
        } else {
            out.append(points.at(maxIndex));
            out.append(points.at(minIndex));
        }
    }
    return out;
}

TimeSeriesQueryResult TimeSeriesQueryService::query(const QString &directory,
                                                    qint64 startUtcMs,
                                                    qint64 endUtcMs,
                                                    const QVector<TimeSeriesChannel> &channels,
                                                    int maxPointsPerChannel)
{
    TimeSeriesQueryResult result;
    result.startUtcMs = startUtcMs;
    result.endUtcMs = endUtcMs;
    if (directory.isEmpty()) {
        result.ok = false;
        result.error = QStringLiteral("未配置时序数据目录");
        return result;
    }
    if (endUtcMs < startUtcMs) {
        result.ok = false;
        result.error = QStringLiteral("结束时间不能早于开始时间");
        return result;
    }
    if (channels.isEmpty()) {
        result.ok = false;
        result.error = QStringLiteral("请至少选择一个通道");
        return result;
    }

    QVector<TimeSeriesSeries> series;
    series.resize(channels.size());
    for (int i = 0; i < channels.size(); ++i) {
        series[i].channel = channels.at(i);
    }

    int sampleIntervalMs = TimeSeriesConfig::kDefaultSampleIntervalMs;
    const QList<QDate> dates = datesForRange(startUtcMs, endUtcMs);
    for (const QDate &date : dates) {
        const QString path = TimeSeriesMeta::dailyFilePath(directory, date);
        if (!QFile::exists(path)) {
            continue;
        }

        const QString connectionName = uniqueConnectionName();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(path);
            if (!db.open()) {
                closeAndRemove(connectionName);
                continue;
            }

            int version = TimeSeriesConfig::kSupportedSchemaVersion;
            QString versionError;
            if (!readSchemaVersion(db, &version, &versionError)
                || version > TimeSeriesConfig::kSupportedSchemaVersion) {
                closeAndRemove(connectionName);
                result.ok = false;
                result.error = QStringLiteral("不支持的数据库版本");
                return result;
            }

            sampleIntervalMs = readSampleIntervalMs(db);

            QStringList columnSql;
            columnSql << QStringLiteral("id") << QStringLiteral("ts_utc_ms");
            for (TimeSeriesChannel channel : channels) {
                columnSql << TimeSeriesMeta::columnName(channel);
            }
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "SELECT %1 FROM samples WHERE ts_utc_ms >= ? AND ts_utc_ms <= ? "
                "ORDER BY ts_utc_ms, id")
                              .arg(columnSql.join(QLatin1Char(','))));
            query.addBindValue(startUtcMs);
            query.addBindValue(endUtcMs);
            if (!query.exec()) {
                closeAndRemove(connectionName);
                result.ok = false;
                result.error = query.lastError().text();
                return result;
            }

            while (query.next()) {
                const qint64 id = query.value(0).toLongLong();
                const qint64 ts = query.value(1).toLongLong();
                for (int i = 0; i < channels.size(); ++i) {
                    const QVariant raw = query.value(2 + i);
                    if (!raw.isValid() || raw.isNull()) {
                        continue;
                    }
                    bool ok = false;
                    const double value = raw.toDouble(&ok);
                    if (!ok || !TimeSeriesMeta::isFiniteNumber(value)) {
                        continue;
                    }
                    TimeSeriesPoint point;
                    point.tsUtcMs = ts;
                    point.id = id;
                    point.value = value;
                    series[i].points.append(point);
                }
            }
        }
        closeAndRemove(connectionName);
    }

    result.sampleIntervalMs = sampleIntervalMs;
    for (TimeSeriesSeries &item : series) {
        item.points = downsampleMinMax(item.points, maxPointsPerChannel);
        result.series.append(item);
    }
    return result;
}
