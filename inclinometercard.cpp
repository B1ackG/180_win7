#include "inclinometercard.h"

#include <QPaintEvent>
#include <QPainter>
#include <QLinearGradient>

namespace {
constexpr int kOuterRadius = 14;
constexpr int kInnerRadius = 12;
constexpr int kInnerInset = 2;
constexpr int kTopLineInset = 8;
constexpr int kValueTopMargin = 12;
constexpr int kAxisBottomMargin = 12;
} // namespace

InclinometerCard::InclinometerCard(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void InclinometerCard::setTiltValue(qreal value)
{
    if (qFuzzyCompare(m_tiltValue + 1.0, value + 1.0)) {
        return;
    }
    m_tiltValue = value;
    emit tiltValueChanged();
    update();
}

void InclinometerCard::setAxisLabel(const QString &label)
{
    if (m_axisLabel == label) {
        return;
    }
    m_axisLabel = label;
    emit axisLabelChanged();
    update();
}

QSize InclinometerCard::sizeHint() const
{
    return QSize(130, 91);
}

QSize InclinometerCard::minimumSizeHint() const
{
    return QSize(110, 78);
}

void InclinometerCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    if (bounds.width() <= 4.0 || bounds.height() <= 4.0) {
        return;
    }

    QLinearGradient panelGradient(bounds.topLeft(), bounds.bottomLeft());
    panelGradient.setColorAt(0.0, QColor(8, 34, 58, 226));
    panelGradient.setColorAt(1.0, QColor(4, 18, 34, 218));
    painter.setPen(QPen(QColor(74, 190, 238, 132), 1.0));
    painter.setBrush(panelGradient);
    painter.drawRoundedRect(bounds, kOuterRadius, kOuterRadius);

    const QRectF inner = bounds.adjusted(kInnerInset, kInnerInset, -kInnerInset, -kInnerInset);
    painter.setPen(QPen(QColor(122, 224, 255, 74), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner, kInnerRadius, kInnerRadius);

    painter.setOpacity(0.55);
    painter.setPen(QPen(QColor(111, 231, 255, 120), 1.0));
    const qreal lineY = bounds.top() + kTopLineInset;
    painter.drawLine(QPointF(bounds.left() + kTopLineInset, lineY),
                     QPointF(bounds.right() - kTopLineInset, lineY));
    painter.setOpacity(1.0);

    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 28, QFont::Bold);
    painter.setFont(valueFont);
    painter.setPen(QColor(QStringLiteral("#F2FBFF")));
    const QString valueText = QString::number(m_tiltValue, 'f', 2) + QStringLiteral("°");
    const QRectF valueRect(bounds.left(),
                           bounds.top() + kValueTopMargin,
                           bounds.width(),
                           bounds.height() * 0.45);
    painter.drawText(valueRect, Qt::AlignHCenter | Qt::AlignVCenter, valueText);

    QFont axisFont(QStringLiteral("Noto Sans CJK SC"), 14, QFont::Bold);
    painter.setFont(axisFont);
    painter.setPen(QColor(QStringLiteral("#A8EAFF")));
    const QRectF axisRect(bounds.left(),
                          bounds.bottom() - kAxisBottomMargin - 22.0,
                          bounds.width(),
                          22.0);
    painter.drawText(axisRect, Qt::AlignHCenter | Qt::AlignVCenter, m_axisLabel);
}
