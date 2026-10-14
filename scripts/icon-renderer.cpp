// Used only by generate-icons; distributed builds consume the generated assets.
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QStringList>
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 4) return 1;
    bool valid = false;
    const int size = QString::fromLocal8Bit(argv[2]).toInt(&valid);
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if (!valid || size < 1 || size > 2048 || !svg.isValid()) return 2;
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    svg.render(&painter);
    painter.end();
    bool opaque = true;
    for (int y = 0; y < size && opaque; ++y)
        for (int x = 0; x < size; ++x)
            if (qAlpha(image.pixel(x, y)) != 255) { opaque = false; break; }
    if (opaque) image = image.convertToFormat(QImage::Format_RGB888);
    return image.save(QString::fromLocal8Bit(argv[3]), "PNG") ? 0 : 3;
}
