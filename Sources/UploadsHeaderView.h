#ifndef UPLOADSHEADERVIEW_H
#define UPLOADSHEADERVIEW_H

#include <QHeaderView>

// Column titles on a tinted band, with the paddings of the rows
class UploadsHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    UploadsHeaderView(QWidget* parent = nullptr);

    static QColor backgroundColor(const QPalette& palette);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;
};

#endif // UPLOADSHEADERVIEW_H
