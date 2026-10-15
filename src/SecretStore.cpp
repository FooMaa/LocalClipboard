#include "SecretStore.h"
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>
#endif

QString SecretStore::encode(const QByteArray &key) {
    if (key.size() != 32) return {};
    const auto text = QString::fromLatin1(key.toHex());
#ifdef Q_OS_ANDROID
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto value = QJniObject::fromString(text);
    return QJniObject::callStaticObjectMethod("org/localclipboard/ProfileSecrets", "wrap",
        "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", context.object(), value.object<jstring>()).toString();
#else
    return text;
#endif
}
QByteArray SecretStore::decode(const QString &stored) {
    QString text = stored;
#ifdef Q_OS_ANDROID
    if (stored.startsWith("AK1.")) {
        // Profile polling by the service must not invoke Keystore on every timer tick.
        static QMutex cacheMutex;
        const QMutexLocker locker(&cacheMutex);
        static QString cachedCipher;
        static QByteArray cachedKey;
        if (stored == cachedCipher) return cachedKey;
        const auto value = QJniObject::fromString(stored);
        const auto context = QNativeInterface::QAndroidApplication::context();
        text = QJniObject::callStaticObjectMethod("org/localclipboard/ProfileSecrets", "unwrap",
            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", context.object(), value.object<jstring>()).toString();
        if (!QRegularExpression("^[0-9a-f]{64}$").match(text).hasMatch()) return {};
        cachedCipher = stored; cachedKey = QByteArray::fromHex(text.toLatin1());
        return cachedKey;
    }
#endif
    // Read legacy keys so an installed device can migrate without losing its profile.
    if (!QRegularExpression("^[0-9a-fA-F]{64}$").match(text).hasMatch()) return {};
    return QByteArray::fromHex(text.toLatin1());
}
