#ifndef INCLINOMETERCARD_H
#define INCLINOMETERCARD_H

#include <QColor>
#include <QWidget>

class InclinometerCard : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal tiltValue READ tiltValue WRITE setTiltValue NOTIFY tiltValueChanged)
    Q_PROPERTY(QString axisLabel READ axisLabel WRITE setAxisLabel NOTIFY axisLabelChanged)

public:
    explicit InclinometerCard(QWidget *parent = nullptr);

    qreal tiltValue() const { return m_tiltValue; }
    void setTiltValue(qreal value);

    QString axisLabel() const { return m_axisLabel; }
    void setAxisLabel(const QString &label);

    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void tiltValueChanged();
    void axisLabelChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString axisLetter() const;
    void syncAccentFromLabel();

    qreal m_tiltValue = 0.0;
    QString m_axisLabel = QStringLiteral("X轴倾角");
    QColor m_accentColor = QColor(QStringLiteral("#5CE1FF"));
};

#endif // INCLINOMETERCARD_H
