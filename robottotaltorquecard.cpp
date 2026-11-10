#include "robottotaltorquecard.h"

#include <QPaintEvent>
#include <QPainter>
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
    if (m_valid == valid && qFuzzyCompare(m_percent + 1.0, clamped + 1.0)) {
        return;
    }
    m_percent = clamped;
    m_valid = valid;
    update();
}

void RobotTotalTorqueCard::setDangerThreshold(double percent)
{
    const double clamped = qBound(1.0, percent, 999.0);
    if (qFuzzyCompare(m_dangerThreshold + 1.0, clamped + 1.0)) {
        return;
    }
    m_dangerThreshold = clamped;
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
    const QColor accent = danger ? kDangerRed : QColor(QStringLiteral("#5CE1FF"));
    const QColor titleColor = danger ? QColor(QStringLiteral("#FFD0D0"))
                                     : QColor(QStringLiteral("#A8EAFF"));
    const QColor valueColor = danger ? QColor(QStringLiteral("#FFE8E8"))
                                     : QColor(QStringLiteral("#F2FBFF"));
    const QColor unitColor = danger ? QColor(accent.red(), accent.green(), accent.blue(), 230)
                                    : QColor(QStringLiteral("#7EC8E8"));

    QLinearGradient panelGradient(r.topLeft(), r.bottomLeft());
    if (danger) {
        panelGradient.setColorAt(0.0, QColor(58, 16, 16, 226));
        panelGradient.setColorAt(1.0, QColor(34, 8, 8, 218));
        painter.setPen(QPen(QColor(255, 85, 85, 170), 1.4));
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
    painter.drawLine(QPointF(r.left() + 10.0, r.top() + 8.0),
                     QPointF(r.right() - 10.0, r.top() + 8.0));
    painter.setOpacity(1.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 210));
    painter.drawRoundedRect(QRectF(r.left() + 12.0, r.top() + 14.0, 3.0, 12.0), 1.5, 1.5);

    QFont titleFont(QStringLiteral("Noto Sans CJK SC"), 12, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(titleColor);
    painter.drawText(QRectF(r.left() + 20.0, r.top() + 10.0, 160.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_title);

    const QString badgeText = danger ? QStringLiteral("超限") : QStringLiteral("实时");
    const QRectF liveBadge(r.left() + 20.0 + painter.fontMetrics().horizontalAdvance(m_title) + 8.0,
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
    painter.setPen(unitColor);
    painter.drawText(QPointF(valueRight - unitWidth, valueBaseline), m_unit);

    painter.setFont(valueFont);
    painter.setPen(valueColor);
    painter.drawText(QPointF(valueRight - unitWidth - 4.0 - numberWidth, valueBaseline), numberText);

    const QRectF barCell(r.left() + 10.0, r.top() + 38.0, r.width() - 20.0, r.height() - 48.0);
    if (barCell.width() <= 8.0 || barCell.height() <= 8.0) {
        return;
    }

    QLinearGradient cellGrad(barCell.topLeft(), barCell.bottomLeft());
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
    painter.drawRoundedRect(barCell, 8.0, 8.0);

    QFont scaleFont(QStringLiteral("Noto Sans CJK SC"), 9, QFont::Normal);
    painter.setFont(scaleFont);
    painter.setPen(danger ? QColor(QStringLiteral("#FFB0A8")) : QColor(QStringLiteral("#6FB8D8")));
    painter.drawText(QRectF(barCell.left() + 8.0, barCell.top() + 2.0, barCell.width() - 16.0, 14.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("危险阈值 %1%").arg(QString::number(m_dangerThreshold, 'f', 0)));

    if (!m_valid) {
        painter.drawText(barCell.adjusted(0, 8, 0, 0), Qt::AlignCenter, QStringLiteral("等待扭矩数据…"));
        return;
    }

    const QRectF barTrack(barCell.left() + 10.0,
                          barCell.center().y() + 2.0,
                          barCell.width() - 20.0,
                          10.0);
    if (barTrack.width() <= 8.0) {
        return;
    }

    painter.setPen(QPen(danger ? QColor(220, 90, 80, 90) : QColor(90, 180, 220, 80), 1.0));
    painter.setBrush(QColor(8, 28, 48, 160));
    painter.drawRoundedRect(barTrack, 5.0, 5.0);

    const double displayMax = qMax(200.0, m_dangerThreshold * 1.25);
    const qreal fillRatio = qBound(0.0, m_percent / displayMax, 1.0);
    if (fillRatio > 0.0) {
        const QRectF fillRect(barTrack.left() + 1.0,
                              barTrack.top() + 1.0,
                              qMax(4.0, (barTrack.width() - 2.0) * fillRatio),
                              barTrack.height() - 2.0);
        QLinearGradient fillGrad(fillRect.topLeft(), fillRect.topRight());
        if (danger) {
            fillGrad.setColorAt(0.0, QColor(255, 90, 70, 210));
            fillGrad.setColorAt(1.0, QColor(255, 51, 51, 240));
        } else {
            fillGrad.setColorAt(0.0, QColor(0, 168, 220, 210));
            fillGrad.setColorAt(1.0, QColor(111, 231, 255, 230));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillGrad);
        painter.drawRoundedRect(fillRect, 4.0, 4.0);
    }

    const qreal tickRatio = qBound(0.0, m_dangerThreshold / displayMax, 1.0);
    const qreal tickX = barTrack.left() + barTrack.width() * tickRatio;
    painter.setPen(QPen(kDangerRed, 1.4, Qt::DashLine));
    painter.drawLine(QPointF(tickX, barTrack.top() - 2.0),
                     QPointF(tickX, barTrack.bottom() + 2.0));
}
