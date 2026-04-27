#include "matrixkeymonitor.h"
#include <QDebug>
#include <QCoreApplication>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

MatrixKeyMonitor::MatrixKeyMonitor(const QString &device, QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_notifier(nullptr)
    , m_devicePath(device)
    , m_isRunning(false)
{
    qDebug() << "MatrixKeyMonitor 创建，设备:" << device;
}

MatrixKeyMonitor::MatrixKeyMonitor(QObject *parent)
    : MatrixKeyMonitor("/dev/input/event0", parent)
{
}

MatrixKeyMonitor::~MatrixKeyMonitor()
{
    stopMonitoring();
    qDebug() << "MatrixKeyMonitor 销毁";
}

bool MatrixKeyMonitor::startMonitoring()
{
    if (m_isRunning) {
        qWarning() << "监控器已在运行";
        return true;
    }

#ifndef Q_OS_LINUX
    const QString error = QStringLiteral("当前平台不支持 Linux input 设备: %1").arg(m_devicePath);
    qWarning() << error;
    emit errorOccurred(error);
    return false;
#else
    m_fd = open(m_devicePath.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) {
        QString error = QString("无法打开设备 %1: %2").arg(m_devicePath).arg(strerror(errno));
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &MatrixKeyMonitor::onSocketActivated);

    m_isRunning = true;
    m_lastKeyState.clear();
    qDebug() << "键盘监控已启动，设备:" << m_devicePath;
    emit statusChanged(true);

    return true;
#endif
}

void MatrixKeyMonitor::stopMonitoring()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

#ifdef Q_OS_LINUX
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
#else
    m_fd = -1;
#endif

    m_lastKeyState.clear();

    qDebug() << "键盘监控已停止";
    emit statusChanged(false);
}

void MatrixKeyMonitor::onSocketActivated(int socket)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(socket);
#else
    struct input_event ev;

    while (true) {
        ssize_t bytesRead = read(socket, &ev, sizeof(ev));

        if (bytesRead != sizeof(ev)) {
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                qWarning() << "读取键盘事件失败:" << strerror(errno);
                break;
            }
            break;
        }

        if (ev.type == EV_KEY) {
            if (ev.value != 0 && ev.value != 1) {
                continue;
            }

            int keyNumber = mapKeyCodeToButtonNumber(ev.code);
            bool pressed = (ev.value == 1);

            if (keyNumber >= 0) {
                if (m_lastKeyState.contains(keyNumber) && m_lastKeyState.value(keyNumber) == pressed) {
                    continue;
                }
                m_lastKeyState[keyNumber] = pressed;

                qDebug() << "矩阵按键事件 - 键码:" << ev.code
                         << "-> 按钮" << keyNumber
                         << "状态:" << (pressed ? "按下" : "释放");

                emit keyPressed(keyNumber, pressed);
                emit rawKeyEvent(ev.code, ev.value);
            }
        }
    }
#endif
}

int MatrixKeyMonitor::mapKeyCodeToButtonNumber(int keyCode)
{
    switch (keyCode) {
    case 62: return 1;
    case 47: return 2;
    case 5:  return 3;
    case 60: return 4;
    case 4:  return 5;
    case 3:  return 6;
    case 18: return 7;
    case 17: return 8;
    case 32: return 9;
    case 31: return 10;
    case 46: return 11;
    case 45: return 12;
    case 30: return 13;
    case 16: return 14;
    default:
        qDebug() << "未映射的键码:" << keyCode;
        return -1;
    }
}

void MatrixKeyMonitor::runEventLoop()
{
    QEventLoop loop;
    connect(this, &MatrixKeyMonitor::statusChanged, &loop, &QEventLoop::quit);
    loop.exec();
}
