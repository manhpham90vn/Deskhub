#include "capture/KmsDevice.h"

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>


namespace {

constexpr uint32_t kMaxCards = 16;
constexpr uint32_t kSixteenDotSixteenShift = 16;
constexpr int kTargetCardShift = 32;

struct ResourcesDeleter {
    void operator()(drmModeRes* r) const {
        drmModeFreeResources(r);
    }
};
struct CrtcDeleter {
    void operator()(drmModeCrtc* c) const {
        drmModeFreeCrtc(c);
    }
};
struct ConnectorDeleter {
    void operator()(drmModeConnector* c) const {
        drmModeFreeConnector(c);
    }
};
struct EncoderDeleter {
    void operator()(drmModeEncoder* e) const {
        drmModeFreeEncoder(e);
    }
};
struct PlaneResourcesDeleter {
    void operator()(drmModePlaneRes* p) const {
        drmModeFreePlaneResources(p);
    }
};
struct PlaneDeleter {
    void operator()(drmModePlane* p) const {
        drmModeFreePlane(p);
    }
};
struct PropertiesDeleter {
    void operator()(drmModeObjectProperties* p) const {
        drmModeFreeObjectProperties(p);
    }
};
struct PropertyDeleter {
    void operator()(drmModePropertyRes* p) const {
        drmModeFreeProperty(p);
    }
};
struct Fb2Deleter {
    void operator()(drmModeFB2* f) const {
        drmModeFreeFB2(f);
    }
};

using Resources = std::unique_ptr<drmModeRes, ResourcesDeleter>;
using Crtc = std::unique_ptr<drmModeCrtc, CrtcDeleter>;
using Connector = std::unique_ptr<drmModeConnector, ConnectorDeleter>;
using Encoder = std::unique_ptr<drmModeEncoder, EncoderDeleter>;
using PlaneResources = std::unique_ptr<drmModePlaneRes, PlaneResourcesDeleter>;
using Plane = std::unique_ptr<drmModePlane, PlaneDeleter>;
using Properties = std::unique_ptr<drmModeObjectProperties, PropertiesDeleter>;
using Property = std::unique_ptr<drmModePropertyRes, PropertyDeleter>;
using Fb2 = std::unique_ptr<drmModeFB2, Fb2Deleter>;

std::map<std::string, uint64_t> PlaneProperties(int fd, uint32_t planeId) {
    std::map<std::string, uint64_t> out;
    const Properties props(drmModeObjectGetProperties(fd, planeId, DRM_MODE_OBJECT_PLANE));
    if (!props) return out;
    for (uint32_t i = 0; i < props->count_props; ++i) {
        const Property prop(drmModeGetProperty(fd, props->props[i]));
        if (prop) out[prop->name] = props->prop_values[i];
    }
    return out;
}

uint64_t PropertyOr(const std::map<std::string, uint64_t>& props, const char* name,
    uint64_t fallback) {
    const auto it = props.find(name);
    return it == props.end() ? fallback : it->second;
}

int32_t SignedProperty(const std::map<std::string, uint64_t>& props, const char* name) {
    return int32_t(int64_t(PropertyOr(props, name, 0)));
}

uint64_t DmaBufIdentity(int fd) {
    struct stat info {};
    if (fstat(fd, &info) != 0) return 0;
    return uint64_t(info.st_ino);
}

void CloseHandles(int fd, const drmModeFB2& fb) {
    std::set<uint32_t> closed;
    for (uint32_t handle : fb.handles)
        if (handle && closed.insert(handle).second) drmCloseBufferHandle(fd, handle);
}

std::string ConnectorName(const drmModeConnector& connector) {
    const char* type = drmModeGetConnectorTypeName(connector.connector_type);
    return std::string(type ? type : "Display") + "-" +
           std::to_string(connector.connector_type_id);
}

std::optional<uint32_t> CrtcOfConnector(int fd, const drmModeConnector& connector) {
    if (connector.connection != DRM_MODE_CONNECTED || !connector.encoder_id) return std::nullopt;
    const Encoder encoder(drmModeGetEncoder(fd, connector.encoder_id));
    if (!encoder || !encoder->crtc_id) return std::nullopt;
    return encoder->crtc_id;
}

std::optional<deskhub::media::ShareSource> DisplayOf(int fd, uint32_t cardIndex,
    const drmModeConnector& connector) {
    const std::optional<uint32_t> crtcId = CrtcOfConnector(fd, connector);
    if (!crtcId) return std::nullopt;
    const Crtc crtc(drmModeGetCrtc(fd, *crtcId));
    if (!crtc || !crtc->mode_valid || !crtc->buffer_id) return std::nullopt;
    deskhub::media::ShareSource source;
    source.targetId = KmsTargetId(cardIndex, *crtcId);
    source.name = ConnectorName(connector);
    source.x = int32_t(crtc->x);
    source.y = int32_t(crtc->y);
    source.width = crtc->mode.hdisplay;
    source.height = crtc->mode.vdisplay;
    return source;
}

}

KmsBuffer::~KmsBuffer() {
    CloseAll();
}

KmsBuffer::KmsBuffer(KmsBuffer&& other) noexcept {
    *this = std::move(other);
}

KmsBuffer& KmsBuffer::operator=(KmsBuffer&& other) noexcept {
    if (this == &other) return *this;
    CloseAll();
    width = other.width;
    height = other.height;
    format = other.format;
    modifier = other.modifier;
    planeCount = other.planeCount;
    identity = other.identity;
    for (uint32_t i = 0; i < kMaxKmsPlanes; ++i) {
        fds[i] = std::exchange(other.fds[i], -1);
        offsets[i] = other.offsets[i];
        pitches[i] = other.pitches[i];
    }
    return *this;
}

void KmsBuffer::CloseAll() {
    for (int& fd : fds) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
}

std::optional<KmsCard> KmsCard::Open(uint32_t cardIndex) {
    const std::string path = "/dev/dri/card" + std::to_string(cardIndex);
    const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) return std::nullopt;
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
    return KmsCard(fd);
}

KmsCard::~KmsCard() {
    if (fd_ >= 0) close(fd_);
}

KmsCard::KmsCard(KmsCard&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)),
      cursorCrtc_(other.cursorCrtc_),
      cursorPlane_(other.cursorPlane_) {}

KmsCard& KmsCard::operator=(KmsCard&& other) noexcept {
    if (this == &other) return *this;
    if (fd_ >= 0) close(fd_);
    fd_ = std::exchange(other.fd_, -1);
    cursorCrtc_ = other.cursorCrtc_;
    cursorPlane_ = other.cursorPlane_;
    return *this;
}

std::optional<KmsScanout> KmsCard::Scanout(uint32_t crtcId) {
    const Crtc crtc(drmModeGetCrtc(fd_, crtcId));
    if (!crtc || !crtc->mode_valid || !crtc->buffer_id) return std::nullopt;
    KmsScanout scanout;
    scanout.fbId = crtc->buffer_id;
    scanout.x = crtc->x;
    scanout.y = crtc->y;
    scanout.width = crtc->mode.hdisplay;
    scanout.height = crtc->mode.vdisplay;
    scanout.cursor = CursorOn(crtcId);
    return scanout;
}

std::optional<uint32_t> KmsCard::FindCursorPlane(uint32_t crtcId) const {
    const Resources res(drmModeGetResources(fd_));
    const PlaneResources planes(drmModeGetPlaneResources(fd_));
    if (!res || !planes) return std::nullopt;
    int crtcIndex = -1;
    for (int i = 0; i < res->count_crtcs; ++i)
        if (res->crtcs[i] == crtcId) crtcIndex = i;
    if (crtcIndex < 0) return std::nullopt;
    for (uint32_t i = 0; i < planes->count_planes; ++i) {
        const Plane plane(drmModeGetPlane(fd_, planes->planes[i]));
        if (!plane || !(plane->possible_crtcs & (1u << unsigned(crtcIndex)))) continue;
        const auto props = PlaneProperties(fd_, plane->plane_id);
        if (PropertyOr(props, "type", DRM_PLANE_TYPE_OVERLAY) == DRM_PLANE_TYPE_CURSOR)
            return plane->plane_id;
    }
    return std::nullopt;
}

std::optional<KmsCursor> KmsCard::CursorOn(uint32_t crtcId) {
    if (cursorCrtc_ != crtcId) {
        cursorCrtc_ = crtcId;
        cursorPlane_ = FindCursorPlane(crtcId);
    }
    if (!cursorPlane_) return std::nullopt;
    const auto props = PlaneProperties(fd_, *cursorPlane_);
    const auto fbId = uint32_t(PropertyOr(props, "FB_ID", 0));
    if (!fbId || PropertyOr(props, "CRTC_ID", 0) != crtcId) return std::nullopt;
    KmsCursor cursor;
    cursor.fbId = fbId;
    cursor.x = SignedProperty(props, "CRTC_X");
    cursor.y = SignedProperty(props, "CRTC_Y");
    cursor.width = uint32_t(PropertyOr(props, "CRTC_W", 0));
    cursor.height = uint32_t(PropertyOr(props, "CRTC_H", 0));
    cursor.srcWidth = uint32_t(PropertyOr(props, "SRC_W", 0) >> kSixteenDotSixteenShift);
    cursor.srcHeight = uint32_t(PropertyOr(props, "SRC_H", 0) >> kSixteenDotSixteenShift);
    if (!cursor.width || !cursor.height) return std::nullopt;
    return cursor;
}

std::optional<KmsBuffer> KmsCard::Export(uint32_t fbId) const {
    const Fb2 fb(drmModeGetFB2(fd_, fbId));
    if (!fb) return std::nullopt;
    KmsBuffer buffer;
    buffer.width = fb->width;
    buffer.height = fb->height;
    buffer.format = fb->pixel_format;
    buffer.modifier =
        (fb->flags & DRM_MODE_FB_MODIFIERS) ? fb->modifier : DRM_FORMAT_MOD_INVALID;
    bool exported = true;
    for (uint32_t i = 0; i < kMaxKmsPlanes && fb->handles[i]; ++i) {
        if (drmPrimeHandleToFD(fd_, fb->handles[i], O_RDONLY | O_CLOEXEC, &buffer.fds[i]) != 0) {
            exported = false;
            break;
        }
        buffer.offsets[i] = fb->offsets[i];
        buffer.pitches[i] = fb->pitches[i];
        buffer.planeCount = i + 1;
    }
    CloseHandles(fd_, *fb);
    if (!exported || !buffer.planeCount) return std::nullopt;
    buffer.identity = DmaBufIdentity(buffer.fds[0]);
    return buffer;
}

std::string KmsCard::RenderNode() const {
    char* name = drmGetRenderDeviceNameFromFd(fd_);
    if (!name) return {};
    std::string node(name);
    std::free(name);
    return node;
}

std::vector<deskhub::media::ShareSource> KmsCard::Displays(uint32_t cardIndex) const {
    std::vector<deskhub::media::ShareSource> out;
    const Resources res(drmModeGetResources(fd_));
    if (!res) return out;
    for (int i = 0; i < res->count_connectors; ++i) {
        const Connector connector(drmModeGetConnector(fd_, res->connectors[i]));
        if (!connector) continue;
        if (auto display = DisplayOf(fd_, cardIndex, *connector)) out.push_back(*display);
    }
    return out;
}

uint64_t KmsTargetId(uint32_t cardIndex, uint32_t crtcId) {
    return (uint64_t(cardIndex) << kTargetCardShift) | crtcId;
}

uint32_t KmsCardOf(uint64_t targetId) {
    return uint32_t(targetId >> kTargetCardShift);
}

uint32_t KmsCrtcOf(uint64_t targetId) {
    return uint32_t(targetId);
}

std::vector<deskhub::media::ShareSource> ListKmsDisplays() {
    std::vector<deskhub::media::ShareSource> out;
    for (uint32_t index = 0; index < kMaxCards; ++index) {
        std::optional<KmsCard> card = KmsCard::Open(index);
        if (!card) continue;
        for (auto& display : card->Displays(index)) out.push_back(std::move(display));
    }
    return out;
}
