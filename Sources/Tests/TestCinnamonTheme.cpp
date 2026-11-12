#include <QTest>

#include "CinnamonTheme.h"

class TestCinnamonTheme : public QObject
{
    Q_OBJECT

private slots:
    void panelColor_data();
    void panelColor();
};

void TestCinnamonTheme::panelColor_data()
{
    QTest::addColumn<QString>("styleSheet");
    QTest::addColumn<QColor>("color");

    // As Mint-Y has it, the rules of one panel without the background come after
    QTest::newRow("mint") << QString(R"(
#panel {
  /* Set the symbolic foreground color for icons, and importantly, the system tray applet icons */
  color: #303030; }

.panel-top, .panel-bottom, .panel-left, .panel-right {
  color: #ffffff;
  border: none;
  background-color: rgba(240, 240, 240, 0.99);
  padding: 0px; }

.panel-bottom {
  box-shadow: 0 -1px #e8e8e8; }

.panel-bottom .workspace-switcher {
  background-color: #ff0000; }
)") << QColor(240, 240, 240, qRound(0.99 * 255));

    QTest::newRow("hex") << QString(".panel-top, .panel-bottom { background-color: #1a1a1a; }") << QColor(26, 26, 26);
    QTest::newRow("name") << QString(".panel-bottom { background: white }") << QColor(Qt::white);
    QTest::newRow("last wins") << QString(".panel-bottom { background-color: #000; }\n"
                                          ".panel-bottom { background-color: #fff; }")
                               << QColor(Qt::white);
    QTest::newRow("top only") << QString(".panel-top { background-color: #123456; }") << QColor(0x12, 0x34, 0x56);
    QTest::newRow("commented") << QString(".panel-bottom { /* background-color: #fff; */ color: red; }")
                               << QColor();
    QTest::newRow("gradient") << QString(".panel-bottom { background-gradient-start: #fff; }") << QColor();
    QTest::newRow("other selector") << QString(".panel-bottom-box { background-color: #fff; }") << QColor();
    QTest::newRow("empty") << QString() << QColor();
}

void TestCinnamonTheme::panelColor()
{
    QFETCH(QString, styleSheet);
    QFETCH(QColor, color);

    QCOMPARE(CinnamonTheme::panelColor(styleSheet), color);
}

QTEST_GUILESS_MAIN(TestCinnamonTheme)
#include "TestCinnamonTheme.moc"
