#include <QDateTime>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "Uploads.h"

namespace {

static const qint64 ChunkSize = 4;

QByteArray makeHead(char value = 'h')
{
    return QByteArray(Uploads::HeadSize, value);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class TestUploads : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void announceRejectsBadRequests();
    void announceIsIdempotent();
    void announceDoesNotCreateDirectory();

    void writeChunksInAnyOrder();
    void writeChunkTwice();
    void writeChunkRejectsBadRequests();
    void writeChunkUnknownUpload();

    void completeSetsModificationTime();
    void completeKeepsExistingFiles();
    void completeSanitizesName();
    void completeEmptyFile();

    void receivedSize();
    void emitsSignals();

private:
    QTemporaryDir* m_temporaryDir = nullptr;
    Uploads* m_uploads = nullptr;
};

void TestUploads::init()
{
    m_temporaryDir = new QTemporaryDir();
    QVERIFY(m_temporaryDir->isValid());
    m_uploads = new Uploads(QDir(m_temporaryDir->filePath("Downloads")), ChunkSize);
}

void TestUploads::cleanup()
{
    delete m_uploads;
    delete m_temporaryDir;
}

void TestUploads::announceRejectsBadRequests()
{
    Uploads::Error error;

    QVERIFY(m_uploads->announce("a.txt", -1, 0, makeHead(), &error).isEmpty());
    QCOMPARE(error, Uploads::Error::BadRequest);

    QVERIFY(m_uploads->announce("a.txt", 10, 0, QByteArray(31, 'h'), &error).isEmpty());
    QCOMPARE(error, Uploads::Error::BadRequest);

    QVERIFY(m_uploads->announce("a.txt", std::numeric_limits<qint64>::max() / 2, 0, makeHead(), &error).isEmpty());
    QCOMPARE(error, Uploads::Error::BadRequest);

    QVERIFY(m_uploads->uploads().isEmpty());
}

void TestUploads::announceIsIdempotent()
{
    Uploads::Error error;

    QByteArray id = m_uploads->announce("a.txt", 10, 1000, makeHead(), &error);
    QCOMPARE(error, Uploads::Error::None);
    QCOMPARE(id.size(), 16);

    QCOMPARE(m_uploads->announce("a.txt", 10, 1000, makeHead(), &error), id);

    // Any difference makes another file
    QVERIFY(m_uploads->announce("b.txt", 10, 1000, makeHead(), &error) != id);
    QVERIFY(m_uploads->announce("a.txt", 11, 1000, makeHead(), &error) != id);
    QVERIFY(m_uploads->announce("a.txt", 10, 1001, makeHead(), &error) != id);
    QVERIFY(m_uploads->announce("a.txt", 10, 1000, makeHead('x'), &error) != id);

    QCOMPARE(m_uploads->uploads().size(), 5);

    Uploads::Upload upload = m_uploads->upload(id);
    QCOMPARE(upload.name, "a.txt");
    QCOMPARE(upload.chunkCount, 3);
    QCOMPARE(upload.receivedCount, 0);
    QVERIFY(!upload.complete);
}

void TestUploads::announceDoesNotCreateDirectory()
{
    Uploads::Error error;
    m_uploads->announce("a.txt", 10, 0, makeHead(), &error);

    // The drop directory appears with the first data, not with a page that was just opened
    QVERIFY(m_uploads->dropDirectoryPath().isEmpty());
    QVERIFY(!QFileInfo::exists(m_temporaryDir->filePath("Downloads")));
}

void TestUploads::writeChunksInAnyOrder()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 10, 0, makeHead(), &error);

    QCOMPARE(m_uploads->writeChunk(id, 2, "89"), Uploads::Error::None);

    QString dropDirectoryPath = m_uploads->dropDirectoryPath();
    QCOMPARE(QFileInfo(dropDirectoryPath).fileName(),
             QString("Drop-%1-1").arg(QDate::currentDate().toString("yyyy-MM-dd")));

    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::None);
    QVERIFY(!m_uploads->upload(id).complete);
    QVERIFY(!QFileInfo::exists(QDir(dropDirectoryPath).filePath("a.txt")));

    QCOMPARE(m_uploads->writeChunk(id, 1, "4567"), Uploads::Error::None);

    Uploads::Upload upload = m_uploads->upload(id);
    QVERIFY(upload.complete);
    QCOMPARE(upload.path, QDir(dropDirectoryPath).filePath("a.txt"));
    QCOMPARE(readFile(upload.path), "0123456789");

    // Nothing temporary is left behind
    QCOMPARE(QDir(dropDirectoryPath).entryList(QDir::Files | QDir::Hidden), QStringList({"a.txt"}));
}

void TestUploads::writeChunkTwice()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 6, 0, makeHead(), &error);

    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::None);
    QCOMPARE(m_uploads->writeChunk(id, 0, "0123"), Uploads::Error::None);
    QCOMPARE(m_uploads->upload(id).receivedCount, 1);

    QCOMPARE(m_uploads->writeChunk(id, 1, "45"), Uploads::Error::None);
    QCOMPARE(m_uploads->writeChunk(id, 1, "45"), Uploads::Error::None);
    QCOMPARE(readFile(m_uploads->upload(id).path), "012345");

    // Announcing a complete file again tells it is done instead of starting over
    QCOMPARE(m_uploads->announce("a.txt", 6, 0, makeHead(), &error), id);
    QVERIFY(m_uploads->upload(id).complete);
}

void TestUploads::writeChunkRejectsBadRequests()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 6, 0, makeHead(), &error);

    QCOMPARE(m_uploads->writeChunk(id, -1, "0123"), Uploads::Error::BadRequest);
    QCOMPARE(m_uploads->writeChunk(id, 2, "67"), Uploads::Error::BadRequest);
    QCOMPARE(m_uploads->writeChunk(id, 0, "012"), Uploads::Error::BadRequest);
    QCOMPARE(m_uploads->writeChunk(id, 0, "01234"), Uploads::Error::BadRequest);
    QCOMPARE(m_uploads->writeChunk(id, 1, "4567"), Uploads::Error::BadRequest);

    QCOMPARE(m_uploads->upload(id).receivedCount, 0);
    QVERIFY(m_uploads->dropDirectoryPath().isEmpty());
}

void TestUploads::writeChunkUnknownUpload()
{
    QCOMPARE(m_uploads->writeChunk(QByteArray(16, 'x'), 0, "0123"), Uploads::Error::NotFound);
}

void TestUploads::completeSetsModificationTime()
{
    QDateTime modified = QDateTime(QDate(2024, 7, 14), QTime(18, 30, 15), QTimeZone::UTC);

    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 2, modified.toMSecsSinceEpoch(), makeHead(), &error);
    QCOMPARE(m_uploads->writeChunk(id, 0, "01"), Uploads::Error::None);

    QCOMPARE(QFileInfo(m_uploads->upload(id).path).lastModified(QTimeZone::UTC), modified);
}

void TestUploads::completeKeepsExistingFiles()
{
    Uploads::Error error;

    // Different photos often share a name, for example from two phones
    QByteArray firstId = m_uploads->announce("IMG_0001.jpg", 2, 0, makeHead('a'), &error);
    QByteArray secondId = m_uploads->announce("IMG_0001.jpg", 2, 0, makeHead('b'), &error);

    QCOMPARE(m_uploads->writeChunk(firstId, 0, "aa"), Uploads::Error::None);
    QCOMPARE(m_uploads->writeChunk(secondId, 0, "bb"), Uploads::Error::None);

    QCOMPARE(QFileInfo(m_uploads->upload(firstId).path).fileName(), "IMG_0001.jpg");
    QCOMPARE(QFileInfo(m_uploads->upload(secondId).path).fileName(), "IMG_0001 (2).jpg");
    QCOMPARE(readFile(m_uploads->upload(firstId).path), "aa");
    QCOMPARE(readFile(m_uploads->upload(secondId).path), "bb");
}

void TestUploads::completeSanitizesName()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("../../evil.sh", 2, 0, makeHead(), &error);
    QCOMPARE(m_uploads->writeChunk(id, 0, "01"), Uploads::Error::None);

    QCOMPARE(m_uploads->upload(id).path, QDir(m_uploads->dropDirectoryPath()).filePath("evil.sh"));

    // The name shown to the user stays as the device sent it
    QCOMPARE(m_uploads->upload(id).name, "../../evil.sh");
}

void TestUploads::completeEmptyFile()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("empty.txt", 0, 0, makeHead(), &error);
    QCOMPARE(error, Uploads::Error::None);

    Uploads::Upload upload = m_uploads->upload(id);
    QVERIFY(upload.complete);
    QCOMPARE(upload.chunkCount, 0);
    QVERIFY(QFileInfo(upload.path).isFile());
    QCOMPARE(QFileInfo(upload.path).size(), 0);
}

void TestUploads::receivedSize()
{
    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 10, 0, makeHead(), &error);

    QCOMPARE(m_uploads->upload(id).receivedSize(ChunkSize), 0);

    m_uploads->writeChunk(id, 2, "89");
    QCOMPARE(m_uploads->upload(id).receivedSize(ChunkSize), 2);

    m_uploads->writeChunk(id, 0, "0123");
    QCOMPARE(m_uploads->upload(id).receivedSize(ChunkSize), 6);

    m_uploads->writeChunk(id, 1, "4567");
    QCOMPARE(m_uploads->upload(id).receivedSize(ChunkSize), 10);
}

void TestUploads::emitsSignals()
{
    QSignalSpy directorySpy(m_uploads, &Uploads::dropDirectoryCreated);
    QSignalSpy addedSpy(m_uploads, &Uploads::uploadAdded);
    QSignalSpy changedSpy(m_uploads, &Uploads::uploadChanged);

    Uploads::Error error;
    QByteArray id = m_uploads->announce("a.txt", 6, 0, makeHead(), &error);
    m_uploads->announce("a.txt", 6, 0, makeHead(), &error);

    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).toByteArray(), id);

    m_uploads->writeChunk(id, 0, "0123");
    m_uploads->writeChunk(id, 0, "0123");
    m_uploads->writeChunk(id, 1, "45");

    QCOMPARE(directorySpy.count(), 1);
    QCOMPARE(directorySpy.at(0).at(0).toString(), m_uploads->dropDirectoryPath());
    QCOMPARE(changedSpy.count(), 2);
}

QTEST_GUILESS_MAIN(TestUploads)
#include "TestUploads.moc"
