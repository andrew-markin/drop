#ifndef NETWORKCOMBOBOX_H
#define NETWORKCOMBOBOX_H

#include <QComboBox>

// The networks to serve on, never narrower than the longest of them: the window takes its least
// width from the row this box is in
class NetworkComboBox : public QComboBox
{
    Q_OBJECT

public:
    NetworkComboBox(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
};

#endif // NETWORKCOMBOBOX_H
