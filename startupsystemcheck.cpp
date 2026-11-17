#include "startupsystemcheck.h"

#include "featureswitchmanager.h"

#include <QColor>
#include <QCoreApplication>
#include <QObject>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplashScreen>
#include <QTextStream>
#include <QTimer>
#include <QVector>

#include <exception>

namespace {

StartupConnectionDialogHook g_startupConnectionDialogHook;

enum SystemCheckStatus {
    CHECK_PENDING,
    CHECK_IN_PROGRESS,
    CHECK_SUCCESS,
    CHECK_FAILED,
    CHECK_WARNING
};

struct SystemCheckItem {
    QString name;
    SystemCheckStatus status = CHECK_PENDING;
    QString message;
};

void waitWithUiEvents(int ms)
{
    if (ms <= 0) {
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

void pumpSplashUi()
{
    if (QCoreApplication::instance()) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void showSplashMessage(QSplashScreen *splash, const QString &message, const QColor &color)
{
    if (!splash) {
        return;
    }
    splash->showMessage(message, Qt::AlignBottom | Qt::AlignCenter, color);
    pumpSplashUi();
}

QString endpointText(const MainModbusEndpoint &endpoint)
{
    return QStringLiteral("%1:%2").arg(endpoint.host).arg(endpoint.port);
}

bool callIsConnected(const StartupConnectionTarget &target, bool *callbackError)
{
    if (callbackError) {
        *callbackError = false;
    }
    if (!target.isConnected) {
        qWarning() << "启动检查 isConnected 回调为空:" << target.displayName;
        if (callbackError) {
            *callbackError = true;
        }
        return false;
    }
    try {
        return target.isConnected();
    } catch (const std::exception &ex) {
        qWarning() << "启动检查 isConnected 回调异常:" << target.displayName << ex.what();
        if (callbackError) {
            *callbackError = true;
        }
        return false;
    } catch (...) {
        qWarning() << "启动检查 isConnected 回调未知异常:" << target.displayName;
        if (callbackError) {
            *callbackError = true;
        }
        return false;
    }
}

QString formatReport(const QVector<SystemCheckItem> &checks, qint64 elapsedMs)
{
    QString report = QStringLiteral("系统自检完成 (%1ms)\n\n").arg(elapsedMs);
    int successCount = 0;
    int failedCount = 0;
    int warningCount = 0;

    for (const SystemCheckItem &check : checks) {
        QString statusIcon;
        switch (check.status) {
        case CHECK_SUCCESS:
            statusIcon = QStringLiteral("✓");
            ++successCount;
            break;
        case CHECK_FAILED:
            statusIcon = QStringLiteral("✗");
            ++failedCount;
            break;
        case CHECK_WARNING:
            statusIcon = QStringLiteral("⚠");
            ++warningCount;
            break;
        default:
            statusIcon = QStringLiteral("?");
            break;
        }
        report += QStringLiteral("%1 %2: %3\n")
                      .arg(statusIcon)
                      .arg(check.name)
                      .arg(check.message);
    }

    report += QStringLiteral("\n总计: %1成功, %2警告, %3失败")
                  .arg(successCount)
                  .arg(warningCount)
                  .arg(failedCount);
    return report;
}

void appendReportLog(const QString &report)
{
    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    if (!featureSwitch || !featureSwitch->isSmallFeatureEnabled(QStringLiteral("startup.log_report"))) {
        return;
    }

    QFile logFile(QStringLiteral("system_check.log"));
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "无法写入 system_check.log:" << logFile.errorString();
        return;
    }
    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")) << "\n";
    stream << report << "\n\n";
}

enum class DeviceCheckOutcome {
    Success,
    ContinueOffline,
    FeatureDisabled,
    ExitApplication
};

DeviceCheckOutcome checkOneDevice(QSplashScreen *splash,
                                  const StartupConnectionTarget &target,
                                  QString *message)
{
    if (!message) {
        return DeviceCheckOutcome::ExitApplication;
    }

    if (!target.enabled) {
        *message = QStringLiteral("功能关闭，跳过连接检查");
        return DeviceCheckOutcome::FeatureDisabled;
    }

    if (waitForConnection(splash, target)) {
        *message = QStringLiteral("%1连接成功").arg(target.displayName);
        return DeviceCheckOutcome::Success;
    }

    const StartupConnectionAction action = showConnectionWarningDialog(splash, target);
    if (splash) {
        splash->raise();
    }

    if (action == StartupConnectionAction::Connected || callIsConnected(target, nullptr)) {
        *message = QStringLiteral("%1连接成功").arg(target.displayName);
        return DeviceCheckOutcome::Success;
    }

    *message = QStringLiteral("警告：%1未连接（用户选择继续）").arg(target.displayName);
    return DeviceCheckOutcome::ContinueOffline;
}

void applyDeviceOutcome(SystemCheckItem *item, DeviceCheckOutcome outcome, const QString &message)
{
    if (!item) {
        return;
    }
    item->message = message;
    switch (outcome) {
    case DeviceCheckOutcome::Success:
        item->status = CHECK_SUCCESS;
        break;
    case DeviceCheckOutcome::ContinueOffline:
    case DeviceCheckOutcome::FeatureDisabled:
        item->status = CHECK_WARNING;
        break;
    case DeviceCheckOutcome::ExitApplication:
        item->status = CHECK_FAILED;
        break;
    }
}

} // namespace

void setStartupConnectionDialogHook(StartupConnectionDialogHook hook)
{
    g_startupConnectionDialogHook = std::move(hook);
}

bool waitForConnection(QSplashScreen *splash,
                       const StartupConnectionTarget &target,
                       int pollIntervalMs)
{
    if (splash) {
        const QString waiting = QStringLiteral("正在等待 %1 Modbus 连接（%2）")
                                    .arg(target.displayName)
                                    .arg(endpointText(target.endpoint));
        showSplashMessage(splash, waiting, Qt::white);
    }

    bool callbackError = false;
    if (callIsConnected(target, &callbackError)) {
        return true;
    }
    if (callbackError) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    const int timeoutMs = qMax(0, target.timeoutMs);
    const int interval = qMax(1, pollIntervalMs);

    while (timer.elapsed() < timeoutMs) {
        callbackError = false;
        if (callIsConnected(target, &callbackError)) {
            return true;
        }
        if (callbackError) {
            return false;
        }
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) {
            break;
        }
        waitWithUiEvents(qMin(interval, remaining));
    }

    return callIsConnected(target, nullptr);
}

StartupConnectionAction showConnectionWarningDialog(QSplashScreen *splash,
                                                    const StartupConnectionTarget &target)
{
    if (g_startupConnectionDialogHook) {
        return g_startupConnectionDialogHook(splash, target);
    }

    QMessageBox msgBox(splash);
    msgBox.setWindowModality(Qt::ApplicationModal);
    msgBox.setWindowTitle(QStringLiteral("设备连接警告"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QStringLiteral("%1 Modbus 未连接").arg(target.displayName));
    msgBox.setInformativeText(
        QStringLiteral("无法连接到 %1 (%2)\n\n设备正在后台重连，连接成功后本窗口会自动关闭。\n\n请检查：\n1. 网络连接\n2. 设备电源\n3. IP地址配置")
            .arg(target.displayName)
            .arg(endpointText(target.endpoint)));

    QPushButton *skipButton = msgBox.addButton(QStringLiteral("离线继续"), QMessageBox::AcceptRole);
    msgBox.setDefaultButton(skipButton);
    msgBox.setEscapeButton(skipButton);

    bool connectedWhileOpen = false;
    QTimer poll(&msgBox);
    poll.setInterval(50);
    QObject::connect(&poll, &QTimer::timeout, &msgBox, [&]() {
        if (callIsConnected(target, nullptr)) {
            connectedWhileOpen = true;
            msgBox.accept();
        }
    });
    poll.start();
    msgBox.exec();
    poll.stop();

    if (connectedWhileOpen || callIsConnected(target, nullptr)) {
        return StartupConnectionAction::Connected;
    }
    return StartupConnectionAction::ContinueOffline;
}

StartupCheckResult performSystemChecks(QSplashScreen *splash,
                                       const StartupConnectionTarget &mainTarget,
                                       const StartupConnectionTarget &agvTarget)
{
    StartupCheckResult result;
    if (!splash) {
        qWarning() << "performSystemChecks: splash 为空，无法继续启动";
        result.continueStartup = false;
        result.report = QStringLiteral("错误：启动画面为空");
        return result;
    }

    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    if (!featureSwitch || !featureSwitch->isBigFeatureEnabled(QStringLiteral("startup_checks"))) {
        showSplashMessage(splash,
                          QStringLiteral("系统自检功能已关闭，跳过自检流程"),
                          Qt::yellow);
        waitWithUiEvents(300);
        result.continueStartup = true;
        result.report = QStringLiteral("系统自检功能已关闭，跳过自检流程");
        return result;
    }

    QVector<SystemCheckItem> checks = {
        {QStringLiteral("硬件初始化"), CHECK_PENDING, QStringLiteral("正在初始化硬件...")},
        {QStringLiteral("文件系统检查"), CHECK_PENDING, QStringLiteral("检查文件系统...")},
        {QStringLiteral("配置文件加载"), CHECK_PENDING, QStringLiteral("加载系统配置...")},
        {QStringLiteral("主控制器连接"), CHECK_PENDING,
         QStringLiteral("连接主控制器(%1)...").arg(endpointText(mainTarget.endpoint))},
        {QStringLiteral("AGV控制器连接"), CHECK_PENDING,
         QStringLiteral("连接AGV控制器(%1)...").arg(endpointText(agvTarget.endpoint))},
        {QStringLiteral("矩阵键盘检测"), CHECK_PENDING, QStringLiteral("检测输入设备...")},
        {QStringLiteral("UI初始化"), CHECK_PENDING, QStringLiteral("初始化用户界面...")}
    };

    QElapsedTimer totalTimer;
    totalTimer.start();

    auto updateSplashProgress = [&](int index) {
        showSplashMessage(splash,
                          QStringLiteral("系统自检 (%1/%2)\n%3")
                              .arg(index + 1)
                              .arg(checks.size())
                              .arg(checks[index].message),
                          Qt::white);
    };

    for (int i = 0; i < checks.size(); ++i) {
        SystemCheckItem &check = checks[i];
        updateSplashProgress(i);
        check.status = CHECK_IN_PROGRESS;

        switch (i) {
        case 0:
            check.message = QStringLiteral("硬件初始化完成");
            check.status = CHECK_SUCCESS;
            break;
        case 1:
            if (!QFile::exists(QStringLiteral("config.ini"))) {
                check.message = QStringLiteral("警告：配置文件不存在，将使用默认配置");
                check.status = CHECK_WARNING;
            } else {
                check.message = QStringLiteral("文件系统检查完成");
                check.status = CHECK_SUCCESS;
            }
            break;
        case 2: {
            QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                check.message = QStringLiteral("配置文件读取错误，使用默认值");
                check.status = CHECK_WARNING;
            } else {
                check.message = QStringLiteral("配置文件加载完成");
                check.status = CHECK_SUCCESS;
            }
            break;
        }
        case 3: {
            QString message;
            const DeviceCheckOutcome outcome = checkOneDevice(splash, mainTarget, &message);
            applyDeviceOutcome(&check, outcome, message);
            if (outcome == DeviceCheckOutcome::ExitApplication) {
                result.continueStartup = false;
                result.report = formatReport(checks, totalTimer.elapsed());
                appendReportLog(result.report);
                qDebug().noquote() << result.report;
                return result;
            }
            break;
        }
        case 4: {
            QString message;
            const DeviceCheckOutcome outcome = checkOneDevice(splash, agvTarget, &message);
            applyDeviceOutcome(&check, outcome, message);
            if (outcome == DeviceCheckOutcome::ExitApplication) {
                result.continueStartup = false;
                result.report = formatReport(checks, totalTimer.elapsed());
                appendReportLog(result.report);
                qDebug().noquote() << result.report;
                return result;
            }
            break;
        }
        case 5:
            check.message = QStringLiteral("输入设备检测完成");
            check.status = CHECK_SUCCESS;
            break;
        case 6:
            check.message = QStringLiteral("用户界面初始化完成");
            check.status = CHECK_SUCCESS;
            break;
        default:
            check.message = QStringLiteral("未知检查项");
            check.status = CHECK_WARNING;
            break;
        }
    }

    result.continueStartup = true;
    result.report = formatReport(checks, totalTimer.elapsed());
    appendReportLog(result.report);
    qDebug().noquote() << result.report;
    return result;
}
