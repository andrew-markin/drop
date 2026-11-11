#include "DesktopNotifier.h"

#include <QApplication>
#include <QIcon>
#include <QImage>

#if defined(Q_OS_LINUX)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

namespace {

static const char* const DefaultActionKey = "default"; // A click on the notification itself

#if defined(Q_OS_LINUX)

// See the Desktop Notifications Specification
static const char* const NotificationsService = "org.freedesktop.Notifications";
static const char* const NotificationsPath = "/org/freedesktop/Notifications";
static const char* const NotificationsInterface = "org.freedesktop.Notifications";

static const int ImageSize = 64;
static const int ShowTimeout = -1; // As the service decides

// The "image-data" hint: the icon itself, so it shows whether the application is installed or not
struct NotificationImage
{
    QImage image;
};

QDBusArgument& operator<<(QDBusArgument& argument, const NotificationImage& image)
{
    QImage rgba = image.image.convertToFormat(QImage::Format_RGBA8888);

    argument.beginStructure();
    argument << rgba.width() << rgba.height() << int(rgba.bytesPerLine()) << true << 8 << 4
             << QByteArray(reinterpret_cast<const char*>(rgba.constBits()), int(rgba.sizeInBytes()));
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, NotificationImage&)
{
    return argument; // Only sent
}

#endif

} // namespace

#if defined(Q_OS_LINUX)
Q_DECLARE_METATYPE(NotificationImage)
#endif

DesktopNotifier::DesktopNotifier(QSystemTrayIcon* trayIcon, QObject* parent)
    : QObject(parent), m_trayIcon(trayIcon), m_dbusConnected(false)
{
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [this]() {
        if (m_trayAction)
        {
            m_trayAction();
        }
    });

#if defined(Q_OS_LINUX)
    qDBusRegisterMetaType<NotificationImage>();

    QDBusConnection bus = QDBusConnection::sessionBus();
    m_dbusConnected = bus.connect(NotificationsService, NotificationsPath, NotificationsInterface, "ActionInvoked",
                                  this, SLOT(invokeAction(uint, QString)));
#endif
}

void DesktopNotifier::notify(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                             const QString& actionText, const std::function<void()>& action)
{
    if (notifyOverDBus(title, text, icon, actionText, action))
    {
        return;
    }

    showTrayMessage(title, text, icon, action);
}

void DesktopNotifier::showTrayMessage(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                                      const std::function<void()>& action)
{
    m_trayAction = action;

    // The icon of the application tells more than the generic one for information
    if (icon == QSystemTrayIcon::Information)
    {
        m_trayIcon->showMessage(title, text, QApplication::windowIcon());
    }
    else
    {
        m_trayIcon->showMessage(title, text, icon);
    }
}

bool DesktopNotifier::notifyOverDBus(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                                     const QString& actionText, const std::function<void()>& action)
{
#if defined(Q_OS_LINUX)
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!m_dbusConnected || !bus.interface() || !bus.interface()->isServiceRegistered(NotificationsService))
    {
        return false;
    }

    QVariantMap hints;
    hints.insert("desktop-entry", QString(PROJECT_NAME));
    hints.insert("image-data",
                 QVariant::fromValue(NotificationImage{QApplication::windowIcon().pixmap(ImageSize).toImage()}));
    hints.insert("urgency", QVariant::fromValue<uchar>(icon == QSystemTrayIcon::Critical ? 2 : 1));

    // Most services show no button for the default action, those that do label it with the text
    QStringList actions = {DefaultActionKey, actionText};

    QDBusMessage message =
        QDBusMessage::createMethodCall(NotificationsService, NotificationsPath, NotificationsInterface, "Notify");
    message << QString(PROJECT_TITLE) << uint(0) << QString() << title << text << actions << hints << ShowTimeout;

    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, title, text, icon, action]() {
                QDBusPendingReply<uint> reply = *watcher;
                watcher->deleteLater();

                if (reply.isError())
                {
                    // Gone in between, the tray icon still tells it
                    showTrayMessage(title, text, icon, action);
                    return;
                }

                m_actions.insert(reply.value(), action);
            });

    return true;
#else
    Q_UNUSED(title);
    Q_UNUSED(text);
    Q_UNUSED(icon);
    Q_UNUSED(actionText);
    Q_UNUSED(action);
    return false;
#endif
}

void DesktopNotifier::invokeAction(uint id, const QString& actionKey)
{
    std::function<void()> action = m_actions.value(id);

    if (action && actionKey == DefaultActionKey)
    {
        action();
    }
}
