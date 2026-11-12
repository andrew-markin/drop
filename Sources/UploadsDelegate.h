#ifndef UPLOADSDELEGATE_H
#define UPLOADSDELEGATE_H

#include <QStyledItemDelegate>

// Draws a row of the uploads with its progress as the background filling up from the left
class UploadsDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // Shared with the header, so the titles stand right above the values
    static const int HorizontalPadding = 12;
    static const int VerticalPadding = 8;

    UploadsDelegate(QObject* parent = nullptr);

    // Of a cell or a title with the text whole: the text is measured to a fraction of a pixel and
    // rounded up, as drawing cuts it when it is wider than the room by any fraction
    static int cellWidth(const QFont& font, const QString& text);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // UPLOADSDELEGATE_H
