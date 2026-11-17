#include "startupsystemcheck.h"
#include "featureswitchmanager.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPixmap>
#include <QSplashScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <QtTest>

#include <stdexcept>

class TestStartupSystemCheck : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void bothConnectedNoDialog();
    void mainOfflineContinueStillChecksAgv();
    void mainConnectedAgvOfflineContinue();
    void bothOfflineContinue();
    void dialogAutoClosesWhenMainConnects();
    void dialogAutoClosesWhenAgvConnects();
    void dialogPollClosesWhenConnectionAppears();
    void closingDialogContinuesOfflineAndChecksAgv();
    void featureDisabledSkipsCallbacks();
    void connectedAtTimeoutBoundarySucceeds();
    void emptyIsConnectedContinuesOffline();
    void throwingIsConnectedContinuesOffline();
    void nullSplashAborts();
    void startupChecksDisabledSkipsDialog();
    void logReportWritesCurrentWorkingDirectory();
    void dialogShowsLocalSimulatorEndpoint();
    void dialogShowsRemoteSimulatorEndpoint();
    void dialogShowsRealDeviceEndpoint();

private:
    struct DeviceProbe {
        QString name;
        MainModbusEndpoint endpoint;
        bool enabled = true;
        bool connected = false;
        bool throwOnConnect = false;
        int timeoutMs = 20;
        int isConnectedCalls = 0;
        int retryCalls = 0;

        StartupConnectionTarget target()
        {
            StartupConnectionTarget item;
            item.displayName = name;
            item.endpoint = endpoint;
            item.enabled = enabled;
            item.timeoutMs = timeoutMs;
            item.isConnected = [this]() {
                ++isConnectedCalls;
                if (throwOnConnect) {
                    throw std::runtime_error("isConnected boom");
                }
                return connected;
            };
            item.requestRetry = [this]() {
                ++retryCalls;
            };
            return item;
        }
    };

    QSplashScreen *makeSplash();
    DeviceProbe makeMainProbe() const;
    DeviceProbe makeAgvProbe() const;

    QPixmap m_pixmap;
};

void TestStartupSystemCheck::init()
{
    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    mgr->setMasterEnabled(true);
    mgr->setBigFeatureEnabled(QStringLiteral("startup_checks"), true);
    mgr->setSmallFeatureEnabled(QStringLiteral("startup.log_report"), false);
    setStartupConnectionDialogHook({});
    if (m_pixmap.isNull()) {
        m_pixmap = QPixmap(8, 8);
        m_pixmap.fill(Qt::black);
    }
}

void TestStartupSystemCheck::cleanup()
{
    setStartupConnectionDialogHook({});
    FeatureSwitchManager::instance()->reload();
}

QSplashScreen *TestStartupSystemCheck::makeSplash()
{
    return new QSplashScreen(m_pixmap);
}

TestStartupSystemCheck::DeviceProbe TestStartupSystemCheck::makeMainProbe() const
{
    DeviceProbe probe;
    probe.name = QStringLiteral("主控制器");
    probe.endpoint.host = QStringLiteral("192.168.1.13");
    probe.endpoint.port = 502;
    return probe;
}

TestStartupSystemCheck::DeviceProbe TestStartupSystemCheck::makeAgvProbe() const
{
    DeviceProbe probe;
    probe.name = QStringLiteral("AGV控制器");
    probe.endpoint.host = QStringLiteral("192.168.1.88");
    probe.endpoint.port = 502;
    return probe;
}

void TestStartupSystemCheck::bothConnectedNoDialog()
{
    DeviceProbe main = makeMainProbe();
    main.connected = true;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    int dialogCount = 0;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &) {
        ++dialogCount;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogCount, 0);
    QVERIFY(main.isConnectedCalls > 0);
    QVERIFY(agv.isConnectedCalls > 0);
    QCOMPARE(main.retryCalls, 0);
    QCOMPARE(agv.retryCalls, 0);
    QVERIFY(result.report.contains(QStringLiteral("主控制器连接成功")));
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::mainOfflineContinueStillChecksAgv()
{
    DeviceProbe main = makeMainProbe();
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    QStringList dialogNames;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &target) {
        dialogNames.append(target.displayName);
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogNames, QStringList{QStringLiteral("主控制器")});
    QVERIFY(agv.isConnectedCalls > 0);
    QVERIFY(result.report.contains(QStringLiteral("警告：主控制器未连接（用户选择继续）")));
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::mainConnectedAgvOfflineContinue()
{
    DeviceProbe main = makeMainProbe();
    main.connected = true;
    DeviceProbe agv = makeAgvProbe();
    setStartupConnectionDialogHook([](QSplashScreen *, const StartupConnectionTarget &) {
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QVERIFY(result.report.contains(QStringLiteral("主控制器连接成功")));
    QVERIFY(result.report.contains(QStringLiteral("警告：AGV控制器未连接（用户选择继续）")));
}

void TestStartupSystemCheck::bothOfflineContinue()
{
    DeviceProbe main = makeMainProbe();
    DeviceProbe agv = makeAgvProbe();
    QStringList dialogNames;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &target) {
        dialogNames.append(target.displayName);
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogNames, (QStringList{QStringLiteral("主控制器"), QStringLiteral("AGV控制器")}));
    QVERIFY(result.report.contains(QStringLiteral("警告：主控制器未连接（用户选择继续）")));
    QVERIFY(result.report.contains(QStringLiteral("警告：AGV控制器未连接（用户选择继续）")));
}

void TestStartupSystemCheck::dialogAutoClosesWhenMainConnects()
{
    DeviceProbe main = makeMainProbe();
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    setStartupConnectionDialogHook([](QSplashScreen *, const StartupConnectionTarget &) {
        return StartupConnectionAction::Connected;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(main.retryCalls, 0);
    QCOMPARE(agv.retryCalls, 0);
    QVERIFY(result.report.contains(QStringLiteral("主控制器连接成功")));
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::dialogAutoClosesWhenAgvConnects()
{
    DeviceProbe main = makeMainProbe();
    main.connected = true;
    DeviceProbe agv = makeAgvProbe();
    setStartupConnectionDialogHook([](QSplashScreen *, const StartupConnectionTarget &) {
        return StartupConnectionAction::Connected;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(main.retryCalls, 0);
    QCOMPARE(agv.retryCalls, 0);
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::dialogPollClosesWhenConnectionAppears()
{
    setStartupConnectionDialogHook({});
    DeviceProbe main = makeMainProbe();
    main.timeoutMs = 10;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    QScopedPointer<QSplashScreen> splash(makeSplash());
    splash->show();

    QTimer connector;
    connector.setInterval(80);
    QObject::connect(&connector, &QTimer::timeout, &connector, [&main, &connector]() {
        if (QApplication::activeModalWidget()) {
            main.connected = true;
            connector.stop();
        }
    });
    QTimer::singleShot(2000, &connector, [&main, &connector]() {
        main.connected = true;
        if (QWidget *modal = QApplication::activeModalWidget()) {
            modal->close();
        }
        connector.stop();
    });
    connector.start();

    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(main.retryCalls, 0);
    QVERIFY(result.report.contains(QStringLiteral("主控制器连接成功")));
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::closingDialogContinuesOfflineAndChecksAgv()
{
    setStartupConnectionDialogHook({});
    DeviceProbe main = makeMainProbe();
    main.timeoutMs = 10;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    QScopedPointer<QSplashScreen> splash(makeSplash());
    splash->show();

    QTimer closer;
    closer.setInterval(20);
    QObject::connect(&closer, &QTimer::timeout, &closer, [&closer]() {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            modal->close();
            closer.stop();
        }
    });
    QTimer::singleShot(2000, &closer, [&closer]() {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            modal->close();
        }
        closer.stop();
    });
    closer.start();

    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QVERIFY(agv.isConnectedCalls > 0);
    QVERIFY(result.report.contains(QStringLiteral("警告：主控制器未连接（用户选择继续）")));
    QVERIFY(result.report.contains(QStringLiteral("AGV控制器连接成功")));
}

void TestStartupSystemCheck::featureDisabledSkipsCallbacks()
{
    DeviceProbe main = makeMainProbe();
    main.enabled = false;
    DeviceProbe agv = makeAgvProbe();
    agv.enabled = false;
    int dialogCount = 0;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &) {
        ++dialogCount;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogCount, 0);
    QCOMPARE(main.isConnectedCalls, 0);
    QCOMPARE(agv.isConnectedCalls, 0);
    QCOMPARE(main.retryCalls, 0);
    QCOMPARE(agv.retryCalls, 0);
    QVERIFY(result.report.contains(QStringLiteral("功能关闭，跳过连接检查")));
}

void TestStartupSystemCheck::connectedAtTimeoutBoundarySucceeds()
{
    StartupConnectionTarget main;
    main.displayName = QStringLiteral("主控制器");
    main.endpoint.host = QStringLiteral("192.168.1.13");
    main.endpoint.port = 502;
    main.enabled = true;
    main.timeoutMs = 80;
    QElapsedTimer clock;
    clock.start();
    main.isConnected = [&clock]() {
        return clock.elapsed() >= 80;
    };

    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    int dialogCount = 0;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &) {
        ++dialogCount;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main, agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogCount, 0);
    QVERIFY(result.report.contains(QStringLiteral("主控制器连接成功")));
}

void TestStartupSystemCheck::emptyIsConnectedContinuesOffline()
{
    StartupConnectionTarget main;
    main.displayName = QStringLiteral("主控制器");
    main.endpoint.host = QStringLiteral("192.168.1.13");
    main.endpoint.port = 502;
    main.enabled = true;
    main.timeoutMs = 20;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    setStartupConnectionDialogHook([](QSplashScreen *, const StartupConnectionTarget &) {
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main, agv.target());
    QVERIFY(result.continueStartup);
    QVERIFY(agv.isConnectedCalls > 0);
    QVERIFY(result.report.contains(QStringLiteral("警告：主控制器未连接（用户选择继续）")));
}

void TestStartupSystemCheck::throwingIsConnectedContinuesOffline()
{
    DeviceProbe main = makeMainProbe();
    main.throwOnConnect = true;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    setStartupConnectionDialogHook([](QSplashScreen *, const StartupConnectionTarget &) {
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QVERIFY(agv.isConnectedCalls > 0);
}

void TestStartupSystemCheck::nullSplashAborts()
{
    DeviceProbe main = makeMainProbe();
    DeviceProbe agv = makeAgvProbe();
    const StartupCheckResult result = performSystemChecks(nullptr, main.target(), agv.target());
    QVERIFY(!result.continueStartup);
    QCOMPARE(main.isConnectedCalls, 0);
    QCOMPARE(agv.isConnectedCalls, 0);
}

void TestStartupSystemCheck::startupChecksDisabledSkipsDialog()
{
    FeatureSwitchManager::instance()->setBigFeatureEnabled(QStringLiteral("startup_checks"), false);
    DeviceProbe main = makeMainProbe();
    DeviceProbe agv = makeAgvProbe();
    int dialogCount = 0;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &) {
        ++dialogCount;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);
    QCOMPARE(dialogCount, 0);
    QCOMPARE(main.isConnectedCalls, 0);
    QCOMPARE(agv.isConnectedCalls, 0);
}

void TestStartupSystemCheck::logReportWritesCurrentWorkingDirectory()
{
    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    const QString oldPath = QDir::currentPath();
    QVERIFY(QDir::setCurrent(logDir.path()));
    FeatureSwitchManager::instance()->setSmallFeatureEnabled(QStringLiteral("startup.log_report"), true);

    DeviceProbe main = makeMainProbe();
    main.connected = true;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    QScopedPointer<QSplashScreen> splash(makeSplash());
    const StartupCheckResult result = performSystemChecks(splash.data(), main.target(), agv.target());
    QVERIFY(result.continueStartup);

    QFile logFile(QStringLiteral("system_check.log"));
    QVERIFY(logFile.exists());
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(logFile.readAll());
    QVERIFY(text.contains(QStringLiteral("主控制器连接成功")));
    QVERIFY(text.contains(QStringLiteral("AGV控制器连接成功")));
    QVERIFY(QDir::setCurrent(oldPath));
}

void TestStartupSystemCheck::dialogShowsLocalSimulatorEndpoint()
{
    const MainModbusEndpoint expected = MainModbusConnector::selectEndpoint(true, false);
    DeviceProbe main = makeMainProbe();
    main.endpoint = expected;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    MainModbusEndpoint seen;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &target) {
        seen = target.endpoint;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    performSystemChecks(splash.data(), main.target(), agv.target());
    QCOMPARE(seen.host, expected.host);
    QCOMPARE(seen.port, expected.port);
}

void TestStartupSystemCheck::dialogShowsRemoteSimulatorEndpoint()
{
    const MainModbusEndpoint expected = MainModbusConnector::selectEndpoint(
        false, true, QStringLiteral("192.168.8.88"));
    DeviceProbe main = makeMainProbe();
    main.endpoint = expected;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    MainModbusEndpoint seen;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &target) {
        seen = target.endpoint;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    performSystemChecks(splash.data(), main.target(), agv.target());
    QCOMPARE(seen.host, expected.host);
    QCOMPARE(seen.port, expected.port);
}

void TestStartupSystemCheck::dialogShowsRealDeviceEndpoint()
{
    const MainModbusEndpoint expected = MainModbusConnector::selectEndpoint(false, false);
    DeviceProbe main = makeMainProbe();
    main.endpoint = expected;
    DeviceProbe agv = makeAgvProbe();
    agv.connected = true;
    MainModbusEndpoint seen;
    setStartupConnectionDialogHook([&](QSplashScreen *, const StartupConnectionTarget &target) {
        seen = target.endpoint;
        return StartupConnectionAction::ContinueOffline;
    });

    QScopedPointer<QSplashScreen> splash(makeSplash());
    performSystemChecks(splash.data(), main.target(), agv.target());
    QCOMPARE(seen.host, expected.host);
    QCOMPARE(seen.port, expected.port);
}

QTEST_MAIN(TestStartupSystemCheck)
#include "tst_startupsystemcheck.moc"
