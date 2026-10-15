#include "QrCode.h"
#include "Profile.h"
#include "ConnectionInfo.h"
#include <BitMatrix.h>
#include <MultiFormatWriter.h>
#include <ReadBarcode.h>

QImage QrCode::encodeInvitation(const QString &invitation) {
    ConnectionInfo check;
    if (!ConnectionInfo::parse(invitation, &check)) return {};
    try {
        const auto matrix = ZXing::MultiFormatWriter(ZXing::BarcodeFormat::QRCode)
            .setMargin(4).setEccLevel(4).encode(invitation.trimmed().toStdString(), 320, 320);
        QImage image(matrix.width(), matrix.height(), QImage::Format_Grayscale8);
        image.fill(255);
        for (int y = 0; y < matrix.height(); ++y)
            for (int x = 0; x < matrix.width(); ++x)
                if (matrix.get(x, y)) image.scanLine(y)[x] = 0;
        return image;
    } catch (const std::exception &) { return {}; }
}
QString QrCode::decodeInvitation(const QImage &source) {
    if (source.isNull() || source.width() > 8192 || source.height() > 8192) return {};
    QImage image = source;
    if (image.width() > 1280 || image.height() > 1280)
        image = image.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image = image.convertToFormat(QImage::Format_Grayscale8);
    try {
        const auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode).setTryRotate(true).setTryInvert(true);
        const auto results = ZXing::ReadBarcodes(ZXing::ImageView(image.constBits(), image.width(), image.height(),
            ZXing::ImageFormat::Lum, image.bytesPerLine()), options);
        for (const auto &result : results) {
            if (!result.isValid() || result.bytes().size() > 1024) continue;
            const QString text = QString::fromStdString(result.text()).trimmed();
            ConnectionInfo check;
            if (ConnectionInfo::parse(text, &check)) return text;
        }
    } catch (const std::exception &) { }
    return {};
}
