#ifndef HISTORYCALENDARPANEL_H
#define HISTORYCALENDARPANEL_H

#include "historyarchiveio.h"

#include <QDate>
#include <QDialog>
#include <QMap>
#include <QVector>

class QLabel;
class QToolButton;

class HistoryCalendarPanel : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryCalendarPanel(const QMap<QDate, HistoryDateEntry> &entries,
                                  QWidget *parent = nullptr);

signals:
    void dateChosen(const QDate &date);

private:
    void buildUi();
    void showMonth(const QDate &month);
    void goToAvailableMonth(int direction);
    bool monthHasEntries(const QDate &month) const;
    void updateMonthShortcuts();

    QMap<QDate, HistoryDateEntry> m_entries;
    QDate m_month;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QToolButton *m_previousButton = nullptr;
    QToolButton *m_nextButton = nullptr;
    QVector<QToolButton *> m_dayButtons;
    QVector<QToolButton *> m_monthButtons;
};

#endif // HISTORYCALENDARPANEL_H
