#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <functional>
#include <memory>

#include "SingleInstance.h"

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void secondInstanceActivatesFirst();
    void startsAgainAfterFirstQuits();
    void namesAreSeparate();

private:
    QString name(const QString& suffix = QString()) const;
    bool runInThread(const std::function<bool()>& function);

private:
    QTemporaryDir* m_temporaryDir = nullptr;
};

void TestSingleInstance::init()
{
    m_temporaryDir = new QTemporaryDir();
    QVERIFY(m_temporaryDir->isValid());
}

void TestSingleInstance::cleanup()
{
    delete m_temporaryDir;
}

// Unique for the run, so tests running at once do not meet
QString TestSingleInstance::name(const QString& suffix) const
{
    return QString("DropTest-%1-%2%3").arg(QCoreApplication::applicationPid()).arg(qintptr(m_temporaryDir)).arg(suffix);
}

// Another instance is another process in real life. On Windows a pipe write completes only once the
// server reads, so the second instance runs in a thread while this one keeps the server going.
bool TestSingleInstance::runInThread(const std::function<bool()>& function)
{
    bool result = false;
    std::unique_ptr<QThread> thread(QThread::create([&result, function]() { result = function(); }));
    thread->start();

    bool finished = QTest::qWaitFor([&thread]() { return thread->isFinished(); }, 30000);

    if (!finished)
    {
        thread->wait();
    }

    return finished && result;
}

void TestSingleInstance::secondInstanceActivatesFirst()
{
    SingleInstance first(name(), m_temporaryDir->path());
    QSignalSpy activationSpy(&first, &SingleInstance::activationRequested);
    QVERIFY(first.start());

    QString lockDirectory = m_temporaryDir->path();

    QVERIFY(runInThread([this, lockDirectory]() {
        SingleInstance second(name(), lockDirectory);
        return !second.start();
    }));

    QTRY_COMPARE(activationSpy.count(), 1);
}

void TestSingleInstance::startsAgainAfterFirstQuits()
{
    {
        SingleInstance first(name(), m_temporaryDir->path());
        QVERIFY(first.start());
    }

    SingleInstance again(name(), m_temporaryDir->path());
    QVERIFY(again.start());
}

void TestSingleInstance::namesAreSeparate()
{
    SingleInstance first(name("a"), m_temporaryDir->path());
    SingleInstance other(name("b"), m_temporaryDir->path());

    QVERIFY(first.start());
    QVERIFY(other.start());
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "TestSingleInstance.moc"
