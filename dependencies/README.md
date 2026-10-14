# Build dependencies

Desktop Linux (Debian/Ubuntu): `./dependencies/linux.sh`.
Desktop macOS: install Homebrew and Xcode Command Line Tools, then
`./dependencies/macos.sh`. These scripts install packages and may require sudo.

Other Linux distributions: C++17 compiler, CMake >=3.21, Ninja (optional),
Qt 6 Core, Network, Widgets, Test and platform plugins. Windows: MSVC, Qt 6 desktop
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
