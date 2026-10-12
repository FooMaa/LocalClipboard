# Build and package

Run scripts from any working directory. Paths passed with `-b` are relative to the caller.

```sh
./scripts/build -t linux -c Release -j 4
./scripts/build -t macos -- -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
./scripts/build -t windows # Git Bash with MSVC environment and Qt configured
./scripts/build -t android -a arm64-v8a -c Debug
./scripts/package -t deb
./scripts/package -t sh
./scripts/package -t apk
```

`desktop` is the default native target. Build directories default to `build_TARGET`.
`-b DIR` overrides the directory in both scripts. `-h` prints help. Additional
CMake options follow `--`. See [dependencies](../dependencies/README.md).

Android builds use the Android kit's `qt-cmake` and Qt's `apk` target. Debug APKs
can be installed directly; Release APKs require signing. Package outputs go to
`dist/`. The Linux shell installer needs Qt 6 runtime libraries installed and
installs the executable into `~/.local/bin`; start it in a graphical session.
Windows/macOS deployment uses `windeployqt`/`macdeployqt` in CI.
