#ifndef DESKTOPNOTIFIER_H
#define DESKTOPNOTIFIER_H

#include <QHash>
#include <QObject>
#include <QSystemTrayIcon>

#include <functional>

// Notifications that do something when clicked. On Linux Qt sends them to the notification service
// with no actions, so a click there reaches nobody: they are sent here with a default action
// instead. Elsewhere, or with no such service, they go through the tray icon.
class DesktopNotifier : public QObject
{
    Q_OBJECT

public:
    DesktopNotifier(QSystemTrayIcon* trayIcon, QObject* parent = nullptr);

    void notify(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                const QString& actionText, const std::function<void()>& action);

private slots:
    void invokeAction(uint id, const QString& actionKey);

private:
    void showTrayMessage(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                         const std::function<void()>& action);
    bool notifyOverDBus(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon,
                        const QString& actionText, const std::function<void()>& action);

private:
    QSystemTrayIcon* m_trayIcon;
    std::function<void()> m_trayAction; // Of the last message of the tray icon

    bool m_dbusConnected;
    // By the ids of the notifications sent, kept after they close: some services tell the closing
    // before the click. A drop makes a few of them at most.
    QHash<uint, std::function<void()>> m_actions;
};

#endif // DESKTOPNOTIFIER_H
