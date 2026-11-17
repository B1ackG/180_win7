// file name: main.cpp
#include "mainwindow.h"
#include "featureswitchmanager.h"
#include "startupsystemcheck.h"
#include <QApplication>
#include <QGuiApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QScreen>
#include <QColor>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QLoggingCategory>
#include <QFont>
#include "debug.h"

// 全局定义（在 debug.h 中声明）
int debug = 0;

// 全局消息处理器：当 debug==0 时过滤掉 Qt 的调试消息
static void globalMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (type == QtDebugMsg && debug == 0) {
        // 在未开启全局 DEBUG 时，保留主窗口分类日志，便于运行期排查 AGV/UI 问题。
        const QString category = context.category ? QString::fromUtf8(context.category) : QString();
        if (category != "app.mainwindow") {
            return; // 丢弃其它调试输出
        }
    }

    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    Q_UNUSED(file);
    Q_UNUSED(function);
    fprintf(stderr, "%s\n", localMsg.constData());
}
static void waitWithUiEvents(int ms)
{
    if (ms <= 0) {
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

int main(int argc, char *argv[])
{
    // 设置环境变量
    qputenv("QT_SCREEN_SCALE_FACTORS", "LVDS1=1");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    qDebug() << "程序启动 - 创建QApplication";
    QApplication a(argc, argv);

    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    if (a.arguments().contains("--all-features-off")) {
        featureSwitch->setAllEnabled(false);
        featureSwitch->save();
    } else if (a.arguments().contains("--all-features-on")) {
        featureSwitch->setAllEnabled(true);
        featureSwitch->save();
    }

    QByteArray allFeatureEnv = qgetenv("FEATURE_ALL");
    if (!allFeatureEnv.isEmpty()) {
        bool enableAll = (allFeatureEnv == "1" || allFeatureEnv.toLower() == "true");
        featureSwitch->setAllEnabled(enableAll);
        featureSwitch->save();
    }

    // 从环境变量或命令行设置全局 debug 开关：
    // 环境变量：DEBUG=1 或命令行参数 --debug / -d
    QByteArray dbgEnv = qgetenv("DEBUG");
    if (!dbgEnv.isEmpty()) {
        debug = dbgEnv.toInt();
    }
    if (a.arguments().contains("--debug") || a.arguments().contains("-d")) {
        debug = 1;
    }

    // 功能开关优先级高于环境变量/命令行：关闭时统一禁用全局 qDebug 输出。
    if (!featureSwitch->isSmallFeatureEnabled("debug.qdebug")) {
        debug = 0;
    }

    // 通讯热路径分类日志：全局调试关闭时彻底禁用这些分类，
    // 使 qCDebug 在源头短路，避免每次轮询/解析的字符串构造开销。
    if (debug == 0) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "app.agvmodbus.debug=false\n"
            "app.modbusmgr.debug=false\n"
            "app.modbustcpclient.debug=false"));
    }

    // 安装全局消息处理器以便统一过滤 qDebug 输出
    qInstallMessageHandler(globalMessageHandler);

    // 设置应用程序信息
    QApplication::setApplicationName("工业控制系统");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("TechControl");

    // 创建启动画面
    QPixmap pixmap(":/Picture/TechBackGround.png");
    if (pixmap.isNull()) {
        qDebug() << "无法加载启动画面图片";
    }

    // 获取主屏幕大小
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    // 缩放图片到全屏大小
    QPixmap scaledPixmap = pixmap.scaled(screenGeometry.size(),
                                         Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);

    QSplashScreen splash(scaledPixmap);

    QFont splashFont = splash.font();
    splashFont.setPointSize(20);
    splashFont.setBold(true);
    splash.setFont(splashFont);

    // 设置全屏显示
    splash.setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    splash.showFullScreen();  // 或者使用 show() 但设置大小
    // 或者如果你想要控制窗口大小，也可以这样：
    // splash.setFixedSize(screenGeometry.size());
    // splash.show();

    // 或者更简单的方式，在创建QSplashScreen后立即设置全屏
    splash.showFullScreen();

    // 第一阶段：显示欢迎信息
    splash.showMessage("正在启动工业控制系统...",
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    a.processEvents();
    waitWithUiEvents(500);

    // 第二阶段：系统自检
    splash.showMessage("开始系统自检...",
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    a.processEvents();

    // 提前创建MainWindow（但不显示），构造过程会发起真实 Modbus 连接
    qDebug() << "准备创建MainWindow";
    MainWindow w;

    StartupConnectionTarget mainTarget;
    mainTarget.displayName = QStringLiteral("主控制器");
    mainTarget.endpoint = w.currentMainModbusEndpoint();
    mainTarget.enabled = featureSwitch->isBigFeatureEnabled(QStringLiteral("modbus_main"));
    mainTarget.timeoutMs = 1000;
    mainTarget.isConnected = [&w]() { return w.isMainModbusConnected(); };
    mainTarget.requestRetry = [&w]() { w.retryMainModbusConnection(); };

    StartupConnectionTarget agvTarget;
    agvTarget.displayName = QStringLiteral("AGV控制器");
    agvTarget.endpoint = w.currentAgvModbusEndpoint();
    agvTarget.enabled = featureSwitch->isBigFeatureEnabled(QStringLiteral("modbus_agv"));
    agvTarget.timeoutMs = 1000;
    agvTarget.isConnected = [&w]() { return w.isAgvModbusConnected(); };
    agvTarget.requestRetry = [&w]() { w.retryAgvModbusConnection(); };

    const StartupCheckResult checkResult = performSystemChecks(&splash, mainTarget, agvTarget);
    if (!checkResult.continueStartup) {
        qDebug() << "用户取消启动";
        return -1;
    }

    splash.showMessage("系统准备就绪",
                       Qt::AlignBottom | Qt::AlignCenter, QColor(100, 255, 100));
    a.processEvents();
    waitWithUiEvents(800);

    // 显示主窗口
    qDebug() << "准备显示主窗口";
    w.showFullScreen();

    // 关闭启动画面
    splash.finish(&w);

    // 主窗口显示后再初始化报警系统，避免在开机自检界面弹出报警
    QTimer::singleShot(0, &w, &MainWindow::startAlarmSystem);

    qDebug() << "进入事件循环";
    return a.exec();
}
