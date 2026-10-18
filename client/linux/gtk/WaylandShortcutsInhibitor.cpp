#include "gtk/WaylandShortcutsInhibitor.h"

#include "deskhubp/diag/Log.h"

#ifdef DESKHUB_HAVE_WAYLAND_SHORTCUTS_INHIBIT
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

#include <cstring>

namespace {

constexpr uint32_t kManagerDestroyOpcode = 0;
constexpr uint32_t kManagerInhibitShortcutsOpcode = 1;
constexpr uint32_t kInhibitorDestroyOpcode = 0;
constexpr int kProtocolVersion = 1;

extern const wl_interface kInhibitorInterface;

const wl_interface* kNoTypes[] = {nullptr};
const wl_interface* kInhibitShortcutsTypes[] = {
    &kInhibitorInterface, &wl_surface_interface, &wl_seat_interface};

const wl_message kManagerRequests[] = {
    {"destroy", "", kNoTypes},
    {"inhibit_shortcuts", "nos", kInhibitShortcutsTypes},
};
const wl_interface kManagerInterface = {
    "zwp_keyboard_shortcuts_inhibit_manager_v1", kProtocolVersion, 2, kManagerRequests, 0,
    nullptr};

const wl_message kInhibitorRequests[] = {{"destroy", "", kNoTypes}};
const wl_message kInhibitorEvents[] = {{"active", "", kNoTypes}, {"inactive", "", kNoTypes}};
const wl_interface kInhibitorInterface = {"zwp_keyboard_shortcuts_inhibitor_v1",
    kProtocolVersion, 1, kInhibitorRequests, 2, kInhibitorEvents};

void OnGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface,
    uint32_t version) {
    auto** manager = static_cast<wl_proxy**>(data);
    if (*manager || std::strcmp(interface, kManagerInterface.name) != 0) return;
    if (version < uint32_t(kProtocolVersion)) return;
    *manager = static_cast<wl_proxy*>(
        wl_registry_bind(registry, name, &kManagerInterface, uint32_t(kProtocolVersion)));
}

void OnGlobalRemove(void*, wl_registry*, uint32_t) {}

const wl_registry_listener kRegistryListener = {OnGlobal, OnGlobalRemove};

wl_proxy* BindInhibitManager(wl_display* display) {
    wl_proxy* manager = nullptr;
    wl_event_queue* queue = wl_display_create_queue(display);
    if (!queue) return nullptr;
    auto* wrapped = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    if (!wrapped) {
        wl_event_queue_destroy(queue);
        return nullptr;
    }
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), queue);
    wl_registry* registry = wl_display_get_registry(wrapped);
    wl_proxy_wrapper_destroy(wrapped);
    if (registry) {
        wl_registry_add_listener(registry, &kRegistryListener, &manager);
        wl_display_roundtrip_queue(display, queue);
        wl_registry_destroy(registry);
    }
    if (manager) wl_proxy_set_queue(manager, nullptr);
    wl_event_queue_destroy(queue);
    return manager;
}

}

WaylandShortcutsInhibitor::~WaylandShortcutsInhibitor() {
    Release();
    if (!manager_) return;
    wl_proxy_marshal(manager_, kManagerDestroyOpcode);
    wl_proxy_destroy(manager_);
    manager_ = nullptr;
}

bool WaylandShortcutsInhibitor::Applies(GtkWidget* toplevel) {
    return GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(toplevel));
}

void WaylandShortcutsInhibitor::Inhibit(GtkWidget* toplevel) {
    if (inhibitor_) return;
    GdkWindow* gw = gtk_widget_get_window(toplevel);
    GdkDisplay* display = gtk_widget_get_display(toplevel);
    if (!gw || !GDK_IS_WAYLAND_DISPLAY(display)) return;

    if (!manager_) manager_ = BindInhibitManager(gdk_wayland_display_get_wl_display(display));
    if (!manager_) {
        LOGW("[Viewer] This Wayland compositor offers no keyboard-shortcuts-inhibit; the "
             "desktop keeps its Super shortcuts while the viewer is focused.");
        return;
    }

    wl_surface* surface = gdk_wayland_window_get_wl_surface(gw);
    wl_seat* seat = gdk_wayland_seat_get_wl_seat(gdk_display_get_default_seat(display));
    if (!surface || !seat) return;
    inhibitor_ = wl_proxy_marshal_constructor(manager_, kManagerInhibitShortcutsOpcode,
        &kInhibitorInterface, nullptr, surface, seat);
    wl_display_flush(gdk_wayland_display_get_wl_display(display));
}

void WaylandShortcutsInhibitor::Release() {
    if (!inhibitor_) return;
    wl_proxy_marshal(inhibitor_, kInhibitorDestroyOpcode);
    wl_proxy_destroy(inhibitor_);
    inhibitor_ = nullptr;
}

#else

WaylandShortcutsInhibitor::~WaylandShortcutsInhibitor() = default;

bool WaylandShortcutsInhibitor::Applies(GtkWidget*) {
    return false;
}

void WaylandShortcutsInhibitor::Inhibit(GtkWidget*) {}

void WaylandShortcutsInhibitor::Release() {}

#endif
