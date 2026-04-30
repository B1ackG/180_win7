#include "robottotalpowercard.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

RobotTotalPowerCard::RobotTotalPowerCard(QWidget *parent)
    : QWidget(parent)
    , m_title(QStringLiteral("总功率"))
    , m_unit(QStringLiteral("W"))
{
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void RobotTotalPowerCard::setCurrentPower(double power)
{
    const double clamped = qMax(0.0, power);
    if (qFuzzyCompare(m_currentPower + 1.0, clamped + 1.0)) {
        appendSample(clamped);
        update();
        return;
    }

    m_currentPower = clamped;
    appendSample(clamped);
    emit currentPowerChanged();
    update();
}

void RobotTotalPowerCard::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    update();
}

void RobotTotalPowerCard::setUnit(const QString &unit)
{
    if (m_unit == unit) {
        return;
    }
    m_unit = unit;
    update();
}

void RobotTotalPowerCard::appendSample(double power)
{
    m_samples.push_back(power);
    while (m_samples.size() > m_maxSamples) {
        m_samples.pop_front();
    }

    double localMax = 100.0;
    for (double v : m_samples) {
        localMax = qMax(localMax, v);
    }
    m_maxDisplayPower = qMax(100.0, localMax * 1.2);
}

void RobotTotalPowerCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    if (r.width() <= 4 || r.height() <= 4) {
        return;
    }

    painter.setPen(QPen(QColor("#4FAFE8"), 1));
    painter.setBrush(QColor("#1A5FB4"));
    painter.drawRoundedRect(r, 18, 18);

    const QRectF inner = r.adjusted(2, 2, -2, -2);
    painter.setPen(QPen(QColor("#2A9FE7AA"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner, 16, 16);

    painter.setPen(QPen(QColor("#67C5F6"), 1));
    painter.setOpacity(0.45);
    painter.drawLine(QPointF(r.left() + 10, r.top() + 10), QPointF(r.right() - 10, r.top() + 10));
    painter.setOpacity(1.0);

    QFont titleFont(QStringLiteral("Noto Sans CJK SC"), 13, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(QColor("#A6D8FF"));
    painter.drawText(QRectF(r.left() + 12, r.top() + 8, 130, 24), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 22, QFont::Bold);
    painter.setFont(valueFont);
    painter.setPen(QColor("#EAF7FF"));
    const QString valueText = QString::number(qRound(m_currentPower)) + " " + m_unit;
    painter.drawText(QRectF(r.right() - 180, r.top() + 6, 168, 38), Qt::AlignRight | Qt::AlignVCenter, valueText);

    QFont subFont(QStringLiteral("Noto Sans CJK SC"), 11, QFont::Normal);
    painter.setFont(subFont);
    painter.setPen(QColor("#89D1FF"));
    painter.drawText(QRectF(r.left() + 12, r.top() + 34, 80, 18), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("实时趋势"));

    const QRectF chartRect(r.left() + 12, r.top() + 54, r.width() - 24, r.height() - 64);
    if (chartRect.width() <= 4 || chartRect.height() <= 4) {
        return;
    }

    QPen gridPen(QColor("#4A95C9CC"), 1, Qt::DashLine);
    painter.setPen(gridPen);
    for (int i = 0; i < 4; ++i) {
        const qreal y = chartRect.top() + (chartRect.height() * i / 3.0);
        painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
    }

    if (m_samples.isEmpty()) {
        return;
    }

    QVector<QPointF> points;
    points.reserve(m_samples.size());
    const int span = qMax(1, m_samples.size() - 1);
    for (int i = 0; i < m_samples.size(); ++i) {
        const qreal x = chartRect.left() + chartRect.width() * i / span;
        const qreal ratio = qBound(0.0, m_samples[i] / m_maxDisplayPower, 1.0);
        const qreal y = chartRect.bottom() - chartRect.height() * ratio;
        points.push_back(QPointF(x, y));
    }
    if (points.size() == 1) {
        points.push_back(QPointF(chartRect.right(), points.first().y()));
    }

    QPainterPath areaPath;
    areaPath.moveTo(chartRect.left(), chartRect.bottom());
    for (const QPointF &p : points) {
        areaPath.lineTo(p);
    }
    areaPath.lineTo(chartRect.right(), chartRect.bottom());
    areaPath.closeSubpath();

    QLinearGradient areaGrad(chartRect.topLeft(), chartRect.bottomLeft());
    areaGrad.setColorAt(0.0, QColor("#3D8FE08A"));
    areaGrad.setColorAt(1.0, QColor("#0A2F5C14"));
    painter.fillPath(areaPath, areaGrad);

    QPen linePen(QColor("#69D0FF"), 2);
    linePen.setCapStyle(Qt::RoundCap);
    painter.setPen(linePen);
    for (int i = 1; i < points.size(); ++i) {
        painter.drawLine(points[i - 1], points[i]);
    }
}
