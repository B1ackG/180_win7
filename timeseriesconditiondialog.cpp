#include "timeseriesconditiondialog.h"

#include <QColor>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

namespace {

TimeSeriesValue medianOf(const QVector<double> &values)
{
    TimeSeriesValue result;
    if (values.isEmpty()) {
        return result;
    }
    QVector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    result.valid = true;
    if (sorted.size() % 2 == 1) {
        result.value = sorted.at(sorted.size() / 2);
    } else {
        result.value = 0.5 * (sorted.at(sorted.size() / 2 - 1) + sorted.at(sorted.size() / 2));
    }
    return result;
}

QTableWidgetItem *textItem(const QString &text, bool currentRow)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if (currentRow) {
        item->setForeground(QColor(QStringLiteral("#5CE1FF")));
    }
    return item;
}

QString conditionColumnTitle(TimeSeriesChannel channel)
{
    switch (channel) {
    case TimeSeriesChannel::ColumnTorquePercent:
        return QStringLiteral("立柱扭矩");
    case TimeSeriesChannel::CounterweightTorquePercent:
        return QStringLiteral("配重扭矩");
    case TimeSeriesChannel::ArmTiltXDeg:
        return QStringLiteral("悬臂X");
    case TimeSeriesChannel::ArmTiltYDeg:
        return QStringLiteral("悬臂Y");
    case TimeSeriesChannel::ChassisTiltXDeg:
        return QStringLiteral("底盘X");
    case TimeSeriesChannel::ChassisTiltYDeg:
        return QStringLiteral("底盘Y");
    case TimeSeriesChannel::TotalPowerW:
        return QStringLiteral("功率");
    default:
        return TimeSeriesMeta::displayName(channel);
    }
}

} // namespace

TimeSeriesConditionDialog::TimeSeriesConditionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("工况反查"));
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(1480, 760);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #08121c; color: #eaf9ff; }"
        "QLabel { color: #eaf9ff; font-size: 16px; }"
        "QPushButton { min-height: 48px; font-size: 16px;"
        "  background: #12364a; color: #eaf9ff; border: 1px solid #00c8ff; }"
        "QTableWidget { background: #102433; color: #eaf9ff; font-size: 16px;"
        "  gridline-color: #1a4a5c; selection-background-color: #1a6d8c; }"
        "QHeaderView::section { background: #12364a; color: #eaf9ff; font-size: 16px;"
        "  padding: 8px; border: 1px solid #00c8ff; min-height: 40px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    m_workLabel = new QLabel(this);
    m_workLabel->setObjectName(QStringLiteral("Lbl_ConditionWork"));
    m_workLabel->setWordWrap(true);
    m_stateLabel = new QLabel(this);
    m_stateLabel->setObjectName(QStringLiteral("Lbl_ConditionState"));
    m_stateLabel->setWordWrap(true);
    m_hintLabel = new QLabel(QStringLiteral("点历史行可查看该时刻前后15分钟曲线"), this);
    m_hintLabel->setObjectName(QStringLiteral("Lbl_ConditionHint"));

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("Tbl_ConditionHits"));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setShowGrid(true);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
    closeButton->setObjectName(QStringLiteral("TBtn_ConditionClose"));
    closeButton->setMinimumSize(120, 48);

    root->addWidget(m_workLabel);
    root->addWidget(m_stateLabel);
    root->addWidget(m_hintLabel);
    root->addWidget(m_table, 1);
    root->addWidget(closeButton, 0, Qt::AlignRight);

    connect(m_table, &QTableWidget::cellClicked, this, &TimeSeriesConditionDialog::onCellClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
}

void TimeSeriesConditionDialog::setResult(const TimeSeriesConditionSearchResult &result)
{
    m_result = result;
    m_workLabel->setText(workSummary());
    m_stateLabel->setText(stateSummary());
    fillTable();
}

void TimeSeriesConditionDialog::onCellClicked(int row, int)
{
    if (row <= 0 || row > m_result.hits.size()) {
        return;
    }
    emit hitSelected(m_result.hits.at(row - 1).tsUtcMs);
    close();
}

void TimeSeriesConditionDialog::fillTable()
{
    const int stateCount = m_result.stateChannels.size();
    m_table->clear();
    m_table->setColumnCount(1 + stateCount);
    QStringList headers;
    headers << QStringLiteral("时间");
    for (TimeSeriesChannel channel : m_result.stateChannels) {
        headers << conditionColumnTitle(channel);
    }
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(1 + m_result.hits.size());

    m_table->setItem(0, 0, textItem(QStringLiteral("当前"), true));
    for (int i = 0; i < stateCount; ++i) {
        const TimeSeriesValue value = (i < m_result.probeState.size())
            ? m_result.probeState.at(i)
            : TimeSeriesValue();
        m_table->setItem(0, 1 + i, textItem(formatValue(m_result.stateChannels.at(i), value), true));
    }

    for (int row = 0; row < m_result.hits.size(); ++row) {
        const TimeSeriesConditionHit &hit = m_result.hits.at(row);
        const QString timeText = QDateTime::fromMSecsSinceEpoch(hit.tsUtcMs, Qt::LocalTime)
                                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        m_table->setItem(row + 1, 0, textItem(timeText, false));
        for (int i = 0; i < stateCount; ++i) {
            const TimeSeriesValue value = (i < hit.stateValues.size())
                ? hit.stateValues.at(i)
                : TimeSeriesValue();
            m_table->setItem(row + 1, 1 + i, textItem(formatValue(m_result.stateChannels.at(i), value), false));
        }
    }
}

QString TimeSeriesConditionDialog::formatValue(TimeSeriesChannel channel, const TimeSeriesValue &value) const
{
    if (!value.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 %2")
        .arg(QString::number(value.value, 'f', TimeSeriesMeta::displayDecimals(channel)),
             TimeSeriesMeta::unit(channel));
}

QString TimeSeriesConditionDialog::workSummary() const
{
    QStringList parts;
    for (int i = 0; i < m_result.workChannels.size(); ++i) {
        const TimeSeriesChannel channel = m_result.workChannels.at(i);
        const TimeSeriesValue value = (i < m_result.probeWork.size())
            ? m_result.probeWork.at(i)
            : TimeSeriesValue();
        const int quantum = TimeSeriesMeta::conditionQuantum(channel);
        const QString tolerance = (quantum <= 1)
            ? QStringLiteral("取整到%1").arg(TimeSeriesMeta::unit(channel))
            : QStringLiteral("%1%2").arg(quantum).arg(TimeSeriesMeta::unit(channel));
        parts << QStringLiteral("%1 %2（容差 %3）")
                     .arg(TimeSeriesMeta::displayName(channel),
                          formatValue(channel, value),
                          tolerance);
    }
    if (parts.isEmpty()) {
        return QStringLiteral("作业条件不足");
    }
    return QStringLiteral("作业条件：%1").arg(parts.join(QStringLiteral("  ")));
}

QString TimeSeriesConditionDialog::stateSummary() const
{
    if (m_result.hits.isEmpty()) {
        return QStringLiteral("近%1天没有相近工况（已扫%2天）")
            .arg(m_result.lookbackDays)
            .arg(m_result.scannedDays);
    }

    QStringList parts;
    for (int i = 0; i < m_result.stateChannels.size(); ++i) {
        const TimeSeriesChannel channel = m_result.stateChannels.at(i);
        QVector<double> history;
        history.reserve(m_result.hits.size());
        for (const TimeSeriesConditionHit &hit : m_result.hits) {
            if (i < hit.stateValues.size() && hit.stateValues.at(i).valid) {
                history.append(hit.stateValues.at(i).value);
            }
        }
        const TimeSeriesValue median = medianOf(history);
        const TimeSeriesValue current = (i < m_result.probeState.size())
            ? m_result.probeState.at(i)
            : TimeSeriesValue();
        QString deltaText = QStringLiteral("--");
        if (current.valid && median.valid) {
            const double delta = current.value - median.value;
            deltaText = QStringLiteral("%1%2")
                            .arg(delta >= 0 ? QStringLiteral("+") : QString())
                            .arg(QString::number(delta, 'f', TimeSeriesMeta::displayDecimals(channel)));
        }
        parts << QStringLiteral("%1 当前 %2 / 历史中位 %3（差 %4）")
                     .arg(TimeSeriesMeta::displayName(channel),
                          formatValue(channel, current),
                          formatValue(channel, median),
                          deltaText);
    }
    return QStringLiteral("命中 %1 条相近工况（近%2天，已去重停留）。%3")
        .arg(m_result.hits.size())
        .arg(m_result.lookbackDays)
        .arg(parts.join(QStringLiteral("；")));
}
