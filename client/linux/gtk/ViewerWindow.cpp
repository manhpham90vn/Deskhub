#include "gtk/ViewerWindow.h"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ClientFfi.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "gtk/GtkUtil.h"

#include "deskhub/input/LocalCursor.h"
#include "deskhub/input/PointerMap.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/media/ViewerTitle.h"
#include "deskhub/ui/Strings.h"

namespace {

uint16_t GdkKeycodeToEvdev(uint32_t hardwareKeycode) {
    return hardwareKeycode >= 8 ? uint16_t(hardwareKeycode - 8) : 0;
}

constexpr int32_t kWheelDelta = deskhub::kWheelDeltaPerNotch;

constexpr int kInitialW = 1024;
constexpr int kInitialH = 600;

constexpr const char* kStatusSeparator = " \xC2\xB7 ";

GdkRectangle WorkArea(GtkWidget* w) {
    GdkRectangle wa{0, 0, kInitialW, kInitialH};
    GdkDisplay* d = gtk_widget_get_display(w);
    if (!d) return wa;
    GdkMonitor* m = nullptr;
    if (GdkWindow* gw = gtk_widget_get_window(w)) m = gdk_display_get_monitor_at_window(d, gw);
    if (!m) m = gdk_display_get_primary_monitor(d);
    if (!m) m = gdk_display_get_monitor(d, 0);
    if (m) gdk_monitor_get_workarea(m, &wa);
    return wa;
}

void LargestScreenPixels(GtkWidget* w, uint32_t& outW, uint32_t& outH) {
    outW = outH = 0;
    GdkDisplay* d = gtk_widget_get_display(w);
    if (!d) return;
    const int n = gdk_display_get_n_monitors(d);
    long bestArea = 0;
    for (int i = 0; i < n; ++i) {
        GdkMonitor* m = gdk_display_get_monitor(d, i);
        if (!m) continue;
        GdkRectangle g{};
        gdk_monitor_get_geometry(m, &g);
        const int s = gdk_monitor_get_scale_factor(m);
        const long pw = long(g.width) * (s > 0 ? s : 1), ph = long(g.height) * (s > 0 ? s : 1);
        if (pw <= 0 || ph <= 0 || pw * ph <= bestArea) continue;
        bestArea = pw * ph;
        outW = uint32_t(pw);
        outH = uint32_t(ph);
    }
}

}

ViewerWindow* ViewerWindow::Open(const NetAddr& server, uint8_t sourceId,
    const std::string& sourceName, bool control, std::function<void()> onClosed) {
    auto* v = new ViewerWindow();
    v->onClosed_ = std::move(onClosed);
    if (!v->Build(server, sourceId, sourceName, control)) {
        delete v;
        return nullptr;
    }
    return v;
}

ViewerWindow::~ViewerWindow() {
    if (alive_) *alive_ = nullptr;
    loop_.Stop();
}

void ViewerWindow::PostToMain(std::function<void(ViewerWindow&)> fn) {
    RunOnMain([token = alive_, fn = std::move(fn)] {
        if (ViewerWindow* self = *token) fn(*self);
    });
}

bool ViewerWindow::Build(const NetAddr& server, uint8_t sourceId, const std::string& sourceName,
    bool control) {
    control_ = control;
    baseTitle_ = deskhub::ViewerBaseTitle(sourceName);
    alive_ = std::make_shared<ViewerWindow*>(this);

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), baseTitle_.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window_), kInitialW, kInitialH);

    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    GtkWidget* fullScreenButton = gtk_button_new_with_label(deskhub::ui::kFullScreenButton);
    g_signal_connect(fullScreenButton, "clicked", G_CALLBACK(OnFullScreenClicked), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), fullScreenButton);
    if (control_) {
        cursorButton_ = gtk_button_new_with_label(cursor_.ButtonLabel());
        g_signal_connect(cursorButton_, "clicked", G_CALLBACK(OnCursorClicked), this);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header), cursorButton_);
    }
    linkLabel_ = gtk_label_new("");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), linkLabel_);
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    glArea_ = gtk_gl_area_new();
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(glArea_), FALSE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(glArea_), FALSE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(glArea_), FALSE);
    gtk_container_add(GTK_CONTAINER(window_), glArea_);
    gtk_widget_add_events(glArea_, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(glArea_, "enter-notify-event", G_CALLBACK(OnCrossing), this);
    g_signal_connect(glArea_, "leave-notify-event", G_CALLBACK(OnCrossing), this);

    g_signal_connect(glArea_, "render", G_CALLBACK(OnRender), this);
    g_signal_connect(glArea_, "realize", G_CALLBACK(OnRealize), this);
    g_signal_connect(glArea_, "unrealize", G_CALLBACK(OnUnrealize), this);

    gtk_widget_add_events(window_, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
                                       GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK |
                                       GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK |
                                       GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
    g_signal_connect(window_, "key-press-event", G_CALLBACK(OnKey), this);
    g_signal_connect(window_, "key-release-event", G_CALLBACK(OnKey), this);
    g_signal_connect(window_, "motion-notify-event", G_CALLBACK(OnMotion), this);
    g_signal_connect(window_, "button-press-event", G_CALLBACK(OnButton), this);
    g_signal_connect(window_, "button-release-event", G_CALLBACK(OnButton), this);
    g_signal_connect(window_, "scroll-event", G_CALLBACK(OnScroll), this);
    g_signal_connect(window_, "focus-in-event", G_CALLBACK(OnFocusIn), this);
    g_signal_connect(window_, "focus-out-event", G_CALLBACK(OnFocusOut), this);
    g_signal_connect(window_, "window-state-event", G_CALLBACK(OnWindowState), this);
    g_signal_connect(window_, "destroy", G_CALLBACK(OnDestroy), this);

    uint32_t sw = 0, sh = 0;
    LargestScreenPixels(window_, sw, sh);

    deskhubp::ScreenViewerConfig cfg;
    cfg.server = server;
    cfg.sourceId = sourceId;
    cfg.screenW = sw;
    cfg.screenH = sh;
    cfg.displayName = deskhubp::SessionDeviceName();
    cfg.wantsAudio = deskhubp::LoadUiSettings().playAudio;
    cfg.onStatus = [this](const char* status) {
        std::string line = status ? status : "";
        PostToMain([line = std::move(line)](ViewerWindow& v) {
            v.statusLine_ = line;
            v.UpdateTitle();
        });
    };
    cfg.onParams = [this](uint32_t, uint32_t, uint8_t) {
        PostToMain([](ViewerWindow& v) {
            v.SizeToVideo();
            v.UpdateTitle();
        });
    };
    cfg.onEnded = [this](const char*) {
        PostToMain([](ViewerWindow& v) { v.EndSession(); });
    };
    cfg.onFinished = [this](const char*) {
        PostToMain([](ViewerWindow& v) { v.EndSession(); });
    };

    loop_.SetSurface(&renderer_);
    if (!loop_.Start(cfg)) {
        gtk_widget_destroy(window_);
        return false;
    }

    gtk_widget_add_tick_callback(glArea_, OnTick, this, nullptr);

    clipboardSync_ = deskhubp::LoadUiSettings().clipboardSync;
    if (clipboardSync_) clipTimerId_ = g_timeout_add(1000, OnClipboardTimer, this);
    linkTimerId_ = g_timeout_add(1000, OnLinkTimer, this);

    UpdateTitle();
    gtk_widget_show_all(window_);
    return true;
}

gboolean ViewerWindow::OnClipboardTimer(gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (self->ended_) return G_SOURCE_CONTINUE;

    GtkClipboard* board = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if (const auto remote = self->loop_.TakeRemoteClipboard()) {
        gtk_clipboard_set_text(board, remote->c_str(), int(remote->size()));
        return G_SOURCE_CONTINUE;
    }
    if (gchar* text = gtk_clipboard_wait_for_text(board)) {
        self->loop_.OfferLocalClipboard(text);
        g_free(text);
    }
    return G_SOURCE_CONTINUE;
}

void ViewerWindow::VideoRect(int& x, int& y, int& w, int& h) const {
    GtkAllocation alloc{};
    gtk_widget_get_allocation(glArea_, &alloc);
    const int vw = int(loop_.videoWidth()), vh = int(loop_.videoHeight());
    x = alloc.x;
    y = alloc.y;
    w = alloc.width;
    h = alloc.height;
    if (vw <= 0 || vh <= 0 || w <= 0 || h <= 0) return;

    const deskhub::ViewRect r = deskhub::FitVideoRect(w, h, double(vw) / double(vh));
    x += int(r.x);
    y += int(r.y);
    w = int(r.width);
    h = int(r.height);
}

bool ViewerWindow::InContent(double px, double py) const {
    GtkAllocation a{};
    gtk_widget_get_allocation(glArea_, &a);
    return px >= a.x && py >= a.y && px < a.x + a.width && py < a.y + a.height;
}

bool ViewerWindow::ToNormalized(double px, double py, int32_t& nx, int32_t& ny) const {
    int rx = 0, ry = 0, rw = 0, rh = 0;
    VideoRect(rx, ry, rw, rh);
    const deskhub::ViewRect rect{double(rx), double(ry), double(rw), double(rh)};
    return deskhub::NormalizePointer(px, py, rect, nx, ny);
}

void ViewerWindow::OnRealize(GtkGLArea* area, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        LOGE("[Viewer] GtkGLArea failed to create an OpenGL context.");
        return;
    }
    self->renderer_.Realize();
}

void ViewerWindow::OnUnrealize(GtkGLArea* area, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    gtk_gl_area_make_current(area);
    self->renderer_.Unrealize();
}

gboolean ViewerWindow::OnRender(GtkGLArea* area, GdkGLContext*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    GtkAllocation alloc{};
    gtk_widget_get_allocation(GTK_WIDGET(area), &alloc);
    const int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    if (!self->renderer_.Render(alloc.width * scale, alloc.height * scale))
        self->renderer_.ClearBlack();
    return TRUE;
}

gboolean ViewerWindow::OnTick(GtkWidget* w, GdkFrameClock*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    const uint64_t serial = self->renderer_.FrameSerial();
    if (serial == self->queuedFrameSerial_) return G_SOURCE_CONTINUE;
    self->queuedFrameSerial_ = serial;
    gtk_gl_area_queue_render(GTK_GL_AREA(w));
    return G_SOURCE_CONTINUE;
}

void ViewerWindow::UpdateTitle() {
    std::string title = control_ ? deskhub::ComposeViewerTitle(baseTitle_, statusLine_)
                                 : deskhub::ComposeViewerTitle(baseTitle_, statusLine_,
                                       deskhub::kViewerViewOnlyHint);
    if (title == shownTitle_) return;
    shownTitle_ = std::move(title);
    gtk_window_set_title(GTK_WINDOW(window_), shownTitle_.c_str());
}

void ViewerWindow::UpdateLinkLabel() {
    if (!linkLabel_) return;
    const bool reattaching = loop_.phase() == deskhubp::ClientPhase::Reattaching;
    gtk_label_set_text(
        GTK_LABEL(linkLabel_), reattaching ? deskhub::ui::kTerminalReattaching : "");
}

gboolean ViewerWindow::OnLinkTimer(gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    self->UpdateLinkLabel();
    return G_SOURCE_CONTINUE;
}

void ViewerWindow::SizeToVideo() {
    const int vw = int(loop_.videoWidth()), vh = int(loop_.videoHeight());
    if (fullScreen_ || vw <= 0 || vh <= 0) return;
    if (!deskhub::ShouldRefitViewer(fittedW_, fittedH_, uint32_t(vw), uint32_t(vh))) return;
    fittedW_ = uint32_t(vw);
    fittedH_ = uint32_t(vh);

    const GdkRectangle wa = WorkArea(window_);
    const deskhub::ViewSize fitted = deskhub::ScaleToFit(uint32_t(vw), uint32_t(vh),
        uint32_t(std::max(320, wa.width - deskhub::kViewerMarginPx)),
        uint32_t(std::max(240, wa.height - deskhub::kViewerMarginPx)));
    gtk_window_resize(GTK_WINDOW(window_), int(fitted.width), int(fitted.height));
}

void ViewerWindow::ToggleFullScreen() {
    if (fullScreen_)
        gtk_window_unfullscreen(GTK_WINDOW(window_));
    else
        gtk_window_fullscreen(GTK_WINDOW(window_));
}

void ViewerWindow::EndSession() {
    if (ended_) return;
    ended_ = true;

    const std::string why = loop_.EndReason();
    gtk_widget_destroy(window_);
    RunOnMain([why] {
        ShowInfo(nullptr, "Deskhub",
            std::string(deskhub::ui::kConnectionEndedTitle) + ": " +
                (why.empty() ? std::string(deskhub::ui::kDisconnected) : why));
    });
}

GdkSeatCapabilities ViewerWindow::WantedSeatGrab() const {
    GdkSeatCapabilities caps = GdkSeatCapabilities(0);
    if (keyboardFocus_ && control_ && !WaylandShortcutsInhibitor::Applies(window_))
        caps = GdkSeatCapabilities(caps | GDK_SEAT_CAPABILITY_KEYBOARD);
    return caps;
}

void ViewerWindow::SyncSeatGrab() {
    GdkWindow* gw = gtk_widget_get_window(window_);
    if (!gw) return;

    if (keyboardFocus_ && control_ && WaylandShortcutsInhibitor::Applies(window_))
        shortcuts_.Inhibit(window_);

    const GdkSeatCapabilities wanted = WantedSeatGrab();
    if (wanted == grabbed_) return;
    GdkDisplay* display = gtk_widget_get_display(window_);
    GdkSeat* seat = gdk_display_get_default_seat(display);
    gdk_seat_ungrab(seat);
    if (wanted) gdk_seat_grab(seat, gw, wanted, FALSE, nullptr, nullptr, nullptr, nullptr);
    grabbed_ = wanted;
}

void ViewerWindow::ShowLocalCursor(bool show) {
    if (show == localCursorShown_) return;
    GdkWindow* gw = gtk_widget_get_window(window_);
    if (!gw) return;
    GdkCursor* blank =
        show ? nullptr : gdk_cursor_new_for_display(gtk_widget_get_display(window_), GDK_BLANK_CURSOR);
    gdk_window_set_cursor(gw, blank);
    if (blank) g_object_unref(blank);
    localCursorShown_ = show;
}

void ViewerWindow::ToggleLocalCursor() {
    if (!control_) return;
    cursor_.Toggle();
    if (cursorButton_) gtk_button_set_label(GTK_BUTTON(cursorButton_), cursor_.ButtonLabel());
    ShowLocalCursor(!cursor_.HidesOverVideo(pointerOverVideo_));
}

gboolean ViewerWindow::OnKey(GtkWidget*, GdkEventKey* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    const bool down = e->type == GDK_KEY_PRESS;
    int32_t vk = 0, scan = 0;
    const bool mapped =
        dh_native_key_to_vk(GdkKeycodeToEvdev(e->hardware_keycode), &vk, &scan);
    if (mapped && deskhub::IsViewerShortcutVk(vk, self->fullScreen_)) {
        if (down && deskhub::TogglesViewerFullScreen(vk, self->fullScreen_))
            self->ToggleFullScreen();
        if (down && vk == deskhub::kViewerCursorToggleVk) self->ToggleLocalCursor();
        return TRUE;
    }
    if (!self->control_) return FALSE;
    if (mapped) self->loop_.QueueKey(vk, scan, down);
    return TRUE;
}

gboolean ViewerWindow::OnMotion(GtkWidget*, GdkEventMotion* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    int32_t nx = 0, ny = 0;
    const bool overVideo = self->ToNormalized(e->x, e->y, nx, ny);
    self->pointerOverVideo_ = self->pointerInArea_ && overVideo;
    self->ShowLocalCursor(!self->cursor_.HidesOverVideo(self->pointerOverVideo_));
    if (!self->InContent(e->x, e->y)) return FALSE;
    if (overVideo) self->loop_.QueueMouseMoveAbs(nx, ny);
    return TRUE;
}

gboolean ViewerWindow::OnCrossing(GtkWidget*, GdkEventCrossing* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (e->detail == GDK_NOTIFY_INFERIOR) return FALSE;
    self->pointerInArea_ = e->type == GDK_ENTER_NOTIFY;
    if (!self->pointerInArea_) {
        self->pointerOverVideo_ = false;
        self->ShowLocalCursor(true);
    }
    return FALSE;
}

gboolean ViewerWindow::OnButton(GtkWidget*, GdkEventButton* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    if (!self->InContent(e->x, e->y)) return FALSE;
    if (e->type != GDK_BUTTON_PRESS && e->type != GDK_BUTTON_RELEASE) return TRUE;

    deskhub::MouseButton btn{};
    if (!deskhub::X11ButtonToMouseButton(e->button, btn)) return TRUE;

    int32_t nx = 0, ny = 0;
    if (self->ToNormalized(e->x, e->y, nx, ny)) self->loop_.QueueMouseMoveAbs(nx, ny);
    self->loop_.QueueMouseButton(int32_t(btn), e->type == GDK_BUTTON_PRESS);
    return TRUE;
}

gboolean ViewerWindow::OnScroll(GtkWidget*, GdkEventScroll* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    if (!self->InContent(e->x, e->y)) return FALSE;

    int32_t delta = 0;
    switch (e->direction) {
        case GDK_SCROLL_UP: delta = kWheelDelta; break;
        case GDK_SCROLL_DOWN: delta = -kWheelDelta; break;
        case GDK_SCROLL_SMOOTH:
            delta = int32_t(-e->delta_y * kWheelDelta);
            break;
        default: return TRUE;
    }
    self->loop_.QueueMouseWheel(delta);
    return TRUE;
}

gboolean ViewerWindow::OnFocusIn(GtkWidget*, GdkEventFocus*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    self->keyboardFocus_ = true;
    self->SyncSeatGrab();
    return FALSE;
}

gboolean ViewerWindow::OnFocusOut(GtkWidget*, GdkEventFocus*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    self->keyboardFocus_ = false;
    self->loop_.ReleaseAllInput();
    self->SyncSeatGrab();
    return FALSE;
}

void ViewerWindow::OnFullScreenClicked(GtkWidget*, gpointer user) {
    static_cast<ViewerWindow*>(user)->ToggleFullScreen();
}

void ViewerWindow::OnCursorClicked(GtkWidget*, gpointer user) {
    static_cast<ViewerWindow*>(user)->ToggleLocalCursor();
}

gboolean ViewerWindow::OnWindowState(GtkWidget*, GdkEventWindowState* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    self->fullScreen_ = (e->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    return FALSE;
}

void ViewerWindow::OnDestroy(GtkWidget*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (self->clipTimerId_) {
        g_source_remove(self->clipTimerId_);
        self->clipTimerId_ = 0;
    }
    if (self->linkTimerId_) {
        g_source_remove(self->linkTimerId_);
        self->linkTimerId_ = 0;
    }
    self->window_ = nullptr;
    auto done = std::move(self->onClosed_);
    delete self;
    if (done) done();
}
