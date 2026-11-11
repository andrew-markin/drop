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

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // UPLOADSDELEGATE_H
