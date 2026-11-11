#include "QrCodeDialog.h"

#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

#include "QrCodeWidget.h"

QrCodeDialog::QrCodeDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("QR Code"));

    // Light as the quiet zone of the code, so the hint reads as part of it, in any theme
    QPalette lightPalette = palette();
    lightPalette.setColor(QPalette::Window, Qt::white);
    lightPalette.setColor(QPalette::WindowText, Qt::black);
    setPalette(lightPalette);
    setAutoFillBackground(true);

    m_qrCodeWidget = new QrCodeWidget(this);

    m_hintLabel = new QLabel(tr("Scan with a phone on the same network"), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);

    // The quiet zone of the code is its margin, and the window is exactly as wide as the code
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_qrCodeWidget);
    m_hintLayout = new QVBoxLayout();
    m_hintLayout->addWidget(m_hintLabel);
    layout->addLayout(m_hintLayout);
}

void QrCodeDialog::setLink(const QString& link)
{
    m_qrCodeWidget->setText(link);

    // The hint keeps to the margins of the code below it, the quiet zone above it is enough. The
    // label width is fixed, a wrapped one would widen the window otherwise.
    int margin = m_qrCodeWidget->quietZoneWidth();
    m_hintLayout->setContentsMargins(margin, 0, margin, margin);
    m_hintLabel->setFixedWidth(m_qrCodeWidget->sizeHint().width() - 2 * margin);
}
