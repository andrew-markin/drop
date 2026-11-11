#ifndef QRCODEDIALOG_H
#define QRCODEDIALOG_H

#include <QDialog>

class QLabel;
class QVBoxLayout;
class QrCodeWidget;

// Shown on demand, so the window is left to the files
class QrCodeDialog : public QDialog
{
    Q_OBJECT

public:
    QrCodeDialog(QWidget* parent = nullptr);

    void setLink(const QString& link);

private:
    QrCodeWidget* m_qrCodeWidget;
    QLabel* m_hintLabel;
    QVBoxLayout* m_hintLayout;
};

#endif // QRCODEDIALOG_H
