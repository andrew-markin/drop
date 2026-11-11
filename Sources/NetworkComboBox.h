#ifndef NETWORKCOMBOBOX_H
#define NETWORKCOMBOBOX_H

#include <QComboBox>

// Tells what it is chosen for in the box itself, the list below keeps the bare networks
class NetworkComboBox : public QComboBox
{
    Q_OBJECT

public:
    NetworkComboBox(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

#endif // NETWORKCOMBOBOX_H
