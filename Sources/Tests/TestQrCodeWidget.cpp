#include <QTest>

#include "QrCodeWidget.h"

class TestQrCodeWidget : public QObject
{
    Q_OBJECT

private slots:
    void makeImage();
    void makeImageGrowsWithText();
};

void TestQrCodeWidget::makeImage()
{
    QImage image = QrCodeWidget::makeImage("http://192.168.0.5:54246/#AAgQGCAoMDhASFBYYGhweICIkJigqLC4wMjQ2ODo8Pg");

    QCOMPARE(image.width(), image.height());

    static const int quietZone = 2;

    // Version 1 has 21 modules, every next one adds 4, plus the quiet zone on each side
    QCOMPARE((image.width() - 2 * quietZone - 21) % 4, 0);

    auto isDark = [&image](int x, int y) { return image.pixelIndex(x, y) == 1; };

    // Quiet zone is light all around
    for (int i = 0; i < image.width(); i++)
    {
        for (int j = 0; j < quietZone; j++)
        {
            QVERIFY(!isDark(i, j));
            QVERIFY(!isDark(j, i));
            QVERIFY(!isDark(i, image.height() - 1 - j));
            QVERIFY(!isDark(image.width() - 1 - j, i));
        }
    }

    // Finder patterns: dark 7x7 border, light ring, dark 3x3 center, in three corners
    int codeSize = image.width() - 2 * quietZone;
    int far = quietZone + codeSize - 7;

    for (QPoint corner : {QPoint(quietZone, quietZone), QPoint(far, quietZone), QPoint(quietZone, far)})
    {
        QVERIFY(isDark(corner.x(), corner.y()));
        QVERIFY(isDark(corner.x() + 6, corner.y() + 6));
        QVERIFY(!isDark(corner.x() + 1, corner.y() + 1));
        QVERIFY(isDark(corner.x() + 3, corner.y() + 3));
    }
}

void TestQrCodeWidget::makeImageGrowsWithText()
{
    QVERIFY(QrCodeWidget::makeImage(QString(300, 'a')).width() > QrCodeWidget::makeImage("a").width());
}

QTEST_GUILESS_MAIN(TestQrCodeWidget)
#include "TestQrCodeWidget.moc"
