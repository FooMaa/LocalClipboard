#include "AndroidUi.h"
#include "I18n.h"
#include "QrCode.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QJniObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QWidget>
#include <QtCore/qnativeinterface.h>

namespace {
constexpr auto Bridge = "org/localclipboard/NativeUi";
QString waitForResult(QWidget *owner, jint request) {
    QEventLoop loop;
    QTimer poll;
    QString result;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        result = QJniObject::callStaticObjectMethod(Bridge, "result", "(I)Ljava/lang/String;", request).toString();
        if (!result.isEmpty()) loop.quit();
    });
    if (owner) QObject::connect(owner, &QObject::destroyed, &loop, &QEventLoop::quit);
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    poll.start(40);
    loop.exec();
    QJniObject::callStaticMethod<void>(Bridge, "cancel", "(I)V", request);
    return result;
}
}
int AndroidUi::choose(QWidget *owner, const QString &title, const QStringList &items, const QList<bool> &enabled, int selected) {
    QJsonArray rows;
    for (int i = 0; i < items.size(); ++i) rows.append(QJsonObject{{"text", items[i]}, {"enabled", enabled.value(i, true)}});
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto heading = QJniObject::fromString(title);
    const auto json = QJniObject::fromString(QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact)));
    const auto cancel = QJniObject::fromString(I18n::text("Cancel"));
    const jint request = QJniObject::callStaticMethod<jint>(Bridge, "choose",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)I",
        context.object(), heading.object<jstring>(), json.object<jstring>(), jint(selected), cancel.object<jstring>());
    bool ok = false;
    const int index = waitForResult(owner, request).toInt(&ok);
    return ok && index >= 0 && index < items.size() && enabled.value(index, true) ? index : -1;
}
QString AndroidUi::scanQr(QWidget *owner) {
    const auto context = QNativeInterface::QAndroidApplication::context();
    const jint request = QJniObject::callStaticMethod<jint>(Bridge, "scan", "(Landroid/content/Context;)I", context.object());
    const QString result = waitForResult(owner, request);
    return result == "cancel" ? QString() : result;
}
extern "C" JNIEXPORT jstring JNICALL Java_org_localclipboard_QrScannerActivity_decode(
    JNIEnv *env, jclass, jbyteArray pixels, jint width, jint height) {
    if (!pixels || width <= 0 || height <= 0 || width > 1280 || height > 1280
        || env->GetArrayLength(pixels) != width * height) return env->NewStringUTF("");
    jbyte *bytes = env->GetByteArrayElements(pixels, nullptr);
    if (!bytes) return nullptr;
    const QImage image(reinterpret_cast<const uchar *>(bytes), width, height, width, QImage::Format_Grayscale8);
    const QString invitation = QrCode::decodeInvitation(image);
    env->ReleaseByteArrayElements(pixels, bytes, JNI_ABORT);
    return env->NewStringUTF(invitation.toUtf8().constData());
}
