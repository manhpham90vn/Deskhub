#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

for tool in rpmbuild rpm; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-rpm.sh: '$tool' is required (sudo apt install rpm)." >&2
        exit 1
    }
done

VERSION="$(tr -d '[:space:]' < VERSION)"
ARCH="$(uname -m)"
DIST=out/dist/linux
PKGROOT="$DIST/rpmroot"
TOP="$DIST/rpmbuild"
RPM="$DIST/deskhub-${VERSION}-1.${ARCH}.rpm"

rm -rf "$PKGROOT" "$TOP" "$RPM"
scripts/stage-linux-pkgroot.sh "$PKGROOT"

mkdir -p "$TOP/SPECS"
cat > "$TOP/SPECS/deskhub.spec" <<EOF
%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none

Name: deskhub
Version: $VERSION
Release: 1
Summary: LAN remote desktop - share and control screens
License: MIT
URL: https://github.com/manhpham90vn/Deskhub
Recommends: xdg-desktop-portal

%description
Deskhub shares the screen of one machine and controls it from another
over UDP on the local network.

This package contains the Linux app (host + viewer role) and the udev
rule that grants access to /dev/uinput, so remote mouse and keyboard
input works without any manual permission setup.

%install
cp -a %{pkgroot}/. %{buildroot}/

%post
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || :
    modprobe uinput 2>/dev/null || :
    udevadm trigger --name-match=uinput || :
fi

%postun
if [ "\$1" = 0 ] && command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || :
fi

%files
/usr/bin/deskhub
/usr/share/applications/deskhub.desktop
/usr/share/icons/hicolor/256x256/apps/deskhub.png
/usr/share/icons/hicolor/512x512/apps/deskhub.png
/usr/lib/udev/rules.d/60-deskhub-uinput.rules
/usr/lib/modules-load.d/deskhub.conf
%doc /usr/share/doc/deskhub/copyright
%doc /usr/share/doc/deskhub/THIRD_PARTY_NOTICES.md
%license /usr/share/doc/deskhub/LGPL-2.1.txt
EOF

rpmbuild -bb "$TOP/SPECS/deskhub.spec" \
    --define "_topdir $PWD/$TOP" \
    --define "pkgroot $PWD/$PKGROOT" \
    --quiet

mv "$TOP/RPMS/$ARCH/deskhub-${VERSION}-1.${ARCH}.rpm" "$RPM"
rm -rf "$PKGROOT" "$TOP"

rpm -qp --requires "$RPM" | grep -q '^libgtk-3\.so\.0' || {
    echo "build-rpm.sh: $RPM is missing its automatic library requires." >&2
    exit 1
}

echo "[ok]      $RPM"
