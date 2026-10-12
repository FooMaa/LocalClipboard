#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) == Darwin ]] || { echo "macOS required" >&2; exit 1; }
command -v brew >/dev/null || { echo "Install Homebrew from https://brew.sh first." >&2; exit 1; }
xcode-select -p >/dev/null || { echo "Run xcode-select --install first." >&2; exit 1; }
brew install cmake ninja qt
echo "Build: ./scripts/build -t macos -- -DCMAKE_PREFIX_PATH=$(brew --prefix qt)"
