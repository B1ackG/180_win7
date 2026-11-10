#ifndef TIMESERIESQUERYSERVICE_H
#define TIMESERIESQUERYSERVICE_H

#include "timeseriestypes.h"

#include <QDate>
#include <QString>
#include <QVector>

class TimeSeriesQueryService
{
public:
    static TimeSeriesQueryResult query(const QString &directory,
                                       qint64 startUtcMs,
                                       qint64 endUtcMs,
                                       const QVector<TimeSeriesChannel> &channels,
                                       int maxPointsPerChannel = TimeSeriesConfig::kMaxQueryPointsPerChannel);
    static QList<QDate> availableDates(const QString &directory);
    static QVector<TimeSeriesPoint> downsampleMinMax(const QVector<TimeSeriesPoint> &points,
                                                     int maxPoints);

private:
    TimeSeriesQueryService() = delete;
};

#endif // TIMESERIESQUERYSERVICE_H
