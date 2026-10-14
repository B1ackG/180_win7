#include "operationrecorder.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QSettings>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QSaveFile>
#include <QFileInfo>
#include <QtConcurrent>
#include <QMetaObject>

namespace {
QStringList parseCsvList(const QString &raw)
{
    QStringList result;
    const QStringList parts = raw.split(',', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        const QString trimmed = p.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result;
}

QString normalizedPeerAddress(const QHostAddress &address)
{
    bool ok = false;
    const quint32 ipv4 = address.toIPv4Address(&ok);
    if (ok) {
        return QHostAddress(ipv4).toString();
    }
    QString text = address.toString();
    const QString prefix = QStringLiteral("::ffff:");
    if (text.startsWith(prefix)) {
        text.remove(0, prefix.size());
    }
    return text;
}
}

OperationRecorder::OperationRecorder(QObject *parent)
    : QObject{parent}
    , m_tcpSocket(nullptr)
    , m_tcpEnabled(false)
    , m_tcpServerIp(WIN7_IP)
    , m_tcpServerPort(WIN7_PORT)
    , m_reconnectTimer(nullptr)
{
    // 初始化TCP传输
    m_tcpSocket = new QTcpSocket(this);
    loadSecuritySettings();

    // 连接TCP信号
    connect(m_tcpSocket, &QTcpSocket::connected, this, &OperationRecorder::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &OperationRecorder::onTcpDisconnected);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &OperationRecorder::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &OperationRecorder::onTcpDataWritten);

    // 初始化重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(5000); // 5秒重连间隔
    connect(m_reconnectTimer, &QTimer::timeout, this, &OperationRecorder::onReconnectTimeout);

    // 初始化 TCP 接收器（用于接收外部记录推送）
    m_tcpReceiverServer = new QTcpServer(this);
    connect(m_tcpReceiverServer, &QTcpServer::newConnection, this, &OperationRecorder::onReceiverNewConnection);
    
    // 初始化自动保存配置
    m_autoSaveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/OperationRecords/";
    updateCurrentDailyFile();
    m_currentSaveDate = QDate::currentDate();

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(1000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &OperationRecorder::onAutoSaveTimeout);
}

void OperationRecorder::loadSecuritySettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("OperationLogTransport");

    const QByteArray allowedHostsEnv = qgetenv("AGV_LOG_ALLOWED_HOSTS");
    if (!allowedHostsEnv.isEmpty()) {
        m_allowedHosts = parseCsvList(QString::fromUtf8(allowedHostsEnv));
    } else {
        m_allowedHosts = parseCsvList(settings.value("allowed_hosts", "127.0.0.1,192.168.1.70").toString());
    }

    const QByteArray signKeyEnv = qgetenv("AGV_LOG_SIGNING_KEY");
    if (!signKeyEnv.isEmpty()) {
        m_signingKey = signKeyEnv;
    } else {
        m_signingKey = settings.value("signing_key", "").toByteArray();
    }

    const QByteArray peerEnv = qgetenv("AGV_LOG_ALLOWED_PEERS");
    if (!peerEnv.isEmpty()) {
        m_allowedPeerHosts = parseCsvList(QString::fromUtf8(peerEnv));
    } else {
        m_allowedPeerHosts = parseCsvList(settings.value("allowed_peer_hosts").toString());
    }
    settings.endGroup();
}

bool OperationRecorder::validateTransportPolicy(QString *reason) const
{
    constexpr int kMinSigningKeyLength = 16;

    if (!m_allowedHosts.isEmpty() && !m_allowedHosts.contains(m_tcpServerIp)) {
        if (reason) {
            *reason = QString("目标主机不在白名单中: %1").arg(m_tcpServerIp);
        }
        return false;
    }

    if (m_signingKey.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("必须配置签名密钥(AGV_LOG_SIGNING_KEY)");
        }
        return false;
    }

    if (m_signingKey.size() < kMinSigningKeyLength) {
        if (reason) {
            *reason = QStringLiteral("签名密钥长度过短，至少需要16字节");
        }
        return false;
    }

    return true;
}

QByteArray OperationRecorder::buildSignedPayload(const OperationRecord &record) const
{
    const QJsonObject payloadObj = record.toJson();
    const QJsonDocument payloadDoc(payloadObj);
    const QByteArray payload = payloadDoc.toJson(QJsonDocument::Compact);
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    const QByteArray nonce = QCryptographicHash::hash(
        QByteArray::number(ts) + payload,
        QCryptographicHash::Sha256).toHex().left(16);

    QJsonObject envelope;
    envelope["payload"] = QString::fromUtf8(payload.toBase64());
    envelope["ts"] = QString::number(ts);
    envelope["nonce"] = QString::fromUtf8(nonce);
    envelope["alg"] = "HMAC-SHA256";

    if (!m_signingKey.isEmpty()) {
        const QByteArray canonical = payload + '|' + QByteArray::number(ts) + '|' + nonce;
        const QByteArray sig = QMessageAuthenticationCode::hash(canonical, m_signingKey, QCryptographicHash::Sha256).toHex();
        envelope["sig"] = QString::fromUtf8(sig);
    }

    QJsonDocument envelopeDoc(envelope);
    QByteArray out = envelopeDoc.toJson(QJsonDocument::Compact);
    out.append("\n");
    return out;
}

OperationRecorder::~OperationRecorder()
{
    m_shuttingDown.store(true);
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }
    waitForBackgroundWrite();

    bool hasRecords = false;
    {
        QMutexLocker locker(&m_mutex);
        hasRecords = !m_records.isEmpty();
    }
    if (hasRecords) {
        flushAutoSave(true);
    }
    disconnectTcpSocket();
}

void OperationRecorder::initAutoSave()
{
    ensureAutoSaveDir();

    const bool loaded = loadTodayFile();
    m_currentSaveDate = QDate::currentDate();
    m_autoSaveInitialized = true;
    if (!loaded) {
        QList<OperationRecord> existing;
        {
            QMutexLocker locker(&m_mutex);
            existing = m_records;
        }
        for (const OperationRecord &record : existing) {
            if (!appendDurableRecord(record)) {
                QMutexLocker locker(&m_mutex);
                m_unsavedDurable.append(record);
            }
        }
        if (!existing.isEmpty()) {
            scheduleAutoSave();
        }
    } else if (m_tcpReceiverServer && m_tcpReceiverServer->isListening()) {
        const QString bindText = m_listenLocalOnly
            ? QStringLiteral("127.0.0.1:%1").arg(m_tcpReceiverPort)
            : QStringLiteral("0.0.0.0:%1").arg(m_tcpReceiverPort);
        OperationRecord rec;
        rec.timestamp = QDateTime::currentDateTime();
        rec.pageName = QStringLiteral("日志服务");
        rec.controlName = QStringLiteral("日志接收");
        rec.controlType = QStringLiteral("网络服务");
        rec.operation = QStringLiteral("开启监听");
        rec.oldValue = QString();
        rec.newValue = QStringLiteral("日志服务已开始监听，%1").arg(bindText);
        appendTcpRecord(rec);
    }
    qDebug() << "按天自动保存系统初始化完成，目录:" << m_autoSaveDir
             << "当前文件:" << getTodayFileName();
}

bool OperationRecorder::ensureAutoSaveDir()
{
    QDir dir(m_autoSaveDir);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

QString OperationRecorder::getTodayFileName() const
{
    return m_autoSaveDir + m_currentSessionFile;
}

QString OperationRecorder::dailyFileName(const QDate &date) const
{
    return QStringLiteral("operation_history_%1.json").arg(date.toString(QStringLiteral("yyyyMMdd")));
}

QString OperationRecorder::durableLogFileName(const QDate &date) const
{
    return QStringLiteral("operation_history_%1.jsonl").arg(date.toString(QStringLiteral("yyyyMMdd")));
}

bool OperationRecorder::appendDurableRecord(const OperationRecord &record)
{
    const QDate date = record.timestamp.isValid() ? record.timestamp.date() : QDate::currentDate();
    QDir().mkpath(m_autoSaveDir);
    QFile file(m_autoSaveDir + durableLogFileName(date));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "追加历史记录失败:" << file.fileName() << file.errorString();
        return false;
    }
    const QByteArray line = QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact) + '\n';
    const bool ok = file.write(line) == line.size() && file.flush();
    if (ok) {
        noteDirtyHistoryDate(date);
    }
    return ok;
}

QString OperationRecorder::durableFileForDate(const QDate &date) const
{
    return m_autoSaveDir + durableLogFileName(date);
}

void OperationRecorder::noteDirtyHistoryDate(const QDate &date)
{
    if (!date.isValid()) {
        return;
    }
    QMutexLocker locker(&m_mutex);
    m_dirtyHistoryDates.insert(date);
}

bool OperationRecorder::persistAcceptedRecord(const OperationRecord &record)
{
    if (!m_autoSaveInitialized) {
        return false;
    }
    if (!appendDurableRecord(record)) {
        const QDate date = record.timestamp.isValid() ? record.timestamp.date() : QDate::currentDate();
        QMutexLocker locker(&m_mutex);
        m_unsavedDurable.append(record);
        m_dirtyHistoryDates.insert(date);
        return false;
    }
    return true;
}

void OperationRecorder::rebuildDirtyHistoryDates(const QDate &primaryDate)
{
    QList<QDate> dirty;
    {
        QMutexLocker locker(&m_mutex);
        dirty = m_dirtyHistoryDates.values();
        m_dirtyHistoryDates.clear();
    }

    QSet<QDate> failed;
    for (const QDate &date : dirty) {
        if (!date.isValid() || date == primaryDate) {
            continue;
        }
        if (!QFile::exists(m_autoSaveDir + durableLogFileName(date))) {
            continue;
        }
        if (!rebuildDailyJson(date)) {
            failed.insert(date);
        }
    }
    if (!failed.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_dirtyHistoryDates.unite(failed);
    }
}

QList<OperationRecord> OperationRecorder::readDurableRecords(const QDate &date) const
{
    QList<OperationRecord> records;
    QFile file(m_autoSaveDir + durableLogFileName(date));
    if (!file.open(QIODevice::ReadOnly)) {
        return records;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) {
            continue;
        }
        records.append(OperationRecord::fromJson(doc.object()));
    }
    return records;
}

bool OperationRecorder::rebuildDailyJson(const QDate &date)
{
    const QList<OperationRecord> records = readDurableRecords(date);
    if (records.isEmpty()) {
        return false;
    }
    return writeRecordsToFile(m_autoSaveDir + dailyFileName(date), records);
}

bool OperationRecorder::verifyIncomingEnvelope(const QJsonObject &envelope, QString *reason) const
{
    const bool keyReady = m_signingKey.size() >= 16;
    if (!envelope.contains(QStringLiteral("payload"))) {
        if (keyReady) {
            if (reason) {
                *reason = QStringLiteral("缺少签名封包");
            }
            return false;
        }
        return true;
    }
    if (!keyReady) {
        return true;
    }
    if (!envelope.contains(QStringLiteral("sig"))
        || !envelope.contains(QStringLiteral("ts"))
        || !envelope.contains(QStringLiteral("nonce"))) {
        if (reason) {
            *reason = QStringLiteral("签名字段不完整");
        }
        return false;
    }

    const QByteArray payload = QByteArray::fromBase64(envelope.value(QStringLiteral("payload")).toString().toUtf8());
    const QByteArray ts = envelope.value(QStringLiteral("ts")).toString().toUtf8();
    const QByteArray nonce = envelope.value(QStringLiteral("nonce")).toString().toUtf8();
    const QByteArray canonical = payload + '|' + ts + '|' + nonce;
    const QByteArray expected = QMessageAuthenticationCode::hash(
        canonical, m_signingKey, QCryptographicHash::Sha256).toHex();
    const QByteArray actual = envelope.value(QStringLiteral("sig")).toString().toUtf8();
    if (expected != actual) {
        if (reason) {
            *reason = QStringLiteral("签名校验失败");
        }
        return false;
    }
    return true;
}

bool OperationRecorder::isAllowedPeer(const QString &peer)
{
    if (m_allowedPeerHosts.isEmpty()) {
        if (!m_loggedOpenPeerPolicy) {
            m_loggedOpenPeerPolicy = true;
            qWarning() << "未配置 allowed_peer_hosts，当前接受所有日志来源";
        }
        return true;
    }
    return m_allowedPeerHosts.contains(peer);
}

void OperationRecorder::updateCurrentDailyFile()
{
    m_currentSessionFile = dailyFileName(QDate::currentDate());
}

qint64 OperationRecorder::getRuntimeDuration() const
{
    if (!m_firstRecordTime.isValid()) return 0;
    
    QDateTime end = m_lastRecordTime.isValid() ? m_lastRecordTime : QDateTime::currentDateTime();
    return m_firstRecordTime.msecsTo(end);
}

QList<OperationRecord> OperationRecorder::recordsSnapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_records;
}

void OperationRecorder::addRecord(const OperationRecord &rec)
{
    if (!m_recordLocalOperations) {
        // 如果不记录本地操作，忽略由 UI 调用的 addRecord
        return;
    }
    
    OperationRecord record = rec;
    
    {
        QMutexLocker locker(&m_mutex);
        if (m_records.isEmpty()) {
            m_firstRecordTime = record.timestamp;
            record.runtimeMs = 0;
        } else {
            record.runtimeMs = m_firstRecordTime.msecsTo(record.timestamp);
        }
        m_lastRecordTime = record.timestamp;

        // 限制记录数量
        if (m_records.size() >= m_maxRecords) {
            m_records.removeFirst();
        }

        m_records.append(record);
    }
    
    emit recordAdded(record);

    persistAcceptedRecord(record);
    scheduleAutoSave();

    // 如果TCP传输已启用，发送记录到服务器
    if (m_tcpEnabled) {
        sendRecordToServer(record);
    }
}

void OperationRecorder::clear()
{
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }
    waitForBackgroundWrite();

    {
        QMutexLocker locker(&m_mutex);
        m_records.clear();
        m_unsavedDurable.clear();
        m_firstRecordTime = QDateTime();
        m_lastRecordTime = QDateTime();
        m_autoSaveDirty = false;
        m_pendingRewrite = false;
    }

    // 如果存在当天自动保存文件，清除时同步删除并重新开始记录
    updateCurrentDailyFile();
    m_currentSaveDate = QDate::currentDate();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"));
    const QString archivedJson = getTodayFileName() + QStringLiteral(".cleared_") + stamp;
    QFile sessionFile(getTodayFileName());
    if (sessionFile.exists()) {
        sessionFile.rename(archivedJson);
    }
    QFile durableFile(m_autoSaveDir + durableLogFileName(QDate::currentDate()));
    if (durableFile.exists()) {
        durableFile.rename(durableFile.fileName() + QStringLiteral(".cleared_") + stamp);
    }

    emit recordsCleared();
}

bool OperationRecorder::saveToFile(const QString &filename)
{
    waitForBackgroundWrite();
    const QList<OperationRecord> durable = readDurableRecords(QDate::currentDate());
    if (!durable.isEmpty()) {
        return saveToFileInternal(filename, durable);
    }

    QList<OperationRecord> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        snapshot = m_records;
    }
    return saveToFileInternal(filename, snapshot);
}

bool OperationRecorder::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }

    QJsonArray jsonArray = doc.array();
    QList<OperationRecord> loadedRecords;
    loadedRecords.reserve(jsonArray.size());
    for (const auto &jsonValue : jsonArray) {
        OperationRecord record = OperationRecord::fromJson(jsonValue.toObject());
        loadedRecords.append(record);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_records = loadedRecords;
        if (m_records.isEmpty()) {
            m_firstRecordTime = QDateTime();
            m_lastRecordTime = QDateTime();
        } else {
            m_firstRecordTime = m_records.first().timestamp;
            m_lastRecordTime = m_records.last().timestamp;
        }
    }

    return true;
}

void OperationRecorder::setupTcpReceiver()
{
    if (!m_tcpReceiverServer) {
        return;
    }

    if (m_tcpReceiverServer->isListening()) {
        return;
    }

    const QHostAddress bindAddress = m_listenLocalOnly ? QHostAddress::LocalHost : QHostAddress::Any;
    const QString bindText = m_listenLocalOnly
        ? QStringLiteral("127.0.0.1:%1").arg(m_tcpReceiverPort)
        : QStringLiteral("0.0.0.0:%1").arg(m_tcpReceiverPort);

    if (!m_tcpReceiverServer->listen(bindAddress, m_tcpReceiverPort)) {
        qWarning() << "TCP接收器监听失败:" << m_tcpReceiverServer->errorString()
                   << "地址:" << bindText;
        scheduleListenRetry();
        return;
    }

    if (m_listenRetryTimer) {
        m_listenRetryTimer->stop();
    }

    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = QStringLiteral("日志服务");
    rec.controlName = QStringLiteral("日志接收");
    rec.controlType = QStringLiteral("网络服务");
    rec.operation = QStringLiteral("开启监听");
    rec.oldValue = QString();
    rec.newValue = QStringLiteral("日志服务已开始监听，%1").arg(bindText);
    appendTcpRecord(rec);
}

void OperationRecorder::setLocalHistoryReceiver(bool enabled)
{
    const bool changed = m_listenLocalOnly != enabled;
    m_listenLocalOnly = enabled;
    if (!m_tcpReceiverServer) {
        return;
    }
    if (m_tcpReceiverServer->isListening() && !changed) {
        return;
    }

    if (m_tcpReceiverClient) {
        QTcpSocket *client = m_tcpReceiverClient;
        m_tcpReceiverClient = nullptr;
        client->disconnect(this);
        client->abort();
        client->deleteLater();
    }
    m_tcpReceiverBuffer.clear();
    if (m_tcpReceiverServer->isListening()) {
        m_tcpReceiverServer->close();
    }
    setupTcpReceiver();
}

void OperationRecorder::scheduleListenRetry()
{
    if (!m_listenRetryTimer) {
        m_listenRetryTimer = new QTimer(this);
        m_listenRetryTimer->setInterval(1000);
        connect(m_listenRetryTimer, &QTimer::timeout, this, &OperationRecorder::setupTcpReceiver);
    }
    if (!m_listenRetryTimer->isActive()) {
        m_listenRetryTimer->start();
    }
}

bool OperationRecorder::targetsLocalReceiver() const
{
    if (m_tcpServerPort != m_tcpReceiverPort) {
        return false;
    }

    const QString host = m_tcpServerIp.trimmed();
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0
        || host == QLatin1String(WIN7_IP)
        || host == QLatin1String("0.0.0.0")) {
        return true;
    }

    const QHostAddress address(host);
    if (address.isLoopback()) {
        return true;
    }

    const QList<QHostAddress> locals = QNetworkInterface::allAddresses();
    for (const QHostAddress &local : locals) {
        if (local == address) {
            return true;
        }
    }
    return false;
}

bool OperationRecorder::decodeRecordLine(const QByteArray &lineBytes, OperationRecord *recordOut) const
{
    if (!recordOut) {
        return false;
    }

    QJsonParseError err;
    const QJsonDocument incomingDoc = QJsonDocument::fromJson(lineBytes, &err);
    if (err.error != QJsonParseError::NoError || !incomingDoc.isObject()) {
        return false;
    }

    QJsonObject recordObj = incomingDoc.object();
    QString verifyReason;
    if (!verifyIncomingEnvelope(recordObj, &verifyReason)) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastDecodeWarnMs > 3000) {
            m_lastDecodeWarnMs = nowMs;
            qWarning() << "拒绝日志记录:" << verifyReason;
        }
        return false;
    }

    // 兼容发送端签名封包：{"payload":"base64-json","ts":"...","nonce":"...","sig":"..."}
    if (recordObj.contains("payload")) {
        const QByteArray payloadB64 = recordObj.value("payload").toString().toUtf8();
        const QByteArray payload = QByteArray::fromBase64(payloadB64);
        QJsonParseError payloadErr;
        const QJsonDocument payloadDoc = QJsonDocument::fromJson(payload, &payloadErr);
        if (payloadErr.error != QJsonParseError::NoError || !payloadDoc.isObject()) {
            return false;
        }
        recordObj = payloadDoc.object();
    }

    OperationRecord record;
    if (recordObj.contains("timestamp")) {
        record.timestamp = QDateTime::fromString(recordObj.value("timestamp").toString(), Qt::ISODate);
    }
    if (!record.timestamp.isValid()) {
        record.timestamp = QDateTime::currentDateTime();
    }

    record.pageName = recordObj.value("pageName").toString();
    record.controlName = recordObj.value("controlName").toString();
    record.controlType = recordObj.value("controlType").toString();
    record.operation = recordObj.value("operation").toString();
    record.oldValue = recordObj.value("oldValue").toVariant();
    record.newValue = recordObj.value("newValue").toVariant();
    if (recordObj.contains("runtime_ms")) {
        record.runtimeMs = static_cast<qint64>(recordObj.value("runtime_ms").toDouble());
    }

    // 兼容缺字段报文，避免记录空行到界面
    if (record.controlName.isEmpty() && record.operation.isEmpty() && record.pageName.isEmpty()) {
        return false;
    }

    *recordOut = record;
    return true;
}

// ===== TCP 接收相关实现 =====
void OperationRecorder::onReceiverNewConnection()
{
    if (m_tcpReceiverClient) {
        m_tcpReceiverClient->disconnectFromHost();
        m_tcpReceiverClient->deleteLater();
        m_tcpReceiverClient = nullptr;
    }

    m_tcpReceiverClient = m_tcpReceiverServer->nextPendingConnection();
    if (!m_tcpReceiverClient) return;

    const QString peer = normalizedPeerAddress(m_tcpReceiverClient->peerAddress());
    if (!isAllowedPeer(peer)) {
        qWarning() << "拒绝未授权的日志来源:" << peer;
        m_tcpReceiverClient->disconnectFromHost();
        m_tcpReceiverClient->deleteLater();
        m_tcpReceiverClient = nullptr;
        return;
    }

    connect(m_tcpReceiverClient, &QTcpSocket::readyRead, this, &OperationRecorder::onReceiverDataReady);
    connect(m_tcpReceiverClient, &QTcpSocket::disconnected, this, &OperationRecorder::onReceiverDisconnected);
    connect(m_tcpReceiverClient, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &OperationRecorder::onReceiverError);
    m_tcpReceiverBuffer.clear();

    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = QStringLiteral("日志服务");
    rec.controlName = QStringLiteral("日志接收");
    rec.controlType = QStringLiteral("网络服务");
    rec.operation = QStringLiteral("客户端连接");
    rec.oldValue = QString();
    rec.newValue = QStringLiteral("示教器 %1 已连上日志服务").arg(peer);
    appendTcpRecord(rec);
    emit tcpConnectionStatusChanged(true);
}

void OperationRecorder::onReceiverDataReady()
{
    if (!m_tcpReceiverClient) return;
    const QByteArray data = m_tcpReceiverClient->readAll();
    if (data.isEmpty()) return;

    m_tcpReceiverBuffer.append(data);

    int newlinePos = m_tcpReceiverBuffer.indexOf('\n');
    while (newlinePos >= 0) {
        const QByteArray rawLine = m_tcpReceiverBuffer.left(newlinePos).trimmed();
        m_tcpReceiverBuffer.remove(0, newlinePos + 1);

        if (!rawLine.isEmpty()) {
            OperationRecord record;
            if (decodeRecordLine(rawLine, &record)) {
                appendTcpRecord(record);
            } else {
                qWarning() << "接收记录JSON解析失败，原始数据:" << QString::fromUtf8(rawLine.left(200));
            }
        }
        newlinePos = m_tcpReceiverBuffer.indexOf('\n');
    }
}

void OperationRecorder::appendTcpRecord(const OperationRecord &record)
{
    OperationRecord normalized = record;
    {
        QMutexLocker locker(&m_mutex);
        if (m_records.isEmpty()) {
            m_firstRecordTime = normalized.timestamp;
            normalized.runtimeMs = 0;
        } else {
            normalized.runtimeMs = m_firstRecordTime.msecsTo(normalized.timestamp);
        }
        m_lastRecordTime = normalized.timestamp;

        if (m_records.size() >= m_maxRecords) {
            m_records.removeFirst();
        }
        m_records.append(normalized);
    }
    emit recordAdded(normalized);
    persistAcceptedRecord(normalized);
    scheduleAutoSave();
}

void OperationRecorder::onReceiverDisconnected()
{
    QString peer;
    if (m_tcpReceiverClient) {
        peer = normalizedPeerAddress(m_tcpReceiverClient->peerAddress());
    }

    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = QStringLiteral("日志服务");
    rec.controlName = QStringLiteral("日志接收");
    rec.controlType = QStringLiteral("网络服务");
    rec.operation = QStringLiteral("客户端断开");
    rec.oldValue = QString();
    rec.newValue = peer.isEmpty()
        ? QStringLiteral("示教器连接已断开")
        : QStringLiteral("示教器 %1 已断开日志服务").arg(peer);
    appendTcpRecord(rec);

    if (m_tcpReceiverClient) {
        m_tcpReceiverClient->deleteLater();
        m_tcpReceiverClient = nullptr;
    }
    m_tcpReceiverBuffer.clear();
    emit tcpConnectionStatusChanged(false);
}

void OperationRecorder::onReceiverError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    qWarning() << "TCP接收器错误:" << (m_tcpReceiverClient ? m_tcpReceiverClient->errorString() : QString());
    emit tcpConnectionStatusChanged(false);
}

bool OperationRecorder::autoSaveCurrentRecord()
{
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }
    return flushAutoSave(true);
}

void OperationRecorder::scheduleAutoSave()
{
    if (!m_autoSaveInitialized || m_shuttingDown.load()) {
        return;
    }

    QDate previousDate;
    bool crossedMidnight = false;
    {
        QMutexLocker locker(&m_mutex);
        m_autoSaveDirty = true;
        const QDate today = QDate::currentDate();
        if (!m_currentSaveDate.isValid()) {
            m_currentSaveDate = today;
        } else if (m_currentSaveDate != today) {
            previousDate = m_currentSaveDate;
            m_currentSaveDate = today;
            crossedMidnight = true;
        }
    }

    if (crossedMidnight) {
        flushAutoSave(true, previousDate);
    }

    if (m_autoSaveTimer && !m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->start();
    }
}

void OperationRecorder::onAutoSaveTimeout()
{
    flushAutoSave(false);
}

QList<OperationRecord> OperationRecorder::snapshotRecordsForDate(const QDate &date) const
{
    // 调用方必须已持有 m_mutex。
    QList<OperationRecord> snapshot;
    snapshot.reserve(m_records.size());
    for (const auto &record : m_records) {
        if (!record.timestamp.isValid()) {
            if (date == QDate::currentDate()) {
                snapshot.append(record);
            }
            continue;
        }
        if (record.timestamp.date() == date) {
            snapshot.append(record);
        }
    }
    return snapshot;
}

bool OperationRecorder::flushAutoSave(bool waitForCompletion, const QDate &dateOverride)
{
    const QDate date = dateOverride.isValid() ? dateOverride : QDate::currentDate();
    QString filename;
    QList<OperationRecord> pending;
    {
        QMutexLocker locker(&m_mutex);
        filename = m_autoSaveDir + dailyFileName(date);
        pending = m_unsavedDurable;
        if (!dateOverride.isValid()) {
            m_currentSessionFile = dailyFileName(date);
            m_currentSaveDate = date;
        }
    }

    QList<OperationRecord> stillPending;
    for (const OperationRecord &record : pending) {
        if (!appendDurableRecord(record)) {
            stillPending.append(record);
        }
    }
    {
        QMutexLocker locker(&m_mutex);
        m_unsavedDurable = stillPending;
        if (!dateOverride.isValid()) {
            m_autoSaveDirty = !stillPending.isEmpty();
        }
    }
    if (!stillPending.isEmpty()) {
        return false;
    }
    if (!QFile::exists(m_autoSaveDir + durableLogFileName(date))) {
        return false;
    }

    rebuildDirtyHistoryDates(date);

    if (waitForCompletion) {
        waitForBackgroundWrite();
        const bool ok = rebuildDailyJson(date);
        if (!ok && !dateOverride.isValid()) {
            QMutexLocker locker(&m_mutex);
            m_autoSaveDirty = true;
        } else if (ok && !m_shuttingDown.load()) {
            emit fileSaved(filename);
        }
        return ok;
    }

    const QList<OperationRecord> records = readDurableRecords(date);
    if (records.isEmpty()) {
        return false;
    }
    startBackgroundWrite(filename, records);
    return true;
}

void OperationRecorder::startBackgroundWrite(const QString &filename, const QList<OperationRecord> &records)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_writeInProgress) {
            m_pendingRewrite = true;
            m_autoSaveDirty = true;
            return;
        }
        m_writeInProgress = true;
    }

    m_writeFuture = QtConcurrent::run([this, filename, records]() {
        const bool ok = writeRecordsToFile(filename, records);
        if (m_shuttingDown.load()) {
            return;
        }
        QMetaObject::invokeMethod(this, "onBackgroundWriteFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, filename),
                                  Q_ARG(bool, ok));
    });
}

void OperationRecorder::waitForBackgroundWrite()
{
    if (m_writeFuture.isRunning()) {
        m_writeFuture.waitForFinished();
    }
    QMutexLocker locker(&m_mutex);
    m_writeInProgress = false;
}

void OperationRecorder::onBackgroundWriteFinished(const QString &filename, bool ok)
{
    if (m_shuttingDown.load()) {
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_writeInProgress = false;
    }
    if (ok) {
        emit fileSaved(filename);
        if (m_autoSaveTimer) {
            m_autoSaveTimer->setInterval(1000);
        }
    } else {
        {
            QMutexLocker locker(&m_mutex);
            m_autoSaveDirty = true;
        }
        if (m_autoSaveTimer && !m_autoSaveTimer->isActive()) {
            m_autoSaveTimer->setInterval(5000);
            m_autoSaveTimer->start();
        }
        return;
    }

    bool needRewrite = false;
    {
        QMutexLocker locker(&m_mutex);
        needRewrite = m_pendingRewrite || m_autoSaveDirty;
        m_pendingRewrite = false;
    }

    if (needRewrite) {
        flushAutoSave(false);
    }
}

bool OperationRecorder::loadTodayFile()
{
    updateCurrentDailyFile();
    const QDate today = QDate::currentDate();
    const QString jsonlPath = m_autoSaveDir + durableLogFileName(today);
    const QString todayFile = getTodayFileName();
    if (!QFile::exists(jsonlPath) && QFile::exists(todayFile)) {
        QFile legacyFile(todayFile);
        if (legacyFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument legacyDoc = QJsonDocument::fromJson(legacyFile.readAll());
            legacyFile.close();
            if (legacyDoc.isArray()) {
                QSaveFile migrated(jsonlPath);
                if (migrated.open(QIODevice::WriteOnly)) {
                    for (const QJsonValue &value : legacyDoc.array()) {
                        const QByteArray line = QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact) + '\n';
                        migrated.write(line);
                    }
                    migrated.commit();
                }
            }
        }
    }

    QList<OperationRecord> loadedRecords = readDurableRecords(today);
    if (loadedRecords.isEmpty() && !QFile::exists(jsonlPath)) {
        const QString dateStr = today.toString(QStringLiteral("yyyyMMdd"));
        QDir dir(m_autoSaveDir);
        const QStringList files = dir.entryList(QStringList() << (dateStr + QStringLiteral("_*.json")),
                                                 QDir::Files, QDir::Time);
        if (!files.isEmpty()) {
            return loadFromFile(m_autoSaveDir + files.first());
        }
        return false;
    }

    QDateTime firstTime;
    QDateTime lastTime;
    if (!loadedRecords.isEmpty()) {
        firstTime = loadedRecords.first().timestamp;
        lastTime = loadedRecords.last().timestamp;
        if (loadedRecords.size() > m_maxRecords) {
            loadedRecords = loadedRecords.mid(loadedRecords.size() - m_maxRecords);
        }
    }
    {
        QMutexLocker locker(&m_mutex);
        m_records = loadedRecords;
        m_firstRecordTime = firstTime;
        m_lastRecordTime = lastTime;
    }
    return !loadedRecords.isEmpty();
}

bool OperationRecorder::writeRecordsToFile(const QString &filename, const QList<OperationRecord> &records)
{
    QDir().mkpath(QFileInfo(filename).absolutePath());

    QJsonArray jsonArray;
    for (const auto &record : records) {
        jsonArray.append(record.toJson());
    }
    QJsonDocument doc(jsonArray);
    const QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    QSaveFile saveFile(filename);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开文件进行写入:" << filename << saveFile.errorString();
        return false;
    }

    const qint64 bytesWritten = saveFile.write(jsonData);
    if (bytesWritten == -1) {
        qWarning() << "写入数据失败:" << filename << saveFile.errorString();
        return false;
    }

    if (!saveFile.commit()) {
        qWarning() << "QSaveFile 提交失败:" << filename << saveFile.errorString();
        return false;
    }

    return true;
}

bool OperationRecorder::saveToFileInternal(const QString &filename, const QList<OperationRecord> &records)
{
    if (!writeRecordsToFile(filename, records)) {
        return false;
    }
    emit fileSaved(filename);
    return true;
}

bool OperationRecorder::exportToText(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QList<OperationRecord> snapshot = readDurableRecords(QDate::currentDate());
    if (snapshot.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        snapshot = m_records;
    }

    QTextStream stream(&file);
    stream << "=== 操作记录报告 ===\n";
    stream << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    stream << "总记录数: " << snapshot.size() << "\n\n";

    // 按页面分组
    QMap<QString, QList<OperationRecord>> pageGroups;
    for (const auto &record : snapshot) {
        pageGroups[record.pageName].append(record);
    }

    // 按页面输出
    for (auto it = pageGroups.begin(); it != pageGroups.end(); ++it) {
        stream << "\n===== 页面: " << it.key() << " =====\n";
        for (const auto &record : it.value()) {
            stream << record.toString() << "\n";
        }
    }

    file.close();
    return true;
}

QList<OperationRecord> OperationRecorder::getPageRecords(const QString &pageName) const
{
    QList<OperationRecord> result;
    for (const auto &record : m_records) {
        if (record.pageName == pageName) {
            result.append(record);
        }
    }
    return result;
}

QList<OperationRecord> OperationRecorder::getControlRecords(const QString &controlName) const
{
    QList<OperationRecord> result;
    for (const auto &record : m_records) {
        if (record.controlName == controlName) {
            result.append(record);
        }
    }
    return result;
}

int OperationRecorder::pageRecordCount(const QString &pageName) const
{
    int count = 0;
    for (const auto &record : m_records) {
        if (record.pageName == pageName) {
            count++;
        }
    }
    return count;
}

// ============ TCP传输相关方法 ============

void OperationRecorder::enableTcpTransmission(bool enabled)
{
    loadSecuritySettings();
    m_tcpEnabled = enabled;

    if (enabled) {
        setupTcpReceiver();
        if (targetsLocalReceiver()) {
            if (m_reconnectTimer) {
                m_reconnectTimer->stop();
            }
            if (m_tcpSocket && m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
                m_tcpSocket->abort();
            }
        } else {
            connectTcpSocket();
        }
    } else {
        disconnectTcpSocket();
    }
}

void OperationRecorder::setTcpServer(const QString &ip, quint16 port)
{
    loadSecuritySettings();
    m_tcpServerIp = ip;
    m_tcpServerPort = port;

    // 如果已经连接，需要重新连接
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        disconnectTcpSocket();
        connectTcpSocket();
    }
}

void OperationRecorder::sendAllRecordsToServer()
{
    if (!m_tcpEnabled || !isTcpConnected()) {
        qWarning() << "TCP传输未启用或未连接，无法发送所有记录";
        emit tcpTransmissionError("TCP传输未启用或未连接");
        return;
    }

    // 将所有记录添加到发送队列
    m_tcpSendQueue.clear();
    for (const auto &record : m_records) {
        m_tcpSendQueue.append(record);
    }

    qDebug() << "开始发送所有记录到服务器，共" << m_tcpSendQueue.size() << "条记录";

    // 开始发送队列中的记录
    sendQueuedRecords();
}

void OperationRecorder::sendRecordToServer(const OperationRecord &record)
{
    if (!m_tcpEnabled) {
        return;
    }

    if (!isTcpConnected()) {
        // 如果未连接，尝试连接
        connectTcpSocket();

        // 传输策略阻断时不再累积离线队列，避免UI长期运行后卡顿。
        if (m_transportPolicyBlocked) {
            return;
        }

        // 将记录添加到队列，等待连接成功后再发送
        enqueueRecordIfPossible(record);
        return;
    }

    const QByteArray data = buildSignedPayload(record);

    // 发送数据
    qint64 bytesWritten = m_tcpSocket->write(data);

    if (bytesWritten == -1) {
        qWarning() << "发送数据失败:" << m_tcpSocket->errorString();
        // 添加到队列等待重试
        enqueueRecordIfPossible(record);
    } else {
        qDebug() << "发送记录到服务器:" << record.controlName << "操作:" << record.operation;
    }
}

void OperationRecorder::sendQueuedRecords()
{
    if (m_tcpSendQueue.isEmpty() || !isTcpConnected()) {
        if (m_tcpSendQueue.isEmpty()) {
            emit tcpTransmissionComplete();
        }
        return;
    }

    // 每次发送最多10条记录，避免阻塞
    int sendCount = qMin(10, m_tcpSendQueue.size());

    for (int i = 0; i < sendCount; ++i) {
        OperationRecord record = m_tcpSendQueue.takeFirst();
        sendRecordToServer(record);
    }

    // 继续发送剩余记录
    if (!m_tcpSendQueue.isEmpty()) {
        QTimer::singleShot(30, this, &OperationRecorder::sendQueuedRecords);
    } else {
        emit tcpTransmissionComplete();
    }
}

void OperationRecorder::connectTcpSocket()
{
    if (targetsLocalReceiver()) {
        if (m_reconnectTimer && m_reconnectTimer->isActive()) {
            m_reconnectTimer->stop();
        }
        return;
    }

    if (!m_tcpEnabled || m_tcpSocket->state() == QAbstractSocket::ConnectingState ||
        m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        return;
    }

    QString policyError;
    if (!validateTransportPolicy(&policyError)) {
        m_transportPolicyBlocked = true;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool shouldReport = (policyError != m_lastTransportPolicyError)
            || ((nowMs - m_lastTransportPolicyErrorMs) > 3000);

        if (shouldReport) {
            m_lastTransportPolicyError = policyError;
            m_lastTransportPolicyErrorMs = nowMs;
            qWarning() << "日志传输策略阻止连接:" << policyError;
            emit tcpTransmissionError(policyError);
        } else {
            qDebug() << "日志传输策略阻止连接(节流):" << policyError;
        }
        return;
    }

    m_transportPolicyBlocked = false;

    qDebug() << "连接日志服务器:" << m_tcpServerIp << ":" << m_tcpServerPort;
    m_tcpSocket->connectToHost(m_tcpServerIp, m_tcpServerPort);
}

void OperationRecorder::disconnectTcpSocket()
{
    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
    }

    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

void OperationRecorder::onTcpConnected()
{
    qDebug() << "TCP服务器连接成功";
    m_lastTcpError.clear();
    m_lastTcpErrorMs = 0;
    m_transportPolicyBlocked = false;
    m_lastTransportPolicyError.clear();
    m_lastTransportPolicyErrorMs = 0;
    // 停止重连定时器
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    // 发送队列中等待的记录
    if (!m_tcpSendQueue.isEmpty()) {
        sendQueuedRecords();
    }
}

void OperationRecorder::onTcpDisconnected()
{
    qDebug() << "TCP服务器连接断开";

    if (m_tcpEnabled && !targetsLocalReceiver()) {
        m_reconnectTimer->start();
    }
}

void OperationRecorder::onTcpError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    const QString error = m_tcpSocket->errorString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool shouldReport = (error != m_lastTcpError);

    if (shouldReport) {
        m_lastTcpError = error;
        m_lastTcpErrorMs = nowMs;
        qWarning() << "TCP连接错误:" << error;
        emit tcpTransmissionError(error);
    } else {
        qDebug() << "TCP连接错误(节流):" << error;
    }

    if (m_tcpEnabled && !targetsLocalReceiver() && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void OperationRecorder::onTcpDataWritten(qint64 bytes)
{
    qDebug() << "已发送" << bytes << "字节到TCP服务器";
}

void OperationRecorder::onReconnectTimeout()
{
    qDebug() << "尝试重新连接TCP服务器...";
    connectTcpSocket();
}

bool OperationRecorder::enqueueRecordIfPossible(const OperationRecord &record)
{
    if (m_tcpSendQueue.size() >= kMaxTcpQueueSize) {
        // 队列满时丢弃最旧记录，防止内存增长拖慢主线程。
        m_tcpSendQueue.removeFirst();
    }
    m_tcpSendQueue.append(record);
    return true;
}
