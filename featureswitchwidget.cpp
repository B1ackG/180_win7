#include "featureswitchwidget.h"
#include "featureswitchmanager.h"
#include "techvirtualkeyboard.h"
#include "techslideredit.h"
#include "mainwindow.h"
#include "mappingconfig.h"
#include "displayregistermapping.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleValidator>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVector>
#include <QVBoxLayout>
#include <algorithm>

namespace {

using ButtonModbusMapping::composeLegacyRegisterString;
using ButtonModbusMapping::kMaxTargetsPerDirection;
using ButtonModbusMapping::loadRegisterSpecs;
using ButtonModbusMapping::resolvedBinding;

bool hasModbusOperation(const MainWindow::ControllableButtonInfo &info)
{
    if (info.objectName == QStringLiteral("steeringModeSelector")) {
        return true;
    }
    return !info.defaultReads.isEmpty() || !info.defaultWrites.isEmpty();
}

bool isPermissionSessionControl(const QString &objectName)
{
    return objectName == QStringLiteral("loginButton")
        || objectName == QStringLiteral("logoutButton")
        || objectName == QStringLiteral("featureButton")
        || objectName == QStringLiteral("exitAppButton")
        || objectName == QStringLiteral("roleComboBox")
        || objectName == QStringLiteral("passwordEdit")
        || objectName == QStringLiteral("passwordHint")
        || objectName == QStringLiteral("netConfigSection")
        || objectName == QStringLiteral("adminTitle");
}

bool shouldSkipControllable(const MainWindow::ControllableButtonInfo &info)
{
    return isPermissionSessionControl(info.objectName)
        || info.widgetKind == QStringLiteral("环形仪表")
        || info.objectName.startsWith(QStringLiteral("robot_ArcGauge_"))
        || info.widgetKind == QStringLiteral("滑块输入")
        || info.objectName.startsWith(QStringLiteral("TechSliderEdit_"))
        || info.objectName.startsWith(QStringLiteral("SEdit_"));
}

bool isDeviceControlChildButton(const QString &objectName)
{
    return objectName == QStringLiteral("TBtn_RobotControl")
        || objectName == QStringLiteral("TBtn_HomePage")
        || objectName == QStringLiteral("TBtn_ChassisControl")
        || objectName == QStringLiteral("TBtn_SixAxies")
        || objectName == QStringLiteral("TBtn_SafetyMonitor");
}

bool isConsoleNoiseControl(MainWindow *mainWindow, const QString &objectName)
{
    if (objectName.startsWith(QStringLiteral("qt_"))) {
        return true;
    }
    if (!mainWindow) {
        return false;
    }
    QWidget *widget = mainWindow->findChild<QWidget*>(objectName);
    if (!widget) {
        return false;
    }
    for (QWidget *p = widget->parentWidget(); p && p != mainWindow; p = p->parentWidget()) {
        if (qobject_cast<QDialog*>(p) || qobject_cast<QMessageBox*>(p)) {
            return true;
        }
        if (qobject_cast<FeatureSwitchWidget*>(p)) {
            return true;
        }
        if (p->inherits("TechVirtualKeyboard")) {
            return true;
        }
    }
    return false;
}

QString lastValidFromProperty(const QLineEdit *edit, const QString &fallback)
{
    if (!edit) {
        return fallback;
    }
    const QString stored = edit->property("lastValidText").toString();
    return stored.isEmpty() ? fallback : stored;
}

QLabel *makeHintLabel(const QString &text, QWidget *parent = nullptr)
{
    auto *hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("consoleHint"));
    return hint;
}

QString smallFeatureGroupTitle(const QString &key)
{
    const QString prefix = key.section(QLatin1Char('.'), 0, 0);
    static const QMap<QString, QString> titles = {
        {QStringLiteral("startup"), QStringLiteral("启动")},
        {QStringLiteral("ui"), QStringLiteral("界面")},
        {QStringLiteral("permission"), QStringLiteral("权限")},
        {QStringLiteral("records"), QStringLiteral("操作记录")},
        {QStringLiteral("tcp"), QStringLiteral("TCP 上报")},
        {QStringLiteral("modbus_main"), QStringLiteral("主控 Modbus")},
        {QStringLiteral("modbus_agv"), QStringLiteral("AGV Modbus")},
        {QStringLiteral("agv"), QStringLiteral("AGV 显示")},
        {QStringLiteral("motion"), QStringLiteral("运动控制")},
        {QStringLiteral("input"), QStringLiteral("输入设备")},
        {QStringLiteral("alarm"), QStringLiteral("报警")},
        {QStringLiteral("debug"), QStringLiteral("调试")},
    };
    return titles.value(prefix, prefix.isEmpty() ? QStringLiteral("其它") : prefix);
}

QMap<QString, QPair<double, double>> builtinArcGaugeRanges()
{
    return {
        {QStringLiteral("robot_ArcGauge_J1Angle"), qMakePair(-170.0, 170.0)},
        {QStringLiteral("robot_ArcGauge_J2Height"), qMakePair(4400.0, 8000.0)},
        {QStringLiteral("robot_ArcGauge_J3Length"), qMakePair(3765.0, 6805.0)},
        {QStringLiteral("robot_ArcGauge_J4Angle"), qMakePair(-90.0, 90.0)},
        {QStringLiteral("robot_ArcGauge_CounterweightHeight"), qMakePair(0.0, 2000.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis1"), qMakePair(-15.0, 15.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis2"), qMakePair(-15.0, 15.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis3"), qMakePair(-17.0, 17.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis4"), qMakePair(-110.0, 110.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis5"), qMakePair(-110.0, 110.0)},
        {QStringLiteral("robot_ArcGauge_SixAxis6"), qMakePair(-90.0, 90.0)},
    };
}

QMap<QString, QString> snapshotIniGroup(const QString &fileName, const QString &group)
{
    QMap<QString, QString> values;
    QSettings settings(fileName, QSettings::IniFormat);
    settings.beginGroup(group);
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        values.insert(key, settings.value(key).toString());
    }
    settings.endGroup();
    return values;
}

QString configFieldLabel(const QString &group, const QString &key)
{
    const QString id = group + QLatin1Char('/') + key;
    static const QMap<QString, QString> labels = {
        {QStringLiteral("Polling/main_ui_state_sync_enabled"), QStringLiteral("主控界面同步")},
        {QStringLiteral("Polling/agv_ui_state_sync_enabled"), QStringLiteral("底盘界面同步")},
        {QStringLiteral("Polling/ui_state_sync_enabled"), QStringLiteral("控件状态同步")},
        {QStringLiteral("Polling/main_modbus_poll_ms"), QStringLiteral("主控轮询间隔")},
        {QStringLiteral("Polling/main_ui_poll_ms"), QStringLiteral("控件同步轮询")},
        {QStringLiteral("Polling/main_device_status_poll_ms"), QStringLiteral("设备状态轮询")},
        {QStringLiteral("Polling/main_device_status_start"), QStringLiteral("设备状态起始地址")},
        {QStringLiteral("Polling/main_device_status_count"), QStringLiteral("设备状态数量")},
        {QStringLiteral("Polling/main_control_sync_start"), QStringLiteral("模式同步起始地址")},
        {QStringLiteral("Polling/main_control_sync_count"), QStringLiteral("模式同步数量")},
        {QStringLiteral("Polling/main_reconnect_ms"), QStringLiteral("主控重连间隔")},
        {QStringLiteral("Polling/agv_poll_ms"), QStringLiteral("底盘轮询间隔")},
        {QStringLiteral("Polling/agv_reconnect_ms"), QStringLiteral("底盘重连间隔")},
        {QStringLiteral("PlaneHeight/offset_mm"), QStringLiteral("平面高度偏移")},
        {QStringLiteral("RobotTorque/danger_percent"), QStringLiteral("总扭矩危险阈值")},
        {QStringLiteral("Permission/engineer_password"), QStringLiteral("工程师密码")},
        {QStringLiteral("Permission/admin_password"), QStringLiteral("管理员密码")},
        {QStringLiteral("ButtonVisibility/TBtn_RobotControl"), QStringLiteral("机械臂控制入口")},
        {QStringLiteral("ButtonVisibility/TBtn_ChassisControl"), QStringLiteral("底盘控制入口")},
        {QStringLiteral("ButtonVisibility/TBtn_SixAxies"), QStringLiteral("六自由度入口")},
        {QStringLiteral("ButtonVisibility/TBtn_SafetyMonitor"), QStringLiteral("安全监控入口")},
        {QStringLiteral("ButtonVisibility/TBtn_ControlMode"), QStringLiteral("示教/遥控控制模式")},
        {QStringLiteral("ButtonVisibility/techBtn_AGV_OA"), QStringLiteral("AGV 避障")},
        {QStringLiteral("ButtonVisibility/techBtn_AGV_Park"), QStringLiteral("AGV 驻车")},
        {QStringLiteral("ButtonVisibility/TBtn_RemoveWarning"), QStringLiteral("消除报警")},
        {QStringLiteral("ButtonVisibility/steeringModeSelector"), QStringLiteral("转向模式")},
    };
    if (group == QLatin1String("DisplayRegisterMapping")) {
        const QStringList names = DisplayRegisterMapping::knownObjectNames();
        for (const QString &name : names) {
            const QString prefix = name + QLatin1Char('_');
            if (key.startsWith(prefix)) {
                const QString field = key.mid(prefix.size());
                static const QMap<QString, QString> fieldLabels = {
                    {QStringLiteral("device"), QStringLiteral("设备")},
                    {QStringLiteral("start"), QStringLiteral("起始地址")},
                    {QStringLiteral("count"), QStringLiteral("字数")},
                    {QStringLiteral("encoding"), QStringLiteral("编码")},
                    {QStringLiteral("scale"), QStringLiteral("倍率")},
                    {QStringLiteral("sum_start"), QStringLiteral("求和起始")},
                    {QStringLiteral("second_start"), QStringLiteral("副值起始")},
                    {QStringLiteral("second_count"), QStringLiteral("副值字数")},
                };
                return DisplayRegisterMapping::displayTitle(name)
                    + QStringLiteral(" ")
                    + fieldLabels.value(field, field);
            }
        }
    }
    return labels.value(id, id);
}

QString displayConfigValue(const QString &value)
{
    if (value == QLatin1String("true")) {
        return QStringLiteral("开启");
    }
    if (value == QLatin1String("false")) {
        return QStringLiteral("关闭");
    }
    return value;
}

} // namespace

FeatureSwitchWidget::FeatureSwitchWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
    loadCurrentState();
    m_virtualKeyboard = new TechVirtualKeyboard(this);

    const QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (QLineEdit *edit : edits) {
        edit->installEventFilter(this);
    }

    setWindowTitle("功能开关管理 (厂家权限)");
    setStyleSheet(
        "QWidget { background-color: #071321; color: #d8f6ff; font-family: 'Microsoft YaHei UI'; }"
        "QGroupBox { background: rgba(5, 20, 38, 0.86); border: 1px solid rgba(74, 190, 238, 0.42); border-radius: 12px; margin-top: 16px; font-weight: bold; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 2px 8px; color: #a8eaff; background: #071321; border-radius: 5px; }"
        "QCheckBox { spacing: 10px; padding: 5px; color: #d8f6ff; }"
        "QCheckBox::indicator { width: 22px; height: 22px; border: 1px solid rgba(74, 190, 238, 0.70); border-radius: 5px; background: rgba(3, 15, 28, 0.92); }"
        "QCheckBox::indicator:checked { background-color: #00a8dc; border: 1px solid #9ff4ff; }"
        "QPushButton { background: rgba(14, 58, 90, 0.88); border: 1px solid rgba(74, 190, 238, 0.52); border-radius: 8px; padding: 8px 20px; color: #eaf9ff; font-weight: bold; }"
        "QPushButton:hover { background: rgba(25, 92, 132, 0.94); border: 1px solid #6fe7ff; }"
        "QLineEdit { background: rgba(3, 15, 28, 0.90); color: #f0fbff; border: 1px solid rgba(74, 190, 238, 0.52); border-radius: 6px; padding: 4px 6px; }"
        "QLineEdit:focus { border: 1px solid #9ff4ff; }"
        "QScrollArea { border: none; background-color: transparent; }"
        "QTabWidget::pane { border: 1px solid rgba(74, 190, 238, 0.42); border-radius: 8px; }"
        "QTabBar::tab { background: rgba(8, 28, 48, 0.92); color: #a8eaff; padding: 8px 16px; margin-right: 2px; border: 1px solid rgba(74, 190, 238, 0.35); border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: rgba(0, 126, 186, 0.55); color: #eaf9ff; }"
        "#consoleHint { color: #91dfff; font-size: 12px; font-weight: normal; }"
        "#consoleHeader { background: rgba(8, 28, 50, 0.92); border: 1px solid rgba(74, 190, 238, 0.42); border-radius: 10px; }"
        "#consoleTitle { color: #eaf9ff; font-size: 22px; font-weight: bold; }"
        "#consoleSubtitle { color: #91dfff; font-size: 12px; }"
        "#consoleBadge { background: rgba(74, 38, 108, 0.88); color: #ffe28f; border: 1px solid rgba(255, 210, 112, 0.70); border-radius: 8px; padding: 2px 10px; font-size: 12px; }"
        "#consoleFooter { background: rgba(5, 18, 34, 0.96); border-top: 1px solid rgba(74, 190, 238, 0.42); }"
        "QWidget#modbusCard { background-color: #0a1c28; border: 1px solid #1e5570; border-radius: 6px; }"
        "QLabel#cardTitle { color: #e8fbff; font-weight: bold; font-size: 13px; }"
        "QLabel#cardMeta { color: #6a9eb0; font-size: 11px; font-weight: normal; }"
        "QLabel#sectionLabel { color: #4ec8ef; font-size: 11px; font-weight: bold; padding-top: 4px; }"
    );

    resize(980, 820);
}

bool FeatureSwitchWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(watched);
        if (lineEdit && lineEdit->isEnabled() && m_virtualKeyboard) {
            m_virtualKeyboard->setTargetLineEdit(lineEdit);
            m_virtualKeyboard->showAtWidget(lineEdit);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FeatureSwitchWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshTechSliderEditList();
    refreshButtonVisibilityList();
    loadCurrentState();

    if (QWidget *host = parentWidget()) {
        const QPoint topLeft = host->mapToGlobal(QPoint(0, 0));
        const QRect hostRect(topLeft, host->size());
        QRect geo = geometry();
        geo.moveCenter(hostRect.center());
        if (QScreen *screen = QGuiApplication::screenAt(hostRect.center())) {
            const QRect avail = screen->availableGeometry();
            if (geo.right() > avail.right()) {
                geo.moveRight(avail.right());
            }
            if (geo.bottom() > avail.bottom()) {
                geo.moveBottom(avail.bottom());
            }
            if (geo.left() < avail.left()) {
                geo.moveLeft(avail.left());
            }
            if (geo.top() < avail.top()) {
                geo.moveTop(avail.top());
            }
        }
        setGeometry(geo);
    }
}

void FeatureSwitchWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(10);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("consoleHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("系统功能控制台"), header);
    title->setObjectName(QStringLiteral("consoleTitle"));
    auto *subtitle = new QLabel(QStringLiteral("厂家运行时配置 · 立即生效写入 config.ini；功能开关需点「保存并写入 INI」才会写入 feature_switches.ini"), header);
    subtitle->setObjectName(QStringLiteral("consoleSubtitle"));
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);

    auto *badge = new QLabel(QStringLiteral("厂家权限"), header);
    badge->setObjectName(QStringLiteral("consoleBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(24);

    headerLayout->addLayout(titleBlock, 1);
    headerLayout->addWidget(badge, 0, Qt::AlignTop);
    mainLayout->addWidget(header);

    auto *tabs = new QTabWidget(this);
    tabs->setUsesScrollButtons(true);

    auto makeTabLayout = [tabs](const QString &tabTitle) -> QVBoxLayout* {
        auto *page = new QWidget(tabs);
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        auto *scroll = new QScrollArea(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *scrollContent = new QWidget(scroll);
        auto *contentLayout = new QVBoxLayout(scrollContent);
        contentLayout->setContentsMargins(12, 12, 12, 16);
        contentLayout->setSpacing(10);
        scroll->setWidget(scrollContent);
        pageLayout->addWidget(scroll);
        tabs->addTab(page, tabTitle);
        return contentLayout;
    };

    QVBoxLayout *baseLayout = makeTabLayout(QStringLiteral("基础开关"));
    QVBoxLayout *commLayout = makeTabLayout(QStringLiteral("通信参数"));
    QVBoxLayout *displayLayout = makeTabLayout(QStringLiteral("显示范围"));
    QVBoxLayout *controlLayout = makeTabLayout(QStringLiteral("控件配置"));
    QVBoxLayout *logsLayout = makeTabLayout(QStringLiteral("日志调试"));

    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();

    QGroupBox *bigGroup = new QGroupBox(QStringLiteral("核心功能模块"));
    QVBoxLayout *bigOuter = new QVBoxLayout(bigGroup);
    bigOuter->addWidget(makeHintLabel(
        QStringLiteral("关闭大模块将连带禁用其下全部子功能。悬停复选框可查看内部键名。"), bigGroup));
    auto *bigLayout = new QGridLayout();
    bigLayout->setHorizontalSpacing(16);
    bigOuter->addLayout(bigLayout);

    const QMap<QString, QString> desc = {
        {QStringLiteral("startup_checks"), QStringLiteral("启动自检")},
        {QStringLiteral("ui_navigation"), QStringLiteral("界面导航")},
        {QStringLiteral("permission_system"), QStringLiteral("权限体系")},
        {QStringLiteral("operation_records"), QStringLiteral("操作记录")},
        {QStringLiteral("tcp_transmission"), QStringLiteral("TCP 上报")},
        {QStringLiteral("modbus_main"), QStringLiteral("主控 Modbus")},
        {QStringLiteral("modbus_agv"), QStringLiteral("AGV Modbus")},
        {QStringLiteral("motion_control"), QStringLiteral("运动控制")},
        {QStringLiteral("input_devices"), QStringLiteral("输入设备")},
        {QStringLiteral("alarm_system"), QStringLiteral("报警系统")},
    };

    QStringList bigKeys = mgr->allBigFeatures().values();
    bigKeys.sort();
    for (int i = 0; i < bigKeys.size(); ++i) {
        const QString key = bigKeys.at(i);
        auto *cb = new QCheckBox(desc.value(key, key));
        cb->setToolTip(key);
        bigLayout->addWidget(cb, i / 2, i % 2);
        m_bigCheckboxes[key] = cb;
    }
    baseLayout->addWidget(bigGroup);

    QGroupBox *smallGroup = new QGroupBox(QStringLiteral("子功能细项"));
    QVBoxLayout *smallOuter = new QVBoxLayout(smallGroup);
    smallOuter->addWidget(makeHintLabel(
        QStringLiteral("按业务类别分组；本机 / 远程 TCP 模拟器互斥，只能启用其一。"), smallGroup));

    const QMap<QString, QString> sdesc = {
        {QStringLiteral("startup.clear_servo_alarm"), QStringLiteral("启动清除伺服报警")},
        {QStringLiteral("startup.write_registers"), QStringLiteral("启动写寄存器")},
        {QStringLiteral("startup.log_report"), QStringLiteral("启动日志报告")},
        {QStringLiteral("ui.styles"), QStringLiteral("界面样式")},
        {QStringLiteral("ui.animations"), QStringLiteral("界面动画")},
        {QStringLiteral("ui.virtual_keyboard"), QStringLiteral("虚拟键盘")},
        {QStringLiteral("permission.admin_login"), QStringLiteral("管理员登录")},
        {QStringLiteral("permission.password_hint"), QStringLiteral("权限页密码提示")},
        {QStringLiteral("records.filter_export"), QStringLiteral("记录筛选与导出")},
        {QStringLiteral("tcp.send_all"), QStringLiteral("TCP 全量发送")},
        {QStringLiteral("tcp.local_simulator"), QStringLiteral("本机 TCP 模拟器 (127.0.0.1)")},
        {QStringLiteral("tcp.remote_simulator"), QStringLiteral("远程 TCP 模拟器 (192.168.1.70)")},
        {QStringLiteral("tcp.local_operation_history"), QStringLiteral("本机历史记录传输 (127.0.0.1:12345)")},
        {QStringLiteral("modbus_main.polling"), QStringLiteral("主控轮询")},
        {QStringLiteral("modbus_main.float_reading"), QStringLiteral("浮点解析")},
        {QStringLiteral("modbus_main.read_logs"), QStringLiteral("主设备 Modbus 读日志")},
        {QStringLiteral("modbus_main.write_logs"), QStringLiteral("主设备 Modbus 写日志")},
        {QStringLiteral("modbus_agv.read_logs"), QStringLiteral("AGV Modbus 读日志")},
        {QStringLiteral("modbus_agv.write_logs"), QStringLiteral("AGV Modbus 写日志")},
        {QStringLiteral("agv.fault_codes"), QStringLiteral("AGV 故障码")},
        {QStringLiteral("agv.speed_gauge"), QStringLiteral("AGV 速度表")},
        {QStringLiteral("motion.steering_mode"), QStringLiteral("转向模式")},
        {QStringLiteral("motion.speed_mode"), QStringLiteral("速度模式")},
        {QStringLiteral("motion.step_mode"), QStringLiteral("步进/点动")},
        {QStringLiteral("motion.force_control"), QStringLiteral("力控参与运动")},
        {QStringLiteral("input.matrix_key"), QStringLiteral("矩阵按键")},
        {QStringLiteral("input.enable_button"), QStringLiteral("使能按钮")},
        {QStringLiteral("alarm.emergency_stop"), QStringLiteral("急停报警")},
        {QStringLiteral("alarm.steering_switch"), QStringLiteral("转向模式切换报警")},
        {QStringLiteral("alarm.popup"), QStringLiteral("报警弹窗显示")},
        {QStringLiteral("alarm.status_logs"), QStringLiteral("报警状态周期日志")},
        {QStringLiteral("debug.qdebug"), QStringLiteral("全局调试输出 (qDebug)")},
    };

    const QSet<QString> logSwitchKeys = {
        QStringLiteral("modbus_main.read_logs"),
        QStringLiteral("modbus_main.write_logs"),
        QStringLiteral("modbus_agv.read_logs"),
        QStringLiteral("modbus_agv.write_logs"),
        QStringLiteral("alarm.status_logs"),
        QStringLiteral("debug.qdebug")
    };

    QStringList smallKeys = mgr->allSmallFeatures().values();
    smallKeys.sort();

    QMap<QString, QStringList> groupedKeys;
    for (const QString &key : smallKeys) {
        if (logSwitchKeys.contains(key)) {
            continue;
        }
        groupedKeys[smallFeatureGroupTitle(key)].append(key);
    }

    const QStringList groupOrder = {
        QStringLiteral("启动"), QStringLiteral("界面"), QStringLiteral("权限"),
        QStringLiteral("操作记录"), QStringLiteral("TCP 上报"),
        QStringLiteral("主控 Modbus"), QStringLiteral("AGV Modbus"), QStringLiteral("AGV 显示"),
        QStringLiteral("运动控制"), QStringLiteral("输入设备"), QStringLiteral("报警"),
    };
    QStringList orderedGroups = groupOrder;
    for (auto it = groupedKeys.constBegin(); it != groupedKeys.constEnd(); ++it) {
        if (!orderedGroups.contains(it.key())) {
            orderedGroups.append(it.key());
        }
    }

    for (const QString &groupTitle : orderedGroups) {
        if (!groupedKeys.contains(groupTitle) || groupedKeys.value(groupTitle).isEmpty()) {
            continue;
        }
        auto *section = new QGroupBox(groupTitle, smallGroup);
        section->setFlat(true);
        auto *grid = new QGridLayout(section);
        grid->setHorizontalSpacing(14);
        const QStringList keys = groupedKeys.value(groupTitle);
        for (int i = 0; i < keys.size(); ++i) {
            const QString &key = keys.at(i);
            auto *cb = new QCheckBox(sdesc.value(key, key), section);
            cb->setToolTip(key == QStringLiteral("tcp.local_operation_history")
                               ? QStringLiteral("tcp.local_operation_history：只把历史记录链路切到 127.0.0.1:12345")
                               : key);
            grid->addWidget(cb, i / 2, i % 2);
            m_smallCheckboxes[key] = cb;
            if (key == QStringLiteral("tcp.local_simulator")) {
                connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                    if (checked && m_smallCheckboxes.contains(QStringLiteral("tcp.remote_simulator"))) {
                        m_smallCheckboxes[QStringLiteral("tcp.remote_simulator")]->setChecked(false);
                    }
                });
            } else if (key == QStringLiteral("tcp.remote_simulator")) {
                connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
                    if (checked && m_smallCheckboxes.contains(QStringLiteral("tcp.local_simulator"))) {
                        m_smallCheckboxes[QStringLiteral("tcp.local_simulator")]->setChecked(false);
                    }
                });
            }
        }
        smallOuter->addWidget(section);
    }
    baseLayout->addWidget(smallGroup);
    setupPermissionPasswordUI(baseLayout);

    QGroupBox *logGroup = new QGroupBox(QStringLiteral("日志与调试开关"));
    auto *logOuter = new QVBoxLayout(logGroup);
    logOuter->addWidget(makeHintLabel(
        QStringLiteral("仅影响日志输出量，不影响业务功能本身。生产环境建议保持关闭。"), logGroup));
    auto *logLayout = new QGridLayout();
    logOuter->addLayout(logLayout);
    QStringList logKeys = logSwitchKeys.values();
    logKeys.sort();
    for (int i = 0; i < logKeys.size(); ++i) {
        const QString key = logKeys.at(i);
        auto *cb = new QCheckBox(sdesc.value(key, key));
        cb->setToolTip(key);
        logLayout->addWidget(cb, i / 2, i % 2);
        m_smallCheckboxes[key] = cb;
    }
    logsLayout->addWidget(logGroup);

    setupPollingUI(commLayout);
    setupSliderLimitUI(displayLayout);
    setupPlaneHeightOffsetUI(displayLayout);
    setupRobotTorqueUI(displayLayout);
    setupButtonVisibilityUI(controlLayout);

    baseLayout->addStretch();
    commLayout->addStretch();
    displayLayout->addStretch();
    controlLayout->addStretch();
    logsLayout->addStretch();
    mainLayout->addWidget(tabs, 1);

    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("consoleFooter"));
    footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    footer->setMinimumHeight(56);
    auto *btnLayout = new QHBoxLayout(footer);
    btnLayout->setContentsMargins(12, 10, 12, 10);

    auto *btnAll = new QPushButton(QStringLiteral("开启全部"), footer);
    connect(btnAll, &QPushButton::clicked, this, [this]() { onToggleAll(true); });
    auto *btnNone = new QPushButton(QStringLiteral("关闭全部"), footer);
    connect(btnNone, &QPushButton::clicked, this, [this]() { onToggleAll(false); });
    auto *btnReload = new QPushButton(QStringLiteral("撤销修改"), footer);
    btnReload->setToolTip(QStringLiteral("从当前 INI 重新加载，丢弃未保存的界面修改"));
    connect(btnReload, &QPushButton::clicked, this, &FeatureSwitchWidget::onReload);

    auto *btnApply = new QPushButton(QStringLiteral("立即生效"), footer);
    btnApply->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1a5f9a; border: 1px solid #4aa8e0; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #2474b8; }"));
    connect(btnApply, &QPushButton::clicked, this, &FeatureSwitchWidget::onApply);

    auto *btnSave = new QPushButton(QStringLiteral("保存并写入 INI"), footer);
    btnSave->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1a7a4a; border: 1px solid #3dca7a; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #21965c; }"));
    connect(btnSave, &QPushButton::clicked, this, &FeatureSwitchWidget::onSave);

    auto *btnClose = new QPushButton(QStringLiteral("退出"), footer);
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #8a2e2e; border: 1px solid #d06060; border-radius: 5px;"
        " padding: 8px 16px; color: #e8fbff; font-weight: bold; }"
        "QPushButton:hover { background-color: #a83838; }"));
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    btnLayout->addWidget(btnAll);
    btnLayout->addWidget(btnNone);
    btnLayout->addWidget(btnReload);
    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnClose);
    mainLayout->addWidget(footer, 0);
}

void FeatureSwitchWidget::loadCurrentState()
{
    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    for (auto it = m_bigCheckboxes.begin(); it != m_bigCheckboxes.end(); ++it) {
        it.value()->setChecked(mgr->isBigFeatureEnabled(it.key()));
    }
    for (auto it = m_smallCheckboxes.begin(); it != m_smallCheckboxes.end(); ++it) {
        it.value()->setChecked(mgr->isSmallFeatureEnabled(it.key()));
    }
    loadPollingState();
    loadSliderLimitState();
    loadDisplayRegisterState();
    loadTechSliderEditState();
    loadPlaneHeightOffsetState();
    loadRobotTorqueState();
    loadPermissionPasswordState();
    loadButtonVisibilityState();
}

void FeatureSwitchWidget::setupPollingUI(QVBoxLayout *scrollLayout)
{
    auto *pollGroup = new QGroupBox(QStringLiteral("通信轮询参数"));
    auto *pollLayout = new QVBoxLayout(pollGroup);
    pollLayout->addWidget(makeHintLabel(
        QStringLiteral("单位均为毫秒（ms），除非另行说明。设备状态数量建议保持默认 85。关闭状态同步只会阻止可写控件被寄存器回读覆盖。"),
        pollGroup));

    auto *syncRow = new QHBoxLayout();
    m_cbMainUiStateSync = new QCheckBox(QStringLiteral("启用主控控件状态同步"));
    m_cbAgvUiStateSync = new QCheckBox(QStringLiteral("启用AGV控件状态同步"));
    syncRow->addWidget(m_cbMainUiStateSync);
    syncRow->addWidget(m_cbAgvUiStateSync);
    syncRow->addStretch();
    pollLayout->addLayout(syncRow);

    auto addPollGrid = [&](const QString &sectionTitle,
                           const QList<QPair<QString, QLineEdit**>> &items) {
        auto *section = new QGroupBox(sectionTitle, pollGroup);
        section->setFlat(true);
        auto *grid = new QGridLayout(section);
        grid->setHorizontalSpacing(10);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);
        for (int i = 0; i < items.size(); ++i) {
            const int row = i / 2;
            const int col = (i % 2) * 2;
            auto *lbl = new QLabel(items.at(i).first, section);
            lbl->setWordWrap(true);
            QLineEdit *&editRef = *items.at(i).second;
            editRef = new QLineEdit(section);
            editRef->setMinimumWidth(88);
            editRef->setMaximumWidth(140);
            editRef->setValidator(new QIntValidator(0, 1000000, section));
            editRef->installEventFilter(this);
            grid->addWidget(lbl, row, col);
            grid->addWidget(editRef, row, col + 1);
        }
        pollLayout->addWidget(section);
    };

    addPollGrid(QStringLiteral("主控"), {
        {QStringLiteral("Modbus 轮询"), &m_editMainModbusPoll},
        {QStringLiteral("控件同步轮询"), &m_editMainUiPoll},
        {QStringLiteral("设备状态轮询 (0~84)"), &m_editMainDeviceStatusPoll},
        {QStringLiteral("设备状态起始地址"), &m_editMainDeviceStatusStart},
        {QStringLiteral("设备状态数量"), &m_editMainDeviceStatusCount},
        {QStringLiteral("模式同步起始地址"), &m_editMainControlSyncStart},
        {QStringLiteral("模式同步数量"), &m_editMainControlSyncCount},
        {QStringLiteral("重连间隔"), &m_editMainReconnect},
    });
    addPollGrid(QStringLiteral("AGV"), {
        {QStringLiteral("Modbus 轮询"), &m_editAgvPoll},
        {QStringLiteral("重连间隔"), &m_editAgvReconnect},
    });

    wireIntEdit(m_editMainModbusPoll, 50, 60000, 500);
    wireIntEdit(m_editMainUiPoll, 50, 60000, 200);
    wireIntEdit(m_editMainDeviceStatusPoll, 50, 60000, 2000);
    wireIntEdit(m_editMainDeviceStatusStart, 0, 65535, 0);
    wireIntEdit(m_editMainDeviceStatusCount, 1, 125, 85);
    wireIntEdit(m_editMainControlSyncStart, 0, 65535, 125);
    wireIntEdit(m_editMainControlSyncCount, 1, 125, 6);
    wireIntEdit(m_editMainReconnect, 500, 120000, 5000);
    wireIntEdit(m_editAgvPoll, 50, 60000, 200);
    wireIntEdit(m_editAgvReconnect, 500, 120000, 5000);

    scrollLayout->addWidget(pollGroup);
}

void FeatureSwitchWidget::setupSliderLimitUI(QVBoxLayout *scrollLayout)
{
    auto *arcGroup = new QGroupBox(QStringLiteral("环形仪表显示与范围"));
    auto *arcLayout = new QVBoxLayout(arcGroup);
    arcLayout->addWidget(makeHintLabel(
        QStringLiteral("控制首页 / 六轴页环形仪表是否显示、数值 Min/Max，以及只读寄存器起始地址（立即生效，不改 PLC 轮询窗口）。"),
        arcGroup));

    auto *arcToolbar = new QHBoxLayout();
    auto *arcShowAll = new QPushButton(QStringLiteral("仪表全部显示"));
    auto *arcHideAll = new QPushButton(QStringLiteral("仪表全部隐藏"));
    arcToolbar->addWidget(arcShowAll);
    arcToolbar->addWidget(arcHideAll);
    arcToolbar->addStretch();
    arcLayout->addLayout(arcToolbar);

    const QStringList arcGaugeNames = DisplayRegisterMapping::arcGaugeObjectNames();

    for (int i = 0; i < arcGaugeNames.size(); ++i) {
        const QString &name = arcGaugeNames.at(i);
        auto *card = new QWidget(arcGroup);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 2, 0, 2);
        cardLayout->setSpacing(3);

        auto *rangeRow = new QHBoxLayout();
        auto *visibleCb = new QCheckBox(QStringLiteral("显示"));
        visibleCb->setFixedWidth(52);
        auto *lbl = new QLabel(DisplayRegisterMapping::displayTitle(name));
        lbl->setMinimumWidth(110);
        lbl->setToolTip(name);
        QLineEdit *minEdit = makeLimitEdit(arcGroup);
        minEdit->setPlaceholderText(QStringLiteral("Min"));
        QLineEdit *maxEdit = makeLimitEdit(arcGroup);
        maxEdit->setPlaceholderText(QStringLiteral("Max"));
        rangeRow->addWidget(visibleCb);
        rangeRow->addWidget(lbl);
        rangeRow->addWidget(minEdit);
        rangeRow->addWidget(new QLabel(QStringLiteral("~")));
        rangeRow->addWidget(maxEdit);
        rangeRow->addStretch();
        cardLayout->addLayout(rangeRow);

        ArcGaugeEdits edits;
        edits.visible = visibleCb;
        edits.minEdit = minEdit;
        edits.maxEdit = maxEdit;
        edits.registers = makeDisplayRegisterEdits(card, name);
        addDisplayRegisterRow(card, cardLayout, name, edits.registers);
        m_arcGaugeEdits[name] = edits;
        arcLayout->addWidget(card);
    }

    connect(arcShowAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(arcHideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });

    scrollLayout->addWidget(arcGroup);

    auto *statusGroup = new QGroupBox(QStringLiteral("其它状态控件读寄存器"));
    auto *statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->addWidget(makeHintLabel(
        QStringLiteral("负载、支撑力、功率、扭矩、倾角、行驶速度只配置读地址，不配置写。"),
        statusGroup));
    const QStringList statusNames = DisplayRegisterMapping::statusObjectNames();
    for (const QString &name : statusNames) {
        auto *card = new QWidget(statusGroup);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 2, 0, 2);
        cardLayout->setSpacing(3);
        auto *titleRow = new QHBoxLayout();
        auto *lbl = new QLabel(DisplayRegisterMapping::displayTitle(name));
        lbl->setMinimumWidth(110);
        lbl->setToolTip(name);
        titleRow->addWidget(lbl);
        titleRow->addStretch();
        cardLayout->addLayout(titleRow);
        DisplayRegisterEdits edits = makeDisplayRegisterEdits(card, name);
        addDisplayRegisterRow(card, cardLayout, name, edits);
        m_statusRegisterEdits[name] = edits;
        statusLayout->addWidget(card);
    }
    scrollLayout->addWidget(statusGroup);

    setupTechSliderEditUI(scrollLayout);
}

void FeatureSwitchWidget::setupTechSliderEditUI(QVBoxLayout *scrollLayout)
{
    auto *group = new QGroupBox(QStringLiteral("滑块输入显示与范围"));
    auto *layout = new QVBoxLayout(group);
    layout->addWidget(makeHintLabel(
        QStringLiteral("设置是否显示及滑块两端范围（Min/Max）；输入框数值范围会与显示范围保持一致。"),
        group));

    auto *toolbar = new QHBoxLayout();
    auto *showAll = new QPushButton(QStringLiteral("滑块全部显示"));
    auto *hideAll = new QPushButton(QStringLiteral("滑块全部隐藏"));
    toolbar->addWidget(showAll);
    toolbar->addWidget(hideAll);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_sliderEditListHost = new QWidget(group);
    m_sliderEditListLayout = new QVBoxLayout(m_sliderEditListHost);
    m_sliderEditListLayout->setContentsMargins(0, 0, 0, 0);
    m_sliderEditListLayout->setSpacing(4);
    layout->addWidget(m_sliderEditListHost);

    connect(showAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(hideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });

    scrollLayout->addWidget(group);
    refreshTechSliderEditList();
}

void FeatureSwitchWidget::refreshTechSliderEditList()
{
    if (!m_sliderEditListLayout) {
        return;
    }

    while (QLayoutItem *item = m_sliderEditListLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        } else if (QLayout *child = item->layout()) {
            while (QLayoutItem *inner = child->takeAt(0)) {
                if (QWidget *innerWidget = inner->widget()) {
                    innerWidget->deleteLater();
                }
                delete inner;
            }
        }
        delete item;
    }
    m_sliderEditEdits.clear();

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QList<TechSliderEdit*> sliders;
    if (mainWindow) {
        sliders = mainWindow->findChildren<TechSliderEdit*>();
    }
    std::sort(sliders.begin(), sliders.end(), [](TechSliderEdit *a, TechSliderEdit *b) {
        return a->objectName() < b->objectName();
    });

    if (sliders.isEmpty()) {
        m_sliderEditListLayout->addWidget(new QLabel(
            QStringLiteral("未找到滑块输入控件（请确认主窗口已初始化）"), m_sliderEditListHost));
        return;
    }

    for (TechSliderEdit *slider : sliders) {
        const QString name = slider->objectName();
        if (name.isEmpty()) {
            continue;
        }

        auto *rowHost = new QWidget(m_sliderEditListHost);
        auto *row = new QHBoxLayout(rowHost);
        row->setContentsMargins(0, 0, 0, 0);
        auto *visibleCb = new QCheckBox(QStringLiteral("显示"), rowHost);
        visibleCb->setFixedWidth(52);
        QString title = slider->labelText().trimmed();
        if (title.isEmpty()) {
            title = name;
        }
        auto *lbl = new QLabel(title, rowHost);
        lbl->setMinimumWidth(120);
        lbl->setToolTip(name);
        QLineEdit *displayMin = makeLimitEdit(rowHost);
        QLineEdit *displayMax = makeLimitEdit(rowHost);
        displayMin->setPlaceholderText(QStringLiteral("Min"));
        displayMax->setPlaceholderText(QStringLiteral("Max"));
        row->addWidget(visibleCb);
        row->addWidget(lbl);
        row->addWidget(displayMin);
        row->addWidget(new QLabel(QStringLiteral("~"), rowHost));
        row->addWidget(displayMax);
        row->addStretch();
        m_sliderEditListLayout->addWidget(rowHost);
        m_sliderEditEdits[name] = {visibleCb, displayMin, displayMax};
    }
}

void FeatureSwitchWidget::setupPermissionPasswordUI(QVBoxLayout *scrollLayout)
{
    auto *group = new QGroupBox(QStringLiteral("权限密码"));
    auto *layout = new QVBoxLayout(group);
    layout->addWidget(makeHintLabel(
        QStringLiteral("厂家密码固定，不可在此修改。工程师与管理员密码保存后立即用于权限页登录。"),
        group));

    auto addRow = [this, layout](const QString &desc, QLineEdit *&edit) {
        auto *row = new QHBoxLayout();
        auto *label = new QLabel(desc);
        label->setMinimumWidth(110);
        edit = new QLineEdit();
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(180);
        edit->setMaxLength(32);
        edit->setAlignment(Qt::AlignCenter);
        edit->installEventFilter(this);
        row->addWidget(label);
        row->addWidget(edit);
        row->addStretch();
        layout->addLayout(row);
    };

    addRow(QStringLiteral("工程师密码"), m_editEngineerPassword);
    addRow(QStringLiteral("管理员密码"), m_editAdminPassword);

    auto *mfgRow = new QHBoxLayout();
    auto *mfgLabel = new QLabel(QStringLiteral("厂家密码"));
    mfgLabel->setMinimumWidth(110);
    auto *mfgHint = new QLabel(QStringLiteral("固定，不可更改"));
    mfgHint->setObjectName(QStringLiteral("consoleHint"));
    mfgRow->addWidget(mfgLabel);
    mfgRow->addWidget(mfgHint);
    mfgRow->addStretch();
    layout->addLayout(mfgRow);

    scrollLayout->addWidget(group);
}

void FeatureSwitchWidget::setupPlaneHeightOffsetUI(QVBoxLayout *scrollLayout)
{
    auto *planeGroup = new QGroupBox(QStringLiteral("平面高度偏移"));
    auto *planeLayout = new QVBoxLayout(planeGroup);
    planeLayout->addWidget(makeHintLabel(
        QStringLiteral("首页平面高度 = J2 升降高度 − 偏移量，单位：mm。默认 1900。"), planeGroup));

    auto *h = new QHBoxLayout();
    h->addWidget(new QLabel(QStringLiteral("偏移量")));
    m_editPlaneHeightOffset = new QLineEdit();
    m_editPlaneHeightOffset->setFixedWidth(120);
    m_editPlaneHeightOffset->setValidator(new QDoubleValidator(-1000000.0, 1000000.0, 0, this));
    m_editPlaneHeightOffset->installEventFilter(this);
    h->addWidget(m_editPlaneHeightOffset);
    h->addWidget(new QLabel(QStringLiteral("mm")));
    h->addStretch();
    planeLayout->addLayout(h);
    wireDoubleEdit(m_editPlaneHeightOffset, -100000.0, 100000.0, 1900.0);
    scrollLayout->addWidget(planeGroup);
}

void FeatureSwitchWidget::setupRobotTorqueUI(QVBoxLayout *scrollLayout)
{
    auto *group = new QGroupBox(QStringLiteral("总扭矩危险阈值"));
    auto *layout = new QVBoxLayout(group);
    layout->addWidget(makeHintLabel(
        QStringLiteral("首页总扭矩卡片按百分比显示。达到或超过该阈值时切换为红色危险样式（与角度力控超限同色）。默认 140。"),
        group));

    auto *h = new QHBoxLayout();
    h->addWidget(new QLabel(QStringLiteral("危险阈值")));
    m_editRobotTorqueDanger = new QLineEdit();
    m_editRobotTorqueDanger->setFixedWidth(120);
    m_editRobotTorqueDanger->setValidator(new QDoubleValidator(1.0, 999.0, 0, this));
    m_editRobotTorqueDanger->installEventFilter(this);
    h->addWidget(m_editRobotTorqueDanger);
    h->addWidget(new QLabel(QStringLiteral("%")));
    h->addStretch();
    layout->addLayout(h);
    wireDoubleEdit(m_editRobotTorqueDanger, 1.0, 999.0, 140.0);
    scrollLayout->addWidget(group);
}

FeatureSwitchWidget::ModbusRegisterEdits FeatureSwitchWidget::makeRegisterRowEdits(
    QWidget *parent, const QString &lineEditStyle)
{
    ModbusRegisterEdits edits;
    edits.device = new QComboBox(parent);
    edits.device->addItems({QStringLiteral("无"), QStringLiteral("主控"), QStringLiteral("AGV")});
    edits.device->setFixedWidth(58);

    auto makeEdit = [&](const QString &placeholder, int width) {
        auto *edit = new QLineEdit(parent);
        edit->setPlaceholderText(placeholder);
        edit->setFixedWidth(width);
        edit->setStyleSheet(lineEditStyle);
        edit->installEventFilter(this);
        return edit;
    };
    edits.address = makeEdit(QStringLiteral("寄存器"), 68);
    edits.bit = makeEdit(QStringLiteral("位，可空"), 72);
    edits.value1 = makeEdit(QStringLiteral("值1"), 68);
    edits.value2 = makeEdit(QStringLiteral("值2"), 68);
    edits.value3 = makeEdit(QStringLiteral("值3"), 68);

    const auto updateEnabled = [edits]() {
        const bool enabled = edits.device && edits.device->currentText() != QStringLiteral("无");
        const auto setEnabledAndClear = [enabled](QLineEdit *edit) {
            if (!edit) {
                return;
            }
            edit->setEnabled(enabled);
            if (!enabled) {
                edit->clear();
            }
        };
        setEnabledAndClear(edits.address);
        setEnabledAndClear(edits.bit);
        setEnabledAndClear(edits.value1);
        setEnabledAndClear(edits.value2);
        setEnabledAndClear(edits.value3);
    };
    connect(edits.device, QOverload<int>::of(&QComboBox::currentIndexChanged), parent,
            [updateEnabled](int) { updateEnabled(); });
    updateEnabled();
    return edits;
}

void FeatureSwitchWidget::addModbusRegisterRow(QHBoxLayout *row,
                                               const QString &label,
                                               const ModbusRegisterEdits &edits,
                                               const QString &syncHint)
{
    QLabel *lbl = new QLabel(label);
    lbl->setFixedWidth(28);
    lbl->setStyleSheet(QStringLiteral("color: #aaccff;"));
    row->addWidget(lbl);
    row->addWidget(edits.device);
    row->addWidget(new QLabel(QStringLiteral("寄存器")));
    row->addWidget(edits.address);
    row->addWidget(new QLabel(QStringLiteral("位")));
    row->addWidget(edits.bit);
    row->addWidget(new QLabel(QStringLiteral("值1")));
    row->addWidget(edits.value1);
    row->addWidget(new QLabel(QStringLiteral("值2")));
    row->addWidget(edits.value2);
    row->addWidget(new QLabel(QStringLiteral("值3")));
    row->addWidget(edits.value3);
    if (!syncHint.isEmpty()) {
        QLabel *hint = new QLabel(syncHint);
        hint->setStyleSheet(QStringLiteral("color: #66aa88; font-size: 10px;"));
        row->addWidget(hint);
    }
}

void FeatureSwitchWidget::applyRegisterSpecToEdits(const MainWindow::ModbusRegisterSpec &spec,
                                                   ModbusRegisterEdits &edits)
{
    if (!edits.device || !edits.address || !edits.bit || !edits.value1 || !edits.value2 || !edits.value3) {
        return;
    }
    QSignalBlocker blockerDevice(edits.device);
    const int deviceIndex = edits.device->findText(spec.device);
    edits.device->setCurrentIndex(deviceIndex >= 0 ? deviceIndex : 0);

    const bool enabled = spec.device != QStringLiteral("无");
    edits.address->setEnabled(enabled);
    edits.bit->setEnabled(enabled);
    edits.value1->setEnabled(enabled);
    edits.value2->setEnabled(enabled);
    edits.value3->setEnabled(enabled);

    edits.address->setText(spec.address);
    edits.bit->setText(spec.bit);
    edits.value1->setText(spec.value1);
    edits.value2->setText(spec.value2);
    edits.value3->setText(spec.value3);
    if (!enabled) {
        edits.address->clear();
        edits.bit->clear();
        edits.value1->clear();
        edits.value2->clear();
        edits.value3->clear();
    }
}

MainWindow::ModbusRegisterSpec FeatureSwitchWidget::readRegisterSpecFromEdits(
    const ModbusRegisterEdits &edits) const
{
    MainWindow::ModbusRegisterSpec spec;
    if (!edits.device || !edits.address || !edits.bit || !edits.value1 || !edits.value2 || !edits.value3) {
        return spec;
    }
    spec.device = edits.device->currentText().trimmed();
    spec.address = edits.address->text().trimmed();
    spec.bit = edits.bit->text().trimmed();
    spec.value1 = edits.value1->text().trimmed();
    spec.value2 = edits.value2->text().trimmed();
    spec.value3 = edits.value3->text().trimmed();
    return spec;
}

void FeatureSwitchWidget::setupButtonVisibilityUI(QVBoxLayout *scrollLayout)
{
    setupDeviceControlChildVisibilityUI(scrollLayout);

    m_modbusButtonGroup = new QGroupBox(QStringLiteral("Modbus 按键：显示与寄存器"));
    QVBoxLayout *modbusLayout = new QVBoxLayout(m_modbusButtonGroup);
    modbusLayout->addWidget(makeHintLabel(
        QStringLiteral("自动扫描主窗口中带 Modbus 读写的按键。每行：设备 / 寄存器 / 位（可空）/ 值1~3。"
                       "读行用于界面状态同步；默认值来自程序内置逻辑。"),
        m_modbusButtonGroup));

    QHBoxLayout *modbusToolbar = new QHBoxLayout();
    QPushButton *modbusShowAll = new QPushButton(QStringLiteral("全部显示"));
    QPushButton *modbusHideAll = new QPushButton(QStringLiteral("全部隐藏"));
    modbusToolbar->addWidget(modbusShowAll);
    modbusToolbar->addWidget(modbusHideAll);
    modbusToolbar->addStretch();
    modbusLayout->addLayout(modbusToolbar);

    QScrollArea *modbusScroll = new QScrollArea();
    modbusScroll->setWidgetResizable(true);
    modbusScroll->setMaximumHeight(420);
    modbusScroll->setFrameShape(QFrame::NoFrame);
    m_modbusButtonListHost = new QWidget();
    m_modbusButtonListLayout = new QVBoxLayout(m_modbusButtonListHost);
    m_modbusButtonListLayout->setSpacing(10);
    m_modbusButtonListLayout->setContentsMargins(2, 2, 8, 2);
    modbusScroll->setWidget(m_modbusButtonListHost);
    modbusLayout->addWidget(modbusScroll);

    connect(modbusShowAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(true);
            }
        }
    });
    connect(modbusHideAll, &QPushButton::clicked, this, [this]() {
        for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
            if (it->visible) {
                it->visible->setChecked(false);
            }
        }
    });
    scrollLayout->addWidget(m_modbusButtonGroup);

    m_otherVisibilityGroup = new QGroupBox(QStringLiteral("其它控件可见性"));
    QVBoxLayout *otherLayout = new QVBoxLayout(m_otherVisibilityGroup);
    otherLayout->addWidget(makeHintLabel(
        QStringLiteral("无 Modbus 映射的导航 / 模式等控件，仅控制是否显示。"
                       "环形仪表与滑块输入请到「显示范围」页配置。"),
        m_otherVisibilityGroup));

    QHBoxLayout *otherToolbar = new QHBoxLayout();
    QPushButton *otherShowAll = new QPushButton(QStringLiteral("全部显示"));
    QPushButton *otherHideAll = new QPushButton(QStringLiteral("全部隐藏"));
    otherToolbar->addWidget(otherShowAll);
    otherToolbar->addWidget(otherHideAll);
    otherToolbar->addStretch();
    otherLayout->addLayout(otherToolbar);

    QScrollArea *otherScroll = new QScrollArea();
    otherScroll->setWidgetResizable(true);
    otherScroll->setMaximumHeight(220);
    otherScroll->setFrameShape(QFrame::NoFrame);
    m_otherVisibilityListHost = new QWidget();
    m_otherVisibilityGrid = new QGridLayout(m_otherVisibilityListHost);
    m_otherVisibilityGrid->setHorizontalSpacing(12);
    m_otherVisibilityGrid->setVerticalSpacing(4);
    otherScroll->setWidget(m_otherVisibilityListHost);
    otherLayout->addWidget(otherScroll);

    connect(otherShowAll, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *cb : m_otherVisibilityCheckboxes) {
            cb->setChecked(true);
        }
    });
    connect(otherHideAll, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *cb : m_otherVisibilityCheckboxes) {
            cb->setChecked(false);
        }
    });
    scrollLayout->addWidget(m_otherVisibilityGroup);
    refreshButtonVisibilityList();
}

void FeatureSwitchWidget::refreshButtonVisibilityList()
{
    if (!m_modbusButtonListLayout || !m_otherVisibilityGrid) {
        return;
    }

    while (QLayoutItem *item = m_modbusButtonListLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_modbusButtonEdits.clear();

    while (QLayoutItem *item = m_otherVisibilityGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_otherVisibilityCheckboxes.clear();

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QList<MainWindow::ControllableButtonInfo> buttons;
    if (mainWindow) {
        buttons = mainWindow->controllableButtons();
    }

    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    MappingConfig *mapping = MappingConfig::instance();
    int modbusCount = 0;
    int otherRow = 0;

    for (int i = 0; i < buttons.size(); ++i) {
        const MainWindow::ControllableButtonInfo &info = buttons.at(i);
        if (shouldSkipControllable(info) || isDeviceControlChildButton(info.objectName)) {
            continue;
        }
        if (!hasModbusOperation(info) && isConsoleNoiseControl(mainWindow, info.objectName)) {
            continue;
        }

        const QString &objectName = info.objectName;
        QString visibleText = info.displayText;
        if (visibleText.isEmpty() && mapping) {
            const QString mapped = mapping->mapControlName(objectName);
            if (mapped != objectName) {
                visibleText = mapped;
            }
        }
        const QString kind = info.widgetKind.isEmpty() ? QStringLiteral("控件") : info.widgetKind;

        settings.beginGroup(QStringLiteral("ButtonVisibility"));
        const bool visibleDefault = settings.value(objectName, true).toBool();
        settings.endGroup();

        if (hasModbusOperation(info)) {
            settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
            QWidget *card = new QWidget(m_modbusButtonListHost);
            card->setObjectName(QStringLiteral("modbusCard"));
            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 8, 10, 8);
            cardLayout->setSpacing(5);

            QHBoxLayout *titleRow = new QHBoxLayout();
            QCheckBox *visibleCb = new QCheckBox(QStringLiteral("显示"), card);
            visibleCb->setChecked(visibleDefault);

            auto *titleBlock = new QVBoxLayout();
            titleBlock->setSpacing(1);
            QLabel *titleLbl = new QLabel(visibleText.isEmpty() ? kind : visibleText, card);
            titleLbl->setObjectName(QStringLiteral("cardTitle"));
            QLabel *metaLbl = new QLabel(QStringLiteral("%1 · %2").arg(kind, objectName), card);
            metaLbl->setObjectName(QStringLiteral("cardMeta"));
            metaLbl->setToolTip(objectName);
            titleBlock->addWidget(titleLbl);
            titleBlock->addWidget(metaLbl);
            titleRow->addWidget(visibleCb, 0, Qt::AlignTop);
            titleRow->addLayout(titleBlock, 1);
            cardLayout->addLayout(titleRow);

            cardLayout->addWidget(makeHintLabel(
                QStringLiteral("状态映射：第1态→值1，第2态→值2，第3态→值3；单一状态只填值1。"), card));

            QVector<ModbusRegisterEdits> readEditsList;
            QVector<ModbusRegisterEdits> writeEditsList;
            const QString readPrefix = objectName + QStringLiteral("_read");
            const QString writePrefix = objectName + QStringLiteral("_write");
            const QList<MainWindow::ModbusRegisterSpec> readSpecs =
                loadRegisterSpecs(settings, readPrefix, info.defaultReads);
            const QList<MainWindow::ModbusRegisterSpec> writeSpecs =
                loadRegisterSpecs(settings, writePrefix, info.defaultWrites);

            auto *readSection = new QLabel(QStringLiteral("读寄存器"), card);
            readSection->setObjectName(QStringLiteral("sectionLabel"));
            cardLayout->addWidget(readSection);
            for (int idx = 0; idx < kMaxTargetsPerDirection; ++idx) {
                ModbusRegisterEdits edits = makeRegisterRowEdits(card, QString());
                const QString readSyncHint = (info.readForUiSync && idx == 0)
                    ? QStringLiteral("同步") : QString();
                QHBoxLayout *readRow = new QHBoxLayout();
                addModbusRegisterRow(readRow, QStringLiteral("读%1").arg(idx + 1), edits, readSyncHint);
                cardLayout->addLayout(readRow);
                if (idx < readSpecs.size()) {
                    applyRegisterSpecToEdits(readSpecs.at(idx), edits);
                }
                readEditsList.push_back(edits);
            }

            auto *writeSection = new QLabel(QStringLiteral("写寄存器"), card);
            writeSection->setObjectName(QStringLiteral("sectionLabel"));
            cardLayout->addWidget(writeSection);
            for (int idx = 0; idx < kMaxTargetsPerDirection; ++idx) {
                ModbusRegisterEdits edits = makeRegisterRowEdits(card, QString());
                QHBoxLayout *writeRow = new QHBoxLayout();
                addModbusRegisterRow(writeRow, QStringLiteral("写%1").arg(idx + 1), edits);
                cardLayout->addLayout(writeRow);
                if (idx < writeSpecs.size()) {
                    applyRegisterSpecToEdits(writeSpecs.at(idx), edits);
                }
                writeEditsList.push_back(edits);
            }

            m_modbusButtonListLayout->addWidget(card);
            m_modbusButtonEdits[objectName] = {visibleCb, readEditsList, writeEditsList, info.readForUiSync};
            settings.endGroup();
            ++modbusCount;
        } else {
            const QString label = visibleText.isEmpty() ? kind : visibleText;
            QCheckBox *cb = new QCheckBox(label, m_otherVisibilityListHost);
            cb->setToolTip(QStringLiteral("%1 · %2").arg(kind, objectName));
            cb->setChecked(visibleDefault);
            m_otherVisibilityGrid->addWidget(cb, otherRow / 2, otherRow % 2);
            m_otherVisibilityCheckboxes[objectName] = cb;
            ++otherRow;
        }
    }

    if (modbusCount == 0) {
        m_modbusButtonListLayout->addWidget(new QLabel(
            QStringLiteral("未扫描到有 Modbus 读写的按键（请确认主窗口已初始化）"),
            m_modbusButtonListHost));
    }
    if (otherRow == 0) {
        m_otherVisibilityGrid->addWidget(new QLabel(
            QStringLiteral("无其它可配置控件"),
            m_otherVisibilityListHost), 0, 0);
    }
}

void FeatureSwitchWidget::setupDeviceControlChildVisibilityUI(QVBoxLayout *scrollLayout)
{
    auto *group = new QGroupBox(QStringLiteral("设备控制子按钮"));
    auto *layout = new QVBoxLayout(group);
    layout->addWidget(makeHintLabel(
        QStringLiteral("分别控制底部「设备控制」弹出菜单中的入口是否显示。"
                       "全部隐藏时，「设备控制」菜单本身也会收起。"),
        group));

    const QVector<QPair<QString, QString>> children = {
        {QStringLiteral("TBtn_RobotControl"), QStringLiteral("机械臂控制")},
        {QStringLiteral("TBtn_ChassisControl"), QStringLiteral("底盘控制")},
        {QStringLiteral("TBtn_SixAxies"), QStringLiteral("六自由度")},
        {QStringLiteral("TBtn_SafetyMonitor"), QStringLiteral("安全监控")},
    };

    auto *grid = new QGridLayout();
    for (int i = 0; i < children.size(); ++i) {
        auto *cb = new QCheckBox(children.at(i).second, group);
        cb->setToolTip(children.at(i).first);
        cb->setChecked(true);
        grid->addWidget(cb, i / 2, i % 2);
        m_deviceControlChildCheckboxes[children.at(i).first] = cb;
    }
    layout->addLayout(grid);
    scrollLayout->addWidget(group);
}

bool FeatureSwitchWidget::loadPollingState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Polling"));
    const bool legacyUiStateSync = settings.value(QStringLiteral("ui_state_sync_enabled"), true).toBool();
    if (m_cbMainUiStateSync) {
        m_cbMainUiStateSync->setChecked(
            settings.value(QStringLiteral("main_ui_state_sync_enabled"), legacyUiStateSync).toBool());
    }
    if (m_cbAgvUiStateSync) {
        m_cbAgvUiStateSync->setChecked(
            settings.value(QStringLiteral("agv_ui_state_sync_enabled"), legacyUiStateSync).toBool());
    }

    const auto fill = [this, &settings](QLineEdit *edit, const char *key, int fallback) {
        if (!edit) {
            return;
        }
        edit->setText(settings.value(QLatin1String(key), fallback).toString());
        rememberValidEditText(edit, edit->text());
    };
    fill(m_editMainModbusPoll, "main_modbus_poll_ms", 500);
    fill(m_editMainUiPoll, "main_ui_poll_ms", 200);
    fill(m_editMainDeviceStatusPoll, "main_device_status_poll_ms", 2000);
    fill(m_editMainDeviceStatusStart, "main_device_status_start", 0);
    fill(m_editMainDeviceStatusCount, "main_device_status_count", 85);
    fill(m_editMainControlSyncStart, "main_control_sync_start", 125);
    fill(m_editMainControlSyncCount, "main_control_sync_count", 6);
    fill(m_editMainReconnect, "main_reconnect_ms", 5000);
    fill(m_editAgvPoll, "agv_poll_ms", 200);
    fill(m_editAgvReconnect, "agv_reconnect_ms", 5000);
    settings.endGroup();
    return true;
}

bool FeatureSwitchWidget::savePollingState(QString *error)
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Polling"));
    const bool mainSync = m_cbMainUiStateSync && m_cbMainUiStateSync->isChecked();
    const bool agvSync = m_cbAgvUiStateSync && m_cbAgvUiStateSync->isChecked();
    settings.setValue(QStringLiteral("main_ui_state_sync_enabled"), mainSync);
    settings.setValue(QStringLiteral("agv_ui_state_sync_enabled"), agvSync);
    settings.setValue(QStringLiteral("ui_state_sync_enabled"), mainSync && agvSync);

    const auto saveBounded = [this, &settings, error](QLineEdit *edit, const char *key,
                                                      int lo, int hi, int fallback) {
        if (!validateIntEdit(edit, lo, hi, fallback, error)) {
            settings.setValue(QLatin1String(key), fallback);
            return false;
        }
        settings.setValue(QLatin1String(key), edit->text().trimmed().toInt());
        return true;
    };

    bool ok = true;
    ok = saveBounded(m_editMainModbusPoll, "main_modbus_poll_ms", 50, 60000, 500) && ok;
    ok = saveBounded(m_editMainUiPoll, "main_ui_poll_ms", 50, 60000, 200) && ok;
    ok = saveBounded(m_editMainDeviceStatusPoll, "main_device_status_poll_ms", 50, 60000, 2000) && ok;
    ok = saveBounded(m_editMainDeviceStatusStart, "main_device_status_start", 0, 65535, 0) && ok;
    ok = saveBounded(m_editMainDeviceStatusCount, "main_device_status_count", 1, 125, 85) && ok;
    ok = saveBounded(m_editMainControlSyncStart, "main_control_sync_start", 0, 65535, 125) && ok;
    ok = saveBounded(m_editMainControlSyncCount, "main_control_sync_count", 1, 125, 6) && ok;
    ok = saveBounded(m_editMainReconnect, "main_reconnect_ms", 500, 120000, 5000) && ok;
    ok = saveBounded(m_editAgvPoll, "agv_poll_ms", 50, 60000, 200) && ok;
    ok = saveBounded(m_editAgvReconnect, "agv_reconnect_ms", 500, 120000, 5000) && ok;
    settings.endGroup();
    if (!ok) {
        return false;
    }
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadSliderLimitState()
{
    const auto defaultRanges = builtinArcGaugeRanges();
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SliderLabelLimits"));
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        const auto range = defaultRanges.value(it.key(), qMakePair(0.0, 100.0));
        const double minVal = settings.contains(it.key() + QStringLiteral("_min"))
                                  ? settings.value(it.key() + QStringLiteral("_min")).toDouble()
                                  : range.first;
        const double maxVal = settings.contains(it.key() + QStringLiteral("_max"))
                                  ? settings.value(it.key() + QStringLiteral("_max")).toDouble()
                                  : range.second;
        if (it->minEdit) {
            it->minEdit->setText(QString::number(minVal));
            rememberValidEditText(it->minEdit);
        }
        if (it->maxEdit) {
            it->maxEdit->setText(QString::number(maxVal));
            rememberValidEditText(it->maxEdit);
        }
    }
    settings.endGroup();
    return true;
}

bool FeatureSwitchWidget::saveSliderLimitState(QString *error)
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SliderLabelLimits"));
    const auto defaultRanges = builtinArcGaugeRanges();
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        const auto range = defaultRanges.value(it.key(), qMakePair(0.0, 100.0));
        if (!validateRange(it->minEdit, it->maxEdit, range.first, range.second, error)) {
            settings.endGroup();
            return false;
        }
        settings.setValue(it.key() + QStringLiteral("_min"), it->minEdit->text().toDouble());
        settings.setValue(it.key() + QStringLiteral("_max"), it->maxEdit->text().toDouble());
    }
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

FeatureSwitchWidget::DisplayRegisterEdits FeatureSwitchWidget::makeDisplayRegisterEdits(
    QWidget *parent, const QString &objectName)
{
    const DisplayRegisterMapping::DisplaySpec fallback =
        DisplayRegisterMapping::defaultSpec(objectName);
    DisplayRegisterEdits edits;
    edits.device = new QComboBox(parent);
    edits.device->addItems({QStringLiteral("主控"), QStringLiteral("AGV")});
    edits.device->setFixedWidth(58);

    auto makeAddrEdit = [this, parent](const QString &placeholder) {
        auto *edit = new QLineEdit(parent);
        edit->setPlaceholderText(placeholder);
        edit->setFixedWidth(72);
        edit->setValidator(new QIntValidator(0, 65535, edit));
        edit->installEventFilter(this);
        return edit;
    };
    edits.startEdit = makeAddrEdit(QStringLiteral("起始"));
    edits.wordCount = new QComboBox(parent);
    edits.wordCount->addItem(QStringLiteral("1"), 1);
    edits.wordCount->addItem(QStringLiteral("2"), 2);
    edits.wordCount->addItem(QStringLiteral("4"), 4);
    edits.wordCount->setFixedWidth(52);
    edits.encoding = new QComboBox(parent);
    using DisplayRegisterMapping::Encoding;
    edits.encoding->addItem(DisplayRegisterMapping::encodingLabel(Encoding::UInt16),
                            DisplayRegisterMapping::encodingKey(Encoding::UInt16));
    edits.encoding->addItem(DisplayRegisterMapping::encodingLabel(Encoding::Int16Div100),
                            DisplayRegisterMapping::encodingKey(Encoding::Int16Div100));
    edits.encoding->addItem(DisplayRegisterMapping::encodingLabel(Encoding::CdabFloat),
                            DisplayRegisterMapping::encodingKey(Encoding::CdabFloat));
    edits.encoding->addItem(DisplayRegisterMapping::encodingLabel(Encoding::DcbaFehgDouble),
                            DisplayRegisterMapping::encodingKey(Encoding::DcbaFehgDouble));
    edits.encoding->setMinimumWidth(150);

    if (DisplayRegisterMapping::hasSumStart(fallback)) {
        edits.sumStartEdit = makeAddrEdit(QStringLiteral("求和"));
    }
    if (DisplayRegisterMapping::hasSecondRing(fallback)) {
        edits.secondStartEdit = makeAddrEdit(QStringLiteral("副值"));
        edits.secondCountEdit = new QLineEdit(parent);
        edits.secondCountEdit->setPlaceholderText(QStringLiteral("副字数"));
        edits.secondCountEdit->setFixedWidth(56);
        edits.secondCountEdit->setValidator(new QIntValidator(1, 8, edits.secondCountEdit));
        edits.secondCountEdit->installEventFilter(this);
    }
    wireIntEdit(edits.startEdit, 0, 65535, fallback.start);
    if (edits.sumStartEdit) {
        wireIntEdit(edits.sumStartEdit, 0, 65535, qMax(0, fallback.sumStart));
    }
    if (edits.secondStartEdit) {
        wireIntEdit(edits.secondStartEdit, 0, 65535, qMax(0, fallback.secondStart));
    }
    if (edits.secondCountEdit) {
        wireIntEdit(edits.secondCountEdit, 1, 8, qMax(1, fallback.secondCount));
    }
    return edits;
}

void FeatureSwitchWidget::addDisplayRegisterRow(QWidget *parent,
                                                QVBoxLayout *hostLayout,
                                                const QString &objectName,
                                                DisplayRegisterEdits &edits)
{
    Q_UNUSED(objectName);
    auto *row = new QHBoxLayout();
    row->setSpacing(6);
    row->addWidget(new QLabel(QStringLiteral("读"), parent));
    row->addWidget(edits.device);
    row->addWidget(new QLabel(QStringLiteral("起始"), parent));
    row->addWidget(edits.startEdit);
    row->addWidget(new QLabel(QStringLiteral("字数"), parent));
    row->addWidget(edits.wordCount);
    row->addWidget(new QLabel(QStringLiteral("编码"), parent));
    row->addWidget(edits.encoding);
    if (edits.sumStartEdit) {
        row->addWidget(new QLabel(QStringLiteral("求和"), parent));
        row->addWidget(edits.sumStartEdit);
    }
    if (edits.secondStartEdit) {
        row->addWidget(new QLabel(QStringLiteral("副值"), parent));
        row->addWidget(edits.secondStartEdit);
        if (edits.secondCountEdit) {
            row->addWidget(edits.secondCountEdit);
        }
    }
    row->addStretch();
    hostLayout->addLayout(row);
}

void FeatureSwitchWidget::applyDisplaySpecToEdits(const DisplayRegisterMapping::DisplaySpec &spec,
                                                 DisplayRegisterEdits &edits)
{
    if (edits.device) {
        const int idx = edits.device->findText(spec.device);
        edits.device->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (edits.startEdit) {
        edits.startEdit->setText(QString::number(spec.start));
        rememberValidEditText(edits.startEdit);
    }
    if (edits.wordCount) {
        const int idx = edits.wordCount->findData(spec.wordCount);
        edits.wordCount->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (edits.encoding) {
        const int idx = edits.encoding->findData(DisplayRegisterMapping::encodingKey(spec.encoding));
        edits.encoding->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (edits.sumStartEdit) {
        edits.sumStartEdit->setText(QString::number(qMax(0, spec.sumStart)));
        rememberValidEditText(edits.sumStartEdit);
    }
    if (edits.secondStartEdit) {
        edits.secondStartEdit->setText(QString::number(qMax(0, spec.secondStart)));
        rememberValidEditText(edits.secondStartEdit);
    }
    if (edits.secondCountEdit) {
        edits.secondCountEdit->setText(QString::number(qMax(1, spec.secondCount)));
        rememberValidEditText(edits.secondCountEdit);
    }
}

DisplayRegisterMapping::DisplaySpec FeatureSwitchWidget::readDisplaySpecFromEdits(
    const QString &objectName, const DisplayRegisterEdits &edits) const
{
    DisplayRegisterMapping::DisplaySpec spec = DisplayRegisterMapping::defaultSpec(objectName);
    if (edits.device) {
        spec.device = edits.device->currentText();
    }
    if (edits.startEdit) {
        spec.start = edits.startEdit->text().trimmed().toInt();
    }
    if (edits.wordCount) {
        spec.wordCount = edits.wordCount->currentData().toInt();
    }
    if (edits.encoding) {
        spec.encoding = DisplayRegisterMapping::encodingFromKey(
            edits.encoding->currentData().toString(), spec.encoding);
    }
    if (edits.sumStartEdit) {
        spec.sumStart = edits.sumStartEdit->text().trimmed().toInt();
    }
    if (edits.secondStartEdit) {
        spec.secondStart = edits.secondStartEdit->text().trimmed().toInt();
    }
    if (edits.secondCountEdit) {
        spec.secondCount = edits.secondCountEdit->text().trimmed().toInt();
    }
    return spec;
}

bool FeatureSwitchWidget::validateDisplayRegisterEdits(const QString &objectName,
                                                       const DisplayRegisterEdits &edits,
                                                       QString *error)
{
    const DisplayRegisterMapping::DisplaySpec fallback =
        DisplayRegisterMapping::defaultSpec(objectName);
    const QString title = DisplayRegisterMapping::displayTitle(objectName);
    if (edits.startEdit
        && !validateIntEdit(edits.startEdit, 0, 65535, fallback.start, error)) {
        if (error && !error->isEmpty()) {
            *error = QStringLiteral("%1 起始地址：%2").arg(title, *error);
        }
        return false;
    }
    if (edits.sumStartEdit
        && !validateIntEdit(edits.sumStartEdit, 0, 65535, qMax(0, fallback.sumStart), error)) {
        if (error && !error->isEmpty()) {
            *error = QStringLiteral("%1 求和起始：%2").arg(title, *error);
        }
        return false;
    }
    if (edits.secondStartEdit
        && !validateIntEdit(edits.secondStartEdit, 0, 65535, qMax(0, fallback.secondStart), error)) {
        if (error && !error->isEmpty()) {
            *error = QStringLiteral("%1 副值起始：%2").arg(title, *error);
        }
        return false;
    }
    if (edits.secondCountEdit
        && !validateIntEdit(edits.secondCountEdit, 1, 8, qMax(1, fallback.secondCount), error)) {
        if (error && !error->isEmpty()) {
            *error = QStringLiteral("%1 副值字数：%2").arg(title, *error);
        }
        return false;
    }
    return true;
}

bool FeatureSwitchWidget::loadDisplayRegisterState()
{
    auto applyOne = [this](const QString &name, DisplayRegisterEdits &edits) {
        applyDisplaySpecToEdits(DisplayRegisterMapping::resolvedSpec(name), edits);
    };
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        applyOne(it.key(), it->registers);
    }
    for (auto it = m_statusRegisterEdits.begin(); it != m_statusRegisterEdits.end(); ++it) {
        applyOne(it.key(), it.value());
    }
    return true;
}

bool FeatureSwitchWidget::saveDisplayRegisterState(QString *error)
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("DisplayRegisterMapping"));
    auto saveOne = [&](const QString &name, const DisplayRegisterEdits &edits) -> bool {
        if (!validateDisplayRegisterEdits(name, edits, error)) {
            return false;
        }
        DisplayRegisterMapping::writeSpec(settings, name, readDisplaySpecFromEdits(name, edits));
        return true;
    };
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (!saveOne(it.key(), it->registers)) {
            settings.endGroup();
            return false;
        }
    }
    for (auto it = m_statusRegisterEdits.begin(); it != m_statusRegisterEdits.end(); ++it) {
        if (!saveOne(it.key(), it.value())) {
            settings.endGroup();
            return false;
        }
    }
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadTechSliderEditState()
{
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("TechSliderEditLimits"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        double fallbackMin = 0.0;
        double fallbackMax = 100.0;
        if (TechSliderEdit *slider = mainWindow ? mainWindow->findChild<TechSliderEdit*>(it.key()) : nullptr) {
            fallbackMin = slider->minimum();
            fallbackMax = slider->maximum();
        }
        const auto read = [&](const QString &suffix, double defaultVal) -> double {
            const QString key = it.key() + suffix;
            return settings.contains(key) ? settings.value(key).toDouble() : defaultVal;
        };
        if (it->displayMinEdit) {
            it->displayMinEdit->setText(QString::number(
                read(QStringLiteral("_display_min"), read(QStringLiteral("_value_min"), fallbackMin))));
            rememberValidEditText(it->displayMinEdit);
        }
        if (it->displayMaxEdit) {
            it->displayMaxEdit->setText(QString::number(
                read(QStringLiteral("_display_max"), read(QStringLiteral("_value_max"), fallbackMax))));
            rememberValidEditText(it->displayMaxEdit);
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    settings.endGroup();
    return true;
}

bool FeatureSwitchWidget::saveTechSliderEditState(QString *error)
{
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("TechSliderEditLimits"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        double fallbackMin = 0.0;
        double fallbackMax = 100.0;
        if (TechSliderEdit *slider = mainWindow ? mainWindow->findChild<TechSliderEdit*>(it.key()) : nullptr) {
            fallbackMin = slider->minimum();
            fallbackMax = slider->maximum();
        }
        if (!validateRange(it->displayMinEdit, it->displayMaxEdit, fallbackMin, fallbackMax, error)) {
            settings.endGroup();
            return false;
        }
        settings.setValue(it.key() + QStringLiteral("_display_min"), it->displayMinEdit->text().toDouble());
        settings.setValue(it.key() + QStringLiteral("_display_max"), it->displayMaxEdit->text().toDouble());
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadPlaneHeightOffsetState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("PlaneHeight"));
    const double offset = settings.value(QStringLiteral("offset_mm"), 1900.0).toDouble();
    settings.endGroup();
    if (m_editPlaneHeightOffset) {
        m_editPlaneHeightOffset->setText(QString::number(offset, 'f', 0));
        rememberValidEditText(m_editPlaneHeightOffset);
    }
    return true;
}

bool FeatureSwitchWidget::savePlaneHeightOffsetState(QString *error)
{
    if (!validateDoubleEdit(m_editPlaneHeightOffset, -100000.0, 100000.0, 1900.0, error)) {
        return false;
    }
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("PlaneHeight"));
    settings.setValue(QStringLiteral("offset_mm"), m_editPlaneHeightOffset->text().trimmed().toDouble());
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadRobotTorqueState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("RobotTorque"));
    const double danger = settings.value(QStringLiteral("danger_percent"), 140.0).toDouble();
    settings.endGroup();
    if (m_editRobotTorqueDanger) {
        m_editRobotTorqueDanger->setText(QString::number(danger, 'f', 0));
        rememberValidEditText(m_editRobotTorqueDanger);
    }
    return true;
}

bool FeatureSwitchWidget::saveRobotTorqueState(QString *error)
{
    if (!validateDoubleEdit(m_editRobotTorqueDanger, 1.0, 999.0, 140.0, error)) {
        return false;
    }
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("RobotTorque"));
    settings.setValue(QStringLiteral("danger_percent"),
                      m_editRobotTorqueDanger->text().trimmed().toDouble());
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadPermissionPasswordState()
{
    if (m_editEngineerPassword) {
        m_editEngineerPassword->setText(MainWindow::rolePassword(MainWindow::UserRole::Engineer));
    }
    if (m_editAdminPassword) {
        m_editAdminPassword->setText(MainWindow::rolePassword(MainWindow::UserRole::Admin));
    }
    return true;
}

bool FeatureSwitchWidget::savePermissionPasswordState(QString *error)
{
    const QString engineer = m_editEngineerPassword
                                 ? m_editEngineerPassword->text().trimmed()
                                 : QString();
    const QString admin = m_editAdminPassword
                              ? m_editAdminPassword->text().trimmed()
                              : QString();
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Permission"));
    settings.setValue(QStringLiteral("engineer_password"), engineer);
    settings.setValue(QStringLiteral("admin_password"), admin);
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::loadButtonVisibilityState()
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    for (auto it = m_deviceControlChildCheckboxes.begin(); it != m_deviceControlChildCheckboxes.end(); ++it) {
        it.value()->setChecked(settings.value(it.key(), true).toBool());
    }
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->visible) {
            it->visible->setChecked(settings.value(it.key(), true).toBool());
        }
    }
    for (auto it = m_otherVisibilityCheckboxes.begin(); it != m_otherVisibilityCheckboxes.end(); ++it) {
        it.value()->setChecked(settings.value(it.key(), true).toBool());
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        const ButtonModbusMapping::Binding binding = resolvedBinding(it.key());
        for (int idx = 0; idx < it->reads.size(); ++idx) {
            if (idx < binding.reads.size()) {
                applyRegisterSpecToEdits(binding.reads.at(idx), it->reads[idx]);
            } else {
                applyRegisterSpecToEdits(MainWindow::ModbusRegisterSpec{}, it->reads[idx]);
            }
        }
        for (int idx = 0; idx < it->writes.size(); ++idx) {
            if (idx < binding.writes.size()) {
                applyRegisterSpecToEdits(binding.writes.at(idx), it->writes[idx]);
            } else {
                applyRegisterSpecToEdits(MainWindow::ModbusRegisterSpec{}, it->writes[idx]);
            }
        }
    }
    settings.endGroup();
    return true;
}

bool FeatureSwitchWidget::saveButtonVisibilityState(QString *error)
{
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonVisibility"));
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    for (auto it = m_deviceControlChildCheckboxes.begin(); it != m_deviceControlChildCheckboxes.end(); ++it) {
        settings.setValue(it.key(), it.value()->isChecked());
    }
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        if (it->visible) {
            settings.setValue(it.key(), it->visible->isChecked());
        }
    }
    for (auto it = m_otherVisibilityCheckboxes.begin(); it != m_otherVisibilityCheckboxes.end(); ++it) {
        settings.setValue(it.key(), it.value()->isChecked());
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));
    for (auto it = m_modbusButtonEdits.begin(); it != m_modbusButtonEdits.end(); ++it) {
        const QString &name = it.key();
        const QString readPrefixBase = name + QStringLiteral("_read");
        for (int idx = 0; idx < it->reads.size(); ++idx) {
            const MainWindow::ModbusRegisterSpec readSpec = readRegisterSpecFromEdits(it->reads.at(idx));
            const QString readPrefix = QStringLiteral("%1_%2").arg(readPrefixBase).arg(idx + 1);
            settings.setValue(readPrefix + QStringLiteral("_device"), readSpec.device);
            settings.setValue(readPrefix + QStringLiteral("_addr"), readSpec.address);
            settings.setValue(readPrefix + QStringLiteral("_bit"), readSpec.bit);
            settings.setValue(readPrefix + QStringLiteral("_value1"), readSpec.value1);
            settings.setValue(readPrefix + QStringLiteral("_value2"), readSpec.value2);
            settings.setValue(readPrefix + QStringLiteral("_value3"), readSpec.value3);
            settings.setValue(readPrefix + QStringLiteral("_value"), readSpec.value1);
            settings.setValue(readPrefix, composeLegacyRegisterString(readSpec));
            if (idx == 0) {
                settings.setValue(readPrefixBase + QStringLiteral("_device"), readSpec.device);
                settings.setValue(readPrefixBase + QStringLiteral("_addr"), readSpec.address);
                settings.setValue(readPrefixBase + QStringLiteral("_bit"), readSpec.bit);
                settings.setValue(readPrefixBase + QStringLiteral("_value1"), readSpec.value1);
                settings.setValue(readPrefixBase + QStringLiteral("_value2"), readSpec.value2);
                settings.setValue(readPrefixBase + QStringLiteral("_value3"), readSpec.value3);
                settings.setValue(readPrefixBase + QStringLiteral("_value"), readSpec.value1);
                settings.setValue(readPrefixBase, composeLegacyRegisterString(readSpec));
            }
        }

        const QString writePrefixBase = name + QStringLiteral("_write");
        for (int idx = 0; idx < it->writes.size(); ++idx) {
            const MainWindow::ModbusRegisterSpec writeSpec = readRegisterSpecFromEdits(it->writes.at(idx));
            const QString writePrefix = QStringLiteral("%1_%2").arg(writePrefixBase).arg(idx + 1);
            settings.setValue(writePrefix + QStringLiteral("_device"), writeSpec.device);
            settings.setValue(writePrefix + QStringLiteral("_addr"), writeSpec.address);
            settings.setValue(writePrefix + QStringLiteral("_bit"), writeSpec.bit);
            settings.setValue(writePrefix + QStringLiteral("_value1"), writeSpec.value1);
            settings.setValue(writePrefix + QStringLiteral("_value2"), writeSpec.value2);
            settings.setValue(writePrefix + QStringLiteral("_value3"), writeSpec.value3);
            settings.setValue(writePrefix + QStringLiteral("_value"), writeSpec.value1);
            settings.setValue(writePrefix, composeLegacyRegisterString(writeSpec));
            if (idx == 0) {
                settings.setValue(writePrefixBase + QStringLiteral("_device"), writeSpec.device);
                settings.setValue(writePrefixBase + QStringLiteral("_addr"), writeSpec.address);
                settings.setValue(writePrefixBase + QStringLiteral("_bit"), writeSpec.bit);
                settings.setValue(writePrefixBase + QStringLiteral("_value1"), writeSpec.value1);
                settings.setValue(writePrefixBase + QStringLiteral("_value2"), writeSpec.value2);
                settings.setValue(writePrefixBase + QStringLiteral("_value3"), writeSpec.value3);
                settings.setValue(writePrefixBase + QStringLiteral("_value"), writeSpec.value1);
                settings.setValue(writePrefixBase, composeLegacyRegisterString(writeSpec));
            }
        }
    }
    settings.endGroup();
    return settingsWriteOk(settings, QStringLiteral("config.ini"), error);
}

bool FeatureSwitchWidget::validateIntEdit(QLineEdit *edit, int min, int max, int fallback, QString *error)
{
    if (!edit) {
        return true;
    }
    bool ok = false;
    const int v = edit->text().trimmed().toInt(&ok);
    if (!ok) {
        restoreLineEditText(edit, lastValidEditText(edit, QString::number(fallback)));
        if (error) {
            *error = QStringLiteral("请输入整数");
        }
        return false;
    }
    if (v < min || v > max) {
        restoreLineEditText(edit, lastValidEditText(edit, QString::number(fallback)));
        if (error) {
            *error = QStringLiteral("输入超出范围（%1 ~ %2）").arg(min).arg(max);
        }
        return false;
    }
    rememberValidEditText(edit, QString::number(v));
    return true;
}

bool FeatureSwitchWidget::validateDoubleEdit(QLineEdit *edit, double min, double max,
                                             double fallback, QString *error)
{
    if (!edit) {
        return true;
    }
    bool ok = false;
    const double v = edit->text().trimmed().toDouble(&ok);
    if (!ok) {
        restoreLineEditText(edit, lastValidEditText(edit, QString::number(fallback)));
        if (error) {
            *error = QStringLiteral("请输入有效数字");
        }
        return false;
    }
    if (v < min || v > max) {
        restoreLineEditText(edit, lastValidEditText(edit, QString::number(fallback)));
        if (error) {
            *error = QStringLiteral("输入超出范围（%1 ~ %2）").arg(min).arg(max);
        }
        return false;
    }
    rememberValidEditText(edit, QString::number(v));
    return true;
}

bool FeatureSwitchWidget::validateRange(QLineEdit *minEdit, QLineEdit *maxEdit,
                                        double fallbackMin, double fallbackMax, QString *error)
{
    if (!minEdit || !maxEdit) {
        return true;
    }
    if (minEdit->text().trimmed().isEmpty() || maxEdit->text().trimmed().isEmpty()) {
        restoreLineEditText(minEdit, lastValidEditText(minEdit, QString::number(fallbackMin)));
        restoreLineEditText(maxEdit, lastValidEditText(maxEdit, QString::number(fallbackMax)));
        if (error) {
            *error = QStringLiteral("范围不能为空");
        }
        return false;
    }
    bool minOk = false;
    bool maxOk = false;
    const double minVal = minEdit->text().trimmed().toDouble(&minOk);
    const double maxVal = maxEdit->text().trimmed().toDouble(&maxOk);
    if (!minOk || !maxOk) {
        restoreLineEditText(minEdit, lastValidEditText(minEdit, QString::number(fallbackMin)));
        restoreLineEditText(maxEdit, lastValidEditText(maxEdit, QString::number(fallbackMax)));
        if (error) {
            *error = QStringLiteral("请输入有效数字");
        }
        return false;
    }
    if (minVal >= maxVal) {
        restoreLineEditText(minEdit, lastValidEditText(minEdit, QString::number(fallbackMin)));
        restoreLineEditText(maxEdit, lastValidEditText(maxEdit, QString::number(fallbackMax)));
        if (error) {
            *error = QStringLiteral("最小值必须小于最大值");
        }
        return false;
    }
    rememberValidEditText(minEdit, QString::number(minVal));
    rememberValidEditText(maxEdit, QString::number(maxVal));
    return true;
}

bool FeatureSwitchWidget::validateAllInputs(QString *error)
{
    if (!validateIntEdit(m_editMainModbusPoll, 50, 60000, 500, error)
        || !validateIntEdit(m_editMainUiPoll, 50, 60000, 200, error)
        || !validateIntEdit(m_editMainDeviceStatusPoll, 50, 60000, 2000, error)
        || !validateIntEdit(m_editMainDeviceStatusStart, 0, 65535, 0, error)
        || !validateIntEdit(m_editMainDeviceStatusCount, 1, 125, 85, error)
        || !validateIntEdit(m_editMainControlSyncStart, 0, 65535, 125, error)
        || !validateIntEdit(m_editMainControlSyncCount, 1, 125, 6, error)
        || !validateIntEdit(m_editMainReconnect, 500, 120000, 5000, error)
        || !validateIntEdit(m_editAgvPoll, 50, 60000, 200, error)
        || !validateIntEdit(m_editAgvReconnect, 500, 120000, 5000, error)
        || !validateDoubleEdit(m_editPlaneHeightOffset, -100000.0, 100000.0, 1900.0, error)
        || !validateDoubleEdit(m_editRobotTorqueDanger, 1.0, 999.0, 140.0, error)) {
        return false;
    }

    const auto checkPassword = [&](QLineEdit *edit, const QString &field) -> bool {
        if (!edit) {
            if (error) {
                *error = QStringLiteral("%1 输入框未初始化").arg(field);
            }
            return false;
        }
        const QString text = edit->text().trimmed();
        if (text.isEmpty()) {
            if (error) {
                *error = QStringLiteral("%1 不能为空").arg(field);
            }
            return false;
        }
        if (text.size() > 32) {
            if (error) {
                *error = QStringLiteral("%1 最长 32 个字符").arg(field);
            }
            return false;
        }
        return true;
    };
    if (!checkPassword(m_editEngineerPassword, QStringLiteral("工程师密码"))
        || !checkPassword(m_editAdminPassword, QStringLiteral("管理员密码"))) {
        return false;
    }

    const auto defaultRanges = builtinArcGaugeRanges();
    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        const auto range = defaultRanges.value(it.key(), qMakePair(0.0, 100.0));
        if (!validateRange(it->minEdit, it->maxEdit, range.first, range.second, error)) {
            return false;
        }
    }

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    for (auto it = m_sliderEditEdits.begin(); it != m_sliderEditEdits.end(); ++it) {
        double fallbackMin = 0.0;
        double fallbackMax = 100.0;
        if (TechSliderEdit *slider = mainWindow ? mainWindow->findChild<TechSliderEdit*>(it.key()) : nullptr) {
            fallbackMin = slider->minimum();
            fallbackMax = slider->maximum();
        }
        if (!validateRange(it->displayMinEdit, it->displayMaxEdit, fallbackMin, fallbackMax, error)) {
            return false;
        }
    }

    for (auto it = m_arcGaugeEdits.begin(); it != m_arcGaugeEdits.end(); ++it) {
        if (!validateDisplayRegisterEdits(it.key(), it->registers, error)) {
            return false;
        }
    }
    for (auto it = m_statusRegisterEdits.begin(); it != m_statusRegisterEdits.end(); ++it) {
        if (!validateDisplayRegisterEdits(it.key(), it.value(), error)) {
            return false;
        }
    }
    return true;
}

bool FeatureSwitchWidget::settingsWriteOk(QSettings &settings, const QString &fileName, QString *error) const
{
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("无法写入 %1").arg(fileName);
        }
        return false;
    }
    return true;
}

void FeatureSwitchWidget::rememberValidEditText(QLineEdit *edit, const QString &text)
{
    if (!edit) {
        return;
    }
    edit->setProperty("lastValidText", text.isEmpty() ? edit->text() : text);
}

QString FeatureSwitchWidget::lastValidEditText(const QLineEdit *edit, const QString &fallback) const
{
    return lastValidFromProperty(edit, fallback);
}

void FeatureSwitchWidget::restoreLineEditText(QLineEdit *edit, const QString &text)
{
    if (!edit) {
        return;
    }
    const QSignalBlocker blocker(edit);
    edit->setText(text);
}

void FeatureSwitchWidget::showConsoleError(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("保存失败"), message);
}

void FeatureSwitchWidget::wireIntEdit(QLineEdit *edit, int min, int max, int fallback)
{
    if (!edit) {
        return;
    }
    rememberValidEditText(edit, QString::number(fallback));
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, min, max, fallback]() {
        QString error;
        if (!validateIntEdit(edit, min, max, fallback, &error)) {
            showConsoleError(error);
        }
    });
}

void FeatureSwitchWidget::wireDoubleEdit(QLineEdit *edit, double min, double max, double fallback)
{
    if (!edit) {
        return;
    }
    rememberValidEditText(edit, QString::number(fallback));
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, min, max, fallback]() {
        QString error;
        if (!validateDoubleEdit(edit, min, max, fallback, &error)) {
            showConsoleError(error);
        }
    });
}

QLineEdit *FeatureSwitchWidget::makeLimitEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setFixedWidth(72);
    edit->installEventFilter(this);
    return edit;
}

void FeatureSwitchWidget::recordIniChanges(const QString &fileName, const QString &group,
                                           const QMap<QString, QString> &before)
{
    QSettings settings(fileName, QSettings::IniFormat);
    settings.beginGroup(group);
    QSet<QString> keys;
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        keys.insert(it.key());
    }
    const QStringList currentKeys = settings.childKeys();
    for (const QString &key : currentKeys) {
        keys.insert(key);
    }
    for (const QString &key : keys) {
        const QString rawOld = before.value(key);
        const QString rawNew = settings.value(key).toString();
        if (rawOld == rawNew) {
            continue;
        }
        const bool secret = key.contains(QLatin1String("password"));
        emitHistoryIfChanged(configFieldLabel(group, key),
                             secret ? QStringLiteral("******") : displayConfigValue(rawOld),
                             secret ? QStringLiteral("已更改") : displayConfigValue(rawNew));
    }
    settings.endGroup();
}

void FeatureSwitchWidget::emitHistoryIfChanged(const QString &controlName,
                                               const QString &oldValue,
                                               const QString &newValue)
{
    if (oldValue == newValue) {
        return;
    }
    emit operationHistoryEntry(controlName, oldValue, newValue);
}

bool FeatureSwitchWidget::onApply()
{
    if (m_virtualKeyboard) {
        m_virtualKeyboard->commitPendingInput();
    }

    QString error;
    if (!validateAllInputs(&error)) {
        showConsoleError(error);
        return false;
    }

    const QString ini = QStringLiteral("config.ini");
    const QMap<QString, QString> pollingBefore = snapshotIniGroup(ini, QStringLiteral("Polling"));
    const QMap<QString, QString> sliderBefore = snapshotIniGroup(ini, QStringLiteral("SliderLabelLimits"));
    const QMap<QString, QString> techSliderBefore = snapshotIniGroup(ini, QStringLiteral("TechSliderEditLimits"));
    const QMap<QString, QString> planeBefore = snapshotIniGroup(ini, QStringLiteral("PlaneHeight"));
    const QMap<QString, QString> torqueBefore = snapshotIniGroup(ini, QStringLiteral("RobotTorque"));
    const QMap<QString, QString> buttonBefore = snapshotIniGroup(ini, QStringLiteral("ButtonVisibility"));
    const QMap<QString, QString> mappingBefore = snapshotIniGroup(ini, QStringLiteral("ButtonModbusMapping"));
    const QMap<QString, QString> displayRegBefore = snapshotIniGroup(ini, QStringLiteral("DisplayRegisterMapping"));
    const QMap<QString, QString> permissionBefore = snapshotIniGroup(ini, QStringLiteral("Permission"));

    if (!savePollingState(&error)
        || !saveSliderLimitState(&error)
        || !saveDisplayRegisterState(&error)
        || !saveTechSliderEditState(&error)
        || !savePlaneHeightOffsetState(&error)
        || !saveRobotTorqueState(&error)
        || !savePermissionPasswordState(&error)
        || !saveButtonVisibilityState(&error)) {
        showConsoleError(error.isEmpty() ? QStringLiteral("无法写入 config.ini") : error);
        return false;
    }

    FeatureSwitchManager *mgr = FeatureSwitchManager::instance();
    for (auto it = m_bigCheckboxes.begin(); it != m_bigCheckboxes.end(); ++it) {
        const bool now = it.value()->isChecked();
        const bool was = mgr->isBigFeatureEnabled(it.key());
        emitHistoryIfChanged(it.value()->text(),
                             was ? QStringLiteral("开启") : QStringLiteral("关闭"),
                             now ? QStringLiteral("开启") : QStringLiteral("关闭"));
        mgr->setBigFeatureEnabled(it.key(), now);
    }
    for (auto it = m_smallCheckboxes.begin(); it != m_smallCheckboxes.end(); ++it) {
        const bool now = it.value()->isChecked();
        const bool was = mgr->isSmallFeatureEnabled(it.key());
        emitHistoryIfChanged(it.value()->text(),
                             was ? QStringLiteral("开启") : QStringLiteral("关闭"),
                             now ? QStringLiteral("开启") : QStringLiteral("关闭"));
        mgr->setSmallFeatureEnabled(it.key(), now);
    }

    recordIniChanges(ini, QStringLiteral("Polling"), pollingBefore);
    recordIniChanges(ini, QStringLiteral("SliderLabelLimits"), sliderBefore);
    recordIniChanges(ini, QStringLiteral("TechSliderEditLimits"), techSliderBefore);
    recordIniChanges(ini, QStringLiteral("PlaneHeight"), planeBefore);
    recordIniChanges(ini, QStringLiteral("RobotTorque"), torqueBefore);
    recordIniChanges(ini, QStringLiteral("ButtonVisibility"), buttonBefore);
    recordIniChanges(ini, QStringLiteral("ButtonModbusMapping"), mappingBefore);
    recordIniChanges(ini, QStringLiteral("DisplayRegisterMapping"), displayRegBefore);
    recordIniChanges(ini, QStringLiteral("Permission"), permissionBefore);

    emit runtimeSettingsChanged();
    hide();
    return true;
}

void FeatureSwitchWidget::onSave()
{
    if (!onApply()) {
        return;
    }
    FeatureSwitchManager::instance()->save();
    QMessageBox::information(this, QStringLiteral("结果"),
                             QStringLiteral("配置已成功保存到 feature_switches.ini。"));
}

void FeatureSwitchWidget::onReload()
{
    FeatureSwitchManager::instance()->reload();
    refreshTechSliderEditList();
    refreshButtonVisibilityList();
    loadCurrentState();
}

void FeatureSwitchWidget::onToggleAll(bool checked)
{
    for (auto cb : m_bigCheckboxes) {
        cb->setChecked(checked);
    }
    for (auto cb : m_smallCheckboxes) {
        cb->setChecked(checked);
    }
}
