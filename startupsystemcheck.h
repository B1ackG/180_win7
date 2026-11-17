#ifndef STARTUPSYSTEMCHECK_H
#define STARTUPSYSTEMCHECK_H

#include "mainmodbusconnector.h"

#include <functional>

#include <QString>

class QSplashScreen;

enum class StartupConnectionAction {
    Connected,
    ContinueOffline
};

struct StartupConnectionTarget {
    QString displayName;
    MainModbusEndpoint endpoint;
    bool enabled = false;
    int timeoutMs = 5000;
    std::function<bool()> isConnected;
    std::function<void()> requestRetry;
};

struct StartupCheckResult {
    bool continueStartup = true;
    QString report;
};

using StartupConnectionDialogHook =
    std::function<StartupConnectionAction(QSplashScreen *, const StartupConnectionTarget &)>;

/** @brief 测试钩子：非空时替代真实连接警告框。传入空函数可恢复默认对话框。 */
void setStartupConnectionDialogHook(StartupConnectionDialogHook hook);

bool waitForConnection(QSplashScreen *splash,
                       const StartupConnectionTarget &target,
                       int pollIntervalMs = 50);

StartupConnectionAction showConnectionWarningDialog(QSplashScreen *splash,
                                                    const StartupConnectionTarget &target);

StartupCheckResult performSystemChecks(QSplashScreen *splash,
                                       const StartupConnectionTarget &mainTarget,
                                       const StartupConnectionTarget &agvTarget);

#endif // STARTUPSYSTEMCHECK_H
