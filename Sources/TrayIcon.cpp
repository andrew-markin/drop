#include "TrayIcon.h"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QStyleHints>

#if defined(Q_OS_LINUX)
#include <QDBusConnection>

#include "CinnamonTheme.h"
#endif

#if defined(Q_OS_WIN)
#include <QSettings>

#define NOMINMAX
#include <windows.h>
#endif

namespace {

static const char* const DarkIconPath = ":/Icons/DropTrayDark.svg";

#if !defined(Q_OS_MACOS)

static const char* const LightIconPath = ":/Icons/DropTrayLight.svg";

#if defined(Q_OS_WIN)

// The taskbar has a theme of its own, apart from the one of applications that Qt follows
static const char* const PersonalizeKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const char* const SystemUsesLightThemeValue = "SystemUsesLightTheme";

// Windows broadcasts it with this text when either of the themes changes
static const wchar_t* const ColorSetChange = L"ImmersiveColorSet";

#endif

#if defined(Q_OS_LINUX)

// dconf tells every change of settings, see the dconf-service manual
static const char* const DconfPath = "/ca/desrt/dconf/Writer/user";
static const char* const DconfInterface = "ca.desrt.dconf.Writer";
static const char* const DconfNotify = "Notify";

// Where Cinnamon keeps the name of its theme
static const QString CinnamonThemeSettings = "/org/cinnamon/theme/";

// Below it a light panel by the luminance of its color, as the eye sees it
static const qreal LightPanelLuminance = 0.5;

bool isLightColor(const QColor& color)
{
    return 0.299 * color.redF() + 0.587 * color.greenF() + 0.114 * color.blueF() >= LightPanelLuminance;
}

// Invalid when the theme or its style sheet is not found
QColor cinnamonPanelColor()
{
    QString name = CinnamonTheme::currentName();

    if (name.isNull())
    {
        return QColor();
    }

    QFile file(CinnamonTheme::styleSheetPath(name));

    if (!file.open(QIODevice::ReadOnly))
    {
        return QColor();
    }

    return CinnamonTheme::panelColor(QString::fromUtf8(file.readAll()));
}

#endif

bool isPanelDark()
{
#if defined(Q_OS_WIN)
    // No value before Windows 10, the taskbar is dark there
    QSettings settings(PersonalizeKey, QSettings::NativeFormat);
    return settings.value(SystemUsesLightThemeValue, 0).toInt() == 0;
#else
#if defined(Q_OS_LINUX)
    if (CinnamonTheme::isCurrentDesktop())
    {
        QColor color = cinnamonPanelColor();

        if (color.isValid())
        {
            return !isLightColor(color);
        }
    }

    // The top bar of GNOME stays dark in the light theme too
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':');

    if (desktops.contains("GNOME", Qt::CaseInsensitive))
    {
        return true;
    }
#endif
    // Panels of an unknown theme are more often dark
    return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
#endif
}

#endif

} // namespace

TrayIcon::TrayIcon(QObject* parent) : QSystemTrayIcon(parent)
{
    updateIcon();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &TrayIcon::updateIcon);

#if defined(Q_OS_LINUX)
    if (CinnamonTheme::isCurrentDesktop())
    {
        QDBusConnection::sessionBus().connect(QString(), DconfPath, DconfInterface, DconfNotify, this,
                                              SLOT(settingsChanged(QString)));
    }
#elif defined(Q_OS_WIN)
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}

TrayIcon::~TrayIcon()
{
#if defined(Q_OS_WIN)
    QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
}

bool TrayIcon::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result);

#if defined(Q_OS_WIN)
    if (eventType == "windows_generic_MSG")
    {
        const MSG* msg = static_cast<const MSG*>(message);

        if (msg->message == WM_SETTINGCHANGE && msg->lParam != 0 &&
            wcscmp(reinterpret_cast<const wchar_t*>(msg->lParam), ColorSetChange) == 0)
        {
            updateIcon();
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif

    return false;
}

void TrayIcon::settingsChanged(const QString& prefix)
{
#if defined(Q_OS_LINUX)
    // A key, or a directory of the keys that changed, which can be above the theme one
    if (prefix.startsWith(CinnamonThemeSettings) || CinnamonThemeSettings.startsWith(prefix))
    {
        updateIcon();
    }
#else
    Q_UNUSED(prefix);
#endif
}

// The SVG itself: the tray picks its size from the few Qt offers for it, as for other applications,
// while a full set of sizes made it pick a small one and look smaller than them
void TrayIcon::updateIcon()
{
#if defined(Q_OS_MACOS)
    // A template image: the menu bar colors it for its appearance and for the highlight
    QIcon icon(DarkIconPath);
    icon.setIsMask(true);
#else
    QIcon icon(isPanelDark() ? LightIconPath : DarkIconPath);
#endif

    setIcon(icon);
}
