#include "timeseriesqueryservice.h"

#include <algorithm>
#include <functional>
#include <QDate>
#include <QVariant>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStringList>
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

namespace {

bool aggregatesForDate(const QString &directory,
                       const QDate &date,
                       const QVector<TimeSeriesChannel> &channels,
                       TimeSeriesCompareStat stat,
                       QVector<TimeSeriesValue> *out,
                       QString *error)
{
    out->fill(TimeSeriesValue(), channels.size());
    if (!date.isValid()) {
        return true;
    }
    const QString path = TimeSeriesMeta::dailyFilePath(directory, date);
    if (!QFile::exists(path)) {
        return true;
    }

    const QString connectionName = uniqueConnectionName();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            closeAndRemove(connectionName);
            return true;
        }

        int version = TimeSeriesConfig::kSupportedSchemaVersion;
        QString versionError;
        if (!readSchemaVersion(db, &version, &versionError)
            || version > TimeSeriesConfig::kSupportedSchemaVersion) {
            closeAndRemove(connectionName);
            if (error) {
                *error = QStringLiteral("不支持的数据库版本");
            }
            return false;
        }

        const QString sqlFn = TimeSeriesMeta::compareStatSql(stat);
        QStringList columnSql;
        for (TimeSeriesChannel channel : channels) {
            columnSql << QStringLiteral("%1(%2)").arg(sqlFn, TimeSeriesMeta::columnName(channel));
        }
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT %1 FROM samples").arg(columnSql.join(QLatin1Char(','))))) {
            closeAndRemove(connectionName);
            if (error) {
                *error = query.lastError().text();
            }
            return false;
        }
        if (query.next()) {
            for (int i = 0; i < channels.size(); ++i) {
                const QVariant raw = query.value(i);
                if (!raw.isValid() || raw.isNull()) {
                    continue;
                }
                bool ok = false;
                const double value = raw.toDouble(&ok);
                if (!ok || !TimeSeriesMeta::isFiniteNumber(value)) {
                    continue;
                }
                (*out)[i].value = value;
                (*out)[i].valid = true;
            }
        }
    }
    closeAndRemove(connectionName);
    return true;
}

void appendIfValid(QVector<TimeSeriesPoint> *points, int dayIndex, const TimeSeriesValue &value)
{
    if (!value.valid) {
        return;
    }
    TimeSeriesPoint point;
    point.tsUtcMs = dayIndex;
    point.id = dayIndex + 1;
    point.value = value.value;
    points->append(point);
}

} // namespace

TimeSeriesCompareResult TimeSeriesQueryService::queryDailyCompare(const QString &directory,
                                                                  const QVector<TimeSeriesChannel> &channels,
                                                                  TimeSeriesCompareKind kind,
                                                                  const QDate &asOf,
                                                                  TimeSeriesCompareStat stat)
{
    TimeSeriesCompareResult result;
    result.kind = kind;
    result.stat = stat;
    result.statLabel = TimeSeriesMeta::compareStatLabel(stat);
    if (directory.isEmpty()) {
        result.ok = false;
        result.error = QStringLiteral("未配置时序数据目录");
        return result;
    }
    if (channels.isEmpty()) {
        result.ok = false;
        result.error = QStringLiteral("请至少选择一个通道");
        return result;
    }

    const QDate today = asOf.isValid() ? asOf : QDate::currentDate();
    QVector<QDate> currentDates;
    QVector<QDate> previousDates;
    if (kind == TimeSeriesCompareKind::MonthOverMonth) {
        result.currentLabel = QStringLiteral("本月");
        result.previousLabel = QStringLiteral("上月");
        const int dayCount = today.day();
        const QDate thisStart(today.year(), today.month(), 1);
        const QDate prevLast = thisStart.addDays(-1);
        for (int day = 1; day <= dayCount; ++day) {
            currentDates.append(QDate(today.year(), today.month(), day));
            previousDates.append(QDate(prevLast.year(), prevLast.month(), day));
            result.tickLabels.append(QString::number(day));
        }
    } else {
        result.currentLabel = QStringLiteral("今年");
        result.previousLabel = QStringLiteral("去年");
        const QDate yearStart(today.year(), 1, 1);
        const int dayCount = yearStart.daysTo(today) + 1;
        for (int i = 0; i < dayCount; ++i) {
            const QDate current = yearStart.addDays(i);
            currentDates.append(current);
            previousDates.append(QDate(today.year() - 1, current.month(), current.day()));
            result.tickLabels.append(current.toString(QStringLiteral("MM-dd")));
        }
    }
    result.dayCount = result.tickLabels.size();
    result.series.resize(channels.size());
    for (int i = 0; i < channels.size(); ++i) {
        result.series[i].channel = channels.at(i);
    }

    QVector<TimeSeriesValue> values;
    values.resize(channels.size());
    for (int dayIndex = 0; dayIndex < result.dayCount; ++dayIndex) {
        QString error;
        if (!aggregatesForDate(directory, currentDates.at(dayIndex), channels, stat, &values, &error)) {
            result.ok = false;
            result.error = error;
            return result;
        }
        for (int i = 0; i < channels.size(); ++i) {
            appendIfValid(&result.series[i].currentPoints, dayIndex, values.at(i));
        }
        if (!aggregatesForDate(directory, previousDates.at(dayIndex), channels, stat, &values, &error)) {
            result.ok = false;
            result.error = error;
            return result;
        }
        for (int i = 0; i < channels.size(); ++i) {
            appendIfValid(&result.series[i].previousPoints, dayIndex, values.at(i));
        }
    }
    return result;
}

namespace {

TimeSeriesValue valueFromVariant(const QVariant &raw)
{
    TimeSeriesValue value;
    if (!raw.isValid() || raw.isNull()) {
        return value;
    }
    bool ok = false;
    const double number = raw.toDouble(&ok);
    if (!ok || !TimeSeriesMeta::isFiniteNumber(number)) {
        return value;
    }
    value.value = number;
    value.valid = true;
    return value;
}

QString bucketSql(TimeSeriesChannel channel)
{
    const QString column = TimeSeriesMeta::columnName(channel);
    const int quantum = TimeSeriesMeta::conditionQuantum(channel);
    if (quantum <= 1) {
        return QStringLiteral("CAST(ROUND(%1) AS INTEGER)").arg(column);
    }
    return QStringLiteral("CAST(ROUND(%1 / %2.0) AS INTEGER)").arg(column).arg(quantum);
}

QVector<TimeSeriesValue> valuesForChannels(const TimeSeriesSample &sample,
                                           const QVector<TimeSeriesChannel> &channels)
{
    QVector<TimeSeriesValue> values;
    values.reserve(channels.size());
    for (TimeSeriesChannel channel : channels) {
        const int index = static_cast<int>(channel);
        if (index >= 0 && index < sample.values.size()) {
            values.append(sample.values.at(index));
        } else {
            values.append(TimeSeriesValue());
        }
    }
    return values;
}

bool readNearestSample(const QString &path,
                       qint64 probeTsUtcMs,
                       int windowMs,
                       TimeSeriesSample *out)
{
    const QString connectionName = uniqueConnectionName();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            closeAndRemove(connectionName);
            return false;
        }

        int version = TimeSeriesConfig::kSupportedSchemaVersion;
        QString versionError;
        if (!readSchemaVersion(db, &version, &versionError)
            || version > TimeSeriesConfig::kSupportedSchemaVersion) {
            closeAndRemove(connectionName);
            return false;
        }

        QStringList columns;
        columns << QStringLiteral("ts_utc_ms")
                << QStringLiteral("local_offset_min")
                << QStringLiteral("session_id");
        for (int i = 0; i < TimeSeriesMeta::channelCount(); ++i) {
            columns << TimeSeriesMeta::columnName(TimeSeriesMeta::channelAt(i));
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
                          "SELECT %1 FROM samples WHERE ts_utc_ms >= ? AND ts_utc_ms <= ? "
                          "ORDER BY ABS(ts_utc_ms - ?) ASC, id ASC LIMIT 1")
                          .arg(columns.join(QLatin1Char(','))));
        query.addBindValue(probeTsUtcMs - windowMs);
        query.addBindValue(probeTsUtcMs + windowMs);
        query.addBindValue(probeTsUtcMs);
        if (!query.exec() || !query.next()) {
            closeAndRemove(connectionName);
            return false;
        }

        out->tsUtcMs = query.value(0).toLongLong();
        out->localOffsetMinutes = query.value(1).toInt();
        out->sessionId = query.value(2).toString();
        out->values.resize(TimeSeriesMeta::channelCount());
        for (int i = 0; i < TimeSeriesMeta::channelCount(); ++i) {
            out->values[i] = valueFromVariant(query.value(3 + i));
        }
    }
    closeAndRemove(connectionName);
    return out->tsUtcMs > 0;
}

void appendMatchesFromDate(const QString &path,
                           const QVector<TimeSeriesChannel> &workChannels,
                           const QVector<qint64> &workBuckets,
                           const QVector<TimeSeriesChannel> &stateChannels,
                           qint64 excludeStartMs,
                           qint64 excludeEndMs,
                           int limit,
                           QVector<TimeSeriesConditionHit> *hits)
{
    const QString connectionName = uniqueConnectionName();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            closeAndRemove(connectionName);
            return;
        }

        int version = TimeSeriesConfig::kSupportedSchemaVersion;
        QString versionError;
        if (!readSchemaVersion(db, &version, &versionError)
            || version > TimeSeriesConfig::kSupportedSchemaVersion) {
            closeAndRemove(connectionName);
            return;
        }

        QStringList columns;
        columns << QStringLiteral("ts_utc_ms");
        for (TimeSeriesChannel channel : workChannels) {
            columns << TimeSeriesMeta::columnName(channel);
        }
        for (TimeSeriesChannel channel : stateChannels) {
            columns << TimeSeriesMeta::columnName(channel);
        }

        QStringList whereSql;
        for (TimeSeriesChannel channel : workChannels) {
            whereSql << QStringLiteral("%1 IS NOT NULL AND %2 = ?")
                            .arg(TimeSeriesMeta::columnName(channel), bucketSql(channel));
        }
        whereSql << QStringLiteral("(ts_utc_ms < ? OR ts_utc_ms > ?)");

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
                          "SELECT %1 FROM samples WHERE %2 ORDER BY ts_utc_ms DESC LIMIT ?")
                          .arg(columns.join(QLatin1Char(',')),
                               whereSql.join(QStringLiteral(" AND "))));
        for (qint64 bucket : workBuckets) {
            query.addBindValue(bucket);
        }
        query.addBindValue(excludeStartMs);
        query.addBindValue(excludeEndMs);
        query.addBindValue(limit);
        if (!query.exec()) {
            closeAndRemove(connectionName);
            return;
        }

        while (query.next()) {
            TimeSeriesConditionHit hit;
            hit.tsUtcMs = query.value(0).toLongLong();
            hit.workValues.resize(workChannels.size());
            hit.stateValues.resize(stateChannels.size());
            for (int i = 0; i < workChannels.size(); ++i) {
                hit.workValues[i] = valueFromVariant(query.value(1 + i));
            }
            for (int i = 0; i < stateChannels.size(); ++i) {
                hit.stateValues[i] = valueFromVariant(query.value(1 + workChannels.size() + i));
            }
            hits->append(hit);
        }
    }
    closeAndRemove(connectionName);
}

} // namespace

TimeSeriesConditionSearchResult TimeSeriesQueryService::searchSimilarConditions(
    const QString &directory,
    qint64 probeTsUtcMs,
    const QDate &asOf,
    int lookbackDays)
{
    TimeSeriesConditionSearchResult result;
    result.probeTsUtcMs = probeTsUtcMs;
    result.lookbackDays = qMax(1, lookbackDays);
    result.stateChannels = TimeSeriesMeta::conditionStateChannels();
    if (directory.isEmpty()) {
        result.ok = false;
        result.error = QStringLiteral("未配置时序数据目录");
        return result;
    }
    if (probeTsUtcMs <= 0) {
        result.ok = false;
        result.error = QStringLiteral("请先在曲线上点游标");
        return result;
    }

    const QDateTime probeLocal = QDateTime::fromMSecsSinceEpoch(probeTsUtcMs, Qt::LocalTime);
    TimeSeriesSample probe;
    bool foundProbe = false;
    for (int delta = 0; delta <= 1 && !foundProbe; ++delta) {
        const QList<QDate> candidates = {
            probeLocal.date().addDays(-delta),
            probeLocal.date().addDays(delta)
        };
        for (const QDate &date : candidates) {
            const QString path = TimeSeriesMeta::dailyFilePath(directory, date);
            if (!QFile::exists(path)) {
                continue;
            }
            if (readNearestSample(path, probeTsUtcMs, TimeSeriesConfig::kConditionProbeWindowMs, &probe)) {
                foundProbe = true;
                break;
            }
        }
    }
    if (!foundProbe) {
        result.ok = false;
        result.error = QStringLiteral("游标附近没有采样，请点在曲线上");
        return result;
    }
    result.probeTsUtcMs = probe.tsUtcMs;

    const QVector<TimeSeriesChannel> allWork = TimeSeriesMeta::conditionWorkChannels();
    QVector<qint64> workBuckets;
    for (TimeSeriesChannel channel : allWork) {
        const int index = static_cast<int>(channel);
        if (index < 0 || index >= probe.values.size() || !probe.values.at(index).valid) {
            continue;
        }
        result.workChannels.append(channel);
        workBuckets.append(TimeSeriesMeta::conditionBucket(
            probe.values.at(index).value, TimeSeriesMeta::conditionQuantum(channel)));
    }
    if (result.workChannels.size() < TimeSeriesConfig::kConditionMinWorkKeys) {
        result.ok = false;
        result.error = QStringLiteral("游标处作业通道不足，至少需要悬臂角/升降/伸展/末端角/配重缸/装配面/负载中的3项");
        return result;
    }
    result.probeWork = valuesForChannels(probe, result.workChannels);
    result.probeState = valuesForChannels(probe, result.stateChannels);

    const QDate today = asOf.isValid() ? asOf : QDate::currentDate();
    const QDate oldest = today.addDays(-result.lookbackDays);
    QList<QDate> dates = availableDates(directory);
    std::sort(dates.begin(), dates.end(), std::greater<QDate>());

    const qint64 excludeStart = probe.tsUtcMs - TimeSeriesConfig::kConditionProbeWindowMs;
    const qint64 excludeEnd = probe.tsUtcMs + TimeSeriesConfig::kConditionProbeWindowMs;
    QVector<TimeSeriesConditionHit> rawHits;
    for (const QDate &date : dates) {
        if (date > today || date < oldest) {
            continue;
        }
        const QString path = TimeSeriesMeta::dailyFilePath(directory, date);
        if (!QFile::exists(path)) {
            continue;
        }
        ++result.scannedDays;
        appendMatchesFromDate(path,
                              result.workChannels,
                              workBuckets,
                              result.stateChannels,
                              excludeStart,
                              excludeEnd,
                              200,
                              &rawHits);
        result.rawHitCount = rawHits.size();
        if (rawHits.size() >= TimeSeriesConfig::kConditionMaxMatches * 4) {
            break;
        }
    }

    std::sort(rawHits.begin(), rawHits.end(),
              [](const TimeSeriesConditionHit &left, const TimeSeriesConditionHit &right) {
                  return left.tsUtcMs > right.tsUtcMs;
              });
    qint64 lastKept = -1;
    for (const TimeSeriesConditionHit &hit : rawHits) {
        if (lastKept >= 0 && lastKept - hit.tsUtcMs < TimeSeriesConfig::kConditionClusterMs) {
            continue;
        }
        result.hits.append(hit);
        lastKept = hit.tsUtcMs;
        if (result.hits.size() >= TimeSeriesConfig::kConditionMaxMatches) {
            break;
        }
    }
    return result;
}
