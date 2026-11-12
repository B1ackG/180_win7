#include "timeseriesfullscreendialog.h"

#include "timeseriesdatazoom.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

TimeSeriesFullscreenDialog::TimeSeriesFullscreenDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("数据曲线全屏"));
    setModal(false);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #08121c; color: #eaf9ff; }"
        "QLabel { color: #eaf9ff; font-size: 18px; }"
        "QPushButton { min-height: 48px; min-width: 120px; font-size: 16px;"
        "  background: #12364a; color: #eaf9ff; border: 1px solid #00c8ff; }"
        "QPushButton:checked { background: #1a6d8c; color: #ffffff; font-weight: bold; }"
        "QPushButton:disabled { color: #7a93a0; border-color: #35505c; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *top = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    m_exitButton = new QPushButton(QStringLiteral("退出全屏"), this);
    m_exitButton->setMinimumHeight(48);
    top->addWidget(m_titleLabel, 1);
    top->addWidget(m_exitButton);
    root->addLayout(top);

    m_chart = new TimeSeriesChartWidget(this);
    m_chart->setFullscreenButtonsVisible(false);
    root->addWidget(m_chart, 1);

    m_dataZoom = new TimeSeriesDataZoom(this);
    m_dataZoom->setRange(0, 10000);
    m_dataZoom->setSelectedRange(0, 10000);
    m_dataZoom->setMinimumSpan(1);
    root->addWidget(m_dataZoom);

    connect(m_exitButton, &QPushButton::clicked, this, &QDialog::close);
    connect(m_chart, &TimeSeriesChartWidget::visibleRangeChanged,
            this, [this](qint64, qint64) { syncDataZoomFromChart(); });
    connect(m_dataZoom, &TimeSeriesDataZoom::selectedRangeChanged,
            this, &TimeSeriesFullscreenDialog::applyDataZoom);
}

void TimeSeriesFullscreenDialog::setContent(const TimeSeriesQueryResult &oneChannelResult,
                                           qint64 viewStartMs,
                                           qint64 viewEndMs,
                                           qint64 cursorTimeMs,
                                           bool cursorVisible,
                                           TimeSeriesChartWidget::InteractionMode mode)
{
    m_closingEmitted = false;
    QString title = QStringLiteral("数据曲线");
    if (!oneChannelResult.series.isEmpty()) {
        title = TimeSeriesMeta::displayName(oneChannelResult.series.first().channel);
    }
    m_titleLabel->setText(title);
    m_chart->setSeries(oneChannelResult);
    m_chart->setVisibleRange(viewStartMs, viewEndMs);
    if (cursorVisible) {
        m_chart->setCursorTime(cursorTimeMs, true);
    } else {
        m_chart->clearCursor();
    }
    Q_UNUSED(mode);
    setMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    syncDataZoomFromChart();
    m_dataZoom->setEnabled(m_chart->canPanZoom());
}

qint64 TimeSeriesFullscreenDialog::viewStartMs() const
{
    return m_chart->visibleRange().first;
}

qint64 TimeSeriesFullscreenDialog::viewEndMs() const
{
    return m_chart->visibleRange().second;
}

qint64 TimeSeriesFullscreenDialog::cursorTimeMs() const
{
    return m_chart->cursorTime();
}

bool TimeSeriesFullscreenDialog::isCursorVisible() const
{
    return m_chart->isCursorVisible();
}

TimeSeriesChartWidget::InteractionMode TimeSeriesFullscreenDialog::interactionMode() const
{
    return m_chart->interactionMode();
}

void TimeSeriesFullscreenDialog::closeEvent(QCloseEvent *event)
{
    emitClosingState();
    QDialog::closeEvent(event);
}

void TimeSeriesFullscreenDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}

void TimeSeriesFullscreenDialog::setMode(TimeSeriesChartWidget::InteractionMode mode)
{
    m_chart->setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    Q_UNUSED(mode);
}

void TimeSeriesFullscreenDialog::syncDataZoomFromChart()
{
    if (!m_chart->hasPlotData()) {
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        m_dataZoom->setEnabled(false);
        return;
    }
    if (!m_chart->canPanZoom()) {
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        m_dataZoom->setEnabled(false);
        return;
    }
    const qint64 span = m_chart->dataEndMs() - m_chart->dataStartMs();
    if (span <= 0) {
        const QSignalBlocker blocker(m_dataZoom);
        m_dataZoom->setSelectedRange(0, 10000);
        m_dataZoom->setEnabled(false);
        return;
    }
    const QPair<qint64, qint64> view = m_chart->visibleRange();
    const int lower = static_cast<int>((view.first - m_chart->dataStartMs()) * 10000 / span);
    const int upper = static_cast<int>((view.second - m_chart->dataStartMs()) * 10000 / span);
    const QSignalBlocker blocker(m_dataZoom);
    m_dataZoom->setEnabled(true);
    m_dataZoom->setSelectedRange(lower, upper);
}

void TimeSeriesFullscreenDialog::applyDataZoom(int lower, int upper)
{
    if (!m_chart->hasPlotData()) {
        return;
    }
    const qint64 span = m_chart->dataEndMs() - m_chart->dataStartMs();
    if (span <= 0) {
        return;
    }
    const qint64 start = m_chart->dataStartMs() + span * lower / 10000;
    const qint64 end = m_chart->dataStartMs() + span * upper / 10000;
    m_chart->setVisibleRange(start, end);
}

void TimeSeriesFullscreenDialog::emitClosingState()
{
    if (m_closingEmitted) {
        return;
    }
    m_closingEmitted = true;
    emit viewStateClosing(viewStartMs(),
                          viewEndMs(),
                          cursorTimeMs(),
                          isCursorVisible(),
                          interactionMode());
}
