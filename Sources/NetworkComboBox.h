#ifndef NETWORKCOMBOBOX_H
#define NETWORKCOMBOBOX_H

#include <QComboBox>

// Tells what it is chosen for in the box itself, the list below keeps the bare networks
class NetworkComboBox : public QComboBox
{
    Q_OBJECT

public:
    NetworkComboBox(QWidget* parent = nullptr);

    // Wide enough for the longest network with the prefix, the window is never narrower
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString prefixed(const QString& text) const;
};

#endif // NETWORKCOMBOBOX_H
