// 离屏渲染：验证功能控制台 QComboBox 及其弹层的可读性
// 参数 before：还原上一版（只在父控件样式表里写 QComboBox QAbstractItemView）的做法
// 用法：QT_QPA_PLATFORM=offscreen ./combo_popup_preview [before]
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidget>

// 控制台样式表（弹层相关规则已移除，弹层由 styleComboPopup 直接上色）
static const char *kConsoleQss =
    "QWidget { background-color: #071321; color: #d8f6ff; font-family: 'Microsoft YaHei UI'; }"
    "QGroupBox { background: rgba(5, 20, 38, 0.86); border: 1px solid rgba(74, 190, 238, 0.42); border-radius: 12px; margin-top: 16px; font-weight: bold; padding: 12px; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 2px 8px; color: #a8eaff; background: #071321; border-radius: 5px; }"
    "QLabel { color: #d8f6ff; }"
    "QLineEdit { background: rgba(3, 15, 28, 0.90); color: #f0fbff; border: 1px solid rgba(74, 190, 238, 0.52); border-radius: 6px; padding: 4px 6px; }"
    "QComboBox { background: rgba(3, 15, 28, 0.92); color: #eaf9ff; border: 1px solid rgba(74, 190, 238, 0.55); border-radius: 6px; padding: 2px 2px 2px 6px; min-height: 20px; }"
    "QComboBox:hover { border: 1px solid #6fe7ff; }"
    "QComboBox:focus, QComboBox:on { border: 1px solid #9ff4ff; }"
    "QComboBox::drop-down { border: none; width: 18px; }";

// 上一版的写法：只靠父控件样式表里的后代选择器
static const char *kOldPopupQss =
    "QComboBox QAbstractItemView { background-color: #0e2c44; color: #eaf9ff; border: 1px solid #6fe7ff; border-radius: 4px; padding: 2px; outline: none; selection-background-color: #007eba; selection-color: #ffffff; }"
    "QComboBox QAbstractItemView::item { min-height: 26px; padding: 2px 8px; }"
    "QComboBox QAbstractItemView::item:hover { background-color: #12557a; color: #ffffff; }";

// 与 featureswitchwidget.cpp 中的 styleComboPopup() 保持一致
static void styleComboPopup(QComboBox *combo)
{
    if (!combo) {
        return;
    }
    QAbstractItemView *view = combo->view();
    if (!view) {
        return;
    }

    const QColor panelColor(0x0e, 0x2c, 0x44);
    const QColor textColor(0xea, 0xf9, 0xff);

    QPalette pal = view->palette();
    pal.setColor(QPalette::Base, panelColor);
    pal.setColor(QPalette::Window, panelColor);
    pal.setColor(QPalette::Text, textColor);
    pal.setColor(QPalette::WindowText, textColor);
    pal.setColor(QPalette::Highlight, QColor(0x00, 0x7e, 0xba));
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));

    view->setPalette(pal);
    view->setStyleSheet(QStringLiteral(
        "QAbstractItemView { background-color: #0e2c44; color: #eaf9ff; border: 1px solid #6fe7ff;"
        " outline: none; selection-background-color: #007eba;"
        " selection-color: #ffffff; }"
        "QAbstractItemView::item { min-height: 26px; padding: 2px 8px; }"
        "QAbstractItemView::item:hover { background-color: #12557a; color: #ffffff; }"));

    if (QWidget *viewport = view->viewport()) {
        viewport->setAutoFillBackground(true);
        viewport->setPalette(pal);
    }

    QWidget *container = view->parentWidget();
    if (container && container != combo) {
        container->setPalette(pal);
        container->setAttribute(Qt::WA_StyledBackground, true);
        container->setStyleSheet(QStringLiteral(
            "QFrame { background-color: #0e2c44; border: 1px solid #6fe7ff; }"));
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const bool oldWay = app.arguments().contains(QStringLiteral("before"));

    QWidget root;
    root.setStyleSheet(QString::fromUtf8(kConsoleQss)
                       + (oldWay ? QString::fromUtf8(kOldPopupQss) : QString()));
    root.resize(560, 320);
    auto *outer = new QVBoxLayout(&root);

    auto *group = new QGroupBox(QStringLiteral("环形仪表显示与范围"), &root);
    auto *layout = new QVBoxLayout(group);

    auto *row = new QWidget(group);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->addWidget(new QLabel(QStringLiteral("读"), row));

    auto *device = new QComboBox(row);
    device->addItems({QStringLiteral("主控"), QStringLiteral("AGV")});
    device->setFixedWidth(58);
    rowLayout->addWidget(device);
    rowLayout->addWidget(new QLabel(QStringLiteral("起始"), row));
    auto *startEdit = new QLineEdit(QStringLiteral("0"), row);
    startEdit->setFixedWidth(72);
    rowLayout->addWidget(startEdit);
    rowLayout->addWidget(new QLabel(QStringLiteral("字数"), row));
    auto *wordCount = new QComboBox(row);
    wordCount->addItems({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("4")});
    wordCount->setFixedWidth(52);
    rowLayout->addWidget(wordCount);
    rowLayout->addWidget(new QLabel(QStringLiteral("编码"), row));
    auto *encoding = new QComboBox(row);
    encoding->addItems({QStringLiteral("uint16"), QStringLiteral("有符号/100"),
                        QStringLiteral("CDAB float"), QStringLiteral("DCBA FEHG double")});
    encoding->setMinimumWidth(150);
    rowLayout->addWidget(encoding);
    rowLayout->addStretch();

    if (!oldWay) {
        styleComboPopup(device);
        styleComboPopup(wordCount);
        styleComboPopup(encoding);
    }

    layout->addWidget(row);
    outer->addWidget(group);
    outer->addStretch();

    root.show();
    app.processEvents();

    QPixmap canvas(root.size());
    canvas.fill(QColor("#071321"));
    root.render(&canvas);

    encoding->showPopup();
    app.processEvents();
    QAbstractItemView *view = encoding->view();
    QWidget *popup = view ? view->window() : nullptr;
    if (popup) {
        const QPoint offset = root.mapFromGlobal(popup->mapToGlobal(QPoint(0, 0)));
        QPainter painter(&canvas);
        painter.drawPixmap(offset, popup->grab());
        painter.end();
    }
    canvas.save(oldWay ? QStringLiteral("combo_popup_old.png")
                       : QStringLiteral("combo_popup_new.png"));
    return 0;
}
