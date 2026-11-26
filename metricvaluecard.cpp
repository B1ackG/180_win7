#include "metricvaluecard.h"

#include <QPaintEvent>
#include <QPainter>

namespace {
constexpr int kOuterRadius = 14;
constexpr int kInnerRadius = 12;
} // namespace

MetricValueCard::MetricValueCard(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void MetricValueCard::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    update();
}

void MetricValueCard::setUnit(const QString &unit)
{
    if (m_unit == unit) {
        return;
    }
    m_unit = unit;
    update();
}

void MetricValueCard::setValue(double value, bool valid)
{
    if (m_valid == valid && qFuzzyCompare(m_value + 1.0, value + 1.0)) {
        return;
    }
    m_value = value;
    m_valid = valid;
    update();
}

void MetricValueCard::setDecimals(int decimals)
{
    m_decimals = qBound(0, decimals, 3);
    update();
}

void MetricValueCard::setAlertLevel(AlertLevel level)
{
    if (m_alertLevel == level) {
        return;
    }
    m_alertLevel = level;
    update();
}

QSize MetricValueCard::sizeHint() const
{
    return QSize(140, 100);
}

QSize MetricValueCard::minimumSizeHint() const
{
    return QSize(110, 86);
}

void MetricValueCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    if (bounds.width() <= 4.0 || bounds.height() <= 4.0) {
        return;
    }

    const bool alarm = m_alertLevel == AlertLevel::Alarm && m_valid;
    const QColor accent = alarm ? QColor(QStringLiteral("#FF6666")) : QColor(QStringLiteral("#5CE1FF"));
    const QColor panelTop = alarm ? QColor(58, 10, 10, 226) : QColor(8, 34, 58, 226);
    const QColor panelBottom = alarm ? QColor(34, 6, 6, 218) : QColor(4, 18, 34, 218);
    const QColor outerPen = alarm ? QColor(255, 102, 102, 180) : QColor(74, 190, 238, 132);
    const QColor innerPen = alarm ? QColor(255, 140, 120, 90) : QColor(122, 224, 255, 74);
    const QColor hairline = alarm ? QColor(255, 140, 120, 140) : QColor(111, 231, 255, 120);
    const QColor titleColor = alarm ? QColor(QStringLiteral("#FFD0D0")) : QColor(QStringLiteral("#A8EAFF"));
    const QColor valueColor = alarm ? QColor(QStringLiteral("#FFE8E8")) : QColor(QStringLiteral("#F2FBFF"));
    const QColor cellTop = alarm ? QColor(92, 22, 18, 140) : QColor(18, 58, 92, 120);
    const QColor cellBottom = alarm ? QColor(60, 12, 10, 110) : QColor(10, 36, 60, 90);
    const QColor cellPen = alarm ? QColor(255, 120, 110, 90) : QColor(90, 180, 220, 70);

    QLinearGradient panelGradient(bounds.topLeft(), bounds.bottomLeft());
    panelGradient.setColorAt(0.0, panelTop);
    panelGradient.setColorAt(1.0, panelBottom);
    painter.setPen(QPen(outerPen, 1.0));
    painter.setBrush(panelGradient);
    painter.drawRoundedRect(bounds, kOuterRadius, kOuterRadius);

    const QRectF inner = bounds.adjusted(2.0, 2.0, -2.0, -2.0);
    painter.setPen(QPen(innerPen, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner, kInnerRadius, kInnerRadius);

    painter.setOpacity(0.55);
    painter.setPen(QPen(hairline, 1.0));
    painter.drawLine(QPointF(bounds.left() + 10.0, bounds.top() + 8.0),
                     QPointF(bounds.right() - 10.0, bounds.top() + 8.0));
    painter.setOpacity(1.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 210));
    painter.drawRoundedRect(QRectF(bounds.left() + 12.0, bounds.top() + 14.0, 3.0, 12.0), 1.5, 1.5);

    QFont titleFont(QStringLiteral("Noto Sans CJK SC"), 9, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(titleColor);
    painter.drawText(QRectF(bounds.left() + 20.0, bounds.top() + 10.0, bounds.width() - 30.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_title);

    const QRectF valueCell(bounds.left() + 10.0,
                           bounds.top() + 36.0,
                           bounds.width() - 20.0,
                           bounds.height() - 48.0);
    if (valueCell.height() > 12.0) {
        QLinearGradient cellGrad(valueCell.topLeft(), valueCell.bottomLeft());
        cellGrad.setColorAt(0.0, cellTop);
        cellGrad.setColorAt(1.0, cellBottom);
        painter.setPen(QPen(cellPen, 1.0));
        painter.setBrush(cellGrad);
        painter.drawRoundedRect(valueCell, 8.0, 8.0);
    }

    const QString numberText = m_valid
        ? QString::number(m_value, 'f', m_decimals)
        : QStringLiteral("--");
    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 16, QFont::Bold);
    painter.setFont(valueFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal numberWidth = valueMetrics.horizontalAdvance(numberText);

    QFont unitFont(QStringLiteral("Noto Sans CJK SC"), 9, QFont::Bold);
    const QFontMetricsF unitMetrics(unitFont);
    const qreal unitWidth = m_valid ? unitMetrics.horizontalAdvance(m_unit) : 0.0;
    const qreal gap = m_valid ? 3.0 : 0.0;
    const qreal totalWidth = numberWidth + gap + unitWidth;
    const qreal baselineY = valueCell.center().y() + valueMetrics.ascent() * 0.35;
    const qreal startX = valueCell.center().x() - totalWidth * 0.5;

    painter.setPen(valueColor);
    painter.drawText(QPointF(startX, baselineY), numberText);
    if (m_valid) {
        painter.setFont(unitFont);
        painter.setPen(QColor(accent.red(), accent.green(), accent.blue(), 220));
        painter.drawText(QPointF(startX + numberWidth + gap,
                                 baselineY - (valueMetrics.ascent() - unitMetrics.ascent()) * 0.35),
                         m_unit);
    }
}
