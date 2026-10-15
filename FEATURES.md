# Delivery, devices and privacy

## Delivery and files

**Transfers** shows the most recent in-process transfers per device: Sending,
Received on device, Copied to clipboard, Failed or Cancelled. The received state
requires an authenticated receipt. The copied state requires successful application
to the clipboard. Android confirms receipt after saving its inbox, then confirms
copying after the user chooses Copy. iOS must remain open to receive.

The progress bar measures bytes handed to the socket, not remote disk progress.
Cancellation stops pending/network work; it cannot recall content already received.
Retry is available for the latest retained batch while the process remains alive.
Transfer metadata and retry payloads are not a durable outgoing history. Old peers
without receipt support never produce a false delivery confirmation: update both
ends. Retried transfer IDs are deduplicated in the receiver's bounded in-memory
cache. Restarting the receiver clears this cache.

Received files show a size and photo preview when decoding is supported. Open,
Delete local copy and Send again operate only on validated local received files.
The existing 32 MiB content / 32 files per transfer limits remain. Resumable streaming
of larger files is a later protocol feature, not part of this release.

## Device access

QR addresses also enable authenticated discovery over TCP 45872 if UDP discovery
is filtered. Update both applications for this fallback. macOS must allow
LocalClipboard in System Settings → Privacy & Security → Local Network. QR pairing
cannot bypass a denied permission, firewall or guest-network client isolation.
If TCP 45872 is occupied and the receiver uses an alternate port, initial discovery
still requires UDP. A new LC3 device remains pending until the owner approves it.

New profiles use `LC3` invitations. Each device has a private X25519 key. The first
device additionally owns an EdDSA authority seed and signs the registry of up to
64 identities. The connection code contains the discovery secret and authority
public key, never device private keys or the authority seed.

Scan the owner's QR, then open **Devices and access** on the owner's device.
Compare the fingerprint displayed on the joining device before approving it.
An invitation alone permits a join request, not clipboard transfer. Names are
labels; fingerprints identify keys. Rename the current device on this screen.
Last seen is remembered locally, including while a device is offline.

Payload and receipt keys derive from X25519 between approved devices and the
profile secret. Signed, increasing registry revisions propagate through discovery.
Only the owner can revoke access. A revoked identity cannot be reapproved with
its old key; reinstall/reset and approve a new identity if needed. Offline devices
learn revocations when they reconnect to a device carrying the newer registry.
Keep the owner online during revocation and reconnect all remaining devices.
Two offline devices with an old registry cannot know about a new revocation.
If the owner itself is lost, create a new profile on a remaining device.

Existing `LC2` profiles retain their shared-key behavior until **Upgrade device
access…** is explicitly confirmed. Upgrading rotates the profile secret, clears
local history and requires reconnecting every other device. New installs and
upgrades preserve stored profiles; upgrading the application alone does not rotate
keys. Pairwise static keys do not provide forward secrecy. This change needs an
independent protocol review before claiming a security certification.

## Privacy and local history

Pause for 15 minutes resumes automatically; the ordinary pause switch remains
indefinite. Sending and receiving can be disabled independently. macOS and Windows
can suppress automatic sending while an excluded application is in the foreground.
Use bundle identifiers on macOS or executable names/paths on Windows. Foreground
application detection is a best effort heuristic, not proof of the clipboard's
source. Manual sending remains available. Source exclusions are unavailable on
Android, iOS and generic Linux/Wayland.

Local history starts disabled. Enabling it stores encrypted payloads on this device
only, with a maximum of 100 items, a retention of 1–30 days and 1–64 MiB. UI presets
are 1 day / 4 MiB and 7 days / 16 MiB. Oversized entries are skipped. Files are stored
as encrypted copies in history; received files have their separate storage limit.
Search and Copy again work on retained entries. Disabling or clearing history
removes history files. Expiration runs during history access and while the app is
running; suspended/offline apps prune after resuming. Deletion is not secure media
erasure. History uses the device private key for managed profiles, or the profile
key for legacy profiles.

## Diagnostics and signed distribution

**Network and diagnostics** refreshes discovery and exports an allowlisted JSON
report: versions, interface count, online peer count and setting flags. It does not
include clipboard data, file/device/user names, IP addresses or any keys/codes.
It reports local availability; it cannot prove a router or firewall permits a
remote connection. Copying the report uses the ordinary system clipboard.

Normal builds still produce the platform package and installer. For public desktop
releases, set `LC_REQUIRE_SIGNING=1`; packaging then fails if signing prerequisites
are missing. macOS needs `MACOS_APP_SIGN_IDENTITY`, `MACOS_INSTALLER_SIGN_IDENTITY`
and `MACOS_NOTARY_PROFILE` (an existing notarytool keychain profile). The build signs,
notarizes and staples artifacts. Windows needs `LC_WINDOWS_CERT_SHA1` for a
certificate installed in CurrentUser/My and `LC_WINDOWS_TIMESTAMP_URL`; application
and installer are signed and verified with SignTool. Linux needs `LC_GPG_SIGNING_KEY`;
both artifacts get detached armored signatures verified by GPG. Publish the trusted
public key separately. Distribution certificates/keys are not generated or stored
in the repository. Android keeps its stable APK signing key; iOS personal installation
uses the existing Xcode team workflow.

---

## Русская версия

# Доставка, устройства и приватность

## Доставка и файлы

Экран **Передачи** показывает последние передачи текущего процесса по устройствам:
«Отправляется», «Получено устройством», «Скопировано в буфер», «Ошибка», «Отменено».
Получение подтверждается аутентифицированным ответом. Копирование подтверждается
после применения к буферу. Android подтверждает получение после сохранения входящей
очереди, а копирование — после нажатия «Копировать». На iOS приложение должно быть открыто.

Прогресс измеряет байты, переданные сокету, а не запись на удалённый диск. Отмена
останавливает ожидание и сетевую работу, но не отзывает уже полученные данные. Повтор
доступен для последнего сохранённого в памяти набора до завершения процесса. Это не
постоянный журнал исходящих данных. Старые версии без подтверждений не показывают
ложную доставку: обновите обе стороны. Повторные идентификаторы подавляются ограниченным
кэшем получателя в памяти; перезапуск очищает кэш.

У полученных файлов отображаются размер и миниатюра поддерживаемого изображения.
Открытие, удаление локальной копии и повторная отправка работают только с проверенными
локальными файлами. Сохраняются пределы 32 МиБ содержимого и 32 файла на передачу.
Потоковая передача больших файлов с возобновлением остаётся следующим этапом.

## Доступ устройств

Адреса из QR также используются для аутентифицированного обнаружения через TCP
45872, если UDP фильтруется. Для этого обновите оба приложения. На macOS разрешите
LocalClipboard доступ в «Системные настройки → Конфиденциальность и безопасность →
Локальная сеть». QR не обходит отказ в разрешении, брандмауэр и изоляцию клиентов
гостевой сети. Если TCP 45872 занят и получатель использует другой порт, первоначальное
обнаружение по-прежнему требует UDP. Новое устройство LC3 ожидает одобрения владельца.

Новые профили используют приглашения `LC3`. У каждого устройства — личный ключ X25519.
Первое устройство также хранит секрет центра подписи EdDSA и подписывает реестр до
64 устройств. В коде подключения — секрет обнаружения и открытый ключ центра подписи;
личных ключей и секрета центра подписи там нет.

Сканируйте QR владельца, затем откройте у него **Устройства и доступ**. До одобрения
сравните отпечаток с экраном нового устройства. Приглашение разрешает запрос на
подключение, а не обмен буфером. Имя служит подписью, отпечаток идентифицирует ключ.
На этом экране можно переименовать текущее устройство. Время последнего обнаружения
сохраняется локально, в том числе для устройств не в сети.

Ключи данных и подтверждений выводятся из X25519 одобренных устройств и секрета
профиля. Подписанные возрастающие версии реестра распространяются при обнаружении.
Отзывать доступ может только владелец. Отозванную личность нельзя одобрить со старым
ключом: при необходимости сбросьте регистрацию и одобрите новое устройство. Устройства
не в сети узнают об отзыве после подключения к узлу с новым реестром. При отзыве
держите владельца в сети и переподключите остальные устройства. Два узла со старым
реестром, изолированные от обновлений, не могут знать о новом отзыве. Если потерян
сам владелец, создайте новый профиль на оставшемся устройстве.

Существующие профили `LC2` продолжают использовать общий ключ до явного подтверждения
«Обновить защиту устройств…». Переход меняет секрет, очищает локальную историю и
требует подключения остальных устройств заново. Установка обновления приложения
сохраняет профиль и сама ключи не меняет. Статические ключи пар не обеспечивают прямую
секретность. До заявлений о сертификации протоколу необходим независимый аудит.

## Приватность и локальная история

Пауза на 15 минут снимается автоматически; обычный переключатель паузы действует
бессрочно. Отправка и приём отключаются раздельно. На macOS и Windows можно исключить
автоматическую отправку, пока активно указанное приложение: по идентификатору пакета
macOS либо имени или пути EXE в Windows. Активное приложение — приближение, а не
доказательство источника буфера. Ручная отправка доступна. Для Android, iOS и общего
случая Linux/Wayland исключения приложений недоступны.

История изначально отключена. При включении содержимое шифруется и хранится только
на этом устройстве: до 100 элементов, 1–30 дней и 1–64 МиБ. Пресеты интерфейса:
1 день / 4 МиБ и 7 дней / 16 МиБ. Слишком большие элементы пропускаются. Файлы в
истории — отдельные зашифрованные копии; у полученных файлов свой лимит хранения.
Доступны поиск и повторное копирование. Отключение и очистка удаляют файлы истории.
Срок проверяется при обращении к истории и во время работы приложения; после сна
проверка выполняется при возобновлении. Удаление не гарантирует стирание с носителя.
В новых профилях история использует личный ключ устройства, в старых — ключ профиля.

## Диагностика и подпись дистрибутивов

Экран **Сеть и диагностика** обновляет обнаружение и экспортирует JSON по разрешённому
списку полей: версии, число интерфейсов и устройств в сети, состояния настроек. В нём
нет буфера, имён файлов, устройств и пользователей, IP-адресов, ключей и кодов. Это
проверка локальной доступности; она не доказывает проходимость маршрутизатора или
брандмауэра. Копирование отчёта использует обычный системный буфер.

Обычная сборка создаёт пакет и установщик платформы. Для публичного настольного
выпуска задайте `LC_REQUIRE_SIGNING=1`: при отсутствии подписи упаковка завершится
ошибкой. Для macOS нужны `MACOS_APP_SIGN_IDENTITY`, `MACOS_INSTALLER_SIGN_IDENTITY`
и `MACOS_NOTARY_PROFILE` — существующий профиль notarytool в связке ключей. Сборка
подписывает, нотариализует и прикрепляет подтверждение к артефактам. Для Windows
нужны `LC_WINDOWS_CERT_SHA1` сертификата в CurrentUser/My и `LC_WINDOWS_TIMESTAMP_URL`;
приложение и установщик подписываются и проверяются SignTool. Для Linux нужен
`LC_GPG_SIGNING_KEY`: для обоих артефактов создаются и проверяются отделённые подписи
GPG в текстовом виде. Публикуйте доверенный открытый ключ отдельно. Сертификаты и
ключи распространения не генерируются и не хранятся в репозитории. Android сохраняет
постоянный ключ подписи APK, а личная установка iOS использует существующую Team Xcode.
