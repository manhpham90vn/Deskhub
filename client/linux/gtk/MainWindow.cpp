#include "gtk/MainWindow.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "deskhubp/media/DisplayEnum.h"
#include "gtk/FileSendDialog.h"
#include "gtk/GtkUtil.h"
#include "gtk/PasscodeDialog.h"
#include "gtk/TerminalWindow.h"
#include "gtk/ViewerWindow.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/FileStore.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/UiSettingsStore.h"

#include "deskhub/media/QualityPreset.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/client/ConnectFlow.h"
#include "deskhub/session/host/ShareFlow.h"
#include "deskhub/ui/Strings.h"

namespace {

namespace ui = deskhub::ui;

constexpr const char* kRecentDevicesFile = "recent-devices.txt";

constexpr int kWindowW = 1040;
constexpr int kWindowH = 700;
constexpr int kSidebarW = 180;
constexpr int kNavH = 42;
constexpr int kListH = 130;
constexpr int kPad = 16;
constexpr int kHintWrapChars = 64;
constexpr int kConnectionWindowWidth = 460;
constexpr int kPrimaryButtonH = 46;

constexpr guint kRescanDelayMs = deskhubp::kLanRescanSecs * 1000;

constexpr int kHostActionWidth = 104;
constexpr int kHostActionHeight = 26;
constexpr int kHostCellGap = 8;
constexpr int kHostRowGap = 6;

struct HostColumn {
    const char* title;
    int width;
    float align;
};

const HostColumn kHostColumns[] = {{"Source", 140, 0.f}, {"Size", 80, 0.f},
    {"Viewers", 58, 1.f}, {"Client", 120, 0.f}, {"Capture", 58, 1.f}, {"Send", 50, 1.f},
    {"Mbps", 55, 1.f}, {"RTT", 55, 1.f}};

constexpr const char* kOnlineColour = "#00913c";
constexpr const char* kOfflineColour = "#c82828";
constexpr const char* kUnknownColour = "#787878";

const char* const kPageLabels[] = {ui::kSidebarHost, ui::kSidebarClient, ui::kSidebarDevices,
    ui::kSidebarSettings};

const char* const kStyleSheet =
    ".deskhub-sidebar { background-color: #1f2937; }"
    ".deskhub-sidebar-title { color: #ffffff; font-weight: bold; font-size: 1.6em; }"
    ".deskhub-nav { background-image: none; background-color: transparent; border: none;"
    " box-shadow: none; color: #d1d5db; font-size: 1.1em; padding: 0 16px; border-radius: 8px; }"
    ".deskhub-nav:hover { background-color: #374151; }"
    ".deskhub-nav:active { background-color: #374151; }"
    ".deskhub-nav-selected { background-color: #2563eb; color: #ffffff; font-weight: bold; }"
    ".deskhub-nav-selected:hover { background-color: #2563eb; }"
    ".deskhub-nav-selected:active { background-color: #2563eb; }"
    ".deskhub-page { background-color: #ffffff; }"
    ".deskhub-page-body { padding: 16px; }"
    ".deskhub-heading { font-weight: bold; font-size: 1.35em; color: #111827; }"
    ".deskhub-section { font-weight: bold; font-size: 1.1em; color: #111827; }"
    ".deskhub-hint { color: #6b7280; }"
    ".deskhub-status-error { color: #c82828; }"
    ".deskhub-status-online { color: #00913c; font-weight: bold; }"
    ".deskhub-status-offline { color: #c82828; font-weight: bold; }"
    ".deskhub-picker { padding: 8px; }"
    ".deskhub-footnote { color: #94a3b8; }"
    ".deskhub-link { color: #d1d5db; background-color: transparent; border: none; }"
    ".deskhub-link:hover { background-color: transparent; }"
    ".deskhub-link:active { background-color: transparent; }"
    ".deskhub-banner { padding: 10px; border-left: 4px solid #6b7280;"
    " background-color: #f3f4f6; }"
    ".deskhub-banner-busy { border-left-color: #2563eb; background-color: #ebf3ff; }"
    ".deskhub-banner-live { border-left-color: #00913c; background-color: #e8faef; }"
    ".deskhub-banner-state { font-weight: bold; font-size: 1.1em; color: #6b7280; }"
    ".deskhub-banner-state-busy { color: #2563eb; }"
    ".deskhub-banner-state-live { color: #00913c; }"
    ".deskhub-primary { font-weight: bold; color: #ffffff; background-image: none;"
    " background-color: #2563eb; border: none; }"
    ".deskhub-primary:hover { background-color: #2563eb; }"
    ".deskhub-primary:active { background-color: #1d4ed8; }"
    ".deskhub-primary:disabled { background-color: #93c5fd; color: #ffffff; }"
    ".deskhub-primary-stop { background-color: #c82828; }"
    ".deskhub-primary-stop:hover { background-color: #c82828; }"
    ".deskhub-primary-stop:active { background-color: #a51f1f; }"
    ".deskhub-row-action { color: #ffffff; background-image: none; border: none;"
    " padding: 2px 10px; border-radius: 4px; }"
    ".deskhub-row-action-stop { background-color: #c82828; }"
    ".deskhub-row-action-stop:hover { background-color: #c82828; }"
    ".deskhub-row-action-stop:active { background-color: #a51f1f; }"
    ".deskhub-row-action-kick { background-color: #ca6c08; }"
    ".deskhub-row-action-kick:hover { background-color: #ca6c08; }"
    ".deskhub-row-action-kick:active { background-color: #a85a06; }"
    ".deskhub-row-header { color: #6b7280; font-weight: bold; font-size: 0.85em; }"
    ".deskhub-row-cell { color: #111827; }"
    ".deskhub-row-cell-online { color: #00913c; }"
    "window { background-color: #ffffff; color: #111827; }"
    "viewport { background-color: #ffffff; }"
    "entry { background-color: #ffffff; color: #111827; caret-color: #111827;"
    " background-image: none; border: 1px solid #d1d5db; border-radius: 6px;"
    " padding: 4px 8px; }"
    "entry:focus { border-color: #2563eb; }"
    "entry selection { background-color: #2563eb; color: #ffffff; }"
    "spinbutton { background-color: #ffffff; color: #111827; background-image: none;"
    " border: 1px solid #d1d5db; border-radius: 6px; }"
    "spinbutton entry { border: none; }"
    "spinbutton button { background-color: #f3f4f6; color: #111827; background-image: none;"
    " border: none; }"
    "button { background-color: #f9fafb; color: #111827; background-image: none;"
    " border: 1px solid #d1d5db; border-radius: 6px; box-shadow: none; text-shadow: none; }"
    "button:hover { background-color: #f3f4f6; }"
    "button:active { background-color: #e5e7eb; }"
    "button:disabled { color: #9ca3af; }"
    "button.combo { background-color: #ffffff; }"
    "button.titlebutton { background-color: transparent; border: none; }"
    "button.titlebutton:hover { background-color: transparent; }"
    "menu { background-color: #ffffff; }"
    "menuitem { color: #111827; }"
    "menuitem:hover { background-color: #2563eb; color: #ffffff; }"
    "check { background-color: #ffffff; background-image: none; border: 1px solid #9ca3af; }"
    "check:checked { background-color: #2563eb; border-color: #2563eb; color: #ffffff;"
    " background-image: none; }"
    "treeview.view { background-color: #ffffff; color: #111827; }"
    "treeview.view:selected { background-color: #2563eb; color: #ffffff; }"
    "treeview header button { background-color: #f9fafb; color: #6b7280; border: none;"
    " border-radius: 0; }"
    "scrolledwindow { border-color: #d1d5db; }"
    "scrollbar { background-color: #f3f4f6; border: none; margin: 0; }"
    "scrollbar contents, scrollbar trough { background-color: #f3f4f6; border: none;"
    " margin: 0; }"
    "scrollbar slider { background-color: #9ca3af; border: none; border-radius: 8px;"
    " min-width: 8px; min-height: 8px; }"
    "scrollbar slider:hover { background-color: #6b7280; }";

void AddClass(GtkWidget* widget, const char* name) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

void RemoveClass(GtkWidget* widget, const char* name) {
    gtk_style_context_remove_class(gtk_widget_get_style_context(widget), name);
}

void InstallStyles() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, kStyleSheet, -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

GtkWidget* Label(const std::string& text) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.f);
    return label;
}

GtkWidget* StyledLabel(const std::string& text, const char* cssClass) {
    GtkWidget* label = Label(text);
    AddClass(label, cssClass);
    return label;
}

GtkWidget* Heading(const char* text) {
    return StyledLabel(text, "deskhub-heading");
}

GtkWidget* Section(const char* text) {
    return StyledLabel(text, "deskhub-section");
}

GtkWidget* HeadingRow(const char* heading, GCallback onRefresh, gpointer user) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(row), Heading(heading), FALSE, FALSE, 0);

    GtkWidget* refresh = gtk_button_new_with_label(ui::kRefreshNow);
    gtk_widget_set_size_request(refresh, 110, 30);
    g_signal_connect(refresh, "clicked", onRefresh, user);
    gtk_box_pack_end(GTK_BOX(row), refresh, FALSE, FALSE, 0);
    return row;
}

GtkWidget* Hint(const std::string& text) {
    GtkWidget* label = StyledLabel(text, "deskhub-hint");
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(label), kHintWrapChars);
    return label;
}

GtkWidget* PasscodeEntry(const std::string& value) {
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), value.c_str());
    gtk_entry_set_width_chars(GTK_ENTRY(entry), gint(deskhub::kPasscodeDigits));
    gtk_entry_set_max_length(GTK_ENTRY(entry), gint(deskhub::kPasscodeDigits));
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
    return entry;
}

GtkWidget* Spin(uint32_t value, uint32_t maxValue) {
    GtkWidget* spin = gtk_spin_button_new_with_range(1, double(maxValue), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), double(value));
    return spin;
}

GtkWidget* WrapPage(GtkWidget* content) {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    AddClass(scroll, "deskhub-page");
    AddClass(content, "deskhub-page");
    AddClass(content, "deskhub-page-body");
    gtk_container_add(GTK_CONTAINER(scroll), content);
    return scroll;
}

void AddColumn(GtkWidget* view, const char* title, int textColumn, int colourColumn,
    int width, float align) {
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", align, nullptr);
    GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(title, renderer, "text",
        textColumn, "foreground", colourColumn, nullptr);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, width);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_alignment(column, align);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
}

void AddPlainColumn(GtkWidget* view, const char* title, int textColumn, int width, float align) {
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", align, nullptr);
    GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(title, renderer, "text",
        textColumn, nullptr);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, width);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_alignment(column, align);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
}

GtkWidget* HostCell(const char* cssClass, int width, float align) {
    GtkWidget* label = StyledLabel(std::string(), cssClass);
    gtk_label_set_xalign(GTK_LABEL(label), align);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(label, width, -1);
    return label;
}

GtkWidget* ListFrame(GtkWidget* view, int height) {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_widget_set_size_request(scroll, -1, height);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    return scroll;
}

bool PickSources(GtkWindow* parent, const std::vector<deskhub::SourceInfo>& sources,
    std::vector<deskhub::SourceInfo>& out) {
    out.clear();
    if (sources.empty()) return false;
    if (!deskhub::DecideAfterSourceQuery(sources).showPicker) {
        out = sources;
        return true;
    }

    GtkWidget* dlg = gtk_dialog_new_with_buttons(ui::kPickerTitle, parent, GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_View", GTK_RESPONSE_ACCEPT, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 460, 340);

    GtkWidget* box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(box), 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);

    GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    for (size_t i = 0; i < sources.size(); ++i) {
        const std::string line = deskhub::media::SourcePickerLabel(sources[i].name,
            sources[i].sourceId, sources[i].width, sources[i].height);
        GtkTreeIter it;
        gtk_list_store_append(store, &it);
        gtk_list_store_set(store, &it, 0, line.c_str(), 1, int(i), -1);
    }

    GtkWidget* tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes(nullptr, gtk_cell_renderer_text_new(), "text", 0,
            nullptr));

    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
    gtk_tree_selection_select_all(sel);

    const auto onActivate = +[](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer d) {
        gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_ACCEPT);
    };
    g_signal_connect(tree, "row-activated", G_CALLBACK(onActivate), dlg);

    gtk_box_pack_start(GTK_BOX(box), ListFrame(tree, 220), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), Label(ui::kPickerEachWindow), FALSE, FALSE, 0);
    gtk_widget_show_all(dlg);

    for (;;) {
        if (gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_ACCEPT) break;

        GList* rows = gtk_tree_selection_get_selected_rows(sel, nullptr);
        for (GList* l = rows; l; l = l->next) {
            GtkTreeIter it;
            if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it,
                    static_cast<GtkTreePath*>(l->data)))
                continue;
            int idx = -1;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &it, 1, &idx, -1);
            if (idx >= 0 && size_t(idx) < sources.size()) out.push_back(sources[size_t(idx)]);
        }
        g_list_free_full(rows, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
        if (!out.empty()) break;
    }

    gtk_widget_destroy(dlg);
    return !out.empty();
}

}

class ConnectionWindow {
public:
    ConnectionWindow(MainWindow* owner, std::string address, std::string passcode, NetAddr server,
        deskhub::HostCaps caps, std::vector<deskhub::SourceInfo> sources, bool control)
        : owner_(owner), address_(std::move(address)), passcode_(std::move(passcode)), server_(server), caps_(caps), sources_(std::move(sources)), control_(control) {
        Build();
    }

    const std::string& Address() const {
        return address_;
    }

    void Present() {
        gtk_window_present(GTK_WINDOW(window_));
    }

    void Destroy() {
        owner_ = nullptr;
        gtk_widget_destroy(window_);
    }

    void ApplyProbe(const deskhubp::DeviceStatus* probe) {
        const bool offline = probe && !probe->online;
        const char* stateClass = offline ? "deskhub-status-offline" : "deskhub-status-online";
        for (GtkWidget* label : {stateLabel_, pingLabel_}) {
            RemoveClass(label, "deskhub-status-online");
            RemoveClass(label, "deskhub-status-offline");
            AddClass(label, stateClass);
        }
        gtk_label_set_text(GTK_LABEL(pingLabel_),
            probe && probe->online ? ui::PingMs(probe->rttMs).c_str() : "");
    }

private:
    void Build() {
        window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window_), address_.c_str());
        gtk_window_set_default_size(GTK_WINDOW(window_), kConnectionWindowWidth, -1);
        g_signal_connect(window_, "destroy", G_CALLBACK(OnDestroy), this);

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(box), 16);
        gtk_container_add(GTK_CONTAINER(window_), box);

        GtkWidget* addressRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_box_pack_start(GTK_BOX(addressRow), StyledLabel(address_, "deskhub-section"), TRUE,
            TRUE, 0);
        GtkWidget* disconnect = gtk_button_new_with_label(ui::kDisconnectButton);
        g_signal_connect(disconnect, "clicked", G_CALLBACK(OnDisconnect), this);
        gtk_box_pack_end(GTK_BOX(addressRow), disconnect, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), addressRow, FALSE, FALSE, 0);

        GtkWidget* stateRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        stateLabel_ = StyledLabel(ui::kConnectedPickSession, "deskhub-status-online");
        gtk_box_pack_start(GTK_BOX(stateRow), stateLabel_, TRUE, TRUE, 0);
        pingLabel_ = StyledLabel(std::string(), "deskhub-status-online");
        gtk_box_pack_end(GTK_BOX(stateRow), pingLabel_, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), stateRow, FALSE, FALSE, 0);

        GtkWidget* desktop = gtk_button_new_with_label(ui::kOpenDesktopLabel);
        gtk_widget_set_sensitive(desktop, !sources_.empty());
        g_signal_connect(desktop, "clicked", G_CALLBACK(OnOpenDesktop), this);
        gtk_box_pack_start(GTK_BOX(box), desktop, FALSE, FALSE, 0);

        controlCheck_ = gtk_check_button_new_with_label(ui::kRequestControlLabel);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controlCheck_), control_);
        gtk_widget_set_sensitive(controlCheck_, !sources_.empty());
        gtk_widget_set_margin_start(controlCheck_, 24);
        g_signal_connect(controlCheck_, "toggled", G_CALLBACK(OnControlToggled), this);
        gtk_box_pack_start(GTK_BOX(box), controlCheck_, FALSE, FALSE, 0);

        GtkWidget* shell = gtk_button_new_with_label(ui::kOpenShellLabel);
        gtk_widget_set_sensitive(shell, caps_.terminal);
        g_signal_connect(shell, "clicked", G_CALLBACK(OnOpenShell), this);
        gtk_box_pack_start(GTK_BOX(box), shell, FALSE, FALSE, 0);

        GtkWidget* files = gtk_button_new_with_label(ui::kOpenFilesLabel);
        gtk_widget_set_sensitive(files, caps_.files);
        g_signal_connect(files, "clicked", G_CALLBACK(OnOpenFiles), this);
        gtk_box_pack_start(GTK_BOX(box), files, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(box), Hint(ui::kMobileHostNote), FALSE, FALSE, 0);
        gtk_widget_show_all(window_);
    }

    static void OnDestroy(GtkWidget*, gpointer user) {
        auto* self = static_cast<ConnectionWindow*>(user);
        if (self->owner_) self->owner_->ForgetConnection(self);
        delete self;
    }

    static void OnDisconnect(GtkButton*, gpointer user) {
        gtk_widget_destroy(static_cast<ConnectionWindow*>(user)->window_);
    }

    static void OnControlToggled(GtkWidget* widget, gpointer user) {
        auto* self = static_cast<ConnectionWindow*>(user);
        self->control_ = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
        if (self->owner_) self->owner_->SetClientControl(self->control_);
    }

    static void OnOpenDesktop(GtkButton*, gpointer user) {
        auto* self = static_cast<ConnectionWindow*>(user);
        if (!self->owner_ || self->sources_.empty()) return;
        std::vector<deskhub::SourceInfo> picked;
        if (!PickSources(GTK_WINDOW(self->window_), self->sources_, picked)) return;
        self->owner_->OpenViewers(self->server_, self->passcode_, picked, self->control_);
    }

    static void OnOpenShell(GtkButton*, gpointer user) {
        auto* self = static_cast<ConnectionWindow*>(user);
        if (self->owner_) self->owner_->OpenShell(self->server_, self->passcode_);
    }

    static void OnOpenFiles(GtkButton*, gpointer user) {
        auto* self = static_cast<ConnectionWindow*>(user);
        if (self->owner_) self->owner_->OpenFileSend(self->server_, self->passcode_);
    }

    MainWindow* owner_ = nullptr;
    std::string address_;
    std::string passcode_;
    NetAddr server_{};
    deskhub::HostCaps caps_{};
    std::vector<deskhub::SourceInfo> sources_;
    bool control_ = false;
    GtkWidget* window_ = nullptr;
    GtkWidget* stateLabel_ = nullptr;
    GtkWidget* pingLabel_ = nullptr;
    GtkWidget* controlCheck_ = nullptr;
};

void MainWindow::Open(GtkApplication* app) {
    auto* w = new MainWindow();
    w->Build(app);
}

void MainWindow::LoadSettings() {
    settings_ = deskhubp::LoadUiSettings();
    recent_ = ui::ParseRecentDevices(deskhubp::ReadAppDataFile(kRecentDevicesFile));
}

uint16_t MainWindow::Port() const {
    return uint16_t(settings_.port);
}

void MainWindow::PostToUi(std::function<void()> fn) {
    RunOnMain([alive = alive_, fn = std::move(fn)] {
        if (alive->load()) fn();
    });
}

void MainWindow::Build(GtkApplication* app) {
    loadingSettings_ = true;
    LoadSettings();
    InstallStyles();

    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), ui::kAppTitle);
    gtk_window_set_default_size(GTK_WINDOW(window_), kWindowW, kWindowH);
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
    g_signal_connect(window_, "delete-event", G_CALLBACK(OnDeleteEvent), this);
    g_signal_connect(window_, "destroy", G_CALLBACK(OnDestroy), this);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), root);

    gtk_box_pack_start(GTK_BOX(root), BuildSidebar(), FALSE, FALSE, 0);

    stack_ = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(stack_), BuildHostPage(), "host");
    gtk_stack_add_named(GTK_STACK(stack_), BuildClientPage(), "client");
    gtk_stack_add_named(GTK_STACK(stack_), BuildDevicesPage(), "devices");
    gtk_stack_add_named(GTK_STACK(stack_), BuildSettingsPage(), "settings");
    gtk_box_pack_start(GTK_BOX(root), stack_, TRUE, TRUE, 0);

    share_.sharingHost().SetTerminal(&share_.terminalHost());
    share_.sharingHost().SetFiles(&share_.fileHost());

    deskhubp::ShareController::Hooks hooks;
    hooks.onError = [this](const std::string& message) {
        ShowError(GTK_WINDOW(window_), "Deskhub", message);
    };
    hooks.postToUi = [this](std::function<void()> fn) { PostToUi(std::move(fn)); };
    hooks.openLocalTerminal = [this](uint32_t termId) {
        return OpenHostTerminalWindow(GTK_WINDOW(window_), share_.terminalHost(), termId);
    };
    hooks.onRowsChanged = [this] { UpdateHostRows(hostStatus_); };
    hooks.askPairing = [this](const PairingRequest& request) { return AskPairing(request); };
    hooks.onBannerChanged = [this] { ApplySharingBanner(); };
    hooks.onNothingLeftShared = [this] { StopHosting(); };
    share_.SetHooks(std::move(hooks));

    loadingSettings_ = false;

    RefreshDeviceList();
    StartPoller();
    StartScan();

    ApplyTrayMode();
    gtk_widget_show_all(window_);
    SelectPage(kPageClient);

    g_signal_connect(gtk_widget_get_screen(window_), "monitors-changed",
        G_CALLBACK(OnMonitorsChanged), this);

    if (settings_.autoShare) {
        SelectPage(kPageHost);
        g_idle_add(
            [](gpointer user) -> gboolean {
                static_cast<MainWindow*>(user)->BeginAutoShare();
                return G_SOURCE_REMOVE;
            },
            this);
    } else {
        g_idle_add(
            [](gpointer user) -> gboolean {
                static_cast<MainWindow*>(user)->StartTenants();
                return G_SOURCE_REMOVE;
            },
            this);
    }
}

GtkWidget* MainWindow::BuildSidebar() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    AddClass(box, "deskhub-sidebar");
    gtk_widget_set_size_request(box, kSidebarW, -1);

    GtkWidget* title = StyledLabel("Deskhub", "deskhub-sidebar-title");
    gtk_widget_set_margin_start(title, kPad);
    gtk_widget_set_margin_end(title, kPad);
    gtk_widget_set_margin_top(title, kPad);
    gtk_widget_set_margin_bottom(title, kPad);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    for (int i = 0; i < kPageCount; ++i) {
        GtkWidget* item = gtk_button_new_with_label(kPageLabels[i]);
        AddClass(item, "deskhub-nav");
        gtk_widget_set_size_request(item, -1, kNavH);
        gtk_widget_set_margin_start(item, 10);
        gtk_widget_set_margin_end(item, 10);
        gtk_widget_set_margin_bottom(item, 10);
        if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(item)))
            gtk_label_set_xalign(GTK_LABEL(child), 0.f);
        g_object_set_data(G_OBJECT(item), "deskhub-page", GINT_TO_POINTER(i));
        g_signal_connect(item, "clicked", G_CALLBACK(OnNavClicked), this);
        gtk_box_pack_start(GTK_BOX(box), item, FALSE, FALSE, 0);
        navButtons_[i] = item;
    }

    GtkWidget* filler = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), filler, TRUE, TRUE, 0);

    GtkWidget* link = gtk_link_button_new_with_label(ui::kProjectUrl, ui::kProjectLinkLabel);
    AddClass(link, "deskhub-link");
    gtk_widget_set_halign(link, GTK_ALIGN_START);
    gtk_widget_set_margin_start(link, kPad - 8);
    gtk_widget_set_margin_end(link, kPad);
    gtk_box_pack_start(GTK_BOX(box), link, FALSE, FALSE, 0);

    GtkWidget* version = StyledLabel(ui::VersionLine(), "deskhub-footnote");
    gtk_widget_set_margin_start(version, kPad);
    gtk_widget_set_margin_end(version, kPad);
    gtk_widget_set_margin_bottom(version, kPad);
    gtk_box_pack_start(GTK_BOX(box), version, FALSE, FALSE, 0);

    return box;
}

GtkWidget* MainWindow::BuildHostPage() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(box), Heading(ui::kHostHeading), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kHostIpIntro), FALSE, FALSE, 0);

    GtkWidget* netRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(netRow), Label(ui::kBindInterfaceLabel), FALSE, FALSE, 0);
    bindCombo_ = gtk_combo_box_text_new();
    PopulateBindCombo();
    g_signal_connect(bindCombo_, "changed", G_CALLBACK(OnBindChanged), this);
    gtk_box_pack_start(GTK_BOX(netRow), bindCombo_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), netRow, FALSE, FALSE, 0);

    hostAddrBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(box), hostAddrBox_, FALSE, FALSE, 0);
    RebuildHostAddressRows();

    hostBanner_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    AddClass(hostBanner_, "deskhub-banner");
    hostStateLabel_ = StyledLabel(std::string(), "deskhub-banner-state");
    gtk_box_pack_start(GTK_BOX(hostBanner_), hostStateLabel_, FALSE, FALSE, 0);
    hostStatusLabel_ = StyledLabel(std::string(), "deskhub-hint");
    gtk_label_set_line_wrap(GTK_LABEL(hostStatusLabel_), TRUE);
    gtk_box_pack_start(GTK_BOX(hostBanner_), hostStatusLabel_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hostBanner_, FALSE, FALSE, 0);

    GtkWidget* pickerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    AddClass(pickerBox, "deskhub-picker");
    displayChecksBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(pickerBox), displayChecksBox_, FALSE, FALSE, 0);
    hostTerminalCheck_ = gtk_check_button_new_with_label(ui::kTerminalPickerLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hostTerminalCheck_), TRUE);
    gtk_box_pack_start(GTK_BOX(pickerBox), hostTerminalCheck_, FALSE, FALSE, 0);
    hostFilesCheck_ = gtk_check_button_new_with_label(ui::kFilesPickerLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hostFilesCheck_), TRUE);
    g_signal_connect(hostFilesCheck_, "toggled", G_CALLBACK(OnFilesTickToggled), this);
    gtk_box_pack_start(GTK_BOX(pickerBox), hostFilesCheck_, FALSE, FALSE, 0);
    hostPickerFrame_ = ListFrame(pickerBox, kListH + 40);
    gtk_box_pack_start(GTK_BOX(box), hostPickerFrame_, TRUE, TRUE, 0);

    hostGrid_ = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(hostGrid_), kHostCellGap);
    gtk_grid_set_row_spacing(GTK_GRID(hostGrid_), kHostRowGap);
    gtk_widget_set_valign(hostGrid_, GTK_ALIGN_START);
    hostGridFrame_ = ListFrame(hostGrid_, kListH + 40);
    gtk_box_pack_start(GTK_BOX(box), hostGridFrame_, TRUE, TRUE, 0);
    RebuildHostRowWidgets();

    hostHintLabel_ = Hint(ui::kPickSourcesHint);
    gtk_box_pack_start(GTK_BOX(box), hostHintLabel_, FALSE, FALSE, 0);

    hostFilesHint_ = Hint(std::string());
    gtk_box_pack_start(GTK_BOX(box), hostFilesHint_, FALSE, FALSE, 0);

    hostPortalNote_ = Hint(ui::kPortalConfirmNote);
    gtk_box_pack_start(GTK_BOX(box), hostPortalNote_, FALSE, FALSE, 0);

    shareButton_ = gtk_button_new_with_label(ui::kStartSharing);
    AddClass(shareButton_, "deskhub-primary");
    gtk_widget_set_size_request(shareButton_, -1, kPrimaryButtonH);
    g_signal_connect(shareButton_, "clicked", G_CALLBACK(OnShareClicked), this);
    gtk_box_pack_start(GTK_BOX(box), shareButton_, FALSE, FALSE, 0);

    RefreshDisplayChoices();
    ShowIdleHostState();
    return WrapPage(box);
}

void MainWindow::RefreshDisplayChoices() {
    std::map<std::string, bool> previousTicks;
    for (size_t i = 0; i < displayChecks_.size() && i < availableDisplays_.size(); ++i) {
        previousTicks[availableDisplays_[i].name] =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(displayChecks_[i]));
    }

    gtk_container_foreach(GTK_CONTAINER(displayChecksBox_), [](GtkWidget* child, gpointer) { gtk_widget_destroy(child); }, nullptr);
    displayChecks_.clear();
    availableDisplays_ = ListMonitors();

    for (size_t i = 0; i < availableDisplays_.size(); ++i) {
        const HostMonitor& monitor = availableDisplays_[i];
        const std::string label = deskhub::media::SourcePickerLabel(monitor.name, uint8_t(i),
            monitor.width, monitor.height);
        GtkWidget* check = gtk_check_button_new_with_label(label.c_str());
        const auto seen = previousTicks.find(monitor.name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
            seen == previousTicks.end() || seen->second);
        gtk_box_pack_start(GTK_BOX(displayChecksBox_), check, FALSE, FALSE, 0);
        displayChecks_.push_back(check);
    }
    gtk_widget_show_all(displayChecksBox_);
}

void MainWindow::ShowHostTable(bool sharing) {
    GtkWidget* shown = sharing ? hostGridFrame_ : hostPickerFrame_;
    GtkWidget* hidden = sharing ? hostPickerFrame_ : hostGridFrame_;
    gtk_widget_set_no_show_all(hidden, TRUE);
    gtk_widget_set_no_show_all(shown, FALSE);
    gtk_widget_hide(hidden);
    gtk_widget_show_all(shown);
    gtk_widget_set_no_show_all(hostHintLabel_, sharing);
    gtk_widget_set_no_show_all(hostPortalNote_, sharing);
    gtk_widget_set_visible(hostHintLabel_, !sharing);
    gtk_widget_set_visible(hostPortalNote_, !sharing);

    const bool showFolder = !sharing && FilesTicked();
    gtk_label_set_text(GTK_LABEL(hostFilesHint_),
        (std::string(ui::kTransferFolderLabel) + " " + deskhubp::PathText(TransferFolder())).c_str());
    gtk_widget_set_no_show_all(hostFilesHint_, !showFolder);
    gtk_widget_set_visible(hostFilesHint_, showFolder);
}

bool MainWindow::TerminalTicked() const {
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hostTerminalCheck_)) != FALSE;
}

bool MainWindow::FilesTicked() const {
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hostFilesCheck_)) != FALSE;
}

std::filesystem::path MainWindow::TransferFolder() const {
    if (settings_.transferDir.empty()) return deskhubp::DefaultTransferDir();
    const std::u8string wide(settings_.transferDir.begin(), settings_.transferDir.end());
    return std::filesystem::path(wide);
}

std::vector<MainWindow::HostMonitor> MainWindow::TickedMonitors() const {
    std::vector<HostMonitor> ticked;
    for (size_t i = 0; i < displayChecks_.size() && i < availableDisplays_.size(); ++i) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(displayChecks_[i])))
            ticked.push_back(availableDisplays_[i]);
    }
    return ticked;
}

std::vector<MainWindow::HostMonitor> MainWindow::ListMonitors() {
    std::vector<HostMonitor> out;
    GdkDisplay* display = gdk_display_get_default();
    if (!display) return out;

    const int count = gdk_display_get_n_monitors(display);
    for (int i = 0; i < count; ++i) {
        GdkMonitor* monitor = gdk_display_get_monitor(display, i);
        if (!monitor) continue;
        GdkRectangle geo{};
        gdk_monitor_get_geometry(monitor, &geo);
        const int scale = std::max(1, gdk_monitor_get_scale_factor(monitor));

        HostMonitor entry;
        entry.name = "Display " + std::to_string(i + 1);
        if (gdk_monitor_is_primary(monitor)) entry.name += " (primary)";
        entry.x = geo.x;
        entry.y = geo.y;
        entry.width = uint32_t(geo.width * scale);
        entry.height = uint32_t(geo.height * scale);
        out.push_back(std::move(entry));
    }
    return out;
}

bool MainWindow::MatchesTickedMonitor(const ShareSource& source,
    const std::vector<HostMonitor>& ticked) {
    return std::any_of(ticked.begin(), ticked.end(), [&source](const HostMonitor& monitor) {
        return (source.x == monitor.x && source.y == monitor.y) || source.name == monitor.name;
    });
}

std::vector<ShareSource> MainWindow::FilterToTickedMonitors(std::vector<ShareSource> sources,
    const std::vector<HostMonitor>& ticked) {
    std::vector<ShareSource> kept;
    for (const ShareSource& source : sources) {
        if (MatchesTickedMonitor(source, ticked)) kept.push_back(source);
    }
    return kept.empty() ? sources : kept;
}

void MainWindow::PopulateBindCombo() {
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(bindCombo_));
    bindChoices_.clear();
    bindChoices_.push_back("");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bindCombo_), ui::kBindAllInterfaces);
    gint active = 0;
    for (const AdapterAddr& adapter : ListLocalIPv4()) {
        const std::string label = adapter.ip + "  (" + adapter.name + ")";
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bindCombo_), label.c_str());
        bindChoices_.push_back(adapter.ip);
        if (adapter.ip == settings_.bindIp) active = gint(bindChoices_.size() - 1);
    }
    if (!settings_.bindIp.empty() && active == 0) {
        const std::string label =
            settings_.bindIp + "  (" + ui::kBindNotConnectedNote + ")";
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bindCombo_), label.c_str());
        bindChoices_.push_back(settings_.bindIp);
        active = gint(bindChoices_.size() - 1);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(bindCombo_), active);
}

void MainWindow::RebuildHostAddressRows() {
    gtk_container_foreach(GTK_CONTAINER(hostAddrBox_), [](GtkWidget* child, gpointer) { gtk_widget_destroy(child); }, nullptr);

    std::vector<AdapterAddr> shown;
    for (const auto& a : ListLocalIPv4())
        if (settings_.bindIp.empty() || a.ip == settings_.bindIp) shown.push_back(a);

    if (shown.empty()) {
        const std::string text = settings_.bindIp.empty()
                                     ? std::string(ui::kNoNetworkAddress)
                                     : settings_.bindIp + "  (" + ui::kBindNotConnectedNote + ")";
        gtk_box_pack_start(GTK_BOX(hostAddrBox_), Label(text), FALSE, FALSE, 0);
    } else {
        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
        int row = 0;
        for (const auto& a : shown) {
            gtk_grid_attach(GTK_GRID(grid), Label(a.name), 0, row, 1, 1);

            GtkWidget* ip = Label(a.ip);
            AddClass(ip, "deskhub-section");
            gtk_label_set_selectable(GTK_LABEL(ip), TRUE);
            gtk_widget_set_hexpand(ip, TRUE);
            gtk_grid_attach(GTK_GRID(grid), ip, 1, row, 1, 1);

            GtkWidget* copy = gtk_button_new_with_label(ui::kCopyButton);
            gtk_widget_set_size_request(copy, 84, 32);
            g_object_set_data_full(G_OBJECT(copy), "deskhub-ip", g_strdup(a.ip.c_str()), g_free);
            g_signal_connect(copy, "clicked", G_CALLBACK(OnCopyClicked), this);
            gtk_grid_attach(GTK_GRID(grid), copy, 2, row, 1, 1);
            ++row;
        }
        gtk_box_pack_start(GTK_BOX(hostAddrBox_), grid, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(hostAddrBox_);
}

void MainWindow::OnBindChanged(GtkWidget*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    if (self->loadingSettings_) return;
    self->SaveSettings();
    self->RebuildHostAddressRows();
}

GtkWidget* MainWindow::BuildClientPage() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(box), Heading(ui::kClientHeading), FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);

    gtk_grid_attach(GTK_GRID(grid), Label(ui::kClientIpPrompt), 0, 0, 1, 1);
    addressEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(addressEntry_), ui::kClientIpPlaceholder);
    gtk_entry_set_width_chars(GTK_ENTRY(addressEntry_), 26);
    g_signal_connect(addressEntry_, "activate", G_CALLBACK(OnAddressActivate), this);
    gtk_grid_attach(GTK_GRID(grid), addressEntry_, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), Label(ui::kUdpPortLabel), 0, 1, 1, 1);
    portEntry_ = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(portEntry_), std::to_string(deskhub::kDeskhubPort).c_str());
    gtk_entry_set_width_chars(GTK_ENTRY(portEntry_), 6);
    gtk_widget_set_halign(portEntry_, GTK_ALIGN_START);
    g_signal_connect(portEntry_, "activate", G_CALLBACK(OnAddressActivate), this);
    gtk_grid_attach(GTK_GRID(grid), portEntry_, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), Label(ui::kClientPasscodePrompt), 0, 2, 1, 1);
    passcodeEntry_ = PasscodeEntry(std::string());
    gtk_widget_set_tooltip_text(passcodeEntry_, ui::kClientPasscodeHint);
    gtk_widget_set_halign(passcodeEntry_, GTK_ALIGN_START);
    g_signal_connect(passcodeEntry_, "activate", G_CALLBACK(OnAddressActivate), this);
    gtk_grid_attach(GTK_GRID(grid), passcodeEntry_, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), Label(ui::kDeviceNameLabel), 0, 3, 1, 1);
    deviceNameEntry_ = gtk_entry_new();
    const std::string initialName =
        settings_.deviceName.empty() ? deskhubp::LocalDeviceName() : settings_.deviceName;
    gtk_entry_set_text(GTK_ENTRY(deviceNameEntry_), initialName.c_str());
    gtk_entry_set_width_chars(GTK_ENTRY(deviceNameEntry_), 26);
    g_signal_connect(deviceNameEntry_, "activate", G_CALLBACK(OnAddressActivate), this);
    gtk_grid_attach(GTK_GRID(grid), deviceNameEntry_, 1, 3, 1, 1);

    addressFormBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(addressFormBox_), grid, FALSE, FALSE, 0);

    connectButton_ = gtk_button_new_with_label(ui::kConnectButton);
    AddClass(connectButton_, "deskhub-primary");
    gtk_widget_set_size_request(connectButton_, -1, kPrimaryButtonH);
    g_signal_connect(connectButton_, "clicked", G_CALLBACK(OnConnectClicked), this);
    gtk_box_pack_start(GTK_BOX(addressFormBox_), connectButton_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), addressFormBox_, FALSE, FALSE, 0);

    clientStatusLabel_ = Hint(std::string());
    gtk_box_pack_start(GTK_BOX(box), clientStatusLabel_, FALSE, FALSE, 0);

    devicesBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(devicesBox_),
        HeadingRow(ui::kDevicesHeading, G_CALLBACK(OnRefreshDevicesClicked), this), FALSE, FALSE,
        0);

    deviceStore_ = gtk_list_store_new(6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* deviceView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(deviceStore_));
    g_object_unref(deviceStore_);
    AddColumn(deviceView, "Device", 0, 5, 170, 0.f);
    AddColumn(deviceView, ui::kDeviceColumnWhere, 1, 5, 120, 0.f);
    AddColumn(deviceView, "Status", 2, 5, 100, 0.f);
    AddColumn(deviceView, "Ping", 3, 5, 70, 1.f);
    AddColumn(deviceView, "Last connected", 4, 5, 150, 0.f);
    g_signal_connect(deviceView, "row-activated", G_CALLBACK(OnDeviceRowActivated), this);
    gtk_box_pack_start(GTK_BOX(devicesBox_), ListFrame(deviceView, kListH), TRUE, TRUE, 0);

    deviceHintLabel_ = Hint(ui::kLanDevicesEmpty);
    gtk_box_pack_start(GTK_BOX(devicesBox_), deviceHintLabel_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), devicesBox_, TRUE, TRUE, 0);

    return WrapPage(box);
}

GtkWidget* MainWindow::BuildDevicesPage() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(box), Heading(ui::kPairedHeading), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kPairedHint), FALSE, FALSE, 0);

    pairedStore_ = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING);
    pairedView_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pairedStore_));
    g_object_unref(pairedStore_);
    AddPlainColumn(pairedView_, ui::kPairedColumnName, 0, 200, 0.f);
    AddPlainColumn(pairedView_, ui::kPairedColumnKey, 1, 130, 0.f);
    AddPlainColumn(pairedView_, ui::kPairedColumnPaired, 2, 150, 0.f);
    AddPlainColumn(pairedView_, ui::kPairedColumnLastSeen, 3, 150, 0.f);
    gtk_box_pack_start(GTK_BOX(box), ListFrame(pairedView_, kListH), TRUE, TRUE, 0);

    pairedHintLabel_ = Hint(ui::kPairedEmpty);
    gtk_box_pack_start(GTK_BOX(box), pairedHintLabel_, FALSE, FALSE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    forgetDeviceButton_ = gtk_button_new_with_label(ui::kPairedForget);
    g_signal_connect(forgetDeviceButton_, "clicked", G_CALLBACK(OnForgetDeviceClicked), this);
    gtk_box_pack_start(GTK_BOX(buttons), forgetDeviceButton_, FALSE, FALSE, 0);
    GtkWidget* forgetAll = gtk_button_new_with_label(ui::kPairedForgetAll);
    g_signal_connect(forgetAll, "clicked", G_CALLBACK(OnForgetAllClicked), this);
    gtk_box_pack_start(GTK_BOX(buttons), forgetAll, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), buttons, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kPairedForgetNote), FALSE, FALSE, 0);

    allowPairingCheck_ = gtk_check_button_new_with_label(ui::kAllowPairingLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allowPairingCheck_),
        settings_.allowNewPairings);
    g_signal_connect(allowPairingCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    gtk_box_pack_start(GTK_BOX(box), allowPairingCheck_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kAllowPairingHint), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kThisMachineHeading), FALSE, FALSE, 0);
    const std::string name =
        settings_.deviceName.empty() ? deskhubp::LocalDeviceName() : settings_.deviceName;
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity(name);
    GtkWidget* keyText = Label(identity.Valid()
                                   ? deskhub::FormatFingerprint(identity.fingerprint)
                                   : std::string(ui::kShareNoHostIdentity));
    gtk_label_set_selectable(GTK_LABEL(keyText), TRUE);
    gtk_box_pack_start(GTK_BOX(box), keyText, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kThisMachineHint), FALSE, FALSE, 0);

    RefreshPairedDevices();
    return WrapPage(box);
}

void MainWindow::RefreshPairedDevices() {
    if (pairedStore_ == nullptr) return;
    pairedDevices_ = deskhubp::LoadPairedDevices().Devices();

    gtk_list_store_clear(pairedStore_);
    for (const deskhub::PairedDevice& device : pairedDevices_) {
        GtkTreeIter it;
        gtk_list_store_append(pairedStore_, &it);
        gtk_list_store_set(pairedStore_, &it, 0,
            device.name.empty() ? "(unnamed)" : device.name.c_str(), 1,
            deskhub::ShortFingerprint(device.fingerprint).c_str(), 2,
            FormatUnixMinute(device.pairedUnix).c_str(), 3,
            FormatUnixMinute(device.lastSeenUnix).c_str(), -1);
    }
    gtk_widget_set_visible(pairedHintLabel_, pairedDevices_.empty());
    gtk_widget_set_sensitive(forgetDeviceButton_, !pairedDevices_.empty());
}

void MainWindow::ForgetSelectedDevice() {
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pairedView_));
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    if (!gtk_tree_selection_get_selected(selection, &model, &it)) return;
    GtkTreePath* path = gtk_tree_model_get_path(model, &it);
    const gint* idx = gtk_tree_path_get_indices(path);
    const bool valid = idx && idx[0] >= 0 && size_t(idx[0]) < pairedDevices_.size();
    if (valid) deskhubp::ForgetPairedDevice(pairedDevices_[size_t(idx[0])].fingerprint);
    gtk_tree_path_free(path);
    if (valid) RefreshPairedDevices();
}

void MainWindow::ForgetEveryDevice() {
    if (pairedDevices_.empty()) return;
    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s", "Deskhub");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s",
        ui::kPairedForgetAllPrompt);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
    const bool confirmed = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
    gtk_widget_destroy(dlg);
    if (!confirmed) return;
    deskhubp::ForgetAllPairedDevices();
    RefreshPairedDevices();
}

void MainWindow::OnForgetDeviceClicked(GtkButton*, gpointer user) {
    static_cast<MainWindow*>(user)->ForgetSelectedDevice();
}

void MainWindow::OnForgetAllClicked(GtkButton*, gpointer user) {
    static_cast<MainWindow*>(user)->ForgetEveryDevice();
}

void MainWindow::OnFilesTickToggled(GtkToggleButton*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    if (!self->Sharing()) self->ShowIdleHostState();
}

void MainWindow::OnTransferDirClicked(GtkButton*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    GtkWidget* chooser = gtk_file_chooser_dialog_new(ui::kTransferFolderLabel,
        GTK_WINDOW(self->window_), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel",
        GTK_RESPONSE_CANCEL, ui::kTransferChooseButton, GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser),
        deskhubp::PathText(self->TransferFolder()).c_str());

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        if (gchar* picked = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser))) {
            self->settings_.transferDir = ui::TruncateSettingsPath(picked);
            g_free(picked);
            gtk_label_set_text(GTK_LABEL(self->transferDirLabel_),
                deskhubp::PathText(self->TransferFolder()).c_str());
            deskhubp::SaveUiSettings(self->settings_);
            if (!self->Sharing()) self->ShowIdleHostState();
        }
    }
    gtk_widget_destroy(chooser);
}

bool MainWindow::AskPairing(const PairingRequest& request) {
    const std::string body = ui::PairingRequestBody(request.name,
        NetAddr::Unpack(request.addrPacked).ToString(), request.shortKey);
    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", ui::kPairingRequestTitle);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kPairingDeny, GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kPairingAllow, GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
    const bool allowed = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
    gtk_widget_destroy(dlg);
    if (allowed) RefreshPairedDevices();
    return allowed;
}

GtkWidget* MainWindow::BuildSettingsPage() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_box_pack_start(GTK_BOX(box), Heading(ui::kSettingsHeading), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kSettingsHint), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kSettingsSectionVideo), FALSE, FALSE, 0);
    GtkWidget* videoGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(videoGrid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(videoGrid), 14);

    gtk_grid_attach(GTK_GRID(videoGrid), Label(ui::kFpsLabel), 0, 0, 1, 1);
    fpsSpin_ = Spin(settings_.fps, ui::kMaxSettingsFps);
    gtk_grid_attach(GTK_GRID(videoGrid), fpsSpin_, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(videoGrid), Label(ui::kBitrateLabel), 0, 1, 1, 1);
    bitrateSpin_ = Spin(settings_.bitrateMbps, ui::kMaxSettingsBitrateMbps);
    gtk_grid_attach(GTK_GRID(videoGrid), bitrateSpin_, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(videoGrid), Label(ui::kQualityLabel), 0, 2, 1, 1);
    qualityCombo_ = gtk_combo_box_text_new();
    for (const auto& preset : deskhub::media::kQualityPresets)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(qualityCombo_), preset.label);
    gtk_combo_box_set_active(GTK_COMBO_BOX(qualityCombo_),
        gint(deskhub::media::QualityPresetIndex(settings_.maxDim)));
    gtk_grid_attach(GTK_GRID(videoGrid), qualityCombo_, 1, 2, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), videoGrid, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kSettingsSectionConnection), FALSE, FALSE, 0);
    GtkWidget* netGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(netGrid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(netGrid), 14);
    gtk_grid_attach(GTK_GRID(netGrid), Label(ui::kUdpPortLabel), 0, 0, 1, 1);
    portSpin_ = Spin(settings_.port, ui::kMaxSettingsPort);
    gtk_grid_attach(GTK_GRID(netGrid), portSpin_, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), netGrid, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kSettingsSectionSecurity), FALSE, FALSE, 0);
    GtkWidget* securityGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(securityGrid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(securityGrid), 14);
    gtk_grid_attach(GTK_GRID(securityGrid), Label(ui::kPasscodeLabel), 0, 0, 1, 1);
    hostPasscodeEntry_ = PasscodeEntry(settings_.passcode);
    gtk_widget_set_halign(hostPasscodeEntry_, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(securityGrid), hostPasscodeEntry_, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), securityGrid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), Hint(ui::kPasscodeHint), FALSE, FALSE, 0);
    allowInputCheck_ = gtk_check_button_new_with_label(ui::kAllowControlLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allowInputCheck_), settings_.allowInput);
    gtk_box_pack_start(GTK_BOX(box), allowInputCheck_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kSettingsSectionSession), FALSE, FALSE, 0);
    clipboardCheck_ = gtk_check_button_new_with_label(ui::kClipboardSyncLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(clipboardCheck_), settings_.clipboardSync);
    gtk_box_pack_start(GTK_BOX(box), clipboardCheck_, FALSE, FALSE, 0);
    shareAudioCheck_ = gtk_check_button_new_with_label(ui::kShareAudioLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shareAudioCheck_), settings_.shareAudio);
    gtk_box_pack_start(GTK_BOX(box), shareAudioCheck_, FALSE, FALSE, 0);
    playAudioCheck_ = gtk_check_button_new_with_label(ui::kPlayAudioLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playAudioCheck_), settings_.playAudio);
    gtk_box_pack_start(GTK_BOX(box), playAudioCheck_, FALSE, FALSE, 0);
    keepAwakeCheck_ = gtk_check_button_new_with_label(ui::kKeepAwakeLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keepAwakeCheck_), settings_.keepAwake);
    gtk_box_pack_start(GTK_BOX(box), keepAwakeCheck_, FALSE, FALSE, 0);

    GtkWidget* folderRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(folderRow), Label(ui::kTransferFolderLabel), FALSE, FALSE, 0);
    transferDirLabel_ = StyledLabel(deskhubp::PathText(TransferFolder()), "deskhub-hint");
    gtk_label_set_ellipsize(GTK_LABEL(transferDirLabel_), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(folderRow), transferDirLabel_, TRUE, TRUE, 0);
    GtkWidget* folderButton = gtk_button_new_with_label(ui::kTransferChooseButton);
    g_signal_connect(folderButton, "clicked", G_CALLBACK(OnTransferDirClicked), this);
    gtk_box_pack_end(GTK_BOX(folderRow), folderButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), folderRow, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), Section(ui::kSettingsSectionLaunch), FALSE, FALSE, 0);
    autostartCheck_ = gtk_check_button_new_with_label(ui::kAutostartLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autostartCheck_),
        deskhubp::AutostartEnabled());
    gtk_box_pack_start(GTK_BOX(box), autostartCheck_, FALSE, FALSE, 0);
    autoShareCheck_ = gtk_check_button_new_with_label(ui::kAutoShareLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoShareCheck_), settings_.autoShare);
    gtk_box_pack_start(GTK_BOX(box), autoShareCheck_, FALSE, FALSE, 0);
    startHiddenCheck_ = gtk_check_button_new_with_label(ui::kCloseToTrayLabel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(startHiddenCheck_), settings_.startHidden);
    gtk_box_pack_start(GTK_BOX(box), startHiddenCheck_, FALSE, FALSE, 0);

    g_signal_connect(fpsSpin_, "value-changed", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(bitrateSpin_, "value-changed", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(portSpin_, "value-changed", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(qualityCombo_, "changed", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(hostPasscodeEntry_, "changed", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(allowInputCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(clipboardCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(shareAudioCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(playAudioCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(keepAwakeCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(autostartCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(autoShareCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);
    g_signal_connect(startHiddenCheck_, "toggled", G_CALLBACK(OnSettingChanged), this);

    return WrapPage(box);
}

void MainWindow::SelectPage(int page) {
    static const char* const kNames[] = {"host", "client", "devices", "settings"};
    gtk_stack_set_visible_child_name(GTK_STACK(stack_), kNames[page]);
    for (int i = 0; i < kPageCount; ++i) {
        if (i == page) {
            AddClass(navButtons_[i], "deskhub-nav-selected");
        } else {
            RemoveClass(navButtons_[i], "deskhub-nav-selected");
        }
    }
    if (page == kPageHost && !Sharing()) RefreshDisplayChoices();
    if (page == kPageDevices) RefreshPairedDevices();
}

void MainWindow::OnNavClicked(GtkButton* b, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->SelectPage(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "deskhub-page")));
}

std::string MainWindow::HostPortDetail() const {
    return ui::UdpPortLine(Port()) + ".";
}

void MainWindow::ApplyHostState(HostShareState state, const std::string& detail) {
    const bool sharing = state == HostShareState::kSharing;
    const bool starting = state == HostShareState::kStarting;

    gtk_label_set_text(GTK_LABEL(hostStateLabel_),
        sharing ? ui::kShareStateOn : (starting ? ui::kStartingShare : ui::kShareStateOff));
    gtk_label_set_text(GTK_LABEL(hostStatusLabel_), detail.c_str());

    RemoveClass(hostBanner_, "deskhub-banner-busy");
    RemoveClass(hostBanner_, "deskhub-banner-live");
    RemoveClass(hostStateLabel_, "deskhub-banner-state-busy");
    RemoveClass(hostStateLabel_, "deskhub-banner-state-live");
    if (sharing) {
        AddClass(hostBanner_, "deskhub-banner-live");
        AddClass(hostStateLabel_, "deskhub-banner-state-live");
    } else if (starting) {
        AddClass(hostBanner_, "deskhub-banner-busy");
        AddClass(hostStateLabel_, "deskhub-banner-state-busy");
    }

    const bool screen = screenSharing_ || starting;
    gtk_button_set_label(GTK_BUTTON(shareButton_),
        screen ? ui::kStopSharing : ui::kStartSharing);
    if (screen) {
        AddClass(shareButton_, "deskhub-primary-stop");
    } else {
        RemoveClass(shareButton_, "deskhub-primary-stop");
    }

    gtk_widget_set_sensitive(bindCombo_, !screen);
    ShowHostTable(screen);
}

void MainWindow::ShowIdleHostState() {
    ApplyHostState(HostShareState::kIdle, HostPortDetail());
}

void MainWindow::SaveSettings() {
    settings_.fps = uint32_t(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(fpsSpin_)));
    settings_.bitrateMbps =
        uint32_t(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(bitrateSpin_)));
    settings_.port = uint32_t(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(portSpin_)));
    const gint quality = gtk_combo_box_get_active(GTK_COMBO_BOX(qualityCombo_));
    if (quality >= 0)
        settings_.maxDim = deskhub::media::QualityPresetMaxDim(size_t(quality), settings_.maxDim);
    settings_.allowInput = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(allowInputCheck_));
    settings_.allowNewPairings =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(allowPairingCheck_));
    settings_.clipboardSync = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(clipboardCheck_));
    settings_.shareAudio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shareAudioCheck_));
    settings_.playAudio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playAudioCheck_));
    settings_.keepAwake = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keepAwakeCheck_));
    const gint bindSel = gtk_combo_box_get_active(GTK_COMBO_BOX(bindCombo_));
    if (bindSel >= 0 && size_t(bindSel) < bindChoices_.size())
        settings_.bindIp = bindChoices_[size_t(bindSel)];
    settings_.autoShare = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoShareCheck_));
    const bool autostart = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autostartCheck_));
    if (autostart != settings_.autostart) {
        deskhubp::SetAutostartEnabled(autostart);
        settings_.autostart = deskhubp::AutostartEnabled();
        loadingSettings_ = true;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autostartCheck_), settings_.autostart);
        loadingSettings_ = false;
    }
    settings_.startHidden = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(startHiddenCheck_));
    ApplyTrayMode();

    const std::string passcode = ui::TrimAscii(gtk_entry_get_text(GTK_ENTRY(hostPasscodeEntry_)));
    if (deskhub::IsValidPasscode(passcode)) settings_.passcode = passcode;

    deskhubp::SaveUiSettings(settings_);
    if (!hosting_ && !hostStarting_) ShowIdleHostState();
}

void MainWindow::SaveRecentDevices() {
    deskhubp::WriteAppDataFile(kRecentDevicesFile, ui::SerializeRecentDevices(recent_));
}

void MainWindow::ApplyTrayMode() {
    if (settings_.startHidden && !tray_.Attached()) {
        EnsureTrayAttached();
        return;
    }
    if (!settings_.startHidden && tray_.Attached()) {
        tray_.Detach();
        ShowMainWindow();
    }
}

bool MainWindow::EnsureTrayAttached() {
    if (tray_.Attached()) return true;
    TrayIcon::Actions actions;
    actions.onToggleWindow = [this] { ToggleWindowFromTray(); };
    actions.onToggleShare = [this] { OnShare(); };
    actions.onQuit = [this] { gtk_widget_destroy(window_); };
    if (!tray_.Attach(actions)) return false;
    tray_.SetSharing(screenSharing_);
    tray_.SetWindowVisible(gtk_widget_get_visible(window_));
    return true;
}

void MainWindow::ToggleWindowFromTray() {
    if (gtk_widget_get_visible(window_)) {
        gtk_widget_hide(window_);
        tray_.SetWindowVisible(false);
        return;
    }
    ShowMainWindow();
}

void MainWindow::ShowMainWindow() {
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
    tray_.SetWindowVisible(true);
    ShowHostTable(screenSharing_);
}

void MainWindow::OnSettingChanged(GtkWidget*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    if (self->loadingSettings_) return;
    self->SaveSettings();
}

void MainWindow::StartScan() {
    scannedThisRound_.clear();
    const bool started = scanner_.Start(
        Port(), [this](const std::function<void()>& fn) { PostToUi(fn); },
        [this](const deskhubp::ScanHit& hit) { OnScanHit(hit); },
        [this](const deskhubp::ScanProgress& progress) { OnScanProgress(progress); },
        [this](const deskhubp::ScanProgress& progress) { OnScanFinished(progress); });
    if (!started) ScheduleRescan();
}

void MainWindow::RescanNow() {
    if (rescanTimerId_) {
        g_source_remove(rescanTimerId_);
        rescanTimerId_ = 0;
    }
    gtk_label_set_text(GTK_LABEL(deviceHintLabel_), ui::kLanDevicesEmpty);
    scanner_.Cancel();
    StartScan();
}

void MainWindow::ScheduleRescan() {
    if (rescanTimerId_) g_source_remove(rescanTimerId_);
    rescanTimerId_ = g_timeout_add(kRescanDelayMs, OnRescanTimer, this);
}

void MainWindow::OnRefreshDevicesClicked(GtkButton*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->RefreshDeviceStatus();
    self->RescanNow();
}

gboolean MainWindow::OnRescanTimer(gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->rescanTimerId_ = 0;
    self->StartScan();
    return G_SOURCE_REMOVE;
}

void MainWindow::OnScanHit(const deskhubp::ScanHit& hit) {
    scannedThisRound_.push_back(hit.addr);
    RecordProbe(hit.addr, true, hit.rttMs);
    for (deskhubp::ScanHit& known : scanned_) {
        if (known.addr != hit.addr) continue;
        known.rttMs = hit.rttMs;
        RefreshDeviceList();
        return;
    }
    scanned_.push_back(hit);
    RefreshDeviceList();
}

void MainWindow::OnScanProgress(const deskhubp::ScanProgress& progress) {
    const std::string text = ui::ScanningStatus(progress.probed, progress.total, Port());
    gtk_label_set_text(GTK_LABEL(deviceHintLabel_), text.c_str());
}

void MainWindow::OnScanFinished(const deskhubp::ScanProgress& progress) {
    const auto gone = [this](const deskhubp::ScanHit& hit) {
        return std::find(scannedThisRound_.begin(), scannedThisRound_.end(), hit.addr) ==
               scannedThisRound_.end();
    };
    scanned_.erase(std::remove_if(scanned_.begin(), scanned_.end(), gone), scanned_.end());
    RefreshDeviceList();

    const std::string text =
        ui::LanDevicesNote(scanned_.size(), progress.total, deskhubp::kLanRescanSecs);
    gtk_label_set_text(GTK_LABEL(deviceHintLabel_), text.c_str());
    ScheduleRescan();
}

void MainWindow::StartPoller() {
    poller_.SetAddresses(ui::AddressesOf(recent_));
    poller_.Start([this](const deskhubp::DeviceStatus& status) {
        PostToUi([this, status] { OnDeviceStatus(status); });
    });
}

void MainWindow::OnDeviceStatus(const deskhubp::DeviceStatus& status) {
    RecordProbe(status.addr, status.online, status.rttMs);
    RefreshDeviceList();
    for (ConnectionWindow* open : connections_)
        if (ui::SameDeviceAddr(open->Address(), status.addr))
            open->ApplyProbe(ProbeFor(status.addr));
}

void MainWindow::RecordProbe(const std::string& addr, bool online, uint32_t rttMs) {
    uint64_t key = 0;
    if (!HostKeyOf(addr, key)) return;
    probes_[key] = deskhubp::DeviceStatus{addr, online, rttMs};
}

const deskhubp::DeviceStatus* MainWindow::ProbeFor(const std::string& addr) const {
    uint64_t key = 0;
    if (!HostKeyOf(addr, key)) return nullptr;
    const auto found = probes_.find(key);
    return found == probes_.end() ? nullptr : &found->second;
}

void MainWindow::RefreshDeviceList() {
    std::vector<std::string> scannedAddrs;
    scannedAddrs.reserve(scanned_.size());
    for (const deskhubp::ScanHit& hit : scanned_) scannedAddrs.push_back(hit.addr);
    deviceRows_ = ui::BuildDeviceRows(scannedAddrs, recent_);

    gtk_list_store_clear(deviceStore_);
    for (const ui::DeviceRow& device : deviceRows_) {
        const deskhubp::DeviceStatus* probe = ProbeFor(device.addr);
        const bool online = probe && probe->online;

        const char* status = !probe ? ui::kStatusChecking
                                    : (online ? ui::kStatusOnline : ui::kStatusOffline);
        const std::string ping = online ? ui::PingMs(probe->rttMs) : std::string("-");
        const char* colour = !probe ? kUnknownColour : (online ? kOnlineColour : kOfflineColour);
        const std::string last = device.lastConnectedUnix != 0
                                     ? FormatUnixMinute(device.lastConnectedUnix)
                                     : std::string("-");

        GtkTreeIter it;
        gtk_list_store_append(deviceStore_, &it);
        gtk_list_store_set(deviceStore_, &it, 0, device.addr.c_str(), 1,
            ui::DeviceOriginLabel(device.origin), 2, status, 3, ping.c_str(), 4, last.c_str(), 5,
            colour, -1);
    }
}

void MainWindow::RefreshDeviceStatus() {
    for (const ui::RecentDevice& device : recent_) {
        uint64_t key = 0;
        if (HostKeyOf(device.addr, key)) probes_.erase(key);
    }
    RefreshDeviceList();
    poller_.RefreshNow();
}

void MainWindow::OnDeviceRowActivated(GtkTreeView*, GtkTreePath* path, GtkTreeViewColumn*,
    gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    const gint* idx = gtk_tree_path_get_indices(path);
    if (!idx || idx[0] < 0 || size_t(idx[0]) >= self->deviceRows_.size()) return;
    const std::string addr = self->deviceRows_[size_t(idx[0])].addr;
    self->ConnectWithPrompt(addr, ui::PasscodeForDevice(self->recent_, addr));
}

void MainWindow::ConnectWithPrompt(const std::string& addr, std::string passcode) {
    std::string target = addr;
    if (!ShowPasscodeDialog(GTK_WINDOW(window_), target, passcode)) return;

    const std::string code = ui::TrimAscii(passcode);
    if (!code.empty() && !deskhub::IsValidPasscode(code)) {
        ShowError(GTK_WINDOW(window_), "Deskhub", ui::kPasscodeInvalid);
        return;
    }

    const uint16_t port = ui::AddressPort(target);
    gtk_entry_set_text(GTK_ENTRY(addressEntry_), ui::AddressHost(target).c_str());
    gtk_entry_set_text(GTK_ENTRY(portEntry_),
        std::to_string(port != 0 ? port : deskhub::kDeskhubPort).c_str());
    gtk_entry_set_text(GTK_ENTRY(passcodeEntry_), code.c_str());
    StartConnect(target, code);
}

bool MainWindow::ReadPasscode(GtkWidget* entry, std::string& out) {
    out = ui::TrimAscii(gtk_entry_get_text(GTK_ENTRY(entry)));
    if (out.empty() || deskhub::IsValidPasscode(out)) return true;

    ShowError(GTK_WINDOW(window_), "Deskhub", ui::kPasscodeInvalid);
    gtk_widget_grab_focus(entry);
    out.clear();
    return false;
}

void MainWindow::SetClientStatus(const std::string& text, bool isError) {
    gtk_label_set_text(GTK_LABEL(clientStatusLabel_), text.c_str());
    if (isError) {
        AddClass(clientStatusLabel_, "deskhub-status-error");
    } else {
        RemoveClass(clientStatusLabel_, "deskhub-status-error");
    }
}

void MainWindow::SetBusy(bool busy, const char* what) {
    gtk_widget_set_sensitive(connectButton_, !busy);
    SetClientStatus(busy && what ? std::string(what) : std::string(), false);
}

void MainWindow::ShowAfterSession() {
    gtk_window_present(GTK_WINDOW(window_));
}

void MainWindow::OnAddressActivate(GtkEntry*, gpointer user) {
    OnConnectClicked(nullptr, user);
}

void MainWindow::OnConnectClicked(GtkButton*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);

    const std::string text = ui::TrimAscii(gtk_entry_get_text(GTK_ENTRY(self->addressEntry_)));
    if (text.empty()) {
        ShowWarning(GTK_WINDOW(self->window_), "Deskhub",
            "Enter the host machine's IP address first (e.g., 192.168.1.10).");
        return;
    }

    std::string passcode;
    if (!self->ReadPasscode(self->passcodeEntry_, passcode)) return;

    const uint16_t port =
        ui::PortOrDefault(gtk_entry_get_text(GTK_ENTRY(self->portEntry_)));
    self->StartConnect(ui::AddressWithPort(text, port), passcode);
}

void MainWindow::StartConnect(const std::string& addr, const std::string& passcode) {
    SetClientStatus(std::string(), false);
    std::string deviceName =
        ui::TruncateDeviceName(gtk_entry_get_text(GTK_ENTRY(deviceNameEntry_)));
    if (deviceName.empty()) deviceName = deskhubp::LocalDeviceName();
    gtk_entry_set_text(GTK_ENTRY(deviceNameEntry_), deviceName.c_str());
    if (deviceName != settings_.deviceName) {
        settings_.deviceName = deviceName;
        deskhubp::SaveUiSettings(settings_);
    }
    NetAddr server{};
    if (!ParseNetAddr(addr, server)) {
        ShowError(GTK_WINDOW(window_), "Deskhub",
            ui::InvalidAddressLine(addr) + "\n" + ui::InvalidAddressHint());
        return;
    }

    const bool started = connectDriver_.QueryAsync(
        server, passcode, [this](const std::function<void()>& fn) { PostToUi(fn); },
        [this, addr, passcode](const deskhubp::ConnectOutcome& outcome) {
            OnSourcesReady(addr, passcode, outcome);
        });
    if (started) SetBusy(true, ui::kQueryingSources);
}

void MainWindow::OpenShell(const NetAddr& server, const std::string& passcode) {
    TerminalLaunch launch;
    launch.address = server.ToString();
    launch.passcode = passcode;
    launch.clientName = ClientDeviceName();

    if (!OpenTerminalWindow(GTK_WINDOW(window_), launch))
        SetClientStatus(ui::kTerminalUnreachable, true);
}

void MainWindow::OpenFileSend(const NetAddr& server, const std::string& passcode) {
    const std::string clientName = ClientDeviceName();

    const std::string address = server.ToString();
    OpenFileSendWindow(GTK_WINDOW(window_), address,
        MakeStandaloneFileSendTarget(server, address, passcode, clientName));
}

void MainWindow::OnSourcesReady(const std::string& addr, const std::string& passcode,
    const deskhubp::ConnectOutcome& outcome) {
    SetBusy(false, nullptr);

    if (!outcome.ok) {
        ShowError(GTK_WINDOW(window_), "Deskhub", ui::SourceQueryFailed(addr));
        return;
    }

    ui::TouchRecentDevice(recent_, addr, int64_t(std::time(nullptr)), passcode);
    SaveRecentDevices();
    poller_.SetAddresses(ui::AddressesOf(recent_));
    RefreshDeviceList();

    OpenConnectionWindow(addr, passcode, outcome);
}

ConnectionWindow* MainWindow::ConnectionFor(const std::string& addr) const {
    for (ConnectionWindow* open : connections_)
        if (ui::SameDeviceAddr(open->Address(), addr)) return open;
    return nullptr;
}

void MainWindow::OpenConnectionWindow(const std::string& addr, const std::string& passcode,
    const deskhubp::ConnectOutcome& outcome) {
    if (ConnectionWindow* open = ConnectionFor(addr)) {
        open->Present();
        return;
    }

    NetAddr server{};
    if (!ParseNetAddr(addr, server)) return;

    auto* window = new ConnectionWindow(this, addr, passcode, server, outcome.caps,
        outcome.sources, settings_.clientControl);
    connections_.push_back(window);
    window->ApplyProbe(ProbeFor(addr));
}

void MainWindow::ForgetConnection(ConnectionWindow* window) {
    connections_.erase(std::remove(connections_.begin(), connections_.end(), window),
        connections_.end());
}

void MainWindow::CloseEveryConnection() {
    const std::vector<ConnectionWindow*> open = connections_;
    connections_.clear();
    for (ConnectionWindow* window : open) window->Destroy();
}

void MainWindow::SetClientControl(bool on) {
    if (settings_.clientControl == on) return;
    settings_.clientControl = on;
    deskhubp::SaveUiSettings(settings_);
}

std::string MainWindow::ClientDeviceName() const {
    std::string name = ui::TruncateDeviceName(gtk_entry_get_text(GTK_ENTRY(deviceNameEntry_)));
    if (name.empty()) name = deskhubp::LocalDeviceName();
    return name;
}

void MainWindow::OpenViewers(const NetAddr& server, const std::string& passcode,
    const std::vector<deskhub::SourceInfo>& picked, bool control) {
    int opened = 0;
    for (const deskhub::SourceInfo& source : picked) {
        if (ViewerWindow::Open(server, source.sourceId, source.name, passcode, control,
                [this, alive = alive_] {
                    if (!alive->load()) return;
                    if (openViewers_.Closed()) ShowAfterSession();
                })) {
            openViewers_.Opened();
            ++opened;
        }
    }
    if (opened == 0) {
        ShowAfterSession();
        ShowWarning(GTK_WINDOW(window_), "Deskhub", ui::kViewerOpenFailed);
    }
}

void MainWindow::OnCopyClicked(GtkButton* b, gpointer) {
    const char* ip = static_cast<const char*>(g_object_get_data(G_OBJECT(b), "deskhub-ip"));
    if (!ip) return;
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), ip, -1);
}

void MainWindow::OnShareClicked(GtkButton*, gpointer user) {
    static_cast<MainWindow*>(user)->OnShare();
}

void MainWindow::BeginAutoShare() {
    if (Sharing() || autoShareGate_.Decided()) {
        autoShareTimerId_ = 0;
        return;
    }

    RefreshDisplayChoices();
    const ui::AutoShareStep step = autoShareGate_.Advance(!availableDisplays_.empty());
    if (step == ui::AutoShareStep::KeepWaiting) {
        ApplyHostState(HostShareState::kIdle, ui::kWaitingForDisplays);
        if (!autoShareTimerId_)
            autoShareTimerId_ = g_timeout_add(autoShareGate_.ProbeMs(), OnAutoShareTimer, this);
        return;
    }

    autoShareTimerId_ = 0;
    if (step == ui::AutoShareStep::GiveUpWaiting)
        LOGW("[Share] No monitor showed up in the %u ms after launch; sharing without one.",
            autoShareGate_.WaitedMs());
    OnShare(ShareTrigger::kAutomatic);
}

gboolean MainWindow::OnAutoShareTimer(gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->BeginAutoShare();
    if (self->autoShareTimerId_) return G_SOURCE_CONTINUE;
    return G_SOURCE_REMOVE;
}

void MainWindow::ReportShareProblem(const char* title, const std::string& text) {
    if (shareTrigger_ == ShareTrigger::kAutomatic) {
        LOGW("[Share] %s", text.c_str());
        ApplyHostState(HostShareState::kIdle, text);
        return;
    }
    ShowWarning(GTK_WINDOW(window_), title, text);
}

void MainWindow::OnMonitorsChanged(GdkScreen*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    if (self->Sharing()) return;
    self->RefreshDisplayChoices();
}

void MainWindow::StartTenants() {
    if (hostStarting_ || Sharing()) return;
    ShareOptions options = deskhub::ShareOptionsOf(settings_, TerminalTicked(), FilesTicked());
    options.port = Port();
    if (options.deviceName.empty()) options.deviceName = deskhubp::LocalDeviceName();
    if (!options.terminal && !options.files) return;
    terminalRequested_ = options.terminal;
    filesRequested_ = options.files;
    StartHosting({}, options);
}

void MainWindow::OnShare(ShareTrigger trigger) {
    if (hostStarting_) return;
    if (screenSharing_) {
        StopHosting();
        StartTenants();
        return;
    }
    if (Sharing()) StopHosting();
    shareTrigger_ = trigger;

    ShareOptions options = deskhub::ShareOptionsOf(settings_, TerminalTicked(), FilesTicked());
    options.port = Port();
    if (options.deviceName.empty()) options.deviceName = deskhubp::LocalDeviceName();
    terminalRequested_ = options.terminal;
    filesRequested_ = options.files;

    const std::vector<HostMonitor> ticked = TickedMonitors();
    if (ticked.empty() && !options.terminal && !options.files) {
        ReportShareProblem("Deskhub",
            availableDisplays_.empty() ? ui::kNoDisplayFound : ui::kNoDisplayTicked);
        return;
    }
    if (ticked.empty()) {
        StartHosting({}, options);
        return;
    }
    const bool filterTicked = ticked.size() < availableDisplays_.size();

    hostStarting_ = true;
    gtk_widget_set_sensitive(shareButton_, FALSE);
    ApplyHostState(HostShareState::kStarting, ui::kWaitingForShareDialog);

    std::thread([this, options, ticked, filterTicked, alive = alive_] {
        std::vector<ShareSource> sources = deskhubp::ListDisplays();
        const bool grantMissesTicked = filterTicked && !sources.empty() &&
                                       std::none_of(sources.begin(), sources.end(), [&ticked](const ShareSource& source) {
                                           return MatchesTickedMonitor(source, ticked);
                                       });
        if (grantMissesTicked) {
            deskhubp::ForgetDisplaySelection();
            std::vector<ShareSource> regranted = deskhubp::ListDisplays();
            if (!regranted.empty()) sources = std::move(regranted);
        }
        const std::string err = sources.empty() ? deskhubp::ListDisplaysError() : std::string();

        RunOnMain([this, sources, options, ticked, filterTicked, err, alive]() mutable {
            if (!alive->load()) return;
            hostStarting_ = false;
            gtk_widget_set_sensitive(shareButton_, TRUE);

            if (sources.empty()) {
                if (options.terminal || options.files) {
                    StartHosting({}, options);
                    return;
                }
                ShowIdleHostState();
                if (!err.empty() && err != deskhubp::kListDisplaysCancelled)
                    ReportShareProblem(ui::kCaptureUnavailableTitle, err);
                return;
            }

            if (filterTicked) sources = FilterToTickedMonitors(std::move(sources), ticked);

            const deskhub::ShareClampResult clamp = deskhub::ClampShareSources(std::move(sources));
            if (clamp.clamped) ReportShareProblem("Deskhub", ui::ShareClampWarning());

            StartHosting(clamp.sources, options);
        });
    }).detach();
}

void MainWindow::StartHosting(const std::vector<ShareSource>& sources,
    const ShareOptions& options) {
    hostStarting_ = true;
    gtk_widget_set_sensitive(shareButton_, FALSE);
    ApplyHostState(HostShareState::kStarting, HostPortDetail());
    ClearHostRows();

    shareDriver_.Join();
    shareDriver_.StartAsync(
        share_.sharingHost(), sources, options,
        [this](const std::function<void()>& fn) { PostToUi(fn); },
        [this, options](bool started, const std::string& error) {
            OnHostStarted(started, error, options);
        });
}

void MainWindow::OnHostStarted(bool started, const std::string& error,
    const ShareOptions& options) {
    hostStarting_ = false;
    gtk_widget_set_sensitive(shareButton_, TRUE);

    if (!started) {
        terminalRequested_ = false;
        filesRequested_ = false;
        ShowIdleHostState();
        ReportShareProblem("Deskhub", std::string(ui::kShareStartFailed) + ".\n\n" + error);
        return;
    }

    hosting_ = true;

    screenSharing_ = !share_.sharingHost().Status().empty();
    sharePort_ = options.port;
    sharePasscodeNote_ = ui::PasscodeNote(options.passcode);
    shareViewOnly_ = !options.allowInput;
    shareBindWarning_ = share_.sharingHost().BindWarning();
    if (terminalRequested_) share_.StartTerminalShare();
    if (filesRequested_) StartFileShare();
    ApplySharingBanner();
    tray_.SetSharing(screenSharing_);

    if (hostTimerId_) g_source_remove(hostTimerId_);
    hostTimerId_ = g_timeout_add(deskhubp::kShareStatusPollMs, OnHostTimer, this);

    if (clipTimerId_) g_source_remove(clipTimerId_);
    clipTimerId_ = 0;
    if (options.clipboardSync) clipTimerId_ = g_timeout_add(1000, OnClipboardTimer, this);
}

bool MainWindow::Sharing() const {
    return hosting_ || hostStarting_ || share_.terminalHost().Running() || share_.fileHost().Running();
}

void MainWindow::ApplySharingBanner() {
    deskhubp::ShareBanner banner;
    banner.screenSharing = screenSharing_;
    banner.hosting = hosting_;
    banner.port = sharePort_;
    banner.viewOnly = shareViewOnly_;
    banner.passcodeNote = sharePasscodeNote_;
    banner.bindWarning = shareBindWarning_;
    ApplyHostState(HostShareState::kSharing, share_.BannerText(banner));
}

void MainWindow::StartFileShare() {
    if (!share_.StartFileShare(TransferFolder())) filesRequested_ = false;
}

void MainWindow::StopHosting() {
    if (hostTimerId_) {
        g_source_remove(hostTimerId_);
        hostTimerId_ = 0;
    }
    if (clipTimerId_) {
        g_source_remove(clipTimerId_);
        clipTimerId_ = 0;
    }
    share_.StopTerminalShare();
    share_.StopFileShare();
    share_.sharingHost().Stop();
    shareDriver_.Join();
    hosting_ = false;
    screenSharing_ = false;
    terminalRequested_ = false;
    filesRequested_ = false;
    shareViewOnly_ = false;
    sharePort_ = 0;
    sharePasscodeNote_.clear();
    shareBindWarning_.clear();
    hostStatus_.clear();
    tray_.SetSharing(false);
    RefreshDisplayChoices();
    ShowIdleHostState();
    ClearHostRows();
}

gboolean MainWindow::OnClipboardTimer(gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    if (!self->hosting_) return G_SOURCE_CONTINUE;

    GtkClipboard* board = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if (const auto remote = self->share_.sharingHost().TakeRemoteClipboard()) {
        gtk_clipboard_set_text(board, remote->c_str(), int(remote->size()));
        return G_SOURCE_CONTINUE;
    }
    if (gchar* text = gtk_clipboard_wait_for_text(board)) {
        self->share_.sharingHost().OfferLocalClipboard(text);
        g_free(text);
    }
    return G_SOURCE_CONTINUE;
}

gboolean MainWindow::OnHostTimer(gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->share_.DrainPairingRequests();
    if (!self->hosting_) {
        self->hostTimerId_ = 0;
        return G_SOURCE_REMOVE;
    }

    std::vector<ShareSourceStatus> rows;
    const deskhubp::ShareDriveState state = self->shareDriver_.Poll(self->share_.sharingHost(), rows);
    if (state == deskhubp::ShareDriveState::Stopped) {
        self->hostTimerId_ = 0;
        self->StopHosting();
        return G_SOURCE_REMOVE;
    }
    if (state == deskhubp::ShareDriveState::Running) {
        self->hostStatus_ = std::move(rows);
        if (self->screenSharing_ && self->hostStatus_.empty()) {
            self->screenSharing_ = false;
            self->ApplySharingBanner();
        }
    }
    self->share_.RefreshShells();
    self->share_.RefreshTransfers();
    self->UpdateHostRows(self->hostStatus_);
    return G_SOURCE_CONTINUE;
}

MainWindow::HostRowWidgets MainWindow::MakeHostRowWidgets(const ui::HostRow& ref, size_t index) {
    HostRowWidgets widgets{};
    for (int i = 0; i < kHostColumnCount; ++i) {
        widgets.cells[i] = HostCell("deskhub-row-cell", kHostColumns[i].width,
            kHostColumns[i].align);
    }

    if (ref.files && ref.viewer) return widgets;

    const bool localShell =
        ref.terminal && ref.viewer && ref.shellState == deskhub::TerminalState::Local;
    const bool remoteRow = ref.viewer && !localShell;
    widgets.action = gtk_button_new_with_label(
        remoteRow ? ui::kDisconnectViewerAction : ui::kStopDisplayAction);
    AddClass(widgets.action, "deskhub-row-action");
    AddClass(widgets.action, remoteRow ? "deskhub-row-action-kick" : "deskhub-row-action-stop");
    gtk_widget_set_size_request(widgets.action, kHostActionWidth, kHostActionHeight);
    gtk_widget_set_valign(widgets.action, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(widgets.action, GTK_ALIGN_END);
    g_object_set_data(G_OBJECT(widgets.action), "deskhub-host-row",
        GINT_TO_POINTER(gint(index)));
    g_signal_connect(widgets.action, "clicked", G_CALLBACK(OnHostRowActionClicked), this);

    if (ref.terminal && ref.viewer && !localShell) {
        widgets.attach = gtk_button_new_with_label(ui::kAttachShellAction);
        AddClass(widgets.attach, "deskhub-row-action");
        AddClass(widgets.attach, "deskhub-row-action-stop");
        gtk_widget_set_size_request(widgets.attach, kHostActionWidth, kHostActionHeight);
        gtk_widget_set_valign(widgets.attach, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(widgets.attach, GTK_ALIGN_END);
        g_object_set_data(G_OBJECT(widgets.attach), "deskhub-host-row",
            GINT_TO_POINTER(gint(index)));
        g_signal_connect(widgets.attach, "clicked", G_CALLBACK(OnHostRowAttachClicked), this);
    }
    return widgets;
}

void MainWindow::RebuildHostRowWidgets() {
    GList* children = gtk_container_get_children(GTK_CONTAINER(hostGrid_));
    for (GList* child = children; child; child = child->next) {
        gtk_widget_destroy(GTK_WIDGET(child->data));
    }
    g_list_free(children);

    for (int i = 0; i < kHostColumnCount; ++i) {
        GtkWidget* title = HostCell("deskhub-row-header", kHostColumns[i].width,
            kHostColumns[i].align);
        gtk_label_set_text(GTK_LABEL(title), kHostColumns[i].title);
        gtk_grid_attach(GTK_GRID(hostGrid_), title, i, 0, 1, 1);
    }

    hostRowWidgets_.clear();
    hostRowWidgets_.reserve(hostRows_.size());
    for (size_t i = 0; i < hostRows_.size(); ++i) {
        hostRowWidgets_.push_back(MakeHostRowWidgets(hostRows_[i], i));
        const HostRowWidgets& widgets = hostRowWidgets_[i];
        const gint row = gint(i) + 1;
        for (int c = 0; c < kHostColumnCount; ++c) {
            gtk_grid_attach(GTK_GRID(hostGrid_), widgets.cells[c], c, row, 1, 1);
        }
        if (widgets.action)
            gtk_grid_attach(GTK_GRID(hostGrid_), widgets.action, kHostColumnCount, row, 1, 1);
        if (widgets.attach)
            gtk_grid_attach(GTK_GRID(hostGrid_), widgets.attach, kHostColumnCount + 1, row, 1,
                1);
    }
    gtk_widget_show_all(hostGrid_);
}

void MainWindow::ClearHostRows() {
    hostRows_.clear();
    RebuildHostRowWidgets();
}

void MainWindow::FillHostRow(const HostRowWidgets& widgets, const ui::HostRowCells& cells) {
    const std::string* text[kHostColumnCount] = {&cells.source, &cells.size, &cells.viewers,
        &cells.client, &cells.capture, &cells.send, &cells.mbps, &cells.rtt};
    for (int i = 0; i < kHostColumnCount; ++i) {
        gtk_label_set_text(GTK_LABEL(widgets.cells[i]), text[i]->c_str());
        if (cells.online) {
            AddClass(widgets.cells[i], "deskhub-row-cell-online");
        } else {
            RemoveClass(widgets.cells[i], "deskhub-row-cell-online");
        }
    }
}

void MainWindow::UpdateHostRows(const std::vector<ShareSourceStatus>& rows) {
    std::vector<ui::HostRow> refs = ui::BuildHostRows(rows, share_.terminalHost().Running(), share_.shells(),
        share_.fileHost().Running(), share_.transfers());
    if (refs != hostRows_) {
        hostRows_ = std::move(refs);
        RebuildHostRowWidgets();
    }

    for (size_t i = 0; i < hostRows_.size() && i < hostRowWidgets_.size(); ++i) {
        const ui::HostRow& ref = hostRows_[i];
        if (ref.terminal) {
            FillHostRow(hostRowWidgets_[i], ui::TerminalRowText(ref, Port(), share_.shells()));
            continue;
        }
        if (ref.files) {
            FillHostRow(hostRowWidgets_[i],
                ui::FilesRowText(ref, deskhubp::PathText(share_.fileHost().Directory()), share_.transfers()));
            continue;
        }
        const ShareSourceStatus* source = ui::FindHostSource(rows, ref.sourceId);
        if (source) FillHostRow(hostRowWidgets_[i], ui::HostRowText(ref, *source));
    }
}

void MainWindow::RunRowAction(const ui::HostRow& row) {
    if (row.files) {
        if (!row.viewer) share_.StopFilesRow(screenSharing_);
        return;
    }
    if (row.terminal) {
        if (row.viewer) {
            share_.KickShell(row.termId);
        } else {
            share_.StopTerminalRow(screenSharing_);
        }
        return;
    }
    if (!hosting_) return;
    if (!row.viewer) {
        share_.sharingHost().StopSource(row.sourceId);
        return;
    }
    NetAddr addr{};
    if (!ParseNetAddr(row.viewerAddr, addr)) return;
    share_.sharingHost().KickViewer(row.sourceId, addr.Pack());
}

void MainWindow::OnHostRowActionClicked(GtkButton* b, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    const gint index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "deskhub-host-row"));
    if (index < 0 || size_t(index) >= self->hostRows_.size()) return;
    self->RunRowAction(self->hostRows_[size_t(index)]);
}

void MainWindow::OnHostRowAttachClicked(GtkButton* b, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    const gint index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "deskhub-host-row"));
    if (index < 0 || size_t(index) >= self->hostRows_.size()) return;
    const deskhub::ui::HostRow& row = self->hostRows_[size_t(index)];
    if (row.terminal && row.viewer) self->share_.StopAndAttachShell(row.termId);
}

gboolean MainWindow::OnDeleteEvent(GtkWidget*, GdkEvent*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    const bool sessionActive = self->Sharing();
    const bool keepRunning = self->settings_.startHidden || sessionActive;
    LOGI("[Ui] Window close requested: startHidden=%d hosting=%d starting=%d",
        self->settings_.startHidden ? 1 : 0, self->hosting_ ? 1 : 0,
        self->hostStarting_ ? 1 : 0);
    if (keepRunning && self->EnsureTrayAttached()) {
        LOGI("[Ui] Hiding to the tray; sessions keep running.");
        gtk_widget_hide(self->window_);
        self->tray_.SetWindowVisible(false);
        return TRUE;
    }
    LOGW("[Ui] Quitting on close (%s).",
        keepRunning ? "no tray available" : "background mode off, no session");
    return FALSE;
}

void MainWindow::OnDestroy(GtkWidget*, gpointer user) {
    auto* self = static_cast<MainWindow*>(user);
    self->alive_->store(false);
    self->CloseEveryConnection();
    self->tray_.Detach();
    if (self->rescanTimerId_) g_source_remove(self->rescanTimerId_);
    if (self->hostTimerId_) g_source_remove(self->hostTimerId_);
    if (self->autoShareTimerId_) g_source_remove(self->autoShareTimerId_);
    self->scanner_.Cancel();
    self->share_.sharingHost().Stop();
    self->shareDriver_.Join();
    self->poller_.Stop();
    delete self;
}
