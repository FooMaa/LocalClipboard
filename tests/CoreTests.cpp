#include "ClipboardPayload.h"
#include "Profile.h"
#include "PendingFiles.h"
#include "SecureProtocol.h"
#include "SyncEngine.h"
#include "FileStorage.h"
#include "SecretStore.h"
#include "ConnectionInfo.h"
#include "DeviceAccess.h"
#include "LocalHistory.h"
#include "ExchangeStore.h"
#include "OutgoingQueue.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>
#include <QUuid>
#include <QtEndian>
#include <memory>

class CoreTests : public QObject {
    Q_OBJECT
    static Profile profile() {
        Profile p;
        p.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.deviceName = "test-device"; p.ownerName = "test-owner"; p.create();
        return p;
    }
    static QJsonObject text(const QString &s = "hello") { return {{"kind", "text"}, {"text", s}, {"html", "<b>hello</b>"}}; }
    static void introduce(SyncEngine &receiver, const Profile &sender, quint16 port) {
        QUdpSocket udp;
        udp.writeDatagram(SecureProtocol::announcement(sender, port), QHostAddress::LocalHost, receiver.discoveryPort());
    }
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("LocalClipboardCoreTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    void cleanupTestCase() { QDir(Profile::dataDirectory()).removeRecursively(); }
    void clipboardReadingIsOptInIncludingExistingProfiles() {
        Profile p = profile(); QVERIFY(!p.automaticClipboard);
        QTemporaryDir directory; QVERIFY(p.save(nullptr, directory.path()));
        QVERIFY(!Profile::load(directory.path()).automaticClipboard);
        p.automaticClipboard = true; QVERIFY(p.save(nullptr, directory.path()));
        QVERIFY(Profile::load(directory.path()).automaticClipboard);
        QFile file(directory.filePath("profile.json")); QVERIFY(file.open(QIODevice::ReadOnly));
        auto obj = QJsonDocument::fromJson(file.readAll()).object(); file.close();
        obj.remove("automaticClipboard"); // Upgrade from releases that always watched the clipboard.
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(QJsonDocument(obj).toJson()); file.close();
        QVERIFY(!Profile::load(directory.path()).automaticClipboard);
    }
    void exchangeIsEncryptedBoundedAndIsolatedFromHistoryAndOtherDevices() {
        ExchangeStore::clear();
        Profile p = profile(); p.createManaged();
        const auto id = ExchangeStore::record(p, text("private shared note"), "sender", "transfer");
        QVERIFY(!id.isEmpty());
        QVERIFY(!p.historyEnabled); QCOMPARE(LocalHistory::entries(p).size(), 0);
        QCOMPARE(ExchangeStore::entries(p).size(), 1);
        QCOMPARE(ExchangeStore::item(p, id).value("payload").toObject(), text("private shared note"));
        const QDir directory(QDir(Profile::dataDirectory()).filePath("exchange"));
        {
            // Windows cannot prune this entry while the test keeps its file open.
            QFile file(directory.filePath(id));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(!file.readAll().contains("private shared note"));
        }
        Profile other = profile(); QVERIFY(other.join(p.invitation()));
        QVERIFY(ExchangeStore::entries(other).isEmpty());
        QVERIFY(ExchangeStore::item(p, "../profile.json").isEmpty());
        QVERIFY(ExchangeStore::record(p, QJsonObject{{"kind", "unknown"}}).isEmpty());
        for (int i = 0; i < ExchangeStore::MaxItems + 2; ++i) {
            QVERIFY(!ExchangeStore::record(p, text(QString::number(i))).isEmpty());
            QTest::qWait(1);
        }
        QCOMPARE(ExchangeStore::entries(p).size(), ExchangeStore::MaxItems);
        QCOMPARE(directory.entryList(QDir::Files | QDir::NoSymLinks).size(), ExchangeStore::MaxItems);
        QVERIFY(ExchangeStore::item(p, id).isEmpty());
        ExchangeStore::clear(); QVERIFY(ExchangeStore::entries(p).isEmpty());
    }
    void outgoingQueuePreservesSeparatePostsAndDoesNotDependOnTheHomeList() {
        OutgoingQueue::clear(); Profile p = profile();
        for (int i = 0; i < OutgoingQueue::MaxItems; ++i) {
            QVERIFY(OutgoingQueue::enqueue(p, text("private post " + QString::number(i))));
            QTest::qWait(1);
        }
        QVERIFY(!OutgoingQueue::enqueue(p, text("queue is full")));
        ExchangeStore::clear(); // Clearing Home must not silently cancel queued sends.
        for (int i = 0; i < OutgoingQueue::MaxItems; ++i) {
            const auto next = OutgoingQueue::next(p); QVERIFY(!next.isEmpty());
            QCOMPARE(next.value("payload").toObject(), text("private post " + QString::number(i)));
            const auto id = next.value("id").toString();
            QFile file(QDir(Profile::dataDirectory()).filePath("outgoing/" + id)); QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(!file.readAll().contains("private post")); file.close();
            OutgoingQueue::remove("../profile.json"); QVERIFY(file.exists());
            OutgoingQueue::remove(id);
        }
        QVERIFY(OutgoingQueue::next(p).isEmpty());
        QVERIFY(OutgoingQueue::enqueue(p, text("expires")));
        const auto id = OutgoingQueue::next(p).value("id").toString();
        QDir dir(QDir(Profile::dataDirectory()).filePath("outgoing"));
        const auto expired = QString::number(QDateTime::currentMSecsSinceEpoch() - 601000) + id.mid(13);
        QVERIFY(dir.rename(id, expired)); OutgoingQueue::prune(); QVERIFY(!dir.exists(expired));
        QVERIFY(OutgoingQueue::enqueue(p, text("other profile cannot send")));
        QVERIFY(OutgoingQueue::next(profile()).isEmpty());
        OutgoingQueue::clear();
    }
    void storedPayloadsKeepTheirFormatAndCannotMoveBetweenStores() {
        ExchangeStore::clear();
        OutgoingQueue::clear();
        Profile p = profile(); p.createManaged();
        Profile key = p; key.key = p.deviceSecret;
        const auto id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "-"
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject item{{"group", p.groupId()}, {"payload", text("previously stored")}};
        const auto wire = SecureProtocol::seal(key, QJsonDocument(item).toJson(QJsonDocument::Compact),
                                              "LocalClipboard/exchange/v1/" + id.toUtf8());
        const QDir directory(Profile::dataDirectory());
        QFile file(directory.filePath("exchange/" + id));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(wire), qint64(wire.size())); file.close();
        QCOMPARE(ExchangeStore::item(p, id), item);
        QVERIFY(file.copy(directory.filePath("outgoing/" + id)));
        QVERIFY(OutgoingQueue::next(p).isEmpty());
        ExchangeStore::clear();
    }
    void managedDevicesHavePairwiseKeysAndSignedRevocation() {
        Profile owner = profile(); owner.createManaged();
        Profile phone = profile(); QVERIFY(phone.join(owner.invitation()));
        QVERIFY(phone.managed()); QVERIFY(!DeviceAccess::isAdministrator(phone));
        QVERIFY(!DeviceAccess::channel(owner, phone.deviceId).registered());
        QVERIFY(DeviceAccess::authorize(&owner, phone.deviceId, DeviceAccess::publicKey(phone), false));
        QVERIFY(DeviceAccess::updateRegistry(&phone, owner.registry));
        const auto originalRegistry = owner.registry;
        Profile impostor = phone; impostor.deviceSecret = SecureProtocol::randomBytes(32);
        QJsonObject announcement;
        QVERIFY(!SecureProtocol::readAnnouncement(owner, SecureProtocol::announcement(impostor, 45872), &announcement));
        QVERIFY(SecureProtocol::readAnnouncement(owner, SecureProtocol::announcement(phone, 45872), &announcement));
        const auto channel = DeviceAccess::channel(owner, phone.deviceId);
        QCOMPARE(channel.key, DeviceAccess::channel(phone, owner.deviceId).key);
        QVERIFY(channel.key != owner.key);
        const auto encrypted = SecureProtocol::seal(channel, "private payload", "test pair");
        QByteArray plain;
        QVERIFY(!SecureProtocol::open(phone, encrypted, "test pair", &plain));
        QVERIFY(SecureProtocol::open(DeviceAccess::channel(phone, owner.deviceId), encrypted, "test pair", &plain));
        Profile stranger = profile(); QVERIFY(stranger.join(owner.invitation()));
        QVERIFY(!DeviceAccess::authorize(&stranger, stranger.deviceId, DeviceAccess::publicKey(stranger), false));
        QVERIFY(!DeviceAccess::channel(stranger, owner.deviceId).registered());
        QVERIFY(DeviceAccess::authorize(&owner, phone.deviceId, DeviceAccess::publicKey(phone), true));
        QVERIFY(DeviceAccess::updateRegistry(&phone, owner.registry));
        QVERIFY(!DeviceAccess::channel(phone, owner.deviceId).registered());
        QVERIFY(!DeviceAccess::channel(owner, phone.deviceId).registered());
        QVERIFY(!DeviceAccess::updateRegistry(&phone, originalRegistry));
        QVERIFY(!DeviceAccess::authorize(&owner, phone.deviceId, DeviceAccess::publicKey(phone), false));
        auto tampered = owner.registry; tampered.insert("revision", 999);
        QVERIFY(!DeviceAccess::validRegistry(phone, tampered));
        QTemporaryDir directory; QVERIFY(owner.save(nullptr, directory.path()));
        const auto loaded = Profile::load(directory.path());
        QCOMPARE(loaded.deviceSecret, owner.deviceSecret); QCOMPARE(loaded.authoritySeed, owner.authoritySeed);
        QCOMPARE(loaded.registry, owner.registry); QVERIFY(DeviceAccess::isAdministrator(loaded));
        QFile stored(QDir(directory.path()).filePath("profile.json")); QVERIFY(stored.open(QIODevice::ReadOnly));
        auto obj = QJsonDocument::fromJson(stored.readAll()).object(); stored.close();
        QCOMPARE(obj.value("version").toInt(), 3);
        obj.remove("deviceSecret"); QVERIFY(stored.open(QIODevice::WriteOnly | QIODevice::Truncate));
        stored.write(QJsonDocument(obj).toJson()); stored.close();
        QVERIFY(!Profile::load(directory.path()).registered());
        ConnectionInfo parsed;
        QVERIFY(ConnectionInfo::parse(ConnectionInfo{owner.invitation(), {"192.168.1.5"}}.encoded(), &parsed));
        QCOMPARE(parsed.code, owner.invitation());
        QMimeData mime; mime.setText(owner.invitation()); QString error;
        QVERIFY(ClipboardPayload::capture(&mime, &error).isEmpty());
    }
    void deliveryReceiptsTrackActualReceiverAndDisabledReception() {
        Profile owner = profile(); owner.createManaged();
        Profile member = profile(); QVERIFY(member.join(owner.invitation()));
        QVERIFY(DeviceAccess::authorize(&owner, member.deviceId, DeviceAccess::publicKey(member), false));
        QVERIFY(DeviceAccess::updateRegistry(&member, owner.registry));
        SyncEngine sender(owner, nullptr, 0, 0), receiver(member, nullptr, 0, 0);
        introduce(sender, member, receiver.port()); introduce(receiver, owner, sender.port());
        QTRY_COMPARE(sender.peerDescriptions().size(), 1); QTRY_COMPARE(receiver.peerDescriptions().size(), 1);
        QSignalSpy delivered(&receiver, &SyncEngine::received), incoming(&receiver, &SyncEngine::incomingTransfer);
        QVERIFY(sender.send(text("receipt content")));
        QTRY_COMPARE(delivered.size(), 1);
        QTRY_COMPARE(sender.transfers().first().toMap().value("state").toString(), QStringLiteral("received"));
        const QString id = incoming.first().at(0).toString();
        receiver.completeIncoming(id, owner.deviceId, "copied");
        QTRY_COMPARE(sender.transfers().first().toMap().value("state").toString(), QStringLiteral("copied"));
        // A duplicate of an accepted transfer does not apply a second copy.
        QVERIFY(sender.retry(id));
        QTRY_COMPARE(sender.transfers().first().toMap().value("state").toString(), QStringLiteral("copied"));
        QCOMPARE(delivered.size(), 1);
        member.receivingEnabled = false;
        SyncEngine disabled(member, nullptr, 0, 0);
        introduce(sender, member, disabled.port()); QTest::qWait(30);
        QVERIFY(sender.send(text("blocked")));
        QTRY_COMPARE(sender.transfers().first().toMap().value("state").toString(), QStringLiteral("failed"));
        QVERIFY(sender.transfers().first().toMap().value("error").toString().contains("disabled"));
    }
    void historyIsOptInEncryptedBoundedAndCanBePurged() {
        LocalHistory::clear();
        Profile p = profile(); p.createManaged();
        QVERIFY(p.save());
        QVERIFY(LocalHistory::record(p, text("history secret marker")));
        QVERIFY(LocalHistory::entries(p).isEmpty());
        p.historyEnabled = true; p.historyMiB = 1; QVERIFY(p.save());
        QVERIFY(LocalHistory::record(p, text("history secret marker")));
        auto list = LocalHistory::entries(p, "SECRET MARKER"); QCOMPARE(list.size(), 1);
        const QString id = list.first().toMap().value("id").toString();
        QFile wire(QDir(Profile::dataDirectory()).filePath("history/" + id)); QVERIFY(wire.open(QIODevice::ReadOnly));
        QVERIFY(!wire.readAll().contains("history secret marker")); wire.close();
        QCOMPARE(LocalHistory::payload(p, id).value("text").toString(), QStringLiteral("history secret marker"));
        QVERIFY(LocalHistory::payload(p, "../profile.json").isEmpty());
        QVERIFY(LocalHistory::entries(p, "absent").isEmpty());
        for (int i = 0; i < 3; ++i) QVERIFY(LocalHistory::record(p, text(QString(400000, QChar('a' + i)))));
        qint64 total = 0;
        const auto directory = QDir(QDir(Profile::dataDirectory()).filePath("history"));
        for (const auto &file : directory.entryInfoList(QDir::Files)) total += file.size();
        QVERIFY(total <= 1024 * 1024);
        const auto expiredId = QString::number(QDateTime::currentMSecsSinceEpoch() - 8LL * 86400000) + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QFile expired(directory.filePath(expiredId)); QVERIFY(expired.open(QIODevice::WriteOnly)); expired.write("expired"); expired.close();
        LocalHistory::prune(p); QVERIFY(!expired.exists());
        p.historyEnabled = false; QVERIFY(p.save()); LocalHistory::prune(p);
        QVERIFY(!QFile::exists(wire.fileName()));
        // A delayed worker using the old enabled snapshot must not recreate deleted history.
        Profile stale = p; stale.historyEnabled = true;
        QVERIFY(LocalHistory::record(stale, text("stale")));
        QVERIFY(!QDir(QDir(Profile::dataDirectory()).filePath("history")).exists());
    }
    void privacySettingsSurviveReloadAndTimedPauseExpires() {
        Profile p = profile(); p.sendingEnabled = false; p.receivingEnabled = false;
        p.historyEnabled = true; p.historyDays = 1; p.historyMiB = 4;
        p.excludedApplications = {"password-manager.exe", "com.apple.keychainaccess"};
        p.pauseUntil = QDateTime::currentSecsSinceEpoch() + 900;
        QTemporaryDir directory; QVERIFY(p.save(nullptr, directory.path()));
        const auto restored = Profile::load(directory.path());
        QVERIFY(restored.isPaused()); QVERIFY(!restored.sendingEnabled); QVERIFY(!restored.receivingEnabled);
        QCOMPARE(restored.historyDays, 1); QCOMPARE(restored.historyMiB, 4); QCOMPARE(restored.excludedApplications, p.excludedApplications);
        p.pauseUntil = QDateTime::currentSecsSinceEpoch() - 1; QVERIFY(!p.isPaused());
    }

    void qrConnectionIncludesAddressesAndRejectsMalformedHints() {
        const Profile source = profile();
        const ConnectionInfo invitation{source.invitation(), {"192.168.1.20", "10.0.0.3"}};
        ConnectionInfo decoded;
        QVERIFY(ConnectionInfo::parse(invitation.encoded(), &decoded));
        QCOMPARE(decoded.code, source.invitation()); QCOMPARE(decoded.addresses, invitation.addresses);
        QVERIFY(ConnectionInfo::parse(source.invitation(), &decoded)); QVERIFY(decoded.addresses.isEmpty());
        for (const QString &address : {"https://evil.example", "224.0.0.1", "255.255.255.255", "not-an-ip"}) {
            const ConnectionInfo invalid{source.invitation(), {address}};
            QVERIFY(!ConnectionInfo::parse(invalid.encoded(), &decoded));
        }
        QVERIFY(!ConnectionInfo::parse(QString(1025, 'x'), &decoded));
        QVERIFY(!ConnectionInfo::parse("LCQR1.???", &decoded));
        const ConnectionInfo duplicate{source.invitation(), {"192.168.1.20", "192.168.1.20"}};
        QVERIFY(!ConnectionInfo::parse(duplicate.encoded(), &decoded));
        QMimeData mime; mime.setText(invitation.encoded());
        QString error;
        QVERIFY(ClipboardPayload::capture(&mime, &error).isEmpty());
    }
    void registrationAndPersistence() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile p = profile(), joined = profile();
        QVERIFY(joined.join(p.invitation()));
        QCOMPARE(joined.groupId(), p.groupId());
        QVERIFY(joined.deviceId != p.deviceId);
        p.peerAddresses = {"192.168.1.10", "10.0.0.2"};
        QVERIFY(!joined.join("LC2.password"));
        QVERIFY(p.save(nullptr, directory.path()));
        const Profile restored = Profile::load(directory.path());
        QCOMPARE(restored.deviceId, p.deviceId); QCOMPARE(restored.key, p.key);
        QCOMPARE(restored.peerAddresses, p.peerAddresses);
        p.paused = true; QVERIFY(p.save(nullptr, directory.path())); QVERIFY(Profile::load(directory.path()).paused);
    }
    void authenticatedDiscovery() {
        Profile p = profile(), other = profile();
        const QByteArray bytes = SecureProtocol::announcement(p, 45872);
        QJsonObject parsed;
        QVERIFY(SecureProtocol::readAnnouncement(p, bytes, &parsed));
        QVERIFY(!SecureProtocol::readAnnouncement(other, bytes, &parsed));
        QJsonObject tampered = QJsonDocument::fromJson(bytes).object();
        tampered.insert("name", "spoofed");
        QVERIFY(!SecureProtocol::readAnnouncement(p, QJsonDocument(tampered).toJson(QJsonDocument::Compact), &parsed));
        QVERIFY(!SecureProtocol::readAnnouncement(p, QByteArray(3000, 'a'), &parsed));
    }
    void encryptionAndReplay() {
        const Profile p = profile(), other = profile();
        const QByteArray plain("clipboard secret"), challenge = SecureProtocol::randomBytes(32);
        const QByteArray wire = SecureProtocol::seal(p, plain, challenge);
        QVERIFY(!wire.contains(plain));
        QByteArray result;
        QVERIFY(SecureProtocol::open(p, wire, challenge, &result)); QCOMPARE(result, plain);
        QVERIFY(!SecureProtocol::open(other, wire, challenge, &result));
        QVERIFY(!SecureProtocol::open(p, wire, SecureProtocol::randomBytes(32), &result));
        QByteArray modified = wire; modified[modified.size() - 1] ^= 1;
        QVERIFY(!SecureProtocol::open(p, modified, challenge, &result));
        QVERIFY(SecureProtocol::seal(p, plain, challenge) != wire);
    }
    void textAndImageRoundTrip() {
        QMimeData rich;
        rich.setText("Привет 👋"); rich.setHtml("<p><b>Привет 👋</b></p>");
        QString error;
        auto payload = ClipboardPayload::capture(&rich, &error);
        QVERIFY(ClipboardPayload::validate(payload, &error));
        std::unique_ptr<QMimeData> received(ClipboardPayload::materialize(payload, "/unused", &error));
        QVERIFY(received); QCOMPARE(received->text(), rich.text()); QCOMPARE(received->html(), rich.html());
        QImage image(160, 120, QImage::Format_ARGB32); image.fill(Qt::magenta);
        QMimeData photo; photo.setImageData(image);
        payload = ClipboardPayload::capture(&photo, &error);
        received.reset(ClipboardPayload::materialize(payload, "/unused", &error));
        QVERIFY(received); QCOMPARE(qvariant_cast<QImage>(received->imageData()), image);
        rich.setText(profile().invitation()); rich.setHtml({});
        QVERIFY(ClipboardPayload::capture(&rich, &error).isEmpty());
        QVERIFY(!ClipboardPayload::validate({{"kind", "image"}, {"png", "invalid-base64!"}}, &error));
    }
    void filesAndTraversal() {
        QTemporaryDir source, target;
        QVERIFY(source.isValid() && target.isValid());
        QFile file(source.filePath("photo.jpg")); QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray binary("\0\1\2file\xff", 8); QCOMPARE(file.write(binary), qint64(binary.size())); file.close();
        QFile empty(source.filePath("empty.txt")); QVERIFY(empty.open(QIODevice::WriteOnly)); empty.close();
        QString error;
        auto payload = ClipboardPayload::fromFiles({QUrl::fromLocalFile(file.fileName()), QUrl::fromLocalFile(empty.fileName())}, &error);
        QVERIFY(ClipboardPayload::validate(payload, &error));
        std::unique_ptr<QMimeData> received(ClipboardPayload::materialize(payload, target.path(), &error));
        QVERIFY(received); QCOMPARE(received->urls().size(), 2);
        QFile copy(received->urls().first().toLocalFile()); QVERIFY(copy.open(QIODevice::ReadOnly)); QCOMPARE(copy.readAll(), binary);
        for (const QString &unsafe : {"../secret", "C:\\secret", "/etc/passwd", "CON.txt", "a:b", "name.", "name "}) QVERIFY(!ClipboardPayload::safeName(unsafe));
        QJsonArray files = payload.value("files").toArray();
        auto entry = files.first().toObject(); entry.insert("name", "../secret"); files[0] = entry; payload.insert("files", files);
        QVERIFY(!ClipboardPayload::validate(payload, &error));
        QVERIFY(!ClipboardPayload::materialize(payload, target.path(), &error));
        entry.insert("name", "safe.txt"); entry.insert("size", 300); files[0] = entry; payload.insert("files", files);
        QVERIFY(!ClipboardPayload::validate(payload, &error));
        entry.insert("size", 8); entry.insert("data", "!!!"); files[0] = entry; payload.insert("files", files);
        QVERIFY(!ClipboardPayload::validate(payload, &error));
        QVERIFY(ClipboardPayload::fromFiles({QUrl::fromLocalFile(source.path())}, &error).isEmpty());
    }
    void sizeLimits() {
        QTemporaryDir dir;
        QFile file(dir.filePath("too-large.bin")); QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(ClipboardPayload::MaxContentBytes + 1)); file.close();
        QString error;
        QVERIFY(ClipboardPayload::fromFiles({QUrl::fromLocalFile(file.fileName())}, &error).isEmpty());
        QVERIFY(!ClipboardPayload::validate(text(QString(ClipboardPayload::MaxContentBytes + 1, 'a')), &error));
    }
    void malformedSchemasAndFilenameAliasesAreRejected() {
        QString error;
        QVERIFY(!ClipboardPayload::validate({{"kind", "text"}, {"text", "hello"}, {"extra", "hidden"}}, &error));
        QVERIFY(!ClipboardPayload::validate({{"kind", "text"}, {"text", "hello"}, {"html", QJsonArray{}}}, &error));
        const auto entry = [](const QString &name) { return QJsonObject{{"name", name}, {"size", 0}, {"data", ""}}; };
        // Filesystems may normalize Unicode before creating a filename.
        const QJsonObject duplicates{{"kind", "files"}, {"files", QJsonArray{entry(QString::fromUtf8("é.txt")), entry(QString::fromUtf8("é.txt"))}}};
        QVERIFY(!ClipboardPayload::validate(duplicates, &error));
        QVERIFY(!ClipboardPayload::safeName(QString::fromUtf8("COM¹.txt")));
        QVERIFY(!ClipboardPayload::safeName(QString::fromUtf8("report\u202Eexe.txt")));
        QVERIFY(!ClipboardPayload::safeName("CONOUT$.txt"));
        Profile p = profile(); p.deviceId = "{" + p.deviceId + "}";
        QVERIFY(!p.registered());
        p = profile(); p.deviceName = "phone\nspoofed"; QVERIFY(!p.registered());
        QTemporaryDir directory;
        QVERIFY(!p.save(&error, directory.path()));
        QVERIFY(!QFile::exists(directory.filePath("profile.json")));
        QVERIFY(SecretStore::decode("not-a-profile-key").isEmpty());
        QStringList addresses{"preserved"};
        QVERIFY(!Profile::parseAddresses("192.168.1.1 not-an-ip", &addresses));
        QCOMPARE(addresses, QStringList{"preserved"});
        QVERIFY(Profile::parseAddresses("192.168.1.1, 192.168.1.1;10.0.0.2", &addresses));
        QCOMPARE(addresses.size(), 2);
    }
    void receivedStorageIsBoundedWithoutDeletingExistingFiles() {
        QTemporaryDir directory;
        QVERIFY(QDir(directory.path()).mkpath("batch"));
        QFile existing(directory.filePath("batch/large.bin")); QVERIFY(existing.open(QIODevice::WriteOnly));
        QVERIFY(existing.resize(FileStorage::MaxReceivedBytes - 1)); existing.close();
        QString error;
        QVERIFY(!FileStorage::canReceive(directory.path(), 2, 1, &error));
        QVERIFY(!error.isEmpty()); QVERIFY(existing.exists());
        QVERIFY(FileStorage::isReceivedFile(existing.fileName(), directory.path()));
        QVERIFY(!FileStorage::isReceivedFile(directory.filePath("../outside"), directory.path()));
        QVERIFY(existing.open(QIODevice::WriteOnly)); QVERIFY(existing.resize(0)); existing.close();
        QVERIFY(FileStorage::canReceive(directory.path(), 32, 1, &error));
    }
    void authenticatedTransfersShareOneReceiveBudget() {
        const Profile recipient = profile(); Profile member = profile(); member.key = recipient.key;
        SyncEngine receiver(recipient, nullptr, 0, 0);
        QSignalSpy delivered(&receiver, &SyncEngine::received);
        const auto reserve = [&](QTcpSocket &socket) {
            socket.connectToHost(QHostAddress::LocalHost, receiver.port());
            QTRY_VERIFY(socket.bytesAvailable() >= 104);
            const QByteArray hello = socket.read(104), challenge = hello.mid(4, 32), id = hello.mid(36, 36);
            const QByteArray sender = member.deviceId.toUtf8();
            QByteArray header(4, Qt::Uninitialized);
            qToBigEndian(quint32(SecureProtocol::MaxWireBytes), reinterpret_cast<uchar *>(header.data()));
            socket.write(sender + SecureProtocol::mac(member, QByteArray("client/") + challenge + sender + id, "handshake") + header);
        };
        QTcpSocket first, second, excess, retry;
        reserve(first); reserve(second); QTest::qWait(30);
        reserve(excess); QTRY_COMPARE(excess.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(first.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(second.state(), QAbstractSocket::ConnectedState);
        first.abort(); QTest::qWait(30);
        reserve(retry); QTest::qWait(30);
        QCOMPARE(retry.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(delivered.size(), 0);
        receiver.setPaused(true);
        QTRY_COMPARE(second.state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(retry.state(), QAbstractSocket::UnconnectedState);
    }
    void networkRoundTripAndIsolation() {
        Profile a = profile(), b = profile(), unrelated = profile(); b.key = a.key;
        SyncEngine sender(a, nullptr, 0, 0), receiver(b, nullptr, 0, 0);
        QVERIFY(sender.port() && receiver.port());
        QSignalSpy delivered(&receiver, &SyncEngine::received);
        introduce(sender, b, receiver.port());
        QTRY_COMPARE(sender.peerDescriptions().size(), 1);
        introduce(sender, unrelated, receiver.port());
        QTest::qWait(50); QCOMPARE(sender.peerDescriptions().size(), 1);
        QVERIFY(sender.send(text())); QTRY_COMPARE(delivered.size(), 1);
        QCOMPARE(delivered.takeFirst().first().toJsonObject(), text());
        // Exercise fragmented TCP reads with a file larger than normal socket buffers.
        const QByteArray bytes(2 * 1024 * 1024, '\x7f');
        const QJsonObject files{{"kind", "files"}, {"files", QJsonArray{QJsonObject{
            {"name", "binary.bin"}, {"size", bytes.size()}, {"data", QString::fromLatin1(bytes.toBase64())}}}}};
        QVERIFY(sender.send(files)); QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, 10000);
        QCOMPARE(delivered.takeFirst().first().toJsonObject(), files);
        receiver.setPaused(true); QVERIFY(sender.send(text("paused")));
        QTest::qWait(100); QCOMPARE(delivered.size(), 0);
        sender.setPaused(true); QVERIFY(!sender.send(text()));
        receiver.setPaused(false); sender.setPaused(false);
        QVERIFY(sender.send(text("cancel pending send")));
        sender.setPaused(true);
        QTest::qWait(100); QCOMPARE(delivered.size(), 0);
    }
    void directDiscoveryWithoutBroadcasts() {
        Profile phone = profile(), desktop = profile(), stranger = profile(); desktop.key = phone.key;
        SyncEngine mobile(phone, nullptr, 0, 0), computer(desktop, nullptr, 0, 0), other(stranger, nullptr, 0, 0);
        QVERIFY(mobile.port() && computer.port() && other.port());
        // Only the phone knows the desktop's discovery endpoint. Its authenticated
        // unicast announcement must introduce both directions without a broadcast.
        mobile.contact(QHostAddress::LocalHost, computer.discoveryPort());
        QTRY_COMPARE(mobile.peerDescriptions().size(), 1);
        QTRY_COMPARE(computer.peerDescriptions().size(), 1);
        other.contact(QHostAddress::LocalHost, computer.discoveryPort());
        QTest::qWait(50);
        QCOMPARE(other.peerDescriptions().size(), 0);
        QCOMPARE(computer.peerDescriptions().size(), 1);
        QSignalSpy onPhone(&mobile, &SyncEngine::received), onComputer(&computer, &SyncEngine::received);
        QVERIFY(computer.send(text("to iPhone"))); QTRY_COMPARE(onPhone.size(), 1);
        QVERIFY(mobile.send(text("from iPhone"))); QTRY_COMPARE(onComputer.size(), 1);
        // Announcements cease after the initial reciprocal exchange (no echo loop).
        QSignalSpy changes(&computer, &SyncEngine::peersChanged);
        QTest::qWait(100); QCOMPARE(changes.size(), 0);
    }
    void activeTransferSocketDestruction_data() {
        QTest::addColumn<bool>("destroyEngine");
        QTest::newRow("socket-destroyed-before-engine") << false;
        QTest::newRow("engine-destroyed-during-transfer") << true;
    }
    void activeTransferSocketDestruction() {
        QFETCH(bool, destroyEngine);
        Profile local = profile(), remote = profile(); remote.key = local.key;
        // Keep the outgoing socket connected while the peer withholds its hello.
        QTcpServer silentPeer; QVERIFY(silentPeer.listen(QHostAddress::LocalHost, 0));
        auto sender = std::make_unique<SyncEngine>(local, nullptr, 0, 0);
        introduce(*sender, remote, silentPeer.serverPort());
        QTRY_COMPARE(sender->peerDescriptions().size(), 1);
        QVERIFY(sender->send(text()));
        QTRY_VERIFY(silentPeer.hasPendingConnections());
        std::unique_ptr<QTcpSocket> incoming(silentPeer.nextPendingConnection());
        const auto outgoing = sender->findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(outgoing.size(), 1);
        QTRY_COMPARE(outgoing.first()->state(), QAbstractSocket::ConnectedState);
        QSignalSpy changes(sender.get(), &SyncEngine::transfersChanged);
        if (destroyEngine) {
            sender.reset();
            // Shutdown must not report an artificial transfer failure to the UI.
            QCOMPARE(changes.size(), 0);
        } else {
            delete outgoing.first();
            QCOMPARE(sender->transfers().first().toMap().value("state").toString(), QStringLiteral("failed"));
            const auto id = sender->transfers().first().toMap().value("id").toString();
            QVERIFY(sender->retry(id));
            QTRY_VERIFY(silentPeer.hasPendingConnections());
            std::unique_ptr<QTcpSocket> retried(silentPeer.nextPendingConnection());
            sender->cancel(id);
            QCOMPARE(sender->transfers().first().toMap().value("state").toString(), QStringLiteral("cancelled"));
        }
        QTRY_COMPARE(incoming->state(), QAbstractSocket::UnconnectedState);
    }
    void qrDiscoveryOverTcpWithoutUdp_data() {
        QTest::addColumn<bool>("managed");
        QTest::newRow("legacy") << false;
        QTest::newRow("managed-approval-and-revocation") << true;
    }
    void qrDiscoveryOverTcpWithoutUdp() {
        QFETCH(bool, managed);
        Profile desktop = profile(); if (managed) desktop.createManaged();
        const auto qr = ConnectionInfo{desktop.invitation(), {"127.0.0.1"}}.encoded();
        ConnectionInfo scanned; QVERIFY(ConnectionInfo::parse(qr, &scanned));
        Profile phone = profile(); QVERIFY(phone.join(scanned.code));
        // Ephemeral UDP endpoints receive no announcements: this exchange uses TCP only.
        SyncEngine mac(desktop, nullptr, 0, 0), android(phone, nullptr, 0, 0);
        android.contactTcp(QHostAddress(scanned.addresses.first()), mac.port());
        QTRY_COMPARE(mac.devices().size(), 1); QTRY_COMPARE(android.devices().size(), 1);
        if (managed) {
            QVERIFY(mac.peerDescriptions().isEmpty());
            QVERIFY(!android.send(text("not yet approved")));
            QVERIFY(mac.authorizeDevice(phone.deviceId, false));
            // A fresh direct exchange carries the signed approval even if UDP is filtered.
            QTest::qWait(20);
            android.contactTcp(QHostAddress::LocalHost, mac.port());
            QTRY_VERIFY(DeviceAccess::approved(android.profile(), phone.deviceId));
        }
        QTRY_COMPARE(mac.peerDescriptions().size(), 1); QTRY_COMPARE(android.peerDescriptions().size(), 1);
        QSignalSpy onPhone(&android, &SyncEngine::received), onMac(&mac, &SyncEngine::received);
        QVERIFY(mac.send(text("Mac to Android after QR"))); QTRY_COMPARE(onPhone.size(), 1);
        QVERIFY(android.send(text("Android to Mac after QR"))); QTRY_COMPARE(onMac.size(), 1);
        QCOMPARE(onPhone.first().first().toJsonObject().value("text").toString(), QStringLiteral("Mac to Android after QR"));
        QCOMPARE(onMac.first().first().toJsonObject().value("text").toString(), QStringLiteral("Android to Mac after QR"));
        if (managed) {
            QVERIFY(mac.authorizeDevice(phone.deviceId, true));
            QTest::qWait(20); android.contactTcp(QHostAddress::LocalHost, mac.port());
            QTRY_VERIFY(!DeviceAccess::approved(android.profile(), phone.deviceId));
            QVERIFY(!android.send(text("revoked"))); QVERIFY(!mac.send(text("revoked")));
        }
    }
    void tcpDiscoveryRejectsWrongGroupsAndOversizedFrames() {
        Profile owner = profile(); owner.createManaged();
        SyncEngine receiver(owner, nullptr, 0, 0);
        auto request = [&](const QByteArray &payload, quint32 length) {
            QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost, receiver.port());
            QTRY_VERIFY(socket.bytesAvailable() >= 104); socket.read(104);
            QByteArray header("LCD?"); header.resize(8);
            qToBigEndian(length, reinterpret_cast<uchar *>(header.data() + 4));
            socket.write(header + payload);
            QTRY_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
            QVERIFY(receiver.devices().isEmpty());
        };
        const auto other = SecureProtocol::announcement(profile(), 45872);
        request(other, quint32(other.size())); request({}, 32769); request({}, 0);
    }
    void tcpDiscoveryDoesNotResumePausedSynchronization() {
        Profile owner = profile(); owner.createManaged(); owner.paused = true;
        Profile phone = profile(); QVERIFY(phone.join(owner.invitation()));
        SyncEngine mac(owner, nullptr, 0, 0), android(phone, nullptr, 0, 0);
        android.contactTcp(QHostAddress::LocalHost, mac.port());
        QTRY_COMPARE(mac.devices().size(), 1); QTRY_COMPARE(android.devices().size(), 1);
        QVERIFY(mac.profile().isPaused()); QVERIFY(!mac.send(text("paused")));
    }
    void importedQueueIsPrivateAndBounded() {
        QTemporaryDir directory;
        const Profile person = profile(), other = profile();
        QVERIFY(QDir(directory.path()).mkpath("imported/batch"));
        QFile file(directory.filePath("imported/batch/photo.jpg"));
        QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("photo"), qint64(5)); file.close();
        const QUrl photo = QUrl::fromLocalFile(file.fileName());
        QString error;
        QVERIFY(PendingFiles::append(person, {photo}, &error, directory.path()));
        QCOMPARE(PendingFiles::load(person, directory.path()), QList<QUrl>{photo});
        QVERIFY(PendingFiles::load(other, directory.path()).isEmpty());
        QVERIFY(PendingFiles::append(person, {photo}, &error, directory.path()));
        QCOMPARE(PendingFiles::load(person, directory.path()).size(), 1);
        QFile secret(directory.filePath("profile.json")); QVERIFY(secret.open(QIODevice::WriteOnly)); secret.write("secret"); secret.close();
        QVERIFY(!PendingFiles::append(person, {QUrl::fromLocalFile(secret.fileName())}, &error, directory.path()));
        // Failed appends must preserve the previous queue.
        QCOMPARE(PendingFiles::load(person, directory.path()).size(), 1);
        QFile huge(directory.filePath("imported/batch/huge.bin")); QVERIFY(huge.open(QIODevice::WriteOnly));
        QVERIFY(huge.resize(ClipboardPayload::MaxContentBytes + 1)); huge.close();
        QVERIFY(!PendingFiles::append(person, {QUrl::fromLocalFile(huge.fileName())}, &error, directory.path()));
        PendingFiles::clear(person, directory.path());
        QVERIFY(PendingFiles::load(person, directory.path()).isEmpty());
        QVERIFY(file.exists()); // Starting a transfer never destroys the retry copy.
        QFile malicious(directory.filePath("outgoing-" + person.groupId() + ".json"));
        QVERIFY(malicious.open(QIODevice::WriteOnly)); malicious.write("[\"imported/../profile.json\"]"); malicious.close();
        QVERIFY(PendingFiles::load(person, directory.path()).isEmpty());
    }
    void rejectWrongKeyAndReplayedFrame() {
        Profile recipient = profile(), member = profile(), stranger = profile(); member.key = recipient.key;
        SyncEngine receiver(recipient, nullptr, 0, 0);
        QSignalSpy delivered(&receiver, &SyncEngine::received);
        auto connectClient = [&](QTcpSocket &client) {
            client.connectToHost(QHostAddress::LocalHost, receiver.port());
        };
        QTcpSocket first; connectClient(first);
        QTRY_VERIFY(first.bytesAvailable() >= 104);
        const QByteArray hello = first.read(104), challenge = hello.mid(4, 32), receiverId = hello.mid(36, 36);
        const QByteArray senderId = member.deviceId.toUtf8();
        const QByteArray ctx = QByteArray("LocalClipboard/v2/frame/") + challenge + senderId + receiverId;
        const QByteArray wire = SecureProtocol::seal(member, QJsonDocument(text()).toJson(QJsonDocument::Compact), ctx);
        QByteArray header(4, Qt::Uninitialized); qToBigEndian(quint32(wire.size()), reinterpret_cast<uchar *>(header.data()));
        const QByteArray proof = senderId + SecureProtocol::mac(member, QByteArray("client/") + challenge + senderId + receiverId, "handshake");
        first.write(proof + header + wire); QTRY_COMPARE(delivered.size(), 1); delivered.clear();
        QTcpSocket replay; connectClient(replay); QTRY_VERIFY(replay.bytesAvailable() >= 104);
        const QByteArray second = replay.read(104);
        QVERIFY(second.mid(4, 32) != challenge);
        // Fresh valid handshake, old encrypted frame: challenge binding must reject it.
        replay.write(senderId + SecureProtocol::mac(member, QByteArray("client/") + second.mid(4, 32) + senderId + receiverId, "handshake") + header + wire);
        QTRY_COMPARE(replay.state(), QAbstractSocket::UnconnectedState); QCOMPARE(delivered.size(), 0);
        QTcpSocket wrong; connectClient(wrong); QTRY_VERIFY(wrong.bytesAvailable() >= 104);
        const QByteArray third = wrong.read(104), wrongId = stranger.deviceId.toUtf8();
        wrong.write(wrongId + SecureProtocol::mac(stranger, QByteArray("client/") + third.mid(4, 32) + wrongId + receiverId, "handshake") + header + wire);
        QTRY_COMPARE(wrong.state(), QAbstractSocket::UnconnectedState); QCOMPARE(delivered.size(), 0);
    }
};
QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
