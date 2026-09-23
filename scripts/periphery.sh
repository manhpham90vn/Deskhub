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
        return 1
    fi
    "$PERIPHERY" scan --project "$project" --schemes app --skip-build \
        --index-store-path "$derived/Index.noindex/DataStore" \
        --retain-objc-accessible --retain-swift-ui-previews \
        --relative-results --disable-update-check --quiet --strict
}

fail=0
scan client/macos/Deskhub.xcodeproj macosx macos || fail=1
scan client/ios/Deskhub.xcodeproj iphonesimulator ios || fail=1

if [ "$fail" = 1 ]; then
    echo "periphery: FAILED - delete the unused Swift declarations listed above"
    exit 1
fi
echo "periphery: OK"
