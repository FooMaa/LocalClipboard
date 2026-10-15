Install on the phone directly by opening LocalClipboard.apk and permitting installation
from your file manager, or use install.sh / install.ps1 on a computer with Android
platform-tools (adb) and an authorized USB-debugging connection. An optional device
serial selects a phone when several are connected. No phone root is required.

The scripts install the APK and open device registration. Create a personal profile
or enter your connection code. Permit notifications to see incoming items.
Background reception starts after registration; restart autostart is enabled by
default and can be changed in the app. Android 10+ requires the app to have focus
to read the clipboard: use Send current clipboard or Share → LocalClipboard.

## Language and uninstall

Choose Русский or English in the installer script or in the app's registration
wizard. The app also has a language picker for later changes.
`LC_LANGUAGE=ru ./install.sh` selects Russian without a terminal prompt.

Run the same `install.sh` again to manage an installed app: choose reinstall,
uninstall or cancel. Removal requires `DELETE` and deletes this device's local
app data. Reinstall preserves the profile. The OS's Delete App action also works.
Use `-Serial` / `-Language ru` / `-Action uninstall` with `install.ps1` on Windows.

Updates require the **same signing key** as the installed APK. The scripts explain
signature conflicts and version downgrades without deleting your data. If the old
key has been lost, first save your connection code from **Connect device…**; then
explicitly choose uninstall, install the new APK and reconnect to that profile.
Uninstalling deletes local received files as well as the phone's registration.

## Incoming notifications and responsiveness

Incoming items have a separate high-importance notification channel,
**Received clipboard**. Permit notifications and enable pop-up notifications for
this channel in Android's app settings (on Samsung: notification categories).
The foreground service's permanent status notification remains quiet. Android's
Do Not Disturb mode and your notification settings still control banners/sound.
Received file decoding, file reads and outgoing encoding run on workers; the
interface does not rebuild the device list when nothing has changed.


## QR connection

On your registered device, open **Connect device…**. In the phone's registration
wizard, tap **Scan QR code**, allow camera access, scan and confirm **Register**.
You can also scan a QR shown on this phone from another device. Use **Read QR from
image…** if a camera is unavailable. Keep the QR private: it grants access to your
personal profile. For an already registered phone, open **Change profile…** first.
Android uses its native Camera2 preview and a background QR decoder. The scanner
reopens the camera after returning from the image picker or system settings.
Camera access is released when the scanner leaves the foreground; no photos are
recorded by live scanning. Choice fields use native Android radio-button dialogs.

---

## Русская версия

### Установка на Android

Для установки непосредственно на телефоне откройте `LocalClipboard.apk` и разрешите установку из файлового менеджера. Также можно использовать `install.sh` или `install.ps1` на компьютере с Android platform-tools (adb) и подключённым по USB телефоном, который разрешил отладку. Необязательный серийный номер устройства позволяет выбрать телефон, если подключено несколько. Root на телефоне не нужен.

Скрипты устанавливают APK и открывают регистрацию устройства. Создайте личный профиль или введите код подключения. Разрешите уведомления, чтобы видеть входящее содержимое. Фоновый приём начинается после регистрации; автозапуск после перезагрузки включён по умолчанию и меняется в приложении. На Android 10+ для чтения буфера приложение должно быть в фокусе: отправляйте текущий буфер из открытого приложения либо используйте **«Поделиться → LocalClipboard»**.

### Язык и удаление

Выберите «Русский» или English в скрипте установщика или мастере регистрации приложения. В приложении также можно изменить язык позже. Команда `LC_LANGUAGE=ru ./install.sh` выбирает русский без запроса в терминале.

Для управления установленным приложением снова запустите тот же `install.sh`: выберите переустановку, удаление или отмену. Удаление требует ввода `DELETE` и стирает локальные данные приложения на этом устройстве. Переустановка сохраняет профиль. Штатное удаление приложения средствами ОС также работает. На Windows используйте параметры `-Serial`, `-Language ru` и `-Action uninstall` со скриптом `install.ps1`.

Для обновления нужен **тот же ключ подписи**, что у установленного APK. Скрипты объясняют конфликт подписей или попытку установить более старую версию, сохраняя данные. Если прежний ключ потерян, сначала сохраните код подключения через **«Подключить устройство…»**, затем явно выберите удаление, установите новый APK и подключитесь к прежнему профилю. При удалении стираются и полученные локальные файлы, и регистрация телефона.

### Уведомления и отзывчивость

Для входящих элементов есть отдельный канал высокой важности **«Получение буфера»**. Разрешите уведомления и всплывающие уведомления этого канала в системных настройках приложения; на Samsung это категории уведомлений. Постоянное уведомление фоновой службы остаётся тихим. Режим «Не беспокоить» и системные настройки продолжают управлять баннерами и звуком. Декодирование полученных файлов, чтение файлов и подготовка отправки выполняются в рабочих потоках; список устройств не пересоздаётся, если он не изменился.

### Подключение по QR-коду

На зарегистрированном устройстве откройте **«Подключить устройство…»**. В мастере регистрации телефона нажмите **«Сканировать QR-код»**, разрешите доступ к камере, отсканируйте код и подтвердите регистрацию. QR можно показать и на телефоне, чтобы считать его с другого устройства. Если камера недоступна, выберите **«Считать QR из изображения…»**. Не передавайте QR посторонним: он даёт доступ к личному профилю. На уже зарегистрированном телефоне сначала откройте **«Сменить профиль…»**.

На Android используется нативный предпросмотр Camera2 и распознавание QR в рабочем потоке. Камера открывается снова после возвращения из выбора изображения или системных настроек и освобождается при уходе сканера в фон. При сканировании в реальном времени фотографии не сохраняются. Варианты в полях выбираются через системные диалоги Android с переключателями.
