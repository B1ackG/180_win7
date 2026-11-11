#ifndef TIMESERIESFULLSCREENDIALOG_H
#define TIMESERIESFULLSCREENDIALOG_H

#include "timeserieschartwidget.h"
#include "timeseriestypes.h"

#include <QDialog>

class QLabel;
class QPushButton;
class TimeSeriesDataZoom;

class TimeSeriesFullscreenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimeSeriesFullscreenDialog(QWidget *parent = nullptr);

    void setContent(const TimeSeriesQueryResult &oneChannelResult,
                    qint64 viewStartMs,
                    qint64 viewEndMs,
                    qint64 cursorTimeMs,
                    bool cursorVisible,
                    TimeSeriesChartWidget::InteractionMode mode);
    void setCompareContent(const TimeSeriesCompareResult &oneChannelResult,
                           qint64 viewStartMs,
                           qint64 viewEndMs,
                           qint64 cursorTimeMs,
                           bool cursorVisible,
                           TimeSeriesChartWidget::InteractionMode mode);

    TimeSeriesChartWidget *chart() const { return m_chart; }
    qint64 viewStartMs() const;
    qint64 viewEndMs() const;
    qint64 cursorTimeMs() const;
    bool isCursorVisible() const;
    TimeSeriesChartWidget::InteractionMode interactionMode() const;

signals:
    void viewStateClosing(qint64 startUtcMs,
                          qint64 endUtcMs,
                          qint64 cursorTimeMs,
                          bool cursorVisible,
                          TimeSeriesChartWidget::InteractionMode mode);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setMode(TimeSeriesChartWidget::InteractionMode mode);
    void syncDataZoomFromChart();
    void applyDataZoom(int lower, int upper);
    void emitClosingState();

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_exitButton = nullptr;
    QPushButton *m_cursorButton = nullptr;
    QPushButton *m_panButton = nullptr;
    TimeSeriesChartWidget *m_chart = nullptr;
    TimeSeriesDataZoom *m_dataZoom = nullptr;
    bool m_closingEmitted = false;
};

#endif // TIMESERIESFULLSCREENDIALOG_H
