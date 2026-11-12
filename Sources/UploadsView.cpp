#include "UploadsView.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QEvent>
#include <QFileInfo>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionFrame>
#include <QUrl>

#include "UploadsDelegate.h"
#include "UploadsHeaderView.h"
#include "UploadsModel.h"

namespace {

static const int MinimumRowCount = 4;
static const int MinimumNameLength = 20; // Characters
static const int FrameProbeSize = 40;

} // namespace

UploadsView::UploadsView(UploadsModel* model, QWidget* parent) : QTreeView(parent)
{
    setHeader(new UploadsHeaderView(this));
    setItemDelegate(new UploadsDelegate(this));
    setModel(model);

    setRootIsDecorated(false);
    setIndentation(0);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(UploadsModel::NameColumn, QHeaderView::Stretch);
    header()->setSectionResizeMode(UploadsModel::SizeColumn, QHeaderView::Fixed);
    header()->setSectionResizeMode(UploadsModel::StatusColumn, QHeaderView::Fixed);
    updateColumnWidths();
    updateLineColor();

    connect(model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex&, int first, int last) { fitColumns(first, last); });
    connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
                fitColumns(topLeft.row(), bottomRight.row());
            });

    // A double click or Enter opens what is selected, the way file managers do
    connect(this, &QAbstractItemView::activated, this, &UploadsView::openFiles);
}

QSize UploadsView::minimumSizeHint() const
{
    int rowHeight = fontMetrics().height() + 2 * UploadsDelegate::VerticalPadding;
    int nameWidth = fontMetrics().horizontalAdvance(QString(MinimumNameLength, 'x'));

    int width = nameWidth + header()->sectionSize(UploadsModel::SizeColumn) +
                header()->sectionSize(UploadsModel::StatusColumn);
    int height = header()->sizeHint().height() + MinimumRowCount * rowHeight;

    return QSize(width, height) + QSize(2 * frameWidth(), 2 * frameWidth());
}

void UploadsView::changeEvent(QEvent* event)
{
    QTreeView::changeEvent(event);

    if (event->type() == QEvent::FontChange)
    {
        updateColumnWidths();
    }

    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    {
        updateLineColor();
    }
}

void UploadsView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    QAction* openAction = menu.addAction(tr("Open"), this, &UploadsView::openFiles);
    openAction->setEnabled(!selectedPaths(true).isEmpty());
    menu.setDefaultAction(openAction);

    QAction* folderAction = menu.addAction(tr("Open Folder"), this, &UploadsView::openFolder);
    folderAction->setEnabled(!selectedPaths(false).isEmpty());

    menu.exec(event->globalPos());
}

void UploadsView::openFiles()
{
    const QStringList paths = selectedPaths(true);

    for (const QString& path : paths)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void UploadsView::openFolder()
{
    // All files of a drop go to one folder
    QStringList paths = selectedPaths(false);

    if (!paths.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(paths.constFirst()).absolutePath()));
    }
}

QStringList UploadsView::selectedPaths(bool completeOnly) const
{
    QStringList paths;
    const QModelIndexList rows = selectionModel()->selectedRows();

    for (const QModelIndex& row : rows)
    {
        QString path = row.data(UploadsModel::PathRole).toString();

        if (!path.isEmpty() && (!completeOnly || row.data(UploadsModel::CompleteRole).toBool()))
        {
            paths.append(path);
        }
    }

    return paths;
}

// Wide enough for most values from the start, so the columns do not jump while files come
void UploadsView::updateColumnWidths()
{
    for (int column : {int(UploadsModel::SizeColumn), int(UploadsModel::StatusColumn)})
    {
        QStringList texts = UploadsModel::reservedTexts(column);
        texts.append(model()->headerData(column, Qt::Horizontal).toString());

        int width = 0;

        for (const QString& text : std::as_const(texts))
        {
            width = qMax(width, textWidth(text));
        }

        header()->resizeSection(column, width);
    }

    fitColumns(0, model()->rowCount() - 1);
}

// Wider for a value that does not fit, a cut number tells nothing. Rows only come, so the columns
// only grow.
void UploadsView::fitColumns(int firstRow, int lastRow)
{
    for (int column : {int(UploadsModel::SizeColumn), int(UploadsModel::StatusColumn)})
    {
        int width = header()->sectionSize(column);

        for (int row = firstRow; row <= lastRow; row++)
        {
            width = qMax(width, textWidth(model()->index(row, column).data().toString()));
        }

        if (width > header()->sectionSize(column))
        {
            header()->resizeSection(column, width);
        }
    }
}

int UploadsView::textWidth(const QString& text) const
{
    return UploadsDelegate::cellWidth(font(), text);
}

void UploadsView::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);

    // The line under the header goes on top of the rows: with fractional scaling the bottom of the
    // header falls between pixels, and a line drawn there leaves a strip of the header under it
    QPainter painter(viewport());
    painter.fillRect(QRect(0, 0, viewport()->width(), 1), m_lineColor);
}

// Styles draw frames their own way and do not tell the color, so it is taken from a frame drawn
// aside: the middle of its top edge, away from rounded corners
void UploadsView::updateLineColor()
{
    QImage image(FrameProbeSize, FrameProbeSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(palette().color(QPalette::Window));

    QStyleOptionFrame option;
    initStyleOption(&option);
    option.rect = image.rect();

    QPainter painter(&image);
    style()->drawControl(QStyle::CE_ShapedFrame, &option, &painter, this);
    painter.end();

    m_lineColor = image.pixelColor(FrameProbeSize / 2, 0);
    viewport()->update();
}
