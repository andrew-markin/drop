#ifndef UPLOADS_H
#define UPLOADS_H

#include <QBitArray>
#include <QDir>
#include <QHash>
#include <QList>
#include <QObject>

// Files being received during this run and the drop directory they go to, see "Storage" and
// "Operations" in docs/PROTOCOL.md
class Uploads : public QObject
{
    Q_OBJECT

public:
    static const qint64 DefaultChunkSize = 4 * 1024 * 1024; // 4 MiB
    static const int HeadSize = 32;

    enum class Error
    {
        None,
        BadRequest,
        NotFound,
        NoSpace,
        Failed,
    };

    struct Upload
    {
        QByteArray id;
        QString name;
        qint64 size = 0;
        qint64 modified = 0;
        QByteArray head;
        int chunkCount = 0;
        QBitArray received;
        int receivedCount = 0;
        bool complete = false;
        QString path; // Temporary until complete, then final

        qint64 receivedSize(qint64 chunkSize) const;
    };

    Uploads(const QDir& baseDir, qint64 chunkSize = DefaultChunkSize, QObject* parent = nullptr);

    qint64 chunkSize() const;
    QString dropDirectoryPath() const;

    QByteArray announce(const QString& name, qint64 size, qint64 modified, const QByteArray& head, Error* error);
    Error writeChunk(const QByteArray& id, int index, const QByteArray& data);

    bool contains(const QByteArray& id) const;
    Upload upload(const QByteArray& id) const;
    QList<Upload> uploads() const;

signals:
    void dropDirectoryCreated(const QString& path);
    void uploadAdded(const QByteArray& id);
    void uploadChanged(const QByteArray& id);

private:
    Error ensureDropDirectory();
    Error complete(Upload& upload);
    Error describeWriteFailure(qint64 neededSize) const;

private:
    QDir m_baseDir;
    qint64 m_chunkSize;
    QString m_dropDirectoryPath;

    QList<Upload> m_uploads;
    QHash<QByteArray, int> m_indexesById;
    QHash<QByteArray, int> m_indexesByKey;
};

#endif // UPLOADS_H
