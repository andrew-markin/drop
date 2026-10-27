#include "Drop.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

#include "QrCodeWidget.h"
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

QString describeCandidate(const LocalNetwork::Candidate& candidate)
{
    return QString("%1 (%2)").arg(candidate.address.toString(), candidate.humanReadableName);
}

} // namespace

Drop::Drop(QWidget* parent) : QWidget(parent), m_secret(generateSecret()), m_webServer(nullptr)
{
    setWindowTitle(PROJECT_TITLE);

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

    QFormLayout* networkLayout = new QFormLayout();
    networkLayout->addRow(tr("Network:"), m_networkComboBox);
    m_networkLabel = qobject_cast<QLabel*>(networkLayout->labelForField(m_networkComboBox));

    QHBoxLayout* copyLayout = new QHBoxLayout();
    copyLayout->addStretch();
    copyLayout->addWidget(m_copyButton);
    copyLayout->addStretch();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_qrCodeWidget, 1);
    layout->addWidget(m_addressLabel);
    layout->addLayout(copyLayout);
    layout->addWidget(m_statusLabel);
    layout->addLayout(networkLayout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(RefreshCandidatesTimeout);
    connect(m_refreshTimer, &QTimer::timeout, this, &Drop::refreshCandidates);
    m_refreshTimer->start();

    refreshCandidates();
    resize(420, 560);
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
        m_webServer = new WebServer(":/Web", this);

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
