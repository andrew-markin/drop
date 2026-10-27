#include "Drop.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QHeaderView>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "QrCodeWidget.h"
#include "Uploads.h"
#include "WebServer.h"

namespace {

static const int SecretSize = 32;

// Network changes are rare, but a laptop may join Wi-Fi after the start
static const int RefreshCandidatesTimeout = 3000; // 3s

static const int CopiedFeedbackTimeout = 1500; // 1.5s

QByteArray generateSecret()
{
    QByteArray secret(SecretSize, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(secret.data()), SecretSize / sizeof(quint32));
    return secret;
}

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

Drop::Drop(QWidget* parent) : QWidget(parent), m_secret(generateSecret()), m_webServer(nullptr)
{
    setWindowTitle(PROJECT_TITLE);

    m_uploads = new Uploads(QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
                            Uploads::DefaultChunkSize, this);

    m_qrCodeWidget = new QrCodeWidget(this);

    m_addressLabel = new QLabel(this);
    m_addressLabel->setAlignment(Qt::AlignCenter);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont addressFont = m_addressLabel->font();
    addressFont.setPointSizeF(addressFont.pointSizeF() * 1.4);
    m_addressLabel->setFont(addressFont);

    // The link holds the secret, so it is copied whole rather than shown
    m_copyButton = new QPushButton(tr("Copy Link"), this);
    m_copyButton->setToolTip(tr("Send the link to a device in a messenger, or scan the QR code with it"));
    connect(m_copyButton, &QPushButton::clicked, this, &Drop::copyLink);

    m_networkComboBox = new QComboBox(this);
    connect(m_networkComboBox, &QComboBox::activated, this, &Drop::restartWebServer);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

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
    m_uploadsTreeWidget->hide();

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

    QFormLayout* networkLayout = new QFormLayout();
    networkLayout->addRow(tr("Network:"), m_networkComboBox);
    m_networkLabel = qobject_cast<QLabel*>(networkLayout->labelForField(m_networkComboBox));

    QHBoxLayout* copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();

    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->addWidget(m_totalProgressBar, 1);
    progressLayout->addWidget(m_openButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_qrCodeWidget, 1);
    layout->addWidget(m_addressLabel);
    layout->addLayout(copyLayout);
    layout->addWidget(m_statusLabel);
    layout->addLayout(networkLayout);
    layout->addWidget(m_uploadsTreeWidget, 1);
    layout->addLayout(progressLayout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(RefreshCandidatesTimeout);
    connect(m_refreshTimer, &QTimer::timeout, this, &Drop::refreshCandidates);
    m_refreshTimer->start();

    refreshCandidates();
    resize(440, 720);
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

    // Choosing makes sense only when there is something to choose from
    m_networkComboBox->setVisible(candidates.size() > 1);
    m_networkLabel->setVisible(candidates.size() > 1);

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
        m_webServer = new WebServer(":/Web", m_uploads, this);

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
    item->setText(UploadSizeColumn, QLocale().formattedDataSize(upload.size));
    item->setTextAlignment(UploadSizeColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(UploadStatusColumn, Qt::AlignRight | Qt::AlignVCenter);
    m_uploadItems.insert(id, item);

    m_uploadsTreeWidget->show();
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
    m_totalProgressBar->setToolTip(tr("%1 of %2 files received").arg(completeCount).arg(uploads.size()));
}

void Drop::updateStatus()
{
    m_qrCodeWidget->setText(m_link);
    m_copyButton->setEnabled(!m_link.isEmpty());

    if (!m_link.isEmpty())
    {
        m_addressLabel->setText(QString("http://%1:%2").arg(m_webServer->serverAddress().toString())
                                    .arg(m_webServer->serverPort()));
        m_statusLabel->setText(tr("Scan the QR code with a phone connected to the same network."));
    }
    else if (m_candidates.isEmpty())
    {
        m_addressLabel->clear();
        m_statusLabel->setText(tr("This computer is not connected to a local network. "
                                  "Connect it to the same Wi-Fi or wired network as the devices."));
    }
    else
    {
        m_addressLabel->clear();
        m_statusLabel->setText(tr("Unable to start the server: %1").arg(m_webServer->errorString()));
    }
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

    Drop drop;
    drop.show();

    return application.exec();
}
