#include "NetworkComboBox.h"

#include <QStyleOptionComboBox>
#include <QStylePainter>

NetworkComboBox::NetworkComboBox(QWidget* parent) : QComboBox(parent)
{
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

QSize NetworkComboBox::sizeHint() const
{
    QSize size = QComboBox::sizeHint();
    size.rwidth() += fontMetrics().horizontalAdvance(prefixed(QString()));
    return size;
}

QSize NetworkComboBox::minimumSizeHint() const
{
    return sizeHint();
}

QString NetworkComboBox::prefixed(const QString& text) const
{
    return tr("Network: %1").arg(text);
}

void NetworkComboBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentText = prefixed(option.currentText);

    QStylePainter painter(this);
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}
