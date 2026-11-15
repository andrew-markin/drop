#include "SingleInstance.h"

#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#if defined(Q_OS_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // AllowSetForegroundWindow, in user32, which MSVC links by default
#endif

namespace {

static const char* const ActivateMessage = "activate";
static const char* const QuitMessage = "quit";
static const int MessageSizeMax = 64;

static const int ConnectTimeout = 1000; // 1s
static const int WriteTimeout = 1000; // 1s

} // namespace

SingleInstance::SingleInstance(const QString& name, const QString& lockDirectory, QObject* parent)
    : QObject(parent), m_name(name), m_lockFile(QDir(lockDirectory).filePath(name + ".lock")), m_server(nullptr)
{
    // Held as long as the application runs: only a lock of a process that is gone is stale
    m_lockFile.setStaleLockTime(0);
}

bool SingleInstance::start()
{
    if (!m_lockFile.tryLock(0))
    {
        sendMessage(m_name, ActivateMessage);
        return false;
    }

    // Only the holder of the lock gets here, so a server left by a crash is safe to remove
    QLocalServer::removeServer(m_name);

    m_server = new QLocalServer(this);

#if defined(Q_OS_UNIX)
    // The owner only, the socket is in a directory shared by all users. Windows named pipes are open
    // to the owner and administrators anyway.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
#endif

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::acceptConnections);

    if (!m_server->listen(m_name))
    {
        // Still the only instance, a second start just will not bring this one up
        qWarning() << "Unable to listen for other instances:" << m_server->errorString();
    }

    return true;
}

bool SingleInstance::requestQuit(const QString& name)
{
    return sendMessage(name, QuitMessage);
}

bool SingleInstance::sendMessage(const QString& name, const QByteArray& message)
{
    QLocalSocket socket;
    socket.connectToServer(name);

    if (!socket.waitForConnected(ConnectTimeout))
    {
        return false;
    }

#if defined(Q_OS_WINDOWS)
    if (message == ActivateMessage)
    {
        // Windows keeps a process in the background from taking the foreground. This one was just
        // started by the user, so it may pass that right on to the running one.
        AllowSetForegroundWindow(ASFW_ANY);
    }
#endif

    socket.write(message);
    socket.waitForBytesWritten(WriteTimeout);
    socket.disconnectFromServer();

    return true;
}

QString SingleInstance::nameForUser(const QString& userName)
{
    return QString("%1-%2").arg(PROJECT_NAME, userName);
}

QString SingleInstance::defaultName()
{
    return nameForUser(qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME")));
}

QString SingleInstance::defaultLockDirectory()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return directory.isEmpty() ? QDir::tempPath() : directory;
}

void SingleInstance::acceptConnections()
{
    while (QLocalSocket* socket = m_server->nextPendingConnection())
    {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);

        // The message is complete when the sender disconnects
        connect(socket, &QLocalSocket::readyRead, socket, [socket]() {
            QByteArray message = socket->property("message").toByteArray() + socket->readAll();

            if (message.size() > MessageSizeMax)
            {
                socket->abort(); // Not another instance
                return;
            }

            socket->setProperty("message", message);
        });

        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            QByteArray message = socket->property("message").toByteArray() + socket->readAll();

            if (message == ActivateMessage)
            {
                emit activationRequested();
            }
            else if (message == QuitMessage)
            {
                emit quitRequested();
            }
        });
    }
}
