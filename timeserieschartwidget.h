#ifndef TIMESERIESCHARTWIDGET_H
#define TIMESERIESCHARTWIDGET_H

#include "timeseriestypes.h"

#include <QPair>
#include <QPoint>
#include <QWidget>

class QColor;
class QGestureEvent;
class QPainter;
class QPinchGesture;
class QTouchEvent;

class TimeSeriesChartWidget : public QWidget
{
    Q_OBJECT

public:
    enum class InteractionMode {
        Cursor,
        Pan
    };

    explicit TimeSeriesChartWidget(QWidget *parent = nullptr);

    void setSeries(const TimeSeriesQueryResult &result);
    void setCompareResult(const TimeSeriesCompareResult &result);
    void clearSeries();
    void resetView();
    bool isCompareMode() const { return m_compareMode; }
    TimeSeriesCompareResult compareResult() const { return m_compare; }

    void setInteractionMode(InteractionMode mode);
    InteractionMode interactionMode() const { return m_mode; }

    void setCursorTime(qint64 utcMs, bool visible = true);
    qint64 cursorTime() const { return m_cursorTimeMs; }
    bool isCursorVisible() const { return m_cursorVisible; }
    void clearCursor();

    void setVisibleRange(qint64 startUtcMs, qint64 endUtcMs);
    void zoomAround(qint64 anchorUtcMs, qreal scaleFactor);
    QPair<qint64, qint64> visibleRange() const;
    qint64 dataStartMs() const { return m_dataStartMs; }
    qint64 dataEndMs() const { return m_dataEndMs; }
    bool hasPlotData() const { return m_hasPoints; }
    bool canPanZoom() const;

    void setFullscreenButtonsVisible(bool visible);
    QRect fullscreenButtonRectFor(TimeSeriesChannel channel) const;
    QRect plotRectFor(TimeSeriesChannel channel) const;
    QString cursorValueText(TimeSeriesChannel channel) const;
    QString cursorReadoutText() const;

signals:
    void cursorTimeChanged(qint64 utcMs);
    void visibleRangeChanged(qint64 startUtcMs, qint64 endUtcMs);
    void fullscreenRequested(TimeSeriesChannel channel);

protected:
    bool event(QEvent *event) override;
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
        QRect fullscreenButtonRect;
        TimeSeriesChannel channel = TimeSeriesChannel::J1AngleDeg;
        double minValue = 0.0;
        double maxValue = 1.0;
        bool hasVisiblePoints = false;
    };

    QVector<PlotRect> layoutPlots(const QRect &area) const;
    QRect contentRect() const;
    qint64 timeAtX(int x, const QRect &plot) const;
    int xAtTime(qint64 ts, const QRect &plot) const;
    int yAtValue(double value, const PlotRect &item) const;
    void valueRange(TimeSeriesChannel channel, double *minValue, double *maxValue) const;
    const TimeSeriesSeries *seriesFor(TimeSeriesChannel channel) const;
    const TimeSeriesCompareChannel *compareChannel(TimeSeriesChannel channel) const;
    qint64 gapMs() const;
    qint64 dataSpanMs() const;
    qint64 minimumSpan() const;
    void drawSeriesPath(QPainter &painter,
                        const QVector<TimeSeriesPoint> &points,
                        const PlotRect &item,
                        const QColor &color,
                        bool *drewAny) const;
    bool isSynthesizedMouse(const QMouseEvent *event) const;
    bool gestureEvent(QGestureEvent *event);
    bool handleTouchEvent(QTouchEvent *event);
    bool handleFullscreenHit(const QPoint &pos);
    bool plotAreaContains(const QPoint &pos) const;
    void beginPointer(const QPoint &pos);
    void updatePointer(const QPoint &pos);
    void endPointer();
    void panByPixels(int dx, const QRect &plot);
    void drawGrid(QPainter &painter, const PlotRect &plot) const;
    QString formatTimeTick(qint64 utcMs) const;
    QString formatValue(TimeSeriesChannel channel, double value) const;
    void applyVisibleRange(qint64 startUtcMs, qint64 endUtcMs, bool emitChange);
    QRect readoutRect() const;

    TimeSeriesQueryResult m_result;
    TimeSeriesCompareResult m_compare;
    bool m_compareMode = false;
    qint64 m_viewStartMs = 0;
    qint64 m_viewEndMs = 1;
    qint64 m_dataStartMs = 0;
    qint64 m_dataEndMs = 1;
    qint64 m_cursorTimeMs = 0;
    bool m_cursorVisible = false;
    bool m_hasPoints = false;
    bool m_fullscreenButtonsVisible = true;
    bool m_pointerActive = false;
    bool m_pressInsidePlot = false;
    bool m_touchActive = false;
    InteractionMode m_mode = InteractionMode::Cursor;
    QPoint m_lastPointerPos;
    QString m_emptyText;
};

#endif // TIMESERIESCHARTWIDGET_H
