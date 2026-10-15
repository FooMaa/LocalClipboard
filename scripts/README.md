# Build and package

Run from any directory; `-b` paths are relative to the caller. Native builds need
the corresponding host. The five-platform GitHub Actions workflow supplies these
hosts and uploads a combined distribution with five packages and five installers.
The iOS CI application is unsigned and must be signed through your Xcode account.

```sh
./scripts/build -t linux -c Release -j 4
./scripts/build -t macos
./scripts/build -t windows       # Git Bash in an MSVC environment
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/build -t ios
```

Packaging is included in the build. Default directories are `build_TARGET`;
`desktop` selects the host OS and is the default target. `-n` skips packaging.
Additional CMake options follow `--`, e.g. `-- -DCMAKE_PREFIX_PATH=/path/to/Qt`.
`BUILD_TESTING` defaults on for desktop; Qt Test is required unless disabled.
Install the Qt Multimedia module in every target kit for QR camera scanning.
ZXing-C++ is built from the included sources without network downloads.

To package an existing build without recompiling:

```sh
./scripts/package -t native -b build_desktop -c Release
./scripts/package -t android -b build_android
cmake --build build_desktop --config Release --target installers
```

All outputs go into `dist/`. `deb`, `sh` and `apk` remain aliases for Linux/Android
packaging; Linux always emits both its DEB and its shell installer. Windows needs
`windeployqt` and NSIS (`makensis` or `NSIS_EXE`). Windows uses `VCToolsRedistDir`
from the MSVC developer environment to include release CRT DLLs locally; Debug
builds should use `-n` because debug CRTs are not redistributable.
macOS needs `macdeployqt`, Python 3, `pkgbuild` and `productbuild`; packaging verifies bundle signatures and rejects
unresolved / externally installed Qt dependencies. Linux needs `dpkg-dev` and
runtime dependency metadata. Its installer uses apt to install the DEB and required
Qt runtimes, then opens registration as the desktop user; do not invoke it as root.

The macOS PKG deploys into `/Applications` and opens registration in the logged-in
user's session. Windows installs into the current user's local Programs directory
and offers to open registration on the finish page. Startup is configured by the
registration wizard under the actual desktop user's account, never under the
package builder or system installer account.

Android packaging accepts exactly one installable signed APK from a clean build
configuration/ABI. Debug APKs are signed by Qt/Gradle for testing. Release builds
must be signed using Qt's Android signing configuration before packaging. Keep a
stable signing key to allow upgrades. The installer ZIP contains the APK and shell /
PowerShell scripts; it needs adb and USB debugging. The APK itself installs directly
on a phone without adb.

For Apple distribution, set `MACOS_APP_SIGN_IDENTITY` and
`MACOS_INSTALLER_SIGN_IDENTITY`; notarize the resulting artifacts separately. The
local default uses ad-hoc app signing and an unsigned PKG. See
[dependencies](../dependencies/README.md) for the Android SDK/NDK setup.

## Android release signing

CI builds development APKs. For a release APK, set `LC_ANDROID_KEYSTORE`,
`LC_ANDROID_KEY_ALIAS`, `LC_ANDROID_STORE_PASSWORD`, `LC_ANDROID_KEY_PASSWORD`
in a private environment, then run `./scripts/build -t android -c Release`.
Signing uses `apksigner` with passwords read from environment references, and
verifies signature and ZIP alignment before packaging. Without those variables,
`-n` can build an unsigned Release; packaging requires a signed APK.
Keep the same signing key for updates. See [release verification](../SECURITY.md).

## iOS / iPadOS (local Xcode installation)

`./scripts/package-ios-installer` assembles the source installer ZIP without an
iOS SDK. The regular `build -t ios` automatically emits both the app and installer.

Requires full Xcode with the iPhoneOS SDK and a Qt iOS kit, iOS 17+:

```sh
export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
./scripts/build -t ios
```

This generates `build_ios/LocalClipboard.xcodeproj`, compiles an unsigned arm64
app, and emits the app ZIP plus the source installer ZIP. Open the generated
project in Xcode, select your Team and a unique Bundle Identifier on the
LocalClipboard app target, choose your phone and Run. The unsigned override is
used only by the CLI build, so it does not disable signing in the Xcode project.

Set `IOS_TEAM_ID` and `IOS_BUNDLE_ID` before building for automatic development
signing through an account already configured in Xcode. The installer additionally
accepts `IOS_DEVICE_ID` to install/launch with `xcrun devicectl`. Otherwise it opens
Xcode for interactive signing and Run. The installer contains complete build source,
not a machine-specific generated Xcode project or another person's provisioning.
See [iOS installation](../packaging/ios/README.md) for permissions, pairing and limits.

## Installer languages and uninstallers

Both languages are compiled into the application without Qt Linguist or separate
translation downloads. Every registration form offers Русский / English and saves
the preference for later launches. NSIS explicitly offers both languages and
passes the choice to the app. Shell installers accept `LC_LANGUAGE=ru|en` or
prompt in a terminal. The macOS PKG includes `en.lproj` / `ru.lproj` welcome pages;
its first-run registration wizard offers the explicit language choice.

The same installer handles reinstall and removal. macOS uses mutually exclusive
PKG choices: choosing Remove deselects the application payload and runs a guarded
script-only removal component. Removal is disabled without an installed bundle.
The ZIP contains only the main app. Legacy separate removal launchers are cleaned
up during removal. The PKG keeps user data and removes the current user's launchd entry.
NSIS detects the recorded Windows installation directory and delegates removal to
its installed uninstaller, including the optional data-erasure dialog. Linux checks
DEB status before offering reinstall/remove/cancel. Android queries the selected
ADB device; iOS queries installed apps with devicectl when IOS_DEVICE_ID is provided.
Phone removal requires DELETE confirmation. Shell scripts accept LC_ACTION for
explicit action selection; Android PowerShell accepts -Action. Reinstall keeps data.
All of these are included by the existing `installers` target; output counts stay
at five packages and five installers.

`python3 tests/PackagingTests.py` checks device arguments, confirmation and macOS
removal-path guards without removing an installation or contacting a device.
Qt app tests cover language persistence, invalid registration, layouts at 360 / 520
pixels with normal / larger text, instance quit IPC and background synchronization.

---

## Русская версия

### Сборка и упаковка

Скрипты можно запускать из любого каталога; пути `-b` отсчитываются от каталога запуска. Для нативной сборки нужна соответствующая ОС. GitHub Actions предоставляет машины для всех пяти платформ и публикует общий архив с пятью пакетами и пятью установщиками. Приложение iOS из CI не подписано: его нужно подписать через свою учётную запись Xcode.

```sh
./scripts/build -t linux -c Release -j 4
./scripts/build -t macos
./scripts/build -t windows       # Git Bash в окружении MSVC
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/build -t ios
```

Упаковка входит в процесс сборки. Каталоги по умолчанию называются `build_TARGET`; цель `desktop` выбирает ОС компьютера и используется по умолчанию. Параметр `-n` пропускает упаковку. Дополнительные параметры CMake указываются после `--`, например `-- -DCMAKE_PREFIX_PATH=/path/to/Qt`. Для компьютеров `BUILD_TESTING` включён по умолчанию; Qt Test нужен, если тесты не отключены.

Чтобы упаковать уже собранное приложение без повторной компиляции:

```sh
./scripts/package -t native -b build_desktop -c Release
./scripts/package -t android -b build_android
cmake --build build_desktop --config Release --target installers
```

Все результаты сохраняются в `dist/`. Значения `deb`, `sh` и `apk` остаются псевдонимами упаковки Linux и Android; Linux всегда создаёт DEB и установщик для командной строки. Для Windows нужны `windeployqt` и NSIS: `makensis` или `NSIS_EXE`. Windows использует `VCToolsRedistDir` из окружения разработчика MSVC, чтобы включить выпускные DLL среды CRT рядом с приложением. Для отладочных сборок используйте `-n`, поскольку отладочные CRT нельзя распространять.

Для macOS нужны `macdeployqt`, Python 3, `pkgbuild` и `productbuild`. При упаковке проверяются подписи пакетов приложения; неразрешённые зависимости Qt и ссылки на внешнюю установку Qt отклоняются. Linux требует `dpkg-dev` и сведения о зависимостях времени выполнения. Его установщик использует apt для установки DEB и нужных библиотек Qt, затем открывает регистрацию от имени пользователя графического сеанса. Не запускайте его от root.

PKG для macOS устанавливает приложение в `/Applications` и открывает регистрацию в сеансе вошедшего пользователя. Windows устанавливает приложение в локальный каталог Programs текущего пользователя и предлагает открыть регистрацию на последней странице. Автозапуск настраивается мастером регистрации от имени реального пользователя компьютера, а не пользователя сборки или системного установщика.

Упаковка Android принимает ровно один устанавливаемый подписанный APK из чистой сборки с одной конфигурацией и ABI. Qt/Gradle подписывают отладочные APK для тестирования. Выпускную сборку перед упаковкой нужно подписать с помощью настроек подписи Qt для Android. Сохраняйте постоянный ключ подписи для последующих обновлений. ZIP установщика содержит APK и скрипты для командной строки и PowerShell; ему нужны adb и отладка USB. Сам APK можно установить на телефоне напрямую, без adb.

Для распространения через инфраструктуру Apple задайте `MACOS_APP_SIGN_IDENTITY` и `MACOS_INSTALLER_SIGN_IDENTITY`; нотариализуйте полученные артефакты отдельно. По умолчанию для локального использования приложение подписывается ad-hoc, а PKG остаётся неподписанным. Настройка Android SDK/NDK описана в [зависимостях](../dependencies/README.md#русская-версия).

### Подпись выпускной сборки Android

CI собирает отладочные APK. Для выпуска задайте в приватном окружении
`LC_ANDROID_KEYSTORE`, `LC_ANDROID_KEY_ALIAS`, `LC_ANDROID_STORE_PASSWORD` и
`LC_ANDROID_KEY_PASSWORD`, затем выполните `./scripts/build -t android -c Release`.
Подпись выполняет apksigner с чтением паролей через окружение; перед упаковкой
проверяются подпись и выравнивание ZIP. Без этих переменных `-n` может собрать
неподписанный Release, но для упаковки нужен подписанный APK. Для обновлений
сохраняйте один ключ. См. [проверки выпуска](../SECURITY.md#русская-версия).

### iOS / iPadOS: локальная установка через Xcode

Команда `./scripts/package-ios-installer` собирает ZIP установщика с исходниками без iOS SDK. Обычная команда `build -t ios` автоматически создаёт приложение и установщик.

Нужны полный Xcode с iPhoneOS SDK, комплект Qt для iOS и устройство с iOS 17 или новее:

```sh
export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
./scripts/build -t ios
```

Команда создаёт `build_ios/LocalClipboard.xcodeproj`, компилирует неподписанное приложение arm64 и выпускает ZIP приложения и ZIP установщика с исходниками. Откройте созданный проект в Xcode, выберите свою Team и уникальный Bundle Identifier для цели приложения LocalClipboard, затем выберите телефон и нажмите Run. Отключение подписи применяется только к сборке из командной строки и не отключает подпись в проекте Xcode.

Для автоматической подписи разработки через уже настроенную учётную запись Xcode перед сборкой задайте `IOS_TEAM_ID` и `IOS_BUNDLE_ID`. Установщик также принимает `IOS_DEVICE_ID` для установки и запуска через `xcrun devicectl`; иначе он открывает Xcode для интерактивной настройки подписи и запуска. Установщик содержит полные исходники, а не созданный для чужого компьютера проект Xcode или чужие профили подготовки. Разрешения, подключение устройств и ограничения описаны в [инструкции iOS](../packaging/ios/README.md#русская-версия).

### Языки установщиков и удаление

Оба языка включены в приложение при компиляции, без Qt Linguist и отдельной загрузки переводов. Каждая форма регистрации предлагает «Русский / English» и сохраняет выбор для следующих запусков. NSIS явно предлагает оба языка и передаёт выбор приложению. Установщики для командной строки принимают `LC_LANGUAGE=ru|en` или предлагают выбрать язык в терминале. PKG для macOS включает страницы приветствия `en.lproj` и `ru.lproj`; мастер регистрации при первом запуске предлагает явный выбор языка.

Тот же установщик выполняет переустановку и удаление. На macOS действия в PKG взаимоисключающие: выбор удаления отключает установку файлов приложения и запускает защищённый проверками компонент удаления, содержащий только скрипт. Если приложение не установлено, удаление недоступно. ZIP содержит только основное приложение. При удалении также убираются отдельные приложения удаления из старых сборок. PKG сохраняет пользовательские данные и удаляет запись launchd текущего пользователя.

NSIS обнаруживает сохранённый каталог установки Windows и передаёт удаление установленному деинсталлятору, включая диалог необязательного стирания данных. Linux проверяет состояние DEB перед предложением переустановки, удаления или отмены. Android проверяет выбранное устройство через adb; iOS запрашивает список приложений через devicectl, если задан `IOS_DEVICE_ID`. Удаление на телефоне требует подтверждения `DELETE`. Скрипты для командной строки принимают `LC_ACTION` для явного выбора действия; PowerShell для Android принимает `-Action`. Переустановка сохраняет данные. Всё это включено в существующую цель `installers`: число результатов остаётся равным пяти пакетам и пяти установщикам.

Команда `python3 tests/PackagingTests.py` проверяет параметры устройств, подтверждение и защиту путей удаления macOS, не удаляя реальную установку и не обращаясь к устройству. Тесты Qt проверяют сохранение языка, ошибочную регистрацию, размещение элементов на экранах шириной 360 и 520 пикселей с обычным и увеличенным текстом, команду завершения экземпляра через IPC и фоновую синхронизацию.

Для сканирования QR камерой установите Qt Multimedia в каждом комплекте Qt для целевой платформы. ZXing-C++ собирается из включённых исходников без загрузок из сети.
