#include "Drop.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QIcon>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "AlertWidget.h"
#include "Crypto.h"
#include "DesktopNotifier.h"
#include "NetworkComboBox.h"
#include "QrCodeDialog.h"
#include "SingleInstance.h"
#include "Translator.h"
#include "Uploads.h"
#include "UploadsEstimate.h"
#include "UploadsModel.h"
#include "UploadsView.h"
#include "WebServer.h"

namespace {

// Network changes are rare, but a laptop may join Wi-Fi after the start
static const int RefreshCandidatesTimeout = 3000; // 3s

static const int CopiedFeedbackTimeout = 1500; // 1.5s

// A device announces all picked files at once, but more may be picked right after, or come from
// another device. The drop is over when everything stays complete for a while.
static const int CompleteQuietTimeout = 5000; // 5s

// Often enough to notice a stalled drop, the estimate itself changes what it shows rarely
static const int EstimateInterval = 1000; // 1s

static const char* const GeometryKey = "Window/Geometry";
static const qreal FirstWidthFactor = 1.1; // Of the minimum width

// The sizes taskbars, docks and menus ask for. Also the ones installed into the icon theme on Linux,
// see PROJECT_ICON_SIZES in CMakeLists.txt.
static const QList<int> IconSizes = {16, 22, 24, 32, 48, 64, 128, 256};

// Drawn from the SVG at each size: given only a few sizes, a taskbar scales the nearest one and
// the thin outline goes blurry. Drawn in the pixels of the screens too, as Qt scales the sizes by
// their scale factor, 16 becomes 20 at 125%.
QIcon makeApplicationIcon(const QString& path)
{
    QIcon svgIcon(path);
    QIcon icon;

    QList<qreal> ratios = {1.0};
    const QList<QScreen*> screens = QGuiApplication::screens();

    for (const QScreen* screen : screens)
    {
        if (!ratios.contains(screen->devicePixelRatio()))
        {
            ratios.append(screen->devicePixelRatio());
        }
    }

    for (qreal ratio : std::as_const(ratios))
    {
        for (int size : IconSizes)
        {
            icon.addPixmap(svgIcon.pixmap(QSize(size, size), ratio));
        }
    }

    return icon;
}

// The network, not the address of this computer: that is what a phone has to be on. The list reads
// as a list of networks, the closed box tells what it is for.
QString describeCandidate(const LocalNetwork::Candidate& candidate)
{
    return Drop::tr("Network: %1")
        .arg(QString("%1 (%2)").arg(LocalNetwork::describeNetwork(candidate.address, candidate.prefixLength),
                                    candidate.humanReadableName));
}

} // namespace

Drop::Drop(QWidget* parent)
    : QWidget(parent), m_secret(Crypto::randomBytes(Crypto::SecretSize)), m_webServer(nullptr), m_notifiedCount(0),
      m_noSpaceNotified(false)
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

    // Under the table, so the buttons above stay where they are
    m_alertWidget = new AlertWidget(this);

    m_uploadsModel = new UploadsModel(m_uploads, this);
    m_uploadsView = new UploadsView(m_uploadsModel, this);

    // There from the start, so the row does not change when the folder appears with the first file
    m_openButton = new QPushButton(tr("Open Folder"), this);
    m_openButton->setEnabled(false);
    connect(m_openButton, &QPushButton::clicked, this, &Drop::openDropDirectory);

    // The table follows the file being received, files only announced just add up at the bottom
    connect(m_uploads, &Uploads::uploadChanged, this, &Drop::scrollToUpload);
    connect(m_uploads, &Uploads::uploadChanged, this, &Drop::maybeNotifyNoSpace);

    // In the title, so the taskbar or the dock tells it too
    m_estimate = new UploadsEstimate(this);
    m_clock.start();

    m_estimateTimer = new QTimer(this);
    m_estimateTimer->setInterval(EstimateInterval);
    connect(m_estimateTimer, &QTimer::timeout, this, &Drop::updateEstimate);
    connect(m_uploads, &Uploads::uploadAdded, this, &Drop::updateEstimate);
    connect(m_uploads, &Uploads::uploadChanged, this, &Drop::updateEstimate);
    connect(m_uploads, &Uploads::dropDirectoryCreated, m_openButton, [this]() { m_openButton->setEnabled(true); });

    // Only to tell when a drop is over: the application quits with its window
    // The SVG itself: the tray picks its size from the few Qt offers for it, as for other
    // applications, while a full set of sizes made it pick a small one and look smaller than them
    m_trayIcon = new QSystemTrayIcon(QIcon(":/Drop.svg"), this);
    m_trayIcon->setToolTip(PROJECT_TITLE);
    m_trayIcon->show();

    m_notifier = new DesktopNotifier(m_trayIcon, this);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &Drop::showWindow);

    m_completeTimer = new QTimer(this);
    m_completeTimer->setSingleShot(true);
    m_completeTimer->setInterval(CompleteQuietTimeout);
    connect(m_completeTimer, &QTimer::timeout, this, &Drop::maybeNotifyComplete);
    connect(m_uploads, &Uploads::uploadAdded, m_completeTimer, qOverload<>(&QTimer::start));
    connect(m_uploads, &Uploads::uploadChanged, m_completeTimer, qOverload<>(&QTimer::start));

    // Styles make combo boxes and buttons a little different in height, in one row they fill it alike
    for (QWidget* widget : {static_cast<QWidget*>(m_networkComboBox), static_cast<QWidget*>(m_copyButton),
                            static_cast<QWidget*>(m_qrCodeButton), static_cast<QWidget*>(m_openButton)})
    {
        widget->setSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    }

    QHBoxLayout* linkLayout = new QHBoxLayout();
    linkLayout->addWidget(m_networkComboBox, 1);
    linkLayout->addWidget(m_copyButton);
    linkLayout->addWidget(m_qrCodeButton);
    linkLayout->addWidget(m_openButton);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(linkLayout);
    layout->addWidget(m_uploadsView, 1);
    layout->addWidget(m_alertWidget);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(RefreshCandidatesTimeout);
    connect(m_refreshTimer, &QTimer::timeout, this, &Drop::refreshCandidates);
    m_refreshTimer->start();

    refreshCandidates();

    // With no network at the start there is no change to notice, the window still has to tell it
    if (!m_webServer)
    {
        updateStatus();
    }

    // Where it was left last time, Qt keeps it on the screens there are now
    if (!restoreGeometry(QSettings().value(GeometryKey).toByteArray()))
    {
        placeFirstTime();
    }
}

// A little wider than the least the row of buttons needs, about square, in the middle of the screen
// the mouse is on
void Drop::placeFirstTime()
{
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());

    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }

    QRect available = screen->availableGeometry();

    int width = qMin(qRound(minimumSizeHint().width() * FirstWidthFactor), available.width());
    int height = qMin(width, available.height());

    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, QSize(width, height), available));
}

void Drop::closeEvent(QCloseEvent* event)
{
    QSettings().setValue(GeometryKey, saveGeometry());
    QWidget::closeEvent(event);
}

void Drop::refreshCandidates()
{
    QList<LocalNetwork::Candidate> candidates = LocalNetwork::getCandidates();

    // The addresses too: a new address in the same network looks the same in the list
    QStringList descriptions, addresses;

    for (const LocalNetwork::Candidate& candidate : candidates)
    {
        descriptions.append(describeCandidate(candidate));
        addresses.append(candidate.address.toString());
    }

    QStringList currentDescriptions, currentAddresses;

    for (int i = 0; i < m_networkComboBox->count(); i++)
    {
        currentDescriptions.append(m_networkComboBox->itemText(i));
        currentAddresses.append(m_networkComboBox->itemData(i).toString());
    }

    if (descriptions == currentDescriptions && addresses == currentAddresses)
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

    for (int i = 0; i < candidates.size(); i++)
    {
        m_networkComboBox->addItem(descriptions.at(i), addresses.at(i));
        m_networkComboBox->setItemData(i, tr("The address of this computer: %1").arg(addresses.at(i)), Qt::ToolTipRole);
    }

    m_networkComboBox->setCurrentIndex(currentIndex);

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

// Out of the taskbar if minimized, as large as it was otherwise
void Drop::showWindow()
{
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    raise();
    activateWindow();

    // Some window managers keep other applications from taking the focus, the taskbar tells then
    QApplication::alert(this);
}

void Drop::openDropDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_uploads->dropDirectoryPath()));
}

void Drop::scrollToUpload(const QByteArray& id)
{
    m_uploadsView->scrollTo(m_uploadsModel->indexOf(id));
}

// The device stops sending when the disk is full, so the window may be the only place to learn it
void Drop::maybeNotifyNoSpace(const QByteArray& id)
{
    Uploads::Error error = m_uploads->upload(id).error;

    if (error == Uploads::Error::None)
    {
        m_noSpaceNotified = false; // Space was freed, a full disk again is news again
        return;
    }

    if (error != Uploads::Error::NoSpace || m_noSpaceNotified)
    {
        return;
    }

    m_noSpaceNotified = true;

    m_notifier->notify(tr("No space left on the disk"),
                       tr("Drop can't receive more files. Free some space and pick the files on the device again."),
                       QSystemTrayIcon::Warning, tr("Show"), [this]() { showWindow(); });
    QApplication::alert(this);
}

void Drop::updateEstimate()
{
    qint64 totalSize = 0;
    qint64 receivedSize = 0;

    const QList<Uploads::Upload> uploads = m_uploads->uploads();

    for (const Uploads::Upload& upload : uploads)
    {
        // A file that does not fit will not come, it would hold the drop back forever
        if (!upload.complete && upload.error == Uploads::Error::NoSpace)
        {
            continue;
        }

        totalSize += upload.size;
        receivedSize += upload.receivedSize(m_uploads->chunkSize());
    }

    m_estimate->update(m_clock.elapsed(), totalSize, receivedSize);

    if (!m_estimate->isActive())
    {
        m_estimateTimer->stop();
        setWindowTitle(PROJECT_TITLE);
        return;
    }

    if (!m_estimateTimer->isActive())
    {
        m_estimateTimer->start();
    }

    // The progress first: a narrow taskbar button cuts the end of the title
    QString progress = QString("%1%").arg(m_estimate->percent());
    QString remaining = m_estimate->remainingText();

    if (!remaining.isEmpty())
    {
        progress = QString("%1 – %2").arg(progress, remaining);
    }

    setWindowTitle(QString("%1 – %2").arg(progress, PROJECT_TITLE));
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

    m_notifier->notify(tr("Drop is complete"), tr("Received %n file(s). Click to open the folder.", "", newCount),
                       QSystemTrayIcon::Information, tr("Open Folder"), [this]() { openDropDirectory(); });
    QApplication::alert(this);
}

void Drop::updateStatus()
{
    // Shown even with one network, it tells which one devices have to join
    m_networkComboBox->setEnabled(!m_candidates.isEmpty());
    m_networkComboBox->setToolTip(m_networkComboBox->currentData(Qt::ToolTipRole).toString());
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
    m_alertWidget->setText(text);
}

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QCoreApplication::setOrganizationName(PROJECT_ORGANIZATION);
    QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);

    QApplication application(argc, argv);
    QApplication::setWindowIcon(makeApplicationIcon(":/Drop.svg"));

    Translator translator(QLocale::system());
    QApplication::installTranslator(&translator);

    // A second start brings up the window of the first one
    SingleInstance singleInstance(SingleInstance::defaultName(), SingleInstance::defaultLockDirectory());

    if (!singleInstance.start())
    {
        return EXIT_SUCCESS;
    }

    if (!Crypto::init())
    {
        QMessageBox::critical(nullptr, PROJECT_TITLE, QObject::tr("Unable to initialize the cryptography library."));
        return EXIT_FAILURE;
    }

    Drop drop;
    drop.show();

    QObject::connect(&singleInstance, &SingleInstance::activationRequested, &drop, &Drop::showWindow);

    return application.exec();
}
