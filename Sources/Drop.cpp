#include "Drop.h"

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

Drop::Drop(QWidget* parent) : QWidget(parent)
{
    setWindowTitle(PROJECT_TITLE);

    m_urlLabel = new QLabel(tr("Starting..."), this);
    m_urlLabel->setAlignment(Qt::AlignCenter);
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_urlLabel);

    resize(480, 560);
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
