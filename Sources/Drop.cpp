#include "Drop.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "Crypto.h"
#include "NetworkComboBox.h"
#include "QrCodeDialog.h"
#include "Translator.h"
#include "Uploads.h"
#include "WebServer.h"

namespace {

// Network changes are rare, but a laptop may join Wi-Fi after the start
static const int RefreshCandidatesTimeout = 3000; // 3s

static const int CopiedFeedbackTimeout = 1500; // 1.5s

// A device announces all picked files at once, but more may be picked right after, or come from
// another device. The drop is over when everything stays complete for a while.
static const int CompleteQuietTimeout = 5000; // 5s

enum UploadColumn
{
    UploadNameColumn,
    UploadSizeColumn,
    UploadStatusColumn,
};

QString describeCandidate(const LocalNetwork::Candidate& candidate)
{
    return QString("%1 (%2)").arg(candidate.address.toString(), candidate.humanReadableName);
}

} // namespace

Drop::Drop(QWidget* parent)
    : QWidget(parent), m_secret(Crypto::randomBytes(Crypto::SecretSize)), m_webServer(nullptr), m_notifiedCount(0)
{
    setWindowTitle(PROJECT_TITLE);

    m_uploads = new Uploads(QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
                            Uploads::DefaultChunkSize, this);

    // The link holds the secret, so it is copied whole rather than shown
    m_copyButton = new QPushButton(tr("Copy Link"), this);
    m_copyButton->setToolTip(tr("Send the link to a device in a messenger"));
    connect(m_copyButton, &QPushButton::clicked, this, &Drop::copyLink);

    m_qrCodeButton = new QPushButton(tr("Show QR"), this);
    m_qrCodeButton->setToolTip(tr("Scan the QR code with a phone"));
    connect(m_qrCodeButton, &QPushButton::clicked, this, &Drop::showQrCode);

    m_networkComboBox = new NetworkComboBox(this);
    connect(m_networkComboBox, &QComboBox::activated, this, &Drop::restartWebServer);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    m_uploadsTreeWidget = new QTreeWidget(this);
    m_uploadsTreeWidget->setHeaderLabels({tr("Name"), tr("Size"), tr("Status")});
    m_uploadsTreeWidget->setRootIsDecorated(false);
    m_uploadsTreeWidget->setUniformRowHeights(true);
    m_uploadsTreeWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_uploadsTreeWidget->setFocusPolicy(Qt::NoFocus);
    m_uploadsTreeWidget->header()->setStretchLastSection(false);
    m_uploadsTreeWidget->header()->setSectionResizeMode(UploadNameColumn, QHeaderView::Stretch);
    m_uploadsTreeWidget->header()->setSectionResizeMode(UploadSizeColumn, QHeaderView::ResizeToContents);
    m_uploadsTreeWidget->header()->setSectionResizeMode(UploadStatusColumn, QHeaderView::ResizeToContents);

    m_totalProgressBar = new QProgressBar(this);
    m_totalProgressBar->setRange(0, 1000);
    m_totalProgressBar->setTextVisible(false);
    m_totalProgressBar->hide();

    m_openButton = new QPushButton(tr("Open Folder"), this);
    m_openButton->hide();
    connect(m_openButton, &QPushButton::clicked, this, &Drop::openDropDirectory);

    connect(m_uploads, &Uploads::uploadAdded, this, &Drop::addUpload);
    connect(m_uploads, &Uploads::uploadChanged, this, &Drop::updateUpload);
    connect(m_uploads, &Uploads::dropDirectoryCreated, m_openButton, &QPushButton::show);

    // Only to tell when a drop is over: the application quits with its window
    m_trayIcon = new QSystemTrayIcon(QApplication::windowIcon(), this);
    m_trayIcon->setToolTip(PROJECT_TITLE);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &Drop::openDropDirectory);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });

    m_completeTimer = new QTimer(this);
    m_completeTimer->setSingleShot(true);
    m_completeTimer->setInterval(CompleteQuietTimeout);
    connect(m_completeTimer, &QTimer::timeout, this, &Drop::maybeNotifyComplete);
    connect(m_uploads, &Uploads::uploadAdded, m_completeTimer, qOverload<>(&QTimer::start));
    connect(m_uploads, &Uploads::uploadChanged, m_completeTimer, qOverload<>(&QTimer::start));

    // Styles make combo boxes and buttons a little different in height, in one row they fill it alike
    for (QWidget* widget : {static_cast<QWidget*>(m_networkComboBox), static_cast<QWidget*>(m_copyButton),
                            static_cast<QWidget*>(m_qrCodeButton)})
    {
        widget->setSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    }

    QHBoxLayout* linkLayout = new QHBoxLayout();
    linkLayout->addWidget(m_networkComboBox, 1);
    linkLayout->addWidget(m_copyButton);
    linkLayout->addWidget(m_qrCodeButton);

    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->addWidget(m_totalProgressBar, 1);
    progressLayout->addWidget(m_openButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(linkLayout);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_uploadsTreeWidget, 1);
    layout->addLayout(progressLayout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(RefreshCandidatesTimeout);
    connect(m_refreshTimer, &QTimer::timeout, this, &Drop::refreshCandidates);
    m_refreshTimer->start();

    refreshCandidates();
    resize(600, 480);
}

void Drop::refreshCandidates()
{
    QList<LocalNetwork::Candidate> candidates = LocalNetwork::getCandidates();

    QStringList descriptions;

    for (const LocalNetwork::Candidate& candidate : candidates)
    {
        descriptions.append(describeCandidate(candidate));
    }

    QStringList currentDescriptions;

    for (int i = 0; i < m_networkComboBox->count(); i++)
    {
        currentDescriptions.append(m_networkComboBox->itemText(i));
    }

    if (descriptions == currentDescriptions)
    {
        return;
    }

    // Keep serving where devices already are, as long as that address is still there
    QHostAddress currentAddress = m_webServer ? m_webServer->serverAddress() : QHostAddress();
    int currentIndex = 0;

    for (int i = 0; i < candidates.size(); i++)
    {
        if (candidates.at(i).address == currentAddress)
        {
            currentIndex = i;
        }
    }

    m_candidates = candidates;

    m_networkComboBox->clear();
    m_networkComboBox->addItems(descriptions);
    m_networkComboBox->setCurrentIndex(currentIndex);

    // Shown even with one network, it tells which one devices have to join
    m_networkComboBox->setEnabled(!candidates.isEmpty());

    if (candidates.value(currentIndex).address != currentAddress || currentAddress.isNull())
    {
        restartWebServer();
    }
}

void Drop::restartWebServer()
{
    delete m_webServer;
    m_webServer = nullptr;
    m_link.clear();

    int index = m_networkComboBox->currentIndex();

    if (index >= 0 && index < m_candidates.size())
    {
        m_webServer = new WebServer(":/Web", m_uploads, m_secret, this);

        // A new random port every time, so a busy port is never a problem
        if (m_webServer->start(m_candidates.at(index).address))
        {
            m_link = LocalNetwork::makeLink(m_webServer->serverAddress(), m_webServer->serverPort(), m_secret);
        }
    }

    updateStatus();
}

void Drop::copyLink()
{
    if (m_link.isEmpty())
    {
        return;
    }

    QApplication::clipboard()->setText(m_link);

    m_copyButton->setText(tr("Copied"));
    QTimer::singleShot(CopiedFeedbackTimeout, m_copyButton, [this]() { m_copyButton->setText(tr("Copy Link")); });
}

void Drop::showQrCode()
{
    if (m_link.isEmpty())
    {
        return;
    }

    m_qrCodeDialog = new QrCodeDialog(this);
    m_qrCodeDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_qrCodeDialog->setLink(m_link);
    m_qrCodeDialog->open();
}

void Drop::openDropDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_uploads->dropDirectoryPath()));
}

void Drop::addUpload(const QByteArray& id)
{
    Uploads::Upload upload = m_uploads->upload(id);

    QTreeWidgetItem* item = new QTreeWidgetItem(m_uploadsTreeWidget);
    item->setText(UploadNameColumn, upload.name);
    item->setToolTip(UploadNameColumn, upload.name);
    item->setText(UploadSizeColumn, QLocale().formattedDataSize(upload.size, 1, QLocale::DataSizeSIFormat));
    item->setTextAlignment(UploadSizeColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(UploadStatusColumn, Qt::AlignRight | Qt::AlignVCenter);
    m_uploadItems.insert(id, item);

    m_totalProgressBar->show();

    updateUpload(id);
}

void Drop::updateUpload(const QByteArray& id)
{
    QTreeWidgetItem* item = m_uploadItems.value(id);

    if (!item)
    {
        return;
    }

    Uploads::Upload upload = m_uploads->upload(id);

    if (upload.complete)
    {
        item->setText(UploadStatusColumn, tr("Done"));
        item->setToolTip(UploadNameColumn, upload.path);
    }
    else
    {
        qint64 percent = upload.size > 0 ? upload.receivedSize(m_uploads->chunkSize()) * 100 / upload.size : 0;
        item->setText(UploadStatusColumn, QString("%1%").arg(percent));
    }

    m_uploadsTreeWidget->scrollToItem(item);
    updateTotalProgress();
}

void Drop::maybeNotifyComplete()
{
    int completeCount = 0;
    const QList<Uploads::Upload> uploads = m_uploads->uploads();

    for (const Uploads::Upload& upload : uploads)
    {
        if (!upload.complete)
        {
            return; // Still coming, or the device went away and may come back
        }

        completeCount++;
    }

    int newCount = completeCount - m_notifiedCount;

    if (newCount <= 0)
    {
        return;
    }

    m_notifiedCount = completeCount;

    m_trayIcon->showMessage(tr("Drop is complete"), tr("Received %n file(s). Click to open the folder.", "", newCount),
                            QApplication::windowIcon());
    QApplication::alert(this);
}

void Drop::updateTotalProgress()
{
    qint64 totalSize = 0;
    qint64 receivedSize = 0;
    int completeCount = 0;

    const QList<Uploads::Upload> uploads = m_uploads->uploads();

    for (const Uploads::Upload& upload : uploads)
    {
        totalSize += upload.size;
        receivedSize += upload.receivedSize(m_uploads->chunkSize());
        completeCount += upload.complete ? 1 : 0;
    }

    m_totalProgressBar->setValue(totalSize > 0 ? int(receivedSize * 1000 / totalSize) : 1000);
    m_totalProgressBar->setToolTip(tr("%1 of %n file(s) received", "", uploads.size()).arg(completeCount));
}

void Drop::updateStatus()
{
    m_copyButton->setEnabled(!m_link.isEmpty());
    m_qrCodeButton->setEnabled(!m_link.isEmpty());

    // The code follows the server when it moves to another network, and goes away with it
    if (m_qrCodeDialog)
    {
        if (m_link.isEmpty())
        {
            m_qrCodeDialog->close();
        }
        else
        {
            m_qrCodeDialog->setLink(m_link);
        }
    }

    if (!m_link.isEmpty())
    {
        setStatus(QString());
    }
    else if (m_candidates.isEmpty())
    {
        setStatus(tr("This computer is not connected to a local network. "
                     "Connect it to the same Wi-Fi or wired network as the devices."));
    }
    else
    {
        setStatus(tr("Unable to start the server: %1").arg(m_webServer->errorString()));
    }
}

// Only problems are told, the rest of the time the space goes to the files
void Drop::setStatus(const QString& text)
{
    m_statusLabel->setText(text);
    m_statusLabel->setVisible(!text.isEmpty());
}

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);

    QApplication application(argc, argv);
    QApplication::setWindowIcon(QIcon(":/Drop.svg"));

    Translator translator(QLocale::system());
    QApplication::installTranslator(&translator);

    if (!Crypto::init())
    {
        QMessageBox::critical(nullptr, PROJECT_TITLE, QObject::tr("Unable to initialize the cryptography library."));
        return EXIT_FAILURE;
    }

    Drop drop;
    drop.show();

    return application.exec();
}
