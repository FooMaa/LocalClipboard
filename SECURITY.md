# Security and release verification

This is a source review and reproducible regression setup, not an independent
penetration test or a certification. Review date: 2026-10-15, application 2.3.1.

## Trust boundaries

New `LC3` profiles use per-device X25519 private keys and an owner-signed EdDSA
registry. The owner must approve a joining device after comparing fingerprints.
The discovery secret alone cannot decrypt pairwise transfers or sign registry
updates. Signed revisions resist rollback; revocations propagate on reconnection.
Offline peers with an old registry cannot learn a new revocation until they contact
an updated peer. Static pairwise keys do not provide forward secrecy. Legacy `LC2`
profiles retain shared-key trust until explicitly upgraded. See [feature limits](FEATURES.md).

Discovery is authenticated and accepts timestamps within 60 seconds. A captured
announcement can be replayed within that window; the fresh mutually authenticated
TCP challenge prevents using it to decrypt or replay an old clipboard transfer.
Metadata (traffic volume, endpoints, profile group identifier, device names) is
visible on the LAN. Do not expose discovery/transfer ports to the Internet.

Encrypted transfers use vendored Monocypher 4.0.3. QR encoding/decoding uses vendored
ZXing-C++ 2.3.0. These version labels are an inventory, not an assurance that no
advisories exist. Review upstream releases/advisories before publishing.

Upstream review on 2026-10-15: [Monocypher 4.0.3](https://github.com/LoupVaillant/Monocypher/releases/tag/4.0.3)
fixes a signature timing leak (this app uses AEAD/MAC and EdDSA for the device registry). Vendored files
were updated to that release; their revision and hashes are recorded alongside
the source. [ZXing's advisory page](https://github.com/zxing-cpp/zxing-cpp/security/advisories)
had no published advisories; this does not prove the older 2.3 branch is free of bugs.
Qt 6.11.2 includes the [Qt Quick path-parser fix](https://www.qt.io/blog/security-advisory-cve-2026-79616).
The [Qt vulnerability inventory](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products)
also lists issues in Qt VNC Server and Qt XML affecting 6.11.2; this app does not
ship VNC Server or use QDom serialization. Reassess those paths if adding features.

## Changes verified by regression tests

- Strict payload schemas, bounded text/images/files, canonical device IDs and MAC
  encodings; filenames reject traversal, reserved Windows names, control/bidi
  characters and Unicode/case aliases.
- Eight connections maximum, five-second authentication deadline and a shared
  96 MiB reservation budget for incoming wire frames and outgoing ciphertext.
  This is **not a process RSS limit**: JSON, plaintext, images and Qt allocate more.
  Discovery processing yields after 64 datagrams. Pause aborts active transfers.
- Received storage is limited to 512 MiB / 16,384 files with a 64 MiB free-space
  reserve. Existing files are never automatically erased. Files do not execute or
  open on receipt. The mobile browser lists at most 500 recent files.
- Android shares allow only bounded content-provider input, refuse our own provider
  as an input deputy, bound import concurrency and require confirmation before
  sending to peers. Only the private `received/` directory is exposed by the
  non-exported FileProvider, with temporary read grants.
- Android profile keys are wrapped by AES-256-GCM using Android Keystore. Legacy
  profiles migrate when the app opens; a failure is reported. Hardware backing is
  device-dependent. Keystore does not protect against a compromised app process.
- Android file preparation, image decoding and inbox processing run on workers.
  GUI clipboard access stays on the GUI thread and is deferred when inactive.
  Profile revisions reject stale asynchronous results.
- QR secrets are absent from image-provider URLs and logs; device names and file
  names use plain text rendering. Clipboard/received content is never included in
  receive notifications.
- Vendored ZXing's implicit Debug image dumps are disabled; the QR round-trip
  regression checks that decoding leaves no files in a temporary working directory.

Desktop/iOS profile secrets remain plaintext in private configuration storage.
POSIX directories/files use 0700/0600; Windows relies on the user's directory ACLs
and has not had a native DACL audit. Clipboard queues and received files are also
plaintext at rest. Malware running as the same OS user, a compromised registered
device, screenshots of QR codes and clipboard history tools are outside this
protection. Deleting a file is not secure erasure.

## Reproduce the checks

```sh
cmake -S . -B build_check -DCMAKE_BUILD_TYPE=Release -DLOCALCLIPBOARD_MOBILE_PREVIEW=ON
cmake --build build_check --parallel 4 --target clipboard_tests app_tests mobile_tests
ctest --test-dir build_check --output-on-failure
python3 tests/PackagingTests.py
python3 tests/AndroidPackagingTests.py

cmake -S . -B build_security -DCMAKE_BUILD_TYPE=Debug -DLOCALCLIPBOARD_SANITIZERS=ON
cmake --build build_security --parallel 4 --target clipboard_tests app_tests
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    ctest --test-dir build_security --output-on-failure

# A Clang installation with libFuzzer is required; CI uses Ubuntu Clang.
cmake -S . -B build_fuzz -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DBUILD_TESTING=OFF -DLOCALCLIPBOARD_FUZZING=ON
cmake --build build_fuzz --parallel 4 --target protocol_fuzz
mkdir -p build_fuzz/corpus
build_fuzz/protocol_fuzz -max_total_time=60 -max_len=65536 build_fuzz/corpus
```

Sanitizers instrument project code and bundled libraries, **not prebuilt Qt**.
Leak detection is disabled because Qt's offscreen platform retains global objects;
address and undefined-behavior checks remain enabled. The fuzzer covers discovery,
encrypted frame rejection, payload validation, invitation and filename parsing;
it does not cover the entire Qt image stack, camera drivers or every Java Intent.
A short fuzz run is a regression check, not exhaustive proof. Apple Clang may lack
libFuzzer; an upstream archive can be supplied via `LOCALCLIPBOARD_FUZZER_LIBRARY`.

The mobile tests check RU/EN layouts at widths 320/412/760, inertial scrolling and
GUI event-loop progress. Offscreen software rendering does not measure device GPU
frame pacing. Use a physical device for gestures, keyboard/insets, QR permission,
background delivery, reboot, force-stop and vendor power-management behavior.

### Observed locally on 2026-10-15

macOS: three CTest suites passed; ASan/UBSan passed both instrumented suites;
the updated protocol fuzzer completed 1,447,035 inputs in 61 seconds without a
finding. Thirteen packaging/signing regression tests passed. The macOS bundle
and installer built and passed bundle verification; they were not installed.

Physical Galaxy S25 SM-S931B, Android 17/API 37, 4 KiB pages: Debug-to-Release
updates preserved the test profile and its Keystore-wrapped key. Authenticated
file receipt, background notification, pause/resume, local-copy deletion, keyboard
visibility and native camera opening/cancellation were exercised. UI automation
no longer produced cross-thread QObject construction or QML errors in the checked
process. Camera decoding against a real QR, reboot/Doze/long-duration survival
and 16 KiB hardware were not exercised in this run.
After disabling ZXing diagnostics, Debug camera opening was repeated on the S25:
no threshold image was written. The test profile/files and earlier diagnostic map
were removed; temporary camera permission and USB stay-awake settings were restored.

Release scrolling: SurfaceFlinger latency samples during 12 alternating ADB swipes
contained 946 presentations over 8.248 s at a reported 8.333 ms refresh period.
Median interval: 8.332 ms; p95: 8.333 ms; 6/945 intervals exceeded 12.5 ms,
including 5 over 40 ms. Gesture pauses and idle boundaries are included: these are
presentation intervals, not CPU/GPU render durations or a certified jank rate.
Android `gfxinfo` reported no Qt SurfaceView frames; FrameTimeline did not expose
this application's SurfaceView either, so neither was treated as evidence of
zero jank. An earlier Debug run had p95 near 25 ms. The Release result does not
guarantee 120 fps under all workloads. APK signature, ZIP alignment and all 49
native ELF load alignments passed; this local Release uses a development key.

## Release gates

`-DLOCALCLIPBOARD_PRODUCTION=ON` requires Qt 6.11.2+ and a release configuration.
Review current [Qt advisories](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products);
the version floor alone does not include future fixes or security backports.
Linux CI is a distribution compatibility build, not a Qt security attestation.

Android CI artifacts are development builds signed by a persistent development
key. Public APKs need a stable private release key. Set `LC_ANDROID_KEYSTORE`,
`LC_ANDROID_KEY_ALIAS`, `LC_ANDROID_STORE_PASSWORD`, `LC_ANDROID_KEY_PASSWORD` and
run `./scripts/build -t android -c Release`. Passwords are passed to apksigner through
environment references, not command arguments. Keep credentials outside the repo,
logs and caches. `scripts/sign-android INPUT.apk OUTPUT.apk` also signs explicitly.
Changing the signing identity prevents an in-place update. An unsigned Release
without signing variables can be built with `-n`, but packaging refuses it.

```sh
python3 scripts/verify-android-apk dist/LocalClipboard-android-package.apk
"$ANDROID_SDK_ROOT/build-tools/36.0.0/apksigner" verify --verbose dist/LocalClipboard-android-package.apk
"$ANDROID_SDK_ROOT/build-tools/36.0.0/zipalign" -c -P 16 4 dist/LocalClipboard-android-package.apk
```

The ELF check verifies every native library's load alignment. ZIP alignment is a
separate check. Neither replaces running on 4 KiB and 16 KiB devices. Android 17
is newer than Qt 6.11's documented Android 9–16 matrix; test it explicitly.
macOS public distribution needs Developer ID signatures and notarization. iOS
requires your Xcode signing team. Windows/Linux/iOS builds must pass their native
CI and physical-device checks before release. CI actions use version tags; a
public release process should review/pin the action revisions it trusts.

The protocol now has authenticated received/copied receipts and bounded in-memory
retry/deduplication state. Restarting a process can lose status and retry data.
Resumable streaming and a durable per-peer outgoing log are not implemented.
Android retains only the latest pending outgoing item for ten minutes and its
latest received item. Local optional history is encrypted and bounded; plaintext
received files and bridge queues remain app-private. Diagnostics use an explicit
allowlist and exclude names, addresses, content and keys.

---

## Русская версия

Проверка исходников и регрессионные тесты от 15.10.2026 для версии 2.3.1 не заменяют
независимый пентест и не являются сертификацией безопасности.

Новые профили `LC3` используют личные ключи X25519 и подписанный владельцем реестр
EdDSA. До одобрения сравниваются отпечатки. Один секрет обнаружения не позволяет
расшифровать обмен между устройствами или подписать изменение реестра. Подписанные
версии защищают от отката; отзыв распространяется при подключении. Узлы со старым
реестром, изолированные от обновлений, не узнают об отзыве до связи с обновлённым
узлом. Статические ключи не обеспечивают прямую секретность. Старые профили `LC2`
сохраняют общий ключ до явного перехода. См. [ограничения возможностей](FEATURES.md#русская-версия).
Метаданные обнаружения видны в сети; не публикуйте порты приложения в Интернете.
Окно времени объявления — 60 секунд, новый обмен TCP защищает каждую передачу.

Добавлены строгие схемы сообщений, проверка имён файлов и Unicode-дубликатов,
лимит восьми соединений, срок аутентификации пять секунд и общий бюджет резерва
сетевых кадров 96 МиБ. Это не предел памяти процесса: JSON, изображения и Qt
выделяют дополнительную память. При паузе соединения закрываются. Хранилище
полученных файлов ограничено 512 МиБ и 16 384 файлами, с резервом свободного места
64 МиБ; существующие файлы не стираются автоматически. Файлы не запускаются сами.

На Android ограничены объём и параллелизм импорта «Поделиться», перед отправкой
нужно подтверждение. FileProvider выдаёт временный доступ только для чтения к
`received/`. Секрет профиля шифруется AES-GCM ключом Android Keystore; старый профиль
переносится при открытии приложения, ошибка показывается пользователю. Аппаратная
защита зависит от устройства. Взломанный процесс приложения всё ещё может
использовать ключ. На компьютерах и iOS ключ остаётся в приватном файле; POSIX
использует 0700/0600, Windows — ACL пользовательского каталога без отдельного аудита
DACL. Очереди и полученные файлы остаются незашифрованными на диске. Удаление не
гарантирует физического стирания. Вредоносное ПО от имени вашего пользователя ОС,
взломанное устройство профиля и перехват QR находятся вне этой защиты.

Подготовка файлов и применение входящего содержимого Android выполняются в рабочих
потоках, обращения к системному буферу — в активном интерфейсе. Результаты от старого
профиля отбрасываются. Имена отображаются простым текстом. Уведомления не раскрывают
содержимое буфера, URL поставщика QR не содержит секрет. Используются включённые
Monocypher 4.0.3 и ZXing-C++ 2.3.0: это сведения о версиях, а не гарантия отсутствия
уязвимостей; перед выпуском проверяйте обновления и рекомендации проектов.

При проверке 15.10.2026 Monocypher обновлён до 4.0.3; исправление утечки времени
подписи описано в ссылке выше (приложение использует AEAD/MAC, а не EdDSA).
Ревизия и SHA-256 оригинальных файлов записаны рядом с исходниками. На странице
ZXing не было опубликованных рекомендаций, что не доказывает отсутствие ошибок
в ветке 2.3. Qt 6.11.2 включает исправление разбора путей Qt Quick. В списке Qt
также есть ошибки VNC Server и сериализации QDom для этой версии; приложение
не включает VNC Server и не использует сериализацию QDom. При расширении
функциональности эти границы нужно пересмотреть.

Команды проверок приведены выше. ASan/UBSan проверяют наш код и включённые
библиотеки, но не готовые бинарники Qt. Проверка утечек отключена из-за глобальных
объектов Qt offscreen; проверки доступа к памяти и неопределённого поведения
включены. Fuzz-тест покрывает разбор протокола, содержимого, кодов и имён файлов;
камеры, Java Intent и весь декодер изображений Qt им не покрыты. Короткий запуск
не доказывает отсутствие ошибок. Тесты интерфейса проверяют RU/EN на ширинах
320/412/760, инерцию и обработку событий; для оценки GPU, клавиатуры, QR, фонового
приёма и автозапуска после перезагрузки нужен настоящий телефон.

В ZXing отключена неявная запись диагностических изображений в Debug. Тест QR
проверяет, что после распознавания в рабочем временном каталоге не осталось файлов;
CI запускает ASan/UBSan в Debug, чтобы проверять и этот сценарий.

Локальный результат 15.10.2026: три набора CTest и два набора ASan/UBSan прошли,
fuzz-прогон обработал 1 447 035 входов за 61 секунду без находок. Прошли 13 тестов
упаковки и подписи; macOS-пакет и установщик собраны и проверены без установки.
На Galaxy S25 SM-S931B (Android 17/API 37, страницы 4 КБ) проверены обновление
Debug → Release с сохранением профиля Keystore, аутентифицированный приём файла,
фоновое уведомление, пауза, удаление локальной копии, клавиатура, открытие и отмена
камеры. При проверке интерфейса больше не воспроизводились ошибки QML и создания
QObject в чужом потоке. Распознавание настоящего QR камерой, перезагрузка, Doze,
длительная автономная работа и устройство с 16 КБ в этот прогон не входили.
После исправления ZXing камера повторно проверена в Debug: диагностическое
изображение не появилось. Тестовый профиль, файлы и прежняя карта порогов удалены;
временное разрешение камеры и настройка бодрствования по USB восстановлены.

В Release записано 946 показов кадров за 8,248 с при 12 жестах ADB: медиана
интервала 8,332 мс, p95 8,333 мс; 6 из 945 интервалов длиннее 12,5 мс, в том числе
5 длиннее 40 мс. Учтены паузы между жестами и границы прокрутки. Это интервалы
показа, а не время рендеринга и не процент задержанных кадров. `gfxinfo` и
FrameTimeline не показали кадры SurfaceView приложения; нулевой счётчик не принят
за отсутствие задержек. В более раннем Debug-прогоне p95 был около 25 мс.
Постоянные 120 кадров/с во всех сценариях не гарантируются. Подпись APK, ZIP и все
49 нативных библиотек прошли проверки; локальный Release подписан ключом разработки.

Для публичного выпуска используйте `LOCALCLIPBOARD_PRODUCTION=ON`, Qt 6.11.2 или
новее и выпускную конфигурацию, проверьте актуальные рекомендации Qt. Linux CI
проверяет совместимость с Qt дистрибутива, а не полноту исправлений безопасности.
Android CI выпускает отладочный APK. Для Release задайте постоянный приватный ключ
через `LC_ANDROID_KEYSTORE`, `LC_ANDROID_KEY_ALIAS`, `LC_ANDROID_STORE_PASSWORD` и
`LC_ANDROID_KEY_PASSWORD`, затем выполните `./scripts/build -t android -c Release`.
Пароли передаются apksigner через окружение без значений в аргументах команд.
Неподписанный Release можно собрать с `-n`, но нельзя упаковать для установки.
Смена подписи мешает обновлению поверх установленной версии. Проверяйте подпись,
ELF и ZIP-выравнивание командами выше и запускайте на устройствах с 4 и 16 КБ.
Android 17 новее документированной матрицы Qt 6.11 (Android 9–16) и требует отдельной
проверки. Для macOS нужны Developer ID и нотариализация, для iOS — ваша Team Xcode;
остальные платформы также должны пройти собственные сборки и проверки. Перед
публичным выпуском проверьте и закрепите доверенные ревизии GitHub Actions.

Протокол подтверждает получение и копирование аутентифицированными ответами.
Повторы и подавление дубликатов используют ограниченную память; перезапуск может
потерять состояния. Потоковая передача с возобновлением и постоянный исходящий журнал
не реализованы. Android хранит последний исходящий элемент до десяти минут и только
последний входящий. Необязательная история шифруется и ограничивается; полученные
файлы и очереди остаются в приватном хранилище. Диагностика формируется по разрешённому
списку полей без имён, адресов, содержимого и ключей.
