#include "AlertWidget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include "Colors.h"

namespace {

// Of the error color over the background: enough to notice, light enough to read the text on
static const qreal BackgroundTint = 0.15;
static const qreal BorderTint = 0.6;

// Of the error color in the text: reddish, still as readable as the rest
static const qreal TextTint = 0.35;

// Barely rounded, so it looks at home with the square and the round controls of any platform
static const qreal Radius = 2;

static const int HorizontalPadding = 12;
static const int VerticalPadding = 10;

} // namespace

AlertWidget::AlertWidget(QWidget* parent) : QWidget(parent)
{
    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(HorizontalPadding, VerticalPadding, HorizontalPadding, VerticalPadding);
    layout->addWidget(m_textLabel);

    updateTextColor();
    hide();
}

void AlertWidget::setText(const QString& text)
{
    m_textLabel->setText(text);
    setVisible(!text.isEmpty());
}

void AlertWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
    {
        updateTextColor();
    }
}

void AlertWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QColor background = palette().color(QPalette::Base);
    QColor error = Colors::error(palette());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Colors::mix(background, error, BorderTint), 1));
    painter.setBrush(Colors::mix(background, error, BackgroundTint));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Radius, Radius);
}

void AlertWidget::updateTextColor()
{
    QPalette labelPalette = palette();

    for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    {
        labelPalette.setColor(group, QPalette::WindowText,
                              Colors::mix(palette().color(group, QPalette::Text), Colors::error(palette(), group),
                                          TextTint));
    }

    m_textLabel->setPalette(labelPalette);
}
