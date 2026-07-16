#ifndef DEVICECOORDPANEL_H
#define DEVICECOORDPANEL_H

#include <QColor>
#include <QWidget>

class QHBoxLayout;
class QLabel;

class DeviceCoordPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double coordX READ coordX NOTIFY coordinatesChanged)
    Q_PROPERTY(double coordY READ coordY NOTIFY coordinatesChanged)
    Q_PROPERTY(double coordZ READ coordZ NOTIFY coordinatesChanged)
    Q_PROPERTY(double coordAr READ coordAr NOTIFY coordinatesChanged)

public:
    explicit DeviceCoordPanel(QWidget *parent = nullptr);

    double coordX() const { return m_coordX; }
    double coordY() const { return m_coordY; }
    double coordZ() const { return m_coordZ; }
    double coordAr() const { return m_coordAr; }

    void setCoordinates(double x, double y, double z, double ar);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void coordinatesChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct AxisColumn {
        QWidget *cell = nullptr;
        QLabel *axisBadge = nullptr;
        QLabel *value = nullptr;
        QLabel *unit = nullptr;
    };

    static QString formatCoord(double v);
    void applyLabel(QLabel *label, double v);
    AxisColumn addAxisColumn(QHBoxLayout *row,
                             const QString &letter,
                             const QString &unit,
                             const QColor &accent);

    AxisColumn m_colX;
    AxisColumn m_colY;
    AxisColumn m_colZ;
    AxisColumn m_colR;

    double m_coordX = 0.0;
    double m_coordY = 0.0;
    double m_coordZ = 0.0;
    double m_coordAr = 0.0;
};

#endif // DEVICECOORDPANEL_H
