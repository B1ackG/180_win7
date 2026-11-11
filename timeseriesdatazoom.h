#ifndef TIMESERIESDATAZOOM_H
#define TIMESERIESDATAZOOM_H

#include <QPair>
#include <QWidget>

class TimeSeriesDataZoom : public QWidget
{
    Q_OBJECT

public:
    explicit TimeSeriesDataZoom(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setSelectedRange(int lower, int upper);
    QPair<int, int> selectedRange() const;
    void setMinimumSpan(int span);
    int minimumSpan() const { return m_minSpan; }
    int minimum() const { return m_minimum; }
    int maximum() const { return m_maximum; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void selectedRangeChanged(int lower, int upper);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class DragPart {
        None,
        Lower,
        Upper,
        Window
    };

    QRect trackRect() const;
    QRect windowRect() const;
    QRect lowerHandleRect() const;
    QRect upperHandleRect() const;
    int valueAtX(int x) const;
    int xAtValue(int value) const;
    void applySelectedRange(int lower, int upper, bool emitChange);
    DragPart hitTest(const QPoint &pos) const;

    int m_minimum = 0;
    int m_maximum = 10000;
    int m_lower = 0;
    int m_upper = 10000;
    int m_minSpan = 1;
    DragPart m_dragPart = DragPart::None;
    int m_dragOffset = 0;
    int m_dragSpan = 0;
};

#endif // TIMESERIESDATAZOOM_H
