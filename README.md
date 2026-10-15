# LocalClipboard

Encrypted LAN clipboard synchronization for **macOS, Linux, Windows, Android and iOS/iPadOS**.
Text, formatted text, images, photos and files go directly between your own devices;
there is no cloud, email account or central server.

## Registration and separate users

The installer opens the registration wizard (or open LocalClipboard after installing).
On your first device, enter your name and a device name, then **Create a personal
profile**. On your other devices, choose **Connect to my existing profile** and enter
the connection code shown by **Connect another device**.

Each person creates their own profile. For example, Anton's laptop and phone use
Anton's code; his partner's laptop and phone use a different code. Sharing the same
Wi-Fi does not connect these profiles. Device IDs, names, profile membership, pause
and startup preferences survive restarts and upgrades. A device uses one profile
at a time; **Change personal profile** registers it with a new or existing profile.

New profiles use `LC3.…` invitations and separate device keys. The profile owner's
device approves join requests and can revoke lost devices under **Devices and access**.
Compare fingerprints before approving. Revocations propagate when devices reconnect;
keep the owner available. Existing `LC2.…` profiles continue working and can be
explicitly upgraded, which requires reconnecting the other devices. Keep all
connection codes and QR images private; they are excluded from automatic sync.

See [delivery, devices, privacy and signed distribution](FEATURES.md) for transfer
receipts, photo previews, retries, optional encrypted history and diagnostic reports.

### Connect using a QR code

On macOS, allow LocalClipboard in **System Settings → Privacy & Security → Local Network**. Addresses from QR also enable authenticated TCP `45872` discovery when UDP `45871` is filtered; update both applications. Network isolation and denied permissions still block connections. For LC3 profiles, approve the joining device under **Devices and access**.

On a registered device, open **Connect device…** to display your profile's QR code.
On the new device, tap **Scan QR code** in registration, allow camera access and
scan the code, then confirm **Register**. This works on Android, iOS and all three
desktop platforms. An already registered device can use **Change profile…** to join.
Without a camera, use **Save QR image…** and **Read QR from image…**, or enter the
connection code manually. The QR contains the same private profile secret as the
connection code; share it only with your own devices. Other QR formats are ignored.

Android and iOS forms fill the available screen. Scroll the registration and
settings pages with a swipe; opening the keyboard keeps the focused input visible.
All five platforms use Qt Quick with inertial scrolling and explicit choices instead of combo boxes.
Wide desktop windows have a sidebar, hover feedback and keyboard shortcuts;
phones have bottom navigation. File preparation runs on workers; the
Android background service stays in a separate process. Android's QR scanner uses a native camera
preview with decoding off the UI thread; images can also be selected.
QR pairing fills in the profile key and local IPv4 addresses together on every platform
after registration is confirmed. Legacy `LC2.…` codes still work; enter addresses manually for them.

## Background operation and autostart

On desktop and Android, autostart is enabled by default at registration and can be toggled in settings.
Desktop synchronization runs in the **logged-in user's graphical session**, where
clipboard access is available. It does not run as a root/system service before login.

- **macOS:** a per-user LaunchAgent starts the app with `--background` at login and
  restarts it after failure. The app remains running when its window is closed.
- **Windows:** an HKCU Run entry starts it at login. Closing the window leaves the
  tray process running; the installer and profile are per user.
- **Linux:** XDG desktop autostart imports the graphical environment and starts a
  systemd user service with restart on failure. Desktops without a systemd user
  manager launch the background process directly. Display names are not hardcoded.
- **Android:** a separate Qt foreground service receives encrypted clipboard
  content, advertises the phone and shows notifications. It continues after the
  activity closes and starts after reboot / APK upgrades once registered, unless
  startup is disabled. It uses the `connectedDevice` foreground-service type for
  communication with local devices. OS force-stop and some manufacturers' battery
  settings can prevent restarting until the app is opened again.
- **iOS/iPadOS:** reception runs while the app is active. iOS suspends ordinary apps
  in the background and does not offer boot autostart for this app. Text/images are
  sent with an explicit clipboard button; native Photos/Files pickers and document
  opening support file imports. There is no background clipboard monitoring.

Reopening the desktop app opens the existing instance's window. Use **Quit LocalClipboard**
in settings or **Quit** in the tray menu to stop it. On desktops without a tray,
reopen it from the application launcher. Android also exposes the system's
foreground-service controls to stop its background work.

**Android 10+ does not permit ordinary background apps to read the clipboard.**
Open the app and press **Send current clipboard**, or use **Share → LocalClipboard**
from a gallery, file manager or another app. Received items are queued in private
storage; tap **Copy** in the notification or **Copy received clipboard** in the app.
Opening the app alone does not replace your clipboard. Enable banners in
**Settings → Notification settings**; Android and Do Not Disturb control their visibility. Photos
and files are exposed as read-only content URIs through FileProvider, so other apps
can paste them without storage-wide permissions. Whether an Android app accepts
file/image clipboard URIs depends on that app.

On iOS, scan the computer's QR to fill in the profile and local IPv4 addresses,
and allow **Local Network** access. With an old connection code, enter the IP in
**Network settings**. The updated desktop/Android
app replies directly, so no restricted broadcast entitlement is needed. Keep the
phone app open to receive. **Open in / Copy to LocalClipboard** imports documents
from apps that offer that action; arbitrary text sharing uses the clipboard button.
Files selected from **Send files / photos** or imported from another app are queued
for the active personal profile. The **Files** tab lists received copies for opening or deletion.
To retry imported content, select the source files again in Photos / Files. See the [iOS setup guide](packaging/ios/README.md).

## Clipboard formats

- Plain text and HTML formatting.
- Clipboard images, transferred as PNG (up to 32 megapixels, 8192 pixels per side).
- Up to 32 files per transfer, including photos, videos and documents. Bytes are
  transferred, then stored locally and placed in the receiving clipboard as files.
  Linux also supplies GNOME/KDE copy formats; Windows uses copy semantics.
- **Send files / photos** sends files selected from a picker without requiring them
  to already be in the clipboard.

The combined raw-content limit is **32 MiB per transfer**. Folders and symbolic links
are not transferred; archive a folder first. Cut/move operations are received as
copies. Unsafe paths and duplicate filenames are rejected; nothing is executed or
opened automatically. Received files stay in the application's private `received`
directory; the **Files** tab lets you access them. They are not automatically
deleted while another app may still need to paste them, so remove old files manually.
There is no transfer history, resumable streaming or delivery acknowledgement yet.
Desktop copies made while all registered peers are offline are not sent later;
Android's latest explicit outgoing item waits for a peer for up to ten minutes.

## Language and removal

Choose **Русский / English** in the registration wizard. You can change it later
in Settings without changing your profile. `--language ru` or
`--language en` selects the initial language for desktop / iOS launches.
The Windows installer has a language dialog; shell / Android installers prompt
for a language and also accept `LC_LANGUAGE=ru` or `LC_LANGUAGE=en`.
The macOS DMG contains a bilingual installer with an explicit language selector.

Run the same installer again to manage an existing installation:

- **macOS:** open the DMG and run **LocalClipboard Installer**. Choose Install/reinstall or Uninstall with radio buttons. Both actions preserve your profile and files. Administrator authorization is requested only when applying the chosen action.
- **Windows:** the installer detects the existing installation and offers
  **Reinstall** or **Uninstall**. Installed apps removal also remains available.
- **Debian/Ubuntu:** the shell installer detects the installed DEB and offers
  reinstall, removal or cancellation; sudo authorizes package changes.
- **Android:** run `install.sh` / `install.ps1` again with the same selected phone.
  When the app is installed, choose reinstall or uninstall. Removal requires `DELETE`.
- **iOS:** run `install.sh` with `IOS_DEVICE_ID` and your actual `IOS_BUNDLE_ID`.
  The installer queries that device and offers reinstall or removal if installed.
  Removal requires `DELETE` and works without the Qt kit or a new build.
  Without a device ID the script opens the Xcode installation flow.

Desktop installer removal preserves personal data by default. Windows removal uses
radio buttons to keep or erase data; keeping it is the default. The app's
**Uninstall…** dialog also offers **Erase my local data**. On Android and iOS,
removing the app deletes its local data. Other devices keep their profiles.
macOS installer receipts may remain; they contain package metadata, not clipboard
content. Linux/Windows native package removal outside our wizard keeps user data.

Application pages scroll when needed and wrap explanatory text. The macOS installer
keeps its primary action outside the scrolling content. Closing the desktop app window still leaves
synchronization running; use **Quit LocalClipboard** to stop it.

## Build: five packages and five installers

CMake 3.21.1+, a C++17 compiler, Qt 6 Core/Network/Widgets/Concurrent, Qt Declarative
(Quick/QuickControls2), and native packaging tools are required on every platform.
Desktop tests need Qt Test; desktop and iOS also need Qt Multimedia. Android uses native Camera2. CI pins Qt 6.11.2 for macOS, Windows, Android and iOS;
supported mobile OS versions are Android 9+ and iOS 17+.
Linux compatibility builds use the distribution Qt package; review its security
backports before distributing it. Monocypher 4.0.3 is vendored.

```sh
./scripts/build -t macos
./scripts/build -t linux
./scripts/build -t windows       # Git Bash + MSVC + Qt + NSIS
./scripts/build -t android       # Android Qt kit, SDK, NDK, Java 21
./scripts/build -t ios           # macOS + full Xcode + Qt iOS kit
```

Each command builds the app **and** its package and installer into `dist/` on the
corresponding host. `-n` builds only the app. With a configured CMake build, use
`cmake --build BUILD_DIR --config Release --target installers`.

| Platform | Application package | Installer |
| --- | --- | --- |
| Linux (Debian/Ubuntu) | `LocalClipboard-linux-amd64.deb` | `LocalClipboard-linux-amd64-installer.sh` |
| macOS | `LocalClipboard-macos-package.zip` (app bundle) | `LocalClipboard-macos-installer.dmg` |
| Windows | `LocalClipboard-windows-package.zip` | `LocalClipboard-windows-installer.exe` (NSIS) |
| Android | `LocalClipboard-android-package.apk` | `LocalClipboard-android-installer.zip` (APK + adb installation/registration launch scripts) |
| iOS/iPadOS | `LocalClipboard-ios-package.zip` (arm64 device `.app`) | `LocalClipboard-ios-installer.zip` (source + Xcode build/sign/install script) |

Linux filenames use the host architecture. APK is also directly installable on the
phone; the separate installer ZIP automates USB installation from a computer with
adb. CI is configured to build all five platforms, test the desktop core and verify all ten
outputs before publishing a combined artifact with checksums. Android CI builds an
installable debug APK. Its development signing key is cached between builds;
set the `ANDROID_DEBUG_KEYSTORE_BASE64` repository secret to preserve it independently
of cache eviction. See [Android signing setup](dependencies/README.md#android-development-signing).
Distribution release APKs need a stable release signing key. macOS
artifacts are ad-hoc signed unless Apple signing identities are supplied; public
Gatekeeper distribution additionally requires Apple signing and notarization.
The iOS CI package is **unsigned**. Install on your own phone through the supplied
Xcode installer, selecting your Apple Team and a unique bundle ID. It needs Xcode
and the Qt iOS kit on the installing Mac; it is not an App Store IPA. A Personal
Team can be used without App Groups or multicast capabilities. See the
[iOS installation instructions](packaging/ios/README.md).

See [build scripts](scripts/README.md) and [dependencies](dependencies/README.md).

```sh
cmake -S . -B build_desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build_desktop --parallel 4
ctest --test-dir build_desktop --output-on-failure
```

## Network and security

Discovery uses IPv4 UDP `45871`; encrypted transfers normally use TCP `45872`
(a free port is advertised if another user session owns that port). Allow these
ports on private/LAN firewall profiles. Router client isolation, guest Wi-Fi, VPN
routing and filtered broadcasts may prevent discovery. **Network settings** accepts
up to 32 direct IPv4 peer addresses on every platform. iOS uses direct discovery
only; enter one peer address on the phone to discover both directions. IPv6-only
networks are not supported. Device clocks must be within 60 seconds
for discovery announcements to be accepted.

Announcements are authenticated using keyed BLAKE2b. A fresh mutual-authentication
challenge precedes each TCP transfer; clipboard payloads use XChaCha20-Poly1305
with that challenge and both device IDs bound to the ciphertext. Wrong profiles,
plaintext legacy packets, tampered data and replays from previous connections are
rejected. Unauthenticated sockets cannot allocate the full transfer buffer.

Android wraps profile secrets with AES-GCM using a non-exportable Android Keystore
key and migrates older profiles on launch. Clipboard queues and received files stay
plaintext in app-private storage with backups disabled. Desktop/iOS profile secrets
remain in a user-private configuration file; Windows relies on directory ACLs.
Received files are limited to 512 MiB / 16,384 files: existing copies are kept and
new transfers are refused when full.
This protects against unrelated LAN users, not a compromised registered device or
malware running as your OS user. The protocol has not had an independent security
audit and group keys do not provide forward secrecy. Pause synchronization when
copying content you do not want on your other registered devices.

See [security checks and release limits](SECURITY.md) for the threat model,
sanitizer/fuzzer commands and checks requiring real devices and signing.

Android platform constraints are documented in the official
[clipboard access rules](https://developer.android.com/about/versions/10/privacy/changes#clipboard-data)
and [connected-device service documentation](https://developer.android.com/develop/background-work/services/fgs/service-types#connected-device).

Apple documents [background suspension](https://developer.apple.com/documentation/uikit/extending-your-app-s-background-execution-time),
[paste permissions](https://developer.apple.com/documentation/uikit/uipastecontrol),
and the [restricted broadcast/multicast entitlement](https://developer.apple.com/news/?id=0oi77447).

The branded app icon is included on all five platforms, in the desktop tray and
Android background-service notification. Assets ship with the source; regular
builds need no icon generation step. See [icon assets](assets/icons/README.md)
for regeneration after changing the SVG.

---

## Русская версия

### LocalClipboard

Зашифрованная синхронизация буфера обмена в локальной сети для **macOS, Linux, Windows, Android и iOS/iPadOS**. Текст, форматированный текст, изображения, фотографии и файлы передаются напрямую между вашими устройствами. Облако, учётная запись электронной почты и центральный сервер не нужны.

### Регистрация и отдельные пользователи

Установщик открывает мастер регистрации; также можно открыть LocalClipboard после установки. На первом устройстве укажите своё имя и имя устройства, затем выберите **«Создать личный профиль»**. На остальных устройствах выберите подключение к существующему профилю и введите код, который отображается при подключении другого устройства.

Каждый человек создаёт отдельный профиль. Например, ноутбук и телефон Антона используют его код, а устройства его девушки — другой. Подключение к одной сети Wi-Fi не объединяет эти профили. Идентификаторы и имена устройств, принадлежность к профилю, настройки паузы и автозапуска сохраняются после перезапуска и обновления. Устройство использует один профиль одновременно; смена личного профиля позволяет зарегистрировать его в новом или существующем профиле.

Новые профили используют приглашения `LC3.…` и отдельные ключи устройств. Устройство
владельца одобряет подключения и отзывает потерянные устройства в разделе
**Устройства и доступ**. Перед одобрением сравнивайте отпечатки. Отзыв распространяется
при подключении: держите владельца в сети. Существующие профили `LC2.…` работают и
могут быть явно обновлены с подключением остальных устройств заново. Храните коды и
QR приватно; они исключены из автоматической синхронизации.

Подтверждения доставки, миниатюры, повторы, необязательная зашифрованная история и
отчёты описаны в [руководстве по возможностям и подписи](FEATURES.md#русская-версия).

### Подключение по QR-коду

На macOS разрешите LocalClipboard доступ в **«Системные настройки → Конфиденциальность и безопасность → Локальная сеть»**. Адреса из QR также позволяют выполнять аутентифицированное обнаружение через TCP `45872`, если UDP `45871` фильтруется; обновите оба приложения. Изоляция сети и отказ в разрешении по-прежнему блокируют подключение. В профилях LC3 одобрите новое устройство в разделе **«Устройства и доступ»**.

На зарегистрированном устройстве откройте **«Подключить устройство…»**, чтобы показать QR-код личного профиля. На новом устройстве нажмите **«Сканировать QR-код»** в мастере регистрации, разрешите доступ к камере, отсканируйте код и подтвердите регистрацию. Это доступно на Android, iOS, macOS, Windows и Linux. Уже зарегистрированное устройство может подключиться к другому профилю через **«Сменить профиль…»**.

Если камеры нет, используйте **«Сохранить QR-код…»** и **«Считать QR из изображения…»** либо введите код вручную. QR содержит тот же секрет профиля, что и код подключения: передавайте его только своим устройствам. QR-коды других форматов игнорируются.

На Android и iOS формы занимают доступный экран. Страницы регистрации и настроек прокручиваются свайпом; при открытии клавиатуры поле ввода остаётся в видимой области. Все пять платформ используют Qt Quick с инерционной прокруткой и явным выбором вместо комбобоксов. В широком окне компьютера есть боковая навигация, реакция на наведение и сочетания клавиш; на телефонах — нижняя навигация. Подготовка файлов выполняется в рабочих потоках, а фоновая служба Android работает в отдельном процессе. Сканер Android использует нативный предпросмотр камеры с распознаванием вне потока интерфейса; также можно выбрать изображение.

На всех платформах QR-регистрация заполняет ключ профиля и локальные IPv4-адреса вместе после подтверждения регистрации. Старые коды `LC2.…` поддерживаются; для них адреса указываются вручную.

### Фоновая работа и автозапуск

На компьютерах и Android автозапуск включается по умолчанию при регистрации; его можно изменить в настройках. На компьютере синхронизация работает в **графическом сеансе вошедшего пользователя**, где доступен буфер обмена. Она не запускается до входа как служба root или системная служба.

- **macOS:** пользовательский LaunchAgent запускает приложение с `--background` при входе и перезапускает его после сбоя. Закрытие окна не останавливает приложение.
- **Windows:** запись HKCU Run запускает приложение при входе. После закрытия окна процесс остаётся в трее; установка и профиль относятся к текущему пользователю.
- **Linux:** автозапуск XDG передаёт графическое окружение и запускает пользовательскую службу systemd с перезапуском после сбоя. Если пользовательский менеджер systemd недоступен, фоновый процесс запускается напрямую. Имена дисплеев не заданы жёстко.
- **Android:** отдельная служба Qt переднего плана принимает зашифрованное содержимое буфера, объявляет телефон в сети и показывает уведомления. Она продолжает работать после закрытия экрана приложения и запускается после перезагрузки или обновления APK, если устройство зарегистрировано и автозапуск включён. Для связи с локальными устройствами используется тип службы `connectedDevice`. Принудительная остановка через ОС и некоторые настройки энергосбережения производителей могут блокировать запуск до следующего открытия приложения.
- **iOS/iPadOS:** приём работает, пока приложение активно. iOS приостанавливает обычные приложения в фоне и не предоставляет этому приложению автозапуск при загрузке. Текст и изображения отправляются отдельной кнопкой; системные окна «Фото» и «Файлы», а также открытие документов поддерживают импорт файлов. Фоновое отслеживание буфера отсутствует.

Повторное открытие приложения на компьютере показывает окно уже работающего экземпляра. Для остановки выберите завершение LocalClipboard в настройках или выход в меню трея. Если трея нет, снова откройте приложение через меню приложений. Android также предоставляет системные средства остановки служб переднего плана.

**Android 10+ запрещает обычным фоновым приложениям читать буфер обмена.** Откройте приложение и отправьте текущий буфер либо выберите **«Поделиться → LocalClipboard»** в галерее, файловом менеджере или другом приложении. Полученное содержимое сохраняется в очереди приватного хранилища; нажмите **«Скопировать»** в уведомлении или **«Скопировать полученный буфер»** в приложении. Простое открытие приложения не заменяет буфер. Включите баннеры в **«Настройки → Настройки уведомлений»**; их видимость зависит от Android и режима «Не беспокоить». Фото и файлы доступны через FileProvider в виде URI содержимого только для чтения: другие приложения могут вставлять их без разрешения на доступ ко всему хранилищу. Поддержка файлов и изображений из таких URI зависит от принимающего приложения Android.

На iOS отсканируйте QR компьютера, чтобы заполнить профиль и локальные IPv4-адреса, и разрешите доступ к **«Локальной сети»**. При использовании старого кода подключения укажите IP вручную в настройках сети. Обновлённое приложение на компьютере или Android отвечает напрямую, поэтому ограниченное разрешение на широковещательную передачу не нужно. Для приёма оставляйте приложение на телефоне открытым. Действие **«Открыть в / Скопировать в LocalClipboard»** импортирует документы из приложений, которые его предлагают; для произвольного текста используется кнопка отправки буфера. Файлы из окна отправки файлов и фотографий или из другого приложения попадают в очередь активного личного профиля. Вкладка **«Файлы»** показывает полученные копии для открытия или удаления. Для повторной отправки импортированного содержимого снова выберите исходные файлы в «Фото» или «Файлах». См. [инструкцию для iOS](packaging/ios/README.md#русская-версия).

### Форматы буфера обмена

- Обычный текст и форматирование HTML.
- Изображения из буфера передаются в PNG: до 32 мегапикселей и до 8192 пикселей по каждой стороне.
- До 32 файлов за одну передачу, включая фото, видео и документы. Передаётся содержимое файлов, которое сохраняется локально и помещается в буфер принимающего устройства. Linux также предоставляет форматы копирования GNOME/KDE; Windows использует операцию копирования.
- Отправка файлов и фотографий позволяет выбрать их в системном окне, без предварительного помещения в буфер.

Общий предел исходного содержимого — **32 МиБ на передачу**. Папки и символические ссылки не передаются: предварительно упакуйте папку в архив. Операции вырезания или перемещения принимаются как копирование. Небезопасные пути и повторяющиеся имена файлов отклоняются; ничего не запускается и не открывается автоматически. Полученные файлы остаются в приватном каталоге приложения `received`; открыть их можно через вкладку **«Файлы»**. Они не удаляются автоматически, поскольку другому приложению ещё может понадобиться их вставить. Удаляйте старые файлы вручную.

История передач, возобновление прерванной передачи и подтверждение доставки пока отсутствуют. Содержимое, скопированное на компьютере, когда все зарегистрированные устройства отключены, позже не отправляется. На Android последний явно отправленный элемент ожидает доступного устройства до десяти минут.

### Язык и удаление

Выберите **«Русский / English»** в мастере регистрации. Позже язык можно изменить в настройках, сохранив профиль. Параметры `--language ru` и `--language en` задают начальный язык при запуске на компьютере или iOS. Установщик Windows показывает окно выбора языка; установщики для командной строки и Android предлагают выбрать язык и поддерживают `LC_LANGUAGE=ru` или `LC_LANGUAGE=en`. Установщик macOS в DMG позволяет выбрать русский или английский язык до установки, переустановки или удаления.

Для управления установленным приложением снова запустите тот же установщик:

- **macOS:** откройте DMG и запустите **LocalClipboard Installer**. Радиокнопки выбирают установку / переустановку или удаление. Обе операции сохраняют профиль и файлы; при выполнении выбранного действия установщик запрашивает права администратора.
- **Windows:** установщик обнаруживает существующую установку и предлагает переустановку или удаление. Удаление через список установленных приложений Windows также доступно.
- **Debian/Ubuntu:** установщик для командной строки обнаруживает установленный DEB и предлагает переустановку, удаление или отмену. Изменение пакетов подтверждается через sudo.
- **Android:** повторно запустите `install.sh` или `install.ps1`, выбрав тот же телефон. Если приложение установлено, можно переустановить или удалить его. Удаление требует ввода `DELETE`.
- **iOS:** запустите `install.sh` с `IOS_DEVICE_ID` и фактическим `IOS_BUNDLE_ID`. Установщик проверяет выбранное устройство и, если приложение установлено, предлагает переустановку или удаление. Удаление требует `DELETE`, но не требует Qt или новой сборки. Без идентификатора устройства открывается сценарий установки через Xcode.

Удаление через установщики для компьютеров по умолчанию сохраняет личные данные. На Windows радиокнопки выбирают сохранение или стирание данных; по умолчанию выбрано сохранение. Диалог **«Удалить…»** внутри приложения также предлагает **«Стереть мои локальные данные»**. На Android и iOS удаление приложения стирает его локальные данные. Профили остальных устройств сохраняются. На macOS могут остаться квитанции установщика: они содержат сведения о пакете, а не содержимое буфера. Штатное удаление пакета Linux или Windows вне нашего мастера сохраняет пользовательские данные.

Страницы приложения при необходимости прокручиваются, пояснения переносятся по строкам. Установщик macOS держит основную кнопку действия вне прокручиваемой области. Закрытие окна на компьютере оставляет синхронизацию работающей; для остановки завершите LocalClipboard.

### Сборка: пять пакетов и пять установщиков

На каждой платформе нужны CMake 3.21.1 или новее, компилятор C++17, модули Qt 6 Core/Network/Widgets/Concurrent, Qt Declarative (Quick/QuickControls2) и нативные инструменты упаковки. Тестам для компьютеров нужен Qt Test; компьютерам и iOS также нужен Qt Multimedia. Android использует нативную Camera2. В CI закреплён Qt 6.11.2 для macOS, Windows, Android и iOS; поддерживаются Android 9+ и iOS 17+. Сборки совместимости Linux используют Qt дистрибутива; перед распространением проверьте его исправления безопасности. Monocypher 4.0.3 включён в исходники.

```sh
./scripts/build -t macos
./scripts/build -t linux
./scripts/build -t windows       # Git Bash + MSVC + Qt + NSIS
./scripts/build -t android       # Комплект Qt для Android, SDK, NDK, Java 21
./scripts/build -t ios           # macOS + полный Xcode + комплект Qt для iOS
```

Каждая команда собирает приложение **вместе с пакетом и установщиком** в `dist/` на соответствующей ОС. Параметр `-n` собирает только приложение. Для уже настроенной сборки CMake используйте `cmake --build BUILD_DIR --config Release --target installers`.

| Платформа | Пакет приложения | Установщик |
| --- | --- | --- |
| Linux (Debian/Ubuntu) | `LocalClipboard-linux-amd64.deb` | `LocalClipboard-linux-amd64-installer.sh` |
| macOS | `LocalClipboard-macos-package.zip` — пакет приложения `.app` | `LocalClipboard-macos-installer.dmg` |
| Windows | `LocalClipboard-windows-package.zip` | `LocalClipboard-windows-installer.exe` — NSIS |
| Android | `LocalClipboard-android-package.apk` | `LocalClipboard-android-installer.zip` — APK и скрипты установки через adb с открытием регистрации |
| iOS/iPadOS | `LocalClipboard-ios-package.zip` — `.app` для устройства arm64 | `LocalClipboard-ios-installer.zip` — исходники и скрипт сборки, подписи и установки через Xcode |

Имена файлов Linux зависят от архитектуры компьютера. APK можно установить непосредственно на телефоне; отдельный ZIP установщика автоматизирует установку через USB с компьютера с adb. CI настроен на сборку всех пяти платформ, проверку ядра приложения для компьютеров и всех десяти результатов перед публикацией общего архива с контрольными суммами. Для Android CI собирает устанавливаемый отладочный APK и кеширует ключ между сборками. Секрет репозитория `ANDROID_DEBUG_KEYSTORE_BASE64` позволяет сохранить подпись независимо от удаления кеша; см. [настройку подписи Android](dependencies/README.md#подпись-отладочных-сборок-android). Выпускной APK для распространения требует постоянного выпускного ключа. Артефакты macOS получают подпись ad-hoc, если не заданы сертификаты Apple; публичное распространение с проверкой Gatekeeper дополнительно требует подписи Apple и нотариализации.

Пакет iOS из CI **не подписан**. Устанавливайте его на свой телефон через прилагаемый установщик для Xcode, выбрав свою Apple Team и уникальный Bundle ID. На Mac для установки нужны Xcode и комплект Qt для iOS; это не IPA для App Store. Можно использовать Personal Team без App Groups и разрешения на multicast. См. [инструкцию установки iOS](packaging/ios/README.md#русская-версия).

Также см. [скрипты сборки](scripts/README.md#русская-версия) и [зависимости](dependencies/README.md#русская-версия).

```sh
cmake -S . -B build_desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build_desktop --parallel 4
ctest --test-dir build_desktop --output-on-failure
```

### Сеть и безопасность

Обнаружение использует IPv4 UDP `45871`, зашифрованные передачи обычно используют TCP `45872`. Если этот порт занят другим пользовательским сеансом, объявляется свободный порт. Разрешите эти порты в профиле брандмауэра для частной или локальной сети. Изоляция клиентов на роутере, гостевой Wi-Fi, маршрутизация VPN и фильтрация широковещательных пакетов могут мешать обнаружению. На всех платформах настройки сети принимают до 32 прямых IPv4-адресов других устройств. iOS использует только прямое обнаружение: достаточно указать адрес одного устройства на телефоне, чтобы обнаружение заработало в обоих направлениях. Сети только с IPv6 не поддерживаются. Расхождение часов устройств не должно превышать 60 секунд, иначе объявления обнаружения отклоняются.

Объявления аутентифицируются с помощью BLAKE2b с ключом. Каждой передаче TCP предшествует новый обмен для взаимной аутентификации; содержимое буфера шифруется XChaCha20-Poly1305 с привязкой этого обмена и идентификаторов обоих устройств к шифротексту. Отклоняются чужие профили, незашифрованные пакеты старого протокола, изменённые данные и повторные сообщения предыдущих соединений. Неаутентифицированные соединения не могут выделить полный буфер передачи.

На Android секрет профиля шифруется AES-GCM ключом из Android Keystore, который нельзя экспортировать; старый профиль переносится при запуске. Очереди буфера и полученные файлы остаются незашифрованными в приватном хранилище с отключённым резервным копированием. На компьютерах и iOS секрет профиля остаётся в приватном файле конфигурации; Windows опирается на ACL пользовательского каталога. Полученные файлы ограничены 512 МиБ и 16 384 файлами: существующие копии сохраняются, а новые передачи при заполнении отклоняются. Это защищает от посторонних пользователей локальной сети, но не от взломанного зарегистрированного устройства или вредоносного ПО от имени вашего пользователя ОС. Протокол не проходил независимый аудит безопасности; групповые ключи не обеспечивают прямую секретность. Приостановите синхронизацию перед копированием содержимого, которое не должно попадать на остальные устройства.

Модель угроз, команды санитайзеров и фаззинга, а также проверки на реальных устройствах и с подписью описаны в [проверках безопасности и ограничениях выпуска](SECURITY.md#русская-версия).

Ограничения Android описаны в официальных документах: [правила доступа к буферу обмена](https://developer.android.com/about/versions/10/privacy/changes#clipboard-data) и [службы для подключённых устройств](https://developer.android.com/develop/background-work/services/fgs/service-types#connected-device).

Apple описывает [приостановку приложений в фоне](https://developer.apple.com/documentation/uikit/extending-your-app-s-background-execution-time), [разрешения на вставку](https://developer.apple.com/documentation/uikit/uipastecontrol) и [ограниченное разрешение на широковещательную передачу и multicast](https://developer.apple.com/news/?id=0oi77447).

Фирменная иконка встроена на всех пяти платформах, в трее компьютера и уведомлении фоновой службы Android. Ресурсы входят в исходники; обычная сборка не требует генерации иконок. Инструкция по обновлению после изменения SVG находится в [описании ресурсов иконки](assets/icons/README.md#русская-версия).
