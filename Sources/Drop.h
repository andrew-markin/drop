#ifndef DROP_H
#define DROP_H

#include <QElapsedTimer>
#include <QHostAddress>
#include <QPointer>
#include <QWidget>

#include "LocalNetwork.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSystemTrayIcon;
class QTimer;
class DesktopNotifier;
class QrCodeDialog;
class Uploads;
class UploadsEstimate;
class UploadsModel;
class UploadsView;
class WebServer;

class Drop : public QWidget
{
    Q_OBJECT

public:
    Drop(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void refreshCandidates();
    void restartWebServer();
    void copyLink();
    void showQrCode();
    void showWindow();
    void openDropDirectory();

    void scrollToUpload(const QByteArray& id);

    void maybeNotifyComplete();
    void updateEstimate();
    void maybeNotifyNoSpace(const QByteArray& id);

private:
    void placeFirstTime();
    void updateStatus();
    void setStatus(const QString& text);

private:
    QByteArray m_secret;
    QList<LocalNetwork::Candidate> m_candidates;

    Uploads* m_uploads;

    WebServer* m_webServer;
    QString m_link;

    QPushButton* m_copyButton;
    QPushButton* m_qrCodeButton;
    QPointer<QrCodeDialog> m_qrCodeDialog;
    QComboBox* m_networkComboBox;
    QLabel* m_statusLabel;
    QTimer* m_refreshTimer;

    UploadsModel* m_uploadsModel;
    UploadsView* m_uploadsView;
    QPushButton* m_openButton;

    QSystemTrayIcon* m_trayIcon;
    DesktopNotifier* m_notifier;
    QTimer* m_completeTimer;

    UploadsEstimate* m_estimate;
    QElapsedTimer m_clock;
    QTimer* m_estimateTimer;
    int m_notifiedCount;
    bool m_noSpaceNotified;
};

#endif // DROP_H
