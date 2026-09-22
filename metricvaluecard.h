#ifndef METRICVALUECARD_H
#define METRICVALUECARD_H

#include <QWidget>

class MetricValueCard : public QWidget
{
    Q_OBJECT

public:
    explicit MetricValueCard(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setUnit(const QString &unit);
    void setValue(double value, bool valid);
    void setDecimals(int decimals);

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
};

#endif // METRICVALUECARD_H
