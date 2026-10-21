#include <EGL/egl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#undef None
#undef Status
#undef Success
#undef Always
#undef Bool
#undef BadName

#include "ViewerRun.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Output.h"
#include "Signals.h"

#include "decode/AvDecoder.h"
#include "render/VideoRenderer.h"

#include "deskhub/input/LocalCursor.h"
#include "deskhub/input/PointerMap.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/media/ViewerTitle.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ClientFfi.h"
#include "deskhubp/client/ScreenViewer.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {
constexpr ::Cursor kNoCursor = 0;
constexpr long kNetWmStateToggle = 2;
constexpr long kNetWmSourceApplication = 1;
}

namespace deskhubcli {

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 720;
constexpr int kMinWidth = 320;
constexpr int kMinHeight = 240;
constexpr int kFrameSleepMs = 8;
constexpr int32_t kWheelDelta = deskhub::kWheelDeltaPerNotch;
constexpr unsigned kEvdevKeycodeOffset = 8;

using Engine = deskhubp::ScreenViewer<AvDecoder, VideoSink*>;

struct EglConfigChoice {
    EGLConfig config = nullptr;
    EGLint contextApi = EGL_OPENGL_API;
    EGLint contextMajor = 3;
    bool found = false;
};

EglConfigChoice ChooseEglConfig(EGLDisplay display) {
    EglConfigChoice choice;

    const EGLint desktopAttrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    const EGLint glesAttrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};

    EGLint count = 0;
    if (eglChooseConfig(display, desktopAttrs, &choice.config, 1, &count) && count > 0) {
        choice.contextApi = EGL_OPENGL_API;
        choice.contextMajor = 3;
        choice.found = true;
        return choice;
    }
    if (eglChooseConfig(display, glesAttrs, &choice.config, 1, &count) && count > 0) {
        choice.contextApi = EGL_OPENGL_ES_API;
        choice.contextMajor = 3;
        choice.found = true;
    }
    return choice;
}

struct Screen {
    ::Window window = 0;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    VideoRenderer renderer{};
    std::unique_ptr<Engine> engine{};
    deskhub::LocalCursorState cursor{};

    std::string baseTitle{};
    std::string statusLine{};
    std::string shownTitle{};
    int width = kInitialWidth;
    int height = kInitialHeight;
    uint32_t fittedW = 0;
    uint32_t fittedH = 0;
    bool control = true;
    bool realized = false;
    bool closed = false;
    bool cursorBlanked = false;
    bool pointerOverVideo = false;
    std::string endReason{};
};

class ViewerSession {
public:
    ~ViewerSession() {
        Shutdown();
    }

    bool Open(const ViewRequest& request);
    int Run();

private:
    bool OpenWindow(const deskhub::SourceInfo& source, const ViewRequest& request);
    void Shutdown();

    Screen* Find(::Window window);
    bool MakeCurrent(Screen& screen);
    void Render(Screen& screen);
    void UpdateTitle(Screen& screen);
    void SizeToVideo(Screen& screen);
    void CloseScreen(Screen& screen);

    void HandleEvent(const XEvent& event);
    void OnKey(Screen& screen, const XKeyEvent& key, bool down);
    void OnMotion(Screen& screen, const XMotionEvent& motion);
    void OnButton(Screen& screen, const XButtonEvent& button, bool down);
    void ToggleFullScreen(Screen& screen);
    void ToggleLocalCursor(Screen& screen);
    void ApplyLocalCursor(Screen& screen);
    deskhub::ViewRect VideoRect(const Screen& screen) const;

    ::Display* display_ = nullptr;
    EGLDisplay egl_ = EGL_NO_DISPLAY;
    EglConfigChoice choice_{};
    Atom deleteWindow_ = 0;
    Atom wmState_ = 0;
    Atom wmStateFullScreen_ = 0;
    ::Cursor blankCursor_ = kNoCursor;
    std::vector<std::unique_ptr<Screen>> screens_{};
    bool anyRefused_ = false;
};

bool ViewerSession::Open(const ViewRequest& request) {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        PrintError(
            "No X display to draw on. Set DISPLAY, or use 'shell' if you only need a prompt.");
        return false;
    }

    egl_ = eglGetDisplay(EGLNativeDisplayType(display_));
    if (egl_ == EGL_NO_DISPLAY || !eglInitialize(egl_, nullptr, nullptr)) {
        PrintError("EGL would not start on this display.");
        return false;
    }

    choice_ = ChooseEglConfig(egl_);
    if (!choice_.found) {
        PrintError("No EGL configuration on this display can show video.");
        return false;
    }

    deleteWindow_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    wmState_ = XInternAtom(display_, "_NET_WM_STATE", False);
    wmStateFullScreen_ = XInternAtom(display_, "_NET_WM_STATE_FULLSCREEN", False);

    const char emptyBits[1] = {0};
    const ::Window root = DefaultRootWindow(display_);
    const Pixmap empty = XCreateBitmapFromData(display_, root, emptyBits, 1, 1);
    XColor black{};
    blankCursor_ = XCreatePixmapCursor(display_, empty, empty, &black, &black, 0, 0);
    XFreePixmap(display_, empty);

    for (const deskhub::SourceInfo& source : request.sources)
        if (!OpenWindow(source, request)) return false;
    return !screens_.empty();
}

bool ViewerSession::OpenWindow(const deskhub::SourceInfo& source, const ViewRequest& request) {
    auto screen = std::make_unique<Screen>();
    screen->control = request.control;
    screen->baseTitle = deskhub::ViewerBaseTitle(
        deskhub::media::SourceName(source.name, source.sourceId));

    EGLint visualId = 0;
    eglGetConfigAttrib(egl_, choice_.config, EGL_NATIVE_VISUAL_ID, &visualId);

    XVisualInfo wanted{};
    wanted.visualid = VisualID(visualId);
    int visualCount = 0;
    XVisualInfo* visuals = XGetVisualInfo(display_, VisualIDMask, &wanted, &visualCount);
    if (!visuals || visualCount <= 0) {
        if (visuals) XFree(visuals);
        PrintError("EGL picked a visual this X server does not know.");
        return false;
    }

    XSetWindowAttributes attrs{};
    attrs.colormap = XCreateColormap(display_, RootWindow(display_, visuals->screen),
        visuals->visual, AllocNone);
    attrs.background_pixel = BlackPixel(display_, visuals->screen);
    attrs.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | StructureNotifyMask | FocusChangeMask | ExposureMask;

    screen->window = XCreateWindow(display_, RootWindow(display_, visuals->screen), 0, 0,
        unsigned(kInitialWidth), unsigned(kInitialHeight), 0, visuals->depth, InputOutput,
        visuals->visual, CWColormap | CWBackPixel | CWEventMask, &attrs);
    XFree(visuals);

    if (!screen->window) {
        PrintError("The X server would not open a window.");
        return false;
    }

    XSetWMProtocols(display_, screen->window, &deleteWindow_, 1);
    XStoreName(display_, screen->window, screen->baseTitle.c_str());

    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = kMinWidth;
    hints.min_height = kMinHeight;
    XSetWMNormalHints(display_, screen->window, &hints);

    eglBindAPI(EGLenum(choice_.contextApi));
    screen->surface =
        eglCreateWindowSurface(egl_, choice_.config, EGLNativeWindowType(screen->window), nullptr);
    if (screen->surface == EGL_NO_SURFACE) {
        PrintError("EGL would not draw on that window.");
        return false;
    }

    const EGLint contextAttrs[] = {EGL_CONTEXT_MAJOR_VERSION, choice_.contextMajor, EGL_NONE};
    screen->context = eglCreateContext(egl_, choice_.config, EGL_NO_CONTEXT, contextAttrs);
    if (screen->context == EGL_NO_CONTEXT) {
        PrintError("EGL would not make a drawing context.");
        return false;
    }

    XMapWindow(display_, screen->window);

    Screen* raw = screen.get();
    screens_.push_back(std::move(screen));

    if (!MakeCurrent(*raw)) return false;
    raw->renderer.Realize();
    raw->realized = true;
    eglSwapInterval(egl_, 0);

    deskhubp::ScreenViewerConfig config;
    config.server = request.server;
    config.sourceId = source.sourceId;
    config.screenW = uint32_t(DisplayWidth(display_, DefaultScreen(display_)));
    config.screenH = uint32_t(DisplayHeight(display_, DefaultScreen(display_)));
    config.displayName = request.displayName;
    config.wantsAudio = request.audio;
    config.onStatus = [raw](const char* status) { raw->statusLine = status ? status : ""; };
    config.onEnded = [raw](const char* reason) {
        raw->endReason = reason ? reason : "";
        raw->closed = true;
    };
    config.onFinished = [raw](const char* reason) {
        if (raw->endReason.empty() && reason) raw->endReason = reason;
        raw->closed = true;
    };
    raw->engine = std::make_unique<Engine>();
    raw->engine->SetSurface(&raw->renderer);
    if (!raw->engine->Start(config)) {
        PrintError(deskhub::ui::CouldNotConnectTo(request.hostLabel));
        return false;
    }

    UpdateTitle(*raw);
    return true;
}

bool ViewerSession::MakeCurrent(Screen& screen) {
    eglBindAPI(EGLenum(choice_.contextApi));
    if (eglMakeCurrent(egl_, screen.surface, screen.surface, screen.context)) return true;
    LOGE("[Viewer] Could not make the EGL context current.");
    return false;
}

Screen* ViewerSession::Find(::Window window) {
    for (const std::unique_ptr<Screen>& screen : screens_)
        if (screen->window == window) return screen.get();
    return nullptr;
}

deskhub::ViewRect ViewerSession::VideoRect(const Screen& screen) const {
    const double vw = double(screen.engine->videoWidth());
    const double vh = double(screen.engine->videoHeight());
    if (vw <= 0 || vh <= 0) return deskhub::ViewRect{0, 0, double(screen.width), double(screen.height)};
    return deskhub::FitVideoRect(screen.width, screen.height, vw / vh);
}

void ViewerSession::UpdateTitle(Screen& screen) {
    std::string title = screen.control
                            ? deskhub::ComposeViewerTitle(screen.baseTitle, screen.statusLine)
                            : deskhub::ComposeViewerTitle(screen.baseTitle, screen.statusLine,
                                  deskhub::kViewerViewOnlyHint);
    if (title == screen.shownTitle) return;
    screen.shownTitle = std::move(title);
    XStoreName(display_, screen.window, screen.shownTitle.c_str());
}

void ViewerSession::SizeToVideo(Screen& screen) {
    const uint32_t vw = screen.engine->videoWidth();
    const uint32_t vh = screen.engine->videoHeight();
    if (!vw || !vh) return;
    if (!deskhub::ShouldRefitViewer(screen.fittedW, screen.fittedH, vw, vh)) return;
    screen.fittedW = vw;
    screen.fittedH = vh;

    const int rootW = DisplayWidth(display_, DefaultScreen(display_));
    const int rootH = DisplayHeight(display_, DefaultScreen(display_));
    const deskhub::ViewSize fitted = deskhub::ScaleToFit(vw, vh,
        uint32_t(std::max(kMinWidth, rootW - deskhub::kViewerMarginPx)),
        uint32_t(std::max(kMinHeight, rootH - deskhub::kViewerMarginPx)));
    XResizeWindow(display_, screen.window, unsigned(fitted.width), unsigned(fitted.height));
}

void ViewerSession::Render(Screen& screen) {
    if (screen.closed || !screen.realized) return;
    if (!MakeCurrent(screen)) return;
    if (!screen.renderer.Render(screen.width, screen.height)) screen.renderer.ClearBlack();
    eglSwapBuffers(egl_, screen.surface);
}

void ViewerSession::ToggleFullScreen(Screen& screen) {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = screen.window;
    event.xclient.message_type = wmState_;
    event.xclient.format = 32;
    event.xclient.data.l[0] = kNetWmStateToggle;
    event.xclient.data.l[1] = long(wmStateFullScreen_);
    event.xclient.data.l[3] = kNetWmSourceApplication;
    XSendEvent(display_, DefaultRootWindow(display_), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void ViewerSession::ToggleLocalCursor(Screen& screen) {
    if (!screen.control) return;
    screen.cursor.Toggle();
    ApplyLocalCursor(screen);
}

void ViewerSession::ApplyLocalCursor(Screen& screen) {
    const bool blank = screen.cursor.HidesOverVideo(screen.pointerOverVideo);
    if (blank == screen.cursorBlanked) return;
    if (blank)
        XDefineCursor(display_, screen.window, blankCursor_);
    else
        XUndefineCursor(display_, screen.window);
    screen.cursorBlanked = blank;
}

void ViewerSession::OnKey(Screen& screen, const XKeyEvent& key, bool down) {
    const unsigned code = key.keycode;
    int32_t vk = 0, scan = 0;
    if (code < kEvdevKeycodeOffset) return;
    if (!dh_native_key_to_vk(uint16_t(code - kEvdevKeycodeOffset), &vk, &scan)) return;
    if (deskhub::IsViewerShortcutVk(vk)) {
        if (down && vk == deskhub::kViewerFullScreenToggleVk) ToggleFullScreen(screen);
        if (down && vk == deskhub::kViewerCursorToggleVk) ToggleLocalCursor(screen);
        return;
    }
    if (screen.control) screen.engine->QueueKey(vk, scan, down);
}

void ViewerSession::OnMotion(Screen& screen, const XMotionEvent& motion) {
    if (!screen.control) return;

    int32_t nx = 0, ny = 0;
    screen.pointerOverVideo =
        deskhub::NormalizePointer(motion.x, motion.y, VideoRect(screen), nx, ny);
    ApplyLocalCursor(screen);
    if (screen.pointerOverVideo) screen.engine->QueueMouseMoveAbs(nx, ny);
}

void ViewerSession::OnButton(Screen& screen, const XButtonEvent& button, bool down) {
    if (!screen.control) return;

    if (button.button == Button4 || button.button == Button5) {
        if (down) screen.engine->QueueMouseWheel(button.button == Button4 ? kWheelDelta : -kWheelDelta);
        return;
    }

    deskhub::MouseButton mouse{};
    if (!deskhub::X11ButtonToMouseButton(button.button, mouse)) return;

    int32_t nx = 0, ny = 0;
    if (deskhub::NormalizePointer(button.x, button.y, VideoRect(screen), nx, ny))
        screen.engine->QueueMouseMoveAbs(nx, ny);
    screen.engine->QueueMouseButton(int32_t(mouse), down);
}

void ViewerSession::CloseScreen(Screen& screen) {
    if (screen.closed) return;
    screen.closed = true;
    if (screen.engine) screen.engine->Stop();
}

void ViewerSession::HandleEvent(const XEvent& event) {
    Screen* screen = Find(event.xany.window);
    if (!screen) return;

    switch (event.type) {
        case ClientMessage:
            if (Atom(event.xclient.data.l[0]) == deleteWindow_) CloseScreen(*screen);
            break;
        case ConfigureNotify:
            screen->width = std::max(1, event.xconfigure.width);
            screen->height = std::max(1, event.xconfigure.height);
            break;
        case KeyPress: OnKey(*screen, event.xkey, true); break;
        case KeyRelease: OnKey(*screen, event.xkey, false); break;
        case MotionNotify: OnMotion(*screen, event.xmotion); break;
        case ButtonPress: OnButton(*screen, event.xbutton, true); break;
        case ButtonRelease: OnButton(*screen, event.xbutton, false); break;
        case FocusOut: screen->engine->ReleaseAllInput(); break;
        default: break;
    }
}

int ViewerSession::Run() {
    while (!Interrupted()) {
        while (XPending(display_)) {
            XEvent event;
            XNextEvent(display_, &event);
            HandleEvent(event);
        }

        bool anyOpen = false;
        for (const std::unique_ptr<Screen>& screen : screens_) {
            if (screen->closed) continue;
            anyOpen = true;
            SizeToVideo(*screen);
            UpdateTitle(*screen);
            Render(*screen);
        }
        if (!anyOpen) break;

        XFlush(display_);
        std::this_thread::sleep_for(std::chrono::milliseconds(kFrameSleepMs));
    }

    for (const std::unique_ptr<Screen>& screen : screens_)
        if (!screen->endReason.empty()) anyRefused_ = true;

    if (anyRefused_) return int(ExitCode::Ok);
    return int(ExitCode::Ok);
}

void ViewerSession::Shutdown() {
    for (const std::unique_ptr<Screen>& screen : screens_) {
        if (screen->engine) screen->engine->Stop();
        if (screen->realized && MakeCurrent(*screen)) screen->renderer.Unrealize();
        if (screen->context != EGL_NO_CONTEXT) eglDestroyContext(egl_, screen->context);
        if (screen->surface != EGL_NO_SURFACE) eglDestroySurface(egl_, screen->surface);
        if (screen->window && display_) XDestroyWindow(display_, screen->window);
    }
    screens_.clear();

    if (blankCursor_ != kNoCursor && display_) {
        XFreeCursor(display_, blankCursor_);
        blankCursor_ = kNoCursor;
    }
    if (egl_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(egl_);
        egl_ = EGL_NO_DISPLAY;
    }
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

}

ExitCode RunViewers(const ViewRequest& request) {
    WatchForInterrupt();

    ViewerSession session;
    if (!session.Open(request)) return ExitCode::Failed;
    return ExitCode(session.Run());
}

}
