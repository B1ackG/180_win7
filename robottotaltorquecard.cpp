#include "robottotaltorquecard.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr int kOuterRadius = 14;
constexpr int kInnerRadius = 12;
const QColor kDangerRed(255, 51, 51); // 与角度力控超限同一套红
} // namespace

RobotTotalTorqueCard::RobotTotalTorqueCard(QWidget *parent)
    : QWidget(parent)
    , m_title(QStringLiteral("立柱升降扭矩"))
    , m_unit(QStringLiteral("%"))
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void RobotTotalTorqueCard::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    update();
}

void RobotTotalTorqueCard::setUnit(const QString &unit)
{
    if (m_unit == unit) {
        return;
    }
    m_unit = unit;
    update();
}

void RobotTotalTorqueCard::setValue(double percent, bool valid)
{
    const double clamped = qMax(0.0, percent);
    const bool unchanged = (m_valid == valid)
        && qFuzzyCompare(m_percent + 1.0, clamped + 1.0);

    m_percent = clamped;
    m_valid = valid;
    if (valid) {
        appendSample(clamped);
    }
    if (!unchanged || valid) {
        update();
    }
}

void RobotTotalTorqueCard::setDangerThreshold(double percent)
{
    const double clamped = qBound(1.0, percent, 999.0);
    if (qFuzzyCompare(m_dangerThreshold + 1.0, clamped + 1.0)) {
        return;
    }
    m_dangerThreshold = clamped;
    double localMax = qMax(100.0, m_dangerThreshold);
    for (double v : m_samples) {
        localMax = qMax(localMax, v);
    }
    m_maxDisplayPercent = qMax(100.0, localMax * 1.2);
    update();
}

void RobotTotalTorqueCard::setDecimals(int decimals)
{
    const int next = qBound(0, decimals, 2);
    if (m_decimals == next) {
        return;
    }
    m_decimals = next;
    update();
}

bool RobotTotalTorqueCard::isDanger() const
{
    return m_valid && m_percent >= m_dangerThreshold;
}

void RobotTotalTorqueCard::appendSample(double percent)
{
    m_samples.push_back(percent);
    while (m_samples.size() > m_maxSamples) {
        m_samples.pop_front();
    }

    double localMax = qMax(100.0, m_dangerThreshold);
    for (double v : m_samples) {
        localMax = qMax(localMax, v);
    }
    m_maxDisplayPercent = qMax(100.0, localMax * 1.2);
}

QSize RobotTotalTorqueCard::sizeHint() const
{
    return QSize(280, 100);
}

QSize RobotTotalTorqueCard::minimumSizeHint() const
{
    return QSize(200, 86);
}

void RobotTotalTorqueCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    if (r.width() <= 4.0 || r.height() <= 4.0) {
        return;
    }

    const bool danger = isDanger();
    const QColor lineCyan(QStringLiteral("#6FE7FF"));
    const QColor glowCyan(111, 231, 255, 70);
    const QColor lineColor = danger ? QColor(QStringLiteral("#FF6B5A")) : lineCyan;
    const QColor glowColor = danger ? QColor(255, 80, 70, 80) : glowCyan;

    // Panel shell — aligned with RobotTotalPowerCard
    QLinearGradient panelGradient(r.topLeft(), r.bottomLeft());
    if (danger) {
        panelGradient.setColorAt(0.0, QColor(58, 16, 16, 226));
        panelGradient.setColorAt(1.0, QColor(34, 8, 8, 218));
        painter.setPen(QPen(QColor(255, 85, 85, 170), 1.0));
    } else {
        panelGradient.setColorAt(0.0, QColor(8, 34, 58, 226));
        panelGradient.setColorAt(1.0, QColor(4, 18, 34, 218));
        painter.setPen(QPen(QColor(74, 190, 238, 132), 1.0));
    }
    painter.setBrush(panelGradient);
    painter.drawRoundedRect(r, kOuterRadius, kOuterRadius);

    const QRectF inner = r.adjusted(2.0, 2.0, -2.0, -2.0);
    painter.setPen(QPen(danger ? QColor(255, 140, 120, 90) : QColor(122, 224, 255, 74), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner, kInnerRadius, kInnerRadius);

    painter.setOpacity(0.55);
    painter.setPen(QPen(danger ? QColor(255, 140, 120, 140) : QColor(111, 231, 255, 120), 1.0));
    const qreal topLineY = r.top() + 8.0;
    painter.drawLine(QPointF(r.left() + 10.0, topLineY),
                     QPointF(r.right() - 10.0, topLineY));
    painter.setOpacity(1.0);

    // Title accent bar
    painter.setPen(Qt::NoPen);
    painter.setBrush(danger ? QColor(255, 90, 80, 210) : QColor(92, 225, 255, 210));
    painter.drawRoundedRect(QRectF(r.left() + 12.0, r.top() + 14.0, 3.0, 12.0), 1.5, 1.5);

    QFont titleFont(QStringLiteral("Noto Sans CJK SC"), 12, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(danger ? QColor(QStringLiteral("#FFD0D0")) : QColor(QStringLiteral("#A8EAFF")));
    const qreal titleWidth = painter.fontMetrics().horizontalAdvance(m_title);
    painter.drawText(QRectF(r.left() + 20.0, r.top() + 10.0, titleWidth + 4.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_title);

    const QString badgeText = danger ? QStringLiteral("超限") : QStringLiteral("实时");
    const QRectF liveBadge(r.left() + 20.0 + titleWidth + 8.0,
                           r.top() + 12.0,
                           36.0,
                           16.0);
    QLinearGradient badgeGrad(liveBadge.topLeft(), liveBadge.topRight());
    if (danger) {
        badgeGrad.setColorAt(0.0, QColor(QStringLiteral("#FF5555")));
        badgeGrad.setColorAt(1.0, QColor(QStringLiteral("#CC3333")));
    } else {
        badgeGrad.setColorAt(0.0, QColor(QStringLiteral("#5CE1FF")));
        badgeGrad.setColorAt(1.0, QColor(QStringLiteral("#3BB8E8")));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeGrad);
    painter.drawRoundedRect(liveBadge, 5.0, 5.0);

    QFont badgeFont(QStringLiteral("Noto Sans CJK SC"), 9, QFont::Bold);
    painter.setFont(badgeFont);
    painter.setPen(danger ? QColor(QStringLiteral("#2A0808")) : QColor(QStringLiteral("#0B2A3F")));
    painter.drawText(liveBadge, Qt::AlignCenter, badgeText);

    const QString numberText = m_valid
        ? QString::number(m_percent, 'f', m_decimals)
        : QStringLiteral("--");
    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 22, QFont::Bold);
    painter.setFont(valueFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal numberWidth = valueMetrics.horizontalAdvance(numberText);

    QFont unitFont(QStringLiteral("Noto Sans CJK SC"), 11, QFont::Bold);
    const QFontMetricsF unitMetrics(unitFont);
    const qreal unitWidth = unitMetrics.horizontalAdvance(m_unit);
    const qreal valueRight = r.right() - 14.0;
    const qreal valueBaseline = r.top() + 28.0;

    painter.setFont(unitFont);
    painter.setPen(danger ? QColor(255, 120, 110, 230) : QColor(QStringLiteral("#7EC8E8")));
    painter.drawText(QPointF(valueRight - unitWidth, valueBaseline), m_unit);

    painter.setFont(valueFont);
    painter.setPen(danger ? QColor(QStringLiteral("#FFE8E8")) : QColor(QStringLiteral("#F2FBFF")));
    painter.drawText(QPointF(valueRight - unitWidth - 4.0 - numberWidth, valueBaseline), numberText);

    const QRectF chartCell(r.left() + 10.0, r.top() + 38.0, r.width() - 20.0, r.height() - 48.0);
    if (chartCell.width() <= 8.0 || chartCell.height() <= 8.0) {
        return;
    }

    QLinearGradient cellGrad(chartCell.topLeft(), chartCell.bottomLeft());
    if (danger) {
        cellGrad.setColorAt(0.0, QColor(92, 28, 28, 130));
        cellGrad.setColorAt(1.0, QColor(60, 16, 16, 100));
        painter.setPen(QPen(QColor(220, 90, 80, 90), 1.0));
    } else {
        cellGrad.setColorAt(0.0, QColor(18, 58, 92, 120));
        cellGrad.setColorAt(1.0, QColor(10, 36, 60, 90));
        painter.setPen(QPen(QColor(90, 180, 220, 70), 1.0));
    }
    painter.setBrush(cellGrad);
    painter.drawRoundedRect(chartCell, 8.0, 8.0);

    const QRectF chartRect = chartCell.adjusted(8.0, 6.0, -8.0, -6.0);
    if (chartRect.width() <= 4.0 || chartRect.height() <= 4.0) {
        return;
    }

    double peak = 0.0;
    for (double sample : m_samples) {
        peak = qMax(peak, sample);
    }

    QFont scaleFont(QStringLiteral("Noto Sans CJK SC"), 9, QFont::Normal);
    painter.setFont(scaleFont);
    painter.setPen(danger ? QColor(QStringLiteral("#FFB0A8")) : QColor(QStringLiteral("#6FB8D8")));
    painter.drawText(QRectF(chartRect.right() - 72.0, chartCell.top() + 2.0, 68.0, 14.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("峰值 %1%").arg(qRound(peak)));

    QPen gridPen(danger ? QColor(190, 90, 80, 90) : QColor(90, 154, 190, 90), 1.0, Qt::DashLine);
    painter.setPen(gridPen);
    for (int i = 0; i < 3; ++i) {
        const qreal y = chartRect.top() + (chartRect.height() * i / 2.0);
        painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
    }

    if (m_samples.isEmpty()) {
        painter.setPen(danger ? QColor(QStringLiteral("#FFB0A8")) : QColor(QStringLiteral("#6FB8D8")));
        painter.drawText(chartRect, Qt::AlignCenter, QStringLiteral("等待扭矩数据…"));
        return;
    }

    const double displayMax = qMax(1.0, m_maxDisplayPercent);
    const qreal dangerRatio = qBound(0.0, m_dangerThreshold / displayMax, 1.0);
    const qreal dangerY = chartRect.bottom() - chartRect.height() * dangerRatio;
    painter.setPen(QPen(kDangerRed, 1.2, Qt::DashLine));
    painter.drawLine(QPointF(chartRect.left(), dangerY), QPointF(chartRect.right(), dangerY));

    QVector<QPointF> points;
    points.reserve(m_samples.size());
    const int span = qMax(1, m_samples.size() - 1);
    for (int i = 0; i < m_samples.size(); ++i) {
        const qreal x = chartRect.left() + chartRect.width() * i / span;
        const qreal ratio = qBound(0.0, m_samples[i] / displayMax, 1.0);
        const qreal y = chartRect.bottom() - chartRect.height() * ratio;
        points.push_back(QPointF(x, y));
    }
    if (points.size() == 1) {
        points.push_back(QPointF(chartRect.right(), points.first().y()));
    }

    QPainterPath areaPath;
    areaPath.moveTo(chartRect.left(), chartRect.bottom());
    for (const QPointF &p : points) {
        areaPath.lineTo(p);
    }
    areaPath.lineTo(chartRect.right(), chartRect.bottom());
    areaPath.closeSubpath();

    QLinearGradient areaGrad(chartRect.topLeft(), chartRect.bottomLeft());
    if (danger) {
        areaGrad.setColorAt(0.0, QColor(255, 70, 55, 110));
        areaGrad.setColorAt(1.0, QColor(140, 20, 12, 12));
    } else {
        areaGrad.setColorAt(0.0, QColor(0, 176, 232, 110));
        areaGrad.setColorAt(1.0, QColor(0, 92, 140, 12));
    }
    painter.fillPath(areaPath, areaGrad);

    QPainterPath linePath;
    linePath.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i) {
        linePath.lineTo(points[i]);
    }

    painter.setPen(QPen(glowColor, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(linePath);

    QPen linePen(lineColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(linePen);
    painter.drawPath(linePath);

    const QPointF tip = points.last();
    QRadialGradient tipGlow(tip, 8.0);
    if (danger) {
        tipGlow.setColorAt(0.0, QColor(255, 90, 80, 200));
        tipGlow.setColorAt(1.0, QColor(255, 90, 80, 0));
    } else {
        tipGlow.setColorAt(0.0, QColor(111, 231, 255, 200));
        tipGlow.setColorAt(1.0, QColor(111, 231, 255, 0));
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(tipGlow);
    painter.drawEllipse(tip, 7.0, 7.0);

    painter.setBrush(QColor(QStringLiteral("#F2FBFF")));
    painter.drawEllipse(tip, 2.6, 2.6);
}
