#include "UploadsDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>

#include "Colors.h"
#include "UploadsModel.h"

namespace {

// Over the row background, so the text stays readable over the filled part: of the progress color
// on a plain row, of the highlighted text color on a selected one, which is the same tone turned over
static const qreal ProgressOpacity = 0.35;
static const qreal SelectedProgressOpacity = 0.25;

// Of the text of a file that is announced but has nothing on the computer yet: it may never come
static const qreal PendingOpacity = 0.5;

} // namespace

UploadsDelegate::UploadsDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
    // Nothing
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
            QColor color = selected ? option.palette.color(colorGroup, QPalette::HighlightedText)
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
    QColor textColor = option.palette.color(colorGroup, selected ? QPalette::HighlightedText : QPalette::Text);

    if (failed && !selected && index.column() == UploadsModel::StatusColumn)
    {
        textColor = Colors::error(option.palette, colorGroup);
    }

    painter->setPen(textColor);
    painter->drawText(textRect, int(alignment), text);

    painter->restore();
}

QSize UploadsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int width = option.fontMetrics.horizontalAdvance(index.data().toString()) + 2 * HorizontalPadding;
    int height = option.fontMetrics.height() + 2 * VerticalPadding;
    return QSize(width, height);
}
