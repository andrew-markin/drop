#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "Uploads.h"
#include "UploadsModel.h"

namespace {

static const qint64 ChunkSize = 4;

QByteArray makeHead(char value = 'h')
{
    return QByteArray(Uploads::HeadSize, value);
}

} // namespace

class TestUploadsModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void startsWithExistingUploads();
    void addsRows();
    void followsProgress();
    void afterglow();
    void writeError();
    void headers();
    void formatSize_data();
    void formatSize();

private:
    QByteArray announce(const QString& name, qint64 size);

private:
    QTemporaryDir* m_temporaryDir = nullptr;
    Uploads* m_uploads = nullptr;
};

void TestUploadsModel::initTestCase()
{
    QLocale::setDefault(QLocale::c()); // Sizes with a decimal point whatever the system is
}

void TestUploadsModel::init()
{
    m_temporaryDir = new QTemporaryDir();
    QVERIFY(m_temporaryDir->isValid());
    m_uploads = new Uploads(QDir(m_temporaryDir->filePath("Downloads")), ChunkSize);
}

void TestUploadsModel::cleanup()
{
    delete m_uploads;
    delete m_temporaryDir;
}

QByteArray TestUploadsModel::announce(const QString& name, qint64 size)
{
    Uploads::Error error;
    return m_uploads->announce(name, size, 0, makeHead(), &error);
}

void TestUploadsModel::startsWithExistingUploads()
{
    QByteArray id = announce("a.txt", 10);

    UploadsModel model(m_uploads);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.indexOf(id).row(), 0);
}

void TestUploadsModel::addsRows()
{
    UploadsModel model(m_uploads);
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);

    QByteArray first = announce("a.txt", 10);
    QByteArray second = announce("b.txt", 2000);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), int(UploadsModel::ColumnCount));
    QCOMPARE(insertedSpy.count(), 2);

    QCOMPARE(model.indexOf(first).row(), 0);
    QCOMPARE(model.indexOf(second).row(), 1);
    QVERIFY(!model.indexOf("unknown").isValid());

    QCOMPARE(model.indexOf(second, UploadsModel::NameColumn).data().toString(), "b.txt");
    QCOMPARE(model.indexOf(second, UploadsModel::SizeColumn).data().toString(), "2.0 KB");
    QCOMPARE(model.indexOf(second, UploadsModel::StatusColumn).data().toString(), "0%");
}

void TestUploadsModel::followsProgress()
{
    QByteArray id = announce("a.txt", 10);

    UploadsModel model(m_uploads);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    QModelIndex status = model.indexOf(id, UploadsModel::StatusColumn);

    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::None);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.first().at(0).toModelIndex(), model.index(0, 0));
    QCOMPARE(changedSpy.first().at(1).toModelIndex(), model.index(0, UploadsModel::ColumnCount - 1));

    QCOMPARE(status.data(UploadsModel::ProgressRole).toDouble(), 0.4);
    QCOMPARE(status.data().toString(), "40%");
    QVERIFY(!status.data(UploadsModel::CompleteRole).toBool());

    QCOMPARE(m_uploads->writeChunk(id, 1, "4567"), Uploads::Error::None);
    QCOMPARE(m_uploads->writeChunk(id, 2, "89"), Uploads::Error::None);

    QCOMPARE(status.data(UploadsModel::ProgressRole).toDouble(), 1.0);
    QCOMPARE(status.data().toString(), "Done");
    QVERIFY(status.data(UploadsModel::CompleteRole).toBool());

    // Once there, the tooltip tells where the file is
    QCOMPARE(model.indexOf(id).data(Qt::ToolTipRole).toString(), m_uploads->upload(id).path);
}

void TestUploadsModel::afterglow()
{
    QByteArray id = announce("a.txt", 4);
    QByteArray earlier = announce("b.txt", 0); // Complete before the model, so it does not glow

    UploadsModel model(m_uploads);
    QModelIndex index = model.indexOf(id);

    QCOMPARE(index.data(UploadsModel::AfterglowRole).toDouble(), 0.0);
    QCOMPARE(model.indexOf(earlier).data(UploadsModel::AfterglowRole).toDouble(), 0.0);

    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::None);
    QCOMPARE(index.data(UploadsModel::AfterglowRole).toDouble(), 1.0);

    // Repainted while fading, then left alone
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QTest::qWait(UploadsModel::AfterglowHold + UploadsModel::AfterglowFade + 200);

    QCOMPARE(index.data(UploadsModel::AfterglowRole).toDouble(), 0.0);
    QVERIFY(changedSpy.count() > 5);

    int count = changedSpy.count();
    QTest::qWait(100);
    QCOMPARE(changedSpy.count(), count);
}

void TestUploadsModel::writeError()
{
    QFile blocker(m_temporaryDir->filePath("Downloads"));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QByteArray id = announce("a.txt", 6);
    UploadsModel model(m_uploads);
    QModelIndex status = model.indexOf(id, UploadsModel::StatusColumn);

    QVERIFY(!status.data(UploadsModel::ErrorRole).toBool());
    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::Failed);

    QVERIFY(status.data(UploadsModel::ErrorRole).toBool());
    QCOMPARE(status.data().toString(), "Failed");
    QVERIFY(!status.data(Qt::ToolTipRole).toString().isEmpty());
}

void TestUploadsModel::headers()
{
    UploadsModel model(m_uploads);

    QCOMPARE(model.headerData(UploadsModel::NameColumn, Qt::Horizontal).toString(), "Name");
    QCOMPARE(model.headerData(UploadsModel::StatusColumn, Qt::Horizontal).toString(), "Status");
    QVERIFY(!model.headerData(0, Qt::Vertical).isValid());
}

void TestUploadsModel::formatSize_data()
{
    QTest::addColumn<qint64>("size");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty") << Q_INT64_C(0) << "0 B";
    QTest::newRow("bytes") << Q_INT64_C(999) << "999 B";
    QTest::newRow("kilobyte") << Q_INT64_C(1000) << "1.0 KB";
    QTest::newRow("one decimal") << Q_INT64_C(4240000) << "4.2 MB";
    QTest::newRow("rounds up to two digits") << Q_INT64_C(9960000) << "10 MB";
    QTest::newRow("no decimals") << Q_INT64_C(37400000000) << "37 GB";
    QTest::newRow("three digits") << Q_INT64_C(512000) << "512 KB";
    QTest::newRow("rounds up to the next unit") << Q_INT64_C(999600) << "1.0 MB";
    QTest::newRow("largest unit") << Q_INT64_C(5000000000000000) << "5000 TB";
}

void TestUploadsModel::formatSize()
{
    QFETCH(qint64, size);
    QFETCH(QString, expected);

    QCOMPARE(UploadsModel::formatSize(size), expected);
}

QTEST_GUILESS_MAIN(TestUploadsModel)
#include "TestUploadsModel.moc"
