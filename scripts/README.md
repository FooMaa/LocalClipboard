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

## iOS / iPadOS (local Xcode installation)

`./scripts/package-ios-installer` assembles the source installer ZIP without an
iOS SDK. The regular `build -t ios` automatically emits both the app and installer.

Requires full Xcode with the iPhoneOS SDK and a Qt iOS kit, iOS 16+:

```sh
export QT_IOS_ROOT="$HOME/Qt/6.8.3/ios"
export QT_HOST_PATH="$HOME/Qt/6.8.3/macos"
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
