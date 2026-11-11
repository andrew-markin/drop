#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QLockFile>
#include <QObject>

class QLocalServer;

// One running application per user: a drop is started, received and closed. A second start asks
// the running one to show its window and quits. The lock file decides who runs, even for starts at
// the same moment, the local server only carries the request.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // The name tells applications apart, the directory holds the lock file
    SingleInstance(const QString& name, const QString& lockDirectory, QObject* parent = nullptr);

    // True for the first instance, false for another one, which has asked the first to show up
    bool start();

    static QString defaultName();
    static QString defaultLockDirectory();

signals:
    void activationRequested();

private slots:
    void acceptConnections();

private:
    QString m_name;
    QLockFile m_lockFile;
    QLocalServer* m_server;
};

#endif // SINGLEINSTANCE_H
