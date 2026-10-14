# ZXing-C++ 2.3.0

`core/` sources and LICENSE from
https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0.
The unused `core/src/libzint/` directory is omitted: its upstream symlinks point
to a zint checkout outside this vendored subset. The libzueci README has an added
Russian translation; the retained source files are unchanged.
The old built-in writer and readers are compiled into LocalClipboard; no runtime
or build-time downloads are required. See LICENSE (Apache-2.0);
NOTICES.txt contains the libzueci notices. Individual source files include their
own copyright and license notices.

## Русская версия

Исходники `core/` и LICENSE из ZXing-C++ 2.3.0 по ссылке выше.
Неиспользуемый каталог `core/src/libzint/` исключён: его исходные симлинки ведут
в отдельный репозиторий zint, которого нет в этой поставке. В README libzueci
добавлен русский перевод; оставшиеся исходники не изменены.
В LocalClipboard встроены распознавание и прежний встроенный генератор кодов;
при сборке и запуске загрузки не нужны. Условия Apache-2.0 находятся в LICENSE,
сведения libzueci — в NOTICES.txt. Отдельные исходники содержат свои
сведения об авторстве и лицензиях.
