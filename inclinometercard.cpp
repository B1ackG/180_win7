#include "inclinometercard.h"

#include <QPaintEvent>
#include <QPainter>

namespace {
constexpr int kOuterRadius = 14;
constexpr int kInnerRadius = 12;
constexpr qreal kTiltVisualRange = 15.0; // bubble maps ±15°
} // namespace

InclinometerCard::InclinometerCard(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    syncAccentFromLabel();
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
    syncAccentFromLabel();
    emit axisLabelChanged();
    update();
}

void InclinometerCard::setAccentColor(const QColor &color)
{
    if (m_accentColor == color) {
        return;
    }
    m_accentColor = color;
    update();
}

void InclinometerCard::syncAccentFromLabel()
{
    if (m_axisLabel.startsWith(QLatin1Char('Y'), Qt::CaseInsensitive)
        || m_axisLabel.contains(QStringLiteral("Y轴"))) {
        m_accentColor = QColor(QStringLiteral("#6FE7A8"));
    } else {
        m_accentColor = QColor(QStringLiteral("#5CE1FF"));
    }
}

QString InclinometerCard::axisLetter() const
{
    if (m_axisLabel.startsWith(QLatin1Char('Y'), Qt::CaseInsensitive)
        || m_axisLabel.contains(QStringLiteral("Y轴"))) {
        return QStringLiteral("Y");
    }
    if (m_axisLabel.startsWith(QLatin1Char('X'), Qt::CaseInsensitive)
        || m_axisLabel.contains(QStringLiteral("X轴"))) {
        return QStringLiteral("X");
    }
    return m_axisLabel.left(1).toUpper();
}

QSize InclinometerCard::sizeHint() const
{
    return QSize(140, 100);
}

QSize InclinometerCard::minimumSizeHint() const
{
    return QSize(120, 86);
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

    // Panel shell — aligned with DeviceCoordPanel
    QLinearGradient panelGradient(bounds.topLeft(), bounds.bottomLeft());
    panelGradient.setColorAt(0.0, QColor(8, 34, 58, 226));
    panelGradient.setColorAt(1.0, QColor(4, 18, 34, 218));
    painter.setPen(QPen(QColor(74, 190, 238, 132), 1.0));
    painter.setBrush(panelGradient);
    painter.drawRoundedRect(bounds, kOuterRadius, kOuterRadius);

    const QRectF inner = bounds.adjusted(2.0, 2.0, -2.0, -2.0);
    painter.setPen(QPen(QColor(122, 224, 255, 74), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner, kInnerRadius, kInnerRadius);

    painter.setOpacity(0.55);
    painter.setPen(QPen(QColor(111, 231, 255, 120), 1.0));
    const qreal lineY = bounds.top() + 8.0;
    painter.drawLine(QPointF(bounds.left() + 10.0, lineY),
                     QPointF(bounds.right() - 10.0, lineY));
    painter.setOpacity(1.0);

    // Title accent bar
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 210));
    painter.drawRoundedRect(QRectF(bounds.left() + 12.0, bounds.top() + 14.0, 3.0, 12.0), 1.5, 1.5);

    // Title
    QFont titleFont(QStringLiteral("Noto Sans CJK SC"), 11, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#A8EAFF")));
    painter.drawText(QRectF(bounds.left() + 20.0, bounds.top() + 10.0, bounds.width() - 54.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_axisLabel);

    // Axis badge
    const QRectF badgeRect(bounds.right() - 30.0, bounds.top() + 12.0, 18.0, 18.0);
    painter.setPen(QPen(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 170), 1.0));
    painter.setBrush(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 40));
    painter.drawRoundedRect(badgeRect, 5.0, 5.0);

    QFont badgeFont(QStringLiteral("Noto Sans CJK SC"), 10, QFont::Bold);
    painter.setFont(badgeFont);
    painter.setPen(m_accentColor);
    painter.drawText(badgeRect, Qt::AlignCenter, axisLetter());

    // Value cell
    const QRectF valueCell(bounds.left() + 10.0,
                           bounds.top() + 34.0,
                           bounds.width() - 20.0,
                           bounds.height() - 58.0);
    if (valueCell.height() > 12.0) {
        QLinearGradient cellGrad(valueCell.topLeft(), valueCell.bottomLeft());
        cellGrad.setColorAt(0.0, QColor(18, 58, 92, 120));
        cellGrad.setColorAt(1.0, QColor(10, 36, 60, 90));
        painter.setPen(QPen(QColor(90, 180, 220, 70), 1.0));
        painter.setBrush(cellGrad);
        painter.drawRoundedRect(valueCell, 8.0, 8.0);
    }

    // Value + unit
    const QString numberText = QString::number(m_tiltValue, 'f', 2);
    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 22, QFont::Bold);
    painter.setFont(valueFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal numberWidth = valueMetrics.horizontalAdvance(numberText);

    QFont unitFont(QStringLiteral("Noto Sans CJK SC"), 12, QFont::Bold);
    painter.setFont(unitFont);
    const QFontMetricsF unitMetrics(unitFont);
    const qreal unitWidth = unitMetrics.horizontalAdvance(QStringLiteral("°"));
    const qreal totalWidth = numberWidth + 2.0 + unitWidth;
    const qreal valueBaselineY = valueCell.center().y() + valueMetrics.ascent() * 0.35;
    const qreal valueStartX = valueCell.center().x() - totalWidth * 0.5;

    painter.setFont(valueFont);
    painter.setPen(QColor(QStringLiteral("#F2FBFF")));
    painter.drawText(QPointF(valueStartX, valueBaselineY), numberText);

    painter.setFont(unitFont);
    painter.setPen(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 220));
    painter.drawText(QPointF(valueStartX + numberWidth + 2.0,
                             valueBaselineY - (valueMetrics.ascent() - unitMetrics.ascent()) * 0.35),
                     QStringLiteral("°"));

    // Bubble level
    const QRectF levelRect(bounds.left() + 14.0,
                           bounds.bottom() - 18.0,
                           bounds.width() - 28.0,
                           8.0);
    if (levelRect.width() > 20.0) {
        painter.setPen(QPen(QColor(90, 180, 220, 80), 1.0));
        painter.setBrush(QColor(8, 28, 48, 160));
        painter.drawRoundedRect(levelRect, 4.0, 4.0);

        // Center tick
        const qreal midX = levelRect.center().x();
        painter.setPen(QPen(QColor(111, 231, 255, 140), 1.0));
        painter.drawLine(QPointF(midX, levelRect.top() + 1.0),
                         QPointF(midX, levelRect.bottom() - 1.0));

        const qreal ratio = qBound(-1.0, m_tiltValue / kTiltVisualRange, 1.0);
        const qreal bubbleRadius = 3.2;
        const qreal travel = (levelRect.width() * 0.5) - bubbleRadius - 2.0;
        const QPointF bubbleCenter(midX + ratio * travel, levelRect.center().y());

        QRadialGradient bubbleGrad(bubbleCenter, bubbleRadius * 1.6);
        bubbleGrad.setColorAt(0.0, QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 240));
        bubbleGrad.setColorAt(1.0, QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), 40));
        painter.setPen(Qt::NoPen);
        painter.setBrush(bubbleGrad);
        painter.drawEllipse(bubbleCenter, bubbleRadius * 1.5, bubbleRadius * 1.5);

        painter.setBrush(m_accentColor);
        painter.drawEllipse(bubbleCenter, bubbleRadius, bubbleRadius);
    }
}
