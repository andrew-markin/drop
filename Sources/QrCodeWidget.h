#ifndef QRCODEWIDGET_H
#define QRCODEWIDGET_H

#include <QImage>
#include <QWidget>

class QrCodeWidget : public QWidget
{
    Q_OBJECT

public:
    QrCodeWidget(QWidget* parent = nullptr);

    void setText(const QString& text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // The light border around the code, in pixels
    int quietZoneWidth() const;

    // One pixel per module, with the quiet zone around the code that scanners need
    static QImage makeImage(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int scale() const;

private:
    QImage m_image;
};

#endif // QRCODEWIDGET_H
