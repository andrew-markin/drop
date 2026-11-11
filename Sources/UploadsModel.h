#ifndef UPLOADSMODEL_H
#define UPLOADSMODEL_H

#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QHash>

class QTimer;
class Uploads;

// Files of the drop in the order devices announced them
class UploadsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn,
        SizeColumn,
        StatusColumn,
        ColumnCount,
    };

    enum Role
    {
        ProgressRole = Qt::UserRole, // Received part from 0 to 1
        CompleteRole,
        PathRole, // Final once complete, empty until the first chunk
        AfterglowRole, // From 1 when the file is just complete down to 0 a moment later
        ErrorRole, // The last chunk could not be written here
    };

    // The full progress stays a moment after a file is complete, so it is seen among small files
    static const int AfterglowHold = 500; // 0.5s
    static const int AfterglowFade = 500; // 0.5s

    UploadsModel(Uploads* uploads, QObject* parent = nullptr);

    QModelIndex indexOf(const QByteArray& id, int column = NameColumn) const;

    // Three digits at most, like 512 KB, 4.2 MB or 37 GB, so the column never needs more room
    static QString formatSize(qint64 size);

    // Values a column has room for from the start, as wide as most real ones
    static QStringList reservedTexts(int column);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private slots:
    void addUpload(const QByteArray& id);
    void updateUpload(const QByteArray& id);
    void updateAfterglows();

private:
    Uploads* m_uploads;
    QList<QByteArray> m_ids;
    QHash<QByteArray, int> m_rowsById;

    QHash<QByteArray, QElapsedTimer> m_completeTimes; // Of files completed lately
    QTimer* m_afterglowTimer;
};

#endif // UPLOADSMODEL_H
