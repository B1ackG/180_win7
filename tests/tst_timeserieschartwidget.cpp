#include "timeserieschartwidget.h"
#include "timeseriesdialog.h"
#include "timeseriesfullscreendialog.h"
#include "timeseriesqueryservice.h"

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDateEdit>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTime>
#include <QtMath>
#include <QtTest>

Q_DECLARE_METATYPE(TimeSeriesChannel)
Q_DECLARE_METATYPE(TimeSeriesChartWidget::InteractionMode)

class TestTimeSeriesChartWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void visibleRangeClampsOutOfBounds();
    void cursorDragDoesNotChangeVisibleRange();
    void panDragDoesNotChangeCursorTime();
    void cursorShowsDashInsideGap();
    void downsampledFourHoursStaysConnected();
    void fullscreenButtonEmitsChannel();
    void fullscreenSyncsRangeAndCursor();
    void defaultChannelSelectionAndLimit();
    void channelDrawerButtonSitsAboveCursor();
    void quickRangesUseLocalDates();
    void compareCursorShowsDashOnMissingDay();
};

namespace {

TimeSeriesQueryResult makeResult(TimeSeriesChannel channel,
                                 const QVector<TimeSeriesPoint> &points,
                                 int sampleIntervalMs = 1000)
{
    TimeSeriesSeries series;
    series.channel = channel;
    series.points = points;
    TimeSeriesQueryResult result;
    result.ok = true;
    result.sampleIntervalMs = sampleIntervalMs;
    result.series.append(series);
    if (!points.isEmpty()) {
        result.startUtcMs = points.first().tsUtcMs;
        result.endUtcMs = points.last().tsUtcMs;
    }
    return result;
}

TimeSeriesPoint pt(qint64 ts, double value)
{
    TimeSeriesPoint point;
    point.tsUtcMs = ts;
    point.value = value;
    return point;
}

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::MouseButtonRelease)
        ? Qt::NoButton
        : Qt::LeftButton;
    QMouseEvent event(type, pos, widget->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

TimeSeriesQueryResult twoHourSeries()
{
    QVector<TimeSeriesPoint> points;
    for (int i = 0; i <= 120; ++i) {
        points.append(pt(i * 1000, i));
    }
    return makeResult(TimeSeriesChannel::ArmTiltXDeg, points);
}

} // namespace

void TestTimeSeriesChartWidget::initTestCase()
{
    qRegisterMetaType<TimeSeriesChannel>("TimeSeriesChannel");
    qRegisterMetaType<TimeSeriesChartWidget::InteractionMode>("TimeSeriesChartWidget::InteractionMode");
}

void TestTimeSeriesChartWidget::visibleRangeClampsOutOfBounds()
{
    TimeSeriesChartWidget widget;
    widget.setSeries(twoHourSeries());
    widget.setVisibleRange(-5000, 500000);
    QCOMPARE(widget.visibleRange().first, widget.dataStartMs());
    QCOMPARE(widget.visibleRange().second, widget.dataEndMs());

    widget.setVisibleRange(90000, 200000);
    QVERIFY(widget.visibleRange().first >= widget.dataStartMs());
    QVERIFY(widget.visibleRange().second <= widget.dataEndMs());
    QVERIFY(widget.visibleRange().second - widget.visibleRange().first >= 1000);

    widget.setVisibleRange(40000, 40500);
    QCOMPARE(widget.visibleRange().second - widget.visibleRange().first, 1000);
}

void TestTimeSeriesChartWidget::cursorDragDoesNotChangeVisibleRange()
{
    TimeSeriesChartWidget widget;
    widget.resize(900, 560);
    widget.setSeries(twoHourSeries());
    widget.setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QRect plot = widget.plotRectFor(TimeSeriesChannel::ArmTiltXDeg);
    QVERIFY(plot.isValid());
    const QPair<qint64, qint64> before = widget.visibleRange();
    sendMouse(&widget, QEvent::MouseButtonPress, plot.center());
    sendMouse(&widget, QEvent::MouseMove, plot.center() - QPoint(90, 0));
    sendMouse(&widget, QEvent::MouseButtonRelease, plot.center() - QPoint(90, 0));

    QCOMPARE(widget.visibleRange().first, before.first);
    QCOMPARE(widget.visibleRange().second, before.second);
    QVERIFY(widget.isCursorVisible());
}

void TestTimeSeriesChartWidget::panDragDoesNotChangeCursorTime()
{
    TimeSeriesChartWidget widget;
    widget.resize(900, 560);
    widget.setSeries(twoHourSeries());
    widget.setCursorTime(widget.dataStartMs() + 30000, true);
    const qint64 cursor = widget.cursorTime();
    widget.setInteractionMode(TimeSeriesChartWidget::InteractionMode::Pan);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.setVisibleRange(widget.dataStartMs() + 10000, widget.dataStartMs() + 70000);
    const QPair<qint64, qint64> before = widget.visibleRange();
    const QRect plot = widget.plotRectFor(TimeSeriesChannel::ArmTiltXDeg);
    sendMouse(&widget, QEvent::MouseButtonPress, plot.center());
    sendMouse(&widget, QEvent::MouseMove, plot.center() + QPoint(80, 0));
    sendMouse(&widget, QEvent::MouseButtonRelease, plot.center() + QPoint(80, 0));

    QCOMPARE(widget.cursorTime(), cursor);
    QVERIFY(widget.visibleRange().first != before.first
            || widget.visibleRange().second != before.second);
}

void TestTimeSeriesChartWidget::cursorShowsDashInsideGap()
{
    QVector<TimeSeriesPoint> points;
    points << pt(0, 1.5) << pt(20000, 8.5);
    TimeSeriesChartWidget widget;
    widget.setSeries(makeResult(TimeSeriesChannel::ArmTiltXDeg, points, 1000));
    widget.setCursorTime(10000, true);
    QCOMPARE(widget.cursorValueText(TimeSeriesChannel::ArmTiltXDeg), QStringLiteral("--"));

    widget.setCursorTime(800, true);
    QVERIFY(widget.cursorValueText(TimeSeriesChannel::ArmTiltXDeg) != QStringLiteral("--"));
}

void TestTimeSeriesChartWidget::downsampledFourHoursStaysConnected()
{
    QVector<TimeSeriesPoint> points;
    points.reserve(14400);
    for (int i = 0; i < 14400; ++i) {
        points.append(pt(i * 1000, 10.0 * qSin(i * 0.05) + 0.01 * i));
    }
    const QVector<TimeSeriesPoint> reduced =
        TimeSeriesQueryService::downsampleMinMax(points, TimeSeriesConfig::kMaxQueryPointsPerChannel);
    QVERIFY(reduced.size() >= 8);

    int wideIndex = -1;
    for (int i = 1; i < reduced.size(); ++i) {
        const qint64 delta = reduced.at(i).tsUtcMs - reduced.at(i - 1).tsUtcMs;
        if (delta > 5000 && delta < 30000) {
            wideIndex = i;
            break;
        }
    }
    QVERIFY2(wideIndex > 0, "4h min/max buckets should be farther apart than the old 5s gap");

    TimeSeriesChartWidget widget;
    widget.setSeries(makeResult(TimeSeriesChannel::ArmTiltXDeg, reduced, 1000));
    const qint64 mid = (reduced.at(wideIndex - 1).tsUtcMs + reduced.at(wideIndex).tsUtcMs) / 2;
    widget.setCursorTime(mid, true);
    QVERIFY(widget.cursorValueText(TimeSeriesChannel::ArmTiltXDeg) != QStringLiteral("--"));
}

void TestTimeSeriesChartWidget::fullscreenButtonEmitsChannel()
{
    TimeSeriesChartWidget widget;
    widget.resize(900, 640);
    TimeSeriesQueryResult result = twoHourSeries();
    TimeSeriesSeries second;
    second.channel = TimeSeriesChannel::ArmTiltYDeg;
    second.points = result.series.first().points;
    result.series.append(second);
    widget.setSeries(result);
    widget.setCursorTime(widget.dataStartMs() + 20000, true);
    const qint64 cursor = widget.cursorTime();
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QSignalSpy spy(&widget, &TimeSeriesChartWidget::fullscreenRequested);
    const QRect button = widget.fullscreenButtonRectFor(TimeSeriesChannel::ArmTiltYDeg);
    QVERIFY(button.width() >= 44);
    QVERIFY(button.height() >= 44);
    sendMouse(&widget, QEvent::MouseButtonPress, button.center());
    sendMouse(&widget, QEvent::MouseButtonRelease, button.center());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().value<TimeSeriesChannel>(), TimeSeriesChannel::ArmTiltYDeg);
    QCOMPARE(widget.cursorTime(), cursor);
}

void TestTimeSeriesChartWidget::fullscreenSyncsRangeAndCursor()
{
    TimeSeriesChartWidget widget;
    widget.setSeries(twoHourSeries());
    widget.setVisibleRange(widget.dataStartMs() + 10000, widget.dataStartMs() + 50000);
    widget.setCursorTime(widget.dataStartMs() + 20000, true);
    widget.setInteractionMode(TimeSeriesChartWidget::InteractionMode::Pan);

    TimeSeriesFullscreenDialog fullscreen;
    fullscreen.setContent(twoHourSeries(),
                          widget.visibleRange().first,
                          widget.visibleRange().second,
                          widget.cursorTime(),
                          widget.isCursorVisible(),
                          widget.interactionMode());
    fullscreen.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fullscreen));
    QCOMPARE(fullscreen.viewStartMs(), widget.visibleRange().first);
    QCOMPARE(fullscreen.viewEndMs(), widget.visibleRange().second);
    QCOMPARE(fullscreen.cursorTimeMs(), widget.cursorTime());
    QVERIFY(fullscreen.isCursorVisible());
    QCOMPARE(fullscreen.interactionMode(), TimeSeriesChartWidget::InteractionMode::Pan);

    fullscreen.chart()->setVisibleRange(widget.dataStartMs() + 15000, widget.dataStartMs() + 40000);
    fullscreen.chart()->setCursorTime(widget.dataStartMs() + 25000, true);
    fullscreen.chart()->setInteractionMode(TimeSeriesChartWidget::InteractionMode::Cursor);
    const qint64 expectedStart = fullscreen.chart()->visibleRange().first;
    const qint64 expectedEnd = fullscreen.chart()->visibleRange().second;
    const qint64 expectedCursor = fullscreen.chart()->cursorTime();

    QSignalSpy spy(&fullscreen, &TimeSeriesFullscreenDialog::viewStateClosing);
    fullscreen.close();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toLongLong(), expectedStart);
    QCOMPARE(spy.first().at(1).toLongLong(), expectedEnd);
    QCOMPARE(spy.first().at(2).toLongLong(), expectedCursor);
    QCOMPARE(spy.first().at(3).toBool(), true);
    QCOMPARE(spy.first().at(4).value<TimeSeriesChartWidget::InteractionMode>(),
             TimeSeriesChartWidget::InteractionMode::Cursor);
}

void TestTimeSeriesChartWidget::defaultChannelSelectionAndLimit()
{
    TimeSeriesDialog dialog(QStringLiteral("/tmp"));
    const QVector<TimeSeriesChannel> selected = dialog.selectedChannels();
    QCOMPARE(selected.size(), 4);
    QVERIFY(selected.contains(TimeSeriesChannel::ArmTiltXDeg));
    QVERIFY(selected.contains(TimeSeriesChannel::ArmTiltYDeg));
    QVERIFY(selected.contains(TimeSeriesChannel::ChassisTiltXDeg));
    QVERIFY(selected.contains(TimeSeriesChannel::ChassisTiltYDeg));

    QPushButton *j1 = dialog.channelButton(TimeSeriesChannel::J1AngleDeg);
    QPushButton *armX = dialog.channelButton(TimeSeriesChannel::ArmTiltXDeg);
    QVERIFY(j1);
    QVERIFY(armX);
    QVERIFY(!j1->isEnabled());
    QVERIFY(armX->isEnabled());

    armX->click();
    QVERIFY(!armX->isChecked());
    QVERIFY(j1->isEnabled());
    j1->click();
    QVERIFY(j1->isChecked());
    QCOMPARE(dialog.selectedChannels().size(), 4);
    QVERIFY(!dialog.channelButton(TimeSeriesChannel::J2HeightMm)->isEnabled());
}

void TestTimeSeriesChartWidget::channelDrawerButtonSitsAboveCursor()
{
    TimeSeriesDialog dialog(QStringLiteral("/tmp"));
    auto *drawerButton = dialog.findChild<QPushButton *>(QStringLiteral("TBtn_ChannelDrawer"));
    auto *cursorButton = dialog.findChild<QPushButton *>(QStringLiteral("TBtn_CursorMode"));
    QVERIFY(drawerButton);
    QVERIFY(cursorButton);
    QCOMPARE(drawerButton->text(), QStringLiteral("通道选择"));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("Lbl_SelectedSummary")) == nullptr);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(drawerButton->geometry().bottom() <= cursorButton->geometry().top());
}

void TestTimeSeriesChartWidget::quickRangesUseLocalDates()
{
    TimeSeriesDialog dialog(QStringLiteral("/tmp"));
    auto *startEdit = dialog.findChild<QDateEdit *>(QStringLiteral("Dt_Start"));
    auto *endEdit = dialog.findChild<QDateEdit *>(QStringLiteral("Dt_End"));
    QVERIFY(startEdit);
    QVERIFY(endEdit);
    QCOMPARE(startEdit->displayFormat(), QStringLiteral("yyyy-MM-dd"));
    QCOMPARE(endEdit->displayFormat(), QStringLiteral("yyyy-MM-dd"));

    dialog.applyQuickRange(TimeSeriesDialog::QuickRange::Last30Minutes);
    QCOMPARE(endEdit->date(), QDate::currentDate());
    QCOMPARE(startEdit->date(), QDateTime::currentDateTime().addSecs(-30 * 60).date());

    dialog.applyQuickRange(TimeSeriesDialog::QuickRange::LastHour);
    QCOMPARE(endEdit->date(), QDate::currentDate());
    QCOMPARE(startEdit->date(), QDateTime::currentDateTime().addSecs(-3600).date());

    dialog.applyQuickRange(TimeSeriesDialog::QuickRange::Last4Hours);
    QCOMPARE(endEdit->date(), QDate::currentDate());
    QCOMPARE(startEdit->date(), QDateTime::currentDateTime().addSecs(-4 * 3600).date());

    QCOMPARE(dialog.findChild<QPushButton *>(QStringLiteral("TBtn_QuickLast30Minutes"))->text(),
             QStringLiteral("最近30分钟"));
    QCOMPARE(dialog.findChild<QPushButton *>(QStringLiteral("TBtn_QuickLastHour"))->text(),
             QStringLiteral("最近1小时"));
    QCOMPARE(dialog.findChild<QPushButton *>(QStringLiteral("TBtn_QuickLast4Hours"))->text(),
             QStringLiteral("最近4小时"));
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("TBtn_CompareAvg"))->isChecked());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("TBtn_CompareMin"))->isChecked());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("TBtn_CompareMax"))->isChecked());
    QCOMPARE(dialog.findChild<QPushButton *>(QStringLiteral("TBtn_ConditionSearch"))->text(),
             QStringLiteral("工况反查"));
}

void TestTimeSeriesChartWidget::compareCursorShowsDashOnMissingDay()
{
    TimeSeriesCompareResult result;
    result.ok = true;
    result.kind = TimeSeriesCompareKind::MonthOverMonth;
    result.currentLabel = QStringLiteral("本月");
    result.previousLabel = QStringLiteral("上月");
    result.dayCount = 5;
    result.tickLabels << QStringLiteral("1") << QStringLiteral("2") << QStringLiteral("3")
                      << QStringLiteral("4") << QStringLiteral("5");
    TimeSeriesCompareChannel item;
    item.channel = TimeSeriesChannel::ArmTiltXDeg;
    item.currentPoints << pt(0, 1.0) << pt(1, 2.0) << pt(3, 4.0);
    item.previousPoints << pt(0, 0.5) << pt(1, 0.6) << pt(2, 0.7);
    result.series.append(item);

    TimeSeriesChartWidget widget;
    widget.setCompareResult(result);
    QVERIFY(widget.isCompareMode());
    widget.setCursorTime(2, true);
    QVERIFY(widget.cursorValueText(TimeSeriesChannel::ArmTiltXDeg).contains(QStringLiteral("--")));
    widget.setCursorTime(1, true);
    QVERIFY(!widget.cursorValueText(TimeSeriesChannel::ArmTiltXDeg).contains(QStringLiteral("本月 --")));
}

QTEST_MAIN(TestTimeSeriesChartWidget)
#include "tst_timeserieschartwidget.moc"
