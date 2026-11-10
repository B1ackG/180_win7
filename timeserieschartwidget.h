#ifndef TIMESERIESCHARTWIDGET_H
#define TIMESERIESCHARTWIDGET_H

#include "timeseriestypes.h"

#include <QPoint>
#include <QWidget>

class TimeSeriesChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimeSeriesChartWidget(QWidget *parent = nullptr);

    void setSeries(const TimeSeriesQueryResult &result);
    void clearSeries();
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct PlotRect {
        QRect plot;
        TimeSeriesChannel channel = TimeSeriesChannel::J1AngleDeg;
        double minValue = 0.0;
        double maxValue = 1.0;
    };

    QVector<PlotRect> layoutPlots(const QRect &area) const;
    qint64 timeAtX(int x, const QRect &plot) const;
    int xAtTime(qint64 ts, const QRect &plot) const;
    int yAtValue(double value, const PlotRect &item) const;
    void valueRange(TimeSeriesChannel channel, double *minValue, double *maxValue) const;
    const TimeSeriesSeries *seriesFor(TimeSeriesChannel channel) const;
    qint64 gapMs() const;

    TimeSeriesQueryResult m_result;
    qint64 m_viewStartMs = 0;
    qint64 m_viewEndMs = 1;
    qint64 m_dataStartMs = 0;
    qint64 m_dataEndMs = 1;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    QPoint m_cursorPos;
    bool m_hasCursor = false;
    QString m_emptyText;
};

#endif // TIMESERIESCHARTWIDGET_H
