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

---

## Русская версия

### Иконка LocalClipboard

Исходник — `localclipboard.svg`: два перекрывающихся буфера со стрелками отправки и приёма на синем градиенте приложения. `localclipboard-monochrome.svg` содержит одноцветный рисунок для маски иконки в строке меню macOS, уведомлений Android и тематических иконок Android.

Готовые PNG, ICO и ICNS, ресурсы иконки запуска Android и каталог AppIcon для iOS включены в репозиторий. Обычная сборка приложения и установщика использует их напрямую, без графических инструментов и загрузок. После изменения любого из двух SVG выполните `./scripts/generate-icons`, установив CMake и комплект Qt для компьютера с модулями Gui/Svg.

Каталог iOS содержит непрозрачные квадратные PNG в RGB: маску иконки применяет сама iOS. Адаптивные иконки Android используют отдельные передний план и фон; начиная с API 33 доступен одноцветный слой для тематических иконок. Исполняемые файлы Windows и установщики NSIS включают ICO. Linux устанавливает SVG и PNG в hicolor и обращается к иконке по имени. macOS включает ICNS в пакет приложения и использует шаблонную иконку в строке меню.
