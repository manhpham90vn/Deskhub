#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

read -ra BUNDLE_IDS <<<"${DESKHUB_BUNDLE_ID:-com.deskhub.macos com.deskhub.macos.debug}"
CHOSEN_BUNDLE_IDS=()
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
PURGE=0

usage() {
    cat <<EOF
usage: scripts/reset-macos-permissions.sh [--purge] [--bundle-id ID]

  --purge          also delete the local build output (out/build/macos,
                   out/dist/macos) so only one copy of the app is left.
                   Never touches /Applications.
  --bundle-id ID   reset only this bundle id; repeat it for several.
                   default: ${BUNDLE_IDS[*]} (the Release and Debug builds)

After it runs, relaunch the ONE copy you want to use and grant the prompts again.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
    --purge) PURGE=1 ;;
    --bundle-id)
        shift
        CHOSEN_BUNDLE_IDS+=("${1:?--bundle-id needs a value}")
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "reset-macos-permissions.sh: unknown argument '$1'" >&2
        usage >&2
        exit 1
        ;;
    esac
    shift
done

if [ ${#CHOSEN_BUNDLE_IDS[@]} -gt 0 ]; then
    BUNDLE_IDS=("${CHOSEN_BUNDLE_IDS[@]}")
fi

if [ "$(uname)" != Darwin ]; then
    echo "reset-macos-permissions.sh: macOS only." >&2
    exit 1
fi

for bundle_id in "${BUNDLE_IDS[@]}"; do
    echo "==> quitting $bundle_id"
    osascript -e "quit app id \"$bundle_id\"" 2>/dev/null || true
done
pkill -f '/(app|Deskhub)\.app/Contents/MacOS/' 2>/dev/null || true

for bundle_id in "${BUNDLE_IDS[@]}"; do
    echo "==> resetting privacy grants for $bundle_id"
    for service in Accessibility ScreenCapture ListenEvent PostEvent Microphone Camera; do
        if tccutil reset "$service" "$bundle_id" >/dev/null 2>&1; then
            echo "    $service: cleared"
        else
            echo "    $service: nothing to clear"
        fi
    done
    tccutil reset All "$bundle_id" >/dev/null 2>&1 || true
done

echo "==> installed copies known to Launch Services"
COPIES=""
for bundle_id in "${BUNDLE_IDS[@]}"; do
    found=$(mdfind "kMDItemCFBundleIdentifier == '$bundle_id'" 2>/dev/null || true)
    [ -n "$found" ] && COPIES+="$found"$'\n'
done
COPIES=${COPIES%$'\n'}
if [ -z "$COPIES" ]; then
    echo "    none found - Spotlight may not have indexed out/, that is fine"
else
    while IFS= read -r app; do
        [ -n "$app" ] || continue
        info=$(codesign -dv --verbose=2 "$app" 2>&1 || true)
        authority=$(awk -F'=' '/^Authority=/ {print $2; exit}' <<<"$info")
        if [ -z "$authority" ]; then
            case "$info" in
            *Signature=adhoc*) authority="ad-hoc" ;;
            esac
        fi
        echo "    ${app}  [${authority:-unsigned}]"
    done <<<"$COPIES"
fi

if [ "$PURGE" -eq 1 ]; then
    echo "==> purging local build output"
    for stale in out/build/macos out/dist/macos; do
        [ -e "$stale" ] || continue
        find "$stale" -maxdepth 3 -name '*.app' -print0 2>/dev/null |
            xargs -0 -I{} "$LSREGISTER" -u "{}" 2>/dev/null || true
        rm -rf "$stale"
        echo "    removed $stale"
    done
fi

cat <<EOF

==> done
Keep ONE copy of the app around - either the local build or the downloaded one.
Launch it, then grant Screen Recording and Accessibility when it asks; both
permissions only take effect for the exact binary that asked for them.

If peer discovery still fails, toggle the app off/on under
System Settings > Privacy & Security > Local Network - tccutil cannot reset that one.
EOF
