#include "UploadsHeaderView.h"

#include <QPainter>

#include "UploadsDelegate.h"

namespace {

// How far the background goes from the rows towards the text: a tint, so it works in any theme
static const qreal BackgroundTint = 0.08;

QColor mix(const QColor& from, const QColor& to, qreal amount)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

} // namespace

UploadsHeaderView::UploadsHeaderView(QWidget* parent) : QHeaderView(Qt::Horizontal, parent)
{
    setSectionsClickable(false);
    setSectionsMovable(false);
    setHighlightSections(false);
}

QColor UploadsHeaderView::backgroundColor(const QPalette& palette)
{
    return mix(palette.color(QPalette::Base), palette.color(QPalette::Text), BackgroundTint);
}

QSize UploadsHeaderView::sizeHint() const
{
    QSize size = QHeaderView::sizeHint();
    size.setHeight(fontMetrics().height() + 2 * UploadsDelegate::VerticalPadding);
    return size;
}

void UploadsHeaderView::paintEvent(QPaintEvent* event)
{
    // Also past the last section
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), backgroundColor(palette()));
    painter.end();

    QHeaderView::paintEvent(event);
}

void UploadsHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    if (!model())
    {
        return;
    }

    QRect textRect = rect.adjusted(UploadsDelegate::HorizontalPadding, 0, -UploadsDelegate::HorizontalPadding, 0);
    Qt::Alignment alignment = model()->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole)
                                  .value<Qt::Alignment>();
    QString text = model()->headerData(logicalIndex, orientation()).toString();

    painter->save();
    painter->setPen(palette().color(QPalette::Text));
    painter->drawText(textRect, int(alignment), fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    painter->restore();
}

QSize UploadsHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    QString text = model() ? model()->headerData(logicalIndex, orientation()).toString() : QString();
    return QSize(fontMetrics().horizontalAdvance(text) + 2 * UploadsDelegate::HorizontalPadding, sizeHint().height());
}
