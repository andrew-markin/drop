// Renders the application icon into PNG files of the sizes given, for the icon theme on Linux:
// themes prefer a file of the exact size to scaling the SVG.
//
// IconRenderer <svg> <output directory> <file name> <size>...
// writes <output directory>/<size>/<file name> for each size.

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <cstdio>

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::fprintf(stderr, "Usage: %s <svg> <output directory> <file name> <size>...\n", argv[0]);
        return 1;
    }

    QSvgRenderer renderer{QString::fromLocal8Bit(argv[1])};

    if (!renderer.isValid())
    {
        std::fprintf(stderr, "Unable to read %s\n", argv[1]);
        return 1;
    }

    QDir outputDirectory(QString::fromLocal8Bit(argv[2]));
    QString fileName = QString::fromLocal8Bit(argv[3]);

    for (int i = 4; i < argc; i++)
    {
        int size = QString::fromLocal8Bit(argv[i]).toInt();
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
        painter.end();

        QString sizeDirectory = QString::number(size);

        if (!outputDirectory.mkpath(sizeDirectory) ||
            !image.save(outputDirectory.filePath(sizeDirectory + "/" + fileName)))
        {
            std::fprintf(stderr, "Unable to write the %d px icon\n", size);
            return 1;
        }
    }

    return 0;
}
