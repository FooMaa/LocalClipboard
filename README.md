# LocalClipboard

Encrypted LAN clipboard synchronization for **macOS, Linux, Windows, Android and iOS/iPadOS**.
Text, formatted text, images, photos and files go directly between your own devices;
there is no cloud, email account or central server.

## Registration and separate users

The installer opens the registration wizard (or open LocalClipboard after installing).
On your first device, enter your name and a device name, then **Create a personal
profile**. On your other devices, choose **Connect to my existing profile** and enter
the connection code shown by **Connect another device**.

Each person creates their own profile. For example, Anton's laptop and phone use
Anton's code; his partner's laptop and phone use a different code. Sharing the same
Wi-Fi does not connect these profiles. Device IDs, names, profile membership, pause
and startup preferences survive restarts and upgrades. A device uses one profile
at a time; **Change personal profile** registers it with a new or existing profile.

The `LC2.…` code contains a random 256-bit secret. Treat it like a password and
transfer it privately. It is excluded from automatic clipboard synchronization.
If a code is exposed or a device is lost, create a new profile and reconnect your
remaining devices. Individual device revocation and cloud accounts are not provided.

## Background operation and autostart

On desktop and Android, autostart is enabled by default at registration and can be toggled in settings.
Desktop synchronization runs in the **logged-in user's graphical session**, where
clipboard access is available. It does not run as a root/system service before login.

- **macOS:** a per-user LaunchAgent starts the app with `--background` at login and
  restarts it after failure. The app remains running when its window is closed.
- **Windows:** an HKCU Run entry starts it at login. Closing the window leaves the
  tray process running; the installer and profile are per user.
- **Linux:** XDG desktop autostart imports the graphical environment and starts a
  systemd user service with restart on failure. Desktops without a systemd user
  manager launch the background process directly. Display names are not hardcoded.
- **Android:** a separate Qt foreground service receives encrypted clipboard
  content, advertises the phone and shows notifications. It continues after the
  activity closes and starts after reboot / APK upgrades once registered, unless
  startup is disabled. It uses the `connectedDevice` foreground-service type for
  communication with local devices. OS force-stop and some manufacturers' battery
  settings can prevent restarting until the app is opened again.
- **iOS/iPadOS:** reception runs while the app is active. iOS suspends ordinary apps
  in the background and does not offer boot autostart for this app. Text/images are
  sent with an explicit clipboard button; native Photos/Files pickers and document
  opening support file imports. There is no background clipboard monitoring.

Reopening the desktop app opens the existing instance's window. Use **Quit LocalClipboard**
in settings or **Quit** in the tray menu to stop it. On desktops without a tray,
reopen it from the application launcher. Android also exposes the system's
foreground-service controls to stop its background work.

**Android 10+ does not permit ordinary background apps to read the clipboard.**
Open the app and press **Send current clipboard**, or use **Share → LocalClipboard**
from a gallery, file manager or another app. Received items are queued in private
storage; tap their notification / open the app to apply them to the clipboard. Photos
and files are exposed as read-only content URIs through FileProvider, so other apps
can paste them without storage-wide permissions. Whether an Android app accepts
file/image clipboard URIs depends on that app.

On iOS, register the same profile, then enter the computer's local IPv4 address in
**Network settings** and allow **Local Network** access. The updated desktop/Android
app replies directly, so no restricted broadcast entitlement is needed. Keep the
phone app open to receive. **Open in / Copy to LocalClipboard** imports documents
from apps that offer that action; arbitrary text sharing uses the clipboard button.
Files selected from **Send files / photos** or imported from another app are queued
for the active personal profile. **Open received files** provides Share / Save,
retry and deletion of local copies. See the [iOS setup guide](packaging/ios/README.md).

## Clipboard formats

- Plain text and HTML formatting.
- Clipboard images, transferred as PNG (up to 32 megapixels, 8192 pixels per side).
- Up to 32 files per transfer, including photos, videos and documents. Bytes are
  transferred, then stored locally and placed in the receiving clipboard as files.
  Linux also supplies GNOME/KDE copy formats; Windows uses copy semantics.
- **Send files / photos** sends files selected from a picker without requiring them
  to already be in the clipboard.

The combined raw-content limit is **32 MiB per transfer**. Folders and symbolic links
are not transferred; archive a folder first. Cut/move operations are received as
copies. Unsafe paths and duplicate filenames are rejected; nothing is executed or
opened automatically. Received files stay in the application's private `received`
directory; **Open received files** lets you access them. They are not automatically
deleted while another app may still need to paste them, so remove old files manually.
There is no transfer history, resumable streaming or delivery acknowledgement yet.
Desktop copies made while all registered peers are offline are not sent later;
Android's latest explicit outgoing item waits for a peer for up to ten minutes.

## Build: five packages and five installers

CMake 3.21.1+, a C++17 compiler, Qt 6 Core/Network/Widgets/Test and native packaging
tools are required. Monocypher 4.0.2 is vendored; no crypto runtime download is needed.

```sh
./scripts/build -t macos
./scripts/build -t linux
./scripts/build -t windows       # Git Bash + MSVC + Qt + NSIS
./scripts/build -t android       # Android Qt kit, SDK, NDK, Java 17
./scripts/build -t ios           # macOS + full Xcode + Qt iOS kit
```

Each command builds the app **and** its package and installer into `dist/` on the
corresponding host. `-n` builds only the app. With a configured CMake build, use
`cmake --build BUILD_DIR --config Release --target installers`.

| Platform | Application package | Installer |
| --- | --- | --- |
| Linux (Debian/Ubuntu) | `LocalClipboard-linux-amd64.deb` | `LocalClipboard-linux-amd64-installer.sh` |
| macOS | `LocalClipboard-macos-package.zip` (deployed `.app`) | `LocalClipboard-macos-installer.pkg` |
| Windows | `LocalClipboard-windows-package.zip` | `LocalClipboard-windows-installer.exe` (NSIS) |
| Android | `LocalClipboard-android-package.apk` | `LocalClipboard-android-installer.zip` (APK + adb installation/registration launch scripts) |
| iOS/iPadOS | `LocalClipboard-ios-package.zip` (arm64 device `.app`) | `LocalClipboard-ios-installer.zip` (source + Xcode build/sign/install script) |

Linux filenames use the host architecture. APK is also directly installable on the
phone; the separate installer ZIP automates USB installation from a computer with
adb. CI is configured to build all five platforms, test the desktop core and verify all ten
outputs before publishing a combined artifact with checksums. Android CI builds an
installable debug APK; distribution release APKs need a stable signing key. macOS
artifacts are ad-hoc signed unless Apple signing identities are supplied; public
Gatekeeper distribution additionally requires Apple signing and notarization.
The iOS CI package is **unsigned**. Install on your own phone through the supplied
Xcode installer, selecting your Apple Team and a unique bundle ID. It needs Xcode
and the Qt iOS kit on the installing Mac; it is not an App Store IPA. A Personal
Team can be used without App Groups or multicast capabilities. See the
[iOS installation instructions](packaging/ios/README.md).

See [build scripts](scripts/README.md) and [dependencies](dependencies/README.md).

```sh
cmake -S . -B build_desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build_desktop --parallel 4
ctest --test-dir build_desktop --output-on-failure
```

## Network and security

Discovery uses IPv4 UDP `45871`; encrypted transfers normally use TCP `45872`
(a free port is advertised if another user session owns that port). Allow these
ports on private/LAN firewall profiles. Router client isolation, guest Wi-Fi, VPN
routing and filtered broadcasts may prevent discovery. **Network settings** accepts
up to 32 direct IPv4 peer addresses on every platform. iOS uses direct discovery
only; enter one peer address on the phone to discover both directions. IPv6-only
networks are not supported. Device clocks must be within 60 seconds
for discovery announcements to be accepted.

Announcements are authenticated using keyed BLAKE2b. A fresh mutual-authentication
challenge precedes each TCP transfer; clipboard payloads use XChaCha20-Poly1305
with that challenge and both device IDs bound to the ciphertext. Wrong profiles,
plaintext legacy packets, tampered data and replays from previous connections are
rejected. Unauthenticated sockets cannot allocate the full transfer buffer.

Profile secrets are stored in a user-private configuration file (Windows relies on
the user's directory ACL; Android uses app-private storage with backups disabled).
This protects against unrelated LAN users, not a compromised registered device or
malware running as your OS user. The protocol has not had an independent security
audit and group keys do not provide forward secrecy. Pause synchronization when
copying content you do not want on your other registered devices.

Android platform constraints are documented in the official
[clipboard access rules](https://developer.android.com/about/versions/10/privacy/changes#clipboard-data)
and [connected-device service documentation](https://developer.android.com/develop/background-work/services/fgs/service-types#connected-device).

Apple documents [background suspension](https://developer.apple.com/documentation/uikit/extending-your-app-s-background-execution-time),
[paste permissions](https://developer.apple.com/documentation/uikit/uipastecontrol),
and the [restricted broadcast/multicast entitlement](https://developer.apple.com/news/?id=0oi77447).
