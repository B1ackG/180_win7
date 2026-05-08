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
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QSaveFile>
#include <QFileInfo>

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
    m_currentSessionFile = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".json";
    
    setupTcpReceiver();
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
    // 程序结束时自动保存当前记录
    if (!m_records.isEmpty()) {
        autoSaveCurrentRecord();
    }
    disconnectTcpSocket();
}

void OperationRecorder::initAutoSave()
{
    // 确保目录存在
    ensureAutoSaveDir();

    // 尝试加载今日的记录文件
    loadTodayFile();
    qDebug() << "自动保存系统初始化完成，目录:" << m_autoSaveDir;
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

qint64 OperationRecorder::getRuntimeDuration() const
{
    if (!m_firstRecordTime.isValid()) return 0;
    
    QDateTime end = m_lastRecordTime.isValid() ? m_lastRecordTime : QDateTime::currentDateTime();
    return m_firstRecordTime.msecsTo(end);
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

    // 立即自动保存到会话文件
    autoSaveCurrentRecord();

    // 如果TCP传输已启用，发送记录到服务器
    if (m_tcpEnabled) {
        sendRecordToServer(record);
    }
}

void OperationRecorder::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_records.clear();
        m_firstRecordTime = QDateTime();
        m_lastRecordTime = QDateTime();
    }
    
    // 如果存在自动保存文件，考虑删除或者重新开始记录
    // 对于清除操作，我们选择重置文件
    QFile sessionFile(getTodayFileName());
    if (sessionFile.exists()) {
        sessionFile.remove();
    }
    
    emit recordsCleared();
}

bool OperationRecorder::saveToFile(const QString &filename)
{
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

    m_records.clear();
    QJsonArray jsonArray = doc.array();
    for (const auto &jsonValue : jsonArray) {
        OperationRecord record = OperationRecord::fromJson(jsonValue.toObject());
        m_records.append(record);
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

    if (!m_tcpReceiverServer->listen(QHostAddress::Any, m_tcpReceiverPort)) {
        qWarning() << "TCP接收器监听失败:" << m_tcpReceiverServer->errorString()
                   << "端口:" << m_tcpReceiverPort;
        return;
    }

    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = "系统";
    rec.controlName = "TCP接收器";
    rec.controlType = "Network";
    rec.operation = "listening";
    rec.oldValue = "";
    rec.newValue = QString("0.0.0.0:%1").arg(m_tcpReceiverPort);
    appendTcpRecord(rec);
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

    connect(m_tcpReceiverClient, &QTcpSocket::readyRead, this, &OperationRecorder::onReceiverDataReady);
    connect(m_tcpReceiverClient, &QTcpSocket::disconnected, this, &OperationRecorder::onReceiverDisconnected);
    connect(m_tcpReceiverClient, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &OperationRecorder::onReceiverError);
    m_tcpReceiverBuffer.clear();

    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = "系统";
    rec.controlName = "TCP接收器";
    rec.controlType = "Network";
    rec.operation = "client_connected";
    rec.oldValue = "";
    rec.newValue = m_tcpReceiverClient->peerAddress().toString();
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
    if (m_records.size() >= m_maxRecords) m_records.removeFirst();
    m_records.append(record);
    emit recordAdded(record);
    autoSaveCurrentRecord();
}

void OperationRecorder::onReceiverDisconnected()
{
    OperationRecord rec;
    rec.timestamp = QDateTime::currentDateTime();
    rec.pageName = "系统";
    rec.controlName = "TCP接收器";
    rec.controlType = "Network";
    rec.operation = "client_disconnected";
    rec.oldValue = "";
    rec.newValue = "";
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
    QList<OperationRecord> snapshot;
    QString filename;

    {
        QMutexLocker locker(&m_mutex);
        if (m_records.isEmpty()) {
            return false;
        }
        filename = getTodayFileName();
        snapshot = m_records;
    }

    return saveToFileInternal(filename, snapshot);
}

bool OperationRecorder::loadTodayFile()
{
    // 尝试查找今日的文件
    QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
    QDir dir(m_autoSaveDir);
    QStringList filters;
    filters << dateStr + "*.json";

    QStringList files = dir.entryList(filters, QDir::Files, QDir::Time);

    if (!files.isEmpty()) {
        QString latestFile = m_autoSaveDir + files.first();
        return loadFromFile(latestFile);
    }

    return false;
}

bool OperationRecorder::saveToFileInternal(const QString &filename, const QList<OperationRecord> &records)
{
    // 确保目录存在
    QDir().mkpath(QFileInfo(filename).absolutePath());

    QJsonArray jsonArray;
    for (const auto &record : records) {
        jsonArray.append(record.toJson());
    }
    QJsonDocument doc(jsonArray);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

    // 使用 QSaveFile 提供跨平台的原子写入（在 Windows 上更可靠）
    QSaveFile saveFile(filename);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开文件进行写入:" << filename << saveFile.errorString();
        return false;
    }

    qint64 bytesWritten = saveFile.write(jsonData);
    if (bytesWritten == -1) {
        qWarning() << "写入数据失败:" << filename << saveFile.errorString();
        return false;
    }

    if (!saveFile.commit()) {
        qWarning() << "QSaveFile 提交失败:" << filename << saveFile.errorString();
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

    QTextStream stream(&file);
    stream << "=== 操作记录报告 ===\n";
    stream << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    stream << "总记录数: " << m_records.size() << "\n\n";

    // 按页面分组
    QMap<QString, QList<OperationRecord>> pageGroups;
    for (const auto &record : m_records) {
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
        connectTcpSocket();
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

    // 如果TCP传输已启用，启动重连定时器
    if (m_tcpEnabled) {
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

    // 如果TCP传输已启用，启动重连定时器
    if (m_tcpEnabled && !m_reconnectTimer->isActive()) {
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
