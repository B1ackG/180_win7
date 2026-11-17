#include "mainmodbusconnector.h"

#include "modbusthreadmanager.h"

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
