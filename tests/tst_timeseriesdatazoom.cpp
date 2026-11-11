#include "timeseriesdatazoom.h"

#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QtTest>

class TestTimeSeriesDataZoom : public QObject
{
    Q_OBJECT

private slots:
    void clampsProgrammaticRange();
    void enforcesMinimumSpan();
    void leftHandleDoesNotCrossOrLeaveBounds();
    void rightHandleDoesNotCrossOrLeaveBounds();
    void windowDragKeepsSpanAndStaysInBounds();
};

namespace {

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &pos, Qt::MouseButton button = Qt::LeftButton)
{
    QMouseEvent event(type, pos, widget->mapToGlobal(pos), button,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : button,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

} // namespace

void TestTimeSeriesDataZoom::clampsProgrammaticRange()
{
    TimeSeriesDataZoom zoom;
    zoom.setRange(0, 10000);
    zoom.setSelectedRange(-50, 12000);
    QCOMPARE(zoom.selectedRange().first, 0);
    QCOMPARE(zoom.selectedRange().second, 10000);
}

void TestTimeSeriesDataZoom::enforcesMinimumSpan()
{
    TimeSeriesDataZoom zoom;
    zoom.setRange(0, 10000);
    zoom.setMinimumSpan(1);
    zoom.setSelectedRange(5000, 5000);
    QVERIFY(zoom.selectedRange().second - zoom.selectedRange().first >= 1);
    zoom.setSelectedRange(10000, 10000);
    QCOMPARE(zoom.selectedRange().first, 9999);
    QCOMPARE(zoom.selectedRange().second, 10000);
    zoom.setSelectedRange(0, 0);
    QCOMPARE(zoom.selectedRange().first, 0);
    QCOMPARE(zoom.selectedRange().second, 1);
}

void TestTimeSeriesDataZoom::leftHandleDoesNotCrossOrLeaveBounds()
{
    TimeSeriesDataZoom zoom;
    zoom.setRange(0, 10000);
    zoom.setSelectedRange(3000, 7000);
    zoom.resize(500, 60);
    zoom.show();
    QVERIFY(QTest::qWaitForWindowExposed(&zoom));

    const QPoint start(static_cast<int>(8 + 484 * 0.30), zoom.height() / 2);
    sendMouse(&zoom, QEvent::MouseButtonPress, start);
    sendMouse(&zoom, QEvent::MouseMove, QPoint(-80, zoom.height() / 2));
    sendMouse(&zoom, QEvent::MouseButtonRelease, QPoint(-80, zoom.height() / 2));

    QVERIFY(zoom.selectedRange().first >= 0);
    QVERIFY(zoom.selectedRange().first < zoom.selectedRange().second);
    QVERIFY(zoom.selectedRange().second - zoom.selectedRange().first >= zoom.minimumSpan());
    QCOMPARE(zoom.selectedRange().second, 7000);
}

void TestTimeSeriesDataZoom::rightHandleDoesNotCrossOrLeaveBounds()
{
    TimeSeriesDataZoom zoom;
    zoom.setRange(0, 10000);
    zoom.setSelectedRange(3000, 7000);
    zoom.resize(500, 60);
    zoom.show();
    QVERIFY(QTest::qWaitForWindowExposed(&zoom));

    const QPoint start(static_cast<int>(8 + 484 * 0.70), zoom.height() / 2);
    sendMouse(&zoom, QEvent::MouseButtonPress, start);
    sendMouse(&zoom, QEvent::MouseMove, QPoint(zoom.width() + 80, zoom.height() / 2));
    sendMouse(&zoom, QEvent::MouseButtonRelease, QPoint(zoom.width() + 80, zoom.height() / 2));

    QCOMPARE(zoom.selectedRange().second, 10000);
    QVERIFY(zoom.selectedRange().first < zoom.selectedRange().second);
    QVERIFY(zoom.selectedRange().first >= 0);
}

void TestTimeSeriesDataZoom::windowDragKeepsSpanAndStaysInBounds()
{
    TimeSeriesDataZoom zoom;
    zoom.setRange(0, 10000);
    zoom.setSelectedRange(2000, 5000);
    const int span = 3000;
    zoom.resize(500, 60);
    zoom.show();
    QVERIFY(QTest::qWaitForWindowExposed(&zoom));

    QSignalSpy spy(&zoom, &TimeSeriesDataZoom::selectedRangeChanged);
    const QPoint start(static_cast<int>(8 + 484 * 0.35), zoom.height() / 2);
    sendMouse(&zoom, QEvent::MouseButtonPress, start);
    sendMouse(&zoom, QEvent::MouseMove, QPoint(zoom.width() + 120, zoom.height() / 2));
    sendMouse(&zoom, QEvent::MouseButtonRelease, QPoint(zoom.width() + 120, zoom.height() / 2));

    QCOMPARE(zoom.selectedRange().second - zoom.selectedRange().first, span);
    QCOMPARE(zoom.selectedRange().second, 10000);
    QCOMPARE(zoom.selectedRange().first, 7000);
    QVERIFY(spy.count() >= 1);
}

QTEST_MAIN(TestTimeSeriesDataZoom)
#include "tst_timeseriesdatazoom.moc"
