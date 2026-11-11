#include "timeseriesdatazoom.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

TimeSeriesDataZoom::TimeSeriesDataZoom(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(54);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

void TimeSeriesDataZoom::setRange(int minimum, int maximum)
{
    if (maximum <= minimum) {
        maximum = minimum + 1;
    }
    m_minimum = minimum;
    m_maximum = maximum;
    applySelectedRange(m_lower, m_upper, false);
    update();
}

void TimeSeriesDataZoom::setSelectedRange(int lower, int upper)
{
    applySelectedRange(lower, upper, false);
    update();
}

QPair<int, int> TimeSeriesDataZoom::selectedRange() const
{
    return qMakePair(m_lower, m_upper);
}

void TimeSeriesDataZoom::setMinimumSpan(int span)
{
    m_minSpan = qMax(1, span);
    applySelectedRange(m_lower, m_upper, false);
    update();
}

QSize TimeSeriesDataZoom::sizeHint() const
{
    return QSize(400, 54);
}

QSize TimeSeriesDataZoom::minimumSizeHint() const
{
    return QSize(200, 54);
}

void TimeSeriesDataZoom::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(8, 18, 28));

    const QRect track = trackRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(16, 36, 51));
    painter.drawRoundedRect(track, 6, 6);

    const QRect window = windowRect();
    painter.setBrush(QColor(0, 200, 255, 90));
    painter.drawRoundedRect(window, 6, 6);

    painter.setBrush(QColor(QStringLiteral("#5CE1FF")));
    painter.drawRoundedRect(lowerHandleRect(), 4, 4);
    painter.drawRoundedRect(upperHandleRect(), 4, 4);
}

void TimeSeriesDataZoom::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_dragPart = hitTest(event->pos());
    if (m_dragPart == DragPart::Window) {
        m_dragOffset = valueAtX(event->pos().x()) - m_lower;
        m_dragSpan = m_upper - m_lower;
    }
}

void TimeSeriesDataZoom::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragPart == DragPart::None) {
        return;
    }
    const int value = valueAtX(event->pos().x());
    if (m_dragPart == DragPart::Lower) {
        applySelectedRange(value, m_upper, true);
    } else if (m_dragPart == DragPart::Upper) {
        applySelectedRange(m_lower, value, true);
    } else {
        int lower = value - m_dragOffset;
        int upper = lower + m_dragSpan;
        if (lower < m_minimum) {
            lower = m_minimum;
            upper = lower + m_dragSpan;
        }
        if (upper > m_maximum) {
            upper = m_maximum;
            lower = upper - m_dragSpan;
        }
        applySelectedRange(lower, upper, true);
    }
    update();
}

void TimeSeriesDataZoom::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPart = DragPart::None;
    }
}

void TimeSeriesDataZoom::leaveEvent(QEvent *)
{
}

QRect TimeSeriesDataZoom::trackRect() const
{
    return rect().adjusted(8, (height() - 16) / 2, -8, -(height() - 16) / 2);
}

QRect TimeSeriesDataZoom::windowRect() const
{
    const QRect track = trackRect();
    return QRect(QPoint(xAtValue(m_lower), track.top()),
                 QPoint(xAtValue(m_upper), track.bottom()));
}

QRect TimeSeriesDataZoom::lowerHandleRect() const
{
    const QRect window = windowRect();
    return QRect(window.left() - 10, (height() - 32) / 2, 20, 32);
}

QRect TimeSeriesDataZoom::upperHandleRect() const
{
    const QRect window = windowRect();
    return QRect(window.right() - 10, (height() - 32) / 2, 20, 32);
}

int TimeSeriesDataZoom::valueAtX(int x) const
{
    const QRect track = trackRect();
    if (track.width() <= 0) {
        return m_minimum;
    }
    const qreal ratio = qBound(0.0, static_cast<qreal>(x - track.left()) / track.width(), 1.0);
    return m_minimum + static_cast<int>((m_maximum - m_minimum) * ratio + 0.5);
}

int TimeSeriesDataZoom::xAtValue(int value) const
{
    const QRect track = trackRect();
    const int span = qMax(1, m_maximum - m_minimum);
    const qreal ratio = qBound(0.0, static_cast<qreal>(value - m_minimum) / span, 1.0);
    return track.left() + static_cast<int>(track.width() * ratio);
}

void TimeSeriesDataZoom::applySelectedRange(int lower, int upper, bool emitChange)
{
    if (upper < lower) {
        qSwap(lower, upper);
    }
    lower = qBound(m_minimum, lower, m_maximum);
    upper = qBound(m_minimum, upper, m_maximum);
    if (upper - lower < m_minSpan) {
        if (m_dragPart == DragPart::Upper) {
            upper = qMin(m_maximum, lower + m_minSpan);
            lower = upper - m_minSpan;
        } else {
            lower = qMax(m_minimum, upper - m_minSpan);
            upper = lower + m_minSpan;
        }
        lower = qBound(m_minimum, lower, m_maximum - m_minSpan);
        upper = qBound(m_minimum + m_minSpan, upper, m_maximum);
    }
    if (lower == m_lower && upper == m_upper) {
        return;
    }
    m_lower = lower;
    m_upper = upper;
    if (emitChange) {
        emit selectedRangeChanged(m_lower, m_upper);
    }
}

TimeSeriesDataZoom::DragPart TimeSeriesDataZoom::hitTest(const QPoint &pos) const
{
    const QRect lower = lowerHandleRect().adjusted(-12, -6, 12, 6);
    const QRect upper = upperHandleRect().adjusted(-12, -6, 12, 6);
    if (lower.contains(pos)) {
        return DragPart::Lower;
    }
    if (upper.contains(pos)) {
        return DragPart::Upper;
    }
    if (windowRect().adjusted(0, -8, 0, 8).contains(pos)) {
        return DragPart::Window;
    }
    if (pos.x() < windowRect().center().x()) {
        return DragPart::Lower;
    }
    return DragPart::Upper;
}
