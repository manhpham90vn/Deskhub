#!/usr/bin/env bash
set -euo pipefail

APT_UPDATE_TIMEOUT=${APT_UPDATE_TIMEOUT:-180}
APT_INSTALL_TIMEOUT=${APT_INSTALL_TIMEOUT:-480}
APT_TOTAL_BUDGET=${APT_TOTAL_BUDGET:-900}
MIRROR_PROBE_TIMEOUT=${MIRROR_PROBE_TIMEOUT:-12}

APT_OPTS=(
    -o Acquire::Retries=3
    -o Acquire::http::Timeout=20
    -o Acquire::https::Timeout=20
    -o Acquire::ForceIPv4=true
    -o DPkg::Lock::Timeout=120
)

FALLBACK_MIRRORS=(
    http://archive.ubuntu.com/ubuntu/
    http://mirrors.edge.kernel.org/ubuntu/
    http://ftp.jaist.ac.jp/pub/Linux/ubuntu/
    http://mirror.bizflycloud.vn/ubuntu/
)

ARCHIVE_URI_PATTERN='https?://[a-z0-9.-]*(archive|security)\.ubuntu\.com/ubuntu/?'
for mirror in "${FALLBACK_MIRRORS[@]}"; do
    ARCHIVE_URI_PATTERN="$ARCHIVE_URI_PATTERN|$(printf '%s' "${mirror%/}" | sed -E 's|\.|\\.|g')/?"
done

RELEASE_CODENAME=$(
    . /etc/os-release 2>/dev/null || true
    printf '%s' "${VERSION_CODENAME:-}"
)

declare -a missing=()
for pkg in "$@"; do
    case "$pkg" in
    ninja-build) cmd=ninja ;;
    clang) cmd=clang ;;
    llvm) cmd=llvm-cov ;;
    shellcheck) cmd=shellcheck ;;
    rpm) cmd=rpmbuild ;;
    apt-utils) cmd=apt-ftparchive ;;
    gnupg) cmd=gpg ;;
    *) cmd=$pkg ;;
    esac
    [ "$(dpkg-query -W -f='${db:Status-Status}' "$pkg" 2>/dev/null || true)" = installed ] && continue
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$pkg")
done

if [ ${#missing[@]} -eq 0 ]; then
    echo "apt-install.sh: already present, skipping apt: $*"
    exit 0
fi

echo "apt-install.sh: installing ${missing[*]}"

apt_get_under_root_timeout() {
    local limit=$1 left=$((APT_TOTAL_BUDGET - SECONDS))
    shift
    if [ "$left" -lt 30 ]; then
        echo "apt-install.sh: ${APT_TOTAL_BUDGET}s budget spent, not starting another apt-get." >&2
        return 1
    fi
    [ "$limit" -le "$left" ] || limit=$left
    sudo DEBIAN_FRONTEND=noninteractive timeout --kill-after=10 "$limit" \
        apt-get "${APT_OPTS[@]}" "$@"
}

install_from_the_current_sources() {
    apt_get_under_root_timeout "$APT_UPDATE_TIMEOUT" update -q &&
        apt_get_under_root_timeout "$APT_INSTALL_TIMEOUT" \
            install -y --no-install-recommends "${missing[@]}"
}

mirror_carries_this_release() {
    command -v curl >/dev/null 2>&1 || return 0
    [ -n "$RELEASE_CODENAME" ] || return 0
    curl -fsS --max-time "$MIRROR_PROBE_TIMEOUT" -o /dev/null \
        "$1dists/$RELEASE_CODENAME/Release"
}

point_the_sources_at() {
    local mirror=$1 list switched=0
    for list in /etc/apt/sources.list /etc/apt/apt-mirrors.txt \
        /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
        [ -f "$list" ] || continue
        grep -qE "$ARCHIVE_URI_PATTERN" "$list" || continue
        [ -f "$list.deskhub-original" ] || sudo cp -p "$list" "$list.deskhub-original"
        sudo sed -i -E "s#$ARCHIVE_URI_PATTERN#$mirror#g" "$list"
        switched=1
    done
    [ "$switched" -eq 1 ]
}

if install_from_the_current_sources; then
    exit 0
fi
echo "apt-install.sh: the configured mirror did not deliver; rotating." >&2

for mirror in "${FALLBACK_MIRRORS[@]}"; do
    if ! mirror_carries_this_release "$mirror"; then
        echo "apt-install.sh: $mirror has no $RELEASE_CODENAME/Release, skipping it." >&2
        continue
    fi
    if ! point_the_sources_at "$mirror"; then
        echo "apt-install.sh: no Ubuntu archive entry to rewrite in sources.list, apt-mirrors.txt (where GitHub runners keep the azure mirror behind mirror+file) or sources.list.d; the sources are not ours." >&2
        break
    fi
    echo "apt-install.sh: retrying against $mirror" >&2
    sleep 5
    if install_from_the_current_sources; then
        exit 0
    fi
done

echo "apt-install.sh: giving up on ${missing[*]}; every mirror in ${FALLBACK_MIRRORS[*]} failed." >&2
echo "apt-install.sh: each rewritten sources file is kept beside itself as .deskhub-original;" >&2
echo "apt-install.sh: apt ignores that suffix, so restoring is a plain mv." >&2
echo "apt-install.sh: timeout runs under sudo, not around it: killing an unprivileged sudo" >&2
echo "apt-install.sh: leaves apt-get alive as root, holding the dpkg lock and the step's" >&2
echo "apt-install.sh: log pipe, so every later attempt blocks and the job never finishes." >&2
echo "apt-install.sh: apt runs at -q, never -qq, because -qq swallows the Err: lines that" >&2
echo "apt-install.sh: say which mirror stalled; a silent failure here is unfixable." >&2
echo "apt-install.sh: apt is told to time out its own transfers in ${APT_OPTS[*]}, and the" >&2
echo "apt-install.sh: whole rotation is capped at ${APT_TOTAL_BUDGET}s; without both, a stalled mirror" >&2
echo "apt-install.sh: costs more than the build it was meant to unblock." >&2
exit 1
