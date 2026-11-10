#include "timeserieschartwidget.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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

QString formatValue(TimeSeriesChannel channel, double value)
{
    return QString::number(value, 'f', TimeSeriesMeta::displayDecimals(channel));
}

} // namespace

TimeSeriesChartWidget::TimeSeriesChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_emptyText(QStringLiteral("没有可显示的曲线数据"))
{
    setMouseTracking(true);
    setMinimumSize(640, 360);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(8, 18, 28));
    setPalette(pal);
}

void TimeSeriesChartWidget::setSeries(const TimeSeriesQueryResult &result)
{
    m_result = result;
    m_dataStartMs = result.startUtcMs;
    m_dataEndMs = qMax(result.endUtcMs, result.startUtcMs + 1);
    bool hasPoints = false;
    for (const TimeSeriesSeries &series : result.series) {
        if (series.points.isEmpty()) {
            continue;
        }
        if (!hasPoints) {
            m_dataStartMs = series.points.first().tsUtcMs;
            m_dataEndMs = series.points.last().tsUtcMs;
            hasPoints = true;
        } else {
            m_dataStartMs = qMin(m_dataStartMs, series.points.first().tsUtcMs);
            m_dataEndMs = qMax(m_dataEndMs, series.points.last().tsUtcMs);
        }
    }
    if (m_dataEndMs <= m_dataStartMs) {
        m_dataEndMs = m_dataStartMs + 1;
    }
    resetView();
    if (!hasPoints) {
        m_emptyText = QStringLiteral("该时间范围内没有采样数据");
    }
    update();
}

void TimeSeriesChartWidget::clearSeries()
{
    m_result = TimeSeriesQueryResult();
    m_emptyText = QStringLiteral("没有可显示的曲线数据");
    resetView();
    update();
}

void TimeSeriesChartWidget::resetView()
{
    m_viewStartMs = m_dataStartMs;
    m_viewEndMs = m_dataEndMs;
    update();
}

void TimeSeriesChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(8, 18, 28));

    const QVector<PlotRect> plots = layoutPlots(rect().adjusted(8, 8, -8, -8));
    if (plots.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#9ad7ea")));
        painter.drawText(rect(), Qt::AlignCenter, m_emptyText);
        return;
    }

    painter.setPen(QColor(0, 200, 255, 80));
    for (int i = 0; i < plots.size(); ++i) {
        const PlotRect &item = plots.at(i);
        painter.setBrush(QColor(6, 22, 34, 180));
        painter.drawRect(item.plot);

        painter.setPen(QColor(QStringLiteral("#eaf9ff")));
        const QString title = QStringLiteral("%1  %2    %3 ~ %4 %2")
                                  .arg(TimeSeriesMeta::displayName(item.channel),
                                       TimeSeriesMeta::unit(item.channel),
                                       formatValue(item.channel, item.minValue),
                                       formatValue(item.channel, item.maxValue));
        painter.drawText(item.plot.adjusted(8, 4, -8, 0), Qt::AlignLeft | Qt::AlignTop, title);

        const TimeSeriesSeries *series = seriesFor(item.channel);
        if (series && !series->points.isEmpty()) {
            QPainterPath path;
            bool drawing = false;
            qint64 lastTs = 0;
            const qint64 gap = gapMs();
            const QColor color = kPlotColors[i % 4];
            painter.setPen(QPen(color, 2));
            for (const TimeSeriesPoint &point : series->points) {
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
            }
            painter.drawPath(path);
        }

        painter.setPen(QColor(154, 215, 234, 160));
        painter.drawText(item.plot.adjusted(-64, 12, 0, -12),
                         Qt::AlignLeft | Qt::AlignTop,
                         formatValue(item.channel, item.maxValue));
        painter.drawText(item.plot.adjusted(-64, 12, 0, -12),
                         Qt::AlignLeft | Qt::AlignBottom,
                         formatValue(item.channel, item.minValue));
    }

    const PlotRect &bottom = plots.last();
    painter.setPen(QColor(QStringLiteral("#9ad7ea")));
    const QString startText = QDateTime::fromMSecsSinceEpoch(m_viewStartMs, Qt::LocalTime)
                                  .toString(QStringLiteral("MM-dd hh:mm:ss"));
    const QString endText = QDateTime::fromMSecsSinceEpoch(m_viewEndMs, Qt::LocalTime)
                                .toString(QStringLiteral("MM-dd hh:mm:ss"));
    painter.drawText(bottom.plot.adjusted(0, 0, 0, 22), Qt::AlignLeft | Qt::AlignBottom, startText);
    painter.drawText(bottom.plot.adjusted(0, 0, 0, 22), Qt::AlignRight | Qt::AlignBottom, endText);

    if (m_hasCursor && plots.first().plot.contains(QPoint(m_cursorPos.x(), plots.first().plot.center().y()))) {
        const qint64 cursorTs = timeAtX(m_cursorPos.x(), plots.first().plot);
        painter.setPen(QPen(QColor(255, 255, 255, 140), 1, Qt::DashLine));
        painter.drawLine(QPoint(m_cursorPos.x(), plots.first().plot.top()),
                         QPoint(m_cursorPos.x(), plots.last().plot.bottom()));

        QStringList lines;
        lines << QDateTime::fromMSecsSinceEpoch(cursorTs, Qt::LocalTime)
                     .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        for (const PlotRect &item : plots) {
            const TimeSeriesSeries *series = seriesFor(item.channel);
            if (!series || series->points.isEmpty()) {
                continue;
            }
            int nearest = 0;
            qint64 best = std::numeric_limits<qint64>::max();
            for (int i = 0; i < series->points.size(); ++i) {
                const qint64 delta = qAbs(series->points.at(i).tsUtcMs - cursorTs);
                if (delta < best) {
                    best = delta;
                    nearest = i;
                }
            }
            const TimeSeriesPoint &point = series->points.at(nearest);
            lines << QStringLiteral("%1 %2 %3")
                         .arg(TimeSeriesMeta::displayName(item.channel),
                              formatValue(item.channel, point.value),
                              TimeSeriesMeta::unit(item.channel));
        }
        const QString text = lines.join(QLatin1Char('\n'));
        QRect box(m_cursorPos.x() + 12, m_cursorPos.y() + 12, 280, 20 + lines.size() * 18);
        if (box.right() > width() - 8) {
            box.moveRight(m_cursorPos.x() - 12);
        }
        if (box.bottom() > height() - 8) {
            box.moveBottom(m_cursorPos.y() - 12);
        }
        painter.fillRect(box, QColor(4, 16, 24, 220));
        painter.setPen(QColor(QStringLiteral("#eaf9ff")));
        painter.drawRect(box);
        painter.drawText(box.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, text);
    }
}

void TimeSeriesChartWidget::wheelEvent(QWheelEvent *event)
{
    if (m_result.series.isEmpty()) {
        return;
    }
    const qreal factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    const qint64 span = qMax<qint64>(1000, m_viewEndMs - m_viewStartMs);
    const qint64 cursorTs = timeAtX(event->position().toPoint().x(), rect().adjusted(72, 8, -16, -28));
    const qint64 newSpan = qBound<qint64>(1000, static_cast<qint64>(span * factor),
                                          qMax<qint64>(1000, m_dataEndMs - m_dataStartMs));
    const qreal ratio = qBound(0.0, static_cast<qreal>(cursorTs - m_viewStartMs) / span, 1.0);
    m_viewStartMs = cursorTs - static_cast<qint64>(newSpan * ratio);
    m_viewEndMs = m_viewStartMs + newSpan;
    if (m_viewStartMs < m_dataStartMs) {
        m_viewStartMs = m_dataStartMs;
        m_viewEndMs = m_viewStartMs + newSpan;
    }
    if (m_viewEndMs > m_dataEndMs) {
        m_viewEndMs = m_dataEndMs;
        m_viewStartMs = m_viewEndMs - newSpan;
        if (m_viewStartMs < m_dataStartMs) {
            m_viewStartMs = m_dataStartMs;
        }
    }
    update();
}

void TimeSeriesChartWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
    }
}

void TimeSeriesChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_cursorPos = event->pos();
    m_hasCursor = true;
    if (m_dragging) {
        const QRect plot = rect().adjusted(72, 8, -16, -28);
        if (plot.width() > 0) {
            const qint64 span = qMax<qint64>(1, m_viewEndMs - m_viewStartMs);
            const qint64 delta = (m_lastMousePos.x() - event->pos().x()) * span / plot.width();
            m_viewStartMs += delta;
            m_viewEndMs += delta;
            if (m_viewStartMs < m_dataStartMs) {
                m_viewEndMs += m_dataStartMs - m_viewStartMs;
                m_viewStartMs = m_dataStartMs;
            }
            if (m_viewEndMs > m_dataEndMs) {
                m_viewStartMs -= m_viewEndMs - m_dataEndMs;
                m_viewEndMs = m_dataEndMs;
                if (m_viewStartMs < m_dataStartMs) {
                    m_viewStartMs = m_dataStartMs;
                }
            }
            m_lastMousePos = event->pos();
        }
    }
    update();
}

void TimeSeriesChartWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void TimeSeriesChartWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    resetView();
}

void TimeSeriesChartWidget::leaveEvent(QEvent *)
{
    m_hasCursor = false;
    update();
}

QVector<TimeSeriesChartWidget::PlotRect> TimeSeriesChartWidget::layoutPlots(const QRect &area) const
{
    QVector<PlotRect> plots;
    int visible = 0;
    for (const TimeSeriesSeries &series : m_result.series) {
        Q_UNUSED(series);
        ++visible;
    }
    if (visible <= 0) {
        return plots;
    }
    const int count = m_result.series.size();
    const int plotHeight = qMax(80, (area.height() - 24) / count);
    for (int i = 0; i < count; ++i) {
        PlotRect item;
        item.channel = m_result.series.at(i).channel;
        item.plot = QRect(area.left() + 64,
                          area.top() + i * plotHeight,
                          area.width() - 80,
                          plotHeight - 10);
        valueRange(item.channel, &item.minValue, &item.maxValue);
        plots.append(item);
    }
    return plots;
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
    return item.plot.top() + 22 + static_cast<int>((item.plot.height() - 28) * ratio);
}

void TimeSeriesChartWidget::valueRange(TimeSeriesChannel channel, double *minValue, double *maxValue) const
{
    double minV = 0.0;
    double maxV = 1.0;
    bool first = true;
    const TimeSeriesSeries *series = seriesFor(channel);
    if (series) {
        for (const TimeSeriesPoint &point : series->points) {
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
    return qMax(3 * m_result.sampleIntervalMs, 5000);
}
