#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

#include "Translator.h"

class TestTranslator : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void russian();
    void russianPlurals_data();
    void russianPlurals();
    void englishPlurals();
    void otherLanguages();

    void everythingIsTranslated();

private:
    Translator* m_translator = nullptr;

    QString translate(QLocale::Language language, const char* sourceText, int n = -1);
};

void TestTranslator::cleanup()
{
    if (m_translator)
    {
        QCoreApplication::removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }
}

// Through QCoreApplication, the way tr() works, so %n is filled in too
QString TestTranslator::translate(QLocale::Language language, const char* sourceText, int n)
{
    cleanup();
    m_translator = new Translator(QLocale(language));
    QCoreApplication::installTranslator(m_translator);
    return QCoreApplication::translate("Drop", sourceText, "", n);
}

void TestTranslator::russian()
{
    QCOMPARE(translate(QLocale::Russian, "Open Folder"), "Открыть папку");
    QCOMPARE(translate(QLocale::Russian, "Not in the table"), "Not in the table");
}

void TestTranslator::russianPlurals_data()
{
    QTest::addColumn<int>("n");
    QTest::addColumn<QString>("expected");

    QTest::newRow("1") << 1 << "Получен 1 файл. Нажмите, чтобы открыть папку.";
    QTest::newRow("2") << 2 << "Получено 2 файла. Нажмите, чтобы открыть папку.";
    QTest::newRow("4") << 4 << "Получено 4 файла. Нажмите, чтобы открыть папку.";
    QTest::newRow("5") << 5 << "Получено 5 файлов. Нажмите, чтобы открыть папку.";
    QTest::newRow("11") << 11 << "Получено 11 файлов. Нажмите, чтобы открыть папку.";
    QTest::newRow("12") << 12 << "Получено 12 файлов. Нажмите, чтобы открыть папку.";
    QTest::newRow("21") << 21 << "Получен 21 файл. Нажмите, чтобы открыть папку.";
    QTest::newRow("22") << 22 << "Получено 22 файла. Нажмите, чтобы открыть папку.";
    QTest::newRow("111") << 111 << "Получено 111 файлов. Нажмите, чтобы открыть папку.";
}

void TestTranslator::russianPlurals()
{
    QFETCH(int, n);
    QFETCH(QString, expected);

    QCOMPARE(translate(QLocale::Russian, "Received %n file(s). Click to open the folder.", n), expected);
}

void TestTranslator::englishPlurals()
{
    QCOMPARE(translate(QLocale::English, "Received %n file(s). Click to open the folder.", 1),
             "Received 1 file. Click to open the folder.");
    QCOMPARE(translate(QLocale::English, "Received %n file(s). Click to open the folder.", 3),
             "Received 3 files. Click to open the folder.");
    QCOMPARE(translate(QLocale::English, "%1 of %n file(s) received", 1).arg(0), "0 of 1 file received");
    QCOMPARE(translate(QLocale::English, "Open Folder"), "Open Folder");
}

void TestTranslator::otherLanguages()
{
    // English, with its plural forms
    QCOMPARE(translate(QLocale::German, "Open Folder"), "Open Folder");
    QCOMPARE(translate(QLocale::German, "%1 of %n file(s) received", 2).arg(1), "1 of 2 files received");
}

void TestTranslator::everythingIsTranslated()
{
    QFile file(DROP_TRANSLATED_SOURCE_PATH);
    QVERIFY2(file.open(QIODevice::ReadOnly), DROP_TRANSLATED_SOURCE_PATH);
    QString source = QString::fromUtf8(file.readAll());

    // tr("...") with adjacent literals joined, as the compiler does
    static const QRegularExpression callPattern("\\btr\\(\\s*((?:\"(?:[^\"\\\\]|\\\\.)*\"\\s*)+)");
    static const QRegularExpression literalPattern("\"((?:[^\"\\\\]|\\\\.)*)\"");

    int count = 0;

    for (const QRegularExpressionMatch& call : callPattern.globalMatch(source))
    {
        QString text;

        for (const QRegularExpressionMatch& literal : literalPattern.globalMatch(call.captured(1)))
        {
            text += literal.captured(1);
        }

        QByteArray sourceText = text.toUtf8();
        bool plural = text.contains("%n");

        QVERIFY2(translate(QLocale::Russian, sourceText.constData(), plural ? 5 : -1) != text,
                 qPrintable("No Russian translation: " + text));

        if (plural)
        {
            QVERIFY2(!translate(QLocale::English, sourceText.constData(), 5).contains("(s)"),
                     qPrintable("No English plural forms: " + text));
        }

        count++;
    }

    QVERIFY(count >= 15);
}

QTEST_GUILESS_MAIN(TestTranslator)
#include "TestTranslator.moc"
