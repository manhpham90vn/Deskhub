#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/tools.sh

TAG=${1:?usage: publish-winget.sh vX.Y.Z}
VERSION=${TAG#v}
RELEASE_URL="https://github.com/manhpham90vn/Deskhub/releases/download/$TAG"
NOTES_URL="https://github.com/manhpham90vn/Deskhub/releases/tag/$TAG"

: "${WINGET_TOKEN:?publish-winget.sh: WINGET_TOKEN must hold a classic PAT with the public_repo scope - komac forks microsoft/winget-pkgs with it and opens the pull request from that fork.}"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/deskhub-winget.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

install_komac() {
    local version sha tarball
    version=$(deskhub_pinned_value KOMAC_VERSION)
    sha=$(deskhub_pinned_value KOMAC_LINUX_SHA256)
    tarball="$WORK/komac.tar.gz"
    curl -fsSL -o "$tarball" \
        "https://github.com/russellbanks/Komac/releases/download/v$version/komac-$version-x86_64-unknown-linux-gnu.tar.gz"
    deskhub_verify_sha256 "$tarball" "$sha" || {
        echo "publish-winget.sh: komac $version does not match KOMAC_LINUX_SHA256 in scripts/pinned-versions.txt." >&2
        exit 1
    }
    tar -xzf "$tarball" -C "$WORK" komac
}

manifest_path() {
    local publisher=${1%%.*}
    local package=${1#*.}
    local initial
    initial=$(printf '%s' "${publisher:0:1}" | tr '[:upper:]' '[:lower:]')
    printf 'manifests/%s/%s/%s' "$initial" "$publisher" "$package"
}

package_is_in_winget() {
    GH_TOKEN=$WINGET_TOKEN gh api "repos/microsoft/winget-pkgs/contents/$(manifest_path "$1")" --silent 2>/dev/null
}

submit() {
    local id=$1 url=$2
    if ! package_is_in_winget "$id"; then
        echo "::warning::$id is not in microsoft/winget-pkgs yet - submit the first version by hand with 'komac new $id' (docs/BUILD.md, Releasing); every later tag updates it from here."
        return 0
    fi
    "$WORK/komac" update "$id" \
        --version "$VERSION" \
        --urls "$url" \
        --release-notes-url "$NOTES_URL" \
        --token "$WINGET_TOKEN" \
        --submit
}

install_komac
submit ManhPham.Deskhub "$RELEASE_URL/deskhub-$TAG-windows.exe"
submit ManhPham.DeskhubCLI "$RELEASE_URL/deskhub-cli-$TAG-windows.exe"
