#ifndef TECHZONEPANEL_H
#define TECHZONEPANEL_H

#include <QWidget>
#include <QColor>
#include <QString>
#include <QPixmap>
#include <QMargins>

class TechZonePanel : public QWidget
{
    Q_OBJECT

public:
    enum AccentStyle {
        AccentCyan,
        AccentRed,
        AccentNeutral
    };
    Q_ENUM(AccentStyle)

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(int chamferSize READ chamferSize WRITE setChamferSize NOTIFY chamferSizeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString backgroundImage READ backgroundImage WRITE setBackgroundImage NOTIFY backgroundImageChanged)
    Q_PROPERTY(int imageBorderMargin READ imageBorderMargin WRITE setImageBorderMargin NOTIFY imageBorderMarginChanged)
    Q_PROPERTY(AccentStyle accentStyle READ accentStyle WRITE setAccentStyle NOTIFY accentStyleChanged)
    Q_PROPERTY(bool designOutline READ designOutline WRITE setDesignOutline NOTIFY designOutlineChanged)

    explicit TechZonePanel(QWidget *parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    int chamferSize() const { return m_chamferSize; }
    void setChamferSize(int size);

    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &color);

    QString backgroundImage() const { return m_backgroundImagePath; }
    void setBackgroundImage(const QString &resourcePath);

    int imageBorderMargin() const { return m_imageBorderMargins.left(); }
    void setImageBorderMargin(int margin);

    AccentStyle accentStyle() const { return m_accentStyle; }
    void setAccentStyle(AccentStyle style);

    void setImageBorderMargins(const QMargins &margins);

    bool designOutline() const { return m_designOutline; }
    void setDesignOutline(bool enabled);

signals:
    void titleChanged();
    void chamferSizeChanged();
    void accentColorChanged();
    void backgroundImageChanged();
    void imageBorderMarginChanged();
    void accentStyleChanged();
    void designOutlineChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static void drawNinePatch(QPainter &painter, const QPixmap &pixmap,
                              const QRect &target, const QMargins &margins);
    void applyAccentStyle(AccentStyle style);
    void paintVectorFrame(QPainter &painter, const QRectF &bounds);
    void paintTitle(QPainter &painter, const QRectF &bounds);
    void paintDesignOutline(QPainter &painter, const QRectF &bounds);
    void syncDesignBorderImageStyle();

    QString m_title;
    QString m_backgroundImagePath;
    int m_chamferSize = 12;
    AccentStyle m_accentStyle = AccentCyan;
    QColor m_accentColor = QColor(0, 200, 255);
    QColor m_glowColor = QColor(0, 255, 255, 120);
    QColor m_titleColor = QColor(0xc0, 0xe8, 0xff);
    QColor m_fillTop = QColor(12, 34, 58, 184);
    QColor m_fillBottom = QColor(8, 24, 42, 163);
    QPixmap m_backgroundPixmap;
    QMargins m_imageBorderMargins{50, 50, 50, 50};
    bool m_designOutline = true;
};

#endif // TECHZONEPANEL_H
