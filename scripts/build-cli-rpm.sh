#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

for tool in rpmbuild rpm; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-cli-rpm.sh: '$tool' is required (sudo apt install rpm)." >&2
        exit 1
    }
done

VERSION="$(tr -d '[:space:]' < VERSION)"
ARCH="$(uname -m)"
DIST=out/dist/linux
PKGROOT="$DIST/cli-rpmroot"
TOP="$DIST/cli-rpmbuild"
RPM="$DIST/deskhub-cli-${VERSION}-1.${ARCH}.rpm"

rm -rf "$PKGROOT" "$TOP" "$RPM"
scripts/stage-linux-cli-pkgroot.sh "$PKGROOT"

mkdir -p "$TOP/SPECS"
cat > "$TOP/SPECS/deskhub-cli.spec" <<EOF
%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none

Name: deskhub-cli
Version: $VERSION
Release: 1
Summary: Command-line client for DeskHub LAN remote desktop
License: MIT
URL: https://github.com/manhpham90vn/Deskhub
Recommends: xdg-desktop-portal
Conflicts: deskhub < $VERSION

%description
DeskHub CLI shares screens, connects to hosts, and opens remote shells
without a graphical user interface. It also installs the udev rule needed
for remote keyboard and mouse input.

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
/usr/bin/deskhub-cli
/usr/lib/udev/rules.d/60-deskhub-cli-uinput.rules
/usr/lib/modules-load.d/deskhub-cli.conf
%doc /usr/share/doc/deskhub-cli/copyright
%doc /usr/share/doc/deskhub-cli/THIRD_PARTY_NOTICES.md
%license /usr/share/doc/deskhub-cli/LGPL-2.1.txt
EOF

rpmbuild -bb "$TOP/SPECS/deskhub-cli.spec" \
    --define "_topdir $PWD/$TOP" \
    --define "pkgroot $PWD/$PKGROOT" \
    --quiet

mv "$TOP/RPMS/$ARCH/deskhub-cli-${VERSION}-1.${ARCH}.rpm" "$RPM"
rm -rf "$PKGROOT" "$TOP"

if rpm -qp --requires "$RPM" | grep -q '^libgtk-3\.so\.0'; then
    echo "build-cli-rpm.sh: $RPM unexpectedly depends on GTK." >&2
    exit 1
fi

echo "[ok]      $RPM"
