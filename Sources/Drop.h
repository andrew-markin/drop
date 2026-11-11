#ifndef DROP_H
#define DROP_H

#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QWidget>

#include "LocalNetwork.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSystemTrayIcon;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QrCodeDialog;
class Uploads;
class WebServer;

class Drop : public QWidget
{
    Q_OBJECT

public:
    Drop(QWidget* parent = nullptr);

private slots:
    void refreshCandidates();
    void restartWebServer();
    void copyLink();
    void showQrCode();
    void openDropDirectory();

    void addUpload(const QByteArray& id);
    void updateUpload(const QByteArray& id);

    void maybeNotifyComplete();

private:
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

    QTreeWidget* m_uploadsTreeWidget;
    QHash<QByteArray, QTreeWidgetItem*> m_uploadItems;
    QPushButton* m_openButton;

    QSystemTrayIcon* m_trayIcon;
    QTimer* m_completeTimer;
    int m_notifiedCount;
};

#endif // DROP_H
