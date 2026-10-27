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
    QImage image = QrCodeWidget::makeImage("http://192.168.0.5:54246/#k=AAgQGCAoMDhASFBYYGhweICIkJigqLC4wMjQ2ODo8Pg");

    QCOMPARE(image.width(), image.height());

    // Version 1 has 21 modules, every next one adds 4, plus the quiet zone of 4 on each side
    QCOMPARE((image.width() - 8 - 21) % 4, 0);

    auto isDark = [&image](int x, int y) { return image.pixelIndex(x, y) == 1; };

    // Quiet zone is light all around
    for (int i = 0; i < image.width(); i++)
    {
        for (int j = 0; j < 4; j++)
        {
            QVERIFY(!isDark(i, j));
            QVERIFY(!isDark(j, i));
            QVERIFY(!isDark(i, image.height() - 1 - j));
            QVERIFY(!isDark(image.width() - 1 - j, i));
        }
    }

    // Finder patterns: dark 7x7 border, light ring, dark 3x3 center, in three corners
    int codeSize = image.width() - 8;

    for (QPoint corner : {QPoint(4, 4), QPoint(4 + codeSize - 7, 4), QPoint(4, 4 + codeSize - 7)})
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
