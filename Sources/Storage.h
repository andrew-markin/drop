#ifndef STORAGE_H
#define STORAGE_H

#include <QDate>
#include <QDir>
#include <QString>

// Where and under which names received files are stored, see "Storage" in docs/PROTOCOL.md
namespace Storage {

QString getDropDirectoryName(const QDate& date, int number);
QString createDropDirectory(const QDir& baseDir, const QDate& date);

QString sanitizeFileName(const QString& name);
QString getUniqueFileName(const QDir& dir, const QString& name);

} // namespace Storage

#endif // STORAGE_H
