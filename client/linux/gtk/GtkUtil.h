#pragma once
#include <gtk/gtk.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "deskhub/ui/Controls.h"
#include "deskhub/ui/HostRows.h"

inline void AddClass(GtkWidget* widget, const char* name) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

inline void RemoveClass(GtkWidget* widget, const char* name) {
    gtk_style_context_remove_class(gtk_widget_get_style_context(widget), name);
}

inline GtkWidget* StandardButton(const char* label) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(button, deskhub::ui::kButtonMinWidth, deskhub::ui::kButtonHeight);
    return button;
}

inline GtkWidget* PrimaryButton(const char* label) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(button, -1, deskhub::ui::kPrimaryButtonHeight);
    return button;
}

inline GtkWidget* RowButton(const char* label, const char* cssClass) {
    GtkWidget* button = gtk_button_new_with_label(label);
    AddClass(button, "deskhub-row-action");
    AddClass(button, cssClass);
    gtk_widget_set_size_request(button, deskhub::ui::kHostActionWidth,
        deskhub::ui::kHostActionHeight);
    gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
    return button;
}

inline GtkWidget* TextField(int widthChars) {
    GtkWidget* entry = gtk_entry_new();
    if (widthChars > 0) gtk_entry_set_width_chars(GTK_ENTRY(entry), widthChars);
    gtk_widget_set_size_request(entry, -1, deskhub::ui::kFieldHeight);
    return entry;
}

inline GtkWidget* NumberField(uint32_t value, uint32_t maxValue) {
    GtkWidget* spin = gtk_spin_button_new_with_range(1, double(maxValue), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), double(value));
    gtk_widget_set_size_request(spin, -1, deskhub::ui::kFieldHeight);
    return spin;
}

inline void RunOnMain(std::function<void()> fn) {
    auto* boxed = new std::function<void()>(std::move(fn));
    g_idle_add(
        [](gpointer data) -> gboolean {
            auto* f = static_cast<std::function<void()>*>(data);
            (*f)();
            delete f;
            return G_SOURCE_REMOVE;
        },
        boxed);
}

inline void ShowMessage(GtkWindow* parent, GtkMessageType type, const char* title,
    const std::string& detail) {
    GtkWidget* dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_CLOSE,
        "%s", title);
    if (!detail.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", detail.c_str());
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

inline void ShowError(GtkWindow* parent, const char* title, const std::string& detail) {
    ShowMessage(parent, GTK_MESSAGE_ERROR, title, detail);
}

inline void ShowWarning(GtkWindow* parent, const char* title, const std::string& detail) {
    ShowMessage(parent, GTK_MESSAGE_WARNING, title, detail);
}

inline void ShowInfo(GtkWindow* parent, const char* title, const std::string& detail) {
    ShowMessage(parent, GTK_MESSAGE_INFO, title, detail);
}
