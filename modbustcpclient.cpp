#include "modbustcpclient.h"
#include "modbus_backend.h"
#include <QDebug>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcModbusTCPClient, "app.modbustcpclient")
#include <algorithm>

ModbusTCPClient::ModbusTCPClient(QObject *parent)
    : QObject(parent)
    , m_networkThread(nullptr)
    , m_port(502)  // Modbus TCP默认端口
    , m_slaveId(1)
    , m_autoReconnect(false)
    , m_reconnectInterval(5000)
    , m_reconnectTimer(nullptr)
    , m_polling(false)
    , m_pollInterval(1000)  // 默认1秒轮询
    , m_pollTimer(nullptr)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &ModbusTCPClient::pollRegisters);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ModbusTCPClient::tryReconnect);

}

ModbusTCPClient::~ModbusTCPClient()
{
    stopPolling();
    disconnectFromServer();
    releaseStaticBackend();

    if (m_networkThread && m_networkThread->isRunning()) {
        m_networkThread->quit();
        m_networkThread->wait();
    }
}

bool ModbusTCPClient::ensureStaticBackendReady()
{
    if (m_backendReady && m_backendHandle) {
        return true;
    }

    m_backendHandle = modbus_backend_create();
    if (!m_backendHandle) {
        qWarning() << "创建静态 Modbus backend 句柄失败";
        return false;
    }

    m_backendReady = true;
    qInfo() << "Modbus 静态库后端已启用 (libmodbus.a)";
    return true;
}

void ModbusTCPClient::releaseStaticBackend()
{
    if (m_backendHandle) {
        modbus_backend_disconnect(m_backendHandle);
        modbus_backend_destroy(m_backendHandle);
        m_backendHandle = nullptr;
    }
    m_backendReady = false;
}

bool ModbusTCPClient::connectToServer(const QString &host, quint16 port, int slaveId)
{
    QMutexLocker locker(&m_mutex);

    m_host = host;
    m_port = port;
    m_slaveId = slaveId;

    if (!ensureStaticBackendReady()) {
        const QString err = QStringLiteral("Modbus 静态后端初始化失败");
        qWarning() << err;
        emit errorOccurred(err);
        return false;
    }

    const int rc = modbus_backend_connect(m_backendHandle,
                                          host.toUtf8().constData(),
                                          static_cast<int>(port),
                                          slaveId);
    m_connectedState = (rc != 0);
    if (m_connectedState) {
        emit connected();
        if (m_autoReconnect) {
            m_reconnectTimer->stop();
        }
        qInfo() << "[Modbus连接] 静态库后端连接成功"
                << host << ":" << port << "SlaveID:" << slaveId;
        return true;
    }
    const QString err = QStringLiteral("静态库后端连接失败 host=%1 port=%2 slave=%3")
                            .arg(host)
                            .arg(port)
                            .arg(slaveId);
    qWarning() << err;
    emit errorOccurred(err);
    if (m_autoReconnect && !m_host.isEmpty() && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
    return false;
}

void ModbusTCPClient::disconnectFromServer()
{
    QMutexLocker locker(&m_mutex);

    stopPolling();
    m_reconnectTimer->stop();
    m_connectedState = false;

    if (m_backendHandle) {
        modbus_backend_disconnect(m_backendHandle);
    }
    emit disconnected();
}

bool ModbusTCPClient::isConnected() const
{
    if (!m_connectedState) {
        return false;
    }
    return m_backendHandle && (modbus_backend_is_connected(m_backendHandle) != 0);
}

void ModbusTCPClient::handleCommunicationFailure(const QString &reason)
{
    bool shouldEmitDisconnected = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_connectedState) {
            m_connectedState = false;
            if (m_backendHandle) {
                modbus_backend_disconnect(m_backendHandle);
            }
            shouldEmitDisconnected = true;
        }
    }

    if (shouldEmitDisconnected) {
        emit disconnected();
    }

    emit errorOccurred(reason);

    if (m_autoReconnect && !m_host.isEmpty() && !m_reconnectTimer->isActive()) {
        qWarning() << "[Modbus通信中断] 启动自动重连，原因:" << reason;
        m_reconnectTimer->start(m_reconnectInterval);
    }
}

void ModbusTCPClient::tryReconnect()
{
    if (m_autoReconnect && !m_host.isEmpty()) {
        qCDebug(lcModbusTCPClient) << "尝试重连Modbus TCP服务器...";
        connectToServer(m_host, m_port, m_slaveId);
    }
}

void ModbusTCPClient::setAutoReconnect(bool enable, int interval)
{
    m_autoReconnect = enable;
    m_reconnectInterval = interval;

    if (!enable) {
        m_reconnectTimer->stop();
    }
}

bool ModbusTCPClient::readHoldingRegisters(int startAddress, int count)
{
    return readRegisters(startAddress, count, 0x03);  // 使用0x03功能码
}
// 新增方法
bool ModbusTCPClient::readInputRegisters(int startAddress, int count)
{
    return readRegisters(startAddress, count, 0x04);  // 使用0x04功能码
}
// 通用读取方法
bool ModbusTCPClient::readRegisters(int startAddress, int count, quint8 functionCode)
{
    if (!isConnected() || count <= 0 || count > 125) {
        return false;
    }

    if (!m_backendHandle) {
        qWarning() << "[Modbus静态库读失败] backend 未就绪";
        return false;
    }

    QVector<quint16> values(count);
    int readCount = -1;
    if (functionCode == 0x03) {
        readCount = modbus_backend_read_holding_registers(m_backendHandle,
                                                          startAddress,
                                                          count,
                                                          values.data(),
                                                          values.size());
    } else if (functionCode == 0x04) {
        readCount = modbus_backend_read_input_registers(m_backendHandle,
                                                        startAddress,
                                                        count,
                                                        values.data(),
                                                        values.size());
        if (readCount <= 0) {
            // 部分从站无独立输入寄存器映射时，回退到保持寄存器
            readCount = modbus_backend_read_holding_registers(m_backendHandle,
                                                              startAddress,
                                                              count,
                                                              values.data(),
                                                              values.size());
        }
    } else {
        return false;
    }
    if (readCount <= 0) {
        const QString reason = QStringLiteral("静态库读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        qWarning() << "[Modbus静态库读失败] 地址:" << startAddress << "数量:" << count;
        handleCommunicationFailure(reason);
        return false;
    }

    const int actualCount = qMin(readCount, count);
    for (int i = 0; i < actualCount; ++i) {
        updateRegisterValue(startAddress + i, values.at(i));
    }
    return true;
}

bool ModbusTCPClient::writeSingleRegister(int address, quint16 value)
{
    if (!isConnected()) {
        qWarning() << "[Modbus写失败] 未连接到服务器";
        return false;
    }

    if (!m_backendHandle) {
        qWarning() << "[Modbus静态库写失败] backend 未就绪";
        return false;
    }
    const bool ok = modbus_backend_write_single_register(m_backendHandle, address, value) != 0;
    if (!ok) {
        qWarning() << "[Modbus静态库写失败] 地址:" << address << "值:" << value;
        const QString reason = QStringLiteral("静态库写入失败 address=%1").arg(address);
        handleCommunicationFailure(reason);
    }
    return ok;
}

bool ModbusTCPClient::writeMultipleRegisters(int startAddress, const QVector<quint16> &values)
{
    if (!isConnected() || values.isEmpty() || values.size() > 123) {
        return false;
    }

    if (m_backendHandle) {
        const bool ok = modbus_backend_write_multiple_registers(m_backendHandle,
                                                                startAddress,
                                                                values.constData(),
                                                                values.size()) != 0;
        if (!ok) {
            const QString reason = QStringLiteral("静态库批量写入失败 start=%1 count=%2")
                                       .arg(startAddress)
                                       .arg(values.size());
            handleCommunicationFailure(reason);
        }
        return ok;
    }
    for (int i = 0; i < values.size(); ++i) {
        if (!writeSingleRegister(startAddress + i, values.at(i))) {
            return false;
        }
    }
    return true;
}

bool ModbusTCPClient::readHoldingRegisterSync(int address, quint16 &value)
{
    QVector<quint16> values;
    if (!readHoldingRegistersSync(address, 1, values) || values.isEmpty()) {
        return false;
    }
    value = values.first();
    return true;
}

bool ModbusTCPClient::readHoldingRegistersSync(int startAddress, int count, QVector<quint16> &values)
{
    values.clear();
    if (!isConnected() || count <= 0 || count > 125) {
        return false;
    }

    if (!m_backendHandle) {
        qWarning() << "[Modbus静态库读失败] backend 未就绪";
        return false;
    }

    values.resize(count);
    const int readCount = modbus_backend_read_holding_registers(m_backendHandle,
                                                                startAddress,
                                                                count,
                                                                values.data(),
                                                                values.size());
    if (readCount <= 0) {
        const QString reason = QStringLiteral("静态库同步读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        qWarning() << "[Modbus静态库读失败] 地址:" << startAddress << "数量:" << count;
        handleCommunicationFailure(reason);
        values.clear();
        return false;
    }

    const int actualCount = qMin(readCount, count);
    values.resize(actualCount);
    for (int i = 0; i < actualCount; ++i) {
        updateRegisterValue(startAddress + i, values.at(i));
    }
    return true;
}

void ModbusTCPClient::addRegisterToPoll(int address, const QString &name)
{
    QMutexLocker locker(&m_mutex);

    if (!m_pollList.contains(address)) {
        m_pollList.append(address);
    }

    if (!name.isEmpty()) {
        m_registerNames[address] = name;
    }
}

void ModbusTCPClient::removeRegisterFromPoll(int address)
{
    QMutexLocker locker(&m_mutex);
    m_pollList.removeAll(address);
    m_registerNames.remove(address);
}

void ModbusTCPClient::clearPollList()
{
    QMutexLocker locker(&m_mutex);
    m_pollList.clear();
    m_registerNames.clear();
}

void ModbusTCPClient::setPollInterval(int ms)
{
    m_pollInterval = ms;
    if (m_pollTimer->isActive()) {
        m_pollTimer->setInterval(ms);
    }
}

void ModbusTCPClient::startPolling()
{
    if (!m_polling && isConnected()) {
        m_polling = true;
        m_pollTimer->start(m_pollInterval);
    }
}

void ModbusTCPClient::stopPolling()
{
    m_polling = false;
    m_pollTimer->stop();
}

void ModbusTCPClient::pollRegisters()
{
    QList<int> pollListSnapshot;
    {
        QMutexLocker locker(&m_mutex);
        pollListSnapshot = m_pollList;
    }

    if (!isConnected() || pollListSnapshot.isEmpty()) {
        return;
    }

    std::sort(pollListSnapshot.begin(), pollListSnapshot.end());
    pollListSnapshot.erase(std::unique(pollListSnapshot.begin(), pollListSnapshot.end()), pollListSnapshot.end());

    constexpr int kMaxRegistersPerRequest = 120;
    int rangeStart = pollListSnapshot.first();
    int previousAddress = rangeStart;

    auto flushRange = [this](int start, int end) {
        int count = end - start + 1;
        if (count > 0) {
            readHoldingRegisters(start, count);
        }
    };

    for (int i = 1; i < pollListSnapshot.size(); ++i) {
        const int currentAddress = pollListSnapshot.at(i);
        const bool isContinuous = (currentAddress == previousAddress + 1);
        const bool exceedMaxCount = (currentAddress - rangeStart + 1) > kMaxRegistersPerRequest;

        if (!isContinuous || exceedMaxCount) {
            flushRange(rangeStart, previousAddress);
            rangeStart = currentAddress;
        }

        previousAddress = currentAddress;
    }

    flushRange(rangeStart, previousAddress);
}

void ModbusTCPClient::updateRegisterValue(int address, quint16 value)
{
    bool changed = true;
    QString registerName;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_registers.find(address);
        if (it != m_registers.end() && it.value().value == value) {
            changed = false;
        } else {
            ModbusRegister &reg = m_registers[address];
            reg.address = address;
            reg.value = value;
        }

        if (!changed) {
            return;
        }

        registerName = m_registerNames.value(address);
    }

    emit registerValueChanged(address, value);
    if (!registerName.isEmpty()) {
        emit registerValueChangedNamed(registerName, value);
    }
}

