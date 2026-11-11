#include <QTest>

#include "UploadsEstimate.h"

namespace {

static const qint64 Megabyte = 1000 * 1000;

} // namespace

class TestUploadsEstimate : public QObject
{
    Q_OBJECT

private slots:
    void describe_data();
    void describe();

    void idle();
    void percent();
    void waitsForEnoughProgress();
    void steadySpeed();
    void doesNotJitter();
    void forgetsWhenStalled();
    void startsAgainForNewFiles();

private:
    // A steady upload of 1 MB a second, from the given time and received size, one update a second
    void run(UploadsEstimate& estimate, qint64 from, qint64 to, qint64 total, qint64 received);
};

void TestUploadsEstimate::run(UploadsEstimate& estimate, qint64 from, qint64 to, qint64 total, qint64 received)
{
    for (qint64 time = from; time <= to; time += 1000)
    {
        estimate.update(time, total, received + (time - from) / 1000 * Megabyte);
    }
}

void TestUploadsEstimate::describe_data()
{
    QTest::addColumn<qint64>("seconds");
    QTest::addColumn<QString>("expected");

    QTest::newRow("seconds") << qint64(40) << "less than a minute left";
    QTest::newRow("a minute") << qint64(60) << "1 min left";
    QTest::newRow("minutes round up") << qint64(125) << "3 min left";
    QTest::newRow("to five minutes") << qint64(13 * 60) << "15 min left";
    QTest::newRow("almost an hour") << qint64(57 * 60) << "1 h left";
    QTest::newRow("to ten minutes") << qint64(72 * 60) << "1 h 20 min left";
    QTest::newRow("whole hours") << qint64(2 * 60 * 60) << "2 h left";
}

void TestUploadsEstimate::describe()
{
    QFETCH(qint64, seconds);
    QFETCH(QString, expected);

    QCOMPARE(UploadsEstimate::describe(seconds), expected);
}

void TestUploadsEstimate::idle()
{
    UploadsEstimate estimate;
    estimate.update(0, 0, 0);

    QVERIFY(!estimate.isActive());
    QCOMPARE(estimate.remainingTime(), -1);
    QVERIFY(estimate.remainingText().isEmpty());
}

void TestUploadsEstimate::percent()
{
    UploadsEstimate estimate;

    estimate.update(0, 200 * Megabyte, 50 * Megabyte);
    QCOMPARE(estimate.percent(), 25);

    // Never 100 while something is left
    estimate.update(1000, 200 * Megabyte, 200 * Megabyte - 1);
    QCOMPARE(estimate.percent(), 99);
}

void TestUploadsEstimate::waitsForEnoughProgress()
{
    UploadsEstimate estimate;
    run(estimate, 0, 4000, 600 * Megabyte, 0);

    QVERIFY(estimate.isActive());
    QCOMPARE(estimate.remainingTime(), -1);

    run(estimate, 5000, 5000, 600 * Megabyte, 5 * Megabyte);
    QVERIFY(estimate.remainingTime() > 0);
}

void TestUploadsEstimate::steadySpeed()
{
    UploadsEstimate estimate;
    run(estimate, 0, 10000, 600 * Megabyte, 0);

    // Taken once 5 seconds of progress are there: 595 MB at 1 MB a second. Still 10 minutes since,
    // so not taken again.
    QCOMPARE(estimate.remainingTime(), 595);
    QCOMPARE(estimate.remainingText(), "10 min left");
}

void TestUploadsEstimate::doesNotJitter()
{
    UploadsEstimate estimate;
    run(estimate, 0, 10000, 600 * Megabyte, 0);
    QString shown = estimate.remainingText();

    // A few seconds later the time left is a little less, which is not worth telling
    run(estimate, 11000, 14000, 600 * Megabyte, 11 * Megabyte);
    QCOMPARE(estimate.remainingText(), shown);

    // A minute later it is
    run(estimate, 15000, 75000, 600 * Megabyte, 15 * Megabyte);
    QCOMPARE(estimate.remainingText(), "9 min left");
}

void TestUploadsEstimate::forgetsWhenStalled()
{
    UploadsEstimate estimate;
    run(estimate, 0, 10000, 600 * Megabyte, 0);
    QVERIFY(estimate.remainingTime() > 0);

    for (qint64 time = 11000; time <= 41000; time += 1000)
    {
        estimate.update(time, 600 * Megabyte, 10 * Megabyte);
    }

    QCOMPARE(estimate.remainingTime(), -1);
    QVERIFY(estimate.isActive());

    // Back as soon as chunks come again
    run(estimate, 42000, 43000, 600 * Megabyte, 11 * Megabyte);
    QVERIFY(estimate.remainingTime() > 0);
}

void TestUploadsEstimate::startsAgainForNewFiles()
{
    UploadsEstimate estimate;
    run(estimate, 0, 10000, 10 * Megabyte, 0);
    estimate.update(11000, 10 * Megabyte, 10 * Megabyte);
    QVERIFY(!estimate.isActive());

    // More files picked: the new drop starts from zero
    estimate.update(12000, 30 * Megabyte, 10 * Megabyte);
    QCOMPARE(estimate.percent(), 0);
    QCOMPARE(estimate.remainingTime(), -1);

    estimate.update(13000, 30 * Megabyte, 20 * Megabyte);
    QCOMPARE(estimate.percent(), 50);
}

QTEST_GUILESS_MAIN(TestUploadsEstimate)
#include "TestUploadsEstimate.moc"
