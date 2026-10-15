#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) == Linux ]] || { echo "Linux required" >&2; exit 1; }
command -v apt-get >/dev/null || { echo "Supported: Debian/Ubuntu with apt-get. Install C++17, CMake >=3.21, Ninja, Qt6 Core/Network/Widgets/Multimedia on other distributions." >&2; exit 1; }
privilege=()
if [[ $EUID != 0 ]]; then privilege=(sudo); fi
"${privilege[@]}" apt-get update
"${privilege[@]}" apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-multimedia-dev qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-layouts qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtqml-workerscript libgl1-mesa-dev dpkg-dev
