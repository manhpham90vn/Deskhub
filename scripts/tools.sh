#!/usr/bin/env bash

deskhub_repo_root() {
    local source_path
    source_path="${BASH_SOURCE[0]:-$0}"
    (cd "$(dirname "$source_path")/.." && pwd)
}

deskhub_tools_dir() {
    printf '%s/tools' "$(deskhub_repo_root)"
}

deskhub_pinned_value() {
    sed -n "s/^$1=//p" "$(deskhub_repo_root)/scripts/pinned-versions.txt"
}

deskhub_verify_sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        printf '%s  %s' "$2" "$1" | sha256sum --check --status -
    else
        printf '%s  %s' "$2" "$1" | shasum -a 256 --check --status -
    fi
}

deskhub_tools_venv_dir() {
    printf '%s/venv' "$(deskhub_tools_dir)"
}

deskhub_tools_venv_bin() {
    local venv
    venv="$(deskhub_tools_venv_dir)"
    if [ -d "$venv/Scripts" ]; then
        printf '%s' "$venv/Scripts"
    elif [ -d "$venv/bin" ]; then
        printf '%s' "$venv/bin"
    else
        case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN* | Windows_NT) printf '%s' "$venv/Scripts" ;;
        *) printf '%s' "$venv/bin" ;;
        esac
    fi
}

deskhub_venv_tool() {
    local bindir
    bindir="$(deskhub_tools_venv_bin)"
    if [ -x "$bindir/$1" ]; then
        printf '%s' "$bindir/$1"
    elif [ -x "$bindir/$1.exe" ]; then
        printf '%s' "$bindir/$1.exe"
    fi
}

deskhub_host_python() {
    if command -v python3 >/dev/null 2>&1 && python3 --version >/dev/null 2>&1; then
        command -v python3
    else
        command -v python
    fi
}

deskhub_pinned_path_tool() {
    if command -v "$1" >/dev/null 2>&1 && "$1" --version 2>/dev/null | grep -qF "$2"; then
        command -v "$1"
    fi
}

ensure_local_tools_dir() {
    mkdir -p "$(deskhub_tools_dir)"
}

ensure_local_ktlint() {
    local tools version sha jar current
    tools="$(deskhub_tools_dir)"
    version="$(deskhub_pinned_value KTLINT_VERSION)"
    sha="$(deskhub_pinned_value KTLINT_SHA256)"
    jar="$tools/ktlint.jar"
    current="$(cat "$tools/ktlint.jar.version" 2>/dev/null || true)"
    if [ -f "$jar" ] && [ "$current" = "$version" ]; then
        echo "[ok]      ktlint $version ($jar)"
        return 0
    fi
    echo "[install] ktlint $version..."
    ensure_local_tools_dir
    curl -fsSL -o "$jar" "https://github.com/pinterest/ktlint/releases/download/$version/ktlint"
    deskhub_verify_sha256 "$jar" "$sha" || {
        echo "tools.sh: ktlint download failed the checksum check." >&2
        rm -f "$jar" "$tools/ktlint.jar.version"
        return 1
    }
    printf '%s' "$version" >"$tools/ktlint.jar.version"
    echo "[ok]      ktlint $version ($jar)"
}

ensure_local_swiftformat() {
    local tools version bin asset inner sha
    tools="$(deskhub_tools_dir)"
    version="$(deskhub_pinned_value SWIFTFORMAT_VERSION)"
    if command -v swiftformat >/dev/null 2>&1; then
        echo "[ok]      swiftformat ($(command -v swiftformat))"
        return 0
    fi
    bin="$tools/swiftformat"
    if [ -x "$bin" ] && [ "$("$bin" --version 2>/dev/null || true)" = "$version" ]; then
        echo "[ok]      swiftformat $version ($bin)"
        return 0
    fi
    echo "[install] SwiftFormat $version..."
    ensure_local_tools_dir
    case "$(uname -s)" in
    Darwin) asset=swiftformat.zip inner=swiftformat sha="$(deskhub_pinned_value SWIFTFORMAT_MACOS_SHA256)" ;;
    *) asset=swiftformat_linux.zip inner=swiftformat_linux sha="$(deskhub_pinned_value SWIFTFORMAT_LINUX_SHA256)" ;;
    esac
    curl -fsSL -o "$tools/swiftformat.zip" "https://github.com/nicklockwood/SwiftFormat/releases/download/$version/$asset"
    deskhub_verify_sha256 "$tools/swiftformat.zip" "$sha" || {
        echo "tools.sh: SwiftFormat download failed the checksum check." >&2
        rm -f "$tools/swiftformat.zip"
        return 1
    }
    unzip -o -q -d "$tools" "$tools/swiftformat.zip" "$inner"
    if [ "$inner" != swiftformat ]; then mv "$tools/$inner" "$bin"; fi
    chmod +x "$bin"
    rm -f "$tools/swiftformat.zip"
    echo "[ok]      SwiftFormat $version ($bin)"
}

ensure_local_clang_tools() {
    local format_version tidy_version venv bindir format_bin tidy_bin host_python path_format path_tidy
    format_version="$(deskhub_pinned_value CLANG_FORMAT_VERSION)"
    tidy_version="$(deskhub_pinned_value CLANG_TIDY_VERSION)"
    format_bin="$(deskhub_venv_tool clang-format || true)"
    tidy_bin="$(deskhub_venv_tool clang-tidy || true)"
    if [ -n "$format_bin" ] && [ -n "$tidy_bin" ] \
        && "$format_bin" --version 2>/dev/null | grep -qF "$format_version" \
        && "$tidy_bin" --version 2>/dev/null | grep -qF "$tidy_version"; then
        echo "[ok]      clang-format $format_version ($format_bin)"
        echo "[ok]      clang-tidy $tidy_version ($tidy_bin)"
        return 0
    fi
    path_format="$(deskhub_pinned_path_tool clang-format "$format_version" || true)"
    path_tidy="$(deskhub_pinned_path_tool clang-tidy "$tidy_version" || true)"
    if [ -n "$path_format" ] && [ -n "$path_tidy" ]; then
        echo "[ok]      clang-format $format_version ($path_format)"
        echo "[ok]      clang-tidy $tidy_version ($path_tidy)"
        return 0
    fi
    echo "[install] clang-format $format_version + clang-tidy $tidy_version (tools/venv)..."
    ensure_local_tools_dir
    venv="$(deskhub_tools_venv_dir)"
    host_python="$(deskhub_host_python || true)"
    if [ -z "$host_python" ]; then
        echo "tools.sh: Python 3 not found - install it, then re-run." >&2
        return 1
    fi
    "$host_python" -m venv "$venv"
    bindir="$(deskhub_tools_venv_bin)"
    "$bindir/python" -m pip install "clang-format==$format_version" "clang-tidy==$tidy_version"
    format_bin="$(deskhub_venv_tool clang-format || true)"
    tidy_bin="$(deskhub_venv_tool clang-tidy || true)"
    if [ -z "$format_bin" ] || [ -z "$tidy_bin" ]; then
        echo "tools.sh: clang tools are still not runnable after their install." >&2
        return 1
    fi
    echo "[ok]      clang-format $format_version ($format_bin)"
    echo "[ok]      clang-tidy $tidy_version ($tidy_bin)"
}

ensure_local_style_tools() {
    ensure_local_clang_tools
    ensure_local_ktlint
    ensure_local_swiftformat
    ensure_local_cppcheck
    ensure_local_detekt
}

resolve_local_clang_format() {
    local version candidate path_tool
    version="$(deskhub_pinned_value CLANG_FORMAT_VERSION)"
    candidate="$(deskhub_venv_tool clang-format || true)"
    if [ -n "$candidate" ] && "$candidate" --version 2>/dev/null | grep -qF "$version"; then
        printf '%s' "$candidate"
        return 0
    fi
    path_tool="$(deskhub_pinned_path_tool clang-format "$version" || true)"
    if [ -n "$path_tool" ]; then
        printf '%s' "$path_tool"
        return 0
    fi
    return 1
}

resolve_local_clang_tidy() {
    local version candidate path_tool
    version="$(deskhub_pinned_value CLANG_TIDY_VERSION)"
    candidate="$(deskhub_venv_tool clang-tidy || true)"
    if [ -n "$candidate" ] && "$candidate" --version 2>/dev/null | grep -qF "$version"; then
        printf '%s' "$candidate"
        return 0
    fi
    path_tool="$(deskhub_pinned_path_tool clang-tidy "$version" || true)"
    if [ -n "$path_tool" ]; then
        printf '%s' "$path_tool"
        return 0
    fi
    return 1
}

resolve_local_swiftformat() {
    local bin
    if command -v swiftformat >/dev/null 2>&1; then
        command -v swiftformat
        return 0
    fi
    bin="$(deskhub_tools_dir)/swiftformat"
    if [ -x "$bin" ]; then
        printf '%s' "$bin"
        return 0
    fi
    return 1
}

deskhub_is_windows() {
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN* | Windows_NT) return 0 ;;
    *) return 1 ;;
    esac
}

deskhub_cppcheck_candidates() {
    command -v cppcheck 2>/dev/null || true
    printf '%s\n' "$(deskhub_tools_dir)/cppcheck/bin/cppcheck"
    if deskhub_is_windows; then printf '%s\n' "/c/Program Files/Cppcheck/cppcheck.exe"; fi
}

resolve_local_cppcheck() {
    local version candidate
    version="$(deskhub_pinned_value CPPCHECK_VERSION)"
    while IFS= read -r candidate; do
        [ -n "$candidate" ] && [ -x "$candidate" ] || continue
        if "$candidate" --version 2>/dev/null | grep -qxF "Cppcheck $version"; then
            printf '%s' "$candidate"
            return 0
        fi
    done <<<"$(deskhub_cppcheck_candidates)"
    return 1
}

ensure_local_cppcheck() {
    local version sha tools src found
    version="$(deskhub_pinned_value CPPCHECK_VERSION)"
    found="$(resolve_local_cppcheck || true)"
    if [ -n "$found" ]; then
        echo "[ok]      cppcheck $version ($found)"
        return 0
    fi
    if deskhub_is_windows; then
        echo "[install] cppcheck $version (winget)..."
        winget.exe install --id Cppcheck.Cppcheck --exact --version "$version" \
            --accept-source-agreements --accept-package-agreements --silent
        found="$(resolve_local_cppcheck || true)"
        [ -n "$found" ] || {
            echo "tools.sh: cppcheck $version is still missing after its winget install." >&2
            return 1
        }
        echo "[ok]      cppcheck $version ($found)"
        return 0
    fi
    echo "[install] cppcheck $version (built from source into tools/cppcheck)..."
    sha="$(deskhub_pinned_value CPPCHECK_SRC_SHA256)"
    tools="$(deskhub_tools_dir)"
    src="$tools/cppcheck-src"
    ensure_local_tools_dir
    curl -fsSL --retry 5 --retry-all-errors -o "$tools/cppcheck.tar.gz" \
        "https://github.com/cppcheck-opensource/cppcheck/archive/refs/tags/$version.tar.gz"
    deskhub_verify_sha256 "$tools/cppcheck.tar.gz" "$sha" || {
        echo "tools.sh: the cppcheck source archive failed the checksum check." >&2
        rm -f "$tools/cppcheck.tar.gz"
        return 1
    }
    rm -rf "$src"
    mkdir -p "$src"
    tar -xzf "$tools/cppcheck.tar.gz" -C "$src" --strip-components=1
    cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$tools/cppcheck" -DBUILD_TESTING=OFF -DBUILD_GUI=OFF >/dev/null
    cmake --build "$src/build"
    cmake --install "$src/build" >/dev/null
    rm -rf "$src" "$tools/cppcheck.tar.gz"
    found="$(resolve_local_cppcheck || true)"
    [ -n "$found" ] || {
        echo "tools.sh: cppcheck $version built but does not report its pinned version." >&2
        return 1
    }
    echo "[ok]      cppcheck $version ($found)"
}

ensure_local_detekt() {
    local tools version sha jar current
    tools="$(deskhub_tools_dir)"
    version="$(deskhub_pinned_value DETEKT_VERSION)"
    sha="$(deskhub_pinned_value DETEKT_SHA256)"
    jar="$tools/detekt-cli.jar"
    current="$(cat "$tools/detekt-cli.jar.version" 2>/dev/null || true)"
    if [ -f "$jar" ] && [ "$current" = "$version" ]; then
        echo "[ok]      detekt $version ($jar)"
        return 0
    fi
    echo "[install] detekt $version..."
    ensure_local_tools_dir
    curl -fsSL --retry 5 --retry-all-errors -o "$jar" \
        "https://github.com/detekt/detekt/releases/download/v$version/detekt-cli-$version-all.jar"
    deskhub_verify_sha256 "$jar" "$sha" || {
        echo "tools.sh: detekt download failed the checksum check." >&2
        rm -f "$jar" "$tools/detekt-cli.jar.version"
        return 1
    }
    printf '%s' "$version" >"$tools/detekt-cli.jar.version"
    echo "[ok]      detekt $version ($jar)"
}

ensure_local_periphery() {
    local tools version sha dir current
    tools="$(deskhub_tools_dir)"
    version="$(deskhub_pinned_value PERIPHERY_VERSION)"
    sha="$(deskhub_pinned_value PERIPHERY_SHA256)"
    dir="$tools/periphery"
    current="$(cat "$dir/.version" 2>/dev/null || true)"
    if [ -x "$dir/periphery" ] && [ "$current" = "$version" ]; then
        echo "[ok]      periphery $version ($dir/periphery)"
        return 0
    fi
    echo "[install] periphery $version..."
    ensure_local_tools_dir
    curl -fsSL --retry 5 --retry-all-errors -o "$tools/periphery.zip" \
        "https://github.com/peripheryapp/periphery/releases/download/$version/periphery-$version.zip"
    deskhub_verify_sha256 "$tools/periphery.zip" "$sha" || {
        echo "tools.sh: periphery download failed the checksum check." >&2
        rm -f "$tools/periphery.zip"
        return 1
    }
    rm -rf "$dir"
    mkdir -p "$dir"
    unzip -o -q -d "$dir" "$tools/periphery.zip"
    chmod +x "$dir/periphery"
    rm -f "$tools/periphery.zip"
    printf '%s' "$version" >"$dir/.version"
    echo "[ok]      periphery $version ($dir/periphery)"
}
