#ifndef TIMESERIESDIALOG_H
#define TIMESERIESDIALOG_H

#include "timeserieschartwidget.h"
#include "timeseriestypes.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QMap>
#include <QVector>

class QDateEdit;
class QLabel;
class QPushButton;
class TimeSeriesDataZoom;
class TimeSeriesFullscreenDialog;

class TimeSeriesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class QuickRange {
        Last30Minutes,
        LastHour,
        Last4Hours
    };

    explicit TimeSeriesDialog(const QString &databaseDirectory, QWidget *parent = nullptr);
    void reloadAvailableDates();

    QVector<TimeSeriesChannel> selectedChannels() const;
    void applyQuickRange(QuickRange range);
    bool isChannelDrawerExpanded() const { return m_channelDrawerExpanded; }
    QPushButton *channelButton(TimeSeriesChannel channel) const;
    TimeSeriesChartWidget *chart() const { return m_chart; }
    TimeSeriesDataZoom *dataZoom() const { return m_dataZoom; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onQueryClicked();
    void onResetClicked();
    void onQueryFinished();
    void onCompareFinished();
    void onConditionFinished();
    void onConditionHitSelected(qint64 tsUtcMs);
    void onChannelButtonToggled(bool checked);
    void onToggleChannelDrawer();
    void onFullscreenRequested(TimeSeriesChannel channel);
    void onFullscreenStateClosing(qint64 startUtcMs,
                                  qint64 endUtcMs,
                                  qint64 cursorTimeMs,
                                  bool cursorVisible,
                                  TimeSeriesChartWidget::InteractionMode mode);

private:
    bool validateRange(QString *error) const;
    QDateTime rangeStartDateTime() const;
    QDateTime rangeEndDateTime() const;
    void startQuery(const QDateTime &start, const QDateTime &end);
    void startQuickQuery(int seconds);
    void startCompare(TimeSeriesCompareKind kind);
    void startConditionSearch();
    void setCompareStat(TimeSeriesCompareStat stat);
    TimeSeriesCompareStat selectedCompareStat() const;
    void setBusy(bool busy);
    void setStatus(const QString &text, bool error = false);
    void updateChannelButtonStates();
    void setChannelDrawerExpanded(bool expanded);
    void setInteractionMode(TimeSeriesChartWidget::InteractionMode mode);
    void syncDataZoomFromChart();
    void applyDataZoom(int lower, int upper);
    void closeFullscreen();
    TimeSeriesQueryResult seriesForChannel(TimeSeriesChannel channel) const;
    TimeSeriesCompareResult compareForChannel(TimeSeriesChannel channel) const;

    QString m_directory;
    QDateEdit *m_startEdit = nullptr;
    QDateEdit *m_endEdit = nullptr;
    QPushButton *m_last30MinutesButton = nullptr;
    QPushButton *m_lastHourButton = nullptr;
    QPushButton *m_last4HoursButton = nullptr;
    QPushButton *m_avgCompareButton = nullptr;
    QPushButton *m_minCompareButton = nullptr;
    QPushButton *m_maxCompareButton = nullptr;
    QPushButton *m_monthCompareButton = nullptr;
    QPushButton *m_yearCompareButton = nullptr;
    QPushButton *m_conditionButton = nullptr;
    QMap<TimeSeriesChannel, QPushButton*> m_channelButtons;
    QWidget *m_channelDrawerBody = nullptr;
    QPushButton *m_channelDrawerButton = nullptr;
    QPushButton *m_cursorButton = nullptr;
    QPushButton *m_panButton = nullptr;
    QPushButton *m_queryButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    TimeSeriesChartWidget *m_chart = nullptr;
    TimeSeriesDataZoom *m_dataZoom = nullptr;
    TimeSeriesFullscreenDialog *m_fullscreen = nullptr;
    QFutureWatcher<TimeSeriesQueryResult> *m_watcher = nullptr;
    QFutureWatcher<TimeSeriesCompareResult> *m_compareWatcher = nullptr;
    QFutureWatcher<TimeSeriesConditionSearchResult> *m_conditionWatcher = nullptr;
    TimeSeriesQueryResult m_lastResult;
    TimeSeriesCompareResult m_lastCompare;
    qint64 m_pendingCursorMs = -1;
    TimeSeriesCompareStat m_compareStat = TimeSeriesCompareStat::Average;
    bool m_channelDrawerExpanded = false;
    bool m_busy = false;
    int m_queryToken = 0;
    int m_activeQueryToken = 0;
};

#endif // TIMESERIESDIALOG_H
