#include <QHeaderView>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>

#include "Uploads.h"
#include "UploadsDelegate.h"
#include "UploadsModel.h"
#include "UploadsView.h"

class TestUploadsView : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void columnsFitValues_data();
    void columnsFitValues();
    void scrollingLeavesNoLines();
};

void TestUploadsView::initTestCase()
{
    QLocale::setDefault(QLocale::c()); // Sizes with a decimal point whatever the system is
}

void TestUploadsView::columnsFitValues_data()
{
    QTest::addColumn<qreal>("pointSize");

    // Without hinting, as GNOME draws, text is a fraction of a pixel wider or narrower than a
    // whole number of pixels
    for (int tenths = 80; tenths <= 140; tenths += 5)
    {
        QTest::addRow("%.1fpt", tenths / 10.0) << tenths / 10.0;
    }
}

void TestUploadsView::columnsFitValues()
{
    QFETCH(qreal, pointSize);

    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    Uploads uploads(QDir(temporaryDir.filePath("Downloads")), 4);
    UploadsModel model(&uploads);
    UploadsView view(&model);

    QFont font = view.font();
    font.setPointSizeF(pointSize);
    font.setHintingPreference(QFont::PreferNoHinting);
    view.setFont(font);

    const QList<qint64> sizes = {214321, 3214567, 312456789, 168234567, 999999999, Q_INT64_C(4123456789)};

    for (qint64 size : sizes)
    {
        Uploads::Error error;
        uploads.announce(QString("IMG_%1.MOV").arg(size), size, 0, QByteArray(Uploads::HeadSize, 'h'), &error);
    }

    // The delegate and the header cut what does not fit in the section without the padding
    for (int column : {int(UploadsModel::SizeColumn), int(UploadsModel::StatusColumn)})
    {
        int room = view.header()->sectionSize(column) - 2 * UploadsDelegate::HorizontalPadding;

        QStringList texts = UploadsModel::reservedTexts(column);
        texts.append(model.headerData(column, Qt::Horizontal).toString());

        for (int row = 0; row < model.rowCount(); row++)
        {
            texts.append(model.index(row, column).data().toString());
        }

        for (const QString& text : std::as_const(texts))
        {
            QCOMPARE(view.fontMetrics().elidedText(text, Qt::ElideRight, room), text);
        }
    }
}

// Scrolling moves the pixels already on the screen, what is drawn over the rows must not move with them
void TestUploadsView::scrollingLeavesNoLines()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    Uploads uploads(QDir(temporaryDir.filePath("Downloads")), 4);
    UploadsModel model(&uploads);
    UploadsView view(&model);

    for (int index = 0; index < 50; index++)
    {
        Uploads::Error error;
        uploads.announce(QString("IMG_%1.JPG").arg(index), 1000, 0, QByteArray(Uploads::HeadSize, 'h'), &error);
    }

    view.resize(400, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QScrollBar* scrollBar = view.verticalScrollBar();
    QVERIFY(scrollBar->maximum() > 6);

    // Down and back up, by a few rows as the mouse wheel does
    for (int step : {3, 3, -3, -3})
    {
        scrollBar->setValue(scrollBar->value() + step);
        QApplication::processEvents();

        // What is on the screen against the whole view painted anew
        QImage shown = view.screen()->grabWindow(view.winId()).toImage().convertToFormat(QImage::Format_RGB32);
        QImage painted = view.grab().toImage().convertToFormat(QImage::Format_RGB32);
        QCOMPARE(shown.size(), painted.size());
        QVERIFY2(shown == painted, qPrintable(QString("Differs after scrolling to %1").arg(scrollBar->value())));
    }
}

QTEST_MAIN(TestUploadsView)
#include "TestUploadsView.moc"
