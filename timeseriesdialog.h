#ifndef TIMESERIESDIALOG_H
#define TIMESERIESDIALOG_H

#include "timeseriestypes.h"

#include <QCheckBox>
#include <QDialog>
#include <QFutureWatcher>
#include <QMap>
#include <QVector>

class QDateTimeEdit;
class QLabel;
class QPushButton;
class TimeSeriesChartWidget;

class TimeSeriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimeSeriesDialog(const QString &databaseDirectory, QWidget *parent = nullptr);
    void reloadAvailableDates();

private slots:
    void onQueryClicked();
    void onResetClicked();
    void onQueryFinished();
    void onChannelToggled(bool checked);

private:
    QVector<TimeSeriesChannel> selectedChannels() const;
    bool validateRange(QString *error) const;
    void setBusy(bool busy);
    void setStatus(const QString &text, bool error = false);

    QString m_directory;
    QDateTimeEdit *m_startEdit = nullptr;
    QDateTimeEdit *m_endEdit = nullptr;
    QMap<TimeSeriesChannel, QCheckBox*> m_channelBoxes;
    QPushButton *m_queryButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    TimeSeriesChartWidget *m_chart = nullptr;
    QFutureWatcher<TimeSeriesQueryResult> *m_watcher = nullptr;
    int m_queryToken = 0;
    int m_activeQueryToken = 0;
};

#endif // TIMESERIESDIALOG_H
