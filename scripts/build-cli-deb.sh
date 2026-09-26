#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

for tool in dpkg-deb dpkg-shlibdeps dpkg; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-cli-deb.sh: '$tool' is required (sudo apt install dpkg-dev)." >&2
        exit 1
    }
done

VERSION="$(tr -d '[:space:]' < VERSION)"
ARCH="$(dpkg --print-architecture)"
DIST=out/dist/linux
STAGE="$DIST/cli-deb"
DEB="$DIST/deskhub-cli_${VERSION}_${ARCH}.deb"

rm -rf "$STAGE" "$DEB"
scripts/stage-linux-cli-pkgroot.sh "$STAGE"

mkdir -p "$STAGE/debian"
touch "$STAGE/debian/control"
DEPENDS="$(cd "$STAGE" && dpkg-shlibdeps -O usr/bin/deskhub-cli | sed 's/^shlibs:Depends=//')"
rm -rf "$STAGE/debian"

INSTALLED_SIZE="$(du -sk "$STAGE" | cut -f1)"
mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: deskhub-cli
Version: $VERSION
Architecture: $ARCH
Maintainer: Manh Pham <manhpv151090@gmail.com>
Section: net
Priority: optional
Installed-Size: $INSTALLED_SIZE
Depends: $DEPENDS
Recommends: xdg-desktop-portal, va-driver-all
Replaces: deskhub (<< $VERSION)
Breaks: deskhub (<< $VERSION)
Homepage: https://github.com/manhpham90vn/Deskhub
Description: Command-line client for DeskHub LAN remote desktop
 DeskHub CLI shares screens, connects to hosts, and opens remote shells
 without a graphical user interface. The package also installs the udev
 rule needed for remote keyboard and mouse input.
EOF

cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "configure" ] && command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    modprobe uinput 2>/dev/null || true
    udevadm trigger --name-match=uinput || true
fi
exit 0
EOF

cat > "$STAGE/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload-rules || true
    fi
fi
exit 0
EOF

chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
dpkg-deb --build --root-owner-group "$STAGE" "$DEB" >/dev/null
rm -rf "$STAGE"
echo "[ok]      $DEB"
