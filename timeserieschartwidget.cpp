#include "timeserieschartwidget.h"

#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QGestureEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPinchGesture>
#include <QPolygon>
#include <QStringList>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QtMath>
#include <limits>

namespace {

const QColor kPlotColors[] = {
    QColor(QStringLiteral("#5CE1FF")),
    QColor(QStringLiteral("#FFB020")),
    QColor(QStringLiteral("#7CFF8E")),
    QColor(QStringLiteral("#FF6B8A"))
};

constexpr int kReadoutHeight = 48;
constexpr int kTitleHeight = 44;
constexpr int kBottomTickHeight = 22;
constexpr int kLeftAxisWidth = 72;
constexpr int kMinVisibleSpanMs = 1000;

} // namespace

TimeSeriesChartWidget::TimeSeriesChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_emptyText(QStringLiteral("没有可显示的曲线数据"))
{
    setMouseTracking(false);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    grabGesture(Qt::PinchGesture);
    setMinimumSize(640, 360);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(8, 18, 28));
    setPalette(pal);
}

void TimeSeriesChartWidget::setSeries(const TimeSeriesQueryResult &result)
{
    m_result = result;
    m_hasPoints = false;
    m_dataStartMs = result.startUtcMs;
    m_dataEndMs = qMax(result.endUtcMs, result.startUtcMs + 1);
    for (const TimeSeriesSeries &series : result.series) {
        if (series.points.isEmpty()) {
            continue;
        }
        if (!m_hasPoints) {
            m_dataStartMs = series.points.first().tsUtcMs;
            m_dataEndMs = series.points.last().tsUtcMs;
            m_hasPoints = true;
        } else {
            m_dataStartMs = qMin(m_dataStartMs, series.points.first().tsUtcMs);
            m_dataEndMs = qMax(m_dataEndMs, series.points.last().tsUtcMs);
        }
    }
    if (m_dataEndMs <= m_dataStartMs) {
        m_dataEndMs = m_dataStartMs + 1;
    }
    clearCursor();
    applyVisibleRange(m_dataStartMs, m_dataEndMs, true);
    if (!m_hasPoints) {
        m_emptyText = QStringLiteral("该时间范围内没有采样数据");
    }
    update();
}

void TimeSeriesChartWidget::clearSeries()
{
    m_result = TimeSeriesQueryResult();
    m_hasPoints = false;
    m_emptyText = QStringLiteral("没有可显示的曲线数据");
    m_dataStartMs = 0;
    m_dataEndMs = 1;
    clearCursor();
    applyVisibleRange(0, 1, true);
    update();
}

void TimeSeriesChartWidget::resetView()
{
    applyVisibleRange(m_dataStartMs, m_dataEndMs, true);
    update();
}

void TimeSeriesChartWidget::setInteractionMode(InteractionMode mode)
{
    m_mode = mode;
    m_pointerActive = false;
    m_pressInsidePlot = false;
}

void TimeSeriesChartWidget::setCursorTime(qint64 utcMs, bool visible)
{
    const qint64 clamped = qBound(m_viewStartMs, utcMs, m_viewEndMs);
    const bool changed = (clamped != m_cursorTimeMs) || (visible != m_cursorVisible);
    m_cursorTimeMs = clamped;
    m_cursorVisible = visible;
    if (changed && visible) {
        emit cursorTimeChanged(m_cursorTimeMs);
    }
    update();
}

void TimeSeriesChartWidget::clearCursor()
{
    m_cursorVisible = false;
    m_cursorTimeMs = m_viewStartMs;
    update();
}

void TimeSeriesChartWidget::setVisibleRange(qint64 startUtcMs, qint64 endUtcMs)
{
    applyVisibleRange(startUtcMs, endUtcMs, true);
    update();
}

void TimeSeriesChartWidget::zoomAround(qint64 anchorUtcMs, qreal scaleFactor)
{
    if (!canPanZoom() || scaleFactor <= 0.0) {
        return;
    }
    const qint64 span = qMax<qint64>(minimumSpan(), m_viewEndMs - m_viewStartMs);
    const qreal ratio = qBound(0.0, static_cast<qreal>(anchorUtcMs - m_viewStartMs) / span, 1.0);
    qint64 newSpan = static_cast<qint64>(span * scaleFactor);
    newSpan = qBound<qint64>(minimumSpan(), newSpan, dataSpanMs());
    const qint64 newStart = anchorUtcMs - static_cast<qint64>(newSpan * ratio);
    applyVisibleRange(newStart, newStart + newSpan, true);
    update();
}

QPair<qint64, qint64> TimeSeriesChartWidget::visibleRange() const
{
    return qMakePair(m_viewStartMs, m_viewEndMs);
}

void TimeSeriesChartWidget::setFullscreenButtonsVisible(bool visible)
{
    m_fullscreenButtonsVisible = visible;
    update();
}

QRect TimeSeriesChartWidget::fullscreenButtonRectFor(TimeSeriesChannel channel) const
{
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    for (const PlotRect &item : plots) {
        if (item.channel == channel) {
            return item.fullscreenButtonRect;
        }
    }
    return QRect();
}

QRect TimeSeriesChartWidget::plotRectFor(TimeSeriesChannel channel) const
{
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    for (const PlotRect &item : plots) {
        if (item.channel == channel) {
            return item.plot;
        }
    }
    return QRect();
}

QPoint TimeSeriesChartWidget::extremumMarkerPos(TimeSeriesChannel channel, bool isMax) const
{
    const TimeSeriesSeries *series = seriesFor(channel);
    if (!series) {
        return QPoint();
    }
    TimeSeriesPoint minPoint;
    TimeSeriesPoint maxPoint;
    if (!queryExtrema(*series, &minPoint, &maxPoint)) {
        return QPoint();
    }
    const TimeSeriesPoint &point = isMax ? maxPoint : minPoint;
    if (point.tsUtcMs < m_viewStartMs || point.tsUtcMs > m_viewEndMs) {
        return QPoint();
    }
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    for (const PlotRect &item : plots) {
        if (item.channel != channel) {
            continue;
        }
        return QPoint(xAtTime(point.tsUtcMs, item.plot), yAtValue(point.value, item));
    }
    return QPoint();
}

namespace {

QString nearestPointText(const QVector<TimeSeriesPoint> &points,
                         qint64 cursorTs,
                         qint64 gap,
                         TimeSeriesChannel channel,
                         int decimals)
{
    if (points.isEmpty()) {
        return QStringLiteral("--");
    }
    int nearest = 0;
    qint64 best = std::numeric_limits<qint64>::max();
    for (int i = 0; i < points.size(); ++i) {
        const qint64 delta = qAbs(points.at(i).tsUtcMs - cursorTs);
        if (delta < best) {
            best = delta;
            nearest = i;
        }
    }
    if (best > gap) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 %2")
        .arg(QString::number(points.at(nearest).value, 'f', decimals),
             TimeSeriesMeta::unit(channel));
}

} // namespace

QString TimeSeriesChartWidget::cursorValueText(TimeSeriesChannel channel) const
{
    if (!m_cursorVisible) {
        return QStringLiteral("--");
    }
    const TimeSeriesSeries *series = seriesFor(channel);
    if (!series) {
        return QStringLiteral("--");
    }
    return nearestPointText(series->points, m_cursorTimeMs, gapMs(), channel,
                            TimeSeriesMeta::displayDecimals(channel));
}

QString TimeSeriesChartWidget::cursorReadoutText() const
{
    if (!m_cursorVisible) {
        return QStringLiteral("点击图表显示游标");
    }
    QStringList parts;
    parts << QDateTime::fromMSecsSinceEpoch(m_cursorTimeMs, Qt::LocalTime)
                 .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    for (const TimeSeriesSeries &series : m_result.series) {
        parts << QStringLiteral("%1：%2")
                     .arg(TimeSeriesMeta::displayName(series.channel),
                          cursorValueText(series.channel));
    }
    return parts.join(QStringLiteral("    "));
}

bool TimeSeriesChartWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        return gestureEvent(static_cast<QGestureEvent*>(event));
    }
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchUpdate
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchCancel) {
        return handleTouchEvent(static_cast<QTouchEvent*>(event));
    }
    return QWidget::event(event);
}

void TimeSeriesChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(8, 18, 28));

    painter.fillRect(readoutRect(), QColor(6, 22, 34));
    painter.setPen(QColor(QStringLiteral("#eaf9ff")));
    painter.drawText(readoutRect().adjusted(12, 0, -12, 0),
                     Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                     cursorReadoutText());

    const QVector<PlotRect> plots = layoutPlots(contentRect());
    if (plots.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#9ad7ea")));
        painter.drawText(contentRect(), Qt::AlignCenter, m_emptyText);
        return;
    }

    for (int i = 0; i < plots.size(); ++i) {
        const PlotRect &item = plots.at(i);
        painter.setPen(QColor(0, 200, 255, 80));
        painter.setBrush(QColor(6, 22, 34, 180));
        painter.drawRect(item.plot);

        drawGrid(painter, item);

        QFont titleFont = painter.font();
        titleFont.setPixelSize(15);
        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#eaf9ff")));
        const QString title = QStringLiteral("%1 (%2)")
                                  .arg(TimeSeriesMeta::displayName(item.channel),
                                       TimeSeriesMeta::unit(item.channel));
        const QRect titleRect(item.plot.left(),
                              item.plot.top() - kTitleHeight,
                              item.plot.width() - (m_fullscreenButtonsVisible ? 48 : 0),
                              kTitleHeight);
        const TimeSeriesSeries *series = seriesFor(item.channel);
        TimeSeriesPoint minPoint;
        TimeSeriesPoint maxPoint;
        QString statsText;
        if (series && queryExtrema(*series, &minPoint, &maxPoint)) {
            statsText = QStringLiteral("最小 %1  最大 %2")
                            .arg(formatValue(item.channel, minPoint.value),
                                 formatValue(item.channel, maxPoint.value));
        }
        const QFontMetrics titleMetrics(titleFont);
        const int statsWidth = statsText.isEmpty()
            ? 0
            : titleMetrics.horizontalAdvance(statsText) + 12;
        const QRect nameRect = titleRect.adjusted(4, 0, -statsWidth - 4, 0);
        painter.drawText(nameRect,
                         Qt::AlignVCenter | Qt::AlignLeft,
                         titleMetrics.elidedText(title, Qt::ElideRight, nameRect.width()));
        if (!statsText.isEmpty()) {
            painter.setPen(kPlotColors[i % 4]);
            painter.drawText(QRect(titleRect.right() - statsWidth,
                                   titleRect.top(),
                                   statsWidth,
                                   titleRect.height()),
                             Qt::AlignVCenter | Qt::AlignRight,
                             statsText);
        }

        if (m_fullscreenButtonsVisible) {
            painter.setBrush(QColor(18, 54, 74));
            painter.setPen(QColor(QStringLiteral("#5CE1FF")));
            painter.drawRoundedRect(item.fullscreenButtonRect, 6, 6);
            painter.drawText(item.fullscreenButtonRect, Qt::AlignCenter, QStringLiteral("⛶"));
        }

        bool drewAny = false;
        if (series) {
            drawSeriesPath(painter, series->points, item, kPlotColors[i % 4], &drewAny);
            drawExtrema(painter, *series, item, kPlotColors[i % 4]);
        }
        if (!drewAny) {
            painter.setPen(QColor(QStringLiteral("#9ad7ea")));
            painter.drawText(item.plot, Qt::AlignCenter, QStringLiteral("该通道在当前范围内无数据"));
        }
    }

    if (m_cursorVisible && m_viewStartMs <= m_cursorTimeMs && m_cursorTimeMs <= m_viewEndMs) {
        const int x = xAtTime(m_cursorTimeMs, plots.first().plot);
        painter.setPen(QPen(QColor(255, 255, 255, 200), 2));
        painter.drawLine(QPoint(x, plots.first().plot.top()),
                         QPoint(x, plots.last().plot.bottom()));
    }
}

void TimeSeriesChartWidget::wheelEvent(QWheelEvent *event)
{
    if (!canPanZoom()) {
        return;
    }
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    if (plots.isEmpty()) {
        return;
    }
    const qreal factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    zoomAround(timeAtX(pos.x(), plots.first().plot), factor);
}

void TimeSeriesChartWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_touchActive || isSynthesizedMouse(event) || event->button() != Qt::LeftButton) {
        return;
    }
    beginPointer(event->pos());
}

void TimeSeriesChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_touchActive || isSynthesizedMouse(event)) {
        return;
    }
    updatePointer(event->pos());
}

void TimeSeriesChartWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_touchActive || isSynthesizedMouse(event) || event->button() != Qt::LeftButton) {
        return;
    }
    endPointer();
}

void TimeSeriesChartWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_touchActive || isSynthesizedMouse(event)) {
        return;
    }
    resetView();
}

void TimeSeriesChartWidget::leaveEvent(QEvent *)
{
    m_pointerActive = false;
    m_pressInsidePlot = false;
}

bool TimeSeriesChartWidget::gestureEvent(QGestureEvent *event)
{
    if (QPinchGesture *pinch = static_cast<QPinchGesture*>(event->gesture(Qt::PinchGesture))) {
        if (!canPanZoom()) {
            event->accept();
            return true;
        }
        const QVector<PlotRect> plots = layoutPlots(contentRect());
        if (plots.isEmpty()) {
            event->accept();
            return true;
        }
        if (pinch->state() == Qt::GestureUpdated || pinch->state() == Qt::GestureFinished) {
            const qreal scale = pinch->scaleFactor();
            if (scale > 0.0 && !qFuzzyCompare(scale, 1.0)) {
                QPointF center = pinch->centerPoint();
                if (center.isNull()) {
                    center = mapFromGlobal(pinch->hotSpot().toPoint());
                }
                const qint64 anchor = timeAtX(static_cast<int>(center.x()), plots.first().plot);
                zoomAround(anchor, 1.0 / scale);
            }
        }
        event->accept();
        return true;
    }
    return false;
}

bool TimeSeriesChartWidget::handleTouchEvent(QTouchEvent *event)
{
    const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    if (event->type() == QEvent::TouchBegin) {
        m_touchActive = true;
        if (points.size() == 1) {
            beginPointer(points.first().pos().toPoint());
        } else {
            m_pointerActive = false;
            m_pressInsidePlot = false;
        }
        event->accept();
        return true;
    }
    if (event->type() == QEvent::TouchUpdate) {
        if (points.size() >= 3) {
            event->accept();
            return true;
        }
        if (points.size() == 1) {
            updatePointer(points.first().pos().toPoint());
        } else {
            m_pointerActive = false;
            m_pressInsidePlot = false;
        }
        event->accept();
        return true;
    }
    m_touchActive = false;
    endPointer();
    event->accept();
    return true;
}

bool TimeSeriesChartWidget::handleFullscreenHit(const QPoint &pos)
{
    if (!m_fullscreenButtonsVisible || !isEnabled()) {
        return false;
    }
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    for (const PlotRect &item : plots) {
        if (item.fullscreenButtonRect.contains(pos)) {
            emit fullscreenRequested(item.channel);
            return true;
        }
    }
    return false;
}

bool TimeSeriesChartWidget::plotAreaContains(const QPoint &pos) const
{
    const QVector<PlotRect> plots = layoutPlots(contentRect());
    for (const PlotRect &item : plots) {
        if (item.plot.contains(pos)) {
            return true;
        }
    }
    return false;
}

void TimeSeriesChartWidget::beginPointer(const QPoint &pos)
{
    if (handleFullscreenHit(pos)) {
        m_pointerActive = false;
        m_pressInsidePlot = false;
        return;
    }
    m_pressInsidePlot = plotAreaContains(pos);
    m_lastPointerPos = pos;
    m_pointerActive = m_pressInsidePlot;
    if (!m_pointerActive) {
        return;
    }
    if (m_mode == InteractionMode::Cursor) {
        const QVector<PlotRect> plots = layoutPlots(contentRect());
        if (!plots.isEmpty()) {
            setCursorTime(timeAtX(pos.x(), plots.first().plot), true);
        }
    }
}

void TimeSeriesChartWidget::updatePointer(const QPoint &pos)
{
    if (!m_pointerActive || !m_pressInsidePlot) {
        return;
    }
    if (m_mode == InteractionMode::Cursor) {
        const QVector<PlotRect> plots = layoutPlots(contentRect());
        if (!plots.isEmpty()) {
            setCursorTime(timeAtX(pos.x(), plots.first().plot), true);
        }
    } else if (canPanZoom()) {
        const QVector<PlotRect> plots = layoutPlots(contentRect());
        if (!plots.isEmpty()) {
            panByPixels(m_lastPointerPos.x() - pos.x(), plots.first().plot);
        }
    }
    m_lastPointerPos = pos;
}

void TimeSeriesChartWidget::endPointer()
{
    m_pointerActive = false;
    m_pressInsidePlot = false;
}

void TimeSeriesChartWidget::panByPixels(int dx, const QRect &plot)
{
    if (plot.width() <= 0 || dx == 0 || !canPanZoom()) {
        return;
    }
    const qint64 span = qMax<qint64>(1, m_viewEndMs - m_viewStartMs);
    const qint64 delta = static_cast<qint64>(dx) * span / plot.width();
    applyVisibleRange(m_viewStartMs + delta, m_viewEndMs + delta, true);
    update();
}

void TimeSeriesChartWidget::drawGrid(QPainter &painter, const PlotRect &plot) const
{
    painter.save();
    painter.setPen(QPen(QColor(154, 215, 234, 55), 1));
    for (int i = 0; i < 6; ++i) {
        const int x = plot.plot.left() + plot.plot.width() * i / 5;
        painter.drawLine(x, plot.plot.top(), x, plot.plot.bottom());
        const qint64 ts = timeAtX(x, plot.plot);
        painter.setPen(QColor(154, 215, 234, 180));
        painter.drawText(QRect(x - 48, plot.plot.bottom() + 2, 96, kBottomTickHeight - 2),
                         Qt::AlignHCenter | Qt::AlignTop,
                         formatTimeTick(ts));
        painter.setPen(QPen(QColor(154, 215, 234, 55), 1));
    }
    for (int i = 0; i < 5; ++i) {
        const int y = plot.plot.top() + plot.plot.height() * i / 4;
        painter.drawLine(plot.plot.left(), y, plot.plot.right(), y);
        const double span = plot.maxValue - plot.minValue;
        const double value = plot.maxValue - span * i / 4.0;
        painter.setPen(QColor(154, 215, 234, 180));
        painter.drawText(QRect(plot.plot.left() - kLeftAxisWidth,
                               y - 10,
                               kLeftAxisWidth - 6,
                               20),
                         Qt::AlignRight | Qt::AlignVCenter,
                         formatValue(plot.channel, value));
        painter.setPen(QPen(QColor(154, 215, 234, 55), 1));
    }
    painter.restore();
}

QString TimeSeriesChartWidget::formatTimeTick(qint64 utcMs) const
{
    const qint64 span = qMax<qint64>(1, m_viewEndMs - m_viewStartMs);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(utcMs, Qt::LocalTime);
    if (span < 24LL * 3600 * 1000) {
        return dt.toString(QStringLiteral("HH:mm:ss"));
    }
    if (span <= 7LL * 24 * 3600 * 1000) {
        return dt.toString(QStringLiteral("MM-dd HH:mm"));
    }
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}

QString TimeSeriesChartWidget::formatValue(TimeSeriesChannel channel, double value) const
{
    return QString::number(value, 'f', TimeSeriesMeta::displayDecimals(channel));
}

void TimeSeriesChartWidget::applyVisibleRange(qint64 startUtcMs, qint64 endUtcMs, bool emitChange)
{
    if (endUtcMs < startUtcMs) {
        qSwap(startUtcMs, endUtcMs);
    }
    const qint64 spanLimit = dataSpanMs();
    const qint64 minSpan = minimumSpan();
    if (!m_hasPoints || spanLimit <= minSpan) {
        startUtcMs = m_dataStartMs;
        endUtcMs = m_dataEndMs;
    } else {
        qint64 span = qMax<qint64>(minSpan, endUtcMs - startUtcMs);
        span = qMin(span, spanLimit);
        if (startUtcMs < m_dataStartMs) {
            startUtcMs = m_dataStartMs;
        }
        if (startUtcMs + span > m_dataEndMs) {
            startUtcMs = m_dataEndMs - span;
        }
        if (startUtcMs < m_dataStartMs) {
            startUtcMs = m_dataStartMs;
            span = qMin(span, m_dataEndMs - m_dataStartMs);
        }
        endUtcMs = startUtcMs + span;
        if (endUtcMs > m_dataEndMs) {
            endUtcMs = m_dataEndMs;
        }
    }
    if (startUtcMs == m_viewStartMs && endUtcMs == m_viewEndMs) {
        return;
    }
    m_viewStartMs = startUtcMs;
    m_viewEndMs = endUtcMs;
    if (emitChange) {
        emit visibleRangeChanged(m_viewStartMs, m_viewEndMs);
    }
}

QRect TimeSeriesChartWidget::readoutRect() const
{
    return QRect(0, 0, width(), kReadoutHeight);
}

QVector<TimeSeriesChartWidget::PlotRect> TimeSeriesChartWidget::layoutPlots(const QRect &area) const
{
    QVector<PlotRect> plots;
    const int count = m_result.series.size();
    if (count <= 0 || area.height() <= 0) {
        return plots;
    }
    const int plotHeight = qMax(80, area.height() / count);
    for (int i = 0; i < count; ++i) {
        PlotRect item;
        item.channel = m_result.series.at(i).channel;
        const int top = area.top() + i * plotHeight;
        item.plot = QRect(area.left() + kLeftAxisWidth,
                          top + kTitleHeight,
                          qMax(40, area.width() - kLeftAxisWidth - 8),
                          qMax(24, plotHeight - kTitleHeight - kBottomTickHeight));
        item.fullscreenButtonRect = QRect(item.plot.right() - 44,
                                          top,
                                          44,
                                          44);
        valueRange(item.channel, &item.minValue, &item.maxValue);
        const TimeSeriesSeries *series = seriesFor(item.channel);
        if (series) {
            for (const TimeSeriesPoint &point : series->points) {
                if (point.tsUtcMs >= m_viewStartMs && point.tsUtcMs <= m_viewEndMs) {
                    item.hasVisiblePoints = true;
                    break;
                }
            }
        }
        plots.append(item);
    }
    return plots;
}

QRect TimeSeriesChartWidget::contentRect() const
{
    return rect().adjusted(8, kReadoutHeight + 4, -8, -4);
}

qint64 TimeSeriesChartWidget::timeAtX(int x, const QRect &plot) const
{
    if (plot.width() <= 0) {
        return m_viewStartMs;
    }
    const qreal ratio = qBound(0.0, static_cast<qreal>(x - plot.left()) / plot.width(), 1.0);
    return m_viewStartMs + static_cast<qint64>((m_viewEndMs - m_viewStartMs) * ratio);
}

int TimeSeriesChartWidget::xAtTime(qint64 ts, const QRect &plot) const
{
    const qint64 span = qMax<qint64>(1, m_viewEndMs - m_viewStartMs);
    const qreal ratio = qBound(0.0, static_cast<qreal>(ts - m_viewStartMs) / span, 1.0);
    return plot.left() + static_cast<int>(plot.width() * ratio);
}

int TimeSeriesChartWidget::yAtValue(double value, const PlotRect &item) const
{
    const double span = item.maxValue - item.minValue;
    if (span <= 0.0) {
        return item.plot.center().y();
    }
    const qreal ratio = qBound(0.0, (item.maxValue - value) / span, 1.0);
    return item.plot.top() + static_cast<int>(item.plot.height() * ratio);
}

void TimeSeriesChartWidget::valueRange(TimeSeriesChannel channel, double *minValue, double *maxValue) const
{
    double minV = 0.0;
    double maxV = 1.0;
    bool first = true;
    auto consider = [&](const QVector<TimeSeriesPoint> &points) {
        for (const TimeSeriesPoint &point : points) {
            if (point.tsUtcMs < m_viewStartMs || point.tsUtcMs > m_viewEndMs) {
                continue;
            }
            if (first) {
                minV = maxV = point.value;
                first = false;
            } else {
                minV = qMin(minV, point.value);
                maxV = qMax(maxV, point.value);
            }
        }
    };
    const TimeSeriesSeries *series = seriesFor(channel);
    if (series) {
        consider(series->points);
    }
    if (first || qFuzzyCompare(minV, maxV)) {
        const double pad = qMax(qAbs(minV) * 0.05, 1.0);
        minV -= pad;
        maxV += pad;
    }
    if (minValue) {
        *minValue = minV;
    }
    if (maxValue) {
        *maxValue = maxV;
    }
}

const TimeSeriesSeries *TimeSeriesChartWidget::seriesFor(TimeSeriesChannel channel) const
{
    for (const TimeSeriesSeries &series : m_result.series) {
        if (series.channel == channel) {
            return &series;
        }
    }
    return nullptr;
}

qint64 TimeSeriesChartWidget::gapMs() const
{
    qint64 typicalMs = qMax(1, m_result.sampleIntervalMs);
    for (const TimeSeriesSeries &series : m_result.series) {
        if (series.points.size() < 8) {
            continue;
        }
        const qint64 spanMs = series.points.last().tsUtcMs - series.points.first().tsUtcMs;
        if (spanMs <= 0) {
            continue;
        }
        // Min/max downsample keeps about two points per time bucket, so the
        // typical adjacent step is closer to span/(n/2) than to 1 Hz.
        const int bucketCount = qMax(1, series.points.size() / 2);
        typicalMs = qMax(typicalMs, spanMs / bucketCount);
    }
    return qMax(3 * typicalMs, qint64(5000));
}

qint64 TimeSeriesChartWidget::dataSpanMs() const
{
    return qMax<qint64>(1, m_dataEndMs - m_dataStartMs);
}

qint64 TimeSeriesChartWidget::minimumSpan() const
{
    return kMinVisibleSpanMs;
}

bool TimeSeriesChartWidget::canPanZoom() const
{
    return m_hasPoints && dataSpanMs() > minimumSpan();
}

void TimeSeriesChartWidget::drawSeriesPath(QPainter &painter,
                                           const QVector<TimeSeriesPoint> &points,
                                           const PlotRect &item,
                                           const QColor &color,
                                           bool *drewAny) const
{
    if (points.isEmpty()) {
        return;
    }
    QPainterPath path;
    bool drawing = false;
    qint64 lastTs = 0;
    const qint64 gap = gapMs();
    painter.setPen(QPen(color, 2));
    for (const TimeSeriesPoint &point : points) {
        if (point.tsUtcMs < m_viewStartMs || point.tsUtcMs > m_viewEndMs) {
            drawing = false;
            continue;
        }
        const QPointF pos(xAtTime(point.tsUtcMs, item.plot),
                          yAtValue(point.value, item));
        if (!drawing || (point.tsUtcMs - lastTs) > gap) {
            path.moveTo(pos);
            drawing = true;
        } else {
            path.lineTo(pos);
        }
        lastTs = point.tsUtcMs;
        if (drewAny) {
            *drewAny = true;
        }
    }
    painter.drawPath(path);
}

bool TimeSeriesChartWidget::queryExtrema(const TimeSeriesSeries &series,
                                         TimeSeriesPoint *minPoint,
                                         TimeSeriesPoint *maxPoint) const
{
    if (series.points.isEmpty()) {
        return false;
    }
    TimeSeriesPoint minP = series.points.first();
    TimeSeriesPoint maxP = series.points.first();
    for (const TimeSeriesPoint &point : series.points) {
        if (point.value < minP.value
            || (qFuzzyCompare(point.value, minP.value) && point.tsUtcMs < minP.tsUtcMs)) {
            minP = point;
        }
        if (point.value > maxP.value
            || (qFuzzyCompare(point.value, maxP.value) && point.tsUtcMs < maxP.tsUtcMs)) {
            maxP = point;
        }
    }
    if (minPoint) {
        *minPoint = minP;
    }
    if (maxPoint) {
        *maxPoint = maxP;
    }
    return true;
}

void TimeSeriesChartWidget::drawExtrema(QPainter &painter,
                                        const TimeSeriesSeries &series,
                                        const PlotRect &item,
                                        const QColor &color) const
{
    TimeSeriesPoint minPoint;
    TimeSeriesPoint maxPoint;
    if (!queryExtrema(series, &minPoint, &maxPoint)) {
        return;
    }

    painter.save();
    QFont labelFont = painter.font();
    labelFont.setPixelSize(14);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    const QFontMetrics metrics(labelFont);

    auto drawMark = [&](const TimeSeriesPoint &point, const QString &label, bool placeAbove) {
        if (point.tsUtcMs < m_viewStartMs || point.tsUtcMs > m_viewEndMs) {
            return;
        }
        const int x = xAtTime(point.tsUtcMs, item.plot);
        const int y = yAtValue(point.value, item);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 160), 1, Qt::DashLine));
        painter.drawLine(item.plot.left(), y, item.plot.right(), y);

        const int radius = 7;
        QPolygon diamond;
        diamond << QPoint(x, y - radius)
                << QPoint(x + radius, y)
                << QPoint(x, y + radius)
                << QPoint(x - radius, y);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(color);
        painter.drawPolygon(diamond);

        const QString text = QStringLiteral("%1 %2")
                                 .arg(label, formatValue(series.channel, point.value));
        QRect textRect = metrics.boundingRect(text);
        textRect.setSize(textRect.size() + QSize(10, 6));
        int tx = x + 12;
        int ty = placeAbove ? (y - textRect.height() - 8) : (y + 8);
        if (tx + textRect.width() > item.plot.right() - 4) {
            tx = x - 12 - textRect.width();
        }
        if (tx < item.plot.left() + 4) {
            tx = item.plot.left() + 4;
        }
        if (ty < item.plot.top() + 2) {
            ty = y + 8;
        }
        if (ty + textRect.height() > item.plot.bottom() - 2) {
            ty = y - textRect.height() - 8;
        }
        ty = qBound(item.plot.top() + 2, ty, item.plot.bottom() - textRect.height() - 2);
        textRect.moveTo(tx, ty);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 18, 28, 210));
        painter.drawRoundedRect(textRect, 4, 4);
        painter.setPen(color);
        painter.drawText(textRect, Qt::AlignCenter, text);
    };

    const bool samePoint = (minPoint.tsUtcMs == maxPoint.tsUtcMs
                            && qFuzzyCompare(minPoint.value, maxPoint.value));
    if (samePoint) {
        drawMark(maxPoint, QStringLiteral("最小/最大"), true);
    } else {
        drawMark(maxPoint, QStringLiteral("最大"), true);
        drawMark(minPoint, QStringLiteral("最小"), false);
    }
    painter.restore();
}

bool TimeSeriesChartWidget::isSynthesizedMouse(const QMouseEvent *event) const
{
    return event && event->source() != Qt::MouseEventNotSynthesized;
}
