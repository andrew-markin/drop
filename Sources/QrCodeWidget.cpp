#include "QrCodeWidget.h"

#include <QPainter>

#include <qrcodegen.hpp>

namespace {

static const int QuietZoneSize = 4; // Modules, as the standard requires
static const int PreferredSize = 280;

} // namespace

QrCodeWidget::QrCodeWidget(QWidget* parent) : QWidget(parent)
{
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy.setHeightForWidth(true);
    setSizePolicy(sizePolicy);
    setMinimumSize(160, 160);
}

void QrCodeWidget::setText(const QString& text)
{
    m_image = text.isEmpty() ? QImage() : makeImage(text);
    update();
}

QSize QrCodeWidget::sizeHint() const
{
    return QSize(PreferredSize, PreferredSize);
}

bool QrCodeWidget::hasHeightForWidth() const
{
    return true;
}

int QrCodeWidget::heightForWidth(int width) const
{
    return width;
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

    // Whole pixels per module, so every module is equally sharp
    int side = qMin(width(), height());
    int scale = qMax(1, side / m_image.width());
    int imageSide = m_image.width() * scale;

    QRect target((width() - imageSide) / 2, (height() - imageSide) / 2, imageSide, imageSide);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_image);
}
