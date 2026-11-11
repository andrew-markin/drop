#include "UploadsModel.h"

#include <QLocale>
#include <QTimer>

#include "Uploads.h"

namespace {

static const int AfterglowInterval = 16; // About the frame rate of a screen

} // namespace

UploadsModel::UploadsModel(Uploads* uploads, QObject* parent) : QAbstractTableModel(parent), m_uploads(uploads)
{
    const QList<Uploads::Upload> existingUploads = m_uploads->uploads();

    for (const Uploads::Upload& upload : existingUploads)
    {
        m_rowsById.insert(upload.id, m_ids.size());
        m_ids.append(upload.id);
    }

    m_afterglowTimer = new QTimer(this);
    m_afterglowTimer->setInterval(AfterglowInterval);
    connect(m_afterglowTimer, &QTimer::timeout, this, &UploadsModel::updateAfterglows);

    connect(m_uploads, &Uploads::uploadAdded, this, &UploadsModel::addUpload);
    connect(m_uploads, &Uploads::uploadChanged, this, &UploadsModel::updateUpload);
}

QString UploadsModel::formatSize(qint64 size)
{
    const QStringList units = {tr("B"), tr("KB"), tr("MB"), tr("GB"), tr("TB")};

    double value = size;
    int unit = 0;

    // Decimal units, as phones and file managers count
    while (value >= 999.5 && unit < units.size() - 1)
    {
        value /= 1000;
        unit++;
    }

    int decimals = unit > 0 && value < 9.95 ? 1 : 0;
    return QString("%1 %2").arg(QLocale().toString(value, 'f', decimals), units.at(unit));
}

QStringList UploadsModel::reservedTexts(int column)
{
    switch (column)
    {
    case SizeColumn:
        // Photos and videos from a phone, from kilobytes to a few gigabytes
        return {formatSize(Q_INT64_C(999000000)), formatSize(Q_INT64_C(9900000000))};
    case StatusColumn:
        return {"100%", tr("Done")};
    }

    return {};
}

QModelIndex UploadsModel::indexOf(const QByteArray& id, int column) const
{
    int row = m_rowsById.value(id, -1);
    return row < 0 ? QModelIndex() : index(row, column);
}

int UploadsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_ids.size();
}

int UploadsModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant UploadsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_ids.size())
    {
        return QVariant();
    }

    Uploads::Upload upload = m_uploads->upload(m_ids.at(index.row()));

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case NameColumn:
            return upload.name;
        case SizeColumn:
            return formatSize(upload.size);
        case StatusColumn:
            if (upload.complete)
            {
                return tr("Done");
            }

            return QString("%1%").arg(int(data(index, ProgressRole).toDouble() * 100));
        }
        break;

    case Qt::ToolTipRole:
        // Where to find the file once it is there
        return index.column() == NameColumn ? (upload.complete ? upload.path : upload.name) : QVariant();

    case Qt::TextAlignmentRole:
        return QVariant::fromValue(index.column() == NameColumn ? Qt::AlignLeft | Qt::AlignVCenter
                                                                : Qt::AlignRight | Qt::AlignVCenter);

    case ProgressRole:
        if (upload.complete)
        {
            return 1.0;
        }

        return upload.size > 0 ? double(upload.receivedSize(m_uploads->chunkSize())) / upload.size : 0.0;

    case CompleteRole:
        return upload.complete;

    case PathRole:
        return upload.path;

    case AfterglowRole:
    {
        auto it = m_completeTimes.constFind(upload.id);

        if (it == m_completeTimes.constEnd())
        {
            return 0.0;
        }

        qint64 fadeTime = it->elapsed() - AfterglowHold;
        return fadeTime <= 0 ? 1.0 : qMax(0.0, 1.0 - double(fadeTime) / AfterglowFade);
    }
    }

    return QVariant();
}

QVariant UploadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
    {
        return QVariant();
    }

    switch (role)
    {
    case Qt::DisplayRole:
        switch (section)
        {
        case NameColumn:
            return tr("Name");
        case SizeColumn:
            return tr("Size");
        case StatusColumn:
            return tr("Status");
        }
        break;

    case Qt::TextAlignmentRole:
        return QVariant::fromValue(section == NameColumn ? Qt::AlignLeft | Qt::AlignVCenter
                                                         : Qt::AlignRight | Qt::AlignVCenter);
    }

    return QVariant();
}

void UploadsModel::addUpload(const QByteArray& id)
{
    int row = m_ids.size();

    beginInsertRows(QModelIndex(), row, row);
    m_rowsById.insert(id, row);
    m_ids.append(id);
    endInsertRows();

    updateUpload(id); // An empty file is complete right away
}

void UploadsModel::updateUpload(const QByteArray& id)
{
    int row = m_rowsById.value(id, -1);

    if (row < 0)
    {
        return;
    }

    if (m_uploads->upload(id).complete && !m_completeTimes.contains(id))
    {
        m_completeTimes[id].start();
        m_afterglowTimer->start();
    }

    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

void UploadsModel::updateAfterglows()
{
    bool glowing = false;

    for (auto it = m_completeTimes.cbegin(); it != m_completeTimes.cend(); ++it)
    {
        // The times stay, so a file glows only once
        if (it->elapsed() > AfterglowHold + AfterglowFade + AfterglowInterval)
        {
            continue;
        }

        int row = m_rowsById.value(it.key());
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1), {AfterglowRole});
        glowing = true;
    }

    if (!glowing)
    {
        m_afterglowTimer->stop();
    }
}
