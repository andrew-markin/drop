#include "QrCodeWidget.h"

#include <QPainter>

#include <qrcodegen.hpp>

namespace {

// Modules. The standard asks for 4, but phone cameras read the code with 2, and the dialog gives
// the code more room this way.
static const int QuietZoneSize = 2;
// Big enough to scan from the screen at arm's length, small enough for any screen
static const int PreferredSize = 420;

} // namespace

QrCodeWidget::QrCodeWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void QrCodeWidget::setText(const QString& text)
{
    m_image = text.isEmpty() ? QImage() : makeImage(text);
    updateGeometry();
    update();
}

QSize QrCodeWidget::sizeHint() const
{
    if (m_image.isNull())
    {
        return QSize(PreferredSize, PreferredSize);
    }

    int side = m_image.width() * scale();
    return QSize(side, side);
}

QSize QrCodeWidget::minimumSizeHint() const
{
    return sizeHint();
}

int QrCodeWidget::quietZoneWidth() const
{
    return m_image.isNull() ? 0 : QuietZoneSize * scale();
}

QImage QrCodeWidget::makeImage(const QString& text)
{
    // Medium error correction survives a glare on a glossy screen and keeps modules large enough
    qrcodegen::QrCode qrCode = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);

    int size = qrCode.getSize() + 2 * QuietZoneSize;
    QImage image(size, size, QImage::Format_Mono);
    image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
    image.fill(0);

    for (int y = 0; y < qrCode.getSize(); y++)
    {
        for (int x = 0; x < qrCode.getSize(); x++)
        {
            if (qrCode.getModule(x, y))
            {
                image.setPixel(x + QuietZoneSize, y + QuietZoneSize, 1);
            }
        }
    }

    return image;
}

void QrCodeWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (m_image.isNull())
    {
        return;
    }

    int imageSide = m_image.width() * scale();
    QRect target((width() - imageSide) / 2, (height() - imageSide) / 2, imageSide, imageSide);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_image);
}

int QrCodeWidget::scale() const
{
    // Whole pixels per module, so every module is equally sharp
    return qMax(1, PreferredSize / m_image.width());
}
