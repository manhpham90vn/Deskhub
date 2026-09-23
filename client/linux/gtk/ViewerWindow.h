#pragma once
#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>

#include "decode/AvDecoder.h"
#include "deskhub/input/PointerLockState.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/ScreenViewer.h"
#include "render/VideoRenderer.h"

class ViewerWindow {
public:
    static ViewerWindow* Open(const NetAddr& server, uint8_t sourceId,
        const std::string& sourceName, const std::string& passcode, bool control,
        std::function<void()> onClosed);

private:
    ViewerWindow() = default;
    ~ViewerWindow();
    ViewerWindow(const ViewerWindow&) = delete;
    ViewerWindow& operator=(const ViewerWindow&) = delete;

    bool Build(const NetAddr& server, uint8_t sourceId, const std::string& sourceName,
        const std::string& passcode, bool control);

    void VideoRect(int& x, int& y, int& w, int& h) const;
    bool ToNormalized(double px, double py, int32_t& nx, int32_t& ny) const;

    bool InContent(double px, double py) const;

    void ApplyLockEffect(const deskhub::PointerLockEffect& effect);
    void GrabPointer(bool locked);
    void AskAboutKey(deskhub::TrustVerdict verdict, const std::string& fingerprint);
    void UpdateTitle();
    void UpdateLinkLabel();
    void SizeToVideo();
    void EndSession();

    void PostToMain(std::function<void(ViewerWindow&)> fn);

    static gboolean OnRender(GtkGLArea* area, GdkGLContext* ctx, gpointer user);
    static void OnRealize(GtkGLArea* area, gpointer user);
    static void OnUnrealize(GtkGLArea* area, gpointer user);
    static gboolean OnKey(GtkWidget* w, GdkEventKey* e, gpointer user);
    static gboolean OnMotion(GtkWidget* w, GdkEventMotion* e, gpointer user);
    static gboolean OnButton(GtkWidget* w, GdkEventButton* e, gpointer user);
    static gboolean OnScroll(GtkWidget* w, GdkEventScroll* e, gpointer user);
    static gboolean OnFocusOut(GtkWidget* w, GdkEventFocus* e, gpointer user);
    static gboolean OnTick(GtkWidget* w, GdkFrameClock* clock, gpointer user);
    static void OnDestroy(GtkWidget* w, gpointer user);
    static void OnDisconnectClicked(GtkWidget* w, gpointer user);

    GtkWidget* window_ = nullptr;
    GtkWidget* glArea_ = nullptr;
    GtkWidget* linkLabel_ = nullptr;
    uint64_t queuedFrameSerial_ = 0;
    guint clipTimerId_ = 0;
    guint linkTimerId_ = 0;
    bool clipboardSync_ = false;

    static gboolean OnClipboardTimer(gpointer user);
    static gboolean OnLinkTimer(gpointer user);

    std::shared_ptr<ViewerWindow*> alive_;

    std::string baseTitle_;
    std::string statusLine_;
    std::string shownTitle_;
    uint32_t fittedW_ = 0;
    uint32_t fittedH_ = 0;
    bool control_ = true;
    bool ended_ = false;
    std::function<void()> onClosed_;

    VideoRenderer renderer_;
    deskhubp::ScreenViewer<AvDecoder, VideoSink*> loop_;

    deskhub::PointerLockState pointer_;
    double lastPx_ = 0, lastPy_ = 0;
    bool haveLastPos_ = false;
};
