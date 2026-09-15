#include "deskhubp/media/PortalScreenCast.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <unistd.h>

#include <atomic>
#include <cstdio>

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/system/AppDataFile.h"

namespace deskhubp {
namespace {

constexpr const char* kBusName = "org.freedesktop.portal.Desktop";
constexpr const char* kObjPath = "/org/freedesktop/portal/desktop";
constexpr const char* kScreenCast = "org.freedesktop.portal.ScreenCast";
constexpr const char* kRequestIface = "org.freedesktop.portal.Request";
constexpr const char* kSessionIface = "org.freedesktop.portal.Session";

constexpr int kQuestionTimeoutSec = 180;

constexpr uint32_t kSourceTypeMonitor = 1;

constexpr uint32_t kCursorModeHidden = 1;
constexpr uint32_t kCursorModeEmbedded = 2;

constexpr uint32_t kPersistModePersistent = 2;
constexpr uint32_t kScreenCastVersionPersist = 4;

constexpr const char* kRestoreTokenFile = "portal-restore-token.txt";

std::atomic<uint64_t> g_tokenCounter{0};

std::string NextToken(const char* prefix) {
    char b[64];
    std::snprintf(b, sizeof(b), "deskhub_%s_%llu", prefix,
        (unsigned long long)g_tokenCounter.fetch_add(1) + 1);
    return b;
}

std::string SenderPathPart(GDBusConnection* conn) {
    const char* unique = g_dbus_connection_get_unique_name(conn);
    std::string s = unique ? unique : "";
    if (!s.empty() && s[0] == ':') s.erase(0, 1);
    for (char& c : s)
        if (c == '.') c = '_';
    return s;
}

std::string RequestPath(GDBusConnection* conn, const std::string& token) {
    return std::string("/org/freedesktop/portal/desktop/request/") + SenderPathPart(conn) + "/" +
           token;
}

struct ResponseWait {
    GMainLoop* loop = nullptr;
    guint32 code = 2;
    GVariant* results = nullptr;
};

void OnResponse(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
    GVariant* params, gpointer user) {
    auto* w = static_cast<ResponseWait*>(user);
    guint32 code = 2;
    GVariant* results = nullptr;
    g_variant_get(params, "(u@a{sv})", &code, &results);
    w->code = code;
    w->results = results;
    g_main_loop_quit(w->loop);
}

gboolean OnWaitTimeout(gpointer user) {
    auto* w = static_cast<ResponseWait*>(user);
    LOGE("[Portal] No Response signal within %d s — giving up.", kQuestionTimeoutSec);
    g_main_loop_quit(w->loop);
    return G_SOURCE_REMOVE;
}

GVariant* PortalRequest(GDBusConnection* conn, GMainContext* ctx, const char* method,
    GVariant* params, const std::string& token, std::string* err, bool* cancelled = nullptr) {
    const std::string reqPath = RequestPath(conn, token);

    ResponseWait wait;
    wait.loop = g_main_loop_new(ctx, FALSE);

    const guint sub = g_dbus_connection_signal_subscribe(conn, kBusName, kRequestIface, "Response",
        reqPath.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NONE, OnResponse, &wait, nullptr);

    GError* gerr = nullptr;
    GVariant* ret = g_dbus_connection_call_sync(conn, kBusName, kObjPath, kScreenCast, method,
        params, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &gerr);
    if (!ret) {
        if (err) *err = std::string(method) + ": " + (gerr ? gerr->message : "D-Bus call failed");
        if (gerr) g_error_free(gerr);
        g_dbus_connection_signal_unsubscribe(conn, sub);
        g_main_loop_unref(wait.loop);
        return nullptr;
    }
    g_variant_unref(ret);

    GSource* to = g_timeout_source_new_seconds(kQuestionTimeoutSec);
    g_source_set_callback(to, OnWaitTimeout, &wait, nullptr);
    g_source_attach(to, ctx);

    g_main_loop_run(wait.loop);

    g_source_destroy(to);
    g_source_unref(to);
    g_dbus_connection_signal_unsubscribe(conn, sub);
    g_main_loop_unref(wait.loop);

    if (wait.code != 0) {
        if (wait.results) g_variant_unref(wait.results);
        if (cancelled) *cancelled = wait.code == 1;
        if (err)
            *err = wait.code == 1 ? deskhubp::kListDisplaysCancelled
                                  : std::string(method) + ": portal ended the request";
        return nullptr;
    }
    return wait.results;
}

uint32_t ReadUintProperty(GDBusConnection* conn, const char* name) {
    GError* gerr = nullptr;
    GVariant* ret = g_dbus_connection_call_sync(conn, kBusName, kObjPath,
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", kScreenCast, name), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &gerr);
    if (!ret) {
        if (gerr) g_error_free(gerr);
        return 0;
    }
    GVariant* inner = nullptr;
    g_variant_get(ret, "(v)", &inner);
    uint32_t v = 0;
    if (inner && g_variant_is_of_type(inner, G_VARIANT_TYPE_UINT32)) v = g_variant_get_uint32(inner);
    if (inner) g_variant_unref(inner);
    g_variant_unref(ret);
    return v;
}

}

PortalScreenCast& PortalScreenCast::Instance() {
    static PortalScreenCast inst;
    return inst;
}

PortalScreenCast::~PortalScreenCast() {
    Close();
}

bool PortalScreenCast::Open() {
    if (isOpen()) return true;
    if (restoreToken_.empty())
        restoreToken_ = deskhub::ui::TrimAscii(ReadAppDataFile(kRestoreTokenFile));

    AttemptResult result = OpenAttempt();
    if (result == AttemptResult::FailedWithToken) {
        LOGW("[Portal] The saved screen selection was rejected — asking with the dialog.");
        ForgetSavedSelection();
        result = OpenAttempt();
    }
    return result == AttemptResult::Ok;
}

void PortalScreenCast::ForgetSavedSelection() {
    restoreToken_.clear();
    RemoveAppDataFile(kRestoreTokenFile);
}

PortalScreenCast::AttemptResult PortalScreenCast::OpenAttempt() {
    lastError_.clear();
    streams_.clear();
    bool sentToken = false;
    bool cancelled = false;

    GMainContext* ctx = g_main_context_new();
    g_main_context_push_thread_default(ctx);

    auto finish = [&](AttemptResult result) {
        g_main_context_pop_thread_default(ctx);
        g_main_context_unref(ctx);
        return result;
    };
    auto fail = [&] {
        if (cancelled) return finish(AttemptResult::Cancelled);
        return finish(sentToken ? AttemptResult::FailedWithToken : AttemptResult::Failed);
    };

    GError* gerr = nullptr;
    if (!bus_) {
        bus_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &gerr);
        if (!bus_) {
            lastError_ = std::string("no session D-Bus: ") + (gerr ? gerr->message : "?");
            if (gerr) g_error_free(gerr);
            LOGE("[Portal] %s", lastError_.c_str());
            return fail();
        }
    }
    GDBusConnection* conn = static_cast<GDBusConnection*>(bus_);

    const uint32_t version = ReadUintProperty(conn, "version");
    const uint32_t cursorModes = ReadUintProperty(conn, "AvailableCursorModes");
    if (version == 0) {
        lastError_ =
            "xdg-desktop-portal ScreenCast not available — install "
            "xdg-desktop-portal-gnome (GNOME), -kde (KDE) or -wlr (wlroots)";
        LOGE("[Portal] %s", lastError_.c_str());
        return fail();
    }
    LOGI("[Portal] ScreenCast version %u, cursor modes 0x%x.", version, cursorModes);

    const std::string tok1 = NextToken("req");
    const std::string sessTok = NextToken("sess");
    {
        GVariantBuilder ob;
        g_variant_builder_init(&ob, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&ob, "{sv}", "handle_token", g_variant_new_string(tok1.c_str()));
        g_variant_builder_add(&ob, "{sv}", "session_handle_token",
            g_variant_new_string(sessTok.c_str()));

        GVariant* res = PortalRequest(conn, ctx, "CreateSession", g_variant_new("(a{sv})", &ob),
            tok1, &lastError_);
        if (!res) {
            LOGE("[Portal] CreateSession failed: %s", lastError_.c_str());
            return fail();
        }
        const char* sh = nullptr;
        g_variant_lookup(res, "session_handle", "&s", &sh);
        if (sh) sessionHandle_ = sh;
        g_variant_unref(res);
        if (sessionHandle_.empty()) {
            lastError_ = "portal returned no session_handle";
            LOGE("[Portal] %s", lastError_.c_str());
            return fail();
        }
    }

    {
        const std::string tok2 = NextToken("req");
        GVariantBuilder ob;
        g_variant_builder_init(&ob, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&ob, "{sv}", "handle_token", g_variant_new_string(tok2.c_str()));
        g_variant_builder_add(&ob, "{sv}", "types", g_variant_new_uint32(kSourceTypeMonitor));
        g_variant_builder_add(&ob, "{sv}", "multiple", g_variant_new_boolean(TRUE));
        g_variant_builder_add(&ob, "{sv}", "cursor_mode",
            g_variant_new_uint32((cursorModes & kCursorModeEmbedded) ? kCursorModeEmbedded
                                                                     : kCursorModeHidden));
        if (version >= kScreenCastVersionPersist) {
            g_variant_builder_add(&ob, "{sv}", "persist_mode",
                g_variant_new_uint32(kPersistModePersistent));
            if (!restoreToken_.empty()) {
                g_variant_builder_add(&ob, "{sv}", "restore_token",
                    g_variant_new_string(restoreToken_.c_str()));
                sentToken = true;
            }
        }

        GVariant* res = PortalRequest(conn, ctx, "SelectSources",
            g_variant_new("(oa{sv})", sessionHandle_.c_str(), &ob), tok2, &lastError_,
            &cancelled);
        if (sentToken) {
            restoreToken_.clear();
            RemoveAppDataFile(kRestoreTokenFile);
        }
        if (!res) {
            LOGE("[Portal] SelectSources failed: %s", lastError_.c_str());
            return fail();
        }
        g_variant_unref(res);
    }

    {
        const std::string tok3 = NextToken("req");
        GVariantBuilder ob;
        g_variant_builder_init(&ob, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&ob, "{sv}", "handle_token", g_variant_new_string(tok3.c_str()));

        if (sentToken)
            LOGI("[Portal] Reusing the saved screen selection — no dialog should appear.");
        else
            LOGI(
                "[Portal] Asking the compositor for screen capture — a system dialog will "
                "appear.");
        GVariant* res = PortalRequest(conn, ctx, "Start",
            g_variant_new("(osa{sv})", sessionHandle_.c_str(), "", &ob), tok3, &lastError_,
            &cancelled);
        if (!res) {
            LOGE("[Portal] Start failed: %s", lastError_.c_str());
            Close();
            return fail();
        }

        const char* rt = nullptr;
        if (g_variant_lookup(res, "restore_token", "&s", &rt) && rt) {
            restoreToken_ = rt;
            WriteAppDataFile(kRestoreTokenFile, restoreToken_);
        }

        GVariant* list = g_variant_lookup_value(res, "streams", G_VARIANT_TYPE("a(ua{sv})"));
        if (list) {
            GVariantIter it;
            g_variant_iter_init(&it, list);
            guint32 nodeId = 0;
            GVariant* props = nullptr;
            while (g_variant_iter_next(&it, "(u@a{sv})", &nodeId, &props)) {
                PortalStream s;
                s.nodeId = nodeId;

                gint32 x = 0, y = 0, w = 0, h = 0;
                if (GVariant* pos = g_variant_lookup_value(props, "position",
                        G_VARIANT_TYPE("(ii)"))) {
                    g_variant_get(pos, "(ii)", &x, &y);
                    g_variant_unref(pos);
                }
                if (GVariant* sz = g_variant_lookup_value(props, "size", G_VARIANT_TYPE("(ii)"))) {
                    g_variant_get(sz, "(ii)", &w, &h);
                    g_variant_unref(sz);
                }
                s.x = x;
                s.y = y;
                s.width = w > 0 ? uint32_t(w) : 0;
                s.height = h > 0 ? uint32_t(h) : 0;

                const char* id = nullptr;
                g_variant_lookup(props, "id", "&s", &id);
                char label[128];
                if (id && *id)
                    std::snprintf(label, sizeof(label), "%s (%ux%u)", id, s.width, s.height);
                else
                    std::snprintf(label, sizeof(label), "Screen %zu (%ux%u)", streams_.size() + 1,
                        s.width, s.height);
                s.name = label;

                LOGI("[Portal] Stream node %u: %s at %d,%d", s.nodeId, s.name.c_str(), s.x, s.y);
                streams_.push_back(std::move(s));
                g_variant_unref(props);
            }
            g_variant_unref(list);
        }
        g_variant_unref(res);

        if (streams_.empty()) {
            lastError_ = "portal returned no stream (no screen selected)";
            LOGE("[Portal] %s", lastError_.c_str());
            Close();
            return fail();
        }
    }

    pipewireFd_ = OpenRemoteFd();
    if (pipewireFd_ < 0) {
        Close();
        return finish(AttemptResult::Failed);
    }

    LOGI("[Portal] Ready: %zu screen(s), PipeWire fd %d.", streams_.size(), pipewireFd_);
    return finish(AttemptResult::Ok);
}

int PortalScreenCast::OpenRemoteFd() {
    GDBusConnection* conn = static_cast<GDBusConnection*>(bus_);
    if (!conn || sessionHandle_.empty()) {
        lastError_ = "no ScreenCast session to open a PipeWire remote on";
        LOGE("[Portal] %s", lastError_.c_str());
        return -1;
    }

    GVariantBuilder ob;
    g_variant_builder_init(&ob, G_VARIANT_TYPE_VARDICT);

    GError* gerr = nullptr;
    GUnixFDList* fdList = nullptr;
    GVariant* ret = g_dbus_connection_call_with_unix_fd_list_sync(conn, kBusName, kObjPath,
        kScreenCast, "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", sessionHandle_.c_str(), &ob), G_VARIANT_TYPE("(h)"),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &fdList, nullptr, &gerr);
    if (!ret) {
        lastError_ = std::string("OpenPipeWireRemote: ") + (gerr ? gerr->message : "?") +
                     " \xE2\x80\x94 every captured screen asks the portal for its own "
                     "PipeWire remote, because two PipeWire connections cannot share one "
                     "socket";
        if (gerr) g_error_free(gerr);
        LOGE("[Portal] %s", lastError_.c_str());
        if (fdList) g_object_unref(fdList);
        return -1;
    }

    gint32 idx = -1;
    g_variant_get(ret, "(h)", &idx);
    g_variant_unref(ret);

    const int fd = fdList ? g_unix_fd_list_get(fdList, idx, &gerr) : -1;
    if (fdList) g_object_unref(fdList);
    if (fd < 0) {
        lastError_ = std::string("no PipeWire fd: ") + (gerr ? gerr->message : "?");
        if (gerr) g_error_free(gerr);
        LOGE("[Portal] %s", lastError_.c_str());
        return -1;
    }
    return fd;
}

void PortalScreenCast::Close() {
    if (pipewireFd_ >= 0) {
        close(pipewireFd_);
        pipewireFd_ = -1;
    }
    GDBusConnection* conn = static_cast<GDBusConnection*>(bus_);
    if (conn && !sessionHandle_.empty()) {
        g_dbus_connection_call(conn, kBusName, sessionHandle_.c_str(), kSessionIface, "Close",
            nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, nullptr, nullptr);
        g_dbus_connection_flush_sync(conn, nullptr, nullptr);
    }
    sessionHandle_.clear();
    if (conn) {
        g_object_unref(conn);
        bus_ = nullptr;
    }
    streams_.clear();
}

}
