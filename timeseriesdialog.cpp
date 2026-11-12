#include "timeseriesdialog.h"

#include "historycalendarpanel.h"
#include "timeserieschartwidget.h"
#include "timeseriesdatazoom.h"
#include "timeseriesconditiondialog.h"
#include "timeseriesfullscreendialog.h"
#include "timeseriesqueryservice.h"

#include <QDate>
#include <QHideEvent>
#include <QDateEdit>
#include <QDateTime>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTime>
#include <QToolButton>
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
    : QWidget(parent)
    , m_directory(databaseDirectory)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "TimeSeriesDialog { background: #08121c; color: #eaf9ff; }"
        "QLabel { color: #eaf9ff; font-size: 16px; }"
        "QDateEdit { background: #102433; color: #eaf9ff; min-height: 48px; font-size: 16px; }"
        "QPushButton { min-height: 48px; font-size: 16px;"
        "  background: #12364a; color: #eaf9ff; border: 1px solid #00c8ff; }"
        "QPushButton:checked { background: #1a6d8c; color: #ffffff; font-weight: bold;"
        "  border: 2px solid #5CE1FF; }"
        "QPushButton:disabled { background: #1a1a1a; color: #6a7a82; border-color: #3a4a52; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *filterRow = new QHBoxLayout();
    auto *dateLabel = new QLabel(QStringLiteral("日期"), this);
    m_dateEdit = new QDateEdit(this);
    m_dateEdit->setObjectName(QStringLiteral("Dt_Date"));
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setReadOnly(true);
    m_dateEdit->setDate(QDate::currentDate());
    m_dateEdit->setMinimumWidth(180);
    m_dateEdit->setAttribute(Qt::WA_InputMethodEnabled, false);
    m_dateEdit->installEventFilter(this);
    const auto dateEditChildren = m_dateEdit->findChildren<QWidget *>();
    for (QWidget *child : dateEditChildren) {
        child->setAttribute(Qt::WA_InputMethodEnabled, false);
        child->setFocusPolicy(Qt::NoFocus);
        child->installEventFilter(this);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(child)) {
            lineEdit->setReadOnly(true);
        }
    }

    m_recentButton = new QPushButton(QStringLiteral("近期状态"), this);
    m_recentButton->setObjectName(QStringLiteral("TBtn_RecentStatus"));
    m_recentButton->setCheckable(true);
    m_recentButton->setMinimumSize(120, 48);
    m_last30MinutesButton = new QPushButton(QStringLiteral("最近30分钟"), this);
    m_lastHourButton = new QPushButton(QStringLiteral("最近1小时"), this);
    m_last4HoursButton = new QPushButton(QStringLiteral("最近4小时"), this);
    m_conditionButton = new QPushButton(QStringLiteral("工况反查"), this);
    m_last30MinutesButton->setObjectName(QStringLiteral("TBtn_QuickLast30Minutes"));
    m_lastHourButton->setObjectName(QStringLiteral("TBtn_QuickLastHour"));
    m_last4HoursButton->setObjectName(QStringLiteral("TBtn_QuickLast4Hours"));
    m_conditionButton->setObjectName(QStringLiteral("TBtn_ConditionSearch"));
    m_last30MinutesButton->setMinimumSize(120, 48);
    m_lastHourButton->setMinimumSize(120, 48);
    m_last4HoursButton->setMinimumSize(120, 48);
    m_conditionButton->setMinimumSize(120, 48);

    m_channelDrawerButton = new QPushButton(QStringLiteral("通道选择"), this);
    m_channelDrawerButton->setObjectName(QStringLiteral("TBtn_ChannelDrawer"));
    m_channelDrawerButton->setMinimumSize(120, 48);

    filterRow->addWidget(dateLabel);
    filterRow->addWidget(m_dateEdit);
    filterRow->addWidget(m_recentButton);
    filterRow->addWidget(m_channelDrawerButton);
    filterRow->addWidget(m_conditionButton);
    filterRow->addStretch(1);
    root->addLayout(filterRow);

    m_recentDrawerBody = new QWidget(this);
    m_recentDrawerBody->setObjectName(QStringLiteral("Wgt_RecentDrawer"));
    auto *recentLayout = new QHBoxLayout(m_recentDrawerBody);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(10);
    recentLayout->addWidget(m_last30MinutesButton);
    recentLayout->addWidget(m_lastHourButton);
    recentLayout->addWidget(m_last4HoursButton);
    recentLayout->addStretch(1);
    m_recentDrawerBody->setVisible(false);
    root->addWidget(m_recentDrawerBody);

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

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("Lbl_TimeSeriesStatus"));
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_chart = new TimeSeriesChartWidget(this);
    m_chart->setObjectName(QStringLiteral("TimeSeriesChart"));
    m_chart->setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    root->addWidget(m_chart, 1);

    m_dataZoom = new TimeSeriesDataZoom(this);
    m_dataZoom->setObjectName(QStringLiteral("TimeSeriesDataZoom"));
    m_dataZoom->setRange(0, 10000);
    m_dataZoom->setSelectedRange(0, 10000);
    m_dataZoom->setMinimumSpan(1);
    m_dataZoom->setEnabled(false);
    root->addWidget(m_dataZoom);

    qRegisterMetaType<TimeSeriesQueryResult>("TimeSeriesQueryResult");
    qRegisterMetaType<TimeSeriesConditionSearchResult>("TimeSeriesConditionSearchResult");
    m_watcher = new QFutureWatcher<TimeSeriesQueryResult>(this);
    m_conditionWatcher = new QFutureWatcher<TimeSeriesConditionSearchResult>(this);
    connect(m_watcher, &QFutureWatcher<TimeSeriesQueryResult>::finished,
            this, &TimeSeriesDialog::onQueryFinished);
    connect(m_conditionWatcher, &QFutureWatcher<TimeSeriesConditionSearchResult>::finished,
            this, &TimeSeriesDialog::onConditionFinished);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &TimeSeriesDialog::onDateChanged);
    connect(m_recentButton, &QPushButton::clicked, this, &TimeSeriesDialog::onToggleRecentDrawer);
    connect(m_last30MinutesButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(30 * 60);
    });
    connect(m_lastHourButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(3600);
    });
    connect(m_last4HoursButton, &QPushButton::clicked, this, [this]() {
        startQuickQuery(4 * 3600);
    });
    connect(m_conditionButton, &QPushButton::clicked, this, &TimeSeriesDialog::startConditionSearch);
    connect(m_channelDrawerButton, &QPushButton::clicked,
            this, &TimeSeriesDialog::onToggleChannelDrawer);
    connect(m_chart, &TimeSeriesChartWidget::fullscreenRequested,
            this, &TimeSeriesDialog::onFullscreenRequested);
    connect(m_chart, &TimeSeriesChartWidget::visibleRangeChanged,
            this, [this](qint64, qint64) { syncDataZoomFromChart(); });
    connect(m_dataZoom, &TimeSeriesDataZoom::selectedRangeChanged,
            this, &TimeSeriesDialog::applyDataZoom);

    updateChannelButtonStates();
    setStatus(QStringLiteral("选择日期后自动查询，或点近期状态"));
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

void TimeSeriesDialog::applyQuickRange(QuickRange)
{
    const QSignalBlocker blocker(m_dateEdit);
    m_dateEdit->setDate(QDate::currentDate());
}

QPushButton *TimeSeriesDialog::channelButton(TimeSeriesChannel channel) const
{
    return m_channelButtons.value(channel, nullptr);
}

void TimeSeriesDialog::hideEvent(QHideEvent *event)
{
    closeFullscreen();
    QWidget::hideEvent(event);
}

bool TimeSeriesDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_dateEdit || (watched && watched->parent() == m_dateEdit)) {
        if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick) {
            if (!m_busy && m_dateEdit->isEnabled()) {
                openDateCalendar();
            }
            return true;
        }
        if (event->type() == QEvent::Wheel) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TimeSeriesDialog::openDateCalendar()
{
    QList<QDate> dates = TimeSeriesQueryService::availableDates(m_directory);
    const QDate today = QDate::currentDate();
    if (!dates.contains(today)) {
        dates.append(today);
    }
    HistoryCalendarPanel dialog(dates, window(),
                                QStringLiteral("选择日期"),
                                QStringLiteral("暂无已保存的时序数据"));
    connect(&dialog, &HistoryCalendarPanel::dateChosen, this, [this](const QDate &date) {
        if (m_dateEdit->date() == date) {
            startDateQuery();
            return;
        }
        m_dateEdit->setDate(date);
    });
    dialog.exec();
}

void TimeSeriesDialog::onDateChanged(const QDate &)
{
    startDateQuery();
}

void TimeSeriesDialog::startDateQuery()
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
    m_lastQuickSeconds = seconds;
}

void TimeSeriesDialog::repeatLastQuery()
{
    if (m_lastQuickSeconds > 0) {
        startQuickQuery(m_lastQuickSeconds);
        return;
    }
    if (m_lastQueryStart.isValid() && m_lastQueryEnd.isValid()) {
        startQuery(m_lastQueryStart, m_lastQueryEnd);
        return;
    }
    startDateQuery();
}

void TimeSeriesDialog::startConditionSearch()
{
    if (!m_lastResult.ok || !m_chart->isCursorVisible()) {
        setStatus(QStringLiteral("请先查询1Hz曲线，在图上点游标后再工况反查"), true);
        return;
    }
    closeFullscreen();
    setChannelDrawerExpanded(false);
    setRecentDrawerExpanded(false);
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
    m_lastQueryStart = start;
    m_lastQueryEnd = end;
    m_lastQuickSeconds = 0;

    closeFullscreen();
    setChannelDrawerExpanded(false);
    setRecentDrawerExpanded(false);

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

void TimeSeriesDialog::onQueryFinished()
{
    setBusy(false);
    if (!m_watcher) {
        return;
    }
    const TimeSeriesQueryResult result = m_watcher->result();
    m_lastResult = result;
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
    if (m_channelDrawerExpanded) {
        setChannelDrawerExpanded(false);
        repeatLastQuery();
        return;
    }
    setChannelDrawerExpanded(true);
}

void TimeSeriesDialog::onToggleRecentDrawer()
{
    setRecentDrawerExpanded(!m_recentDrawerExpanded);
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
                             TimeSeriesChartWidget::InteractionMode::Cursor);
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
    Q_UNUSED(mode);
    m_chart->setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
}

bool TimeSeriesDialog::validateRange(QString *error) const
{
    const QDate date = m_dateEdit->date();
    if (!date.isValid()) {
        if (error) {
            *error = QStringLiteral("请选择有效日期");
        }
        return false;
    }
    if (date > QDate::currentDate()) {
        if (error) {
            *error = QStringLiteral("日期不能晚于今天");
        }
        return false;
    }
    return true;
}

QDateTime TimeSeriesDialog::rangeStartDateTime() const
{
    return QDateTime(m_dateEdit->date(), QTime(0, 0, 0));
}

QDateTime TimeSeriesDialog::rangeEndDateTime() const
{
    const QDate date = m_dateEdit->date();
    const QDateTime now = QDateTime::currentDateTime();
    if (date == now.date()) {
        return now;
    }
    return QDateTime(date, QTime(23, 59, 59, 999));
}

void TimeSeriesDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_dateEdit->setEnabled(!busy);
    m_recentButton->setEnabled(!busy);
    m_last30MinutesButton->setEnabled(!busy);
    m_lastHourButton->setEnabled(!busy);
    m_last4HoursButton->setEnabled(!busy);
    m_conditionButton->setEnabled(!busy);
    m_channelDrawerButton->setEnabled(!busy);
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
    if (expanded) {
        setRecentDrawerExpanded(false);
    }
}

void TimeSeriesDialog::setRecentDrawerExpanded(bool expanded)
{
    m_recentDrawerExpanded = expanded;
    m_recentDrawerBody->setVisible(expanded);
    const QSignalBlocker blocker(m_recentButton);
    m_recentButton->setChecked(expanded);
    if (expanded) {
        setChannelDrawerExpanded(false);
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
