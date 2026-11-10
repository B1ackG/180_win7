#include "mainmodbusconnector.h"

#include "modbusthreadmanager.h"

MainModbusEndpoint MainModbusConnector::selectEndpoint(bool localSimulatorEnabled,
                                                       bool remoteSimulatorEnabled,
                                                       const QString &remoteSimulatorHost)
{
    MainModbusEndpoint endpoint;
    endpoint.host = QStringLiteral("192.168.1.13");
    endpoint.port = 502;

    if (localSimulatorEnabled) {
        endpoint.host = QStringLiteral("127.0.0.1");
        endpoint.port = 5020;
    } else if (remoteSimulatorEnabled) {
        endpoint.host = remoteSimulatorHost;
        endpoint.port = 5020;
    }

    return endpoint;
}

bool MainModbusConnector::connectAndConfigure(ModbusThreadManager *manager,
                                              const MainModbusEndpoint &endpoint,
                                              int pollIntervalMs,
                                              int reconnectIntervalMs)
{
    if (!manager) {
        return false;
    }

    manager->setPollInterval(pollIntervalMs);
    manager->setAutoReconnect(true, reconnectIntervalMs);
    return manager->connectToDevice(endpoint.host, endpoint.port);
}

MainModbusEndpoint MainModbusConnector::selectAgvEndpoint(bool localSimulatorEnabled,
                                                          bool remoteSimulatorEnabled,
                                                          const QString &remoteSimulatorHost,
                                                          const QString &agvHost,
                                                          quint16 agvPort)
{
    MainModbusEndpoint endpoint;
    endpoint.host = agvHost.isEmpty() ? QStringLiteral("192.168.1.88") : agvHost;
    endpoint.port = agvPort == 0 ? static_cast<quint16>(502) : agvPort;

    if (localSimulatorEnabled) {
        endpoint.host = QStringLiteral("127.0.0.1");
        endpoint.port = 5021;
    } else if (remoteSimulatorEnabled) {
        endpoint.host = remoteSimulatorHost;
        endpoint.port = 5021;
    }

    return endpoint;
}
