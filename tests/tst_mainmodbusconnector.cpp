#include "mainmodbusconnector.h"

#include <QtTest>

class TestMainModbusConnector : public QObject
{
    Q_OBJECT
private slots:
    void mainRealDevice();
    void mainLocalSimulator();
    void mainRemoteSimulator();
    void mainBothPreferLocal();
    void agvRealDevice();
    void agvLocalSimulator();
    void agvRemoteSimulator();
    void agvBothPreferLocal();
    void agvEmptyHostUsesDefault();
    void agvPortZeroUsesDefault();
};

void TestMainModbusConnector::mainRealDevice()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        false, false, QStringLiteral("192.168.1.70"));
    QCOMPARE(endpoint.host, QStringLiteral("192.168.1.13"));
    QCOMPARE(endpoint.port, static_cast<quint16>(502));
}

void TestMainModbusConnector::mainLocalSimulator()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        true, false, QStringLiteral("192.168.1.70"));
    QCOMPARE(endpoint.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5020));
}

void TestMainModbusConnector::mainRemoteSimulator()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        false, true, QStringLiteral("192.168.8.88"));
    QCOMPARE(endpoint.host, QStringLiteral("192.168.8.88"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5020));
}

void TestMainModbusConnector::mainBothPreferLocal()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectEndpoint(
        true, true, QStringLiteral("192.168.8.88"));
    QCOMPARE(endpoint.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5020));
}

void TestMainModbusConnector::agvRealDevice()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        false, false, QStringLiteral("192.168.1.70"),
        QStringLiteral("192.168.1.88"), 502);
    QCOMPARE(endpoint.host, QStringLiteral("192.168.1.88"));
    QCOMPARE(endpoint.port, static_cast<quint16>(502));
}

void TestMainModbusConnector::agvLocalSimulator()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        true, false, QStringLiteral("192.168.1.70"),
        QStringLiteral("192.168.1.88"), 502);
    QCOMPARE(endpoint.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5021));
}

void TestMainModbusConnector::agvRemoteSimulator()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        false, true, QStringLiteral("192.168.8.88"),
        QStringLiteral("192.168.1.88"), 1502);
    QCOMPARE(endpoint.host, QStringLiteral("192.168.8.88"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5021));
}

void TestMainModbusConnector::agvBothPreferLocal()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        true, true, QStringLiteral("192.168.8.88"),
        QStringLiteral("192.168.1.88"), 1502);
    QCOMPARE(endpoint.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(endpoint.port, static_cast<quint16>(5021));
}

void TestMainModbusConnector::agvEmptyHostUsesDefault()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        false, false, QStringLiteral("192.168.1.70"),
        QString(), 502);
    QCOMPARE(endpoint.host, QStringLiteral("192.168.1.88"));
    QCOMPARE(endpoint.port, static_cast<quint16>(502));
}

void TestMainModbusConnector::agvPortZeroUsesDefault()
{
    const MainModbusEndpoint endpoint = MainModbusConnector::selectAgvEndpoint(
        false, false, QStringLiteral("192.168.1.70"),
        QStringLiteral("192.168.1.88"), 0);
    QCOMPARE(endpoint.host, QStringLiteral("192.168.1.88"));
    QCOMPARE(endpoint.port, static_cast<quint16>(502));
}

QTEST_MAIN(TestMainModbusConnector)
#include "tst_mainmodbusconnector.moc"
