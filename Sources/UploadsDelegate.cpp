#include "UploadsDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QtMath>

#include "Colors.h"
#include "UploadsModel.h"

namespace {

// Over the row background, so the text stays readable over the filled part: of the progress color
// on a plain row, of the text color on a selected one
static const qreal ProgressOpacity = 0.35;
static const qreal SelectedProgressOpacity = 0.25;

// Of the text of a file that is announced but has nothing on the computer yet: it may never come
static const qreal PendingOpacity = 0.5;

static const int SelectionProbeSize = 16;

// Relative luminance, without the gamma: enough to tell a light color from a dark one
qreal luminance(const QColor& color)
{
    return 0.2126 * color.redF() + 0.7152 * color.greenF() + 0.0722 * color.blueF();
}

} // namespace

UploadsDelegate::UploadsDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
    // Nothing
}

int UploadsDelegate::cellWidth(const QFont& font, const QString& text)
{
    return qCeil(QFontMetricsF(font).horizontalAdvance(text)) + 2 * HorizontalPadding;
}

void UploadsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    bool selected = option.state & QStyle::State_Selected;
    QPalette::ColorGroup colorGroup = !(option.state & QStyle::State_Enabled) ? QPalette::Disabled
                                      : option.state & QStyle::State_Active   ? QPalette::Active
                                                                              : QPalette::Inactive;

    // Selection and hover as the style draws them, without the text
    QStyleOptionViewItem backgroundOption = option;
    initStyleOption(&backgroundOption, index);
    backgroundOption.text.clear();

    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &backgroundOption, painter, widget);

    double progress = index.data(UploadsModel::ProgressRole).toDouble();
    bool complete = index.data(UploadsModel::CompleteRole).toBool();
    bool failed = index.data(UploadsModel::ErrorRole).toBool();

    // One bar across the whole row: each cell fills its own part of it
    const QAbstractItemView* view = qobject_cast<const QAbstractItemView*>(option.widget);

    // A complete file keeps the full fill for a moment, fading away
    double afterglow = complete ? index.data(UploadsModel::AfterglowRole).toDouble() : 1.0;

    if (progress > 0 && afterglow > 0 && view)
    {
        int filledWidth = qRound(view->viewport()->width() * progress);
        QRect filledRect = option.rect.intersected(QRect(0, option.rect.top(), filledWidth, option.rect.height()));

        if (!filledRect.isEmpty())
        {
            QColor color = selected ? selectedTextColor(option, colorGroup)
                           : failed ? Colors::error(option.palette, colorGroup)
                                    : Colors::progress(option.palette, colorGroup);
            color.setAlphaF((selected ? SelectedProgressOpacity : ProgressOpacity) * afterglow);
            painter->fillRect(filledRect, color);
        }
    }

    QRect textRect = option.rect.adjusted(HorizontalPadding, 0, -HorizontalPadding, 0);
    Qt::Alignment alignment = index.data(Qt::TextAlignmentRole).value<Qt::Alignment>();

    // The end of a name tells the type, so it is the middle that gives way
    Qt::TextElideMode elideMode = index.column() == UploadsModel::NameColumn ? Qt::ElideMiddle : Qt::ElideRight;
    QString text = option.fontMetrics.elidedText(index.data().toString(), elideMode, textRect.width());

    if (!complete && progress == 0 && !failed)
    {
        painter->setOpacity(PendingOpacity);
    }

    painter->setFont(option.font);
    QColor textColor =
        selected ? selectedTextColor(option, colorGroup) : option.palette.color(colorGroup, QPalette::Text);

    if (failed && !selected && index.column() == UploadsModel::StatusColumn)
    {
        textColor = Colors::error(option.palette, colorGroup);
    }

    painter->setPen(textColor);
    painter->drawText(textRect, int(alignment), text);

    painter->restore();
}

// Styles draw the selection their own way and do not tell its color. The Windows ones draw a light
// one and keep the text dark, while the palette still has white for the highlighted text. So the
// selection is drawn aside, and of the two text colors the one farther from it wins.
QColor UploadsDelegate::selectedTextColor(const QStyleOptionViewItem& option, QPalette::ColorGroup group) const
{
    QImage image(SelectionProbeSize, SelectionProbeSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(option.palette.color(group, QPalette::Base));

    QStyleOptionViewItem probeOption = option;
    probeOption.rect = image.rect();
    probeOption.state &= ~QStyle::State_MouseOver;
    probeOption.text.clear();

    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();

    QPainter painter(&image);
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &probeOption, &painter, widget);
    painter.end();

    qreal background = luminance(image.pixelColor(SelectionProbeSize / 2, SelectionProbeSize / 2));
    QColor highlightedText = option.palette.color(group, QPalette::HighlightedText);
    QColor text = option.palette.color(group, QPalette::Text);

    return qAbs(luminance(highlightedText) - background) >= qAbs(luminance(text) - background) ? highlightedText : text;
}

QSize UploadsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int width = cellWidth(option.font, index.data().toString());
    int height = option.fontMetrics.height() + 2 * VerticalPadding;
    return QSize(width, height);
}
