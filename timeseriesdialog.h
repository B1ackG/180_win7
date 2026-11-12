#ifndef TIMESERIESDIALOG_H
#define TIMESERIESDIALOG_H

#include "timeserieschartwidget.h"
#include "timeseriestypes.h"

#include <QDate>
#include <QDateTime>
#include <QFutureWatcher>
#include <QMap>
#include <QVector>
#include <QWidget>

class QDateEdit;
class QHideEvent;
class QLabel;
class QPushButton;
class TimeSeriesDataZoom;
class TimeSeriesFullscreenDialog;

class TimeSeriesDialog : public QWidget
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
    bool isRecentDrawerExpanded() const { return m_recentDrawerExpanded; }
    QPushButton *channelButton(TimeSeriesChannel channel) const;
    TimeSeriesChartWidget *chart() const { return m_chart; }
    TimeSeriesDataZoom *dataZoom() const { return m_dataZoom; }

protected:
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onDateChanged(const QDate &date);
    void onQueryFinished();
    void onConditionFinished();
    void onConditionHitSelected(qint64 tsUtcMs);
    void onChannelButtonToggled(bool checked);
    void onToggleChannelDrawer();
    void onToggleRecentDrawer();
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
    void startDateQuery();
    void startQuickQuery(int seconds);
    void repeatLastQuery();
    void startConditionSearch();
    void openDateCalendar();
    void setBusy(bool busy);
    void setStatus(const QString &text, bool error = false);
    void updateChannelButtonStates();
    void setChannelDrawerExpanded(bool expanded);
    void setRecentDrawerExpanded(bool expanded);
    void syncDataZoomFromChart();
    void applyDataZoom(int lower, int upper);
    void closeFullscreen();
    TimeSeriesQueryResult seriesForChannel(TimeSeriesChannel channel) const;

    QString m_directory;
    QDateEdit *m_dateEdit = nullptr;
    QPushButton *m_recentButton = nullptr;
    QPushButton *m_last30MinutesButton = nullptr;
    QPushButton *m_lastHourButton = nullptr;
    QPushButton *m_last4HoursButton = nullptr;
    QPushButton *m_conditionButton = nullptr;
    QMap<TimeSeriesChannel, QPushButton*> m_channelButtons;
    QWidget *m_recentDrawerBody = nullptr;
    QWidget *m_channelDrawerBody = nullptr;
    QPushButton *m_channelDrawerButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    TimeSeriesChartWidget *m_chart = nullptr;
    TimeSeriesDataZoom *m_dataZoom = nullptr;
    TimeSeriesFullscreenDialog *m_fullscreen = nullptr;
    QFutureWatcher<TimeSeriesQueryResult> *m_watcher = nullptr;
    QFutureWatcher<TimeSeriesConditionSearchResult> *m_conditionWatcher = nullptr;
    TimeSeriesQueryResult m_lastResult;
    QDateTime m_lastQueryStart;
    QDateTime m_lastQueryEnd;
    int m_lastQuickSeconds = 0;
    qint64 m_pendingCursorMs = -1;
    bool m_channelDrawerExpanded = false;
    bool m_recentDrawerExpanded = false;
    bool m_busy = false;
    int m_queryToken = 0;
    int m_activeQueryToken = 0;
};

#endif // TIMESERIESDIALOG_H
