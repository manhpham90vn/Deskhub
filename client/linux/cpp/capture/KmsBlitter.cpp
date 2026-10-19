#include "capture/KmsBlitter.h"

#include <array>
#include <list>
#include <utility>
#include <vector>

#include <drm_fourcc.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <fcntl.h>
#include <gbm.h>
#include <unistd.h>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace {

constexpr size_t kTargetCount = 3;
constexpr size_t kMaxCachedImages = 8;
constexpr GLuint kPositionAttribute = 0;
constexpr GLuint kTexcoordAttribute = 1;
constexpr GLsizei kVerticesPerQuad = 4;

constexpr const char* kVertexShader =
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  uv = texcoord;\n"
    "  gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

constexpr const char* kFragmentShader =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 uv;\n"
    "uniform samplerExternalOES image;\n"
    "uniform float opaque;\n"
    "void main() {\n"
    "  vec4 color = texture2D(image, uv);\n"
    "  gl_FragColor = vec4(color.rgb, max(color.a, opaque));\n"
    "}\n";

struct Rect {
    float x0, y0, x1, y1;
};

struct Vertex {
    GLfloat x, y, u, v;
};

struct ImportedImage {
    uint64_t identity = 0;
    KmsBuffer buffer;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint texture = 0;
};

struct Target {
    gbm_bo* bo = nullptr;
    int fd = -1;
    uint32_t stride = 0;
    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint renderbuffer = 0;
    GLuint framebuffer = 0;
};

const std::array<EGLint, kMaxKmsPlanes> kPlaneFd = {EGL_DMA_BUF_PLANE0_FD_EXT,
    EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT};
const std::array<EGLint, kMaxKmsPlanes> kPlaneOffset = {EGL_DMA_BUF_PLANE0_OFFSET_EXT,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT};
const std::array<EGLint, kMaxKmsPlanes> kPlanePitch = {EGL_DMA_BUF_PLANE0_PITCH_EXT,
    EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT};
const std::array<EGLint, kMaxKmsPlanes> kPlaneModifierLo = {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT};
const std::array<EGLint, kMaxKmsPlanes> kPlaneModifierHi = {EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};

std::vector<EGLint> ImportAttributes(uint32_t width, uint32_t height, uint32_t format,
    uint64_t modifier, uint32_t planeCount, const int* fds, const uint32_t* offsets,
    const uint32_t* pitches) {
    std::vector<EGLint> attrs = {EGL_WIDTH, EGLint(width), EGL_HEIGHT, EGLint(height),
        EGL_LINUX_DRM_FOURCC_EXT, EGLint(format)};
    for (uint32_t i = 0; i < planeCount; ++i) {
        attrs.insert(attrs.end(), {kPlaneFd[i], fds[i], kPlaneOffset[i], EGLint(offsets[i]),
                                      kPlanePitch[i], EGLint(pitches[i])});
        if (modifier == DRM_FORMAT_MOD_INVALID) continue;
        attrs.insert(attrs.end(), {kPlaneModifierLo[i], EGLint(modifier & 0xffffffffu),
                                      kPlaneModifierHi[i], EGLint(modifier >> 32)});
    }
    attrs.push_back(EGL_NONE);
    return attrs;
}

GLuint CompileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok) return shader;
    std::array<char, 512> log{};
    glGetShaderInfoLog(shader, GLsizei(log.size()), nullptr, log.data());
    LOGE("[KMS] A blit shader did not compile: %s", log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint LinkBlitProgram() {
    const GLuint vertex = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vertex || !fragment) return 0;
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, kPositionAttribute, "position");
    glBindAttribLocation(program, kTexcoordAttribute, "texcoord");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok) return program;
    glDeleteProgram(program);
    LOGE("[KMS] The blit program did not link.");
    return 0;
}

float Ndc(float pixel, uint32_t extent) {
    return 2.0f * pixel / float(extent) - 1.0f;
}

void DrawQuad(const Rect& target, const Rect& source, uint32_t targetW, uint32_t targetH) {
    const float x0 = Ndc(target.x0, targetW), x1 = Ndc(target.x1, targetW);
    const float y0 = Ndc(target.y0, targetH), y1 = Ndc(target.y1, targetH);
    const std::array<Vertex, kVerticesPerQuad> vertices = {{
        {x0, y0, source.x0, source.y0},
        {x1, y0, source.x1, source.y0},
        {x0, y1, source.x0, source.y1},
        {x1, y1, source.x1, source.y1},
    }};
    const auto stride = GLsizei(sizeof(Vertex));
    glVertexAttribPointer(kPositionAttribute, 2, GL_FLOAT, GL_FALSE, stride, &vertices[0].x);
    glVertexAttribPointer(kTexcoordAttribute, 2, GL_FLOAT, GL_FALSE, stride, &vertices[0].u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, kVerticesPerQuad);
}

}

struct KmsBlitter::Impl {
    int renderFd = -1;
    gbm_device* gbm = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0;
    GLint opaqueUniform = -1;
    std::list<ImportedImage> images;
    std::vector<Target> targets;
    uint32_t targetW = 0;
    uint32_t targetH = 0;
    size_t nextTarget = 0;
    uint64_t frameId = 0;
    bool warnedImport = false;

    bool OpenDisplay(const std::string& renderNode) {
        renderFd = open(renderNode.c_str(), O_RDWR | O_CLOEXEC);
        if (renderFd < 0) return false;
        gbm = gbm_create_device(renderFd);
        if (!gbm) return false;
        display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, nullptr);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) return false;
        return epoxy_has_egl_extension(display, "EGL_EXT_image_dma_buf_import") &&
               epoxy_has_egl_extension(display, "EGL_EXT_image_dma_buf_import_modifiers") &&
               epoxy_has_egl_extension(display, "EGL_KHR_surfaceless_context") &&
               epoxy_has_egl_extension(display, "EGL_KHR_no_config_context");
    }

    bool OpenContext() {
        if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;
        const std::array<EGLint, 3> attrs = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrs.data());
        if (context == EGL_NO_CONTEXT) return false;
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) return false;
        if (!epoxy_has_gl_extension("GL_OES_EGL_image_external")) return false;
        program = LinkBlitProgram();
        if (!program) return false;
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "image"), 0);
        opaqueUniform = glGetUniformLocation(program, "opaque");
        glEnableVertexAttribArray(kPositionAttribute);
        glEnableVertexAttribArray(kTexcoordAttribute);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

    void Forget(ImportedImage& entry) {
        if (entry.texture) glDeleteTextures(1, &entry.texture);
        if (entry.image != EGL_NO_IMAGE_KHR) eglDestroyImageKHR(display, entry.image);
    }

    GLuint TextureFor(KmsBuffer buffer) {
        for (auto it = images.begin(); it != images.end(); ++it) {
            if (it->identity != buffer.identity) continue;
            images.splice(images.begin(), images, it);
            return images.front().texture;
        }
        const std::vector<EGLint> attrs = ImportAttributes(buffer.width, buffer.height,
            buffer.format, buffer.modifier, buffer.planeCount, buffer.fds, buffer.offsets,
            buffer.pitches);
        const EGLImageKHR image =
            eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
        if (image == EGL_NO_IMAGE_KHR) {
            WarnImportFailed(buffer);
            return 0;
        }
        ImportedImage entry;
        entry.identity = buffer.identity;
        entry.buffer = std::move(buffer);
        entry.image = image;
        glGenTextures(1, &entry.texture);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, entry.texture);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
        images.push_front(std::move(entry));
        EvictOldImages();
        return images.front().texture;
    }

    void EvictOldImages() {
        while (images.size() > kMaxCachedImages) {
            Forget(images.back());
            images.pop_back();
        }
    }

    void WarnImportFailed(const KmsBuffer& buffer) {
        if (warnedImport) return;
        warnedImport = true;
        const uint32_t f = buffer.format;
        LOGE(
            "[KMS] The GPU would not import the scanout buffer (format %c%c%c%c, modifier "
            "0x%llx, EGL error 0x%x).",
            char(f), char(f >> 8), char(f >> 16), char(f >> 24),
            static_cast<unsigned long long>(buffer.modifier), unsigned(eglGetError()));
    }

    void DropTarget(Target& t) {
        if (t.framebuffer) glDeleteFramebuffers(1, &t.framebuffer);
        if (t.renderbuffer) glDeleteRenderbuffers(1, &t.renderbuffer);
        if (t.image != EGL_NO_IMAGE_KHR) eglDestroyImageKHR(display, t.image);
        if (t.fd >= 0) close(t.fd);
        if (t.bo) gbm_bo_destroy(t.bo);
        t = Target{};
    }

    bool MakeTarget(Target& t, uint32_t width, uint32_t height) {
        t.bo = gbm_bo_create(gbm, width, height, GBM_FORMAT_XRGB8888,
            GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
        if (!t.bo) return false;
        t.fd = gbm_bo_get_fd(t.bo);
        t.stride = gbm_bo_get_stride(t.bo);
        const uint64_t modifier = gbm_bo_get_modifier(t.bo);
        t.modifier = modifier == DRM_FORMAT_MOD_INVALID ? DRM_FORMAT_MOD_LINEAR : modifier;
        const uint32_t offset = 0;
        const std::vector<EGLint> attrs = ImportAttributes(width, height, DRM_FORMAT_XRGB8888,
            t.modifier, 1, &t.fd, &offset, &t.stride);
        t.image =
            eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
        if (t.fd < 0 || t.image == EGL_NO_IMAGE_KHR) return false;
        glGenRenderbuffers(1, &t.renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, t.renderbuffer);
        glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, t.image);
        glGenFramebuffers(1, &t.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, t.framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
            t.renderbuffer);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    bool EnsureTargets(uint32_t width, uint32_t height) {
        if (!targets.empty() && targetW == width && targetH == height) return true;
        for (Target& t : targets) DropTarget(t);
        targets.assign(kTargetCount, Target{});
        targetW = width;
        targetH = height;
        for (Target& t : targets) {
            if (MakeTarget(t, width, height)) continue;
            LOGE("[KMS] Could not allocate a %ux%u linear frame for the encoder.", width,
                height);
            for (Target& made : targets) DropTarget(made);
            targets.clear();
            return false;
        }
        return true;
    }

    void DrawScreen(GLuint texture, const KmsScanout& scanout, uint32_t fbW, uint32_t fbH) {
        glDisable(GL_BLEND);
        glUniform1f(opaqueUniform, 1.0f);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
        const Rect target{0, 0, float(scanout.width), float(scanout.height)};
        const Rect source{float(scanout.x) / float(fbW), float(scanout.y) / float(fbH),
            float(scanout.x + scanout.width) / float(fbW),
            float(scanout.y + scanout.height) / float(fbH)};
        DrawQuad(target, source, targetW, targetH);
    }

    void DrawCursor(KmsBuffer cursorBuffer, const KmsCursor& cursor) {
        const float srcW = cursor.srcWidth ? float(cursor.srcWidth) : float(cursorBuffer.width);
        const float srcH = cursor.srcHeight ? float(cursor.srcHeight) : float(cursorBuffer.height);
        const Rect source{0, 0, srcW / float(cursorBuffer.width), srcH / float(cursorBuffer.height)};
        const GLuint texture = TextureFor(std::move(cursorBuffer));
        if (!texture) return;
        glEnable(GL_BLEND);
        glUniform1f(opaqueUniform, 0.0f);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
        const Rect target{float(cursor.x), float(cursor.y), float(cursor.x) + float(cursor.width),
            float(cursor.y) + float(cursor.height)};
        DrawQuad(target, source, targetW, targetH);
    }

    LinuxFrameInfo FrameOf(const Target& t) {
        LinuxFrameInfo fi;
        fi.memory = FrameMemory::DmaBuf;
        fi.planes[0] = DmaPlane{t.fd, 0, t.stride};
        fi.planeCount = 1;
        fi.modifier = t.modifier;
        fi.drmFormat = DRM_FORMAT_XRGB8888;
        fi.stride = t.stride;
        fi.meta.width = targetW;
        fi.meta.height = targetH;
        fi.meta.timestampUs = NowUs();
        fi.meta.frameId = ++frameId;
        return fi;
    }

    void Release() {
        if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
            for (ImportedImage& entry : images) Forget(entry);
            for (Target& t : targets) DropTarget(t);
            if (program) glDeleteProgram(program);
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
        }
        images.clear();
        targets.clear();
        if (display != EGL_NO_DISPLAY) eglTerminate(display);
        if (gbm) gbm_device_destroy(gbm);
        if (renderFd >= 0) close(renderFd);
        *this = Impl{};
    }
};

KmsBlitter::KmsBlitter() : impl_(std::make_unique<Impl>()) {}

KmsBlitter::~KmsBlitter() {
    Close();
}

bool KmsBlitter::Open(const std::string& renderNode) {
    Close();
    if (impl_->OpenDisplay(renderNode) && impl_->OpenContext()) return true;
    LOGE(
        "[KMS] %s cannot import scanout buffers through EGL (dma-buf import with modifiers, "
        "surfaceless GLES 2 and external images are all needed).",
        renderNode.c_str());
    Close();
    return false;
}

void KmsBlitter::Close() {
    impl_->Release();
}

std::optional<LinuxFrameInfo> KmsBlitter::Compose(const KmsScanout& scanout, KmsBuffer screen,
    std::optional<KmsBuffer> cursor) {
    if (!screen.width || !screen.height || !scanout.width || !scanout.height) return std::nullopt;
    if (!impl_->EnsureTargets(scanout.width, scanout.height)) return std::nullopt;
    const uint32_t fbW = screen.width;
    const uint32_t fbH = screen.height;
    const GLuint texture = impl_->TextureFor(std::move(screen));
    if (!texture) return std::nullopt;

    Target& t = impl_->targets[impl_->nextTarget++ % impl_->targets.size()];
    glBindFramebuffer(GL_FRAMEBUFFER, t.framebuffer);
    glViewport(0, 0, GLsizei(impl_->targetW), GLsizei(impl_->targetH));
    impl_->DrawScreen(texture, scanout, fbW, fbH);
    if (cursor && scanout.cursor) impl_->DrawCursor(std::move(*cursor), *scanout.cursor);
    glFinish();
    return impl_->FrameOf(t);
}
