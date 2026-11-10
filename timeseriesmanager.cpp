#include "timeseriesmanager.h"
#include "timeserieswriter.h"

#include <QDateTime>
#include <QMetaType>
#include <QUuid>

TimeSeriesManager::TimeSeriesManager(QObject *parent)
    : QObject(parent)
    , m_snapshot(TimeSeriesMeta::channelCount())
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<TimeSeriesSample>("TimeSeriesSample");
        qRegisterMetaType<TimeSeriesConfig>("TimeSeriesConfig");
        registered = true;
    }

    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setTimerType(Qt::PreciseTimer);
    connect(m_sampleTimer, &QTimer::timeout, this, &TimeSeriesManager::onSampleTimeout);
}

TimeSeriesManager::~TimeSeriesManager()
{
    stopAndFlush();
}

bool TimeSeriesManager::initialize(const TimeSeriesConfig &config, QString *error)
{
    if (m_initialized || m_stopping) {
        return m_initialized;
    }

    m_config = config.normalized();
    if (m_config.sessionId.isEmpty()) {
        m_config.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (!m_config.enabled) {
        m_initialized = true;
        m_recording = false;
        return true;
    }

    m_writerThread = new QThread(this);
    m_writer = new TimeSeriesWriter();
    m_writer->moveToThread(m_writerThread);

    connect(m_writerThread, &QThread::finished, m_writer, &QObject::deleteLater);
    connect(this, &TimeSeriesManager::sampleReady,
            m_writer, &TimeSeriesWriter::enqueueSample, Qt::QueuedConnection);
    connect(m_writer, &TimeSeriesWriter::initialized,
            this, &TimeSeriesManager::onWriterInitialized, Qt::QueuedConnection);
    connect(m_writer, &TimeSeriesWriter::writeError,
            this, &TimeSeriesManager::onWriterError, Qt::QueuedConnection);
    connect(m_writer, &TimeSeriesWriter::recordingPaused,
            this, &TimeSeriesManager::onRecordingPaused, Qt::QueuedConnection);
    connect(m_writer, &TimeSeriesWriter::recordingResumed,
            this, &TimeSeriesManager::onRecordingResumed, Qt::QueuedConnection);

    m_writerThread->start();
    const bool invoked = QMetaObject::invokeMethod(m_writer, "initialize",
                                                   Qt::QueuedConnection,
                                                   Q_ARG(TimeSeriesConfig, m_config));
    if (!invoked) {
        if (error) {
            *error = QStringLiteral("无法启动时序写入线程");
        }
        return false;
    }

    m_initialized = true;
    m_recording = true;
    startSampling();
    return true;
}

void TimeSeriesManager::updateValue(TimeSeriesChannel channel, double value)
{
    if (!TimeSeriesMeta::isFiniteNumber(value)) {
        setChannelValue(channel, 0.0, false);
        return;
    }
    setChannelValue(channel, value, true);
}

void TimeSeriesManager::invalidateChannel(TimeSeriesChannel channel)
{
    setChannelValue(channel, 0.0, false);
}

void TimeSeriesManager::invalidateSource(TimeSeriesSource source)
{
    for (TimeSeriesChannel channel : TimeSeriesMeta::allChannels()) {
        if (TimeSeriesMeta::source(channel) == source) {
            invalidateChannel(channel);
        }
    }
}

void TimeSeriesManager::startSampling()
{
    if (!m_config.enabled || !m_sampleTimer) {
        return;
    }
    if (!m_sampleTimer->isActive()) {
        m_sampleTimer->start(m_config.sampleIntervalMs);
    }
}

void TimeSeriesManager::stopAndFlush(int timeoutMs)
{
    if (m_stopping) {
        return;
    }
    m_stopping = true;
    if (m_sampleTimer) {
        m_sampleTimer->stop();
    }
    m_recording = false;

    if (!m_writer || !m_writerThread) {
        m_initialized = false;
        m_stopping = false;
        return;
    }

    QMetaObject::invokeMethod(m_writer, "shutdown", Qt::QueuedConnection);
    m_writerThread->quit();
    if (!m_writerThread->wait(timeoutMs)) {
        m_writerThread->quit();
        m_writerThread->wait(500);
    }
    m_writer = nullptr;
    m_writerThread->deleteLater();
    m_writerThread = nullptr;
    m_initialized = false;
    m_stopping = false;
}

bool TimeSeriesManager::isRecording() const
{
    return m_config.enabled && m_recording && m_initialized;
}

void TimeSeriesManager::onSampleTimeout()
{
    if (!m_recording || !m_config.enabled) {
        return;
    }
    const TimeSeriesSample sample = snapshotSample();
    if (!sample.hasAnyValid()) {
        return;
    }
    emit sampleReady(sample);
}

void TimeSeriesManager::onWriterInitialized(bool ok, const QString &message)
{
    m_recording = ok;
    emit recordingStateChanged(ok, message);
    if (!ok && !message.isEmpty()) {
        emit recordingError(message);
    }
}

void TimeSeriesManager::onWriterError(const QString &message)
{
    emit recordingError(message);
}

void TimeSeriesManager::onRecordingPaused(const QString &message)
{
    m_recording = false;
    emit recordingStateChanged(false, message);
    emit recordingError(message);
}

void TimeSeriesManager::onRecordingResumed(const QString &message)
{
    m_recording = true;
    emit recordingStateChanged(true, message);
}

TimeSeriesSample TimeSeriesManager::snapshotSample() const
{
    TimeSeriesSample sample;
    const QDateTime now = QDateTime::currentDateTime();
    sample.tsUtcMs = now.toUTC().toMSecsSinceEpoch();
    sample.localOffsetMinutes = now.offsetFromUtc() / 60;
    sample.sessionId = m_config.sessionId;
    sample.values = m_snapshot;
    return sample;
}

void TimeSeriesManager::setChannelValue(TimeSeriesChannel channel, double value, bool valid)
{
    const int index = static_cast<int>(channel);
    if (index < 0 || index >= m_snapshot.size()) {
        return;
    }
    m_snapshot[index].value = value;
    m_snapshot[index].valid = valid;
}
