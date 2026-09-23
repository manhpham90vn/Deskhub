#include "render/VideoRenderer.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>
}

#include <drm_fourcc.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "deskhub/media/ViewFit.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace {

const float kQuad[] = {
    -1.f,
    -1.f,
    0.f,
    1.f,
    1.f,
    -1.f,
    1.f,
    1.f,
    -1.f,
    1.f,
    0.f,
    0.f,
    1.f,
    1.f,
    1.f,
    0.f,
};

const char* kVertexBody = R"(
in vec2 aPos;
in vec2 aTex;
out vec2 vTex;
void main() {
    vTex = aTex;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* kFragmentBody = R"(
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;
uniform int planarUv;
in vec2 vTex;
out vec4 fragColor;
void main() {
    float y = texture(texY, vTex).r;
    float u, v;
    if (planarUv == 1) {
        u = texture(texU, vTex).r;
        v = texture(texV, vTex).r;
    } else {
        vec2 uv = texture(texU, vTex).rg;
        u = uv.x;
        v = uv.y;
    }
    float Y  = (y - 0.0627451) * 1.1643836;
    float Cb = (u - 0.5019608) * 1.1383929;
    float Cr = (v - 0.5019608) * 1.1383929;
    fragColor = vec4(Y + 1.5748 * Cr,
                     Y - 0.1873243 * Cb - 0.4681243 * Cr,
                     Y + 1.8556 * Cb,
                     1.0);
}
)";

const char* ShaderHeader(bool fragment) {
    if (epoxy_is_desktop_gl()) return "#version 150\n";
    return fragment ? "#version 300 es\nprecision mediump float;\n" : "#version 300 es\n";
}

AVFrame* CopyToSystemMemory(AVFrame* src) {
    AVFrame* dst = av_frame_alloc();
    if (!dst) return nullptr;
    dst->format = AV_PIX_FMT_NV12;
    dst->width = src->width;
    dst->height = src->height;
    if (av_frame_get_buffer(dst, 0) < 0 || av_hwframe_transfer_data(dst, src, 0) < 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOGE("[Render] Could not copy the decoded surface into system memory.");
        }
        av_frame_free(&dst);
        return nullptr;
    }
    av_frame_copy_props(dst, src);
    return dst;
}

unsigned CompileShader(GLenum type, const char* header, const char* body) {
    const unsigned id = glCreateShader(type);
    const char* parts[] = {header, body};
    glShaderSource(id, 2, parts, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(id, sizeof(log) - 1, nullptr, log);
        LOGE("[Render] Shader compile failed: %s", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

}

VideoRenderer::~VideoRenderer() {
    std::lock_guard<std::mutex> lk(mutex_);
    ClearSlotLocked();
}

void VideoRenderer::SetVaDisplay(void* vaDisplay) {
    vaDisplay_ = vaDisplay;
}

void VideoRenderer::ClearSlotLocked() {
    if (dmabuf_) {
        for (uint32_t i = 0; i < desc_.num_objects; ++i)
            if (desc_.objects[i].fd >= 0) close(desc_.objects[i].fd);
        desc_ = VADRMPRIMESurfaceDescriptor{};
        dmabuf_ = false;
    }
    if (frame_) {
        auto* f = static_cast<AVFrame*>(frame_);
        av_frame_free(&f);
        frame_ = nullptr;
    }
}

void VideoRenderer::DropFrames() {
    std::lock_guard<std::mutex> lk(mutex_);
    ClearSlotLocked();
    frameSerial_.fetch_add(1, std::memory_order_release);
}

void VideoRenderer::SubmitFrame(void* avFrame, uint64_t ptsUs) {
    auto* f = static_cast<AVFrame*>(avFrame);
    if (!f) return;

    bool isDma = false;
    VADRMPRIMESurfaceDescriptor desc{};
    if (f->format == AV_PIX_FMT_VAAPI) {
        const bool canImport = vaDisplay_ && !dmaImportBroken_.load(std::memory_order_acquire);
        if (!canImport) {
            AVFrame* copy = CopyToSystemMemory(f);
            av_frame_free(&f);
            if (!copy) return;
            f = copy;
        } else {
            const auto surface = VASurfaceID(uintptr_t(f->data[3]));
            const VAStatus st = vaExportSurfaceHandle(static_cast<VADisplay>(vaDisplay_), surface,
                VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &desc);
            if (st == VA_STATUS_SUCCESS) {
                isDma = true;
            } else {
                static bool warned = false;
                if (!warned) {
                    warned = true;
                    LOGW(
                        "[Render] vaExportSurfaceHandle failed (%s) — copying frames through "
                        "system memory instead.",
                        vaErrorStr(st));
                }
                dmaImportBroken_.store(true, std::memory_order_release);
                AVFrame* copy = CopyToSystemMemory(f);
                av_frame_free(&f);
                if (!copy) return;
                f = copy;
            }
        }
    }

    std::lock_guard<std::mutex> lk(mutex_);
    ClearSlotLocked();
    frame_ = f;
    dmabuf_ = isDma;
    desc_ = desc;
    ptsUs_ = ptsUs;
    frameSerial_.fetch_add(1, std::memory_order_release);
}

bool VideoRenderer::Realize() {
    if (glReady_) return true;

    const unsigned vs = CompileShader(GL_VERTEX_SHADER, ShaderHeader(false), kVertexBody);
    const unsigned fs = CompileShader(GL_FRAGMENT_SHADER, ShaderHeader(true), kFragmentBody);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aTex");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetProgramInfoLog(program_, sizeof(log) - 1, nullptr, log);
        LOGE("[Render] Shader link failed: %s", log);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "texY"), 0);
    glUniform1i(glGetUniformLocation(program_, "texU"), 1);
    glUniform1i(glGetUniformLocation(program_, "texV"), 2);
    uPlanarUv_ = glGetUniformLocation(program_, "planarUv");

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenTextures(3, tex_);
    for (unsigned t : tex_) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glReady_ = true;
    uploadedSerial_ = 0;
    LOGI("[Render] OpenGL ready (%s).", epoxy_is_desktop_gl() ? "desktop GL" : "GLES");
    return true;
}

void VideoRenderer::Unrealize() {
    if (!glReady_) return;
    glDeleteTextures(3, tex_);
    tex_[0] = tex_[1] = tex_[2] = 0;
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    vbo_ = vao_ = program_ = 0;
    glReady_ = false;
}

bool VideoRenderer::UploadLocked() {
    auto* f = static_cast<AVFrame*>(frame_);
    if (!f) return false;

    if (dmabuf_) {
        const EGLDisplay edpy = eglGetCurrentDisplay();
        if (edpy == EGL_NO_DISPLAY) {
            LOGE("[Render] No current EGL display — cannot import dma-buf.");
            return false;
        }
        const uint32_t nLayers = desc_.num_layers > 2 ? 2 : desc_.num_layers;
        for (uint32_t i = 0; i < nLayers; ++i) {
            const auto& layer = desc_.layers[i];
            const uint32_t obj = layer.object_index[0];
            const uint32_t lw = i == 0 ? desc_.width : desc_.width / 2;
            const uint32_t lh = i == 0 ? desc_.height : desc_.height / 2;
            const uint64_t mod = desc_.objects[obj].drm_format_modifier;

            EGLint attrs[] = {
                EGL_WIDTH, EGLint(lw),
                EGL_HEIGHT, EGLint(lh),
                EGL_LINUX_DRM_FOURCC_EXT, EGLint(layer.drm_format),
                EGL_DMA_BUF_PLANE0_FD_EXT, EGLint(desc_.objects[obj].fd),
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGLint(layer.offset[0]),
                EGL_DMA_BUF_PLANE0_PITCH_EXT, EGLint(layer.pitch[0]),
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGLint(mod & 0xFFFFFFFFu),
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGLint(mod >> 32),
                EGL_NONE};
            EGLImageKHR img = eglCreateImageKHR(edpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                nullptr, attrs);
            if (img == EGL_NO_IMAGE_KHR) {
                if (!dmaImportBroken_.exchange(true, std::memory_order_acq_rel))
                    LOGW(
                        "[Render] The display GPU cannot import the decoder's dma-buf (layer %u, "
                        "fourcc 0x%x, modifier 0x%llx, EGL error 0x%x) — decode and display are "
                        "likely on different GPUs. Copying frames through system memory instead.",
                        i, layer.drm_format, (unsigned long long)mod, eglGetError());
                return false;
            }
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, tex_[i]);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);
            eglDestroyImageKHR(edpy, img);
        }
        glUniform1i(uPlanarUv_, 0);
        return true;
    }

    const bool planar = f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P;
    if (!planar && f->format != AV_PIX_FMT_NV12) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            LOGE("[Render] Unsupported software pixel format %d.", f->format);
        }
        return false;
    }

    struct Plane {
        const uint8_t* data;
        int stride;
        uint32_t w, h;
        GLenum internalFmt, fmt;
    };
    const uint32_t w = uint32_t(f->width), h = uint32_t(f->height);
    Plane planes[3];
    uint32_t nPlanes = 0;
    planes[nPlanes++] = {f->data[0], f->linesize[0], w, h, GL_R8, GL_RED};
    if (planar) {
        planes[nPlanes++] = {f->data[1], f->linesize[1], w / 2, h / 2, GL_R8, GL_RED};
        planes[nPlanes++] = {f->data[2], f->linesize[2], w / 2, h / 2, GL_R8, GL_RED};
    } else {
        planes[nPlanes++] = {f->data[1], f->linesize[1], w / 2, h / 2, GL_RG8, GL_RG};
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (uint32_t i = 0; i < nPlanes; ++i) {
        const Plane& p = planes[i];
        if (!p.data || p.stride <= 0) return false;
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, tex_[i]);
        const int pixelSize = p.fmt == GL_RG ? 2 : 1;
        glPixelStorei(GL_UNPACK_ROW_LENGTH, p.stride / pixelSize);
        glTexImage2D(GL_TEXTURE_2D, 0, GLint(p.internalFmt), GLsizei(p.w), GLsizei(p.h), 0, p.fmt,
            GL_UNSIGNED_BYTE, p.data);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glUniform1i(uPlanarUv_, planar ? 1 : 0);
    return true;
}

void VideoRenderer::ClearBlack() {
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool VideoRenderer::Render(int viewW, int viewH) {
    if (!glReady_ || viewW <= 0 || viewH <= 0) return false;

    std::lock_guard<std::mutex> lk(mutex_);
    if (!frame_) return false;
    auto* f = static_cast<AVFrame*>(frame_);
    const int fw = f->width, fh = f->height;
    if (fw <= 0 || fh <= 0) return false;

    glUseProgram(program_);
    const uint64_t serial = frameSerial_.load(std::memory_order_acquire);
    const bool fresh = serial != uploadedSerial_;
    if (fresh) {
        if (!UploadLocked()) return false;
        uploadedSerial_ = serial;
    }

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glViewport(0, 0, viewW, viewH);
    glClear(GL_COLOR_BUFFER_BIT);

    const deskhub::ViewRect rect = deskhub::FitVideoRect(double(viewW), double(viewH),
        double(fw) / double(fh));
    glViewport(int(rect.x), int(rect.y), int(rect.width), int(rect.height));

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    if (fresh) counters_.FramePresented(ptsUs_, NowUs());
    return true;
}
