# Build dependencies

Desktop Linux (Debian/Ubuntu): `./dependencies/linux.sh`.
Desktop macOS: install Homebrew and Xcode Command Line Tools, then
`./dependencies/macos.sh`. These scripts install packages and may require sudo.

Install Qt Multimedia for desktop and iOS kits. Android uses the native Camera2
scanner and does not ship Qt Multimedia or FFmpeg.
For Qt 6.11 kits, also install Qt TaskTree (`qttasktree`): the bundled QML asset
downloader depends on it. Include it in both the target and desktop host kits,
including the macOS kit used to test the Android interface preview.
QR encoding/decoding uses the bundled ZXing-C++ sources; no QR library download is needed.
Qt Quick / QuickControls2 are required on every platform. macOS installer packaging also
requires Swift (Command Line Tools or Xcode) and creates a DMG with a bilingual maintenance wizard.

Other Linux distributions: C++17 compiler, CMake >=3.21.1, Ninja (optional),
Qt 6 Core, Network, Widgets, Concurrent, Multimedia, Quick, QuickControls2, Test and platform plugins. Windows: MSVC, Qt 6 desktop
and NSIS (e.g. `choco install nsis`). Linux packaging uses `dpkg-dev`; macOS uses
`macdeployqt`, Python 3, Swift, `pkgbuild` and `hdiutil` from the development tools.

Android on Linux/macOS additionally requires JDK 21, Android SDK command-line tools,
platform/build tools and an NDK compatible with your Qt version. Install matching
Qt desktop (host tools) and Qt Android kits through the Qt installer.
For the pinned CI Qt 6.11.2 toolchain: SDK 36, build-tools 36.0.0, NDK 27.2.12479018.
Desktop package-manager Qt and Android Qt must have matching versions when used together.

```sh
sdkmanager 'platform-tools' 'platforms;android-36' 'build-tools;36.0.0' 'ndk;27.2.12479018'
export QT_ANDROID_ROOT=/path/to/Qt/6.11.2/android_arm64_v8a
export QT_HOST_PATH=/path/to/Qt/6.11.2/gcc_64 # macOS: macos
export ANDROID_SDK_ROOT=/path/to/Android/sdk
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
./scripts/build -t android
```

SDK licenses must be accepted with `sdkmanager --licenses`.
Android dependencies are documented separately because apt/Homebrew desktop Qt does
not include the Android kit. The installers do not accept licenses automatically.

## Android — Qt 6

All platforms use the Qt Quick interface in `qml/`, the C++ controller/session in `src/`
and the shared protocol core. Install Qt Declarative (Quick, QuickControls2) in both
kits. The root CMake project generates the Gradle project and APK configuration.
The `android/` directory supplies a custom manifest, QtActivity subclass for Share
intents and FileProvider clipboard URIs, and a separate QtService with reboot
registration. The target is SDK 36 with minimum SDK 28; CMake uses this package source.

After installing the dependencies above, run:

```sh
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t apk
```

Register the phone with your personal profile and permit notifications. The service
receives text/images/files in the background; tap **Copy** in its notification or
**Copy** on the desired item on **Home** to apply it. Opening the app alone does not copy it. Android 10+ restricts reading the clipboard to the focused app, so tap
**Settings → Send from clipboard** while open or use **Share → LocalClipboard** from another
app. Release APKs require signing; CI builds an installable debug APK and an adb
installer ZIP. Device testing is required to verify clipboard, LAN, reboot and
manufacturer battery-management behavior on your Android version.

### Android development signing

Android only accepts an update signed with the installed application's key.
CI restores `~/.android/debug.keystore` from a dedicated development-key cache,
rather than generating a different signature on every runner. Caches can expire
and branches may have different cache visibility. For a durable key, generate
one locally (or reuse your existing development keystore):

```sh
keytool -genkeypair -keystore /private/path/debug.keystore -storepass android -keypass android \
  -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
  -dname 'CN=LocalClipboard Development,O=LocalClipboard,C=US'
```

Put the base64 encoding of that file into the repository Actions secret
`ANDROID_DEBUG_KEYSTORE_BASE64`. It must contain alias `androiddebugkey`, with
store/key passwords `android`, matching Gradle's development signing configuration.
The secret takes precedence over the cache and is never saved into that cache.
Keep a private backup of the key; do not commit it. Use a separate release key
and release signing configuration for public distribution. Fork pull requests
cannot access repository secrets, so their development APKs may use another key.
If an earlier CI key has already been lost, it cannot sign an in-place update:
save the connection code, uninstall once explicitly, then install and rejoin.

## iOS / iPadOS

Install **full Xcode**, its iOS SDK, and matching Qt iOS / macOS host kits. The
Homebrew desktop Qt package and Xcode Command Line Tools cannot build for iPhone.
Use `xcode-select -p` and `xcrun --sdk iphoneos --show-sdk-path` to verify Xcode setup.
The supported deployment target here is iOS 17+; CI uses Qt 6.11.2 and the runner's Xcode.
The Qt iOS kit must include its native AVFoundation multimedia plugin. CMake selects
only that media backend for QR camera capture, so FFmpeg libraries are not required.

```sh
export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
./scripts/build -t ios
```

Then open `build_ios/LocalClipboard.xcodeproj`, select your Team / unique Bundle ID,
and Run on your phone. No App Groups or restricted multicast permission are needed.
See [installation and device verification](../packaging/ios/README.md).

---

## Русская версия

### Зависимости сборки

Для компьютера с Linux (Debian/Ubuntu): `./dependencies/linux.sh`. Для macOS установите Homebrew и Xcode Command Line Tools, затем выполните `./dependencies/macos.sh`. Эти скрипты устанавливают пакеты и могут потребовать sudo.

Установите Qt Multimedia для комплектов компьютеров и iOS. Android использует нативную камеру Camera2 и не включает Qt Multimedia или FFmpeg. Для генерации и распознавания QR используются включённые исходники ZXing-C++; отдельно скачивать библиотеку QR не нужно.

Qt Quick / QuickControls2 нужны для всех платформ. Для упаковки установщика macOS также нужен Swift из Command Line Tools или Xcode; сборка создаёт DMG с единым двуязычным мастером установки и удаления.

Для комплектов Qt 6.11 также установите Qt TaskTree (`qttasktree`): от него зависит включённый в QML загрузчик ресурсов. Модуль нужен в целевом комплекте и комплекте компьютера с инструментами сборки, включая macOS-комплект для проверки предпросмотра интерфейса Android.

Для других дистрибутивов Linux нужны компилятор C++17, CMake 3.21.1 или новее, Ninja при желании, Qt 6 Core, Network, Widgets, Concurrent, Multimedia, Quick, QuickControls2, Test и плагины платформы. Для Windows нужны MSVC, Qt 6 для компьютеров и NSIS, например `choco install nsis`. Упаковка Linux использует `dpkg-dev`; macOS — `macdeployqt`, Python 3, Swift, `pkgbuild` и `hdiutil` из инструментов разработки.

Для Android на Linux или macOS дополнительно нужны JDK 21, инструменты командной строки Android SDK, инструменты платформы и сборки, а также NDK, совместимый с вашей версией Qt. Установите комплекты Qt для компьютера с инструментами сборки и Qt для Android через установщик Qt. Для закреплённого в CI Qt 6.11.2 используются SDK 36, build-tools 36.0.0 и NDK 27.2.12479018. Если Qt из пакетного менеджера компьютера используется вместе с Qt для Android, их версии должны совпадать.

```sh
sdkmanager 'platform-tools' 'platforms;android-36' 'build-tools;36.0.0' 'ndk;27.2.12479018'
export QT_ANDROID_ROOT=/path/to/Qt/6.11.2/android_arm64_v8a
export QT_HOST_PATH=/path/to/Qt/6.11.2/gcc_64 # Для macOS: macos
export ANDROID_SDK_ROOT=/path/to/Android/sdk
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
./scripts/build -t android
```

Лицензии SDK нужно принять через `sdkmanager --licenses`. Зависимости Android описаны отдельно, поскольку Qt для компьютера из apt или Homebrew не включает комплект Android. Установщики не принимают лицензии автоматически.

### Android — Qt 6

Все платформы используют интерфейс Qt Quick из `qml/`, контроллер и сессию C++ из `src/` и общее ядро протокола. В обоих комплектах нужен Qt Declarative (Quick, QuickControls2). Корневой проект CMake создаёт проект Gradle и настройки APK. Каталог `android/` содержит манифест, подкласс QtActivity для «Поделиться» и URI буфера через FileProvider, а также отдельную QtService с запуском после перезагрузки. Целевой SDK — 36, минимальный — 28 (Android 9).

После установки перечисленных зависимостей выполните:

```sh
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t apk
```

Зарегистрируйте телефон в личном профиле и разрешите уведомления. Служба принимает текст, изображения и файлы в фоне; нажмите «Скопировать» в уведомлении или у нужного элемента на «Главной», чтобы поместить его в буфер. Простое открытие приложения не копирует его. Android 10+ позволяет читать буфер только приложению в фокусе, поэтому выберите **«Настройки → Отправить из буфера»** в открытом приложении либо используйте **«Поделиться → LocalClipboard»** из другого приложения. Выпускные APK требуют подписи; CI создаёт устанавливаемый отладочный APK и ZIP установщика через adb. На реальном устройстве нужно проверить буфер, локальную сеть, перезагрузку и управление батареей производителем на вашей версии Android.

### Подпись отладочных сборок Android

Android принимает обновление только с тем же ключом подписи, что у установленной версии. CI восстанавливает `~/.android/debug.keystore` из отдельного кеша отладочного ключа вместо новой подписи на каждом запуске. Кеш может быть удалён, а видимость кешей зависит от ветки. Для постоянной подписи создайте ключ локально или используйте свой существующий отладочный ключ:

```sh
keytool -genkeypair -keystore /private/path/debug.keystore -storepass android -keypass android \
  -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
  -dname 'CN=LocalClipboard Development,O=LocalClipboard,C=US'
```

Сохраните base64-содержимое файла в секрете Actions репозитория `ANDROID_DEBUG_KEYSTORE_BASE64`. Требуются псевдоним `androiddebugkey` и пароли хранилища/ключа `android`, соответствующие конфигурации отладочной подписи Gradle.

Секрет имеет приоритет перед кешем и никогда не сохраняется в этот кеш. Сделайте приватную резервную копию ключа и не коммитьте его. Для публичного распространения используйте отдельный выпускной ключ и конфигурацию подписи. Pull request из форка не имеет доступа к секретам, поэтому его APK может иметь другой ключ. Если прежний ключ CI уже потерян, обновление поверх старой версии невозможно: сохраните код подключения, явно удалите приложение один раз, установите заново и подключитесь к прежнему профилю.

### iOS / iPadOS

Установите **полный Xcode**, его iOS SDK и совпадающие версии комплектов Qt для iOS и macOS с инструментами сборки. Qt для компьютера из Homebrew и Xcode Command Line Tools не позволяют собирать приложение для iPhone. Проверьте настройку Xcode командами `xcode-select -p` и `xcrun --sdk iphoneos --show-sdk-path`. Поддерживается iOS 17 и новее; CI использует Qt 6.11.2 и Xcode машины сборки.

Комплект Qt для iOS должен содержать нативный мультимедийный плагин AVFoundation. CMake выбирает только этот модуль для сканирования QR камерой, поэтому библиотеки FFmpeg не требуются.

```sh
export QT_IOS_ROOT="$HOME/Qt/6.11.2/ios"
export QT_HOST_PATH="$HOME/Qt/6.11.2/macos"
./scripts/build -t ios
```

Затем откройте `build_ios/LocalClipboard.xcodeproj`, выберите свою Team и уникальный Bundle ID, после чего запустите приложение на телефоне через Run. App Groups и ограниченное разрешение на multicast не нужны. См. [установку и проверку на устройстве](../packaging/ios/README.md#русская-версия).
