#ifndef TRAYICON_H
#define TRAYICON_H

#include <QAbstractNativeEventFilter>
#include <QSystemTrayIcon>

// A monochrome drop, as the icons of the system next to it: light on a dark panel, dark on a light
// one. macOS tints it by itself, elsewhere it follows the theme of the panel as far as it is known:
// of the taskbar on Windows, of applications on Linux desktops.
class TrayIcon : public QSystemTrayIcon, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void updateIcon();
};

#endif // TRAYICON_H
