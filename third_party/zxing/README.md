# ZXing-C++ 2.3.0

`core/` sources and LICENSE from
https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0.
The unused `core/src/libzint/` directory is omitted: its upstream symlinks point
to a zint checkout outside this vendored subset. The libzueci README has an added
Russian translation. Local patch in `core/src/HybridBinarizer.cpp`: diagnostic
image writes require `ZXING_DEBUG_IMAGE_WRITES` instead of being enabled by every
Debug build. The app and CI never define it, so camera-derived threshold maps
are not persisted. Other retained source files are unchanged.
The old built-in writer and readers are compiled into LocalClipboard; no runtime
or build-time downloads are required. See LICENSE (Apache-2.0);
NOTICES.txt contains the libzueci notices. Individual source files include their
own copyright and license notices.

## Русская версия

### ZXing-C++ 2.3.0

Исходники `core/` и LICENSE из [ZXing-C++ 2.3.0](https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0).
Неиспользуемый каталог `core/src/libzint/` исключён: его исходные симлинки ведут
в отдельный репозиторий zint, которого нет в этой поставке. В README libzueci
добавлен русский перевод. В `core/src/HybridBinarizer.cpp` внесена локальная
поправка: диагностические изображения записываются только при явном
`ZXING_DEBUG_IMAGE_WRITES`, а не в любой отладочной сборке. Приложение и CI
не задают этот флаг, поэтому карты порогов с камеры не сохраняются. Остальные
исходники не изменены.
В LocalClipboard встроены распознавание и прежний встроенный генератор кодов;
при сборке и запуске загрузки не нужны. Условия Apache-2.0 находятся в LICENSE,
сведения libzueci — в NOTICES.txt. Отдельные исходники содержат свои
сведения об авторстве и лицензиях.
