#ifndef DROP_H
#define DROP_H

#include <QHash>
#include <QHostAddress>
#include <QWidget>

#include "LocalNetwork.h"

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QrCodeWidget;
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
    void openDropDirectory();

    void addUpload(const QByteArray& id);
    void updateUpload(const QByteArray& id);

private:
    void updateStatus();
    void updateTotalProgress();

private:
    QByteArray m_secret;
    QList<LocalNetwork::Candidate> m_candidates;

    Uploads* m_uploads;

    WebServer* m_webServer;
    QString m_link;

    QrCodeWidget* m_qrCodeWidget;
    QLabel* m_addressLabel;
    QPushButton* m_copyButton;
    QLabel* m_networkLabel;
    QComboBox* m_networkComboBox;
    QLabel* m_statusLabel;
    QTimer* m_refreshTimer;

    QTreeWidget* m_uploadsTreeWidget;
    QHash<QByteArray, QTreeWidgetItem*> m_uploadItems;
    QProgressBar* m_totalProgressBar;
    QPushButton* m_openButton;
};

#endif // DROP_H
