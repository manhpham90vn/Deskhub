#include "gtk/FileSendDialog.h"

#include <system_error>
#include <utility>

#include "deskhub/ui/Strings.h"
#include "deskhubp/client/FileTransferClient.h"
#include "deskhubp/client/FileUpload.h"
#include "gtk/GtkUtil.h"

namespace {

namespace ui = deskhub::ui;

constexpr guint kPollMs = 200;
constexpr int kWindowW = 560;
constexpr int kWindowH = 520;
constexpr int kListH = 140;
constexpr int kPad = 16;

constexpr const char* kSentGlyph = "\xE2\x9C\x93";
constexpr const char* kNotSentGlyph = "\xE2\x9C\x95";

std::string Utf8Of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

uint64_t SizeOf(const std::filesystem::path& path) {
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? 0 : uint64_t(size);
}

class StandaloneTarget final : public FileSendTarget {
public:
    explicit StandaloneTarget(deskhubp::FileTransferClientConfig config)
        : config_(std::move(config)) {}

    ~StandaloneTarget() override {
        client_.Stop();
    }

    bool Begin(const std::vector<std::filesystem::path>& files) override {
        client_.Stop();
        error_.clear();

        const deskhubp::FileBatch batch = deskhubp::InspectFiles(files);
        if (!batch.Ok()) {
            error_ = batch.error;
            return false;
        }

        deskhubp::FileTransferClientConfig config = config_;
        config.files = files;
        if (!client_.Start(config, deskhubp::FileTransferClientCallbacks{})) {
            error_ = ui::CouldNotConnectTo(config_.hostLabel);
            return false;
        }
        return true;
    }

    void Cancel() override {
        client_.Cancel();
    }

    bool AcceptChangedKey() override {
        return client_.AcceptKeyAndRetry();
    }

    deskhub::ui::TransferView View() const override {
        return client_.View();
    }

    std::string LastError() const override {
        return error_;
    }

private:
    deskhubp::FileTransferClientConfig config_{};
    std::string error_;
    deskhubp::FileTransferClient client_;
};

struct SendWindow {
    GtkWidget* window = nullptr;
    GtkWidget* chooseButton = nullptr;
    GtkWidget* sendButton = nullptr;
    GtkWidget* stopButton = nullptr;
    GtkWidget* emptyLabel = nullptr;
    GtkWidget* errorLabel = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* stepLabel = nullptr;
    GtkWidget* progress = nullptr;
    GtkWidget* progressBox = nullptr;
    GtkWidget* historyBox = nullptr;
    GtkListStore* chosenStore = nullptr;
    GtkListStore* historyStore = nullptr;
    guint timerId = 0;
    bool settled = true;
    std::vector<std::filesystem::path> chosen;
    std::vector<uint64_t> sizes;
    std::unique_ptr<FileSendTarget> target;
    deskhub::ui::TransferView view;
};

void AddColumn(GtkWidget* view, const char* title, int index, int width) {
    GtkCellRenderer* cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, nullptr);
    GtkTreeViewColumn* column =
        gtk_tree_view_column_new_with_attributes(title, cell, "text", index, nullptr);
    gtk_tree_view_column_set_fixed_width(column, width);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
}

GtkWidget* ListFrame(GtkWidget* child, int height) {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_widget_set_size_request(scroll, -1, height);
    gtk_container_add(GTK_CONTAINER(scroll), child);
    return scroll;
}

void SetErrorText(SendWindow& self, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(self.errorLabel), text.c_str());
    gtk_widget_set_visible(self.errorLabel, !text.empty());
}

void ShowChosen(SendWindow& self) {
    gtk_list_store_clear(self.chosenStore);
    for (size_t i = 0; i < self.chosen.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(self.chosenStore, &iter);
        gtk_list_store_set(self.chosenStore, &iter, 0,
            Utf8Of(self.chosen[i].filename()).c_str(), 1,
            ui::FileSizeText(self.sizes[i]).c_str(), -1);
    }
    gtk_widget_set_visible(self.emptyLabel, self.chosen.empty());
}

void Refresh(SendWindow& self) {
    const deskhub::ui::TransferView& view = self.view;
    const bool active = view.active;

    gtk_widget_set_sensitive(self.chooseButton, !active);
    gtk_widget_set_sensitive(self.sendButton, !active && !self.chosen.empty());
    gtk_widget_set_visible(self.stopButton, active);

    gtk_widget_set_visible(self.progressBox, !view.Idle());
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self.progress), view.Fraction());
    gtk_label_set_text(GTK_LABEL(self.statusLabel), view.StatusLine().c_str());
    gtk_label_set_text(GTK_LABEL(self.stepLabel), view.Step().c_str());
}

void Settle(SendWindow& self) {
    if (self.settled || self.view.active) return;
    if (!self.view.done && !self.view.failed) return;
    self.settled = true;

    for (size_t i = 0; i < self.chosen.size(); ++i) {
        const bool sent = self.view.done || i < size_t(self.view.fileIndex);
        GtkTreeIter iter;
        gtk_list_store_append(self.historyStore, &iter);
        gtk_list_store_set(self.historyStore, &iter, 0, sent ? kSentGlyph : kNotSentGlyph, 1,
            Utf8Of(self.chosen[i].filename()).c_str(), 2,
            sent ? ui::FileSizeText(self.sizes[i]).c_str() : self.view.message.c_str(), -1);
    }
    gtk_widget_set_visible(self.historyBox, TRUE);

    self.chosen.clear();
    self.sizes.clear();
    ShowChosen(self);
}

bool AskAboutChangedKey(SendWindow& self) {
    std::string body(ui::kTrustChangedBody);
    body += "\n\n";
    body += ui::kTrustFingerprintLabel;
    body += " ";
    body += self.view.fingerprint;

    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(self.window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", ui::kTrustChangedTitle);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kTrustReject, GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dlg), ui::kTrustAccept, GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_NO);
    const bool accepted = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
    gtk_widget_destroy(dlg);
    if (!accepted || !self.target->AcceptChangedKey()) return false;

    self.view = deskhub::ui::TransferView{};
    self.view.active = true;
    Refresh(self);
    return true;
}

gboolean OnPollTimer(gpointer user) {
    auto* self = static_cast<SendWindow*>(user);
    const deskhub::ui::TransferView view = self->target->View();
    if (!(view == self->view)) {
        self->view = view;
        Refresh(*self);
    }
    if (self->view.active) return G_SOURCE_CONTINUE;

    if (self->view.keyChanged && !self->settled && AskAboutChangedKey(*self))
        return G_SOURCE_CONTINUE;

    Settle(*self);
    Refresh(*self);
    self->timerId = 0;
    return G_SOURCE_REMOVE;
}

void OnChooseClicked(GtkButton*, gpointer user) {
    auto* self = static_cast<SendWindow*>(user);
    GtkWidget* chooser = gtk_file_chooser_dialog_new(ui::kTransferSendHeading,
        GTK_WINDOW(self->window), GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL,
        ui::kTransferChooseButton, GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }

    std::vector<std::filesystem::path> picked;
    GSList* names = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(chooser));
    for (GSList* node = names; node; node = node->next) {
        auto* name = static_cast<char*>(node->data);
        picked.push_back(std::filesystem::path(name));
        g_free(name);
    }
    g_slist_free(names);
    gtk_widget_destroy(chooser);

    const bool overflowed = picked.size() > deskhub::kMaxTransferFiles;
    if (overflowed) picked.resize(deskhub::kMaxTransferFiles);

    self->chosen = std::move(picked);
    self->sizes.clear();
    for (const std::filesystem::path& path : self->chosen) self->sizes.push_back(SizeOf(path));
    SetErrorText(*self, overflowed ? ui::kTransferTooManyFiles : "");
    ShowChosen(*self);
    Refresh(*self);
}

void OnSendClicked(GtkButton*, gpointer user) {
    auto* self = static_cast<SendWindow*>(user);
    if (self->chosen.empty() || self->view.active) return;

    SetErrorText(*self, "");
    if (!self->target->Begin(self->chosen)) {
        SetErrorText(*self, self->target->LastError());
        return;
    }

    self->settled = false;
    self->view = deskhub::ui::TransferView{};
    self->view.active = true;
    Refresh(*self);
    if (self->timerId) g_source_remove(self->timerId);
    self->timerId = g_timeout_add(kPollMs, OnPollTimer, self);
}

void OnStopClicked(GtkButton*, gpointer user) {
    static_cast<SendWindow*>(user)->target->Cancel();
}

void OnWindowDestroy(GtkWidget*, gpointer user) {
    auto* self = static_cast<SendWindow*>(user);
    if (self->timerId) g_source_remove(self->timerId);
    delete self;
}

GtkWidget* MutedLabel(const std::string& text) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.f);
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "deskhub-hint");
    return label;
}

}

std::unique_ptr<FileSendTarget> MakeStandaloneFileSendTarget(const NetAddr& server,
    const std::string& address, const std::string& passcode, const std::string& clientName) {
    deskhubp::FileTransferClientConfig config;
    config.host = server;
    config.hostLabel = address;
    config.passcode = passcode;
    config.clientName = clientName;
    return std::make_unique<StandaloneTarget>(std::move(config));
}

void OpenFileSendWindow(GtkWindow* parent, const std::string& subtitle,
    std::unique_ptr<FileSendTarget> target) {
    auto* self = new SendWindow();
    self->target = std::move(target);

    self->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(self->window), ui::kTransferSendHeading);
    gtk_window_set_default_size(GTK_WINDOW(self->window), kWindowW, kWindowH);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(self->window), parent);
    g_signal_connect(self->window, "destroy", G_CALLBACK(OnWindowDestroy), self);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), kPad);
    gtk_container_add(GTK_CONTAINER(self->window), box);

    GtkWidget* heading = gtk_label_new(ui::kTransferSendHeading);
    gtk_label_set_xalign(GTK_LABEL(heading), 0.f);
    gtk_style_context_add_class(gtk_widget_get_style_context(heading), "deskhub-heading");
    gtk_box_pack_start(GTK_BOX(box), heading, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), MutedLabel(subtitle), FALSE, FALSE, 0);

    self->chosenStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* chosenView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(self->chosenStore));
    g_object_unref(self->chosenStore);
    AddColumn(chosenView, "File", 0, 320);
    AddColumn(chosenView, "Size", 1, 110);
    gtk_box_pack_start(GTK_BOX(box), ListFrame(chosenView, kListH), TRUE, TRUE, 0);

    self->emptyLabel = MutedLabel(ui::kTransferNoneChosen);
    gtk_box_pack_start(GTK_BOX(box), self->emptyLabel, FALSE, FALSE, 0);

    self->errorLabel = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(self->errorLabel), 0.f);
    gtk_label_set_line_wrap(GTK_LABEL(self->errorLabel), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(self->errorLabel),
        "deskhub-status-error");
    gtk_box_pack_start(GTK_BOX(box), self->errorLabel, FALSE, FALSE, 0);

    self->progressBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    self->progress = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(self->progressBox), self->progress, FALSE, FALSE, 0);
    GtkWidget* statusRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    self->statusLabel = MutedLabel("");
    gtk_label_set_ellipsize(GTK_LABEL(self->statusLabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(statusRow), self->statusLabel, TRUE, TRUE, 0);
    self->stepLabel = MutedLabel("");
    gtk_box_pack_end(GTK_BOX(statusRow), self->stepLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(self->progressBox), statusRow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), self->progressBox, FALSE, FALSE, 0);

    self->historyBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(self->historyBox), MutedLabel(ui::kTransferSentHeading), FALSE,
        FALSE, 0);
    self->historyStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* historyView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(self->historyStore));
    g_object_unref(self->historyStore);
    AddColumn(historyView, "", 0, 28);
    AddColumn(historyView, "File", 1, 250);
    AddColumn(historyView, "Result", 2, 180);
    gtk_box_pack_start(GTK_BOX(self->historyBox), ListFrame(historyView, kListH), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), self->historyBox, TRUE, TRUE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    self->chooseButton = gtk_button_new_with_label(ui::kTransferChooseButton);
    g_signal_connect(self->chooseButton, "clicked", G_CALLBACK(OnChooseClicked), self);
    gtk_box_pack_start(GTK_BOX(buttons), self->chooseButton, FALSE, FALSE, 0);

    self->sendButton = gtk_button_new_with_label("Send");
    gtk_style_context_add_class(gtk_widget_get_style_context(self->sendButton),
        "deskhub-primary");
    g_signal_connect(self->sendButton, "clicked", G_CALLBACK(OnSendClicked), self);
    gtk_box_pack_end(GTK_BOX(buttons), self->sendButton, FALSE, FALSE, 0);

    self->stopButton = gtk_button_new_with_label(ui::kTransferCancelButton);
    g_signal_connect(self->stopButton, "clicked", G_CALLBACK(OnStopClicked), self);
    gtk_box_pack_end(GTK_BOX(buttons), self->stopButton, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), buttons, FALSE, FALSE, 0);

    gtk_widget_show_all(self->window);
    gtk_widget_set_no_show_all(self->errorLabel, TRUE);
    gtk_widget_set_no_show_all(self->historyBox, TRUE);
    gtk_widget_set_visible(self->errorLabel, FALSE);
    gtk_widget_set_visible(self->historyBox, FALSE);
    Refresh(*self);
}
