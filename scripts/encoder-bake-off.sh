#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

OUT_DIR=out/bake-off/encoder
CLIP_SOURCE=""
WIDTH=1920
HEIGHT=1080
FPS=60
BITRATE=20000000
CLIP_SECONDS=1
LOOPS=4
BACKENDS=""

usage() {
    cat <<'USAGE'
encoder-bake-off.sh - run every encoder backend on this machine over one fixed clip

  --clip FILE        measure this clip instead of the generated one
  --width W          encode width (default 1920)
  --height H         encode height (default 1080)
  --fps N            frame rate (default 60)
  --bitrate BPS      target bitrate (default 20000000)
  --seconds N        length of the generated clip (default 1)
  --loops N          how many times each backend replays the clip (default 4)
  --backends "a b"   backends to try (default: what the bench answers to --backends)
  --out DIR          where the table, the CSV and the bitstreams go

Every backend gets the same clip, the same size, the same fps and the same bitrate.
A backend that will not start on this machine is left out of the table rather than
measured under another backend's name.
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        --clip) CLIP_SOURCE=$2; shift 2 ;;
        --width) WIDTH=$2; shift 2 ;;
        --height) HEIGHT=$2; shift 2 ;;
        --fps) FPS=$2; shift 2 ;;
        --bitrate) BITRATE=$2; shift 2 ;;
        --seconds) CLIP_SECONDS=$2; shift 2 ;;
        --loops) LOOPS=$2; shift 2 ;;
        --backends) BACKENDS=$2; shift 2 ;;
        --out) OUT_DIR=$2; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "encoder-bake-off: unknown option $1" >&2; usage >&2; exit 2 ;;
    esac
done

require_tool() {
    command -v "$1" >/dev/null 2>&1 && return 0
    echo "encoder-bake-off: $1 is not on PATH. $2" >&2
    exit 1
}

require_tool ffmpeg "It builds the clip, decodes each bitstream and scores VMAF; without it this script can measure latency but not quality, which is the column C1 was missing."
require_tool ffprobe "It is part of the same ffmpeg install."

if ! ffmpeg -hide_banner -filters 2>/dev/null | grep -q ' libvmaf '; then
    echo "encoder-bake-off: this ffmpeg has no libvmaf filter, so there is no quality column." >&2
    echo "encoder-bake-off: install an ffmpeg built with --enable-libvmaf; enc_us alone ranks" >&2
    echo "encoder-bake-off: encoders by speed and says nothing about what they gave up for it." >&2
    exit 1
fi

find_bench() {
    if [ -n "${DESKHUB_ENCBENCH:-}" ]; then
        echo "$DESKHUB_ENCBENCH"
        return 0
    fi
    local candidate
    for candidate in \
        out/build/x64-release/client/windows/cpp/deskhub_encbench.exe \
        out/build/x64-debug/client/windows/cpp/deskhub_encbench.exe \
        out/build/release/client/windows/cpp/deskhub_encbench \
        out/build/debug/client/windows/cpp/deskhub_encbench; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

if ! BENCH=$(find_bench); then
    echo "encoder-bake-off: no encoder bench binary found." >&2
    echo "encoder-bake-off: build it with 'make build-windows' (or configure with" >&2
    echo "encoder-bake-off: -DDESKHUB_ENCODER_BENCH=ON), or point DESKHUB_ENCBENCH at it." >&2
    echo "encoder-bake-off: the bench is the only thing that feeds a fixed clip to Deskhub's" >&2
    echo "encoder-bake-off: own encoders; ffmpeg's h264_qsv and h264_nvenc are different code." >&2
    exit 1
fi

if [ -z "$BACKENDS" ]; then
    BACKENDS=$("$BENCH" --backends)
    if [ -z "$BACKENDS" ]; then
        echo "encoder-bake-off: $BENCH did not answer --backends, so there is nothing to race." >&2
        echo "encoder-bake-off: name them with --backends \"a b\" if this bench predates the flag." >&2
        exit 1
    fi
fi

mkdir -p "$OUT_DIR"
RAW_CLIP="$OUT_DIR/clip-${WIDTH}x${HEIGHT}.bgra"
REFERENCE="$OUT_DIR/reference-${WIDTH}x${HEIGHT}.y4m"
TABLE="$OUT_DIR/encoder-bake-off.csv"

build_clip() {
    if [ -n "$CLIP_SOURCE" ]; then
        echo "clip: $CLIP_SOURCE scaled to ${WIDTH}x${HEIGHT} at ${FPS} fps"
        ffmpeg -nostdin -v error -y -i "$CLIP_SOURCE" \
            -vf "scale=${WIDTH}:${HEIGHT},fps=${FPS}" -pix_fmt bgra -f rawvideo "$RAW_CLIP"
        return
    fi
    echo "clip: generated, ${CLIP_SECONDS}s of testsrc2 at ${WIDTH}x${HEIGHT}/${FPS}"
    ffmpeg -nostdin -v error -y \
        -f lavfi -i "testsrc2=size=${WIDTH}x${HEIGHT}:rate=${FPS}:duration=${CLIP_SECONDS}" \
        -pix_fmt bgra -f rawvideo "$RAW_CLIP"
}

build_reference() {
    ffmpeg -nostdin -v error -y \
        -f rawvideo -pix_fmt bgra -s "${WIDTH}x${HEIGHT}" -r "$FPS" \
        -stream_loop "$((LOOPS - 1))" -i "$RAW_CLIP" \
        -pix_fmt yuv420p "$REFERENCE"
}

clip_digest() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$RAW_CLIP" | cut -c1-16
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$RAW_CLIP" | cut -c1-16
    else
        echo "unhashed"
    fi
}

wait_for_reported_pid() {
    local err_file=$1 shell_pid=$2 waited=0 reported=""
    while [ "$waited" -lt 100 ]; do
        reported=$(sed -n 's/^encbench: pid \([0-9][0-9]*\)$/\1/p' "$err_file" 2>/dev/null | head -1)
        if [ -n "$reported" ]; then
            echo "$reported"
            return 0
        fi
        kill -0 "$shell_pid" 2>/dev/null || return 1
        sleep 0.1
        waited=$((waited + 1))
    done
    return 1
}

gpu_percent() {
    local os_pid=$1 shell_pid=$2
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            powershell -NoProfile -Command "
                \$path = '\\GPU Engine(pid_${os_pid}*)\\Utilization Percentage'
                \$total = 0.0; \$taken = 0
                while (Get-Process -Id ${os_pid} -ErrorAction SilentlyContinue) {
                    try {
                        \$now = (Get-Counter -Counter \$path -ErrorAction Stop).CounterSamples |
                            Measure-Object -Property CookedValue -Sum
                        \$total += \$now.Sum; \$taken += 1
                    } catch { Start-Sleep -Milliseconds 200 }
                }
                if (\$taken -gt 0) { '{0:N1}' -f (\$total / \$taken) } else { 'n/a' }
            " 2>/dev/null | tr -d '\r' | tail -1
            ;;
        *)
            while kill -0 "$shell_pid" 2>/dev/null; do sleep 1; done
            echo "n/a"
            ;;
    esac
}

vmaf_mean() {
    local bitstream=$1 log=$2
    if ! ffmpeg -nostdin -v error -y -r "$FPS" -i "$bitstream" -i "$REFERENCE" \
        -lavfi "[0:v]settb=AVTB,setpts=N[dis];[1:v]settb=AVTB,setpts=N[ref];[dis][ref]libvmaf=log_fmt=json:log_path=${log}" \
        -f null - 2>/dev/null; then
        echo "failed"
        return
    fi
    if command -v python3 >/dev/null 2>&1; then
        python3 -c "import json,sys; print('%.2f' % json.load(open(sys.argv[1]))['pooled_metrics']['vmaf']['mean'])" "$log"
    else
        grep -o '"mean"[^,]*' "$log" | head -1 | grep -o '[0-9.]*'
    fi
}

build_clip
build_reference
CLIP_FRAMES=$(( $(wc -c <"$RAW_CLIP") / (WIDTH * HEIGHT * 4) ))
DIGEST=$(clip_digest)

BENCH_FIELDS=$("$BENCH" --fields)
if [ -z "$BENCH_FIELDS" ]; then
    echo "encoder-bake-off: $BENCH did not answer --fields, so its rows cannot be labelled." >&2
    exit 1
fi
echo "${BENCH_FIELDS},gpu_pct,vmaf,kbps" >"$TABLE"

MEASURED=0
SKIPPED=""
for backend in $BACKENDS; do
    bitstream="$OUT_DIR/$backend.h264"
    row_file="$OUT_DIR/$backend.row"
    gpu_file="$OUT_DIR/$backend.gpu"

    "$BENCH" --clip "$RAW_CLIP" --width "$WIDTH" --height "$HEIGHT" --fps "$FPS" \
        --bitrate "$BITRATE" --loops "$LOOPS" --encoder "$backend" \
        --bitstream "$bitstream" >"$row_file" 2>"$OUT_DIR/$backend.err" &
    bench_pid=$!
    gpu_pid=""
    if os_pid=$(wait_for_reported_pid "$OUT_DIR/$backend.err" "$bench_pid"); then
        gpu_percent "$os_pid" "$bench_pid" >"$gpu_file" &
        gpu_pid=$!
    else
        echo "n/a" >"$gpu_file"
    fi
    bench_status=0
    wait "$bench_pid" || bench_status=$?
    if [ -n "$gpu_pid" ]; then
        wait "$gpu_pid" || true
    fi

    if [ "$bench_status" -ne 0 ] || [ ! -s "$row_file" ]; then
        SKIPPED="$SKIPPED $backend"
        rm -f "$bitstream"
        continue
    fi

    row=$(tail -1 "$row_file")
    gpu=$(tail -1 "$gpu_file")
    vmaf=$(vmaf_mean "$bitstream" "$OUT_DIR/$backend.vmaf.json")
    bytes=$(echo "$row" | cut -d, -f13)
    wall=$(echo "$row" | cut -d, -f12)
    kbps=$(awk -v b="$bytes" -v ms="$wall" 'BEGIN { if (ms > 0) printf "%.0f", b * 8 / ms; else print 0 }')
    echo "$row,${gpu:-n/a},${vmaf:-n/a},$kbps" >>"$TABLE"
    MEASURED=$((MEASURED + 1))
done

echo
echo "clip ${WIDTH}x${HEIGHT} ${FPS}fps, $CLIP_FRAMES frames x $LOOPS loops, ${BITRATE} bps, sha256:$DIGEST"
echo
if command -v column >/dev/null 2>&1; then
    cut -d, -f1,8,9,11,17,18,19 "$TABLE" | column -t -s,
else
    cut -d, -f1,8,9,11,17,18,19 "$TABLE"
fi
echo
echo "full table: $TABLE"
if [ -n "$SKIPPED" ]; then
    echo "not on this machine:$SKIPPED (see $OUT_DIR/<backend>.err for what each said)"
fi
if [ "$MEASURED" -lt 2 ]; then
    echo
    echo "Only $MEASURED backend(s) ran here, so this table ranks nothing yet - C1 needs a"
    echo "second column from a machine with different silicon before any of it is a comparison."
fi
SATURATED=$(awk -F, 'NR > 1 && $18 != "n/a" && $18 != "failed" && $18 + 0 < 99.5 { found = 1 }
                     END { print found ? "no" : "yes" }' "$TABLE")
if [ "$MEASURED" -gt 0 ] && [ "$SATURATED" = "yes" ]; then
    echo
    echo "Every VMAF here is at or above 99.5, which means the quality column ranks nothing:"
    echo "${BITRATE} bps is more than ${WIDTH}x${HEIGHT} at ${FPS} fps needs, so each encoder"
    echo "had room to be visually lossless. Lower --bitrate until the backends separate, or"
    echo "read this run as a latency and cost measurement with no quality result in it."
fi

echo
echo "cpu_pct is this process over wall time and includes the harness's own frame uploads;"
echo "gpu_pct is every GPU engine this process touched, so it counts the upload copy too."
echo "Frames go in paced at ${FPS} fps, so cpu_pct and gpu_pct are shares of real time and"
echo "another process on the same encode engine lands in enc_us as if it were the encoder."
echo "The clip is synthetic unless --clip named a real one: testsrc2 is not desktop content."
