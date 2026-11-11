#include "timeseriesdialog.h"

#include "timeserieschartwidget.h"
#include "timeseriesdatazoom.h"
#include "timeseriesconditiondialog.h"
#include "timeseriesfullscreendialog.h"
#include "timeseriesqueryservice.h"

#include <QCloseEvent>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QtConcurrent>
#include <QVBoxLayout>

namespace {

const QSet<TimeSeriesChannel> kDefaultChannels = {
    TimeSeriesChannel::ArmTiltXDeg,
    TimeSeriesChannel::ArmTiltYDeg,
    TimeSeriesChannel::ChassisTiltXDeg,
    TimeSeriesChannel::ChassisTiltYDeg
};

} // namespace

TimeSeriesDialog::TimeSeriesDialog(const QString &databaseDirectory, QWidget *parent)
    : QDialog(parent)
    , m_directory(databaseDirectory)
{
    setWindowTitle(QStringLiteral("数据曲线"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(1480, 860);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #08121c; color: #eaf9ff; }"
        "QLabel { color: #eaf9ff; font-size: 16px; }"
        "QDateEdit { background: #102433; color: #eaf9ff; min-height: 36px; font-size: 16px; }"
        "QPushButton { min-height: 48px; font-size: 16px;"
        "  background: #12364a; color: #eaf9ff; border: 1px solid #00c8ff; }"
        "QPushButton:checked { background: #1a6d8c; color: #ffffff; font-weight: bold;"
        "  border: 2px solid #5CE1FF; }"
        "QPushButton:disabled { background: #1a1a1a; color: #6a7a82; border-color: #3a4a52; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *filterRow = new QHBoxLayout();
    auto *startLabel = new QLabel(QStringLiteral("开始"), this);
    auto *endLabel = new QLabel(QStringLiteral("结束"), this);
    m_startEdit = new QDateEdit(this);
    m_endEdit = new QDateEdit(this);
    m_startEdit->setObjectName(QStringLiteral("Dt_Start"));
    m_endEdit->setObjectName(QStringLiteral("Dt_End"));
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startEdit->setCalendarPopup(true);
    m_endEdit->setCalendarPopup(true);
    const QDate today = QDate::currentDate();
    m_endEdit->setDate(today);
    m_startEdit->setDate(today);

    m_last30MinutesButton = new QPushButton(QStringLiteral("最近30分钟"), this);
    m_lastHourButton = new QPushButton(QStringLiteral("最近1小时"), this);
    m_last4HoursButton = new QPushButton(QStringLiteral("最近4小时"), this);
    m_avgCompareButton = new QPushButton(QStringLiteral("日均"), this);
    m_minCompareButton = new QPushButton(QStringLiteral("日最小"), this);
    m_maxCompareButton = new QPushButton(QStringLiteral("日最大"), this);
    m_monthCompareButton = new QPushButton(QStringLiteral("上月对比"), this);
    m_yearCompareButton = new QPushButton(QStringLiteral("去年对比"), this);
    m_conditionButton = new QPushButton(QStringLiteral("工况反查"), this);
    m_last30MinutesButton->setObjectName(QStringLiteral("TBtn_QuickLast30Minutes"));
    m_lastHourButton->setObjectName(QStringLiteral("TBtn_QuickLastHour"));
    m_last4HoursButton->setObjectName(QStringLiteral("TBtn_QuickLast4Hours"));
    m_avgCompareButton->setObjectName(QStringLiteral("TBtn_CompareAvg"));
    m_minCompareButton->setObjectName(QStringLiteral("TBtn_CompareMin"));
    m_maxCompareButton->setObjectName(QStringLiteral("TBtn_CompareMax"));
    m_monthCompareButton->setObjectName(QStringLiteral("TBtn_MonthCompare"));
    m_yearCompareButton->setObjectName(QStringLiteral("TBtn_YearCompare"));
    m_conditionButton->setObjectName(QStringLiteral("TBtn_ConditionSearch"));
    m_last30MinutesButton->setMinimumSize(120, 48);
    m_lastHourButton->setMinimumSize(120, 48);
    m_last4HoursButton->setMinimumSize(120, 48);
    m_avgCompareButton->setMinimumSize(96, 48);
    m_minCompareButton->setMinimumSize(96, 48);
    m_maxCompareButton->setMinimumSize(96, 48);
    m_monthCompareButton->setMinimumSize(120, 48);
    m_yearCompareButton->setMinimumSize(120, 48);
    m_conditionButton->setMinimumSize(120, 48);
    m_avgCompareButton->setCheckable(true);
    m_minCompareButton->setCheckable(true);
    m_maxCompareButton->setCheckable(true);

    m_queryButton = new QPushButton(QStringLiteral("查询"), this);
    m_resetButton = new QPushButton(QStringLiteral("重置"), this);
    m_queryButton->setObjectName(QStringLiteral("TBtn_Query"));
    m_resetButton->setObjectName(QStringLiteral("TBtn_Reset"));

    filterRow->addWidget(startLabel);
    filterRow->addWidget(m_startEdit, 1);
    filterRow->addSpacing(8);
    filterRow->addWidget(endLabel);
    filterRow->addWidget(m_endEdit, 1);
    filterRow->addWidget(m_last30MinutesButton);
    filterRow->addWidget(m_lastHourButton);
    filterRow->addWidget(m_last4HoursButton);
    filterRow->addWidget(m_queryButton);
    filterRow->addWidget(m_resetButton);
    root->addLayout(filterRow);

    auto *channelRow = new QHBoxLayout();
    m_channelDrawerButton = new QPushButton(QStringLiteral("通道选择"), this);
    m_channelDrawerButton->setObjectName(QStringLiteral("TBtn_ChannelDrawer"));
    m_channelDrawerButton->setMinimumSize(120, 48);
    channelRow->addWidget(m_channelDrawerButton);
    channelRow->addSpacing(12);
    channelRow->addWidget(m_avgCompareButton);
    channelRow->addWidget(m_minCompareButton);
    channelRow->addWidget(m_maxCompareButton);
    channelRow->addSpacing(12);
    channelRow->addWidget(m_monthCompareButton);
    channelRow->addWidget(m_yearCompareButton);
    channelRow->addSpacing(12);
    channelRow->addWidget(m_conditionButton);
    channelRow->addStretch(1);
    root->addLayout(channelRow);

    m_channelDrawerBody = new QWidget(this);
    auto *channelLayout = new QGridLayout(m_channelDrawerBody);
    channelLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->setHorizontalSpacing(10);
    channelLayout->setVerticalSpacing(8);
    const QVector<TimeSeriesChannel> channels = TimeSeriesMeta::allChannels();
    for (int i = 0; i < channels.size(); ++i) {
        const TimeSeriesChannel channel = channels.at(i);
        auto *button = new QPushButton(QStringLiteral("%1 (%2)")
                                           .arg(TimeSeriesMeta::displayName(channel),
                                                TimeSeriesMeta::unit(channel)),
                                       m_channelDrawerBody);
        button->setCheckable(true);
        button->setMinimumHeight(52);
        button->setObjectName(QStringLiteral("TBtn_Channel_%1")
                                  .arg(TimeSeriesMeta::columnName(channel)));
        button->setChecked(kDefaultChannels.contains(channel));
        connect(button, &QPushButton::toggled, this, &TimeSeriesDialog::onChannelButtonToggled);
        m_channelButtons.insert(channel, button);
        channelLayout->addWidget(button, i / 4, i % 4);
    }
    m_channelDrawerBody->setVisible(false);
    root->addWidget(m_channelDrawerBody);

    auto *modeRow = new QHBoxLayout();
    m_cursorButton = new QPushButton(QStringLiteral("游标"), this);
    m_panButton = new QPushButton(QStringLiteral("平移"), this);
    m_cursorButton->setObjectName(QStringLiteral("TBtn_CursorMode"));
    m_panButton->setObjectName(QStringLiteral("TBtn_PanMode"));
    m_cursorButton->setCheckable(true);
    m_panButton->setCheckable(true);
    m_cursorButton->setChecked(true);
    m_cursorButton->setMinimumSize(120, 48);
    m_panButton->setMinimumSize(120, 48);
    modeRow->addWidget(m_cursorButton);
    modeRow->addWidget(m_panButton);
    modeRow->addStretch(1);
    root->addLayout(modeRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_chart = new TimeSeriesChartWidget(this);
    m_chart->setObjectName(QStringLiteral("TimeSeriesChart"));
    root->addWidget(m_chart, 1);

    m_dataZoom = new TimeSeriesDataZoom(this);
    m_dataZoom->setObjectName(QStringLiteral("TimeSeriesDataZoom"));
    m_dataZoom->setRange(0, 10000);
    m_dataZoom->setSelectedRange(0, 10000);
    m_dataZoom->setMinimumSpan(1);
    m_dataZoom->setEnabled(false);
    root->addWidget(m_dataZoom);

    qRegisterMetaType<TimeSeriesQueryResult>("TimeSeriesQueryResult");
    qRegisterMetaType<TimeSeriesCompareResult>("TimeSeriesCompareResult");
    qRegisterMetaType<TimeSeriesConditionSearchResult>("TimeSeriesConditionSearchResult");
    m_watcher = new QFutureWatcher<TimeSeriesQueryResult>(this);
    m_compareWatcher = new QFutureWatcher<TimeSeriesCompareResult>(this);
    m_conditionWatcher = new QFutureWatcher<TimeSeriesConditionSearchResult>(this);
    connect(m_watcher, &QFutureWatcher<TimeSeriesQueryResult>::finished,
            this, &TimeSeriesDialog::onQueryFinished);
    connect(m_compareWatcher, &QFutureWatcher<TimeSeriesCompareResult>::finished,
            this, &TimeSeriesDialog::onCompareFinished);
    connect(m_conditionWatcher, &QFutureWatcher<TimeSeriesConditionSearchResult>::finished,
            this, &TimeSeriesDialog::onConditionFinished);
    connect(m_queryButton, &QPushButton::clicked, this, &TimeSeriesDialog::onQueryClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &TimeSeriesDialog::onResetClicked);
    connect(m_last30MinutesButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(30 * 60);
    });
    connect(m_lastHourButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(3600);
    });
    connect(m_last4HoursButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(4 * 3600);
    });
    connect(m_avgCompareButton, &QPushButton::clicked, this, [this]() {
        setCompareStat(TimeSeriesCompareStat::Average);
    });
    connect(m_minCompareButton, &QPushButton::clicked, this, [this]() {
        setCompareStat(TimeSeriesCompareStat::Minimum);
    });
    connect(m_maxCompareButton, &QPushButton::clicked, this, [this]() {
        setCompareStat(TimeSeriesCompareStat::Maximum);
    });
    connect(m_monthCompareButton, &QPushButton::clicked, this, [this]() {
        startCompare(TimeSeriesCompareKind::MonthOverMonth);
    });
    connect(m_yearCompareButton, &QPushButton::clicked, this, [this]() {
        startCompare(TimeSeriesCompareKind::YearOverYear);
    });
    connect(m_conditionButton, &QPushButton::clicked, this, &TimeSeriesDialog::startConditionSearch);
    connect(m_channelDrawerButton, &QPushButton::clicked,
            this, &TimeSeriesDialog::onToggleChannelDrawer);
    connect(m_cursorButton, &QPushButton::clicked, this, [this]() {
        setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    });
    connect(m_panButton, &QPushButton::clicked, this, [this]() {
        setInteractionMode(TimeSeriesChartWidget::InteractionMode::Pan);
    });
    connect(m_chart, &TimeSeriesChartWidget::fullscreenRequested,
            this, &TimeSeriesDialog::onFullscreenRequested);
    connect(m_chart, &TimeSeriesChartWidget::visibleRangeChanged,
            this, [this](qint64, qint64) { syncDataZoomFromChart(); });
    connect(m_dataZoom, &TimeSeriesDataZoom::selectedRangeChanged,
            this, &TimeSeriesDialog::applyDataZoom);

    updateChannelButtonStates();
    setCompareStat(TimeSeriesCompareStat::Average);
    setStatus(QStringLiteral("选择时间和通道后点击查询"));
}

void TimeSeriesDialog::reloadAvailableDates()
{
    const QList<QDate> dates = TimeSeriesQueryService::availableDates(m_directory);
    if (dates.isEmpty()) {
        setStatus(QStringLiteral("还没有保存的时序数据"));
        return;
    }
    setStatus(QStringLiteral("已保存 %1 天数据，最近一天 %2")
                  .arg(dates.size())
                  .arg(dates.last().toString(QStringLiteral("yyyy-MM-dd"))));
}

QVector<TimeSeriesChannel> TimeSeriesDialog::selectedChannels() const
{
    QVector<TimeSeriesChannel> selected;
    const QVector<TimeSeriesChannel> channels = TimeSeriesMeta::allChannels();
    for (TimeSeriesChannel channel : channels) {
        QPushButton *button = m_channelButtons.value(channel, nullptr);
        if (button && button->isChecked()) {
            selected.append(channel);
        }
    }
    if (selected.size() > 4) {
        selected = selected.mid(0, 4);
    }
    return selected;
}

void TimeSeriesDialog::applyQuickRange(QuickRange range)
{
    const QDateTime now = QDateTime::currentDateTime();
    m_endEdit->setDate(now.date());
    int seconds = 3600;
    if (range == QuickRange::Last30Minutes) {
        seconds = 30 * 60;
    } else if (range == QuickRange::Last4Hours) {
        seconds = 4 * 3600;
    }
    m_startEdit->setDate(now.addSecs(-seconds).date());
}

QPushButton *TimeSeriesDialog::channelButton(TimeSeriesChannel channel) const
{
    return m_channelButtons.value(channel, nullptr);
}

void TimeSeriesDialog::closeEvent(QCloseEvent *event)
{
    closeFullscreen();
    QDialog::closeEvent(event);
}

void TimeSeriesDialog::onQueryClicked()
{
    QString error;
    if (!validateRange(&error)) {
        setStatus(error, true);
        return;
    }
    startQuery(rangeStartDateTime(), rangeEndDateTime());
}

void TimeSeriesDialog::startQuickQuery(int seconds)
{
    if (seconds == 30 * 60) {
        applyQuickRange(QuickRange::Last30Minutes);
    } else if (seconds == 4 * 3600) {
        applyQuickRange(QuickRange::Last4Hours);
    } else {
        applyQuickRange(QuickRange::LastHour);
    }
    const QDateTime now = QDateTime::currentDateTime();
    startQuery(now.addSecs(-seconds), now);
}

void TimeSeriesDialog::setCompareStat(TimeSeriesCompareStat stat)
{
    m_compareStat = stat;
    m_avgCompareButton->setChecked(stat == TimeSeriesCompareStat::Average);
    m_minCompareButton->setChecked(stat == TimeSeriesCompareStat::Minimum);
    m_maxCompareButton->setChecked(stat == TimeSeriesCompareStat::Maximum);
}

TimeSeriesCompareStat TimeSeriesDialog::selectedCompareStat() const
{
    return m_compareStat;
}

void TimeSeriesDialog::startCompare(TimeSeriesCompareKind kind)
{
    const QVector<TimeSeriesChannel> channels = selectedChannels();
    if (channels.isEmpty()) {
        setStatus(QStringLiteral("请至少选择一个通道"), true);
        return;
    }
    closeFullscreen();
    setChannelDrawerExpanded(false);
    const QString directory = m_directory;
    const TimeSeriesCompareStat stat = selectedCompareStat();
    const QString statLabel = TimeSeriesMeta::compareStatLabel(stat);
    m_activeQueryToken = ++m_queryToken;
    setBusy(true);
    setStatus(kind == TimeSeriesCompareKind::MonthOverMonth
                  ? QStringLiteral("正在对比上月%1…").arg(statLabel)
                  : QStringLiteral("正在对比去年%1…").arg(statLabel));
    m_compareWatcher->setFuture(QtConcurrent::run([directory, channels, kind, stat]() {
        return TimeSeriesQueryService::queryDailyCompare(directory, channels, kind, QDate(), stat);
    }));
}

void TimeSeriesDialog::startConditionSearch()
{
    if (m_chart->isCompareMode() || !m_lastResult.ok || !m_chart->isCursorVisible()) {
        setStatus(QStringLiteral("请先查询1Hz曲线，在图上点游标后再工况反查"), true);
        return;
    }
    closeFullscreen();
    setChannelDrawerExpanded(false);
    const QString directory = m_directory;
    const qint64 probeTsUtcMs = m_chart->cursorTime();
    m_activeQueryToken = ++m_queryToken;
    setBusy(true);
    setStatus(QStringLiteral("正在按作业条件反查近90天状态…"));
    m_conditionWatcher->setFuture(QtConcurrent::run([directory, probeTsUtcMs]() {
        return TimeSeriesQueryService::searchSimilarConditions(directory, probeTsUtcMs);
    }));
}

void TimeSeriesDialog::startQuery(const QDateTime &start, const QDateTime &end)
{
    const QVector<TimeSeriesChannel> channels = selectedChannels();
    if (channels.isEmpty()) {
        setStatus(QStringLiteral("请至少选择一个通道"), true);
        return;
    }

    closeFullscreen();
    setChannelDrawerExpanded(false);

    const qint64 startUtcMs = start.toUTC().toMSecsSinceEpoch();
    const qint64 endUtcMs = end.toUTC().toMSecsSinceEpoch();
    const QString directory = m_directory;
    m_activeQueryToken = ++m_queryToken;
    setBusy(true);
    setStatus(QStringLiteral("正在查询…"));
    m_watcher->setFuture(QtConcurrent::run([directory, startUtcMs, endUtcMs, channels]() {
        return TimeSeriesQueryService::query(directory, startUtcMs, endUtcMs, channels);
    }));
}

void TimeSeriesDialog::onResetClicked()
{
    applyQuickRange(QuickRange::Last30Minutes);
    m_chart->resetView();
}

void TimeSeriesDialog::onQueryFinished()
{
    setBusy(false);
    if (!m_watcher) {
        return;
    }
    const TimeSeriesQueryResult result = m_watcher->result();
    m_lastResult = result;
    m_lastCompare = TimeSeriesCompareResult();
    if (!result.ok) {
        m_chart->clearSeries();
        m_lastResult = TimeSeriesQueryResult();
        m_dataZoom->setEnabled(false);
        {
            const QSignalBlocker blocker(m_dataZoom);
            m_dataZoom->setSelectedRange(0, 10000);
        }
        setStatus(result.error.isEmpty() ? QStringLiteral("查询失败") : result.error, true);
        return;
    }

    bool hasPoints = false;
    for (const TimeSeriesSeries &series : result.series) {
        if (!series.points.isEmpty()) {
            hasPoints = true;
            break;
        }
    }
    m_chart->setSeries(result);
    setChannelDrawerExpanded(false);
    if (!hasPoints) {
        m_dataZoom->setEnabled(false);
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        setStatus(QStringLiteral("该时间范围内没有采样数据"));
        m_pendingCursorMs = -1;
    } else {
        syncDataZoomFromChart();
        if (m_pendingCursorMs >= 0) {
            m_chart->setCursorTime(m_pendingCursorMs, true);
            m_pendingCursorMs = -1;
            setStatus(QStringLiteral("已定位到该工况时刻"));
        } else {
            setStatus(QStringLiteral("查询完成"));
        }
    }
}

void TimeSeriesDialog::onCompareFinished()
{
    setBusy(false);
    if (!m_compareWatcher) {
        return;
    }
    const TimeSeriesCompareResult result = m_compareWatcher->result();
    m_lastCompare = result;
    m_lastResult = TimeSeriesQueryResult();
    if (!result.ok) {
        m_chart->clearSeries();
        m_lastCompare = TimeSeriesCompareResult();
        m_dataZoom->setEnabled(false);
        {
            const QSignalBlocker blocker(m_dataZoom);
            m_dataZoom->setSelectedRange(0, 10000);
        }
        setStatus(result.error.isEmpty() ? QStringLiteral("对比失败") : result.error, true);
        return;
    }

    bool hasPoints = false;
    for (const TimeSeriesCompareChannel &item : result.series) {
        if (!item.currentPoints.isEmpty() || !item.previousPoints.isEmpty()) {
            hasPoints = true;
            break;
        }
    }
    m_chart->setCompareResult(result);
    setChannelDrawerExpanded(false);
    if (!hasPoints) {
        m_dataZoom->setEnabled(false);
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        setStatus(QStringLiteral("该对比范围内没有日数据"));
    } else {
        syncDataZoomFromChart();
        const QString statLabel = result.statLabel.isEmpty()
            ? TimeSeriesMeta::compareStatLabel(result.stat)
            : result.statLabel;
        setStatus(result.kind == TimeSeriesCompareKind::MonthOverMonth
                      ? QStringLiteral("上月对比完成（按%1）").arg(statLabel)
                      : QStringLiteral("去年对比完成（按%1）").arg(statLabel));
    }
}

void TimeSeriesDialog::onConditionFinished()
{
    setBusy(false);
    if (!m_conditionWatcher) {
        return;
    }
    const TimeSeriesConditionSearchResult result = m_conditionWatcher->result();
    if (!result.ok) {
        setStatus(result.error.isEmpty() ? QStringLiteral("工况反查失败") : result.error, true);
        return;
    }
    setStatus(result.hits.isEmpty()
                  ? QStringLiteral("近90天没有相近工况")
                  : QStringLiteral("工况反查完成，命中 %1 条").arg(result.hits.size()));
    auto *dialog = new TimeSeriesConditionDialog(this);
    dialog->setResult(result);
    connect(dialog, &TimeSeriesConditionDialog::hitSelected,
            this, &TimeSeriesDialog::onConditionHitSelected);
    dialog->show();
}

void TimeSeriesDialog::onConditionHitSelected(qint64 tsUtcMs)
{
    const QDateTime center = QDateTime::fromMSecsSinceEpoch(tsUtcMs, Qt::LocalTime);
    m_pendingCursorMs = tsUtcMs;
    startQuery(center.addMSecs(-TimeSeriesConfig::kConditionReviewSpanMs),
               center.addMSecs(TimeSeriesConfig::kConditionReviewSpanMs));
}

void TimeSeriesDialog::onChannelButtonToggled(bool)
{
    QVector<TimeSeriesChannel> selected;
    const QVector<TimeSeriesChannel> channels = TimeSeriesMeta::allChannels();
    for (TimeSeriesChannel channel : channels) {
        QPushButton *button = m_channelButtons.value(channel, nullptr);
        if (button && button->isChecked()) {
            selected.append(channel);
        }
    }
    if (selected.size() > 4) {
        for (int i = 4; i < selected.size(); ++i) {
            QPushButton *button = m_channelButtons.value(selected.at(i), nullptr);
            if (!button) {
                continue;
            }
            const QSignalBlocker blocker(button);
            button->setChecked(false);
        }
    }
    updateChannelButtonStates();
}

void TimeSeriesDialog::onToggleChannelDrawer()
{
    setChannelDrawerExpanded(!m_channelDrawerExpanded);
}

void TimeSeriesDialog::onFullscreenRequested(TimeSeriesChannel channel)
{
    if (m_busy) {
        return;
    }
    closeFullscreen();
    m_fullscreen = new TimeSeriesFullscreenDialog(this);
    connect(m_fullscreen, &TimeSeriesFullscreenDialog::viewStateClosing,
            this, &TimeSeriesDialog::onFullscreenStateClosing);
    connect(m_fullscreen, &QObject::destroyed, this, [this]() {
        m_fullscreen = nullptr;
    });
    const QPair<qint64, qint64> view = m_chart->visibleRange();
    if (m_chart->isCompareMode()) {
        const TimeSeriesCompareResult one = compareForChannel(channel);
        if (one.series.isEmpty()) {
            m_fullscreen->deleteLater();
            m_fullscreen = nullptr;
            return;
        }
        m_fullscreen->setCompareContent(one,
                                        view.first,
                                        view.second,
                                        m_chart->cursorTime(),
                                        m_chart->isCursorVisible(),
                                        m_chart->interactionMode());
    } else {
        const TimeSeriesQueryResult one = seriesForChannel(channel);
        if (one.series.isEmpty()) {
            m_fullscreen->deleteLater();
            m_fullscreen = nullptr;
            return;
        }
        m_fullscreen->setContent(one,
                                 view.first,
                                 view.second,
                                 m_chart->cursorTime(),
                                 m_chart->isCursorVisible(),
                                 m_chart->interactionMode());
    }
    m_fullscreen->showFullScreen();
}

void TimeSeriesDialog::onFullscreenStateClosing(qint64 startUtcMs,
                                               qint64 endUtcMs,
                                               qint64 cursorTimeMs,
                                               bool cursorVisible,
                                               TimeSeriesChartWidget::InteractionMode mode)
{
    m_chart->setVisibleRange(startUtcMs, endUtcMs);
    if (cursorVisible) {
        m_chart->setCursorTime(cursorTimeMs, true);
    } else {
        m_chart->clearCursor();
    }
    setInteractionMode(mode);
}

bool TimeSeriesDialog::validateRange(QString *error) const
{
    const QDate start = m_startEdit->date();
    const QDate end = m_endEdit->date();
    const QDate today = QDate::currentDate();
    if (end < start) {
        if (error) {
            *error = QStringLiteral("结束日期不能早于开始日期");
        }
        return false;
    }
    if (end > today) {
        if (error) {
            *error = QStringLiteral("结束日期不能晚于今天");
        }
        return false;
    }
    return true;
}

QDateTime TimeSeriesDialog::rangeStartDateTime() const
{
    return QDateTime(m_startEdit->date(), QTime(0, 0, 0));
}

QDateTime TimeSeriesDialog::rangeEndDateTime() const
{
    const QDate endDate = m_endEdit->date();
    const QDateTime now = QDateTime::currentDateTime();
    if (endDate == now.date()) {
        return now;
    }
    return QDateTime(endDate, QTime(23, 59, 59, 999));
}

void TimeSeriesDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_queryButton->setEnabled(!busy);
    m_resetButton->setEnabled(!busy);
    m_startEdit->setEnabled(!busy);
    m_endEdit->setEnabled(!busy);
    m_last30MinutesButton->setEnabled(!busy);
    m_lastHourButton->setEnabled(!busy);
    m_last4HoursButton->setEnabled(!busy);
    m_avgCompareButton->setEnabled(!busy);
    m_minCompareButton->setEnabled(!busy);
    m_maxCompareButton->setEnabled(!busy);
    m_monthCompareButton->setEnabled(!busy);
    m_yearCompareButton->setEnabled(!busy);
    m_conditionButton->setEnabled(!busy);
    m_channelDrawerButton->setEnabled(!busy);
    m_cursorButton->setEnabled(!busy);
    m_panButton->setEnabled(!busy);
    m_chart->setEnabled(!busy);
    if (busy) {
        m_dataZoom->setEnabled(false);
        for (QPushButton *button : m_channelButtons) {
            if (button) {
                button->setEnabled(false);
            }
        }
    } else {
        updateChannelButtonStates();
        syncDataZoomFromChart();
    }
}

void TimeSeriesDialog::setStatus(const QString &text, bool error)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #FF6B8A;")
        : QStringLiteral("color: #9ad7ea;"));
}

void TimeSeriesDialog::updateChannelButtonStates()
{
    const QVector<TimeSeriesChannel> selected = selectedChannels();
    const bool atLimit = selected.size() >= 4;
    for (auto it = m_channelButtons.cbegin(); it != m_channelButtons.cend(); ++it) {
        QPushButton *button = it.value();
        if (!button) {
            continue;
        }
        if (m_busy) {
            button->setEnabled(false);
            continue;
        }
        button->setEnabled(button->isChecked() || !atLimit);
    }
}

void TimeSeriesDialog::setChannelDrawerExpanded(bool expanded)
{
    m_channelDrawerExpanded = expanded;
    m_channelDrawerBody->setVisible(expanded);
    m_channelDrawerButton->setText(expanded ? QStringLiteral("收起") : QStringLiteral("通道选择"));
}

void TimeSeriesDialog::setInteractionMode(TimeSeriesChartWidget::InteractionMode mode)
{
    m_chart->setInteractionMode(mode);
    m_cursorButton->setChecked(mode == TimeSeriesChartWidget::InteractionMode::Cursor);
    m_panButton->setChecked(mode == TimeSeriesChartWidget::InteractionMode::Pan);
    if (m_fullscreen) {
        // Keep fullscreen in sync if it is still open.
    }
}

void TimeSeriesDialog::syncDataZoomFromChart()
{
    if (!m_chart->hasPlotData() || !m_chart->canPanZoom()) {
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        m_dataZoom->setEnabled(false);
        return;
    }
    const qint64 span = m_chart->dataEndMs() - m_chart->dataStartMs();
    const QPair<qint64, qint64> view = m_chart->visibleRange();
    const int lower = static_cast<int>((view.first - m_chart->dataStartMs()) * 10000 / span);
    const int upper = static_cast<int>((view.second - m_chart->dataStartMs()) * 10000 / span);
    const QSignalBlocker blocker(m_dataZoom);
    m_dataZoom->setEnabled(!m_busy);
    m_dataZoom->setSelectedRange(lower, upper);
}

void TimeSeriesDialog::applyDataZoom(int lower, int upper)
{
    if (!m_chart->hasPlotData()) {
        return;
    }
    const qint64 span = m_chart->dataEndMs() - m_chart->dataStartMs();
    if (span <= 0) {
        return;
    }
    const qint64 start = m_chart->dataStartMs() + span * static_cast<qint64>(lower) / 10000;
    const qint64 end = m_chart->dataStartMs() + span * static_cast<qint64>(upper) / 10000;
    m_chart->setVisibleRange(start, end);
}

void TimeSeriesDialog::closeFullscreen()
{
    if (!m_fullscreen) {
        return;
    }
    TimeSeriesFullscreenDialog *dialog = m_fullscreen;
    m_fullscreen = nullptr;
    dialog->close();
    dialog->deleteLater();
}

TimeSeriesQueryResult TimeSeriesDialog::seriesForChannel(TimeSeriesChannel channel) const
{
    TimeSeriesQueryResult one;
    one.ok = m_lastResult.ok;
    one.error = m_lastResult.error;
    one.startUtcMs = m_lastResult.startUtcMs;
    one.endUtcMs = m_lastResult.endUtcMs;
    one.sampleIntervalMs = m_lastResult.sampleIntervalMs;
    for (const TimeSeriesSeries &series : m_lastResult.series) {
        if (series.channel == channel) {
            one.series.append(series);
            break;
        }
    }
    return one;
}

TimeSeriesCompareResult TimeSeriesDialog::compareForChannel(TimeSeriesChannel channel) const
{
    TimeSeriesCompareResult one = m_lastCompare;
    one.series.clear();
    for (const TimeSeriesCompareChannel &item : m_lastCompare.series) {
        if (item.channel == channel) {
            one.series.append(item);
            break;
        }
    }
    return one;
}
