#ifndef TIMESERIESMANAGER_H
#define TIMESERIESMANAGER_H

#include "timeseriestypes.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVector>

class TimeSeriesWriter;

class TimeSeriesManager : public QObject
{
    Q_OBJECT

public:
    explicit TimeSeriesManager(QObject *parent = nullptr);
    ~TimeSeriesManager() override;

    bool initialize(const TimeSeriesConfig &config, QString *error = nullptr);
    void updateValue(TimeSeriesChannel channel, double value);
    void invalidateChannel(TimeSeriesChannel channel);
    void invalidateSource(TimeSeriesSource source);
    void startSampling();
    void stopAndFlush(int timeoutMs = 5000);
    bool isRecording() const;
    bool isEnabled() const { return m_config.enabled; }
    QString databaseDirectory() const { return m_config.databaseDirectory; }
    TimeSeriesConfig config() const { return m_config; }

signals:
    void recordingError(const QString &message);
    void recordingStateChanged(bool recording, const QString &message);
    void sampleReady(const TimeSeriesSample &sample);

private slots:
    void onSampleTimeout();
    void onWriterInitialized(bool ok, const QString &message);
    void onWriterError(const QString &message);
    void onRecordingPaused(const QString &message);
    void onRecordingResumed(const QString &message);

private:
    TimeSeriesSample snapshotSample() const;
    void setChannelValue(TimeSeriesChannel channel, double value, bool valid);

    TimeSeriesConfig m_config;
    QVector<TimeSeriesValue> m_snapshot;
    QTimer *m_sampleTimer = nullptr;
    QThread *m_writerThread = nullptr;
    TimeSeriesWriter *m_writer = nullptr;
    bool m_initialized = false;
    bool m_recording = false;
    bool m_stopping = false;
};

#endif // TIMESERIESMANAGER_H
