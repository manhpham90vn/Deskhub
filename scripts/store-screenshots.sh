#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

IOS_BUNDLE=com.ios.deskhub
IOS_APP=out/build/ios/Debug-iphonesimulator/app.app
IOS_SHOTS=client/ios/fastlane/screenshots/en-US
IPHONE_SIM="iPhone 13 Pro Max"
IPHONE_SIZE=1284x2778
IPAD_SIM="iPad Pro 13-inch (M5)"
IPAD_SIZE=2064x2752

ANDROID_PKG=com.manhpham.deskhub
ANDROID_ACTIVITY=com.manhpham.deskhub/com.deskhub.app.MainActivity
ANDROID_APK=client/android/app/build/outputs/apk/debug/app-debug.apk
PLAY_IMAGES=client/android/fastlane/metadata/android/en-US/images
PHONE_AVD=Deskhub_Phone
PHONE_DEVICE=medium_phone
PHONE_IMAGE="system-images;android-34;default;arm64-v8a"
PHONE_SIZE=1080x2400
TABLET_AVD=Small_Tablet
TABLET_SIZE=1920x1200
ANDROID_SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
EMULATOR="$ANDROID_SDK/emulator/emulator"
ADB="$ANDROID_SDK/platform-tools/adb"
AVDMANAGER="$ANDROID_SDK/cmdline-tools/latest/bin/avdmanager"
SERIAL=""

MACOS_BUNDLE=com.deskhub.macos
MACOS_APP=out/build/macos/Debug/app.app
MACOS_OUT=out/screenshots/macos

README_IMGS=docs/imgs
README_IOS_HEIGHT=1200
README_IOS_WIDTH=555

PAGES=(client host devices settings)
SETTLE="${SETTLE:-4}"
EMU_PID=""

die() {
    echo "store-screenshots: $*" >&2
    exit 1
}

cleanup() {
    if [ -n "$EMU_PID" ]; then
        kill "$EMU_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

check_size() {
    local file=$1 want=$2 got
    got=$(sips -g pixelWidth -g pixelHeight "$file" | awk '/pixel/ {print $2}' | paste -sd x -)
    if [ "$got" != "$want" ]; then
        die "$file is $got but the store listing uses $want - wrong simulator/emulator profile"
    fi
}

sim_udid() {
    xcrun simctl list devices available | grep -F "$1 (" | grep -oE '[0-9A-F-]{36}' | head -1
}

shoot_simulator() {
    local sim=$1 prefix=$2 size=$3 udid index file
    udid=$(sim_udid "$sim") ||
        die "simulator \"$sim\" not found - create it in Xcode (Window > Devices and Simulators)"
    echo "== $sim"
    xcrun simctl boot "$udid" 2>/dev/null || true
    xcrun simctl bootstatus "$udid" -b
    xcrun simctl status_bar "$udid" override --time 9:41 --batteryState charged \
        --batteryLevel 100 --wifiMode active --wifiBars 3 --cellularMode notSupported
    xcrun simctl install "$udid" "$IOS_APP"
    rm -f "$IOS_SHOTS/${prefix}"_0*.png
    for index in 0 1 2 3; do
        file="$IOS_SHOTS/${prefix}_0$((index + 1)).png"
        xcrun simctl terminate "$udid" "$IOS_BUNDLE" 2>/dev/null || true
        xcrun simctl launch "$udid" "$IOS_BUNDLE" -DeskhubStartPage "$index" >/dev/null
        sleep "$SETTLE"
        xcrun simctl io "$udid" screenshot --type=png "$file" >/dev/null
        check_size "$file" "$size"
        echo "$file"
    done
    xcrun simctl terminate "$udid" "$IOS_BUNDLE" 2>/dev/null || true
    xcrun simctl shutdown "$udid"
}

adb_shell() {
    "$ADB" -s "$SERIAL" shell "$@"
}

emulator_display_id() {
    adb_shell dumpsys SurfaceFlinger --display-id | awk '/^Display/ {print $2; exit}' | tr -d '\r'
}

screencap_png() {
    local display_id=$1 file=$2
    if [ -n "$display_id" ]; then
        "$ADB" -s "$SERIAL" exec-out screencap -p -d "$display_id" >"$file"
    else
        "$ADB" -s "$SERIAL" exec-out screencap -p >"$file"
    fi
}

wait_for_boot() {
    "$ADB" -s "$SERIAL" wait-for-device
    until [ "$(adb_shell getprop sys.boot_completed | tr -d '\r')" = "1" ]; do
        sleep 2
    done
}

demo_status_bar() {
    adb_shell settings put global sysui_demo_allowed 1
    local demo="am broadcast -a com.android.systemui.demo -e command"
    adb_shell "$demo" enter >/dev/null
    adb_shell "$demo" clock -e hhmm 0941 >/dev/null
    adb_shell "$demo" battery -e level 100 -e plugged false >/dev/null
    adb_shell "$demo" network -e wifi show -e level 4 -e fully true >/dev/null
    adb_shell "$demo" network -e mobile hide >/dev/null
    adb_shell "$demo" notifications -e visible false >/dev/null
}

ensure_avd() {
    local avd=$1 device=$2 image=$3
    if "$EMULATOR" -list-avds | grep -qx "$avd"; then
        return 0
    fi
    [ -x "$AVDMANAGER" ] ||
        die "no AVD named \"$avd\" and no avdmanager at $AVDMANAGER - install the SDK command-line tools, or create the AVD by hand in Android Studio"
    echo "== creating $avd ($device)"
    echo no | "$AVDMANAGER" create avd -n "$avd" -k "$image" -d "$device" >/dev/null ||
        die "could not create \"$avd\" - install its system image first with: sdkmanager \"$image\""
}

grant_notifications() {
    local sdk
    sdk=$(adb_shell getprop ro.build.version.sdk | tr -d '\r')
    if [ "$sdk" -ge 33 ]; then
        adb_shell pm grant "$ANDROID_PKG" android.permission.POST_NOTIFICATIONS
    fi
}

serial_for_avd() {
    local serial name
    for serial in $("$ADB" devices | awk '/^emulator-/ {print $1}'); do
        name=$("$ADB" -s "$serial" emu avd name 2>/dev/null | head -1 | tr -d '\r')
        if [ "$name" = "$1" ]; then
            echo "$serial"
            return 0
        fi
    done
    return 1
}

free_serial() {
    local port
    for port in 5554 5556 5558 5560; do
        if ! "$ADB" devices | grep -q "^emulator-$port"; then
            echo "emulator-$port"
            return 0
        fi
    done
    return 1
}

shoot_emulator() {
    local avd=$1 outdir=$2 size=$3 index section file started=0 display_id
    if SERIAL=$(serial_for_avd "$avd"); then
        echo "== $avd (reusing $SERIAL)"
    else
        "$EMULATOR" -list-avds | grep -qx "$avd" ||
            die "no AVD named \"$avd\" - create it in Android Studio's Device Manager with a $size display"
        SERIAL=$(free_serial) ||
            die "no free emulator port between 5554 and 5560 - close an emulator"
        echo "== $avd (booting as $SERIAL)"
        "$EMULATOR" -avd "$avd" -port "${SERIAL#emulator-}" -no-audio -no-boot-anim \
            >/dev/null 2>&1 &
        EMU_PID=$!
        started=1
    fi
    wait_for_boot
    demo_status_bar
    adb_shell settings put system accelerometer_rotation 0
    adb_shell settings put system user_rotation 0
    display_id=$(emulator_display_id)
    "$ADB" -s "$SERIAL" install -r "$ANDROID_APK" >/dev/null
    grant_notifications
    rm -f "$outdir"/0*.png
    for index in 0 1 2 3; do
        section=${PAGES[$index]}
        file="$outdir/0$((index + 1)).png"
        adb_shell am force-stop "$ANDROID_PKG"
        adb_shell am start -n "$ANDROID_ACTIVITY" -e section "$section" >/dev/null
        sleep "$SETTLE"
        screencap_png "$display_id" "$file"
        check_size "$file" "$size"
        echo "$file"
    done
    adb_shell am broadcast -a com.android.systemui.demo -e command exit >/dev/null || true
    if [ "$started" = 1 ]; then
        "$ADB" -s "$SERIAL" emu kill >/dev/null || true
        wait "$EMU_PID" 2>/dev/null || true
        EMU_PID=""
    fi
}

macos_window_id() {
    DESKHUB_BUNDLE="$MACOS_BUNDLE" swift - <<'EOF'
import AppKit

let bundle = ProcessInfo.processInfo.environment["DESKHUB_BUNDLE"] ?? ""
guard let app = NSRunningApplication.runningApplications(withBundleIdentifier: bundle).first
else { exit(1) }
let pid = app.processIdentifier
let options: CGWindowListOption = [.optionOnScreenOnly, .excludeDesktopElements]
let windows = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] ?? []
for window in windows {
    guard let owner = window[kCGWindowOwnerPID as String] as? NSNumber,
          owner.int32Value == pid,
          let layer = window[kCGWindowLayer as String] as? NSNumber,
          layer.intValue == 0,
          let number = window[kCGWindowNumber as String] as? NSNumber
    else { continue }
    print(number.intValue)
    exit(0)
}
exit(1)
EOF
}

quit_macos_app() {
    osascript -e "tell application id \"$MACOS_BUNDLE\" to quit" 2>/dev/null || true
    local tries=0
    while pgrep -qf "$MACOS_APP/Contents/MacOS/" && [ "$tries" -lt 10 ]; do
        sleep 1
        tries=$((tries + 1))
    done
}

shoot_macos() {
    local order=(host client devices settings) index page wid tries file
    mkdir -p "$MACOS_OUT"
    echo "== macOS app"
    for index in 0 1 2 3; do
        page=${order[$index]}
        file="$MACOS_OUT/macos_$page.png"
        quit_macos_app
        tries=0
        until open "$MACOS_APP" --args -DeskhubStartPage "$index"; do
            tries=$((tries + 1))
            if [ "$tries" -ge 5 ]; then
                die "open kept failing for $MACOS_APP - LaunchServices refused the launch"
            fi
            sleep 2
        done
        wid=""
        tries=0
        until wid=$(macos_window_id) || [ "$tries" -ge 30 ]; do
            sleep 1
            tries=$((tries + 1))
        done
        if [ -z "$wid" ]; then
            die "the macOS app never showed a window - launch $MACOS_APP by hand to see why"
        fi
        sleep "$SETTLE"
        screencapture -o -x -l "$wid" "$file" ||
            die "screencapture failed - grant your terminal Screen Recording in System Settings > Privacy & Security"
        echo "$file"
    done
    quit_macos_app
}

run_ios() {
    make build-ios
    shoot_simulator "$IPHONE_SIM" APP_IPHONE_65 "$IPHONE_SIZE"
    shoot_simulator "$IPAD_SIM" APP_IPAD_PRO_3GEN_129 "$IPAD_SIZE"
}

run_android() {
    if [ ! -x "$EMULATOR" ] || [ ! -x "$ADB" ]; then
        die "Android SDK not found at $ANDROID_SDK - run make bootstrap or set ANDROID_HOME"
    fi
    make build-android
    ensure_avd "$PHONE_AVD" "$PHONE_DEVICE" "$PHONE_IMAGE"
    shoot_emulator "$PHONE_AVD" "$PLAY_IMAGES/phoneScreenshots" "$PHONE_SIZE"
    shoot_emulator "$TABLET_AVD" "$PLAY_IMAGES/sevenInchScreenshots" "$TABLET_SIZE"
    rm -f "$PLAY_IMAGES"/tenInchScreenshots/0*.png
    cp "$PLAY_IMAGES"/sevenInchScreenshots/0*.png "$PLAY_IMAGES/tenInchScreenshots/"
}

run_macos() {
    quit_macos_app
    make build-macos
    shoot_macos
}

readme_source() {
    if [ ! -f "$1" ]; then
        die "$1 is missing - run the ios, android and macos targets before readme"
    fi
}

run_readme() {
    local pages=(host client devices settings) index n
    for index in 0 1 2 3; do
        n=$((index + 1))
        readme_source "$IOS_SHOTS/APP_IPHONE_65_0$n.png"
        readme_source "$PLAY_IMAGES/phoneScreenshots/0$n.png"
        readme_source "$MACOS_OUT/macos_${pages[$index]}.png"
        sips -z "$README_IOS_HEIGHT" "$README_IOS_WIDTH" \
            "$IOS_SHOTS/APP_IPHONE_65_0$n.png" \
            --out "$README_IMGS/ios_$n.png" >/dev/null
        cp "$PLAY_IMAGES/phoneScreenshots/0$n.png" "$README_IMGS/android_$n.png"
        cp "$MACOS_OUT/macos_${pages[$index]}.png" "$README_IMGS/macos_$n.png"
        echo "$README_IMGS/ios_$n.png $README_IMGS/android_$n.png $README_IMGS/macos_$n.png"
    done
}

main() {
    local targets=("$@") target
    if [ "${#targets[@]}" -eq 0 ]; then
        targets=(ios android macos readme)
    fi
    quit_macos_app
    for target in "${targets[@]}"; do
        case "$target" in
            ios) run_ios ;;
            android) run_android ;;
            macos) run_macos ;;
            readme) run_readme ;;
            *) die "unknown target \"$target\" (expected: ios android macos readme)" ;;
        esac
    done
    echo "store-screenshots: done"
}

main "$@"
