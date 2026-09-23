#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/tools.sh

TAG=${1:?usage: publish-homebrew.sh vX.Y.Z}
VERSION=${TAG#v}
TAP_REPO=${TAP_REPO:-manhpham90vn/homebrew-tap}
TAP_BRANCH=${TAP_BRANCH:-main}

: "${HOMEBREW_TAP_TOKEN:?publish-homebrew.sh: HOMEBREW_TAP_TOKEN must hold a token with contents write access to $TAP_REPO.}"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/deskhub-homebrew.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

DMG="deskhub-$TAG-macos.dmg"
CLI="deskhub-cli-$TAG-macos"

sha256_of() {
    sha256sum "$1" | cut -d' ' -f1
}

render() {
    sed -e "s/@VERSION@/$VERSION/g" -e "s/@SHA256@/$2/g" "packaging/homebrew/$1"
}

deskhub_release_asset "$TAG" "$DMG" "$WORK/assets"
deskhub_release_asset "$TAG" "$CLI" "$WORK/assets"

git clone --depth 1 "https://x-access-token:$HOMEBREW_TAP_TOKEN@github.com/$TAP_REPO.git" "$WORK/tap"
mkdir -p "$WORK/tap/Casks" "$WORK/tap/Formula"
render deskhub.rb "$(sha256_of "$WORK/assets/$DMG")" > "$WORK/tap/Casks/deskhub.rb"
render deskhub-cli.rb "$(sha256_of "$WORK/assets/$CLI")" > "$WORK/tap/Formula/deskhub-cli.rb"

cd "$WORK/tap"
git add Casks/deskhub.rb Formula/deskhub-cli.rb
if git diff --cached --quiet; then
    echo "[ok]      $TAP_REPO already at $VERSION"
    exit 0
fi
git -c user.name="github-actions[bot]" \
    -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
    commit -m "deskhub $VERSION"
git push origin "HEAD:refs/heads/$TAP_BRANCH"
echo "[ok]      $TAP_REPO -> $VERSION"
