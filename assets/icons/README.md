# LocalClipboard icon

The source is `localclipboard.svg`: two overlapping clipboards with send/receive
arrows, on the app's blue gradient. `localclipboard-monochrome.svg` supplies the
macOS menu-bar mask and Android notification/themed icon artwork.

PNG, ICO and ICNS outputs, the Android launcher assets and the iOS AppIcon catalog
are committed. Normal application and installer builds use them directly; they
need no graphics tools or downloads. After changing either SVG, run
`./scripts/generate-icons` with CMake and the host Qt Gui/Svg kit installed.

The iOS catalog contains opaque square RGB PNGs: iOS applies the icon mask.
Android adaptive icons use a separate foreground and background; API 33 adds a
monochrome layer for themed icons. Windows executables and NSIS installers embed
the ICO. Linux installs the SVG and PNGs into hicolor and references the icon by
name. macOS bundles the ICNS and uses a template icon in the menu bar.
