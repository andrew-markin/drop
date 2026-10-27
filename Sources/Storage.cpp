#include "Storage.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace {

static const int DropDirectoryNumberMax = 9999;
static const int UniqueFileNumberMax = 9999;

// Leaves room for the temporary name prefix and the " (N)" suffix within the usual 255 bytes limit
static const int FileNameSizeMax = 200;
static const int FileSuffixSizeMax = 32;

static const QString FallbackFileName = "File";

QPair<QString, QString> splitFileName(const QString& name)
{
    int dotIndex = name.lastIndexOf('.');

    if (dotIndex <= 0)
    {
        return qMakePair(name, QString());
    }

    return qMakePair(name.left(dotIndex), name.mid(dotIndex));
}

bool isReservedOnWindows(const QString& name)
{
    static const QRegularExpression reservedPattern("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$",
                                                    QRegularExpression::CaseInsensitiveOption);

    // Windows treats "nul.txt" or "con.tar.gz" as devices too
    return reservedPattern.match(name.section('.', 0, 0).trimmed()).hasMatch();
}

QString truncateToUtf8Size(const QString& text, int sizeMax)
{
    QString result = text;

    while (result.toUtf8().size() > sizeMax)
    {
        result.chop(result.size() >= 2 && result.at(result.size() - 1).isLowSurrogate() ? 2 : 1);
    }

    return result;
}

} // namespace

namespace Storage {

QString getDropDirectoryName(const QDate& date, int number)
{
    return QString("Drop-%1-%2").arg(date.toString("yyyy-MM-dd")).arg(number);
}

QString createDropDirectory(const QDir& baseDir, const QDate& date)
{
    if (!baseDir.mkpath("."))
    {
        return QString();
    }

    for (int number = 1; number <= DropDirectoryNumberMax; number++)
    {
        QString name = getDropDirectoryName(date, number);

        // Not mkpath: it succeeds for an existing directory, while a taken number must be skipped
        if (!baseDir.exists(name) && baseDir.mkdir(name))
        {
            return baseDir.absoluteFilePath(name);
        }
    }

    return QString();
}

QString sanitizeFileName(const QString& name)
{
    // Browsers report bare names, but the server never trusts them as paths
    QString result = name.section(QRegularExpression("[/\\\\]"), -1);

    static const QRegularExpression forbiddenPattern("[\\x00-\\x1f\\x7f<>:\"|?*]");
    result.remove(forbiddenPattern);

    // Windows drops trailing dots and spaces, and leading dots make files hidden elsewhere
    static const QRegularExpression edgesPattern("^[\\s.]+|[\\s.]+$");
    result.remove(edgesPattern);

    if (result.isEmpty())
    {
        return FallbackFileName;
    }

    if (isReservedOnWindows(result))
    {
        result.prepend('_');
    }

    if (result.toUtf8().size() > FileNameSizeMax)
    {
        auto [base, suffix] = splitFileName(result);

        if (suffix.toUtf8().size() > FileSuffixSizeMax)
        {
            base = result;
            suffix.clear();
        }

        result = truncateToUtf8Size(base, FileNameSizeMax - suffix.toUtf8().size()).trimmed() + suffix;
    }

    return result;
}

QString getUniqueFileName(const QDir& dir, const QString& name)
{
    if (!QFileInfo::exists(dir.filePath(name)))
    {
        return name;
    }

    auto [base, suffix] = splitFileName(name);

    for (int number = 2; number <= UniqueFileNumberMax; number++)
    {
        QString candidate = QString("%1 (%2)%3").arg(base).arg(number).arg(suffix);

        if (!QFileInfo::exists(dir.filePath(candidate)))
        {
            return candidate;
        }
    }

    return QString();
}

} // namespace Storage
