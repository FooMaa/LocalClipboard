# LocalClipboard

A small LAN-only clipboard synchronizer for macOS, Linux, Windows, and Android using Qt 6. It exchanges **plain text only** over the current local network; no cloud account or central server is used.

## How it works

- UDP broadcast on port `45871` announces peer presence and receives announcements.
- TCP on port `45872` transfers JSON text messages directly between peers.
- A device ID and content hash suppress simple echo loops.
- The UI shows peers and a log. Use **Pause sync** to stop sending/applying clipboard updates.
- On Android, background clipboard access is restricted by the OS. The Qt Android app sends text via an explicit button while open.

## Build

Requirements: CMake 3.21+, a C++17 compiler, Qt 6 Core, Network and Widgets.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/LocalClipboard
```

On Windows, launch `build/Release/LocalClipboard.exe` for multi-config generators, or `build/LocalClipboard.exe` for single-config generators.

Build helpers: `./scripts/build -t linux`, `./scripts/build -t macos`,
`./scripts/build -t desktop`, or `./scripts/build -t android`.
See [scripts](scripts/README.md) and [dependencies](dependencies/README.md)
for setup, Android instructions, and packaging.

## First run

1. Start the app on each computer connected to the same Wi-Fi.
2. Allow it through the OS firewall on **private/local networks only**.
3. The peers list should populate within a few seconds.
4. Copy text on one computer and paste it on another.

If your router has **AP/client isolation** enabled, peer-to-peer connections will be blocked. Guest Wi-Fi often enables this.

## Security

This is a prototype for a trusted home LAN, **not hardened for untrusted networks**. Clipboard text is sent unencrypted and there is no pairing/authentication. Do not use it on public or shared Wi-Fi, and do not copy passwords, tokens, private keys, or sensitive data while it is running. Stop the app when not needed.

## Limitations

- Text only; no images or files.
- Android uses the same Qt application. Keep it open and tap **Send current clipboard**; background clipboard synchronization is not supported.
- macOS may require Accessibility/automation or privacy prompts depending on OS behavior; allow local network connections if prompted.
- Linux desktop clipboard support depends on the Qt platform plugin and the session (X11/Wayland).
- UDP broadcast discovery may not work across VLANs, guest Wi-Fi, VPNs, or networks with broadcast filtering.
- Device discovery uses IPv4 broadcast. IPv6-only networks are not supported.

## Ports

- UDP `45871` discovery
- TCP `45872` clipboard transfer

## Project layout

- `src/ClipboardSync.h/.cpp`: discovery, peer tracking, clipboard monitor and TCP message handling
- `src/main.cpp`: tray/window UI
