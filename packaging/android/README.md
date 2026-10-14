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


## QR connection

On your registered device, open **Connect device…**. In the phone's registration
wizard, tap **Scan QR code**, allow camera access, scan and confirm **Register**.
You can also scan a QR shown on this phone from another device. Use **Read QR from
image…** if a camera is unavailable. Keep the QR private: it grants access to your
personal profile. For an already registered phone, open **Change profile…** first.

---

## Русская версия

### Установка на Android

Для установки непосредственно на телефоне откройте `LocalClipboard.apk` и разрешите установку из файлового менеджера. Также можно использовать `install.sh` или `install.ps1` на компьютере с Android platform-tools (adb) и подключённым по USB телефоном, который разрешил отладку. Необязательный серийный номер устройства позволяет выбрать телефон, если подключено несколько. Root на телефоне не нужен.

Скрипты устанавливают APK и открывают регистрацию устройства. Создайте личный профиль или введите код подключения. Разрешите уведомления, чтобы видеть входящее содержимое. Фоновый приём начинается после регистрации; автозапуск после перезагрузки включён по умолчанию и меняется в приложении. На Android 10+ для чтения буфера приложение должно быть в фокусе: отправляйте текущий буфер из открытого приложения либо используйте **«Поделиться → LocalClipboard»**.

### Язык и удаление

Выберите «Русский» или English в скрипте установщика или мастере регистрации приложения. В приложении также можно изменить язык позже. Команда `LC_LANGUAGE=ru ./install.sh` выбирает русский без запроса в терминале.

Для управления установленным приложением снова запустите тот же `install.sh`: выберите переустановку, удаление или отмену. Удаление требует ввода `DELETE` и стирает локальные данные приложения на этом устройстве. Переустановка сохраняет профиль. Штатное удаление приложения средствами ОС также работает. На Windows используйте параметры `-Serial`, `-Language ru` и `-Action uninstall` со скриптом `install.ps1`.

### Подключение по QR-коду

На зарегистрированном устройстве откройте **«Подключить устройство…»**. В мастере регистрации телефона нажмите **«Сканировать QR-код»**, разрешите доступ к камере, отсканируйте код и подтвердите регистрацию. QR можно показать и на телефоне, чтобы считать его с другого устройства. Если камера недоступна, выберите **«Считать QR из изображения…»**. Не передавайте QR посторонним: он даёт доступ к личному профилю. На уже зарегистрированном телефоне сначала откройте **«Сменить профиль…»**.
