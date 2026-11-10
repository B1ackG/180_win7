#ifndef TIMESERIESWRITER_H
#define TIMESERIESWRITER_H

#include "timeseriestypes.h"

#include <QDate>
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <functional>

class TimeSeriesWriter : public QObject
{
    Q_OBJECT

public:
    explicit TimeSeriesWriter(QObject *parent = nullptr);
    ~TimeSeriesWriter() override;

public slots:
    void initialize(TimeSeriesConfig config);
    void enqueueSample(TimeSeriesSample sample);
    void flush();
    void shutdown();

signals:
    void initialized(bool ok, const QString &message);
    void writeError(const QString &message);
    void recordingPaused(const QString &message);
    void recordingResumed(const QString &message);
    void stopped();

private slots:
    void onFlushTimeout();
    void onDiskCheckTimeout();

private:
    QSqlDatabase database() const;
    bool openDatabaseForDate(const QDate &localDate, QString *error);
    bool createOrValidateSchema(QString *error);
    bool insertBatch(const QVector<TimeSeriesSample> &batch, QString *error);
    bool insertBatchForDate(const QDate &localDate,
                            const QVector<TimeSeriesSample> &batch,
                            QString *error);
    void removeExpiredDailyDatabases(const QDate &today);
    bool hasReservedDiskSpace() const;
    void closeDatabase();
    bool runQuickCheck(QString *error);
    bool quarantineCurrentDatabase(QString *error);
    bool execWithRetry(const std::function<bool(QString *error)> &op, QString *error);
    void setPaused(bool paused, const QString &message);
    void writeMetadataValue(const QString &key, const QString &value);
    QString readMetadataValue(const QString &key) const;
    static void removeSidecarFiles(const QString &dbPath);

    TimeSeriesConfig m_config;
    QString m_connectionName;
    QDate m_openDate;
    QVector<TimeSeriesSample> m_pending;
    QTimer *m_flushTimer = nullptr;
    QTimer *m_diskTimer = nullptr;
    bool m_ready = false;
    bool m_paused = false;
    bool m_overflowNotified = false;
    bool m_lowDiskNotified = false;
    std::atomic<bool> m_shuttingDown {false};
};

#endif // TIMESERIESWRITER_H
