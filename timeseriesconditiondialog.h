#ifndef TIMESERIESCONDITIONDIALOG_H
#define TIMESERIESCONDITIONDIALOG_H

#include "timeseriestypes.h"

#include <QDialog>

class QLabel;
class QTableWidget;

class TimeSeriesConditionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimeSeriesConditionDialog(QWidget *parent = nullptr);

    void setResult(const TimeSeriesConditionSearchResult &result);
    TimeSeriesConditionSearchResult result() const { return m_result; }

signals:
    void hitSelected(qint64 tsUtcMs);

private slots:
    void onCellClicked(int row, int column);

private:
    void fillTable();
    QString formatValue(TimeSeriesChannel channel, const TimeSeriesValue &value) const;
    QString workSummary() const;
    QString stateSummary() const;

    TimeSeriesConditionSearchResult m_result;
    QLabel *m_workLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // TIMESERIESCONDITIONDIALOG_H
