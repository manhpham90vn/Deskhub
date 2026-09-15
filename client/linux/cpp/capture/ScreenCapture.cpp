#include "capture/ScreenCapture.h"

#include "deskhubp/media/PortalScreenCast.h"

#include <pipewire/pipewire.h>
#include <spa/debug/types.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/result.h>

#include <drm_fourcc.h>

#include <atomic>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "deskhub/control/StreamSize.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace {

std::once_flag g_pwInit;

uint32_t SpaToDrmFormat(uint32_t spaFormat) {
    switch (spaFormat) {
        case SPA_VIDEO_FORMAT_BGRx: return DRM_FORMAT_XRGB8888;
        case SPA_VIDEO_FORMAT_BGRA: return DRM_FORMAT_ARGB8888;
        case SPA_VIDEO_FORMAT_RGBx: return DRM_FORMAT_XBGR8888;
        case SPA_VIDEO_FORMAT_RGBA: return DRM_FORMAT_ABGR8888;
        default: return 0;
    }
}

}

struct ScreenCapture::Impl {
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_stream* stream = nullptr;
    spa_hook streamListener{};

    uint32_t nodeId = 0;
    uint32_t fps = 60;
    ScreenCapture::FrameHandler onFrame;

    spa_video_info_raw format{};
    uint32_t drmFormat = 0;
    bool haveFormat = false;
    bool wantDmaBuf = false;

    std::atomic<bool> closed{false};
    std::atomic<bool> dmaBufActive{false};

    static constexpr uint64_t kModifiers[] = {DRM_FORMAT_MOD_LINEAR, DRM_FORMAT_MOD_INVALID};
};

namespace {

const spa_pod* BuildFormat(spa_pod_builder* b, const uint64_t* modifiers, size_t modifierCount,
    uint32_t maxFps) {
    spa_pod_frame f0{}, f1{};

    const spa_rectangle defSize{1920, 1080};
    const spa_rectangle minSize{16, 16};
    const spa_rectangle maxSize{8192, 8192};
    const spa_fraction defFps{maxFps, 1};
    const spa_fraction minFps{0, 1};
    const spa_fraction maxFpsFrac{maxFps, 1};

    spa_pod_builder_push_object(b, &f0, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);

    spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_format, 0);
    spa_pod_builder_push_choice(b, &f1, SPA_CHOICE_Enum, 0);
    spa_pod_builder_id(b, SPA_VIDEO_FORMAT_BGRx);
    spa_pod_builder_id(b, SPA_VIDEO_FORMAT_BGRx);
    spa_pod_builder_id(b, SPA_VIDEO_FORMAT_BGRA);
    spa_pod_builder_id(b, SPA_VIDEO_FORMAT_RGBx);
    spa_pod_builder_id(b, SPA_VIDEO_FORMAT_RGBA);
    spa_pod_builder_pop(b, &f1);

    if (modifiers && modifierCount) {
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier,
            SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
        spa_pod_builder_push_choice(b, &f1, SPA_CHOICE_Enum, 0);
        spa_pod_builder_long(b, int64_t(modifiers[0]));
        for (size_t i = 0; i < modifierCount; ++i) spa_pod_builder_long(b, int64_t(modifiers[i]));
        spa_pod_builder_pop(b, &f1);
    }

    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size,
        SPA_POD_CHOICE_RANGE_Rectangle(&defSize, &minSize, &maxSize), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate,
        SPA_POD_CHOICE_RANGE_Fraction(&defFps, &minFps, &maxFpsFrac), 0);

    return static_cast<const spa_pod*>(spa_pod_builder_pop(b, &f0));
}

const spa_pod* BuildFixatedFormat(spa_pod_builder* b, const spa_video_info_raw& info,
    uint64_t modifier, uint32_t maxFps) {
    spa_pod_frame f0{};
    const spa_fraction defFps{maxFps, 1};
    const spa_fraction minFps{0, 1};
    const spa_fraction maxFpsFrac{maxFps, 1};

    spa_pod_builder_push_object(b, &f0, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(info.format), 0);
    spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY);
    spa_pod_builder_long(b, int64_t(modifier));
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&info.size), 0);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate,
        SPA_POD_CHOICE_RANGE_Fraction(&defFps, &minFps, &maxFpsFrac), 0);
    return static_cast<const spa_pod*>(spa_pod_builder_pop(b, &f0));
}

const char* ModifierName(uint64_t modifier) {
    if (modifier == uint64_t(DRM_FORMAT_MOD_INVALID)) return "implicit";
    if (modifier == uint64_t(DRM_FORMAT_MOD_LINEAR)) return "linear";
    return "vendor-specific";
}

std::string ModifierList(const int64_t* modifiers, uint32_t count) {
    constexpr uint32_t kMaxLogged = 8;
    if (!modifiers || !count) return "none";
    std::string out;
    for (uint32_t i = 0; i < count && i < kMaxLogged; ++i) {
        char one[32];
        std::snprintf(one, sizeof(one), "%s0x%llx", i ? " " : "",
            (unsigned long long)modifiers[i]);
        out += one;
    }
    if (count > kMaxLogged) out += " ...";
    return out;
}

void OnStateChanged(void* data, pw_stream_state old, pw_stream_state state, const char* error) {
    auto* im = static_cast<ScreenCapture::Impl*>(data);
    LOGI("[Capture][node %u] state %s -> %s%s%s", im->nodeId, pw_stream_state_as_string(old),
        pw_stream_state_as_string(state), error ? ": " : "", error ? error : "");

    if (error && std::strstr(error, "no target node")) {
        LOGE(
            "[Capture][node %u] The node the portal named is gone, so there is nothing to "
            "capture. Either the ScreenCast session ended — it dies with the D-Bus "
            "connection that opened it, so whoever holds that connection must keep it "
            "referenced for as long as the capture runs — or the compositor never created "
            "the node at all. 'pw-dump' during a share tells the two apart: no screencast "
            "node listed means the compositor, and a Wayland session cannot restart its "
            "compositor in place, so log out and back in before sharing again.",
            im->nodeId);
    }

    if (state == PW_STREAM_STATE_UNCONNECTED || state == PW_STREAM_STATE_ERROR)
        im->closed.store(true, std::memory_order_release);
}

void OnParamChanged(void* data, uint32_t id, const spa_pod* param) {
    auto* im = static_cast<ScreenCapture::Impl*>(data);
    if (!param || id != SPA_PARAM_Format) return;

    uint32_t mediaType = 0, mediaSubtype = 0;
    if (spa_format_parse(param, &mediaType, &mediaSubtype) < 0) return;
    if (mediaType != SPA_MEDIA_TYPE_video || mediaSubtype != SPA_MEDIA_SUBTYPE_raw) return;

    spa_video_info_raw info{};
    if (spa_format_video_raw_parse(param, &info) < 0) {
        LOGE("[Capture][node %u] Could not parse the negotiated format.", im->nodeId);
        return;
    }

    const spa_pod_prop* modProp = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    uint8_t podBuf[2048];
    if (modProp && (modProp->flags & SPA_POD_PROP_FLAG_DONT_FIXATE)) {
        uint32_t nVals = 0, choiceType = 0;
        spa_pod* child = spa_pod_get_values(&modProp->value, &nVals, &choiceType);
        const int64_t* mods = (child && SPA_POD_TYPE(child) == SPA_TYPE_Long)
                                  ? static_cast<const int64_t*>(SPA_POD_BODY(child))
                                  : nullptr;

        uint64_t chosen = DRM_FORMAT_MOD_INVALID;
        bool found = false;
        for (uint64_t want : ScreenCapture::Impl::kModifiers) {
            for (uint32_t i = 0; mods && i < nVals && !found; ++i)
                if (uint64_t(mods[i]) == want) {
                    chosen = want;
                    found = true;
                }
            if (found) break;
        }
        if (!found && mods && nVals) chosen = uint64_t(mods[0]);

        spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
        const spa_pod* params[2];
        params[0] = BuildFixatedFormat(&b, info, chosen, im->fps);
        params[1] = BuildFormat(&b, nullptr, 0, im->fps);
        LOGI(
            "[Capture][node %u] Fixating dma-buf modifier 0x%llx (%s); compositor offered [%s]. "
            "Keeping the shared-memory format as a fallback.",
            im->nodeId, (unsigned long long)chosen, ModifierName(chosen),
            ModifierList(mods, nVals).c_str());
        pw_stream_update_params(im->stream, params, 2);
        return;
    }

    im->format = info;
    im->drmFormat = SpaToDrmFormat(info.format);
    im->wantDmaBuf = modProp != nullptr;
    im->haveFormat = im->drmFormat != 0;
    if (!im->haveFormat) {
        LOGE("[Capture][node %u] Negotiated an unsupported pixel format (%u).", im->nodeId,
            info.format);
        im->closed.store(true, std::memory_order_release);
        return;
    }

    LOGI("[Capture][node %u] Format: %ux%u %s, %s.", im->nodeId, info.size.width, info.size.height,
        spa_debug_type_find_short_name(spa_type_video_format, info.format),
        im->wantDmaBuf ? "dma-buf (zero-copy)" : "mapped memory (CPU copy)");

    const int dataType = im->wantDmaBuf ? (1 << SPA_DATA_DmaBuf)
                                        : ((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr));

    spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
    const spa_pod* params[1];
    params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
        SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(dataType)));
    pw_stream_update_params(im->stream, params, 1);
}

void OnProcess(void* data) {
    auto* im = static_cast<ScreenCapture::Impl*>(data);

    pw_buffer* b = pw_stream_dequeue_buffer(im->stream);
    if (!b) return;
    while (pw_buffer* next = pw_stream_dequeue_buffer(im->stream)) {
        pw_stream_queue_buffer(im->stream, b);
        b = next;
    }

    spa_buffer* sb = b->buffer;
    if (!im->haveFormat || !sb->n_datas) {
        pw_stream_queue_buffer(im->stream, b);
        return;
    }

    LinuxFrameInfo fi;
    fi.drmFormat = im->drmFormat;
    fi.meta.width = deskhub::EvenDown(im->format.size.width);
    fi.meta.height = deskhub::EvenDown(im->format.size.height);
    fi.meta.timestampUs = NowUs();

    bool ok = false;
    if (sb->datas[0].type == SPA_DATA_DmaBuf) {
        fi.memory = FrameMemory::DmaBuf;
        fi.modifier = im->format.modifier;
        fi.planeCount = sb->n_datas > kMaxDmaPlanes ? kMaxDmaPlanes : sb->n_datas;
        for (uint32_t i = 0; i < fi.planeCount; ++i) {
            fi.planes[i].fd = int(sb->datas[i].fd);
            fi.planes[i].offset = sb->datas[i].chunk->offset;
            fi.planes[i].stride = uint32_t(sb->datas[i].chunk->stride);
        }
        ok = fi.planeCount > 0 && fi.planes[0].fd >= 0;
        im->dmaBufActive.store(true, std::memory_order_relaxed);
    } else if (sb->datas[0].data) {
        fi.memory = FrameMemory::Mapped;
        fi.handle = static_cast<const uint8_t*>(sb->datas[0].data) + sb->datas[0].chunk->offset;
        fi.stride = uint32_t(sb->datas[0].chunk->stride);
        ok = sb->datas[0].chunk->size > 0 && fi.stride > 0;
        im->dmaBufActive.store(false, std::memory_order_relaxed);
    }

    if (sb->datas[0].chunk->flags & SPA_CHUNK_FLAG_CORRUPTED) ok = false;

    if (ok && fi.meta.width && fi.meta.height && im->onFrame) im->onFrame(fi);

    pw_stream_queue_buffer(im->stream, b);
}

const pw_stream_events kStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = nullptr,
    .state_changed = OnStateChanged,
    .control_info = nullptr,
    .io_changed = nullptr,
    .param_changed = OnParamChanged,
    .add_buffer = nullptr,
    .remove_buffer = nullptr,
    .process = OnProcess,
    .drained = nullptr,
    .command = nullptr,
    .trigger_done = nullptr,
};

}

ScreenCapture::ScreenCapture() : impl_(std::make_unique<Impl>()) {}

ScreenCapture::~ScreenCapture() {
    Stop();
}

bool ScreenCapture::Closed() const {
    return impl_ && impl_->closed.load(std::memory_order_acquire);
}

bool ScreenCapture::usingDmaBuf() const {
    return impl_ && impl_->dmaBufActive.load(std::memory_order_relaxed);
}

bool ScreenCapture::Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
    FrameHandler onFrame) {
    const uint32_t nodeId = uint32_t(targetId);
    const uint32_t fps = opt.fps;
    std::call_once(g_pwInit, [] { pw_init(nullptr, nullptr); });

    Impl* im = impl_.get();
    im->nodeId = nodeId;
    im->fps = fps ? fps : 60;
    im->onFrame = std::move(onFrame);
    im->closed.store(false);

    im->loop = pw_thread_loop_new("deskhub-capture", nullptr);
    if (!im->loop) {
        LOGE("[Capture][node %u] pw_thread_loop_new failed.", nodeId);
        return false;
    }

    pw_thread_loop_lock(im->loop);

    im->context = pw_context_new(pw_thread_loop_get_loop(im->loop), nullptr, 0);
    if (!im->context) {
        LOGE("[Capture][node %u] pw_context_new failed.", nodeId);
        pw_thread_loop_unlock(im->loop);
        Stop();
        return false;
    }

    const int fd = deskhubp::PortalScreenCast::Instance().OpenRemoteFd();
    if (fd < 0) {
        LOGE(
            "[Capture][node %u] The portal would not open a PipeWire remote for this screen: "
            "%s. Each captured screen needs a remote of its own: a duplicated descriptor is "
            "the same socket, so two pw_core objects would interleave their messages and "
            "steal each other's SCM_RIGHTS buffer descriptors, which fails one of the "
            "streams with 'Buffer allocation failed'.",
            nodeId, deskhubp::PortalScreenCast::Instance().lastError().c_str());
        pw_thread_loop_unlock(im->loop);
        Stop();
        return false;
    }
    im->core = pw_context_connect_fd(im->context, fd, nullptr, 0);
    if (!im->core) {
        LOGE("[Capture][node %u] pw_context_connect_fd failed.", nodeId);
        pw_thread_loop_unlock(im->loop);
        Stop();
        return false;
    }

    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
        "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr);

    im->stream = pw_stream_new(im->core, "deskhub-capture", props);
    if (!im->stream) {
        LOGE("[Capture][node %u] pw_stream_new failed.", nodeId);
        pw_thread_loop_unlock(im->loop);
        Stop();
        return false;
    }
    pw_stream_add_listener(im->stream, &im->streamListener, &kStreamEvents, im);

    uint8_t podBuf[2048];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
    const spa_pod* params[2];
    params[0] = BuildFormat(&b, Impl::kModifiers,
        sizeof(Impl::kModifiers) / sizeof(Impl::kModifiers[0]), im->fps);
    params[1] = BuildFormat(&b, nullptr, 0, im->fps);

    const int rc = pw_stream_connect(im->stream, PW_DIRECTION_INPUT, nodeId,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
        params, 2);
    pw_thread_loop_unlock(im->loop);

    if (rc < 0) {
        LOGE("[Capture][node %u] pw_stream_connect failed: %s", nodeId, spa_strerror(rc));
        Stop();
        return false;
    }

    if (pw_thread_loop_start(im->loop) < 0) {
        LOGE("[Capture][node %u] pw_thread_loop_start failed.", nodeId);
        Stop();
        return false;
    }
    return true;
}

void ScreenCapture::Stop() {
    Impl* im = impl_.get();
    if (!im) return;

    if (im->loop) pw_thread_loop_stop(im->loop);
    if (im->stream) {
        pw_stream_destroy(im->stream);
        im->stream = nullptr;
    }
    if (im->core) {
        pw_core_disconnect(im->core);
        im->core = nullptr;
    }
    if (im->context) {
        pw_context_destroy(im->context);
        im->context = nullptr;
    }
    if (im->loop) {
        pw_thread_loop_destroy(im->loop);
        im->loop = nullptr;
    }
    im->closed.store(true, std::memory_order_release);
}
