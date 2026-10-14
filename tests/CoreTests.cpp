#include "ClipboardPayload.h"
#include "Profile.h"
#include "PendingFiles.h"
#include "SecureProtocol.h"
#include "SyncEngine.h"
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
