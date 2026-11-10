#include "timeseriesdialog.h"

#include "timeserieschartwidget.h"
#include "timeseriesqueryservice.h"

#include <QDateTime>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QtConcurrent>
#include <QVBoxLayout>

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
        "QGroupBox { color: #5CE1FF; font-size: 16px; border: 1px solid rgba(0,200,255,0.4); margin-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
        "QCheckBox { color: #eaf9ff; font-size: 15px; spacing: 8px; }"
        "QDateTimeEdit { background: #102433; color: #eaf9ff; min-height: 36px; font-size: 16px; }"
        "QPushButton { min-width: 120px; min-height: 44px; font-size: 16px;"
        "  background: #12364a; color: #eaf9ff; border: 1px solid #00c8ff; }"
        "QPushButton:disabled { color: #7a93a0; border-color: #35505c; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *filterRow = new QHBoxLayout();
    auto *startLabel = new QLabel(QStringLiteral("开始"), this);
    auto *endLabel = new QLabel(QStringLiteral("结束"), this);
    m_startEdit = new QDateTimeEdit(this);
    m_endEdit = new QDateTimeEdit(this);
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_startEdit->setCalendarPopup(true);
    m_endEdit->setCalendarPopup(true);
    const QDateTime now = QDateTime::currentDateTime();
    m_endEdit->setDateTime(now);
    m_startEdit->setDateTime(now.addSecs(-3600));
    filterRow->addWidget(startLabel);
    filterRow->addWidget(m_startEdit, 1);
    filterRow->addSpacing(12);
    filterRow->addWidget(endLabel);
    filterRow->addWidget(m_endEdit, 1);

    m_queryButton = new QPushButton(QStringLiteral("查询"), this);
    m_resetButton = new QPushButton(QStringLiteral("重置"), this);
    filterRow->addWidget(m_queryButton);
    filterRow->addWidget(m_resetButton);
    root->addLayout(filterRow);

    auto *channelBox = new QGroupBox(QStringLiteral("通道（最多 4 个）"), this);
    auto *channelLayout = new QGridLayout(channelBox);
    channelLayout->setHorizontalSpacing(18);
    channelLayout->setVerticalSpacing(8);
    const QSet<TimeSeriesChannel> defaultChecked = {
        TimeSeriesChannel::ArmTiltXDeg,
        TimeSeriesChannel::ArmTiltYDeg,
        TimeSeriesChannel::ChassisTiltXDeg,
        TimeSeriesChannel::ChassisTiltYDeg
    };
    const QVector<TimeSeriesChannel> channels = TimeSeriesMeta::allChannels();
    for (int i = 0; i < channels.size(); ++i) {
        const TimeSeriesChannel channel = channels.at(i);
        auto *box = new QCheckBox(QStringLiteral("%1 (%2)")
                                      .arg(TimeSeriesMeta::displayName(channel),
                                           TimeSeriesMeta::unit(channel)),
                                  channelBox);
        box->setChecked(defaultChecked.contains(channel));
        connect(box, &QCheckBox::toggled, this, &TimeSeriesDialog::onChannelToggled);
        m_channelBoxes.insert(channel, box);
        channelLayout->addWidget(box, i / 4, i % 4);
    }
    root->addWidget(channelBox);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_chart = new TimeSeriesChartWidget(this);
    root->addWidget(m_chart, 1);

    m_watcher = new QFutureWatcher<TimeSeriesQueryResult>(this);
    connect(m_watcher, &QFutureWatcher<TimeSeriesQueryResult>::finished,
            this, &TimeSeriesDialog::onQueryFinished);
    connect(m_queryButton, &QPushButton::clicked, this, &TimeSeriesDialog::onQueryClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &TimeSeriesDialog::onResetClicked);

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

void TimeSeriesDialog::onQueryClicked()
{
    QString error;
    if (!validateRange(&error)) {
        setStatus(error, true);
        return;
    }
    const QVector<TimeSeriesChannel> channels = selectedChannels();
    if (channels.isEmpty()) {
        setStatus(QStringLiteral("请至少选择一个通道"), true);
        return;
    }
    if (channels.size() > 4) {
        setStatus(QStringLiteral("最多同时显示 4 个通道"), true);
        return;
    }

    const qint64 startUtcMs = m_startEdit->dateTime().toUTC().toMSecsSinceEpoch();
    const qint64 endUtcMs = m_endEdit->dateTime().toUTC().toMSecsSinceEpoch();
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
    const QDateTime now = QDateTime::currentDateTime();
    m_endEdit->setDateTime(now);
    m_startEdit->setDateTime(now.addSecs(-3600));
    m_chart->resetView();
}

void TimeSeriesDialog::onQueryFinished()
{
    setBusy(false);
    if (!m_watcher) {
        return;
    }
    const TimeSeriesQueryResult result = m_watcher->result();
    if (!result.ok) {
        m_chart->clearSeries();
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
    if (!hasPoints) {
        setStatus(QStringLiteral("该时间范围内没有采样数据"));
    } else {
        setStatus(QStringLiteral("查询完成"));
    }
}

void TimeSeriesDialog::onChannelToggled(bool checked)
{
    if (!checked) {
        return;
    }
    if (selectedChannels().size() <= 4) {
        return;
    }
    auto *box = qobject_cast<QCheckBox*>(sender());
    if (!box) {
        return;
    }
    const QSignalBlocker blocker(box);
    box->setChecked(false);
    setStatus(QStringLiteral("最多同时显示 4 个通道"), true);
}

QVector<TimeSeriesChannel> TimeSeriesDialog::selectedChannels() const
{
    QVector<TimeSeriesChannel> selected;
    for (auto it = m_channelBoxes.cbegin(); it != m_channelBoxes.cend(); ++it) {
        if (it.value() && it.value()->isChecked()) {
            selected.append(it.key());
        }
    }
    return selected;
}

bool TimeSeriesDialog::validateRange(QString *error) const
{
    const QDateTime start = m_startEdit->dateTime();
    const QDateTime end = m_endEdit->dateTime();
    if (end < start) {
        if (error) {
            *error = QStringLiteral("结束时间不能早于开始时间");
        }
        return false;
    }
    if (end > QDateTime::currentDateTime().addSecs(60)) {
        if (error) {
            *error = QStringLiteral("结束时间不能超过当前时间");
        }
        return false;
    }
    return true;
}

void TimeSeriesDialog::setBusy(bool busy)
{
    m_queryButton->setEnabled(!busy);
    m_resetButton->setEnabled(!busy);
    for (QCheckBox *box : m_channelBoxes) {
        if (box) {
            box->setEnabled(!busy);
        }
    }
}

void TimeSeriesDialog::setStatus(const QString &text, bool error)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #FF6B8A;")
        : QStringLiteral("color: #9ad7ea;"));
}
