#ifndef ROBOTTOTALTORQUECARD_H
#define ROBOTTOTALTORQUECARD_H

#include <QVector>
#include <QWidget>

class RobotTotalTorqueCard : public QWidget
{
    Q_OBJECT

public:
    static constexpr double kDefaultDangerPercent = 140.0;

    explicit RobotTotalTorqueCard(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setUnit(const QString &unit);
    void setValue(double percent, bool valid);
    void setDangerThreshold(double percent);
    void setDecimals(int decimals);

    double currentPercent() const { return m_percent; }
    double dangerThreshold() const { return m_dangerThreshold; }
    bool isDanger() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void appendSample(double percent);
    void recomputeDisplayRange();

    QString m_title;
    QString m_unit;
    double m_percent = 0.0;
    double m_dangerThreshold = kDefaultDangerPercent;
    bool m_valid = false;
    int m_decimals = 0;
    QVector<double> m_samples;
    int m_maxSamples = 50;
    double m_maxDisplayPercent = 200.0;
    double m_minDisplayPercent = 0.0;
};

#endif // ROBOTTOTALTORQUECARD_H
