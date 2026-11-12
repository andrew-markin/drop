#include "CinnamonTheme.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

static const char* const DesktopName = "X-Cinnamon";

// Through the command, not GIO, so the build needs no more libraries
static const char* const SettingsCommand = "gsettings";
static const char* const SettingsSchema = "org.cinnamon.theme";
static const char* const NameKey = "name";
static const int SettingsTimeout = 1000; // 1s

static const char* const StyleSheetFile = "cinnamon/cinnamon.css";
static const char* const DefaultStyleSheetFile = "cinnamon/theme/cinnamon.css";

// The top one is the same in the themes of Mint, but the bottom one is where Cinnamon puts it
static const QStringList PanelSelectors = {".panel-bottom", ".panel-top"};

QColor parseColor(const QString& value)
{
    static const QRegularExpression rgbPattern(
        R"(^rgba?\(\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*(?:,\s*(\d*(?:\.\d+)?)\s*)?\)$)");

    QRegularExpressionMatch match = rgbPattern.match(value);

    if (match.hasMatch())
    {
        QColor color(qRound(match.captured(1).toDouble()), qRound(match.captured(2).toDouble()),
                     qRound(match.captured(3).toDouble()));

        if (match.hasCaptured(4))
        {
            color.setAlpha(qRound(match.captured(4).toDouble() * 255));
        }

        return color.isValid() ? color : QColor();
    }

    return QColor::fromString(value);
}

} // namespace

namespace CinnamonTheme {

bool isCurrentDesktop()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':').contains(DesktopName, Qt::CaseInsensitive);
}

QString currentName()
{
    QProcess process;
    process.start(SettingsCommand, {"get", SettingsSchema, NameKey});

    if (!process.waitForFinished(SettingsTimeout) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0)
    {
        process.kill();
        return QString();
    }

    // A GVariant string, quoted, as 'Mint-Y'
    QString value = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

    if (value.size() < 2 || value.front() != value.back() || (value.front() != '\'' && value.front() != '"'))
    {
        return QString();
    }

    return value.mid(1, value.size() - 2);
}

QString styleSheetPath(const QString& name)
{
    if (name.isEmpty())
    {
        return QStandardPaths::locate(QStandardPaths::GenericDataLocation, DefaultStyleSheetFile);
    }

    // Themes of the user go to ~/.themes as well
    QString path = QDir::home().filePath(QString(".themes/%1/%2").arg(name, StyleSheetFile));

    if (QFileInfo::exists(path))
    {
        return path;
    }

    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString("themes/%1/%2").arg(name, StyleSheetFile));
}

QColor panelColor(const QString& styleSheet)
{
    static const QRegularExpression commentPattern(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression rulePattern(R"(([^{}]*)\{([^{}]*)\})");
    static const QRegularExpression backgroundPattern(R"((?:^|;)\s*background(?:-color)?\s*:\s*([^;]+))");

    QString css = styleSheet;
    css.remove(commentPattern);

    for (const QString& panelSelector : PanelSelectors)
    {
        QColor color;

        // As in CSS, the last rule wins
        QRegularExpressionMatchIterator rules = rulePattern.globalMatch(css);

        while (rules.hasNext())
        {
            QRegularExpressionMatch rule = rules.next();
            const QStringList selectors = rule.captured(1).split(',');

            bool matches = std::any_of(selectors.begin(), selectors.end(), [&panelSelector](const QString& selector) {
                return selector.trimmed() == panelSelector;
            });

            if (!matches)
            {
                continue;
            }

            QRegularExpressionMatchIterator declarations = backgroundPattern.globalMatch(rule.captured(2));

            while (declarations.hasNext())
            {
                QColor value = parseColor(declarations.next().captured(1).trimmed());

                if (value.isValid())
                {
                    color = value;
                }
            }
        }

        if (color.isValid())
        {
            return color;
        }
    }

    return QColor();
}

} // namespace CinnamonTheme
