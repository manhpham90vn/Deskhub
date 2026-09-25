#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

. scripts/tools.sh

have() { command -v "$1" >/dev/null 2>&1; }

sync_submodules() {
    [ -e .git ] || return 0
    if git submodule status | grep -q '^-'; then
        echo "[install] git submodules (nvenc headers)..."
        git submodule update --init
    else
        echo "[ok]      git submodules"
    fi
}

check_rust() {
    if have cargo && have rustc; then
        echo "[ok]      rust ($(rustc --version | cut -d' ' -f2))"
        return 0
    fi
    echo "[action]  Rust missing - install it from https://rustup.rs and re-run bootstrap." >&2
    echo "          quiche (the QUIC transport) is built with cargo." >&2
    exit 1
}

install_cargo_ndk() {
    if have cargo-ndk; then
        echo "[ok]      cargo-ndk ($(command -v cargo-ndk))"
    else
        echo "[install] cargo-ndk (builds quiche for Android ABIs)..."
        cargo install cargo-ndk
    fi
}

ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-26.1.10909125}"
ANDROID_SDK_PACKAGES="platform-tools platforms;android-37.0 ndk;$ANDROID_NDK_VERSION cmake;3.22.1"

missing_android_packages() {
    for pkg in $ANDROID_SDK_PACKAGES; do
        [ -d "$1/$(echo "$pkg" | tr ';' '/')" ] || printf ' %s' "$pkg"
    done
}

install_android_packages() {
    SDK="${ANDROID_HOME:-$1}"
    ANDROID_CLI=""
    SDKMANAGER=""
    for cand in "$SDK"/cmdline-tools/*/bin/android; do
        if [ -x "$cand" ]; then ANDROID_CLI="$cand"; break; fi
    done
    for cand in "$SDK"/cmdline-tools/*/bin/sdkmanager; do
        if [ -x "$cand" ]; then SDKMANAGER="$cand"; break; fi
    done
    if [ -z "$ANDROID_CLI" ] && [ -z "$SDKMANAGER" ]; then
        echo "[action]  Android cmdline-tools missing - install Android Studio or sdkmanager, set ANDROID_HOME, then re-run bootstrap."
        return
    fi
    echo "[ok]      Android SDK ($SDK)"
    missing=$(missing_android_packages "$SDK")
    if [ -z "$missing" ]; then
        echo "[ok]      Android SDK packages ($ANDROID_SDK_PACKAGES)"
        return
    fi
    echo "[install] SDK packages (${missing# })..."
    for pkg in $missing; do
        if [ -n "$ANDROID_CLI" ]; then
            "$ANDROID_CLI" sdk install "$(echo "$pkg" | tr ';' '/')" || true
        else
            "$SDKMANAGER" --install "$pkg" || true
        fi
    done
    missing=$(missing_android_packages "$SDK")
    [ -z "$missing" ] || {
        echo "bootstrap: Android SDK packages still absent after the install ran:$missing" >&2
        echo "           cmdline-tools 23 replaced sdkmanager with 'android sdk install <group>/<version>', which returns a" >&2
        echo "           non-zero exit code even when it succeeds, so bootstrap judges the install by the directories under" >&2
        echo "           $SDK rather than by that code. Install them by hand and re-run." >&2
        exit 1
    }
}

sync_submodules

case "$(uname -s)" in
Darwin)
    if xcode-select -p >/dev/null 2>&1; then
        echo "[ok]      Xcode command line tools ($(xcode-select -p))"
    else
        echo "[action]  Xcode missing - run 'xcode-select --install' (CLT) and install Xcode from the App Store for client/ios."
    fi

    have brew || { echo "Homebrew not found - install from https://brew.sh first." >&2; exit 1; }

    for pkg in cmake ninja swiftlint pipx; do
        if have "$pkg"; then
            echo "[ok]      $pkg ($(command -v "$pkg"))"
        else
            echo "[install] $pkg..."
            brew install "$pkg"
        fi
    done
    if [ -x "$(brew --prefix llvm)/bin/clang++" ]; then
        echo "[ok]      llvm ($(brew --prefix llvm))"
    else
        echo "[install] llvm (Apple clang ships no libFuzzer runtime - 'make fuzz' needs this)..."
        brew install llvm
    fi

    if have java; then
        echo "[ok]      java ($(command -v java))"
    else
        echo "[install] JDK 17 (Temurin)..."
        brew install --cask temurin@17
    fi

    check_rust
    install_cargo_ndk
    scripts/build-quiche.sh apple
    scripts/build-opus.sh apple

    ensure_local_style_tools
    install_android_packages "$HOME/Library/Android/sdk"

    ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME:-$HOME/Library/Android/sdk}/ndk/$ANDROID_NDK_VERSION}"
    if [ -d "$ANDROID_NDK_HOME" ]; then
        export ANDROID_NDK_HOME
        scripts/build-quiche.sh android
        scripts/build-opus.sh android
    else
        echo "[skip]    quiche and opus for Android (no NDK at $ANDROID_NDK_HOME)"
    fi
    ;;

Linux)
    have apt-get || { echo "Only Ubuntu/Debian (apt) is supported for now." >&2; exit 1; }

    echo "[install] apt packages (build-essential clang llvm cmake ninja-build openjdk-17-jdk-headless pipx python3-venv unzip curl pkg-config rpm)..."
    scripts/apt-install.sh build-essential clang llvm cmake ninja-build \
        openjdk-17-jdk-headless pipx python3-venv unzip curl pkg-config rpm

    echo "[install] apt packages for the Ubuntu app (PipeWire, VA-API, GTK3, tray, nasm)..."
    scripts/apt-install.sh \
        libgtk-3-dev libglib2.0-dev libepoxy-dev libegl-dev libgles-dev \
        libdrm-dev libva-dev libpipewire-0.3-dev libspa-0.2-dev \
        libayatana-appindicator3-dev \
        nasm

    echo "[install] VA-API drivers + GNOME portal (KDE/wlroots users: install the matching xdg-desktop-portal backend)..."
    scripts/apt-install.sh va-driver-all vainfo xdg-desktop-portal xdg-desktop-portal-gnome || true

    scripts/build-ffmpeg.sh

    check_rust
    install_cargo_ndk
    scripts/build-quiche.sh host
    scripts/build-opus.sh host

    ensure_local_style_tools
    install_android_packages "$HOME/Android/Sdk"

    ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME:-$HOME/Android/Sdk}/ndk/$ANDROID_NDK_VERSION}"
    if [ -d "$ANDROID_NDK_HOME" ]; then
        export ANDROID_NDK_HOME
        scripts/build-quiche.sh android
        scripts/build-opus.sh android
    else
        echo "[skip]    quiche and opus for Android (no NDK at $ANDROID_NDK_HOME)"
    fi
    ;;

*)
    echo "Unsupported OS: $(uname -s) - Windows uses scripts/bootstrap.ps1." >&2
    exit 1
    ;;
esac

echo ""
echo "bootstrap: DONE"
echo "  Next: 'make' (list every target), 'make test', 'make lint', 'make build-<os>'"
