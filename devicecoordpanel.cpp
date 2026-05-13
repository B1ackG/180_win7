#include "devicecoordpanel.h"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {

constexpr int kOuterRadius = 10;
constexpr int kInnerRadius = 8;

const QString &labelFontFamily()
{
    static const QString s = QStringLiteral(
        "'Noto Sans CJK SC', 'Microsoft YaHei', 'WenQuanYi Micro Hei', sans-serif");
    return s;
}

} // namespace

DeviceCoordPanel::DeviceCoordPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumSize(460, 70);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->setSpacing(1);

    auto *title = new QLabel(QStringLiteral("当前位姿 (主控)"), this);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont titleFont;
    titleFont.setPixelSize(12);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: #A6D8FF; border: none; background: transparent; font-family: %1;")
                             .arg(labelFontFamily()));

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    auto addColumn = [this, row](const QString &letter, QLabel **outValue) {
        auto *col = new QVBoxLayout();
        col->setSpacing(1);

        auto *axis = new QLabel(letter, this);
        axis->setAlignment(Qt::AlignHCenter);
        QFont axisFont;
        axisFont.setPixelSize(11);
        axisFont.setBold(true);
        axis->setFont(axisFont);
        axis->setStyleSheet(QStringLiteral("color: #A8DAFF; border: none; background: transparent; font-family: %1;")
                                 .arg(labelFontFamily()));

        auto *value = new QLabel(QStringLiteral("—"), this);
        value->setAlignment(Qt::AlignHCenter);
        QFont valueFont;
        valueFont.setPixelSize(17);
        valueFont.setBold(true);
        value->setFont(valueFont);
        value->setStyleSheet(QStringLiteral("color: #EAF7FF; border: none; background: transparent; font-family: %1;")
                                  .arg(labelFontFamily()));

        col->addWidget(axis);
        col->addWidget(value);
        row->addLayout(col, 1);
        *outValue = value;
    };

    addColumn(QStringLiteral("X"), &m_valueX);
    addColumn(QStringLiteral("Y"), &m_valueY);
    addColumn(QStringLiteral("Z"), &m_valueZ);
    addColumn(QStringLiteral("R"), &m_valueR);

    root->addWidget(title);
    root->addLayout(row);
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

    applyLabel(m_valueX, x);
    applyLabel(m_valueY, y);
    applyLabel(m_valueZ, z);
    applyLabel(m_valueR, ar);

    emit coordinatesChanged();
}

void DeviceCoordPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath outer;
    outer.addRoundedRect(r, kOuterRadius, kOuterRadius);
    p.fillPath(outer, QColor(QStringLiteral("#1A5FB4")));
    p.setPen(QPen(QColor(QStringLiteral("#4FAFE8")), 1.0));
    p.drawPath(outer);

    const QRectF inner = r.adjusted(2, 2, -2, -2);
    QPainterPath innerPath;
    innerPath.addRoundedRect(inner, kInnerRadius, kInnerRadius);
    p.setPen(QPen(QColor(42, 159, 231, 170), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(innerPath);
}
