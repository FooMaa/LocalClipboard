# Build dependencies

Desktop Linux (Debian/Ubuntu): `./dependencies/linux.sh`.
Desktop macOS: install Homebrew and Xcode Command Line Tools, then
`./dependencies/macos.sh`. These scripts install packages and may require sudo.

Other Linux distributions: C++17 compiler, CMake >=3.21, Ninja (optional),
Qt 6 Core, Network, Widgets and platform plugins. Windows: MSVC and Qt 6 desktop.

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
CMake project. The previous Kotlin/Gradle application has been replaced. Qt generates
the Android manifest, QtActivity, Gradle project and APK deployment configuration.
Network permissions and package name are configured on the CMake target.

After installing the dependencies above, run:

```sh
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t apk
```

Keep the app open on the same trusted Wi-Fi as the desktop peers. Tap **Send current
clipboard** to send Android text; incoming text is applied while the app is active.
Background clipboard synchronization is not supported. Release APKs require signing;
CI builds an installable debug APK. Device testing is required to verify clipboard
and LAN behavior on your Android version.
