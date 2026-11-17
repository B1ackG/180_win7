#include "timeserieswriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStorageInfo>
#include <QThread>
#include <QUuid>
#include <QVariant>
#include <functional>

namespace {

QStringList sidecarPaths(const QString &dbPath)
{
    return {
        dbPath,
        dbPath + QStringLiteral("-wal"),
        dbPath + QStringLiteral("-shm")
    };
}

QString columnListSql()
{
    QStringList names;
    names << QStringLiteral("ts_utc_ms")
          << QStringLiteral("local_offset_min")
          << QStringLiteral("session_id");
    for (TimeSeriesChannel channel : TimeSeriesMeta::allChannels()) {
        names << TimeSeriesMeta::columnName(channel);
    }
    return names.join(QLatin1Char(','));
}

QString placeholdersSql()
{
    const int count = 3 + TimeSeriesMeta::channelCount();
    QStringList marks;
    marks.reserve(count);
    for (int i = 0; i < count; ++i) {
        marks << QStringLiteral("?");
    }
    return marks.join(QLatin1Char(','));
}

} // namespace

TimeSeriesWriter::TimeSeriesWriter(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("timeseries_writer_%1")
                           .arg(QString::number(reinterpret_cast<quintptr>(this), 16)))
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(false);
    connect(m_flushTimer, &QTimer::timeout, this, &TimeSeriesWriter::onFlushTimeout);

    m_diskTimer = new QTimer(this);
    m_diskTimer->setSingleShot(false);
    m_diskTimer->setInterval(60000);
    connect(m_diskTimer, &QTimer::timeout, this, &TimeSeriesWriter::onDiskCheckTimeout);
}

TimeSeriesWriter::~TimeSeriesWriter()
{
    m_shuttingDown.store(true);
    if (!m_pending.isEmpty()) {
        QString error;
        insertBatch(m_pending, &error);
        m_pending.clear();
    }
    closeDatabase();
}

void TimeSeriesWriter::initialize(TimeSeriesConfig config)
{
    m_config = config.normalized();
    m_shuttingDown.store(false);
    m_paused = false;
    m_overflowNotified = false;
    m_lowDiskNotified = false;
    m_ready = false;

    QDir dir(m_config.databaseDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit initialized(false, QStringLiteral("无法创建时序数据目录"));
        return;
    }

    QString error;
    if (!openDatabaseForDate(QDate::currentDate(), &error)) {
        emit initialized(false, error);
        return;
    }

    removeExpiredDailyDatabases(QDate::currentDate());
    m_ready = true;
    m_flushTimer->start(m_config.flushIntervalMs);
    m_diskTimer->start();
    onDiskCheckTimeout();
    emit initialized(true, QString());
}

void TimeSeriesWriter::enqueueSample(TimeSeriesSample sample)
{
    if (m_shuttingDown.load() || !m_ready || m_paused) {
        return;
    }
    if (!sample.hasAnyValid()) {
        return;
    }
    if (sample.values.size() != TimeSeriesMeta::channelCount()) {
        sample.values.resize(TimeSeriesMeta::channelCount());
    }

    if (m_pending.size() >= m_config.maxQueueRows) {
        if (!m_overflowNotified) {
            m_overflowNotified = true;
            emit writeError(QStringLiteral("时序数据记录已暂停：写入队列已满"));
        }
        return;
    }

    m_pending.append(sample);
    m_overflowNotified = false;
    if (m_pending.size() >= TimeSeriesConfig::kInsertBatchTriggerRows) {
        flush();
    }
}

void TimeSeriesWriter::flush()
{
    if (m_pending.isEmpty()) {
        return;
    }

    QVector<TimeSeriesSample> batch = m_pending;
    m_pending.clear();

    QString error;
    if (!insertBatch(batch, &error)) {
        emit writeError(error.isEmpty() ? QStringLiteral("时序数据写入失败") : error);
    }
}

void TimeSeriesWriter::shutdown()
{
    m_shuttingDown.store(true);
    if (m_flushTimer) {
        m_flushTimer->stop();
    }
    if (m_diskTimer) {
        m_diskTimer->stop();
    }
    flush();
    closeDatabase();
    m_ready = false;
    emit stopped();
}

void TimeSeriesWriter::onFlushTimeout()
{
    flush();
}

void TimeSeriesWriter::onDiskCheckTimeout()
{
    if (!m_ready && !m_paused) {
        return;
    }
    if (!hasReservedDiskSpace()) {
        if (!m_paused) {
            flush();
            setPaused(true, QStringLiteral("时序数据记录已暂停：磁盘空间不足"));
        }
        return;
    }
    if (m_paused && m_lowDiskNotified) {
        setPaused(false, QStringLiteral("时序数据记录已恢复"));
    }
}

QSqlDatabase TimeSeriesWriter::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool TimeSeriesWriter::openDatabaseForDate(const QDate &localDate, QString *error)
{
    if (!localDate.isValid()) {
        if (error) {
            *error = QStringLiteral("无效的采样日期");
        }
        return false;
    }

    if (m_ready && m_openDate == localDate && database().isOpen()) {
        return true;
    }

    closeDatabase();

    const QString path = TimeSeriesMeta::dailyFilePath(m_config.databaseDirectory, localDate);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    const bool opened = db.open();
    if (!opened) {
        const QString openError = db.lastError().text();
        db = QSqlDatabase();
        if (QSqlDatabase::contains(m_connectionName)) {
            QSqlDatabase::removeDatabase(m_connectionName);
        }
        if (QFile::exists(path) && QFileInfo(path).size() > 0) {
            m_openDate = localDate;
            QString quarantineError;
            if (quarantineCurrentDatabase(&quarantineError)) {
                emit writeError(QStringLiteral("时序数据库已损坏，已隔离并重建当天文件"));
                return openDatabaseForDate(localDate, error);
            }
        }
        if (error) {
            *error = QStringLiteral("无法打开时序数据库：%1").arg(openError);
        }
        return false;
    }

    {
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
        pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
    }

    m_openDate = localDate;

    QString checkError;
    if (QFile::exists(path) && QFileInfo(path).size() > 0 && !runQuickCheck(&checkError)) {
        db = QSqlDatabase();
        if (!quarantineCurrentDatabase(&checkError)) {
            if (error) {
                *error = checkError;
            }
            return false;
        }
        emit writeError(QStringLiteral("时序数据库已损坏，已隔离并重建当天文件"));
        return openDatabaseForDate(localDate, error);
    }

    db = QSqlDatabase();
    if (!createOrValidateSchema(error)) {
        closeDatabase();
        return false;
    }
    return true;
}

bool TimeSeriesWriter::createOrValidateSchema(QString *error)
{
    QSqlDatabase db = database();
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)"))) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }

    const QString createSamples = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS samples("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ts_utc_ms INTEGER NOT NULL,"
        "local_offset_min INTEGER NOT NULL,"
        "session_id TEXT NOT NULL,"
        "j1_angle_deg REAL,"
        "j2_height_mm REAL,"
        "j3_total_length_mm REAL,"
        "j4_angle_deg REAL,"
        "counterweight_height_mm REAL,"
        "column_torque_percent REAL,"
        "counterweight_torque_percent REAL,"
        "arm_tilt_x_deg REAL,"
        "arm_tilt_y_deg REAL,"
        "plane_height_mm REAL,"
        "load_kg REAL,"
        "total_power_w REAL,"
        "chassis_tilt_x_deg REAL,"
        "chassis_tilt_y_deg REAL,"
        "arm_torque_percent REAL,"
        "front_support_force_kg REAL)");
    if (!query.exec(createSamples)) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_ts ON samples(ts_utc_ms, id)"));

    const QString versionText = readMetadataValue(QStringLiteral("schema_version"));
    if (!versionText.isEmpty()) {
        bool ok = false;
        const int version = versionText.toInt(&ok);
        if (!ok || version > TimeSeriesConfig::kSupportedSchemaVersion) {
            if (error) {
                *error = QStringLiteral("不支持的数据库版本");
            }
            m_ready = false;
            m_paused = true;
            return false;
        }
    } else {
        writeMetadataValue(QStringLiteral("schema_version"),
                           QString::number(TimeSeriesConfig::kSupportedSchemaVersion));
    }

    QSqlQuery info(db);
    if (!info.exec(QStringLiteral("PRAGMA table_info(samples)"))) {
        if (error) {
            *error = info.lastError().text();
        }
        return false;
    }
    QSet<QString> columns;
    while (info.next()) {
        columns.insert(info.value(1).toString());
    }
    QSqlQuery alter(db);
    for (TimeSeriesChannel channel : TimeSeriesMeta::allChannels()) {
        const QString column = TimeSeriesMeta::columnName(channel);
        if (columns.contains(column) || column.isEmpty()) {
            continue;
        }
        if (!alter.exec(QStringLiteral("ALTER TABLE samples ADD COLUMN %1 REAL").arg(column))) {
            if (error) {
                *error = alter.lastError().text();
            }
            return false;
        }
        columns.insert(column);
    }
    const QStringList required = columnListSql().split(QLatin1Char(','));
    for (const QString &column : required) {
        if (!columns.contains(column)) {
            if (error) {
                *error = QStringLiteral("时序数据库缺少列：%1").arg(column);
            }
            return false;
        }
    }

    writeMetadataValue(QStringLiteral("created_at"),
                       QDateTime::currentDateTime().toString(Qt::ISODate));
    writeMetadataValue(QStringLiteral("app_version"), m_config.appVersion);
    writeMetadataValue(QStringLiteral("sample_interval_ms"),
                       QString::number(m_config.sampleIntervalMs));
    for (TimeSeriesChannel channel : TimeSeriesMeta::allChannels()) {
        const QString column = TimeSeriesMeta::columnName(channel);
        writeMetadataValue(QStringLiteral("channel.%1.name").arg(column),
                           TimeSeriesMeta::displayName(channel));
        writeMetadataValue(QStringLiteral("channel.%1.unit").arg(column),
                           TimeSeriesMeta::unit(channel));
    }
    return true;
}

bool TimeSeriesWriter::insertBatch(const QVector<TimeSeriesSample> &batch, QString *error)
{
    if (batch.isEmpty()) {
        return true;
    }

    QMap<QDate, QVector<TimeSeriesSample>> grouped;
    for (const TimeSeriesSample &sample : batch) {
        grouped[sample.localDate()].append(sample);
    }

    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        if (!insertBatchForDate(it.key(), it.value(), error)) {
            return false;
        }
    }
    removeExpiredDailyDatabases(QDate::currentDate());
    return true;
}

bool TimeSeriesWriter::insertBatchForDate(const QDate &localDate,
                                         const QVector<TimeSeriesSample> &batch,
                                         QString *error)
{
    auto op = [this, localDate, &batch](QString *opError) -> bool {
        if (!openDatabaseForDate(localDate, opError)) {
            return false;
        }
        QSqlDatabase db = database();
        if (!db.transaction()) {
            if (opError) {
                *opError = db.lastError().text();
            }
            return false;
        }

        QSqlQuery query(db);
        const QString sql = QStringLiteral("INSERT INTO samples(%1) VALUES (%2)")
                                .arg(columnListSql(), placeholdersSql());
        if (!query.prepare(sql)) {
            db.rollback();
            if (opError) {
                *opError = query.lastError().text();
            }
            return false;
        }

        for (const TimeSeriesSample &sample : batch) {
            query.bindValue(0, sample.tsUtcMs);
            query.bindValue(1, sample.localOffsetMinutes);
            query.bindValue(2, sample.sessionId);
            for (int i = 0; i < TimeSeriesMeta::channelCount(); ++i) {
                const TimeSeriesValue value = (i < sample.values.size())
                    ? sample.values.at(i)
                    : TimeSeriesValue();
                if (value.valid && TimeSeriesMeta::isFiniteNumber(value.value)) {
                    query.bindValue(3 + i, value.value);
                } else {
                    query.bindValue(3 + i, QVariant());
                }
            }
            if (!query.exec()) {
                db.rollback();
                if (opError) {
                    *opError = query.lastError().text();
                }
                return false;
            }
        }

        if (!db.commit()) {
            db.rollback();
            if (opError) {
                *opError = db.lastError().text();
            }
            return false;
        }
        return true;
    };

    return execWithRetry(op, error);
}

void TimeSeriesWriter::removeExpiredDailyDatabases(const QDate &today)
{
    if (!today.isValid()) {
        return;
    }

    const QDate cutoff = today.addDays(-m_config.retentionDays);
    QDir dir(m_config.databaseDirectory);
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("metrics_*.db"),
                                                  QDir::Files | QDir::NoSymLinks);
    const QRegularExpression pattern(QStringLiteral("^metrics_(\\d{8})\\.db$"));
    for (const QFileInfo &info : files) {
        const QRegularExpressionMatch match = pattern.match(info.fileName());
        if (!match.hasMatch()) {
            continue;
        }
        const QDate fileDate = QDate::fromString(match.captured(1), QStringLiteral("yyyyMMdd"));
        if (!fileDate.isValid() || fileDate >= cutoff) {
            continue;
        }
        if (m_openDate == fileDate) {
            closeDatabase();
        }
        removeSidecarFiles(info.absoluteFilePath());
        QFile::remove(info.absoluteFilePath());
    }
}

bool TimeSeriesWriter::hasReservedDiskSpace() const
{
    const QStorageInfo storage(m_config.databaseDirectory);
    if (!storage.isValid() || !storage.isReady()) {
        return true;
    }
    const qint64 required = static_cast<qint64>(m_config.reserveFreeMb) * 1024 * 1024;
    return storage.bytesAvailable() >= required;
}

void TimeSeriesWriter::closeDatabase()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isValid() && db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_openDate = QDate();
}

bool TimeSeriesWriter::runQuickCheck(QString *error)
{
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("PRAGMA quick_check")) || !query.next()) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    const QString result = query.value(0).toString().trimmed().toLower();
    if (result != QLatin1String("ok")) {
        if (error) {
            *error = QStringLiteral("quick_check 失败：%1").arg(result);
        }
        return false;
    }
    return true;
}

bool TimeSeriesWriter::quarantineCurrentDatabase(QString *error)
{
    const QString path = TimeSeriesMeta::dailyFilePath(m_config.databaseDirectory, m_openDate);
    closeDatabase();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"));
    for (const QString &filePath : sidecarPaths(path)) {
        if (!QFile::exists(filePath)) {
            continue;
        }
        const QString renamed = filePath + QStringLiteral(".corrupt_") + stamp;
        if (!QFile::rename(filePath, renamed) && error) {
            *error = QStringLiteral("无法隔离损坏的时序数据库");
            return false;
        }
    }
    return true;
}

bool TimeSeriesWriter::execWithRetry(const std::function<bool(QString *error)> &op, QString *error)
{
    const int delaysMs[] = {1000, 2000, 5000};
    QString lastError;
    for (int attempt = 0; attempt < 4; ++attempt) {
        QString opError;
        if (op(&opError)) {
            return true;
        }
        lastError = opError;
        const QString lowered = opError.toLower();
        const bool retryable = lowered.contains(QStringLiteral("locked"))
            || lowered.contains(QStringLiteral("busy"))
            || lowered.contains(QStringLiteral("i/o"));
        if (!retryable || attempt == 3 || m_shuttingDown.load()) {
            break;
        }
        QThread::msleep(static_cast<unsigned long>(delaysMs[attempt]));
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

void TimeSeriesWriter::setPaused(bool paused, const QString &message)
{
    m_paused = paused;
    if (paused) {
        m_lowDiskNotified = true;
        emit recordingPaused(message);
    } else {
        m_lowDiskNotified = false;
        emit recordingResumed(message);
    }
}

void TimeSeriesWriter::writeMetadataValue(const QString &key, const QString &value)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO metadata(key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

QString TimeSeriesWriter::readMetadataValue(const QString &key) const
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT value FROM metadata WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

void TimeSeriesWriter::removeSidecarFiles(const QString &dbPath)
{
    for (const QString &path : sidecarPaths(dbPath)) {
        if (path != dbPath) {
            QFile::remove(path);
        }
    }
}
