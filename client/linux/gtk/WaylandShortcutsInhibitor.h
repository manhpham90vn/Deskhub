#pragma once
#include <gtk/gtk.h>

struct wl_proxy;

class WaylandShortcutsInhibitor {
public:
    WaylandShortcutsInhibitor() = default;
    ~WaylandShortcutsInhibitor();
    WaylandShortcutsInhibitor(const WaylandShortcutsInhibitor&) = delete;
    WaylandShortcutsInhibitor& operator=(const WaylandShortcutsInhibitor&) = delete;

    static bool Applies(GtkWidget* toplevel);
    void Inhibit(GtkWidget* toplevel);
    void Release();

private:
    wl_proxy* manager_ = nullptr;
    wl_proxy* inhibitor_ = nullptr;
};
