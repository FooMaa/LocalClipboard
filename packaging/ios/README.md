# LocalClipboard for your iPhone / iPad

Requires iOS 16+, a Mac with full Xcode (iOS SDK installed), and the Qt 6 iOS kit.
Xcode Command Line Tools and desktop Homebrew Qt alone are insufficient.

## Install through your own Xcode

1. Install matching Qt iOS and macOS host kits (CI uses Qt 6.8.3).
2. Connect the iPhone, enable Developer Mode, and trust the Mac if asked.
3. Unzip `LocalClipboard-ios-installer.zip` and run:

   ```sh
   export QT_IOS_ROOT="$HOME/Qt/6.8.3/ios"
   export QT_HOST_PATH="$HOME/Qt/6.8.3/macos"
   ./install.sh
   ```

4. The script builds an unsigned app and opens its Xcode project. Add your Apple
   account in Xcode Settings → Accounts. Select the **LocalClipboard** app target,
   choose your **Team**, enable automatic signing, and choose a unique Bundle
   Identifier, for example `com.yourname.LocalClipboard`.
5. Select your connected iPhone/iPad as the run destination and press **Run**.
   Follow Xcode/device prompts for trusting the developer. Register the device
   in the app using the same `LC2.…` connection code as your computer.

A Personal Team can be used for local device development. Its provisioning expires;
rebuild with Xcode when required. No App Groups, push notifications, multicast
entitlement, background mode or App Store upload is required by this app.
The installer needs the Qt iOS kit and builds from the included source; it does not
re-sign someone else's prebuilt application.

For an already configured Xcode account, the script can sign, install and launch:

```sh
export IOS_TEAM_ID=YOUR_TEAM_ID
export IOS_BUNDLE_ID=com.yourname.LocalClipboard
export IOS_DEVICE_ID=YOUR_DEVICE_ID # see: xcrun devicectl list devices
./install.sh
```

`LocalClipboard-ios-package.zip` contains the arm64 device `.app`. **CI's package
is unsigned and cannot be installed directly.** A build made with `IOS_TEAM_ID`
contains your development-signed app; its provisioning must cover the target phone.
Use the installer/source project for signing with another account. This is a local
Xcode installation flow, not an App Store IPA or a standalone iPhone installer.

## Connect and use

- On the computer, open **Network settings** and note its Wi-Fi/LAN IPv4 address.
- On the phone, enter that address in **Network settings**. Allow **Local Network**
  access when iOS asks (or enable it in Settings → Apps → LocalClipboard).
- Keep both apps open on the same LAN. The computer must use the updated version
  that replies to authenticated direct discovery. Your personal profile still
  controls membership; knowing an IP never grants clipboard access.
- **Send current clipboard** sends text / images explicitly. iOS may ask for paste
  permission. The app never polls the clipboard or reads it automatically on launch.
- **Send files / photos** uses Apple's Files or Photos picker. Up to 32 files,
  32 MiB combined; iCloud content may need downloading first.
- From Files or another app that offers **Open in / Copy to LocalClipboard**, send
  a document to LocalClipboard. Available actions depend on the sending app. This
  uses iOS document opening; there is no separate Share extension for arbitrary
  text selections. Use the clipboard button for text.
- Imported files are queued for the current personal profile and sent when the app
  is active and a registered peer is discovered. A different profile never sends
  the old queue. Once a send starts, its queue entry is removed; delivery is not
  acknowledged by the current protocol. Local imported copies remain available
  for retry in **Open received files → Send selected files again**.
- Received images and files are saved locally; **Open received files** opens a
  browser with **Share / Save** for exporting to Files, Photos or another app.
  Clipboard file support depends on the receiving app. Local copies can be deleted
  from this browser. The configuration / connection secret is never exposed there.

## iOS limits

Ordinary iOS apps cannot run as a permanent LAN daemon, start at boot, or monitor
other apps' clipboard in the background. Reception runs while LocalClipboard is
active. Leaving the app closes its sockets; returning starts discovery again.
No audio/location/VPN background-mode workaround is used. Open the phone app before
copying on the computer; copies made while it is suspended are not replayed later.

Direct IPv4 discovery avoids the restricted multicast/broadcast entitlement.
Wi-Fi client isolation, firewalls and VPN routing can still block communication.
If DHCP changes the computer's IP, update the saved address (or reserve it on the router).

Device validation: first-run registration and paste/network permissions; send text,
PNG, photo, empty file and multi-file batches in both directions; verify another
personal profile is invisible; suspend/resume; retry an imported file after an
interrupted transfer; install a new build over the old one and check profile retention.

## Language and uninstall

Choose Русский or English in the installer script or in the app's registration
wizard. The app also has a language picker for later changes.
`LC_LANGUAGE=ru ./install.sh` selects Russian without a terminal prompt.

Run the same `install.sh` again to manage an installed app: choose reinstall,
uninstall or cancel. Removal requires `DELETE` and deletes this device's local
app data. Reinstall preserves the profile. The OS's Delete App action also works.
Set `IOS_DEVICE_ID` and your actual `IOS_BUNDLE_ID` to enable installation detection
and removal. Removal needs Xcode devicectl but does not need Qt or a new build.
Without a device ID installation proceeds through Xcode.
