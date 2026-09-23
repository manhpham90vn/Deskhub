#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/tools.sh

OUT=${1:?usage: build-apt-repo.sh OUT_DIR}
KEEP_RELEASES=${APT_KEEP_RELEASES:-3}
REPO=manhpham90vn/Deskhub
SUITE=stable
COMPONENT=main
ARCH=amd64

for tool in apt-ftparchive gpg gpgv gh; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-apt-repo.sh: '$tool' is required (sudo apt install apt-utils gnupg gpgv gh)." >&2
        exit 1
    }
done

: "${APT_GPG_PRIVATE_KEY:?build-apt-repo.sh: APT_GPG_PRIVATE_KEY must hold the ASCII-armored signing key - every user already trusts its public half, so a new key breaks their apt update.}"

GNUPGHOME=$(mktemp -d "${TMPDIR:-/tmp}/deskhub-apt-gpg.XXXXXX")
export GNUPGHOME
trap 'rm -rf "$GNUPGHOME"' EXIT

sign() {
    gpg --batch --yes --pinentry-mode loopback --passphrase "${APT_GPG_PASSPHRASE:-}" \
        --local-user "$KEY_FPR" "$@"
}

fetch_debs() {
    local pool=$1 tag
    gh release list --repo "$REPO" --exclude-drafts --exclude-pre-releases \
        --limit "$KEEP_RELEASES" --json tagName --jq '.[].tagName' |
        while read -r tag; do
            deskhub_release_asset "$tag" "deskhub-$tag-$ARCH.deb" "$pool" ||
                echo "::warning::$tag has no deskhub-$tag-$ARCH.deb - left out of the apt repo"
        done
    compgen -G "$pool/*.deb" >/dev/null || {
        echo "build-apt-repo.sh: none of the last $KEEP_RELEASES releases carries a $ARCH deb." >&2
        exit 1
    }
}

write_release() {
    local dist=$1
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin=Deskhub \
        -o APT::FTPArchive::Release::Label=Deskhub \
        -o APT::FTPArchive::Release::Suite="$SUITE" \
        -o APT::FTPArchive::Release::Codename="$SUITE" \
        -o APT::FTPArchive::Release::Architectures="$ARCH" \
        -o APT::FTPArchive::Release::Components="$COMPONENT" \
        -o APT::FTPArchive::Release::Description="Deskhub LAN remote desktop" \
        release "$dist" > "$GNUPGHOME/Release"
    mv "$GNUPGHOME/Release" "$dist/Release"
}

printf '%s\n' "$APT_GPG_PRIVATE_KEY" | gpg --batch --import
KEY_FPR=$(gpg --list-secret-keys --with-colons | awk -F: '/^fpr:/ { print $10; exit }')

rm -rf "$OUT"
POOL="$OUT/pool/$COMPONENT/d/deskhub"
BINARY="$OUT/dists/$SUITE/$COMPONENT/binary-$ARCH"
mkdir -p "$POOL" "$BINARY"
fetch_debs "$POOL"

(cd "$OUT" && apt-ftparchive packages "pool/$COMPONENT") > "$BINARY/Packages"
gzip -9 --keep "$BINARY/Packages"
write_release "$OUT/dists/$SUITE"

sign --clearsign --output "$OUT/dists/$SUITE/InRelease" "$OUT/dists/$SUITE/Release"
sign --armor --detach-sign --output "$OUT/dists/$SUITE/Release.gpg" "$OUT/dists/$SUITE/Release"
gpg --export "$KEY_FPR" > "$OUT/deskhub.gpg"
gpg --armor --export "$KEY_FPR" > "$OUT/deskhub.asc"
touch "$OUT/.nojekyll"

gpgv --keyring "$OUT/deskhub.gpg" "$OUT/dists/$SUITE/InRelease" 2>/dev/null || {
    echo "build-apt-repo.sh: InRelease does not verify against the exported deskhub.gpg." >&2
    exit 1
}

echo "[ok]      $OUT ($(grep -c '^Package:' "$BINARY/Packages") package versions, key $KEY_FPR)"
