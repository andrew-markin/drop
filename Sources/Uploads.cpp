#include "Uploads.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QRandomGenerator>
#include <QStorageInfo>

#include <limits>

#include "Storage.h"

namespace {

static const int UploadIdSize = 16;

QByteArray generateUploadId()
{
    QByteArray id(UploadIdSize, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(id.data()), UploadIdSize / sizeof(quint32));
    return id;
}

// The same file picked again, even after a page reload, resumes the same upload
QByteArray makeUploadKey(const QString& name, qint64 size, qint64 modified, const QByteArray& head)
{
    return name.toUtf8() + '\0' + QByteArray::number(size) + '\0' + QByteArray::number(modified) + '\0' + head;
}

} // namespace

qint64 Uploads::Upload::receivedSize(qint64 chunkSize) const
{
    if (complete)
    {
        return size;
    }

    qint64 result = qint64(receivedCount) * chunkSize;

    // The last chunk is shorter than the others
    if (chunkCount > 0 && received.testBit(chunkCount - 1))
    {
        result -= qint64(chunkCount) * chunkSize - size;
    }

    return result;
}

Uploads::Uploads(const QDir& baseDir, qint64 chunkSize, QObject* parent)
    : QObject(parent), m_baseDir(baseDir), m_chunkSize(chunkSize)
{
}

qint64 Uploads::chunkSize() const
{
    return m_chunkSize;
}

QString Uploads::dropDirectoryPath() const
{
    return m_dropDirectoryPath;
}

QByteArray Uploads::announce(const QString& name, qint64 size, qint64 modified, const QByteArray& head, Error* error)
{
    *error = Error::None;

    if (size < 0 || head.size() != HeadSize || (size + m_chunkSize - 1) / m_chunkSize > std::numeric_limits<int>::max())
    {
        *error = Error::BadRequest;
        return QByteArray();
    }

    QByteArray key = makeUploadKey(name, size, modified, head);

    if (m_indexesByKey.contains(key))
    {
        return m_uploads.at(m_indexesByKey.value(key)).id;
    }

    Upload upload;
    upload.id = generateUploadId();
    upload.name = name;
    upload.size = size;
    upload.modified = modified;
    upload.head = head;
    upload.chunkCount = int((size + m_chunkSize - 1) / m_chunkSize);
    upload.received.resize(upload.chunkCount);

    if (upload.chunkCount == 0)
    {
        // Nothing to wait for, an empty file is complete right away
        *error = complete(upload);

        if (*error != Error::None)
        {
            return QByteArray();
        }
    }

    m_uploads.append(upload);
    m_indexesById.insert(upload.id, m_uploads.size() - 1);
    m_indexesByKey.insert(key, m_uploads.size() - 1);

    emit uploadAdded(upload.id);
    return upload.id;
}

Uploads::Error Uploads::writeChunk(const QByteArray& id, int index, const QByteArray& data)
{
    if (!m_indexesById.contains(id))
    {
        return Error::NotFound;
    }

    Upload& upload = m_uploads[m_indexesById.value(id)];

    if (index < 0 || index >= upload.chunkCount)
    {
        return Error::BadRequest;
    }

    qint64 offset = qint64(index) * m_chunkSize;
    qint64 expectedSize = qMin(m_chunkSize, upload.size - offset);

    if (data.size() != expectedSize)
    {
        return Error::BadRequest;
    }

    if (upload.received.testBit(index))
    {
        // Retried after a lost answer, already written. Completion may have failed last time.
        return upload.complete || upload.receivedCount < upload.chunkCount ? Error::None : complete(upload);
    }

    Error error = ensureDropDirectory();

    if (error != Error::None)
    {
        return error;
    }

    if (upload.path.isEmpty())
    {
        upload.path = QDir(m_dropDirectoryPath).filePath(QString(".drop-%1.part").arg(QString::fromLatin1(id.toHex())));
    }

    QFile file(upload.path);

    // Chunks come in any order, writing past the end leaves a gap to be filled later
    if (!file.open(QIODevice::ReadWrite) || !file.seek(offset) || file.write(data) != data.size() || !file.flush())
    {
        qWarning() << "Unable to write" << upload.path << ":" << file.errorString();
        return describeWriteFailure(data.size());
    }

    file.close();

    upload.received.setBit(index);
    upload.receivedCount++;

    if (upload.receivedCount == upload.chunkCount)
    {
        error = complete(upload);

        if (error != Error::None)
        {
            return error;
        }
    }

    emit uploadChanged(id);
    return Error::None;
}

bool Uploads::contains(const QByteArray& id) const
{
    return m_indexesById.contains(id);
}

Uploads::Upload Uploads::upload(const QByteArray& id) const
{
    return m_uploads.value(m_indexesById.value(id, -1));
}

QList<Uploads::Upload> Uploads::uploads() const
{
    return m_uploads;
}

Uploads::Error Uploads::ensureDropDirectory()
{
    if (!m_dropDirectoryPath.isEmpty())
    {
        return Error::None;
    }

    m_dropDirectoryPath = Storage::createDropDirectory(m_baseDir, QDate::currentDate());

    if (m_dropDirectoryPath.isEmpty())
    {
        qWarning() << "Unable to create drop directory in" << m_baseDir.absolutePath();
        return Error::Failed;
    }

    emit dropDirectoryCreated(m_dropDirectoryPath);
    return Error::None;
}

Uploads::Error Uploads::complete(Upload& upload)
{
    Error error = ensureDropDirectory();

    if (error != Error::None)
    {
        return error;
    }

    QDir dropDirectory(m_dropDirectoryPath);

    if (upload.path.isEmpty())
    {
        upload.path = dropDirectory.filePath(QString(".drop-%1.part").arg(QString::fromLatin1(upload.id.toHex())));
    }

    QFile file(upload.path);

    if (!file.open(QIODevice::ReadWrite))
    {
        qWarning() << "Unable to open" << upload.path << ":" << file.errorString();
        return describeWriteFailure(0);
    }

    // Photos sort by the time they were taken, not by the time they arrived
    file.setFileTime(QDateTime::fromMSecsSinceEpoch(upload.modified), QFileDevice::FileModificationTime);
    file.close();

    QString name = Storage::getUniqueFileName(dropDirectory, Storage::sanitizeFileName(upload.name));
    QString path = dropDirectory.filePath(name);

    if (name.isEmpty() || !QFile::rename(upload.path, path))
    {
        qWarning() << "Unable to rename" << upload.path << "to" << path;
        return Error::Failed;
    }

    upload.path = path;
    upload.complete = true;
    return Error::None;
}

Uploads::Error Uploads::describeWriteFailure(qint64 neededSize) const
{
    QStorageInfo storage(m_dropDirectoryPath.isEmpty() ? m_baseDir.absolutePath() : m_dropDirectoryPath);
    return storage.isValid() && storage.bytesAvailable() < neededSize ? Error::NoSpace : Error::Failed;
}
