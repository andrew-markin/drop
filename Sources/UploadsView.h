#ifndef UPLOADSVIEW_H
#define UPLOADSVIEW_H

#include <QTreeView>

class UploadsModel;

// The table of the files of the drop, the received ones open from it
class UploadsView : public QTreeView
{
    Q_OBJECT

public:
    UploadsView(UploadsModel* model, QWidget* parent = nullptr);

    // Room for a few rows and a readable name
    QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void fitColumns(int firstRow, int lastRow);
    void openFiles();
    void openFolder();

private:
    // Files of the selected rows, complete ones only or all that are on disk
    QStringList selectedPaths(bool completeOnly) const;

    void updateColumnWidths();
    void updateLineColor();
    int textWidth(const QString& text) const;

private:
    QColor m_lineColor;
};

#endif // UPLOADSVIEW_H
