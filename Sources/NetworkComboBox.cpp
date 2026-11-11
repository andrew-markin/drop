#include "NetworkComboBox.h"

#include <QStyleOptionComboBox>
#include <QStylePainter>

NetworkComboBox::NetworkComboBox(QWidget* parent) : QComboBox(parent)
{
    // Nothing
}

void NetworkComboBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentText = tr("Network: %1").arg(option.currentText);

    QStylePainter painter(this);
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}
