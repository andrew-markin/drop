#ifndef DROP_H
#define DROP_H

#include <QHostAddress>
#include <QWidget>

#include "LocalNetwork.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;
class QrCodeWidget;
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

private:
    void updateStatus();

private:
    QByteArray m_secret;
    QList<LocalNetwork::Candidate> m_candidates;

    WebServer* m_webServer;
    QString m_link;

    QrCodeWidget* m_qrCodeWidget;
    QLabel* m_addressLabel;
    QPushButton* m_copyButton;
    QLabel* m_networkLabel;
    QComboBox* m_networkComboBox;
    QLabel* m_statusLabel;
    QTimer* m_refreshTimer;
};

#endif // DROP_H
