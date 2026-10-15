# Build dependencies

Desktop Linux (Debian/Ubuntu): `./dependencies/linux.sh`.
Desktop macOS: install Homebrew and Xcode Command Line Tools, then
`./dependencies/macos.sh`. These scripts install packages and may require sudo.

Install the Qt Multimedia module for every target kit, including Android and iOS.
QR encoding/decoding uses the bundled ZXing-C++ sources; no QR library download is needed.

Other Linux distributions: C++17 compiler, CMake >=3.21.1, Ninja (optional),
Qt 6 Core, Network, Widgets, Multimedia, Test and platform plugins. Windows: MSVC, Qt 6 desktop
and NSIS (e.g. `choco install nsis`). Linux packaging uses `dpkg-dev`; macOS uses
`macdeployqt`, Python 3, `pkgbuild` and `productbuild` from the development tools.

Android on Linux/macOS additionally requires JDK 17, Android SDK command-line tools,
platform/build tools and an NDK compatible with your Qt version. Install matching
Qt desktop (host tools) and Qt Android kits through the Qt installer.
For the pinned CI Qt 6.8.3 toolchain: SDK 35, build-tools 35.0.0, NDK 26.1.10909125.
Desktop package-manager Qt and Android Qt must have matching versions when used together.

```sh
sdkmanager 'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' 'ndk;26.1.10909125'
export QT_ANDROID_ROOT=/path/to/Qt/6.8.3/android_arm64_v8a
export QT_HOST_PATH=/path/to/Qt/6.8.3/gcc_64 # macOS: macos
export ANDROID_SDK_ROOT=/path/to/Android/sdk
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/26.1.10909125"
./scripts/build -t android
```

SDK licenses must be accepted with `sdkmanager --licenses`.
Android dependencies are documented separately because apt/Homebrew desktop Qt does
not include the Android kit. The installers do not accept licenses automatically.

## Android — Qt 6

Android uses the shared C++/Qt Widgets application in `src/`, built from the root
CMake project. Qt generates the Gradle project and APK deployment configuration.
The `android/` directory supplies a custom manifest, QtActivity subclass for Share
intents and FileProvider clipboard URIs, and a separate QtService with reboot
registration. The target is SDK 35 with minimum SDK 26; CMake uses this package source.

After installing the dependencies above, run:

```sh
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t apk
```

Register the phone with your personal profile and permit notifications. The service
receives text/images/files in the background; open the notification to apply the
latest item. Android 10+ restricts reading the clipboard to the focused app, so tap
**Send current clipboard** while open or use **Share → LocalClipboard** from another
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
The supported deployment target here is iOS 16+; CI uses Qt 6.8.3 and the runner's Xcode.
The Qt iOS kit must include its native AVFoundation multimedia plugin. CMake selects
only that media backend for QR camera capture, so FFmpeg libraries are not required.

```sh
export QT_IOS_ROOT="$HOME/Qt/6.8.3/ios"
export QT_HOST_PATH="$HOME/Qt/6.8.3/macos"
./scripts/build -t ios
```

Then open `build_ios/LocalClipboard.xcodeproj`, select your Team / unique Bundle ID,
and Run on your phone. No App Groups or restricted multicast permission are needed.
See [installation and device verification](../packaging/ios/README.md).

---

## Русская версия

### Зависимости сборки

Для компьютера с Linux (Debian/Ubuntu): `./dependencies/linux.sh`. Для macOS установите Homebrew и Xcode Command Line Tools, затем выполните `./dependencies/macos.sh`. Эти скрипты устанавливают пакеты и могут потребовать sudo.

Установите модуль Qt Multimedia для каждого используемого комплекта, включая Android и iOS. Для генерации и распознавания QR используются включённые исходники ZXing-C++; отдельно скачивать библиотеку QR не нужно.

Для других дистрибутивов Linux нужны компилятор C++17, CMake 3.21.1 или новее, Ninja при желании, Qt 6 Core, Network, Widgets, Multimedia, Test и плагины платформы. Для Windows нужны MSVC, Qt 6 для компьютеров и NSIS, например `choco install nsis`. Упаковка Linux использует `dpkg-dev`; macOS — `macdeployqt`, Python 3, `pkgbuild` и `productbuild` из инструментов разработки.

Для Android на Linux или macOS дополнительно нужны JDK 17, инструменты командной строки Android SDK, инструменты платформы и сборки, а также NDK, совместимый с вашей версией Qt. Установите комплекты Qt для компьютера с инструментами сборки и Qt для Android через установщик Qt. Для закреплённого в CI Qt 6.8.3 используются SDK 35, build-tools 35.0.0 и NDK 26.1.10909125. Если Qt из пакетного менеджера компьютера используется вместе с Qt для Android, их версии должны совпадать.

```sh
sdkmanager 'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' 'ndk;26.1.10909125'
export QT_ANDROID_ROOT=/path/to/Qt/6.8.3/android_arm64_v8a
export QT_HOST_PATH=/path/to/Qt/6.8.3/gcc_64 # Для macOS: macos
export ANDROID_SDK_ROOT=/path/to/Android/sdk
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/26.1.10909125"
./scripts/build -t android
```

Лицензии SDK нужно принять через `sdkmanager --licenses`. Зависимости Android описаны отдельно, поскольку Qt для компьютера из apt или Homebrew не включает комплект Android. Установщики не принимают лицензии автоматически.

### Android — Qt 6

Android использует общее приложение C++/Qt Widgets из `src/`, которое собирается из корневого проекта CMake. Qt создаёт проект Gradle и настройки развёртывания APK. Каталог `android/` содержит собственный манифест, подкласс QtActivity для обработки действий «Поделиться» и URI буфера через FileProvider, а также отдельную QtService с запуском после перезагрузки. Целевая версия SDK — 35, минимальная — 26; CMake использует этот каталог исходников пакета.

После установки перечисленных зависимостей выполните:

```sh
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t apk
```

Зарегистрируйте телефон в личном профиле и разрешите уведомления. Служба принимает текст, изображения и файлы в фоне; откройте уведомление, чтобы поместить последний элемент в буфер. Android 10+ позволяет читать буфер только приложению в фокусе, поэтому отправляйте текущий буфер из открытого приложения либо используйте **«Поделиться → LocalClipboard»** из другого приложения. Выпускные APK требуют подписи; CI создаёт устанавливаемый отладочный APK и ZIP установщика через adb. На реальном устройстве нужно проверить буфер, локальную сеть, перезагрузку и управление батареей производителем на вашей версии Android.

### Подпись отладочных сборок Android

Android принимает обновление только с тем же ключом подписи, что у установленной версии. CI восстанавливает `~/.android/debug.keystore` из отдельного кеша отладочного ключа вместо новой подписи на каждом запуске. Кеш может быть удалён, а видимость кешей зависит от ветки. Для постоянной подписи создайте ключ локально командой из английского раздела выше или используйте свой существующий отладочный ключ. Сохраните base64-содержимое файла в секрете Actions репозитория `ANDROID_DEBUG_KEYSTORE_BASE64`. Требуются псевдоним `androiddebugkey` и пароли хранилища/ключа `android`, соответствующие конфигурации отладочной подписи Gradle.

Секрет имеет приоритет перед кешем и никогда не сохраняется в этот кеш. Сделайте приватную резервную копию ключа и не коммитьте его. Для публичного распространения используйте отдельный выпускной ключ и конфигурацию подписи. Pull request из форка не имеет доступа к секретам, поэтому его APK может иметь другой ключ. Если прежний ключ CI уже потерян, обновление поверх старой версии невозможно: сохраните код подключения, явно удалите приложение один раз, установите заново и подключитесь к прежнему профилю.

### iOS / iPadOS

Установите **полный Xcode**, его iOS SDK и совпадающие версии комплектов Qt для iOS и macOS с инструментами сборки. Qt для компьютера из Homebrew и Xcode Command Line Tools не позволяют собирать приложение для iPhone. Проверьте настройку Xcode командами `xcode-select -p` и `xcrun --sdk iphoneos --show-sdk-path`. Поддерживается iOS 16 и новее; CI использует Qt 6.8.3 и Xcode машины сборки.

Комплект Qt для iOS должен содержать нативный мультимедийный плагин AVFoundation. CMake выбирает только этот модуль для сканирования QR камерой, поэтому библиотеки FFmpeg не требуются.

```sh
export QT_IOS_ROOT="$HOME/Qt/6.8.3/ios"
export QT_HOST_PATH="$HOME/Qt/6.8.3/macos"
./scripts/build -t ios
```

Затем откройте `build_ios/LocalClipboard.xcodeproj`, выберите свою Team и уникальный Bundle ID, после чего запустите приложение на телефоне через Run. App Groups и ограниченное разрешение на multicast не нужны. См. [установку и проверку на устройстве](../packaging/ios/README.md#русская-версия).
