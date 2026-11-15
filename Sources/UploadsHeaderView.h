#ifndef UPLOADSHEADERVIEW_H
#define UPLOADSHEADERVIEW_H

#include <QHeaderView>

// Column titles on a tinted band with a line under it, with the paddings of the rows
class UploadsHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    UploadsHeaderView(QWidget* parent = nullptr);

    static QColor backgroundColor(const QPalette& palette);

    void setLineColor(const QColor& color);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

private:
    QColor m_lineColor;
};

#endif // UPLOADSHEADERVIEW_H
