#include "QrScanner.h"
#include "QrCode.h"
#include "Ui.h"
#include "I18n.h"
#include <QApplication>
#include <QCamera>
#include <QCameraDevice>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermissions>
#endif
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QImageReader>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPushButton>
#include <QVideoFrame>
#include <QVideoSink>
#include <QTimer>
#ifdef Q_OS_ANDROID
#include "AndroidUi.h"
#endif

QString QrScanner::scan(QWidget *parent) {
#ifdef Q_OS_ANDROID
    return AndroidUi::scanQr(parent);
#else
    QDialog dialog(parent); Ui::title(&dialog, "Scan connection QR code");
    const auto form = Ui::form(&dialog, "Scan connection QR code", "Point the camera at the QR code on your other device. Nothing is saved until you confirm registration.");
    auto *preview = new QLabel(&dialog); preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumSize(0, 200); preview->setMaximumHeight(360);
    preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    form.body->addWidget(preview);
    auto *status = Ui::label("", &dialog, "muted"); form.body->addWidget(status);
    auto *devices = Ui::comboBox(&dialog); Ui::field(form.body, "Camera", devices);
    auto *start = Ui::button("Open camera", &dialog, true); form.body->addWidget(start);
    auto *imageButton = Ui::button("Read QR from image…", &dialog); form.body->addWidget(imageButton);
    form.body->addStretch();
    auto *cancel = Ui::button("Cancel", &dialog); form.footer->addWidget(cancel);
    QCamera camera; QMediaCaptureSession capture; QVideoSink sink;
    capture.setCamera(&camera); capture.setVideoSink(&sink);
    QString invitation;
    auto refreshDevices = [&] {
        devices->clear();
        const auto cameras = QMediaDevices::videoInputs();
        int back = -1;
        for (const auto &device : cameras) {
            devices->addItem(device.description(), device.id());
            if (device.position() == QCameraDevice::BackFace) back = devices->count() - 1;
        }
        if (back >= 0) devices->setCurrentIndex(back);
    };
    refreshDevices();
    QObject::connect(devices, &QComboBox::currentIndexChanged, &dialog, [&](int index) {
        if (index < 0) return;
        const bool active = camera.isActive(); camera.stop();
        for (const auto &device : QMediaDevices::videoInputs())
            if (device.id() == devices->itemData(index).toByteArray()) { camera.setCameraDevice(device); break; }
        if (active) camera.start();
    });
    auto startCamera = [&] {
        refreshDevices();
        if (devices->count() == 0) { status->setText(I18n::text("No camera available. Choose a QR image instead.")); return; }
        for (const auto &device : QMediaDevices::videoInputs())
            if (device.id() == devices->currentData().toByteArray()) { camera.setCameraDevice(device); break; }
        camera.start(); status->setText(I18n::text("Point the camera at the connection QR code."));
    };
    QObject::connect(start, &QPushButton::clicked, &dialog, [&] {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        const QCameraPermission permission;
        const auto state = qApp->checkPermission(permission);
        if (state == Qt::PermissionStatus::Granted) { startCamera(); return; }
        if (state == Qt::PermissionStatus::Denied) {
            status->setText(I18n::text("Camera access denied. Enable it in system settings or choose a QR image.")); return;
        }
        qApp->requestPermission(permission, &dialog, [&](const QPermission &result) {
            if (result.status() == Qt::PermissionStatus::Granted) startCamera();
            else status->setText(I18n::text("Camera access denied. Enable it in system settings or choose a QR image."));
        });
#else
        startCamera();
#endif
    });
    QObject::connect(&camera, &QCamera::errorOccurred, &dialog, [&](QCamera::Error, const QString &) {
        status->setText(I18n::text("Could not start the camera. Choose a QR image or check camera access."));
    });
    QElapsedTimer throttle; throttle.start();
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &dialog, [&](const QVideoFrame &frame) {
        if (!camera.isActive() || throttle.elapsed() < 180) return;
        throttle.restart();
        const QImage image = frame.toImage(); if (image.isNull()) return;
        preview->setPixmap(QPixmap::fromImage(image.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        const QString decoded = QrCode::decodeInvitation(image);
        if (decoded.isEmpty()) return;
        invitation = decoded; camera.stop(); dialog.accept();
    });
    QObject::connect(imageButton, &QPushButton::clicked, &dialog, [&] {
        camera.stop();
        const QString file = QFileDialog::getOpenFileName(&dialog, I18n::text("Choose a QR image"), {}, "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (file.isEmpty()) return;
        QImageReader reader(file);
        const QSize size = reader.size();
        if (!size.isValid() || size.width() > 8192 || size.height() > 8192) {
            status->setText(I18n::text("No valid LocalClipboard QR code in this image.")); return;
        }
        reader.setAutoTransform(true);
        if (size.width() > 1280 || size.height() > 1280) reader.setScaledSize(size.scaled(1280, 1280, Qt::KeepAspectRatio));
        const QString decoded = QrCode::decodeInvitation(reader.read());
        if (decoded.isEmpty()) { status->setText(I18n::text("No valid LocalClipboard QR code in this image.")); return; }
        invitation = decoded; dialog.accept();
    });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(qApp, &QGuiApplication::applicationStateChanged, &dialog, [&](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) camera.stop();
    });
    QTimer::singleShot(0, start, &QPushButton::click);
    dialog.exec(); camera.stop(); capture.setCamera(nullptr); capture.setVideoSink(nullptr);
    return invitation;
#endif
}
