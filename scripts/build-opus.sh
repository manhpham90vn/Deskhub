#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

OPUS_VERSION=1.5.2
OPUS_SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
ANDROID_API=24
IOS_MIN=17.0
MACOS_MIN=14.0
PREFIX="$PWD/third_party/opus"
SRC="$PREFIX/src"

ANDROID_TARGETS=(aarch64-linux-android x86_64-linux-android)
APPLE_TARGETS=(aarch64-apple-darwin x86_64-apple-darwin aarch64-apple-ios aarch64-apple-ios-sim)
WINDOWS_TARGETS=(x86_64-pc-windows-msvc)

CMAKE_COMMON=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DOPUS_BUILD_SHARED_LIBRARY=OFF
    -DOPUS_BUILD_PROGRAMS=OFF
    -DOPUS_BUILD_TESTING=OFF
    -DBUILD_TESTING=OFF
    -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF
    -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF
)

command -v cmake >/dev/null 2>&1 || {
    echo "build-opus.sh: 'cmake' is required." >&2
    exit 1
}

host_target() {
    local machine
    machine=$(uname -m)
    case "$(uname -s)" in
        Linux)
            case "$machine" in
                aarch64 | arm64) echo aarch64-unknown-linux-gnu ;;
                *) echo x86_64-unknown-linux-gnu ;;
            esac
            ;;
        Darwin)
            case "$machine" in
                arm64) echo aarch64-apple-darwin ;;
                *) echo x86_64-apple-darwin ;;
            esac
            ;;
        MINGW* | MSYS* | CYGWIN*) echo x86_64-pc-windows-msvc ;;
        *)
            echo "build-opus.sh: unknown host $(uname -s)/$machine." >&2
            exit 1
            ;;
    esac
}

resolve_targets() {
    local group targets=()
    for group in "$@"; do
        case "$group" in
            host) targets+=("$(host_target)") ;;
            android) targets+=("${ANDROID_TARGETS[@]}") ;;
            apple) targets+=("${APPLE_TARGETS[@]}") ;;
            windows) targets+=("${WINDOWS_TARGETS[@]}") ;;
            *) targets+=("$group") ;;
        esac
    done
    printf '%s\n' "${targets[@]}"
}

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{ print $1 }'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{ print $1 }'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$1" | awk '{ print $NF }'
    else
        echo "build-opus.sh: no sha256 tool (sha256sum, shasum or openssl) on PATH." >&2
        exit 1
    fi
}

fetch_source() {
    [ -f "$SRC/.version" ] && [ "$(cat "$SRC/.version")" = "$OPUS_VERSION" ] && return 0
    echo "[fetch]   opus $OPUS_VERSION"
    rm -rf "$SRC"
    mkdir -p "$SRC"
    local tarball="$PREFIX/opus-$OPUS_VERSION.tar.gz"
    curl -fsSL -o "$tarball" \
        "https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz"
    local got
    got=$(sha256_of "$tarball")
    if [ "$got" != "$OPUS_SHA256" ]; then
        echo "build-opus.sh: opus-$OPUS_VERSION.tar.gz hashed $got, expected $OPUS_SHA256." >&2
        rm -f "$tarball"
        rm -rf "$SRC"
        exit 1
    fi
    tar xz -f "$tarball" -C "$SRC" --strip-components=1
    rm -f "$tarball"
    echo "$OPUS_VERSION" >"$SRC/.version"
}

is_msvc_target() {
    case "$1" in
        *-windows-msvc) return 0 ;;
        *) return 1 ;;
    esac
}

is_android_target() {
    case "$1" in
        *-linux-android | *-linux-androideabi) return 0 ;;
        *) return 1 ;;
    esac
}

android_abi_of() {
    case "$1" in
        aarch64-linux-android) echo arm64-v8a ;;
        armv7-linux-androideabi) echo armeabi-v7a ;;
        x86_64-linux-android) echo x86_64 ;;
        i686-linux-android) echo x86 ;;
    esac
}

artifact_of() {
    case "$1" in
        *-windows-msvc) echo opus.lib ;;
        *) echo libopus.a ;;
    esac
}

on_windows_host() {
    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*) return 0 ;;
        *) return 1 ;;
    esac
}

require_ninja() {
    command -v ninja >/dev/null 2>&1 || {
        echo "build-opus.sh: 'ninja' is not on PATH." >&2
        echo "               Windows builds pin the Ninja generator: the Visual Studio one" >&2
        echo "               defaults to Win32 and cannot target the NDK at all. Visual Studio" >&2
        echo "               ships ninja - run this from a Native Tools prompt, or install it." >&2
        exit 1
    }
}

prefer_msvc_tools() {
    local cl
    cl=$(command -v cl 2>/dev/null) || cl=""
    [ -n "$cl" ] || {
        echo "build-opus.sh: MSVC 'cl.exe' is not on PATH." >&2
        echo "               Open 'x64 Native Tools Command Prompt for VS' (or run vcvars64.bat)," >&2
        echo "               then start bash from it and re-run this script." >&2
        exit 1
    }
    PATH="$(dirname "$cl"):$PATH"
    export PATH
}

require_ndk() {
    [ -n "${ANDROID_NDK_HOME:-}" ] || {
        echo "build-opus.sh: ANDROID_NDK_HOME is not set." >&2
        echo "               'make build-android' sets it from the SDK; outside make, export it." >&2
        exit 1
    }
    [ -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" ] || {
        echo "build-opus.sh: no android.toolchain.cmake under $ANDROID_NDK_HOME." >&2
        exit 1
    }
}

configure_args_for() {
    local target=$1
    ARGS=("${CMAKE_COMMON[@]}")
    case "$target" in
        *-windows-msvc)
            ARGS+=(-DOPUS_STATIC_RUNTIME=ON)
            ;;
        *-linux-android | *-linux-androideabi)
            require_ndk
            ARGS+=(
                -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
                -DANDROID_ABI="$(android_abi_of "$target")"
                -DANDROID_PLATFORM="android-$ANDROID_API"
            )
            ;;
        aarch64-apple-darwin)
            ARGS+=(-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_MIN")
            ;;
        x86_64-apple-darwin)
            ARGS+=(-DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_MIN")
            ;;
        aarch64-apple-ios)
            ARGS+=(
                -DCMAKE_SYSTEM_NAME=iOS
                -DCMAKE_OSX_SYSROOT=iphoneos
                -DCMAKE_OSX_ARCHITECTURES=arm64
                -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_MIN"
            )
            ;;
        aarch64-apple-ios-sim)
            ARGS+=(
                -DCMAKE_SYSTEM_NAME=iOS
                -DCMAKE_OSX_SYSROOT=iphonesimulator
                -DCMAKE_OSX_ARCHITECTURES=arm64
                -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_MIN"
            )
            ;;
    esac
}

install_headers() {
    local staged=$1
    [ -d "$staged/include/opus" ] || {
        echo "build-opus.sh: cmake --install produced no include/opus under $staged." >&2
        exit 1
    }
    mkdir -p "$PREFIX/include"
    rm -rf "$PREFIX/include/opus"
    cp -R "$staged/include/opus" "$PREFIX/include/opus"
}

build_target() {
    local target=$1
    local out="$PREFIX/$target"
    local stamp="$out/.stamp"
    local artifact want
    artifact=$(artifact_of "$target")
    want="$OPUS_VERSION"
    is_msvc_target "$target" && want="$OPUS_VERSION+static-runtime"

    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$want" ] && [ -f "$out/$artifact" ] &&
        [ -f "$PREFIX/include/opus/opus.h" ]; then
        echo "[ok]      opus $OPUS_VERSION ($target)"
        return 0
    fi

    is_msvc_target "$target" && prefer_msvc_tools
    configure_args_for "$target"
    if is_msvc_target "$target" || on_windows_host; then
        require_ninja
        ARGS+=(-G Ninja)
    fi

    local build="$SRC/build-$target"
    local staged="$SRC/staged-$target"
    rm -rf "$build" "$staged"

    echo "[build]   opus $OPUS_VERSION ($target)..."
    cmake -S "$SRC" -B "$build" "${ARGS[@]}" >/dev/null
    cmake --build "$build" --config Release --parallel >/dev/null
    cmake --install "$build" --config Release --prefix "$staged" >/dev/null

    local built
    built=$(find "$staged" -name "$artifact" -type f | head -1) || built=""
    [ -n "$built" ] || {
        echo "build-opus.sh: cmake reported success but no $artifact under $staged." >&2
        echo "               Installed files:" >&2
        find "$staged" -type f | sed 's/^/                 /' >&2
        exit 1
    }

    install_headers "$staged"
    mkdir -p "$out"
    cp "$built" "$out/$artifact"
    rm -rf "$build" "$staged"
    echo "$want" >"$stamp"
    echo "[ok]      opus $OPUS_VERSION ($target) → third_party/opus/$target/$artifact"
}

make_macos_universal() {
    [ "$(uname -s)" = "Darwin" ] || return 0
    local arm="$PREFIX/aarch64-apple-darwin/libopus.a"
    local x64="$PREFIX/x86_64-apple-darwin/libopus.a"
    local universal="$PREFIX/macos-universal/libopus.a"
    [ -f "$arm" ] && [ -f "$x64" ] || return 0
    if [ "$universal" -nt "$arm" ] && [ "$universal" -nt "$x64" ]; then
        echo "[ok]      opus $OPUS_VERSION (macos-universal) is newer than both slices"
        return 0
    fi
    mkdir -p "$PREFIX/macos-universal"
    lipo -create "$arm" "$x64" -output "$universal"
    echo "[ok]      opus $OPUS_VERSION (macos-universal) = arm64 + x86_64"
}

REQUESTED=("$@")
[ ${#REQUESTED[@]} -gt 0 ] || REQUESTED=(host)

TARGETS=()
while IFS= read -r target; do
    TARGETS+=("$target")
done < <(resolve_targets "${REQUESTED[@]}")

fetch_source

for target in "${TARGETS[@]}"; do
    build_target "$target"
done

make_macos_universal
