#include "historycalendarpanel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QGuiApplication>

namespace {

QString dayStyle(bool available, bool today, bool outsideMonth)
{
    const QString base = QStringLiteral(
        "QToolButton { border-radius: 8px; font-size: 20px; font-weight: bold; }");
    if (outsideMonth || !available) {
        return base + QStringLiteral(
            "QToolButton { background: rgba(8, 18, 30, 0.55); color: rgba(180, 200, 214, 0.35);"
            " border: 1px solid rgba(80, 110, 130, 0.18); }");
    }
    if (today) {
        return base + QStringLiteral(
            "QToolButton { background: rgba(0, 126, 186, 0.95); color: #ffffff;"
            " border: 2px solid #8ff6ff; }"
            "QToolButton:pressed { background: rgba(0, 90, 140, 1.0); }");
    }
    return base + QStringLiteral(
        "QToolButton { background: rgba(14, 58, 90, 0.95); color: #eaf9ff;"
        " border: 1px solid rgba(111, 231, 255, 0.70); }"
        "QToolButton:pressed { background: rgba(0, 126, 186, 1.0); }");
}

} // namespace

HistoryCalendarPanel::HistoryCalendarPanel(const QMap<QDate, HistoryDateEntry> &entries,
                                           QWidget *parent)
    : QDialog(parent)
    , m_entries(entries)
{
    setWindowTitle(QStringLiteral("查看历史记录"));
    setModal(true);
    const QRect screen = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1280, 800);
    resize(qMin(1100, screen.width() - 80), qMin(760, screen.height() - 80));
    setMinimumSize(qMin(900, screen.width() - 40), qMin(640, screen.height() - 40));
    setStyleSheet(QStringLiteral(
        "QDialog { background: #071422; }"
        "QLabel { color: #eaf9ff; }"
        "QToolButton { min-height: 56px; font-size: 18px; color: #eaf9ff;"
        " background: rgba(14, 58, 90, 0.92); border: 1px solid rgba(111, 231, 255, 0.45);"
        " border-radius: 8px; padding: 6px 10px; }"
        "QToolButton:disabled { background: rgba(12, 22, 34, 0.80); color: rgba(180, 200, 214, 0.35);"
        " border: 1px solid rgba(80, 110, 130, 0.20); }"));
    buildUi();

    QDate initial = QDate::currentDate();
    if (!monthHasEntries(initial)) {
        initial = m_entries.isEmpty() ? QDate::currentDate() : m_entries.lastKey();
    }
    showMonth(QDate(initial.year(), initial.month(), 1));
}

void HistoryCalendarPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    auto *header = new QHBoxLayout();
    m_previousButton = new QToolButton(this);
    m_previousButton->setText(QStringLiteral("上一月"));
    m_nextButton = new QToolButton(this);
    m_nextButton->setText(QStringLiteral("下一月"));
    auto *closeButton = new QToolButton(this);
    closeButton->setText(QStringLiteral("关闭"));
    closeButton->setMinimumSize(88, 56);
    m_titleLabel = new QLabel(this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold; color: #8ff6ff;"));
    header->addWidget(m_previousButton);
    header->addWidget(m_titleLabel, 1);
    header->addWidget(m_nextButton);
    header->addWidget(closeButton);
    root->addLayout(header);

    auto *weekRow = new QHBoxLayout();
    weekRow->setSpacing(8);
    const QStringList weeks = {QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
                               QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
                               QStringLiteral("日")};
    for (const QString &week : weeks) {
        auto *label = new QLabel(week, this);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QStringLiteral("font-size: 16px; color: #91dfff;"));
        weekRow->addWidget(label, 1);
    }
    root->addLayout(weekRow);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    for (int index = 0; index < 42; ++index) {
        auto *button = new QToolButton(this);
        button->setMinimumSize(72, 64);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_dayButtons.append(button);
        grid->addWidget(button, index / 7, index % 7);
        connect(button, &QAbstractButton::clicked, this, [this, button]() {
            const QDate date = button->property("historyDate").toDate();
            if (!button->isEnabled() || !date.isValid()) {
                return;
            }
            emit dateChosen(date);
            accept();
        });
    }
    root->addLayout(grid, 1);

    m_emptyLabel = new QLabel(QStringLiteral("暂无可查看的历史记录"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("font-size: 20px; color: rgba(234, 249, 255, 0.70);"));
    root->addWidget(m_emptyLabel);

    auto *months = new QGridLayout();
    months->setHorizontalSpacing(8);
    months->setVerticalSpacing(8);
    for (int month = 1; month <= 12; ++month) {
        auto *button = new QToolButton(this);
        button->setText(QStringLiteral("%1月").arg(month));
        button->setMinimumSize(64, 48);
        m_monthButtons.append(button);
        months->addWidget(button, (month - 1) / 6, (month - 1) % 6);
        connect(button, &QAbstractButton::clicked, this, [this, month]() {
            showMonth(QDate(m_month.year(), month, 1));
        });
    }
    root->addLayout(months);

    connect(m_previousButton, &QAbstractButton::clicked, this, [this]() { goToAvailableMonth(-1); });
    connect(m_nextButton, &QAbstractButton::clicked, this, [this]() { goToAvailableMonth(1); });
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
}

void HistoryCalendarPanel::showMonth(const QDate &month)
{
    m_month = QDate(month.year(), month.month(), 1);
    m_titleLabel->setText(m_month.toString(QStringLiteral("yyyy年MM月")));
    m_emptyLabel->setVisible(m_entries.isEmpty());

    const QDate today = QDate::currentDate();
    const QDate firstCell = m_month.addDays(1 - m_month.dayOfWeek());
    for (int index = 0; index < m_dayButtons.size(); ++index) {
        QToolButton *button = m_dayButtons.at(index);
        const QDate date = firstCell.addDays(index);
        const bool outside = date.month() != m_month.month() || date.year() != m_month.year();
        const bool available = !outside && m_entries.contains(date);
        button->setText(QString::number(date.day()));
        button->setEnabled(available);
        button->setProperty("historyDate", date);
        button->setStyleSheet(dayStyle(available, date == today, outside));
    }
    updateMonthShortcuts();
    m_previousButton->setEnabled(!m_entries.isEmpty());
    m_nextButton->setEnabled(!m_entries.isEmpty());
}

bool HistoryCalendarPanel::monthHasEntries(const QDate &month) const
{
    const QDate first(month.year(), month.month(), 1);
    auto it = m_entries.lowerBound(first);
    return it != m_entries.cend() && it.key().year() == first.year() && it.key().month() == first.month();
}

void HistoryCalendarPanel::goToAvailableMonth(int direction)
{
    if (direction == 0 || m_entries.isEmpty()) {
        return;
    }
    QDate cursor = m_month.addMonths(direction);
    for (int step = 0; step < 240; ++step) {
        if (monthHasEntries(cursor)) {
            showMonth(cursor);
            return;
        }
        cursor = cursor.addMonths(direction);
    }
}

void HistoryCalendarPanel::updateMonthShortcuts()
{
    for (int month = 1; month <= m_monthButtons.size(); ++month) {
        QToolButton *button = m_monthButtons.at(month - 1);
        const bool available = monthHasEntries(QDate(m_month.year(), month, 1));
        button->setEnabled(available);
        button->setText(QStringLiteral("%1月").arg(month));
    }
}
