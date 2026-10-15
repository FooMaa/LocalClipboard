# LocalClipboard for your iPhone / iPad

Requires iOS 17+, a Mac with full Xcode (iOS SDK installed), and the Qt 6 iOS kit.
Xcode Command Line Tools and desktop Homebrew Qt alone are insufficient.

## Install through your own Xcode

1. Install matching Qt iOS and macOS host kits (CI uses Qt 6.11.2).
2. Connect the iPhone, enable Developer Mode, and trust the Mac if asked.
3. Unzip `LocalClipboard-ios-installer.zip` and run:

   ```sh
   export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
   export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
   ./install.sh
   ```

4. The script builds an unsigned app and opens its Xcode project. Add your Apple
   account in Xcode Settings → Accounts. Select the **LocalClipboard** app target,
   choose your **Team**, enable automatic signing, and choose a unique Bundle
   Identifier, for example `com.yourname.LocalClipboard`.
5. Select your connected iPhone/iPad as the run destination and press **Run**.
   Follow Xcode/device prompts for trusting the developer. Register the device
   in the app using the same `LC2.…` connection code as your computer.

A Personal Team can be used for local device development. Its provisioning expires;
rebuild with Xcode when required. No App Groups, push notifications, multicast
entitlement, background mode or App Store upload is required by this app.
The installer needs the Qt iOS kit and builds from the included source; it does not
re-sign someone else's prebuilt application.

For an already configured Xcode account, the script can sign, install and launch:

```sh
export IOS_TEAM_ID=YOUR_TEAM_ID
export IOS_BUNDLE_ID=com.yourname.LocalClipboard
export IOS_DEVICE_ID=YOUR_DEVICE_ID # see: xcrun devicectl list devices
./install.sh
```

`LocalClipboard-ios-package.zip` contains the arm64 device `.app`. **CI's package
is unsigned and cannot be installed directly.** A build made with `IOS_TEAM_ID`
contains your development-signed app; its provisioning must cover the target phone.
Use the installer/source project for signing with another account. This is a local
Xcode installation flow, not an App Store IPA or a standalone iPhone installer.

## Connect and use

- On the computer, open **Network settings** and note its Wi-Fi/LAN IPv4 address.
- On the phone, enter that address in **Network settings**. Allow **Local Network**
  access when iOS asks (or enable it in Settings → Apps → LocalClipboard).
- Keep both apps open on the same LAN. The computer must use the updated version
  that replies to authenticated direct discovery. Your personal profile still
  controls membership; knowing an IP never grants clipboard access.
- **Send current clipboard** sends text / images explicitly. iOS may ask for paste
  permission. The app never polls the clipboard or reads it automatically on launch.
- **Send files / photos** uses Apple's Files or Photos picker. Up to 32 files,
  32 MiB combined; iCloud content may need downloading first.
- From Files or another app that offers **Open in / Copy to LocalClipboard**, send
  a document to LocalClipboard. Available actions depend on the sending app. This
  uses iOS document opening; there is no separate Share extension for arbitrary
  text selections. Use the clipboard button for text.
- Imported files are queued for the current personal profile and sent when the app
  is active and a registered peer is discovered. A different profile never sends
  the old queue. Once a send starts, its queue entry is removed; delivery is not
  acknowledged by the current protocol. Local imported copies remain available
  for retry in **Open received files → Send selected files again**.
- Received images and files are saved locally; **Open received files** opens a
  browser with **Share / Save** for exporting to Files, Photos or another app.
  Clipboard file support depends on the receiving app. Local copies can be deleted
  from this browser. The configuration / connection secret is never exposed there.

## iOS limits

Ordinary iOS apps cannot run as a permanent LAN daemon, start at boot, or monitor
other apps' clipboard in the background. Reception runs while LocalClipboard is
active. Leaving the app closes its sockets; returning starts discovery again.
No audio/location/VPN background-mode workaround is used. Open the phone app before
copying on the computer; copies made while it is suspended are not replayed later.

Direct IPv4 discovery avoids the restricted multicast/broadcast entitlement.
Wi-Fi client isolation, firewalls and VPN routing can still block communication.
If DHCP changes the computer's IP, update the saved address (or reserve it on the router).

Device validation: first-run registration and paste/network permissions; send text,
PNG, photo, empty file and multi-file batches in both directions; verify another
personal profile is invisible; suspend/resume; retry an imported file after an
interrupted transfer; install a new build over the old one and check profile retention.

## Language and uninstall

Choose Русский or English in the installer script or in the app's registration
wizard. The app also has a language picker for later changes.
`LC_LANGUAGE=ru ./install.sh` selects Russian without a terminal prompt.

Run the same `install.sh` again to manage an installed app: choose reinstall,
uninstall or cancel. Removal requires `DELETE` and deletes this device's local
app data. Reinstall preserves the profile. The OS's Delete App action also works.
Set `IOS_DEVICE_ID` and your actual `IOS_BUNDLE_ID` to enable installation detection
and removal. Removal needs Xcode devicectl but does not need Qt or a new build.
Without a device ID installation proceeds through Xcode.


## QR connection

On your registered device, open **Connect device…**. In the phone's registration
wizard, tap **Scan QR code**, allow camera access, scan and confirm **Register**.
You can also scan a QR shown on this phone from another device. Use **Read QR from
image…** if a camera is unavailable. Keep the QR private: it grants access to your
personal profile. For an already registered phone, open **Change profile…** first.

---

## Русская версия

### LocalClipboard для вашего iPhone / iPad

Нужны iOS 17 или новее, Mac с полным Xcode и установленным iOS SDK, а также комплект Qt 6 для iOS. Одних Xcode Command Line Tools и Qt для компьютера из Homebrew недостаточно.

### Установка через свой Xcode

1. Установите совпадающие версии комплектов Qt для iOS и macOS с инструментами сборки. В CI используется Qt 6.11.2.
2. Подключите iPhone, включите режим разработчика и при запросе подтвердите доверие к Mac.
3. Распакуйте `LocalClipboard-ios-installer.zip` и выполните:

   ```sh
   export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
   export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
   ./install.sh
   ```

4. Скрипт собирает неподписанное приложение и открывает проект Xcode. Добавьте учётную запись Apple в Xcode Settings → Accounts. Выберите цель приложения **LocalClipboard**, свою **Team**, включите автоматическую подпись и укажите уникальный Bundle Identifier, например `com.yourname.LocalClipboard`.
5. Выберите подключённый iPhone или iPad как устройство запуска и нажмите **Run**. Следуйте запросам Xcode и устройства для подтверждения доверия разработчику. Зарегистрируйте устройство в приложении с тем же кодом подключения `LC2.…`, который используется на компьютере.

Для локальной разработки на своём устройстве подходит Personal Team. Её профиль подготовки имеет срок действия; при необходимости пересоберите приложение в Xcode. Приложение не требует App Groups, push-уведомлений, разрешения multicast, фонового режима или загрузки в App Store. Установщику нужен комплект Qt для iOS; он собирает приложение из включённых исходников, а не переподписывает чужую готовую сборку.

Если учётная запись Xcode уже настроена, скрипт может подписать, установить и запустить приложение:

```sh
export IOS_TEAM_ID=YOUR_TEAM_ID
export IOS_BUNDLE_ID=com.yourname.LocalClipboard
export IOS_DEVICE_ID=YOUR_DEVICE_ID # См.: xcrun devicectl list devices
./install.sh
```

`LocalClipboard-ios-package.zip` содержит `.app` для устройства arm64. **Пакет CI не подписан и не устанавливается напрямую.** Сборка с `IOS_TEAM_ID` содержит приложение с вашей подписью разработки; профиль подготовки должен включать нужный телефон. Для подписи другой учётной записью используйте установщик или проект с исходниками. Это локальная установка через Xcode, а не IPA для App Store и не автономный установщик на iPhone.

### Подключение и использование

- На компьютере откройте настройки сети и определите его IPv4-адрес в Wi-Fi или локальной сети.
- На телефоне введите этот адрес в настройках сети. При запросе iOS разрешите доступ к **«Локальной сети»** либо включите его в Settings → Apps → LocalClipboard.
- Оставляйте оба приложения открытыми в одной локальной сети. На компьютере нужна обновлённая версия, отвечающая на аутентифицированные прямые запросы обнаружения. Доступ по-прежнему определяется личным профилем: знание IP-адреса не даёт доступа к буферу.
- Кнопка отправки текущего буфера явно отправляет текст и изображения. iOS может запросить разрешение на вставку. Приложение не опрашивает буфер и не читает его автоматически при запуске.
- Отправка файлов и фотографий использует системные окна Apple «Файлы» и «Фото». Поддерживается до 32 файлов общим объёмом до 32 МиБ; содержимое iCloud может потребоваться предварительно скачать.
- Из «Файлов» или другого приложения с действием **«Открыть в / Скопировать в LocalClipboard»** можно передать документ. Доступные действия зависят от отправляющего приложения. Используется механизм открытия документов iOS; отдельного расширения «Поделиться» для произвольного выделенного текста нет. Для текста используйте кнопку отправки буфера.
- Импортированные файлы попадают в очередь текущего личного профиля и отправляются, когда приложение активно и обнаружено зарегистрированное устройство. Другой профиль не отправляет старую очередь. После начала отправки запись удаляется из очереди; текущий протокол не подтверждает доставку. Локальные импортированные копии остаются доступны для повторной отправки через просмотр полученных файлов и действие повторной отправки выбранных файлов.
- Полученные изображения и файлы сохраняются локально. Просмотр полученных файлов открывает список с действиями отправки через системное меню и сохранения в «Файлы», «Фото» или другое приложение. Поддержка файлов из буфера зависит от принимающего приложения. Локальные копии можно удалить из этого списка. Конфигурация и секрет подключения никогда не показываются в нём.

### Ограничения iOS

Обычное приложение iOS не может работать как постоянный демон локальной сети, запускаться при загрузке или отслеживать буфер других приложений в фоне. Приём работает, пока LocalClipboard активно. При выходе из приложения его сокеты закрываются; после возвращения обнаружение запускается снова. Обходы через фоновые режимы аудио, геолокации или VPN не используются. Откройте приложение на телефоне до копирования на компьютере: содержимое, скопированное во время приостановки, позже не повторяется.

Прямое обнаружение IPv4 позволяет обойтись без ограниченного разрешения на multicast и широковещательную передачу. Изоляция клиентов Wi-Fi, брандмауэры и маршрутизация VPN всё равно могут блокировать связь. Если DHCP изменил IP компьютера, обновите сохранённый адрес или закрепите его на роутере.

Для проверки на устройстве: пройдите первую регистрацию и запросы разрешений на вставку и сеть; отправьте текст, PNG, фото, пустой файл и несколько файлов в обоих направлениях; убедитесь, что другой личный профиль невидим; проверьте приостановку и возвращение в приложение, повторную отправку импортированного файла после прерывания передачи, установку новой сборки поверх старой и сохранение профиля.

### Язык и удаление

Выберите «Русский» или English в скрипте установщика или мастере регистрации приложения. В приложении также можно изменить язык позже. Команда `LC_LANGUAGE=ru ./install.sh` выбирает русский без запроса в терминале.

Для управления установленным приложением снова запустите тот же `install.sh`: выберите переустановку, удаление или отмену. Удаление требует ввода `DELETE` и стирает локальные данные приложения на этом устройстве. Переустановка сохраняет профиль. Штатное удаление приложения средствами ОС также работает. Задайте `IOS_DEVICE_ID` и фактический `IOS_BUNDLE_ID`, чтобы включить обнаружение установленного приложения и удаление. Для удаления нужен devicectl из Xcode, но не нужны Qt или новая сборка. Без идентификатора устройства установка продолжается через Xcode.

### Подключение по QR-коду

На зарегистрированном устройстве откройте **«Подключить устройство…»**. В мастере регистрации телефона нажмите **«Сканировать QR-код»**, разрешите доступ к камере, отсканируйте код и подтвердите регистрацию. QR можно показать и на телефоне, чтобы считать его с другого устройства. Если камера недоступна, выберите **«Считать QR из изображения…»**. Не передавайте QR посторонним: он даёт доступ к личному профилю. На уже зарегистрированном телефоне сначала откройте **«Сменить профиль…»**.
