#include "NetworkComboBox.h"

NetworkComboBox::NetworkComboBox(QWidget* parent) : QComboBox(parent)
{
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

QSize NetworkComboBox::minimumSizeHint() const
{
    return sizeHint();
}
