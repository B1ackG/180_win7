#include "robottotaltorquecard.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <cmath>

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
    // 扭矩有方向：负值表示反向，不做 0 截断，原样显示
    const double normalized = std::isfinite(percent) ? percent : 0.0;
    const bool unchanged = (m_valid == valid)
        && qFuzzyCompare(m_percent + 1.0, normalized + 1.0);

    m_percent = normalized;
    m_valid = valid;
    if (valid) {
        appendSample(normalized);
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
    recomputeDisplayRange();
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
    // 超限按绝对值判断，反向满载同样算超限
    return m_valid && qAbs(m_percent) >= m_dangerThreshold;
}

void RobotTotalTorqueCard::appendSample(double percent)
{
    m_samples.push_back(percent);
    while (m_samples.size() > m_maxSamples) {
        m_samples.pop_front();
    }

    recomputeDisplayRange();
}

void RobotTotalTorqueCard::recomputeDisplayRange()
{
    double high = qMax(100.0, m_dangerThreshold);
    double low = 0.0;
    for (double v : m_samples) {
        high = qMax(high, v);
        low = qMin(low, v);
    }

    // 始终包含 0 基线；负向至少要留出 1% 的可视高度，避免微小负值把曲线压平
    m_maxDisplayPercent = qMax(100.0, high * 1.2);
    m_minDisplayPercent = (low < 0.0) ? qMin(-1.0, low * 1.2) : 0.0;
    if (m_maxDisplayPercent - m_minDisplayPercent < 1.0) {
        m_maxDisplayPercent = m_minDisplayPercent + 1.0;
    }
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
    // 负值比正值多一个符号位，按可用宽度自动缩字号，避免和“实时/超限”徽标叠字
    QFont valueFont(QStringLiteral("Noto Sans CJK SC"), 22, QFont::Bold);
    QFontMetricsF valueMetrics(valueFont);
    qreal numberWidth = valueMetrics.horizontalAdvance(numberText);

    QFont unitFont(QStringLiteral("Noto Sans CJK SC"), 11, QFont::Bold);
    const QFontMetricsF unitMetrics(unitFont);
    const qreal unitWidth = unitMetrics.horizontalAdvance(m_unit);
    const qreal valueRight = r.right() - 14.0;
    const qreal valueBaseline = r.top() + 28.0;
    const qreal valueLeftLimit = liveBadge.right() + 8.0;
    const qreal valueSpace = valueRight - unitWidth - 4.0 - valueLeftLimit;
    while (numberWidth > valueSpace && valueFont.pointSize() > 13) {
        valueFont.setPointSize(valueFont.pointSize() - 1);
        valueMetrics = QFontMetricsF(valueFont);
        numberWidth = valueMetrics.horizontalAdvance(numberText);
    }

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

    // 峰谷按窗口内实际样本统计，纯负值窗口不会把 0 当成峰值
    double peakHigh = m_samples.isEmpty() ? 0.0 : m_samples.first();
    double peakLow = peakHigh;
    for (double sample : m_samples) {
        peakHigh = qMax(peakHigh, sample);
        peakLow = qMin(peakLow, sample);
    }

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

    // 纵向映射以 0 为基线：正值向上、负值向下
    const double displayMin = qMin(0.0, m_minDisplayPercent);
    const double displayMax = qMax(displayMin + 1.0, m_maxDisplayPercent);
    const double displaySpan = displayMax - displayMin;
    auto yForValue = [&](double value) {
        const double ratio = qBound(0.0, (value - displayMin) / displaySpan, 1.0);
        return chartRect.bottom() - chartRect.height() * ratio;
    };
    const qreal zeroY = yForValue(0.0);

    // 超限线按绝对值画，正反向各一条（落在可视范围内的才画）
    painter.setPen(QPen(kDangerRed, 1.2, Qt::DashLine));
    if (m_dangerThreshold <= displayMax) {
        const qreal dangerY = yForValue(m_dangerThreshold);
        painter.drawLine(QPointF(chartRect.left(), dangerY), QPointF(chartRect.right(), dangerY));
    }
    if (-m_dangerThreshold >= displayMin) {
        const qreal reverseDangerY = yForValue(-m_dangerThreshold);
        painter.drawLine(QPointF(chartRect.left(), reverseDangerY),
                         QPointF(chartRect.right(), reverseDangerY));
    }

    if (displayMin < 0.0) {
        painter.setPen(QPen(danger ? QColor(255, 150, 140, 175) : QColor(126, 200, 232, 175), 1.0));
        painter.drawLine(QPointF(chartRect.left(), zeroY), QPointF(chartRect.right(), zeroY));
    }

    QVector<QPointF> points;
    points.reserve(m_samples.size());
    const int span = qMax(1, m_samples.size() - 1);
    for (int i = 0; i < m_samples.size(); ++i) {
        const qreal x = chartRect.left() + chartRect.width() * i / span;
        points.push_back(QPointF(x, yForValue(m_samples[i])));
    }
    if (points.size() == 1) {
        points.push_back(QPointF(chartRect.right(), points.first().y()));
    }

    // 面积填充到 0 基线，负值区域自然落在基线下方
    QPainterPath areaPath;
    areaPath.moveTo(points.first().x(), zeroY);
    for (const QPointF &p : points) {
        areaPath.lineTo(p);
    }
    areaPath.lineTo(points.last().x(), zeroY);
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

    // 峰谷提示：右上角小胶囊，先铺底再压字，保证可读
    // 出现负值时只提示谷值（反向最深处），全为正时仍提示峰值
    QFont scaleFont(QStringLiteral("Noto Sans CJK SC"), 8, QFont::Normal);
    painter.setFont(scaleFont);
    const QFontMetricsF scaleMetrics(scaleFont);
    const QString peakText = (peakLow < 0.0)
        ? QStringLiteral("谷值 %1%").arg(qRound(peakLow))
        : QStringLiteral("峰值 %1%").arg(qRound(peakHigh));
    const qreal peakTextWidth = scaleMetrics.horizontalAdvance(peakText);
    const QRectF peakChip(chartCell.right() - 6.0 - peakTextWidth - 10.0,
                          chartCell.top() + 2.0,
                          peakTextWidth + 10.0,
                          13.0);
    if (peakChip.left() > chartCell.left() + 4.0) {
        painter.setPen(QPen(danger ? QColor(255, 140, 120, 90) : QColor(110, 200, 235, 80), 1.0));
        painter.setBrush(danger ? QColor(64, 16, 16, 190) : QColor(4, 18, 32, 190));
        painter.drawRoundedRect(peakChip, 4.0, 4.0);
        painter.setPen(danger ? QColor(QStringLiteral("#FFB0A8")) : QColor(QStringLiteral("#7EC8E8")));
        painter.drawText(peakChip, Qt::AlignCenter, peakText);
    }

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
