# Build and package

Run from any directory; `-b` paths are relative to the caller. Native builds need
the corresponding host. The four-platform GitHub Actions workflow supplies these
hosts and uploads a combined distribution with exactly four packages and four installers.

```sh
./scripts/build -t linux -c Release -j 4
./scripts/build -t macos
./scripts/build -t windows       # Git Bash in an MSVC environment
./scripts/build -t android -a arm64-v8a -c Debug
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
