#include "ClipboardPayload.h"
#include "Profile.h"
#include "SecureProtocol.h"
#include "DeviceAccess.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Small inputs exercise parsers efficiently; large frame/file boundaries have regression tests.
    if (size > 65536) return 0;
    static int argc = 1;
    static char name[] = "protocol-fuzz";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    static const Profile profile = [] {
        Profile p; p.deviceId = "4c4e889d-a62b-4f64-8180-58a43d8a55fb";
        p.deviceName = "fuzz"; p.ownerName = "fuzz"; p.key = QByteArray(32, 'x'); return p;
    }();
    static const Profile managed = [] { Profile p = profile; p.createManaged(); return p; }();
    const QByteArray bytes(reinterpret_cast<const char *>(data), qsizetype(size));
    QJsonObject parsed;
    SecureProtocol::readAnnouncement(profile, bytes, &parsed);
    SecureProtocol::readAnnouncement(managed, bytes, &parsed);
    DeviceAccess::validRegistry(managed, QJsonDocument::fromJson(bytes).object());
    QByteArray plain;
    SecureProtocol::open(profile, bytes, QByteArray("fuzz/context"), &plain);
    QString error;
    ClipboardPayload::validate(QJsonDocument::fromJson(bytes).object(), &error);
    Profile candidate;
    candidate.join(QString::fromUtf8(bytes));
    ClipboardPayload::safeName(QString::fromUtf8(bytes));
    return 0;
}
