#include "gtk/ViewerWindow.h"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ClientFfi.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "gtk/GtkUtil.h"

#include "deskhub/input/PointerLockState.h"
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
    const std::string& sourceName, const std::string& passcode, bool control,
    std::function<void()> onClosed) {
    auto* v = new ViewerWindow();
    v->onClosed_ = std::move(onClosed);
    if (!v->Build(server, sourceId, sourceName, passcode, control)) {
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
    const std::string& passcode, bool control) {
    control_ = control;
    baseTitle_ = deskhub::ViewerBaseTitle(sourceName);
    alive_ = std::make_shared<ViewerWindow*>(this);

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), baseTitle_.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window_), kInitialW, kInitialH);

    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    GtkWidget* disconnectButton = gtk_button_new_with_label(deskhub::ui::kDisconnectButton);
    g_signal_connect(disconnectButton, "clicked", G_CALLBACK(OnDisconnectClicked), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), disconnectButton);
    linkLabel_ = gtk_label_new("");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), linkLabel_);
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    glArea_ = gtk_gl_area_new();
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(glArea_), FALSE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(glArea_), FALSE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(glArea_), FALSE);
    gtk_container_add(GTK_CONTAINER(window_), glArea_);

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
    g_signal_connect(window_, "focus-out-event", G_CALLBACK(OnFocusOut), this);
    g_signal_connect(window_, "destroy", G_CALLBACK(OnDestroy), this);

    uint32_t sw = 0, sh = 0;
    LargestScreenPixels(window_, sw, sh);

    deskhubp::ScreenViewerConfig cfg;
    cfg.server = server;
    cfg.sourceId = sourceId;
    cfg.screenW = sw;
    cfg.screenH = sh;
    cfg.passcode = passcode;
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
    cfg.onTrustAsked = [this](deskhub::TrustVerdict verdict, std::string_view fingerprint) {
        PostToMain([verdict, copy = std::string(fingerprint)](ViewerWindow& v) {
            v.AskAboutKey(verdict, copy);
        });
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

void ViewerWindow::AskAboutKey(deskhub::TrustVerdict verdict, const std::string& fingerprint) {
    const bool changed = verdict == deskhub::TrustVerdict::Changed;
    std::string body(changed ? deskhub::ui::kTrustChangedBody : deskhub::ui::kTrustNewHostBody);
    body += "\n\n";
    body += deskhub::ui::kTrustFingerprintLabel;
    body += " ";
    body += fingerprint;

    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
        changed ? GTK_MESSAGE_WARNING : GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
        changed ? deskhub::ui::kTrustChangedTitle : deskhub::ui::kTrustNewHostTitle);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dlg), deskhub::ui::kTrustReject, GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dlg), deskhub::ui::kTrustAccept, GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
    const bool accepted = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
    gtk_widget_destroy(dlg);
    if (accepted)
        loop_.AcceptFingerprint();
    else
        loop_.RejectFingerprint();
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
    std::string title =
        control_ ? pointer_.TitleFor(baseTitle_, statusLine_)
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
    if (vw <= 0 || vh <= 0) return;
    if (!deskhub::ShouldRefitViewer(fittedW_, fittedH_, uint32_t(vw), uint32_t(vh))) return;
    fittedW_ = uint32_t(vw);
    fittedH_ = uint32_t(vh);

    const GdkRectangle wa = WorkArea(window_);
    const deskhub::ViewSize fitted = deskhub::ScaleToFit(uint32_t(vw), uint32_t(vh),
        uint32_t(std::max(320, wa.width - deskhub::kViewerMarginPx)),
        uint32_t(std::max(240, wa.height - deskhub::kViewerMarginPx)));
    gtk_window_resize(GTK_WINDOW(window_), int(fitted.width), int(fitted.height));
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

void ViewerWindow::GrabPointer(bool locked) {
    GdkWindow* gw = gtk_widget_get_window(window_);
    if (!gw) return;

    GdkDisplay* display = gtk_widget_get_display(window_);
    GdkSeat* seat = gdk_display_get_default_seat(display);
    if (locked) {
        GdkCursor* blank = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
        gdk_seat_grab(seat, gw, GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, blank, nullptr, nullptr,
            nullptr);
        if (blank) g_object_unref(blank);
    } else {
        gdk_seat_ungrab(seat);
    }
    haveLastPos_ = false;
}

void ViewerWindow::ApplyLockEffect(const deskhub::PointerLockEffect& effect) {
    if (effect.releaseHeldInput) loop_.ReleaseAllInput();
    if (effect.lockChanged) {
        GrabPointer(pointer_.locked());
        UpdateTitle();
    }
}

gboolean ViewerWindow::OnKey(GtkWidget*, GdkEventKey* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    const bool down = e->type == GDK_KEY_PRESS;

    if (down && e->keyval == GDK_KEY_Escape && self->pointer_.locked()) {
        self->ApplyLockEffect(self->pointer_.OnEscape());
        return TRUE;
    }

    int32_t vk = 0, scan = 0;
    if (!dh_native_key_to_vk(GdkKeycodeToEvdev(e->hardware_keycode), &vk, &scan)) return TRUE;
    if (vk == deskhub::kViewerLockToggleVk) {
        if (down) self->ApplyLockEffect(self->pointer_.OnToggleLockKey());
        return TRUE;
    }
    self->loop_.QueueKey(vk, scan, down);
    return TRUE;
}

gboolean ViewerWindow::OnMotion(GtkWidget*, GdkEventMotion* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    if (!self->pointer_.locked() && !self->InContent(e->x, e->y)) return FALSE;

    if (!self->pointer_.locked()) {
        int32_t nx = 0, ny = 0;
        if (self->ToNormalized(e->x, e->y, nx, ny)) self->loop_.QueueMouseMoveAbs(nx, ny);
        self->haveLastPos_ = false;
        return TRUE;
    }

    GtkAllocation alloc{};
    gtk_widget_get_allocation(self->window_, &alloc);
    const double cx = alloc.width / 2.0, cy = alloc.height / 2.0;

    if (self->haveLastPos_) {
        const int32_t dx = int32_t(e->x - self->lastPx_);
        const int32_t dy = int32_t(e->y - self->lastPy_);
        if (dx || dy) self->loop_.QueueMouseMoveRel(dx, dy);
    }
    self->lastPx_ = cx;
    self->lastPy_ = cy;
    self->haveLastPos_ = true;

    GdkWindow* gw = gtk_widget_get_window(self->window_);
    if (gw) {
        int ox = 0, oy = 0;
        gdk_window_get_origin(gw, &ox, &oy);
        gdk_device_warp(gdk_event_get_device(reinterpret_cast<GdkEvent*>(e)),
            gtk_widget_get_screen(self->window_), ox + int(cx), oy + int(cy));
    }
    return TRUE;
}

gboolean ViewerWindow::OnButton(GtkWidget*, GdkEventButton* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    if (!self->pointer_.locked() && !self->InContent(e->x, e->y)) return FALSE;
    if (e->type != GDK_BUTTON_PRESS && e->type != GDK_BUTTON_RELEASE) return TRUE;

    deskhub::MouseButton btn{};
    if (!deskhub::X11ButtonToMouseButton(e->button, btn)) return TRUE;

    if (!self->pointer_.locked()) {
        int32_t nx = 0, ny = 0;
        if (self->ToNormalized(e->x, e->y, nx, ny)) self->loop_.QueueMouseMoveAbs(nx, ny);
    }
    self->loop_.QueueMouseButton(int32_t(btn), e->type == GDK_BUTTON_PRESS);
    return TRUE;
}

gboolean ViewerWindow::OnScroll(GtkWidget*, GdkEventScroll* e, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    if (!self->control_) return FALSE;
    if (!self->pointer_.locked() && !self->InContent(e->x, e->y)) return FALSE;

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

gboolean ViewerWindow::OnFocusOut(GtkWidget*, GdkEventFocus*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    self->ApplyLockEffect(self->pointer_.OnFocusLost());
    return FALSE;
}

void ViewerWindow::OnDisconnectClicked(GtkWidget*, gpointer user) {
    auto* self = static_cast<ViewerWindow*>(user);
    gtk_window_close(GTK_WINDOW(self->window_));
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
