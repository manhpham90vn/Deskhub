#include "gtk/TerminalWindow.h"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "gtk/GtkUtil.h"

#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Palette.h"
#include "deskhub/terminal/ScrollAnchor.h"
#include "deskhub/terminal/VtParser.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/TerminalFeed.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/client/TerminalViewer.h"

namespace {

namespace ui = deskhub::ui;
namespace term = deskhub::term;

constexpr guint kRedrawMs = 33;
constexpr int kGridPadding = 6;
constexpr int kInitialGridW = 900;
constexpr int kInitialGridH = 480;
constexpr int kScrollLines = 3;

void SetCairoColor(cairo_t* cr, const term::Rgb& rgb) {
    cairo_set_source_rgb(cr, rgb.r / 255.0, rgb.g / 255.0, rgb.b / 255.0);
}

bool NamedKeyOf(guint keyval, term::TermKey& out) {
    switch (keyval) {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: out = term::TermKey::Enter; return true;
        case GDK_KEY_BackSpace: out = term::TermKey::Backspace; return true;
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab: out = term::TermKey::Tab; return true;
        case GDK_KEY_Escape: out = term::TermKey::Escape; return true;
        case GDK_KEY_Up: out = term::TermKey::Up; return true;
        case GDK_KEY_Down: out = term::TermKey::Down; return true;
        case GDK_KEY_Left: out = term::TermKey::Left; return true;
        case GDK_KEY_Right: out = term::TermKey::Right; return true;
        case GDK_KEY_Home: out = term::TermKey::Home; return true;
        case GDK_KEY_End: out = term::TermKey::End; return true;
        case GDK_KEY_Page_Up: out = term::TermKey::PageUp; return true;
        case GDK_KEY_Page_Down: out = term::TermKey::PageDown; return true;
        case GDK_KEY_Insert: out = term::TermKey::Insert; return true;
        case GDK_KEY_Delete: out = term::TermKey::Delete; return true;
        case GDK_KEY_F1: out = term::TermKey::F1; return true;
        case GDK_KEY_F2: out = term::TermKey::F2; return true;
        case GDK_KEY_F3: out = term::TermKey::F3; return true;
        case GDK_KEY_F4: out = term::TermKey::F4; return true;
        case GDK_KEY_F5: out = term::TermKey::F5; return true;
        case GDK_KEY_F6: out = term::TermKey::F6; return true;
        case GDK_KEY_F7: out = term::TermKey::F7; return true;
        case GDK_KEY_F8: out = term::TermKey::F8; return true;
        case GDK_KEY_F9: out = term::TermKey::F9; return true;
        case GDK_KEY_F10: out = term::TermKey::F10; return true;
        case GDK_KEY_F11: out = term::TermKey::F11; return true;
        case GDK_KEY_F12: out = term::TermKey::F12; return true;
        default: return false;
    }
}

term::TermMods ModsOf(guint state) {
    term::TermMods mods;
    mods.shift = (state & GDK_SHIFT_MASK) != 0;
    mods.alt = (state & GDK_MOD1_MASK) != 0;
    mods.ctrl = (state & GDK_CONTROL_MASK) != 0;
    return mods;
}

class TerminalWindow {
public:
    static bool Open(GtkWindow* parent, const TerminalLaunch& launch) {
        auto* w = new TerminalWindow();
        if (!w->BuildRemote(parent, launch)) {
            delete w;
            return false;
        }
        return true;
    }

    static bool OpenLocal(GtkWindow* parent, deskhubp::TerminalHost& host, uint32_t termId) {
        auto* w = new TerminalWindow();
        if (!w->BuildLocal(parent, host, termId)) {
            delete w;
            return false;
        }
        return true;
    }

private:
    TerminalWindow() = default;
    ~TerminalWindow() = default;
    TerminalWindow(const TerminalWindow&) = delete;
    TerminalWindow& operator=(const TerminalWindow&) = delete;

    void BuildShell(GtkWindow* parent, const std::string& title) {
        alive_ = std::make_shared<TerminalWindow*>(this);

        window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window_), title.c_str());
        if (parent) gtk_window_set_transient_for(GTK_WINDOW(window_), parent);
        gtk_window_set_default_size(GTK_WINDOW(window_), kInitialGridW, kInitialGridH);

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(window_), box);

        grid_ = gtk_drawing_area_new();
        gtk_widget_set_can_focus(grid_, TRUE);
        gtk_widget_add_events(grid_, GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK |
                                         GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |
                                         GDK_FOCUS_CHANGE_MASK);
        g_signal_connect(grid_, "draw", G_CALLBACK(OnDraw), this);
        g_signal_connect(grid_, "key-press-event", G_CALLBACK(OnKeyPress), this);
        g_signal_connect(grid_, "scroll-event", G_CALLBACK(OnScroll), this);
        g_signal_connect(grid_, "size-allocate", G_CALLBACK(OnSizeAllocate), this);
        g_signal_connect(grid_, "button-press-event", G_CALLBACK(OnButtonPress), this);
        g_signal_connect(grid_, "focus-in-event", G_CALLBACK(OnFocusChange), this);
        g_signal_connect(grid_, "focus-out-event", G_CALLBACK(OnFocusChange), this);
        gtk_box_pack_start(GTK_BOX(box), grid_, TRUE, TRUE, 0);

        GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(footer, 8);
        gtk_widget_set_margin_end(footer, 8);
        gtk_widget_set_margin_top(footer, 4);
        gtk_widget_set_margin_bottom(footer, 4);

        statusLabel_ = gtk_label_new(ui::kTerminalConnecting);
        gtk_label_set_xalign(GTK_LABEL(statusLabel_), 0.f);
        gtk_box_pack_start(GTK_BOX(footer), statusLabel_, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(box), footer, FALSE, FALSE, 0);

        g_signal_connect(window_, "destroy", G_CALLBACK(OnDestroy), this);

        MeasureCell();
    }

    void ShowBuilt() {
        redrawTimerId_ = g_timeout_add(kRedrawMs, OnRedrawTimer, this);
        gtk_widget_show_all(window_);
        gtk_widget_grab_focus(grid_);
    }

    bool BuildRemote(GtkWindow* parent, const TerminalLaunch& launch) {
        NetAddr host{};
        if (!ParseNetAddr(launch.address, host)) return false;

        BuildShell(parent, ui::AddressHost(launch.address));

        auto feed = std::make_unique<deskhubp::RemoteTerminalFeed>();
        remote_ = feed.get();

        deskhubp::TerminalViewerConfig config;
        config.host = host;
        config.hostLabel = ui::AddressHost(launch.address);
        config.passcode = launch.passcode;
        config.clientName = launch.clientName;
        config.size = CellsFor(kInitialGridW, kInitialGridH);

        deskhubp::TerminalViewerCallbacks hooks;
        hooks.onState = [token = alive_](deskhubp::TerminalViewerState state,
                            std::string_view message) {
            RunOnMain([token, state, copy = std::string(message)] {
                if (TerminalWindow* self = *token) self->OnViewerState(state, copy);
            });
        };
        hooks.onTrustAsked = [token = alive_](deskhub::TrustVerdict verdict,
                                 std::string_view fingerprint) {
            RunOnMain([token, verdict, copy = std::string(fingerprint)] {
                if (TerminalWindow* self = *token) self->AskAboutKey(verdict, copy);
            });
        };

        if (!remote_->viewer.Start(config, std::move(hooks))) {
            remote_ = nullptr;
            *alive_ = nullptr;
            g_signal_handlers_disconnect_by_func(window_,
                reinterpret_cast<gpointer>(G_CALLBACK(OnDestroy)), this);
            gtk_widget_destroy(window_);
            if (font_) {
                pango_font_description_free(font_);
                font_ = nullptr;
            }
            return false;
        }
        feed_ = std::move(feed);

        ShowBuilt();
        return true;
    }

    bool BuildLocal(GtkWindow* parent, deskhubp::TerminalHost& host, uint32_t termId) {
        if (!host.LocalAlive(termId)) return false;

        BuildShell(parent, ui::kTerminalLocalWindowTitle);
        gtk_label_set_text(GTK_LABEL(statusLabel_), ui::kTerminalAttachedHere);
        feed_ = std::make_unique<deskhubp::LocalTerminalFeed>(host, termId);

        ShowBuilt();
        return true;
    }

    void MeasureCell() {
        font_ = pango_font_description_from_string("Monospace 11");
        PangoLayout* layout = gtk_widget_create_pango_layout(grid_, "M");
        pango_layout_set_font_description(layout, font_);
        int w = 0, h = 0;
        pango_layout_get_pixel_size(layout, &w, &h);
        g_object_unref(layout);
        cellWidth_ = std::max(1, w);
        cellHeight_ = std::max(1, h);
    }

    deskhub::TermSize CellsFor(int width, int height) const {
        const int cols = (width - 2 * kGridPadding) / cellWidth_;
        const int rows = (height - 2 * kGridPadding) / cellHeight_;
        return deskhub::ClampTermSize(
            deskhub::TermSize{uint16_t(std::max(1, cols)), uint16_t(std::max(1, rows))});
    }

    deskhub::TermSize CellsThatFit() const {
        GtkAllocation alloc{};
        gtk_widget_get_allocation(grid_, &alloc);
        return CellsFor(alloc.width, alloc.height);
    }

    void PullSnapshot() {
        if (feed_->Alive()) {
            const size_t was = snapshot_.scrollbackRows;
            snapshot_ = feed_->Snapshot(scrollOffset_);
            const size_t anchored =
                term::AnchorScroll(scrollOffset_, was, snapshot_.scrollbackRows);
            if (anchored != scrollOffset_) {
                scrollOffset_ = anchored;
                snapshot_ = feed_->Snapshot(scrollOffset_);
            }
            scrollOffset_ = snapshot_.scrollOffset;
        } else {
            snapshot_ = deskhubp::TerminalSnapshot{};
            snapshot_.size = deskhub::TermSize{0, 0};
            scrollOffset_ = 0;
            if (remote_ == nullptr) ShowLocalShellEnded();
        }
        gtk_widget_queue_draw(grid_);
    }

    void ShowLocalShellEnded() {
        if (localEndShown_) return;
        localEndShown_ = true;
        gtk_label_set_text(GTK_LABEL(statusLabel_), ui::kTerminalClosed);
        StopRedrawTimer();
    }

    void ScrollBy(int rows) {
        const size_t before = scrollOffset_;
        scrollOffset_ = term::ScrollByRows(scrollOffset_, rows, snapshot_.scrollbackRows);
        if (scrollOffset_ != before) PullSnapshot();
    }

    void ScrollToBottom() {
        if (scrollOffset_ == 0) return;
        scrollOffset_ = 0;
        PullSnapshot();
    }

    void Draw(cairo_t* cr) {
        SetCairoColor(cr, term::kDefaultBackground);
        cairo_paint(cr);

        PangoLayout* layout = gtk_widget_create_pango_layout(grid_, nullptr);
        PangoFontDescription* desc = pango_font_description_copy(font_);

        for (uint16_t row = 0; row < snapshot_.size.rows; ++row) {
            for (uint16_t col = 0; col < snapshot_.size.cols; ++col) {
                const term::Cell& cell = snapshot_.At(row, col);
                const term::CellColors colors = term::ResolveCell(cell.pen, false);
                const double x = kGridPadding + double(col) * cellWidth_;
                const double y = kGridPadding + double(row) * cellHeight_;

                SetCairoColor(cr, colors.bg);
                cairo_rectangle(cr, x, y, cellWidth_, cellHeight_);
                cairo_fill(cr);

                if (cell.ch == U' ') continue;
                pango_font_description_set_weight(desc,
                    (cell.pen.attrs & term::kAttrBold) != 0 ? PANGO_WEIGHT_BOLD
                                                            : PANGO_WEIGHT_NORMAL);
                pango_font_description_set_style(desc,
                    (cell.pen.attrs & term::kAttrItalic) != 0 ? PANGO_STYLE_ITALIC
                                                              : PANGO_STYLE_NORMAL);
                pango_layout_set_font_description(layout, desc);
                const std::string glyph = term::EncodeUtf8(cell.ch);
                pango_layout_set_text(layout, glyph.c_str(), int(glyph.size()));
                SetCairoColor(cr, colors.fg);
                cairo_move_to(cr, x, y);
                pango_cairo_show_layout(cr, layout);

                if ((cell.pen.attrs & term::kAttrUnderline) != 0) {
                    cairo_rectangle(cr, x, y + cellHeight_ - 1, cellWidth_, 1);
                    cairo_fill(cr);
                }
            }
        }

        pango_font_description_free(desc);
        g_object_unref(layout);

        if (snapshot_.cursor.visible && snapshot_.size.rows > 0) {
            const double x = kGridPadding + double(snapshot_.cursor.col) * cellWidth_;
            const double y = kGridPadding + double(snapshot_.cursor.row) * cellHeight_;
            SetCairoColor(cr, term::kCursorColor);
            if (gtk_widget_has_focus(grid_)) {
                cairo_rectangle(cr, x, y, cellWidth_, cellHeight_);
                cairo_fill(cr);
            } else {
                cairo_set_line_width(cr, 1.0);
                cairo_rectangle(cr, x + 0.5, y + 0.5, cellWidth_ - 1, cellHeight_ - 1);
                cairo_stroke(cr);
            }
        }
    }

    gboolean HandleKey(GdkEventKey* event) {
        term::TermKey named{};
        const bool shift = (event->state & GDK_SHIFT_MASK) != 0;
        if (shift && (event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_Page_Down)) {
            const int page = std::max(1, snapshot_.size.rows - 1);
            ScrollBy(event->keyval == GDK_KEY_Page_Up ? page : -page);
            return TRUE;
        }
        ScrollToBottom();
        if (NamedKeyOf(event->keyval, named)) {
            term::TermKeyEvent key;
            key.key = named;
            key.mods = ModsOf(event->state);
            feed_->SendKey(key);
            return TRUE;
        }
        const guint32 unicode = gdk_keyval_to_unicode(event->keyval);
        if (unicode < 32 || unicode == 127) return FALSE;
        term::TermKeyEvent key;
        key.key = term::TermKey::Char;
        key.codepoint = char32_t(unicode);
        if ((event->state & GDK_CONTROL_MASK) != 0) {
            key.mods = ModsOf(event->state);
        } else {
            key.mods.alt = (event->state & GDK_MOD1_MASK) != 0;
        }
        feed_->SendKey(key);
        return TRUE;
    }

    void ResizeToGrid() {
        if (!feed_->Alive()) return;
        feed_->Resize(CellsThatFit());
    }

    void OnViewerState(deskhubp::TerminalViewerState state, const std::string& message) {
        gtk_label_set_text(GTK_LABEL(statusLabel_), message.c_str());
        if (state == deskhubp::TerminalViewerState::Live) gtk_widget_grab_focus(grid_);
        if (state == deskhubp::TerminalViewerState::Ended ||
            state == deskhubp::TerminalViewerState::Failed ||
            state == deskhubp::TerminalViewerState::Refused) {
            StopRedrawTimer();
            PullSnapshot();
        }
    }

    void AskAboutKey(deskhub::TrustVerdict verdict, const std::string& fingerprint) {
        const bool changed = verdict == deskhub::TrustVerdict::Changed;
        std::string body(changed ? ui::kTrustChangedBody : ui::kTrustNewHostBody);
        body += "\n\n";
        body += ui::kTrustFingerprintLabel;
        body += " ";
        body += fingerprint;

        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
            changed ? GTK_MESSAGE_WARNING : GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
            changed ? ui::kTrustChangedTitle : ui::kTrustNewHostTitle);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body.c_str());
        gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kTrustReject, GTK_RESPONSE_NO);
        gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kTrustAccept, GTK_RESPONSE_YES);
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
        const bool accepted = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
        gtk_widget_destroy(dlg);
        if (remote_ == nullptr) return;
        if (accepted)
            remote_->viewer.AcceptFingerprint();
        else
            remote_->viewer.RejectFingerprint();
    }

    void StopRedrawTimer() {
        if (!redrawTimerId_) return;
        g_source_remove(redrawTimerId_);
        redrawTimerId_ = 0;
    }

    static gboolean OnDraw(GtkWidget*, cairo_t* cr, gpointer user) {
        static_cast<TerminalWindow*>(user)->Draw(cr);
        return TRUE;
    }

    static gboolean OnKeyPress(GtkWidget*, GdkEventKey* event, gpointer user) {
        return static_cast<TerminalWindow*>(user)->HandleKey(event);
    }

    static gboolean OnScroll(GtkWidget*, GdkEventScroll* event, gpointer user) {
        auto* self = static_cast<TerminalWindow*>(user);
        if (event->direction == GDK_SCROLL_UP) {
            self->ScrollBy(kScrollLines);
        } else if (event->direction == GDK_SCROLL_DOWN) {
            self->ScrollBy(-kScrollLines);
        } else if (event->direction == GDK_SCROLL_SMOOTH) {
            self->ScrollBy(event->delta_y < 0 ? kScrollLines : -kScrollLines);
        }
        return TRUE;
    }

    static void OnSizeAllocate(GtkWidget*, GdkRectangle*, gpointer user) {
        static_cast<TerminalWindow*>(user)->ResizeToGrid();
    }

    static gboolean OnButtonPress(GtkWidget* widget, GdkEventButton*, gpointer) {
        gtk_widget_grab_focus(widget);
        return TRUE;
    }

    static gboolean OnFocusChange(GtkWidget* widget, GdkEventFocus*, gpointer) {
        gtk_widget_queue_draw(widget);
        return FALSE;
    }

    static gboolean OnRedrawTimer(gpointer user) {
        static_cast<TerminalWindow*>(user)->PullSnapshot();
        return G_SOURCE_CONTINUE;
    }

    static void OnDestroy(GtkWidget*, gpointer user) {
        auto* self = static_cast<TerminalWindow*>(user);
        *self->alive_ = nullptr;
        self->StopRedrawTimer();
        if (self->feed_) self->feed_->Shutdown();
        if (self->font_) pango_font_description_free(self->font_);
        delete self;
    }

    GtkWidget* window_ = nullptr;
    GtkWidget* grid_ = nullptr;
    GtkWidget* statusLabel_ = nullptr;
    guint redrawTimerId_ = 0;
    PangoFontDescription* font_ = nullptr;
    int cellWidth_ = 8;
    int cellHeight_ = 16;

    std::unique_ptr<deskhubp::TerminalFeed> feed_{};
    deskhubp::RemoteTerminalFeed* remote_ = nullptr;
    bool localEndShown_ = false;
    deskhubp::TerminalSnapshot snapshot_{};
    size_t scrollOffset_ = 0;

    std::shared_ptr<TerminalWindow*> alive_;
};

}

bool OpenTerminalWindow(GtkWindow* parent, const TerminalLaunch& launch) {
    return TerminalWindow::Open(parent, launch);
}

bool OpenHostTerminalWindow(GtkWindow* parent, deskhubp::TerminalHost& host, uint32_t termId) {
    return TerminalWindow::OpenLocal(parent, host, termId);
}
