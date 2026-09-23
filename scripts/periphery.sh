#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

. scripts/tools.sh

[ "$(uname -s)" = Darwin ] || {
    echo "periphery.sh: Swift dead-code analysis builds the Xcode projects, so it needs macOS + Xcode." >&2
    exit 1
}

ensure_local_periphery
PERIPHERY="$(deskhub_tools_dir)/periphery/periphery"

scan() {
    local project=$1 sdk=$2 name=$3
    local derived="out/periphery/$name"
    echo "[periphery] $project ($sdk)"
    mkdir -p "$derived"
    if ! xcodebuild -project "$project" -scheme app -configuration Debug -sdk "$sdk" \
        -derivedDataPath "$derived" CODE_SIGNING_ALLOWED=NO build >"$derived/build.log" 2>&1; then
        tail -n 60 "$derived/build.log"
        echo "periphery.sh: the $name build failed, so there is no index to analyse - see $derived/build.log." >&2
        exit 1
    fi
    "$PERIPHERY" scan --project "$project" --schemes app --skip-build \
        --index-store-path "$derived/Index.noindex/DataStore" \
        --retain-objc-accessible --retain-swift-ui-previews \
        --relative-results --disable-update-check --quiet >"$derived/scan.txt" || true
    grep -E '^[^ ]+:[0-9]+:[0-9]+: warning: ' "$derived/scan.txt" | sort -u >"$derived/unused.txt" || true
}

shared_only() {
    grep -E '^client/apple/' "$1" || true
}

own_only() {
    grep -vE '^client/apple/' "$1" || true
}

scan client/macos/Deskhub.xcodeproj macosx macos
scan client/ios/Deskhub.xcodeproj iphonesimulator ios

mac=out/periphery/macos/unused.txt
ios=out/periphery/ios/unused.txt
findings=$(
    own_only "$mac"
    own_only "$ios"
    comm -12 <(shared_only "$mac") <(shared_only "$ios")
)

if [ -n "$findings" ]; then
    echo "$findings"
    echo "periphery: FAILED - delete the unused Swift declarations listed above." \
        "Code in client/apple is reported only when neither app uses it."
    exit 1
fi
echo "periphery: OK"
