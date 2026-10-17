#!/usr/bin/env bash
set -euo pipefail

DAEMON_BINARY=${1:?usage: embed-macos-daemon.sh <DeskhubDaemon> <app Contents dir> <bundle id> <LaunchDaemon.plist.in> <deskhub-cli>}
CONTENTS=${2:?missing the Contents directory of the app}
BUNDLE_ID=${3:?missing the bundle identifier}
TEMPLATE=${4:?missing the LaunchDaemon plist template}
CLI_BINARY=${5:?missing the deskhub-cli binary}

[ -f "$DAEMON_BINARY" ] || {
    echo "embed-macos-daemon.sh: $DAEMON_BINARY is missing - the CMake phase builds deskhub_daemon before this one." >&2
    exit 1
}
[ -f "$CLI_BINARY" ] || {
    echo "embed-macos-daemon.sh: $CLI_BINARY is missing - the CMake phase builds deskhub_cli before this one." >&2
    exit 1
}

mkdir -p "$CONTENTS/MacOS" "$CONTENTS/Library/LaunchDaemons"
cp -f "$DAEMON_BINARY" "$CONTENTS/MacOS/DeskhubDaemon"
cp -f "$CLI_BINARY" "$CONTENTS/MacOS/deskhub-cli"
sed "s/@BUNDLE_ID@/$BUNDLE_ID/g" "$TEMPLATE" > "$CONTENTS/Library/LaunchDaemons/$BUNDLE_ID.daemon.plist"
plutil -lint "$CONTENTS/Library/LaunchDaemons/$BUNDLE_ID.daemon.plist" >/dev/null

if [ "${CODE_SIGNING_ALLOWED:-YES}" = NO ]; then
    exit 0
fi

sign=(codesign --force --sign "${EXPANDED_CODE_SIGN_IDENTITY:--}")
if [ "${ENABLE_HARDENED_RUNTIME:-NO}" = YES ]; then
    sign+=(--options runtime)
fi
read -ra extra <<< "${OTHER_CODE_SIGN_FLAGS:-}"
sign+=(${extra[@]+"${extra[@]}"})
"${sign[@]}" --identifier "$BUNDLE_ID.daemon" "$CONTENTS/MacOS/DeskhubDaemon"
"${sign[@]}" --identifier "$BUNDLE_ID.cli" "$CONTENTS/MacOS/deskhub-cli"
