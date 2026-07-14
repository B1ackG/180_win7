#include "techzonepanel.h"
#include "techshapes.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontMetricsF>
#include <QDebug>

TechZonePanel::TechZonePanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    applyAccentStyle(m_accentStyle);
    setBackgroundImage(QStringLiteral(":/Picture/zonePanel.png"));
}

void TechZonePanel::setBackgroundImage(const QString &resourcePath)
{
    if (resourcePath.isEmpty()) {
        return;
    }

    QPixmap pixmap(resourcePath);
    if (pixmap.isNull()) {
        qWarning() << "TechZonePanel: failed to load background image:" << resourcePath;
        return;
    }

    m_backgroundImagePath = resourcePath;
    m_backgroundPixmap = pixmap;
    emit backgroundImageChanged();
    syncDesignBorderImageStyle();
    update();
}

void TechZonePanel::setImageBorderMargins(const QMargins &margins)
{
    if (m_imageBorderMargins != margins) {
        m_imageBorderMargins = margins;
        emit imageBorderMarginChanged();
        syncDesignBorderImageStyle();
        update();
    }
}

void TechZonePanel::setImageBorderMargin(int margin)
{
    margin = qMax(0, margin);
    setImageBorderMargins(QMargins(margin, margin, margin, margin));
}

void TechZonePanel::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
        update();
    }
}

void TechZonePanel::setChamferSize(int size)
{
    size = qMax(4, size);
    if (m_chamferSize != size) {
        m_chamferSize = size;
        emit chamferSizeChanged();
        update();
    }
}

void TechZonePanel::setAccentColor(const QColor &color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        m_glowColor = QColor(color.red(), color.green(), color.blue(), 120);
        emit accentColorChanged();
        update();
    }
}

void TechZonePanel::setAccentStyle(AccentStyle style)
{
    if (m_accentStyle != style) {
        m_accentStyle = style;
        applyAccentStyle(style);
        emit accentStyleChanged();
        update();
    }
}

void TechZonePanel::setDesignOutline(bool enabled)
{
    if (m_designOutline != enabled) {
        m_designOutline = enabled;
        emit designOutlineChanged();
        if (!m_designOutline) {
            setStyleSheet(QString());
        } else {
            syncDesignBorderImageStyle();
        }
        update();
    }
}

void TechZonePanel::syncDesignBorderImageStyle()
{
    if (!m_designOutline || m_backgroundImagePath.isEmpty()) {
        return;
    }

    const int m = m_imageBorderMargins.left();
    const QString css = QStringLiteral(
        "border-image: url(%1) %2 %2 %2 %2 stretch stretch;")
                            .arg(m_backgroundImagePath)
                            .arg(m);
    const QString current = styleSheet().trimmed();
    if (current.isEmpty() || current.contains(QStringLiteral("border-image"))) {
        setStyleSheet(css);
    }
}

void TechZonePanel::applyAccentStyle(AccentStyle style)
{
    switch (style) {
    case AccentRed:
        m_accentColor = QColor(255, 120, 120);
        m_glowColor = QColor(255, 100, 100, 120);
        m_titleColor = QColor(0xff, 0xd0, 0xd0);
        m_fillTop = QColor(76, 18, 18, 179);
        m_fillBottom = QColor(56, 16, 16, 158);
        break;
    case AccentNeutral:
        m_accentColor = QColor(79, 175, 232);
        m_glowColor = QColor(79, 175, 232, 100);
        m_titleColor = QColor(0xa6, 0xd8, 0xff);
        m_fillTop = QColor(14, 38, 64, 179);
        m_fillBottom = QColor(10, 28, 48, 153);
        break;
    case AccentCyan:
    default:
        m_accentColor = QColor(0, 200, 255);
        m_glowColor = QColor(0, 255, 255, 120);
        m_titleColor = QColor(0xc0, 0xe8, 0xff);
        m_fillTop = QColor(12, 34, 58, 184);
        m_fillBottom = QColor(8, 24, 42, 163);
        break;
    }
}

void TechZonePanel::drawNinePatch(QPainter &painter, const QPixmap &pixmap,
                                  const QRect &target, const QMargins &margins)
{
    if (pixmap.isNull() || target.isEmpty()) {
        return;
    }

    const int srcW = pixmap.width();
    const int srcH = pixmap.height();
    int left = qBound(0, margins.left(), srcW / 2);
    int top = qBound(0, margins.top(), srcH / 2);
    int right = qBound(0, margins.right(), srcW / 2);
    int bottom = qBound(0, margins.bottom(), srcH / 2);

    const int centerSrcW = qMax(1, srcW - left - right);
    const int centerSrcH = qMax(1, srcH - top - bottom);

    const int centerDstW = qMax(0, target.width() - left - right);
    const int centerDstH = qMax(0, target.height() - top - bottom);

    const QRect srcTopLeft(0, 0, left, top);
    const QRect srcTopCenter(left, 0, centerSrcW, top);
    const QRect srcTopRight(srcW - right, 0, right, top);
    const QRect srcMiddleLeft(0, top, left, centerSrcH);
    const QRect srcMiddleCenter(left, top, centerSrcW, centerSrcH);
    const QRect srcMiddleRight(srcW - right, top, right, centerSrcH);
    const QRect srcBottomLeft(0, srcH - bottom, left, bottom);
    const QRect srcBottomCenter(left, srcH - bottom, centerSrcW, bottom);
    const QRect srcBottomRight(srcW - right, srcH - bottom, right, bottom);

    const QRect dstTopLeft(target.left(), target.top(), left, top);
    const QRect dstTopCenter(target.left() + left, target.top(), centerDstW, top);
    const QRect dstTopRight(target.right() - right + 1, target.top(), right, top);
    const QRect dstMiddleLeft(target.left(), target.top() + top, left, centerDstH);
    const QRect dstMiddleCenter(target.left() + left, target.top() + top, centerDstW, centerDstH);
    const QRect dstMiddleRight(target.right() - right + 1, target.top() + top, right, centerDstH);
    const QRect dstBottomLeft(target.left(), target.bottom() - bottom + 1, left, bottom);
    const QRect dstBottomCenter(target.left() + left, target.bottom() - bottom + 1, centerDstW, bottom);
    const QRect dstBottomRight(target.right() - right + 1, target.bottom() - bottom + 1, right, bottom);

    painter.drawPixmap(dstTopLeft, pixmap, srcTopLeft);
    painter.drawPixmap(dstTopCenter, pixmap, srcTopCenter);
    painter.drawPixmap( dstTopRight, pixmap, srcTopRight);
    painter.drawPixmap(dstMiddleLeft, pixmap, srcMiddleLeft);
    painter.drawPixmap(dstMiddleCenter, pixmap, srcMiddleCenter);
    painter.drawPixmap(dstMiddleRight, pixmap, srcMiddleRight);
    painter.drawPixmap(dstBottomLeft, pixmap, srcBottomLeft);
    painter.drawPixmap(dstBottomCenter, pixmap, srcBottomCenter);
    painter.drawPixmap(dstBottomRight, pixmap, srcBottomRight);
}

void TechZonePanel::paintVectorFrame(QPainter &painter, const QRectF &bounds)
{
    const qreal cut = m_chamferSize;
    const QPainterPath outerPath = TechShapes::chamferedRect(bounds, cut);

    QLinearGradient fillGrad(bounds.topLeft(), bounds.bottomLeft());
    fillGrad.setColorAt(0.0, m_fillTop);
    fillGrad.setColorAt(1.0, m_fillBottom);
    painter.fillPath(outerPath, fillGrad);

    QPen glowPen(m_glowColor, 3.0);
    glowPen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(glowPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(outerPath);

    QPen accentPen(m_accentColor, 1.0);
    accentPen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(accentPen);
    painter.drawPath(outerPath);

    const QRectF innerBounds = bounds.adjusted(2.0, 2.0, -2.0, -2.0);
    const QPainterPath innerPath = TechShapes::chamferedRect(innerBounds, qMax(0.0, cut - 2.0));
    QPen innerPen(QColor(255, 255, 255, 100), 1.0);
    innerPen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(innerPen);
    painter.drawPath(innerPath);
}

void TechZonePanel::paintTitle(QPainter &painter, const QRectF &bounds)
{
    if (m_title.isEmpty()) {
        return;
    }

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    titleFont.setPixelSize(13);
    painter.setFont(titleFont);

    QFontMetricsF metrics(titleFont);
    const qreal titleWidth = metrics.horizontalAdvance(m_title) + 24.0;
    const qreal titleHeight = metrics.height() + 6.0;
    const qreal titleX = bounds.center().x() - titleWidth / 2.0;
    const qreal titleY = bounds.top() + 6.0;

    QRectF titleBg(titleX, titleY, titleWidth, titleHeight);
    QPainterPath titlePath;
    titlePath.addRoundedRect(titleBg, 4.0, 4.0);
    painter.fillPath(titlePath, QColor(8, 24, 42, 235));

    QPen titleUnderline(m_accentColor);
    titleUnderline.setWidthF(1.0);
    painter.setPen(titleUnderline);
    painter.drawLine(QPointF(titleBg.left() + 8.0, titleBg.bottom() - 1.0),
                     QPointF(titleBg.right() - 8.0, titleBg.bottom() - 1.0));

    painter.setPen(m_titleColor);
    painter.drawText(titleBg, Qt::AlignCenter, m_title);
}

void TechZonePanel::paintDesignOutline(QPainter &painter, const QRectF &bounds)
{
    if (m_backgroundPixmap.isNull()) {
        painter.fillRect(bounds, QColor(0, 60, 120, 72));

        QFont hintFont = painter.font();
        hintFont.setPixelSize(14);
        hintFont.setBold(true);
        painter.setFont(hintFont);
        painter.setPen(QColor(0, 232, 255, 200));
        painter.drawText(bounds, Qt::AlignCenter, QStringLiteral("TechZonePanel"));
    }

    QPen outlinePen(QColor(0, 232, 255, 220));
    outlinePen.setStyle(Qt::DashLine);
    outlinePen.setWidthF(2.5);
    outlinePen.setDashPattern({6.0, 4.0});
    painter.setPen(outlinePen);
    painter.setBrush(QColor(0, 60, 120, 72));
    painter.drawRect(bounds.adjusted(1.0, 1.0, -1.0, -1.0));

    const QString label = !objectName().isEmpty() ? objectName() : m_title;
    if (label.isEmpty()) {
        return;
    }

    QFont labelFont = painter.font();
    labelFont.setPixelSize(12);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    QFontMetricsF metrics(labelFont);
    const qreal padH = 8.0;
    const qreal padV = 4.0;
    const QRectF labelBg(bounds.left() + 6.0, bounds.top() + 6.0,
                         metrics.horizontalAdvance(label) + padH * 2.0,
                         metrics.height() + padV * 2.0);
    painter.fillRect(labelBg, QColor(0, 24, 48, 230));
    painter.setPen(Qt::white);
    painter.drawText(labelBg, Qt::AlignCenter, label);
}

void TechZonePanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF bounds = rect();

    if (!m_backgroundPixmap.isNull()) {
        drawNinePatch(painter, m_backgroundPixmap, bounds.toRect(), m_imageBorderMargins);
    } else {
        paintVectorFrame(painter, bounds.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    paintTitle(painter, bounds);

    if (m_designOutline) {
        paintDesignOutline(painter, bounds);
    }
}
