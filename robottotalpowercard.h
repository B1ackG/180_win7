#ifndef ROBOTTOTALPOWERCARD_H
#define ROBOTTOTALPOWERCARD_H

#include <QWidget>

class RobotTotalPowerCard : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double currentPower READ currentPower WRITE setCurrentPower NOTIFY currentPowerChanged)

public:
    explicit RobotTotalPowerCard(QWidget *parent = nullptr);

    double currentPower() const { return m_currentPower; }
    void setCurrentPower(double power);

    void setTitle(const QString &title);
    void setUnit(const QString &unit);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void currentPowerChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void appendSample(double power);

    QString m_title;
    QString m_unit;
    double m_currentPower = 0.0;
    QVector<double> m_samples;
    int m_maxSamples = 50;
    double m_maxDisplayPower = 1000.0;
};

#endif // ROBOTTOTALPOWERCARD_H
