#include "devicecoordpanel.h"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QVBoxLayout>

namespace {

constexpr int kOuterRadius = 14;
constexpr int kInnerRadius = 12;
constexpr int kCellRadius = 8;

const QString &uiFontFamily()
{
    static const QString s = QStringLiteral(
        "'Noto Sans CJK SC', 'Microsoft YaHei', 'WenQuanYi Micro Hei', sans-serif");
    return s;
}

QString transparentStyle()
{
    return QStringLiteral("border: none; background: transparent; font-family: %1;")
        .arg(uiFontFamily());
}

} // namespace

DeviceCoordPanel::DeviceCoordPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumSize(520, 78);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(4);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(10, 0, 2, 0);
    titleRow->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("当前位姿"), this);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont titleFont;
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: #A8EAFF; %1").arg(transparentStyle()));

    auto *source = new QLabel(QStringLiteral("主控"), this);
    source->setAlignment(Qt::AlignCenter);
    QFont sourceFont;
    sourceFont.setPixelSize(11);
    sourceFont.setBold(true);
    source->setFont(sourceFont);
    source->setFixedHeight(20);
    source->setMinimumWidth(42);
    source->setStyleSheet(
        QStringLiteral("color: #0B2A3F;"
                       "background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
                       " stop:0 #5CE1FF, stop:1 #3BB8E8);"
                       "border: none; border-radius: 6px;"
                       "padding: 0 8px; font-family: %1;")
            .arg(uiFontFamily()));

    auto *hint = new QLabel(QStringLiteral("X / Y / Z / R"), this);
    hint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont hintFont;
    hintFont.setPixelSize(11);
    hint->setFont(hintFont);
    hint->setStyleSheet(QStringLiteral("color: #6FB8D8; %1").arg(transparentStyle()));

    titleRow->addWidget(title, 0);
    titleRow->addWidget(source, 0);
    titleRow->addStretch(1);
    titleRow->addWidget(hint, 0);

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_colX = addAxisColumn(row, QStringLiteral("X"), QStringLiteral("mm"), QColor(QStringLiteral("#5CE1FF")));
    m_colY = addAxisColumn(row, QStringLiteral("Y"), QStringLiteral("mm"), QColor(QStringLiteral("#6FE7A8")));
    m_colZ = addAxisColumn(row, QStringLiteral("Z"), QStringLiteral("mm"), QColor(QStringLiteral("#7AA8FF")));
    m_colR = addAxisColumn(row, QStringLiteral("R"), QStringLiteral("°"), QColor(QStringLiteral("#FFC56E")));

    root->addLayout(titleRow);
    root->addLayout(row, 1);
}

DeviceCoordPanel::AxisColumn DeviceCoordPanel::addAxisColumn(QHBoxLayout *row,
                                                            const QString &letter,
                                                            const QString &unitText,
                                                            const QColor &accent)
{
    AxisColumn col;

    col.cell = new QWidget(this);
    col.cell->setAttribute(Qt::WA_TranslucentBackground, true);
    col.cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *cellLayout = new QHBoxLayout(col.cell);
    cellLayout->setContentsMargins(8, 4, 8, 4);
    cellLayout->setSpacing(6);

    col.axisBadge = new QLabel(letter, col.cell);
    col.axisBadge->setAlignment(Qt::AlignCenter);
    col.axisBadge->setFixedSize(22, 22);
    QFont badgeFont;
    badgeFont.setPixelSize(12);
    badgeFont.setBold(true);
    col.axisBadge->setFont(badgeFont);
    col.axisBadge->setStyleSheet(
        QStringLiteral("color: %1;"
                       "background-color: rgba(%2, %3, %4, 38);"
                       "border: 1px solid rgba(%2, %3, %4, 160);"
                       "border-radius: 6px; font-family: %5;")
            .arg(accent.name())
            .arg(accent.red())
            .arg(accent.green())
            .arg(accent.blue())
            .arg(uiFontFamily()));

    auto *valueCol = new QVBoxLayout();
    valueCol->setContentsMargins(0, 0, 0, 0);
    valueCol->setSpacing(0);

    col.value = new QLabel(QStringLiteral("—"), col.cell);
    col.value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont valueFont;
    valueFont.setPixelSize(16);
    valueFont.setBold(true);
    col.value->setFont(valueFont);
    col.value->setStyleSheet(QStringLiteral("color: #F2FBFF; %1").arg(transparentStyle()));

    col.unit = new QLabel(unitText, col.cell);
    col.unit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont unitFont;
    unitFont.setPixelSize(10);
    col.unit->setFont(unitFont);
    col.unit->setStyleSheet(QStringLiteral("color: #7EC8E8; %1").arg(transparentStyle()));

    valueCol->addWidget(col.value);
    valueCol->addWidget(col.unit);

    cellLayout->addWidget(col.axisBadge, 0, Qt::AlignVCenter);
    cellLayout->addLayout(valueCol, 1);

    row->addWidget(col.cell, 1);
    return col;
}

QSize DeviceCoordPanel::sizeHint() const
{
    return QSize(560, 84);
}

QSize DeviceCoordPanel::minimumSizeHint() const
{
    return QSize(520, 78);
}

QString DeviceCoordPanel::formatCoord(double v)
{
    if (!std::isfinite(v)) {
        return QStringLiteral("—");
    }
    return QString::number(v, 'f', 3);
}

void DeviceCoordPanel::applyLabel(QLabel *label, double v)
{
    if (!label) {
        return;
    }
    label->setText(formatCoord(v));
}

void DeviceCoordPanel::setCoordinates(double x, double y, double z, double ar)
{
    m_coordX = x;
    m_coordY = y;
    m_coordZ = z;
    m_coordAr = ar;

    applyLabel(m_colX.value, x);
    applyLabel(m_colY.value, y);
    applyLabel(m_colZ.value, z);
    applyLabel(m_colR.value, ar);

    emit coordinatesChanged();
    update();
}

void DeviceCoordPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (bounds.width() <= 4.0 || bounds.height() <= 4.0) {
        return;
    }

    QLinearGradient panelGradient(bounds.topLeft(), bounds.bottomLeft());
    panelGradient.setColorAt(0.0, QColor(8, 34, 58, 226));
    panelGradient.setColorAt(1.0, QColor(4, 18, 34, 218));
    p.setPen(QPen(QColor(74, 190, 238, 132), 1.0));
    p.setBrush(panelGradient);
    p.drawRoundedRect(bounds, kOuterRadius, kOuterRadius);

    const QRectF inner = bounds.adjusted(2.0, 2.0, -2.0, -2.0);
    p.setPen(QPen(QColor(122, 224, 255, 74), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(inner, kInnerRadius, kInnerRadius);

    p.setOpacity(0.55);
    p.setPen(QPen(QColor(111, 231, 255, 120), 1.0));
    const qreal lineY = bounds.top() + 8.0;
    p.drawLine(QPointF(bounds.left() + 10.0, lineY),
               QPointF(bounds.right() - 10.0, lineY));
    p.setOpacity(1.0);

    // Title accent bar
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(92, 225, 255, 210));
    p.drawRoundedRect(QRectF(bounds.left() + 12.0, bounds.top() + 14.0, 3.0, 12.0), 1.5, 1.5);

    const AxisColumn *columns[] = {&m_colX, &m_colY, &m_colZ, &m_colR};
    for (const AxisColumn *column : columns) {
        if (!column->cell) {
            continue;
        }
        const QRectF cellRect = QRectF(column->cell->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (cellRect.width() < 8.0 || cellRect.height() < 8.0) {
            continue;
        }

        QLinearGradient cellGrad(cellRect.topLeft(), cellRect.bottomLeft());
        cellGrad.setColorAt(0.0, QColor(18, 58, 92, 120));
        cellGrad.setColorAt(1.0, QColor(10, 36, 60, 90));
        p.setPen(QPen(QColor(90, 180, 220, 70), 1.0));
        p.setBrush(cellGrad);
        p.drawRoundedRect(cellRect, kCellRadius, kCellRadius);
    }
}
