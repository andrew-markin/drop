#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "Storage.h"

class TestStorage : public QObject
{
    Q_OBJECT

private slots:
    void getDropDirectoryName_data();
    void getDropDirectoryName();

    void createDropDirectory();
    void createDropDirectoryMissingBase();

    void sanitizeFileName_data();
    void sanitizeFileName();
    void sanitizeFileNameLong();

    void getUniqueFileName();
};

void TestStorage::getDropDirectoryName_data()
{
    QTest::addColumn<QDate>("date");
    QTest::addColumn<int>("number");
    QTest::addColumn<QString>("expected");

    QTest::newRow("first") << QDate(2026, 10, 27) << 1 << "Drop-2026-10-27-1";
    QTest::newRow("padded date") << QDate(2027, 1, 5) << 12 << "Drop-2027-01-05-12";
}

void TestStorage::getDropDirectoryName()
{
    QFETCH(QDate, date);
    QFETCH(int, number);
    QFETCH(QString, expected);

    QCOMPARE(Storage::getDropDirectoryName(date, number), expected);
}

void TestStorage::createDropDirectory()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    QDir baseDir(temporaryDir.path());
    QDate date(2026, 10, 27);

    QCOMPARE(Storage::createDropDirectory(baseDir, date), baseDir.absoluteFilePath("Drop-2026-10-27-1"));
    QCOMPARE(Storage::createDropDirectory(baseDir, date), baseDir.absoluteFilePath("Drop-2026-10-27-2"));

    // A file takes the number as well as a directory does
    QFile file(baseDir.filePath("Drop-2026-10-27-3"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QCOMPARE(Storage::createDropDirectory(baseDir, date), baseDir.absoluteFilePath("Drop-2026-10-27-4"));

    // Numbering starts over on another day
    QCOMPARE(Storage::createDropDirectory(baseDir, date.addDays(1)),
             baseDir.absoluteFilePath("Drop-2026-10-28-1"));

    QVERIFY(QFileInfo(baseDir.filePath("Drop-2026-10-27-4")).isDir());
}

void TestStorage::createDropDirectoryMissingBase()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    // The downloads directory may not exist yet on a fresh system
    QDir baseDir(temporaryDir.filePath("Downloads"));

    QCOMPARE(Storage::createDropDirectory(baseDir, QDate(2026, 10, 27)),
             baseDir.absoluteFilePath("Drop-2026-10-27-1"));
}

void TestStorage::sanitizeFileName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << "IMG_0042.jpg" << "IMG_0042.jpg";
    QTest::newRow("unicode") << "Фото с моря 🌊.heic" << "Фото с моря 🌊.heic";
    QTest::newRow("unix path") << "../../etc/passwd" << "passwd";
    QTest::newRow("windows path") << "C:\\Windows\\win.ini" << "win.ini";
    QTest::newRow("trailing separator") << "folder/" << "File";
    QTest::newRow("forbidden characters") << "a<b>c:d\"e|f?g*h.txt" << "abcdefgh.txt";
    QTest::newRow("control characters") << QString("a\nb\tc") + QChar(0x7f) + ".txt" << "abc.txt";
    QTest::newRow("leading dots") << ".bashrc" << "bashrc";
    QTest::newRow("dot dot") << ".." << "File";
    QTest::newRow("trailing dots and spaces") << "report. . " << "report";
    QTest::newRow("empty") << "" << "File";
    QTest::newRow("reserved") << "CON" << "_CON";
    QTest::newRow("reserved with extension") << "nul.tar.gz" << "_nul.tar.gz";
    QTest::newRow("reserved lowercase") << "com1.txt" << "_com1.txt";
    QTest::newRow("not reserved") << "CONSOLE.txt" << "CONSOLE.txt";
}

void TestStorage::sanitizeFileName()
{
    QFETCH(QString, name);
    QFETCH(QString, expected);

    QCOMPARE(Storage::sanitizeFileName(name), expected);
}

void TestStorage::sanitizeFileNameLong()
{
    // Keeps the extension
    QString result = Storage::sanitizeFileName(QString(300, 'a') + ".jpg");
    QCOMPARE(result.toUtf8().size(), 200);
    QVERIFY(result.endsWith("a.jpg"));

    // Never splits a character, even one outside the basic plane
    result = Storage::sanitizeFileName(QString("🌊").repeated(100) + ".png");
    QVERIFY(result.toUtf8().size() <= 200);
    QVERIFY(result.endsWith("🌊.png"));
    QCOMPARE(QString::fromUtf8(result.toUtf8()), result);

    // An absurdly long extension is not an extension
    result = Storage::sanitizeFileName("a." + QString(300, 'b'));
    QCOMPARE(result, "a." + QString(198, 'b'));
}

void TestStorage::getUniqueFileName()
{
    QTemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    QDir dir(temporaryDir.path());

    auto touch = [&dir](const QString& name) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
    };

    QCOMPARE(Storage::getUniqueFileName(dir, "IMG_0042.jpg"), "IMG_0042.jpg");

    touch("IMG_0042.jpg");
    QCOMPARE(Storage::getUniqueFileName(dir, "IMG_0042.jpg"), "IMG_0042 (2).jpg");

    touch("IMG_0042 (2).jpg");
    QCOMPARE(Storage::getUniqueFileName(dir, "IMG_0042.jpg"), "IMG_0042 (3).jpg");

    touch("README");
    QCOMPARE(Storage::getUniqueFileName(dir, "README"), "README (2)");
}

QTEST_GUILESS_MAIN(TestStorage)
#include "TestStorage.moc"
