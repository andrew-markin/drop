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
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

    // One pixel per module, with the quiet zone around the code that scanners need
    static QImage makeImage(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
};

#endif // QRCODEWIDGET_H
