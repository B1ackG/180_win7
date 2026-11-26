#ifndef METRICVALUECARD_H
#define METRICVALUECARD_H

#include <QWidget>

class MetricValueCard : public QWidget
{
    Q_OBJECT

public:
    enum class AlertLevel : quint8 { Normal, Alarm };

    explicit MetricValueCard(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setUnit(const QString &unit);
    void setValue(double value, bool valid);
    void setDecimals(int decimals);

    AlertLevel alertLevel() const { return m_alertLevel; }
    void setAlertLevel(AlertLevel level);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_unit;
    double m_value = 0.0;
    bool m_valid = false;
    int m_decimals = 0;
    AlertLevel m_alertLevel = AlertLevel::Normal;
};

#endif // METRICVALUECARD_H
