#ifndef HISTORYCALENDARPANEL_H
#define HISTORYCALENDARPANEL_H

#include <QDate>
#include <QDialog>
#include <QList>
#include <QSet>
#include <QVector>

class QLabel;
class QToolButton;

class HistoryCalendarPanel : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryCalendarPanel(const QList<QDate> &availableDates,
                                  QWidget *parent = nullptr,
                                  const QString &title = QStringLiteral("查看历史记录"),
                                  const QString &emptyText = QStringLiteral("暂无可查看的历史记录"));

signals:
    void dateChosen(const QDate &date);

private:
    void buildUi();
    void showMonth(const QDate &month);
    void goToAvailableMonth(int direction);
    bool monthHasEntries(const QDate &month) const;
    void updateMonthShortcuts();

    QSet<QDate> m_dates;
    QString m_emptyText;
    QDate m_month;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QToolButton *m_previousButton = nullptr;
    QToolButton *m_nextButton = nullptr;
    QVector<QToolButton *> m_dayButtons;
    QVector<QToolButton *> m_monthButtons;
};

#endif // HISTORYCALENDARPANEL_H
