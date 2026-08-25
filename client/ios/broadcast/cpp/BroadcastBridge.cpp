#include "BroadcastBridge.h"

#include <Accelerate/Accelerate.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <CoreVideo/CVPixelBufferPool.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "capture/ScreenCapture.h"

#include "deskhubp/diag/Log.h"
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/ffi/ShareFfi.h"
#include "deskhubp/ffi/DiscoveryFfi.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/MemoryFootprint.h"

namespace {

constexpr int kMaxViewerRows = 16;
constexpr const char* kFallbackScreenName = "Deskhub";

enum class StartState { Idle,
    Starting,
    Sharing,
    Failed,
    Finished };

std::mutex g_startMutex;
StartState g_startState = StartState::Idle;
std::string g_startError;
std::string g_screenName = kFallbackScreenName;
std::string g_containerPath;
std::thread g_startThread;

std::string ScreenName() {
    std::lock_guard<std::mutex> lk(g_startMutex);
    return g_screenName;
}

std::string GroupTransferDir() {
    std::lock_guard<std::mutex> lk(g_startMutex);
    if (g_containerPath.empty()) return {};
    return g_containerPath + "/Deskhub";
}

std::string StartSharing(uint32_t width, uint32_t height) {
    deskhubp::SetLocalDisplay(width, height, ScreenName());

    DHShareSource source{};
    if (dh_share_list_sources(&source, 1) != 1)
        return "The broadcast reported no screen size.";

    const DHUiSettings settings = dh_settings_load();
    const DHShareDefaults defaults = dh_share_default_options();
    const std::string transferDir = GroupTransferDir();
    if (!transferDir.empty()) dh_set_transfer_dir(transferDir.c_str());

    constexpr int kPortHandoffTries = 8;
    constexpr auto kPortHandoffPause = std::chrono::milliseconds(500);
    bool ok = false;
    for (int attempt = 0; attempt < kPortHandoffTries; ++attempt) {
        ok = dh_share_start(&source, 1, settings.fps ? settings.fps : defaults.fps,
            settings.bitrateMbps ? settings.bitrateMbps : defaults.bitrateMbps,
            settings.maxDim ? settings.maxDim : defaults.maxDim, uint16_t(settings.port), false,
            settings.passcode, false, !transferDir.empty());
        if (ok || attempt + 1 == kPortHandoffTries) break;
        std::this_thread::sleep_for(kPortHandoffPause);
    }
    if (ok) return std::string();

    const char* reason = dh_share_last_error();
    return reason && *reason ? std::string(reason) : std::string("Sharing could not start.");
}

void SpawnStart(uint32_t width, uint32_t height) {
    g_startState = StartState::Starting;
    g_startError.clear();
    g_startThread = std::thread([width, height] {
        std::string failure = StartSharing(width, height);
        if (!failure.empty())
            LOGE("[Broadcast] Could not start sharing: %s", failure.c_str());
        std::lock_guard<std::mutex> lk(g_startMutex);
        if (g_startState == StartState::Finished) return;
        g_startState = failure.empty() ? StartState::Sharing : StartState::Failed;
        g_startError = std::move(failure);
    });
}

std::thread TakeStartThread() {
    std::lock_guard<std::mutex> lk(g_startMutex);
    g_startState = StartState::Finished;
    return std::move(g_startThread);
}

constexpr uint32_t kCgOrientationDown = 3;
constexpr uint32_t kCgOrientationRight = 6;
constexpr uint32_t kCgOrientationLeft = 8;
constexpr Pixel_16U kNeutralChromaPair = 0x8080;

CVPixelBufferPoolRef g_rotatePool = nullptr;
OSType g_rotatePoolFormat = 0;
size_t g_rotatePoolW = 0;
size_t g_rotatePoolH = 0;
bool g_rotateWarned = false;

uint8_t RotationFor(uint32_t cgOrientation) {
    switch (cgOrientation) {
        case kCgOrientationDown: return kRotate180DegreesClockwise;
        case kCgOrientationLeft: return kRotate90DegreesClockwise;
        case kCgOrientationRight: return kRotate270DegreesClockwise;
        default: return kRotate0DegreesClockwise;
    }
}

void ReleaseRotatePool() {
    if (!g_rotatePool) return;
    CVPixelBufferPoolRelease(g_rotatePool);
    g_rotatePool = nullptr;
    g_rotatePoolFormat = 0;
    g_rotatePoolW = 0;
    g_rotatePoolH = 0;
}

bool EnsureRotatePool(OSType format, size_t width, size_t height) {
    if (g_rotatePool && g_rotatePoolFormat == format && g_rotatePoolW == width &&
        g_rotatePoolH == height)
        return true;
    ReleaseRotatePool();

    const int32_t fmt = int32_t(format);
    const int32_t w = int32_t(width);
    const int32_t h = int32_t(height);
    CFNumberRef fmtRef = CFNumberCreate(nullptr, kCFNumberSInt32Type, &fmt);
    CFNumberRef wRef = CFNumberCreate(nullptr, kCFNumberSInt32Type, &w);
    CFNumberRef hRef = CFNumberCreate(nullptr, kCFNumberSInt32Type, &h);
    CFDictionaryRef surface = CFDictionaryCreate(nullptr, nullptr, nullptr, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void* keys[] = {kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferWidthKey,
        kCVPixelBufferHeightKey, kCVPixelBufferIOSurfacePropertiesKey};
    const void* values[] = {fmtRef, wRef, hRef, surface};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const CVReturn made = CVPixelBufferPoolCreate(nullptr, nullptr, attrs, &g_rotatePool);
    CFRelease(attrs);
    CFRelease(surface);
    CFRelease(hRef);
    CFRelease(wRef);
    CFRelease(fmtRef);
    if (made != kCVReturnSuccess) {
        g_rotatePool = nullptr;
        return false;
    }
    g_rotatePoolFormat = format;
    g_rotatePoolW = width;
    g_rotatePoolH = height;
    return true;
}

CVPixelBufferRef RotatedCopy(CVPixelBufferRef src, uint32_t cgOrientation) {
    const uint8_t rotation = RotationFor(cgOrientation);
    if (rotation == kRotate0DegreesClockwise) return nullptr;

    const OSType format = CVPixelBufferGetPixelFormatType(src);
    const bool bgra = format == kCVPixelFormatType_32BGRA;
    const bool biplanar = format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
                          format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    if (!bgra && !biplanar) {
        if (!g_rotateWarned) {
            g_rotateWarned = true;
            LOGW(
                "[Broadcast] Pixel format 0x%08X has no rotation path - rotated frames "
                "stay sideways.",
                unsigned(format));
        }
        return nullptr;
    }

    const size_t srcW = CVPixelBufferGetWidth(src);
    const size_t srcH = CVPixelBufferGetHeight(src);
    const bool quarterTurn =
        rotation == kRotate90DegreesClockwise || rotation == kRotate270DegreesClockwise;
    const size_t dstW = quarterTurn ? srcH : srcW;
    const size_t dstH = quarterTurn ? srcW : srcH;
    if (!dstW || !dstH || !EnsureRotatePool(format, dstW, dstH)) return nullptr;

    CVPixelBufferRef dst = nullptr;
    if (CVPixelBufferPoolCreatePixelBuffer(nullptr, g_rotatePool, &dst) != kCVReturnSuccess ||
        !dst)
        return nullptr;

    CVPixelBufferLockBaseAddress(src, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferLockBaseAddress(dst, 0);
    bool ok = false;
    if (bgra) {
        const vImage_Buffer in{CVPixelBufferGetBaseAddress(src), srcH, srcW,
            CVPixelBufferGetBytesPerRow(src)};
        const vImage_Buffer out{CVPixelBufferGetBaseAddress(dst), dstH, dstW,
            CVPixelBufferGetBytesPerRow(dst)};
        const uint8_t opaqueBlack[4] = {0, 0, 0, 255};
        ok = vImageRotate90_ARGB8888(&in, &out, rotation, opaqueBlack, kvImageNoFlags) ==
             kvImageNoError;
    } else {
        const vImage_Buffer inY{CVPixelBufferGetBaseAddressOfPlane(src, 0), srcH, srcW,
            CVPixelBufferGetBytesPerRowOfPlane(src, 0)};
        const vImage_Buffer outY{CVPixelBufferGetBaseAddressOfPlane(dst, 0), dstH, dstW,
            CVPixelBufferGetBytesPerRowOfPlane(dst, 0)};
        const vImage_Buffer inC{CVPixelBufferGetBaseAddressOfPlane(src, 1), srcH / 2, srcW / 2,
            CVPixelBufferGetBytesPerRowOfPlane(src, 1)};
        const vImage_Buffer outC{CVPixelBufferGetBaseAddressOfPlane(dst, 1), dstH / 2, dstW / 2,
            CVPixelBufferGetBytesPerRowOfPlane(dst, 1)};
        ok = vImageRotate90_Planar8(&inY, &outY, rotation, 0, kvImageNoFlags) == kvImageNoError &&
             vImageRotate90_Planar16U(&inC, &outC, rotation, kNeutralChromaPair, kvImageNoFlags) ==
                 kvImageNoError;
    }
    CVPixelBufferUnlockBaseAddress(dst, 0);
    CVPixelBufferUnlockBaseAddress(src, kCVPixelBufferLock_ReadOnly);
    if (!ok) {
        CVPixelBufferRelease(dst);
        return nullptr;
    }
    return dst;
}

}

void dhb_start_broadcast(const char* containerPath, const char* screenName) {
    deskhubp::SetAppDataDir(containerPath ? std::string(containerPath) : std::string());
    ScreenCapture::BeginBroadcast();

    std::lock_guard<std::mutex> lk(g_startMutex);
    g_containerPath = containerPath ? containerPath : "";
    g_screenName = screenName && *screenName ? std::string(screenName)
                                             : std::string(kFallbackScreenName);
    g_startState = StartState::Idle;
    g_startError.clear();
}

void dhb_push_audio(const int16_t* pcm, int samples) {
    dh_share_offer_audio(pcm, samples);
}

void dhb_push_frame(void* pixelBuffer, uint32_t cgOrientation) {
    if (!pixelBuffer) return;
    auto pb = static_cast<CVPixelBufferRef>(pixelBuffer);

    CVPixelBufferRef rotated = RotatedCopy(pb, cgOrientation);
    CVPixelBufferRef out = rotated ? rotated : pb;
    const uint32_t width = uint32_t(CVPixelBufferGetWidth(out));
    const uint32_t height = uint32_t(CVPixelBufferGetHeight(out));
    if (!width || !height) {
        if (rotated) CVPixelBufferRelease(rotated);
        return;
    }

    {
        std::lock_guard<std::mutex> lk(g_startMutex);
        if (g_startState == StartState::Idle) SpawnStart(width, height);
    }

    ScreenCapture::Frame frame;
    frame.handle = out;
    frame.meta.width = width;
    frame.meta.height = height;
    frame.meta.timestampUs = NowUs();
    ScreenCapture::DeliverFrame(frame);
    if (rotated) CVPixelBufferRelease(rotated);
}

void dhb_finish_broadcast(void) {
    ScreenCapture::ReportBroadcastFinished();

    std::thread pendingStart = TakeStartThread();
    if (pendingStart.joinable()) pendingStart.join();

    dh_share_stop();
    ReleaseRotatePool();
}

bool dhb_sharing(void) {
    return dh_share_running();
}

int dhb_viewer_count(void) {
    std::vector<DHHostRow> rows(kMaxViewerRows);
    const int count = dh_share_rows(rows.data(), kMaxViewerRows);
    int viewers = 0;
    for (int i = 0; i < count; ++i)
        if (rows[size_t(i)].viewer) ++viewers;
    return viewers;
}

int dhb_viewer_list(char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::vector<DHHostRow> rows(kMaxViewerRows);
    const int count = dh_share_rows(rows.data(), kMaxViewerRows);
    std::string names;
    for (int i = 0; i < count; ++i) {
        const DHHostRow& row = rows[size_t(i)];
        if (!row.viewer) continue;
        if (!names.empty()) names += ", ";
        names += row.client;
    }
    deskhubp::CopyToBuf(out, size_t(capacity), names);
    return int(std::strlen(out));
}

int dhb_memory_footprint_mb(void) {
    const int mb = deskhubp::MemoryFootprintMb();
    static int lastLoggedMb = -1;
    if (mb > 0 && mb != lastLoggedMb) {
        LOGI("[Broadcast] Memory footprint %d MB.", mb);
        lastLoggedMb = mb;
    }
    return mb;
}

int dhb_last_error(char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_startMutex);
    deskhubp::CopyToBuf(out, size_t(capacity), g_startError);
    return int(std::strlen(out));
}
