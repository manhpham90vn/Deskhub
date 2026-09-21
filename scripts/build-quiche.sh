#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

QUICHE_VERSION=0.29.3
QUICHE_COMMIT=55886df3be579579207104c8e645825b6347a209
ANDROID_API=24
PREFIX="$PWD/third_party/quiche"
SRC="$PREFIX/src"

ANDROID_TARGETS=(aarch64-linux-android x86_64-linux-android)
APPLE_TARGETS=(aarch64-apple-darwin x86_64-apple-darwin aarch64-apple-ios aarch64-apple-ios-sim)
WINDOWS_TARGETS=(x86_64-pc-windows-msvc)

command -v cargo >/dev/null 2>&1 || {
    echo "build-quiche.sh: 'cargo' is required. Install Rust: https://rustup.rs" >&2
    exit 1
}

host_target() {
    rustc -vV | awk '/^host:/ { print $2 }'
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

fetch_source() {
    [ -f "$SRC/.commit" ] && [ "$(cat "$SRC/.commit")" = "$QUICHE_COMMIT" ] && return 0
    echo "[fetch]   quiche $QUICHE_VERSION ($QUICHE_COMMIT)"
    rm -rf "$SRC"
    mkdir -p "$SRC"
    git clone --quiet --depth 1 --branch "$QUICHE_VERSION" \
        https://github.com/cloudflare/quiche "$SRC"
    local got
    got=$(git -C "$SRC" rev-parse HEAD)
    if [ "$got" != "$QUICHE_COMMIT" ]; then
        echo "build-quiche.sh: tag $QUICHE_VERSION resolved to $got, expected $QUICHE_COMMIT." >&2
        rm -rf "$SRC"
        exit 1
    fi
    echo "$QUICHE_COMMIT" >"$SRC/.commit"
}

export_boringssl_headers() {
    local cargo_home="${CARGO_HOME:-$HOME/.cargo}"
    local found=""
    if [ -d "$cargo_home/registry/src" ]; then
        found=$(find "$cargo_home/registry/src" -maxdepth 6 -type d \
            -path '*/boring-sys-*/deps/boringssl/src/include' 2>/dev/null | sort | tail -1) ||
            true
    fi
    if [ -z "$found" ]; then
        if [ -d "$PREFIX/include/openssl" ]; then
            echo "[ok]      BoringSSL headers already exported"
            return 0
        fi
        echo "build-quiche.sh: BoringSSL headers not found under $cargo_home/registry/src." >&2
        echo "                 Without include/openssl the CMake build silently falls back to" >&2
        echo "                 the no-QUIC stubs and the apps cannot share. They appear once" >&2
        echo "                 cargo has fetched boring-sys; re-run this script after a build." >&2
        return 1
    fi
    rm -rf "$PREFIX/include/openssl"
    cp -R "$found/openssl" "$PREFIX/include/openssl"
    echo "[ok]      BoringSSL headers → third_party/quiche/include/openssl"
}

is_msvc_target() {
    case "$1" in
        *-windows-msvc) return 0 ;;
        *) return 1 ;;
    esac
}

prefer_msvc_tools() {
    local cl
    cl=$(command -v cl 2>/dev/null) || cl=""
    [ -n "$cl" ] || {
        echo "build-quiche.sh: MSVC 'cl.exe' is not on PATH." >&2
        echo "                 Open 'x64 Native Tools Command Prompt for VS' (or run vcvars64.bat)," >&2
        echo "                 then start bash from it and re-run this script." >&2
        exit 1
    }
    PATH="$(dirname "$cl"):$PATH"
    export PATH
}

prefer_nasm() {
    command -v nasm >/dev/null 2>&1 && return 0
    local nasm_dir="${LOCALAPPDATA:-}/bin/NASM"
    [ -x "$nasm_dir/nasm.exe" ] || return 0
    PATH="$nasm_dir:$PATH"
    export PATH
}

refuse_ninja_generator() {
    [ "${CMAKE_GENERATOR:-}" = "Ninja" ] || return 0
    echo "build-quiche.sh: unset CMAKE_GENERATOR=Ninja for this build." >&2
    echo "                 The cmake crate only communicates the static CRT through" >&2
    echo "                 per-config flags under the default Visual Studio generator;" >&2
    echo "                 with Ninja, BoringSSL silently builds /MD and the final link" >&2
    echo "                 fails with LNK2038. If MSBuild trips MSB6003 on long paths," >&2
    echo "                 enable Windows long paths instead of switching generators." >&2
    exit 1
}

prefer_static_crt() {
    export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS="-C target-feature=+crt-static"
    export CFLAGS_x86_64_pc_windows_msvc="-MT"
    export CXXFLAGS_x86_64_pc_windows_msvc="-MT"
}

rust_checks_wanted() {
    [ "${DESKHUB_QUICHE_CHECKS:-0}" = "1" ]
}

prefer_runtime_checks() {
    rust_checks_wanted || return 0
    local target=$1 triple_upper var flags
    triple_upper=$(printf '%s' "${target//-/_}" | tr '[:lower:]' '[:upper:]')
    var="CARGO_TARGET_${triple_upper}_RUSTFLAGS"
    flags="-C debug-assertions=on -C overflow-checks=on"
    if [ -n "${!var:-}" ]; then
        export "$var=${!var} $flags"
    else
        export RUSTFLAGS="${RUSTFLAGS:+$RUSTFLAGS }$flags"
    fi
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

android_clang_target_of() {
    case "$1" in
        armv7-linux-androideabi) echo "armv7a-linux-androideabi$ANDROID_API" ;;
        *) echo "$1$ANDROID_API" ;;
    esac
}

artifact_of() {
    case "$1" in
        *-windows-msvc) echo quiche.lib ;;
        *) echo libquiche.a ;;
    esac
}

on_windows_host() {
    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*) return 0 ;;
        *) return 1 ;;
    esac
}

prefer_android_sdk_cmake() {
    local sdk="${ANDROID_HOME:-${LOCALAPPDATA:-}/Android/Sdk}"
    local bin
    bin=$(ls -d "$sdk"/cmake/*/bin 2>/dev/null | sort | tail -1) || bin=""
    [ -n "$bin" ] || {
        echo "build-quiche.sh: no cmake under $sdk/cmake." >&2
        echo "                 BoringSSL cross-compiles for the NDK with Ninja; MSVC's cmake" >&2
        echo "                 defaults to the Visual Studio generator, which cannot target" >&2
        echo "                 Android and fails on VCTargetsPath.vcxproj." >&2
        echo "                 Install it: sdkmanager \"cmake;3.22.1\" (bootstrap does)." >&2
        exit 1
    }
    PATH="$bin:$PATH"
    export PATH
    export CMAKE_GENERATOR=Ninja
}

bindgen_args_single_ndk_resource() {
    printf '%s' "--target=$1 -isystem$2 -nobuiltininc"
}

build_android_with_ndk_clang() {
    local target=$1
    local triple_u triple_upper ndk prebuilt bin resource clang_target
    triple_u=${target//-/_}
    triple_upper=$(printf '%s' "$triple_u" | tr '[:lower:]' '[:upper:]')
    ndk=$(cygpath -m "$ANDROID_NDK_HOME")
    prebuilt="$ndk/toolchains/llvm/prebuilt/windows-x86_64"
    bin="$prebuilt/bin"
    [ -x "$bin/clang.exe" ] || {
        echo "build-quiche.sh: $bin/clang.exe is missing." >&2
        echo "                 Windows drives the NDK toolchain directly instead of through" >&2
        echo "                 cargo-ndk: cargo-ndk hands BoringSSL an extension-less compiler" >&2
        echo "                 path and CMake refuses it on this host." >&2
        exit 1
    }
    resource=$(ls -d "$prebuilt"/lib/clang/*/include 2>/dev/null | sort | tail -1) || resource=""
    [ -n "$resource" ] || {
        echo "build-quiche.sh: no clang resource headers under $prebuilt/lib/clang." >&2
        echo "                 bindgen loads Visual Studio's libclang here, which looks for" >&2
        echo "                 stddef.h next to its own binary, so the NDK's copy has to be" >&2
        echo "                 pointed at explicitly." >&2
        exit 1
    }
    clang_target=$(android_clang_target_of "$target")

    export ANDROID_NDK_HOME="$ndk"
    export "CC_$triple_u=$bin/clang.exe"
    export "CXX_$triple_u=$bin/clang++.exe"
    export "AR_$triple_u=$bin/llvm-ar.exe"
    export "CFLAGS_$triple_u=--target=$clang_target"
    export "CXXFLAGS_$triple_u=--target=$clang_target"
    export "CARGO_TARGET_${triple_upper}_LINKER=$bin/clang.exe"
    export "CARGO_TARGET_${triple_upper}_RUSTFLAGS=-Clink-arg=--target=$clang_target"
    BINDGEN_EXTRA_CLANG_ARGS="$(bindgen_args_single_ndk_resource "$clang_target" "$resource")"
    export BINDGEN_EXTRA_CLANG_ARGS
    prefer_runtime_checks "$target"

    cargo build --release --target "$target" -p quiche --features ffi >/dev/null
}

build_target() {
    local target=$1
    local out="$PREFIX/$target"
    local stamp="$out/.stamp"
    local artifact want
    artifact=$(artifact_of "$target")
    want="$QUICHE_COMMIT"
    is_msvc_target "$target" && want="$QUICHE_COMMIT+crt-static"
    rust_checks_wanted && want="$want+checks"

    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$want" ] && [ -f "$out/$artifact" ]; then
        echo "[ok]      quiche $QUICHE_VERSION ($target)"
        return 0
    fi

    rustup target add "$target" >/dev/null 2>&1 || true
    if is_msvc_target "$target"; then
        prefer_msvc_tools
        prefer_nasm
        refuse_ninja_generator
        prefer_static_crt
    fi
    prefer_runtime_checks "$target"

    echo "[build]   quiche $QUICHE_VERSION ($target)..."
    if is_android_target "$target"; then
        [ -n "${ANDROID_NDK_HOME:-}" ] || {
            echo "build-quiche.sh: ANDROID_NDK_HOME is not set." >&2
            exit 1
        }
        if on_windows_host; then
            (
                cd "$SRC"
                prefer_msvc_tools
                prefer_android_sdk_cmake
                build_android_with_ndk_clang "$target"
            )
        else
            command -v cargo-ndk >/dev/null 2>&1 || {
                echo "build-quiche.sh: 'cargo-ndk' is required for Android targets." >&2
                echo "                 cargo install cargo-ndk" >&2
                exit 1
            }
            (
                cd "$SRC"
                cargo ndk --target "$(android_abi_of "$target")" --platform "$ANDROID_API" \
                    -- build --release -p quiche --features ffi >/dev/null
            )
        fi
    else
        (
            cd "$SRC"
            case "$target" in
            *-apple-ios*)
                export IPHONEOS_DEPLOYMENT_TARGET="${IPHONEOS_DEPLOYMENT_TARGET:-17.0}"
                ;;
            esac
            cargo build --release --target "$target" -p quiche --features ffi >/dev/null
        )
    fi

    local built="$SRC/target/$target/release/$artifact"
    [ -f "$built" ] || {
        echo "build-quiche.sh: cargo reported success but $built is missing." >&2
        echo "                 Built artifacts for $target:" >&2
        ls "$SRC/target/$target/release/" 2>/dev/null | sed 's/^/                   /' >&2
        exit 1
    }

    mkdir -p "$out"
    cp "$built" "$out/$artifact"
    echo "$want" >"$stamp"
    echo "[ok]      quiche $QUICHE_VERSION ($target) → third_party/quiche/$target/$artifact"
}

REQUESTED=("$@")
[ ${#REQUESTED[@]} -gt 0 ] || REQUESTED=(host)

TARGETS=()
while IFS= read -r target; do
    TARGETS+=("$target")
done < <(resolve_targets "${REQUESTED[@]}")

fetch_source
mkdir -p "$PREFIX/include"
cp "$SRC/quiche/include/quiche.h" "$PREFIX/include/quiche.h"

for target in "${TARGETS[@]}"; do
    build_target "$target"
done

make_macos_universal() {
    [ "$(uname -s)" = "Darwin" ] || return 0
    local arm="$PREFIX/aarch64-apple-darwin/libquiche.a"
    local x64="$PREFIX/x86_64-apple-darwin/libquiche.a"
    [ -f "$arm" ] && [ -f "$x64" ] || return 0
    mkdir -p "$PREFIX/macos-universal"
    lipo -create "$arm" "$x64" -output "$PREFIX/macos-universal/libquiche.a"
    echo "[ok]      quiche $QUICHE_VERSION (macos-universal) = arm64 + x86_64"
}
make_macos_universal

export_boringssl_headers
