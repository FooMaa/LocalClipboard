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
