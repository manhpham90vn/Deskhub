// AgentLoop (--serve) - ghép chuỗi GD2 với mạng GD3:
//
//   Thread FrameArrived (WGC):  capture -> NVENC -> onPacket(NAL)
//       -> nếu session STREAMING: Packetizer.SendFrame -> sock.SendTo(peer)
//       -> chưa STREAMING: bỏ NAL (không đếm)
//   Thread chính (Recv):        recvfrom timeout 100ms -> HostSession.HandlePacket
//       -> gói hợp lệ: cập nhật peer theo địa chỉ nguồn (roaming theo sessionId)
//       -> mỗi vòng: HostSession.Tick + in thống kê mỗi 1s
//
// ForceKeyframe là ATOMIC FLAG: đặt từ thread Recv (onStart/onKeyframeRequest),
// tiêu thụ ở lần Encode kế tiếp trên thread FrameArrived (docs/06 §4).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "AgentLoop.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>

#include <wrl/client.h>

#include "GpuSelect.h"
#include "InputInjector.h"
#include "IVideoEncoder.h"
#include "NetInfo.h"
#include "TimeUs.h"
#include "UdpSocket.h"
#include "WindowCapture.h"

#include "rgc/HostSession.h"
#include "rgc/Packetizer.h"

namespace {

std::atomic<bool> g_ctrlC{false};

BOOL WINAPI CtrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_ctrlC.store(true);
        return TRUE;
    }
    return FALSE;
}

const char* StateName(rgc::HostSession::State s) {
    switch (s) {
    case rgc::HostSession::State::Idle:      return "IDLE";
    case rgc::HostSession::State::Ready:     return "READY";
    case rgc::HostSession::State::Streaming: return "STREAMING";
    }
    return "?";
}

} // namespace

int RunAgent(HWND target, const AgentOptions& opt) {
    g_ctrlC.store(false);
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    GpuChoice gpu;
    if (!CreateBestDevice({ GpuVendor::Nvidia, GpuVendor::Intel, GpuVendor::Amd }, gpu)) {
        std::printf("[Agent] Failed to create D3D11 device.\n");
        return 1;
    }
    std::wprintf(L"[Agent] GPU: %ls [%ls]\n", gpu.description.c_str(), GpuVendorName(gpu.vendor));
    {
        // Immediate context được dùng từ 2 thread (FrameArrived copy cache,
        // Recv encode lại frame tĩnh) -> bật multithread protection.
        Microsoft::WRL::ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(gpu.device.As(&mt))) mt->SetMultithreadProtected(TRUE);
    }

    UdpSocket sock;
    if (!sock.Open(opt.port)) return 1;
    sock.SetRecvTimeout(100);
    std::printf("[Agent] Listening on UDP port %u. On the other machine, open client.exe"
                " and enter one of:\n", opt.port);
    for (const auto& a : ListLocalIPv4())
        std::wprintf(L"    %hs:%u    (%ls)\n", a.ip.c_str(), opt.port, a.name.c_str());

    // --- Trạng thái chia sẻ giữa thread FrameArrived và thread Recv ---
    std::atomic<uint32_t> srcW{0}, srcH{0};       // kích thước NÉN (đã làm chẵn)
    std::atomic<uint32_t> srcTexW{0}, srcTexH{0}; // kích thước texture WGC thật
    std::atomic<bool>     sizeChanged{false};     // dat o FrameArrived, tieu thu o Recv
    std::atomic<bool>     wantFec{false};         // dat o Recv, ap dung o FrameArrived
    std::atomic<uint32_t> curBitrateBps{opt.bitrateMbps * 1'000'000u};
    std::atomic<bool>     netReady{false};   // session da tao xong (sau frame dau)
    std::atomic<bool>     failed{false};
    std::atomic<bool>     forceIdr{false};   // cau IDR: dat o Recv, tieu thu o Encode
    std::atomic<uint64_t> peerPacked{0};     // NetAddr::Pack cua client hien tai (0 = chua co)
    std::atomic<uint64_t> bytesSent{0}, framesSent{0};
    std::atomic<uint32_t> captured{0};

    std::unique_ptr<rgc::HostSession> session; // tạo sau khi biết kích thước nguồn
    rgc::Packetizer packetizer;
    std::atomic<uint32_t> nextFrameId{0}; // chạm từ cả 2 thread (frame mới / re-encode tĩnh)

    std::mutex encMutex; // bảo vệ encoder + cachedTex giữa 2 thread
    std::unique_ptr<IVideoEncoder> encoder;

    // WGC chỉ phát frame khi nội dung ĐỔI. Cache frame cuối để khi client xin IDR
    // mà nguồn đang tĩnh (menu, màn hình đứng im) vẫn có cái để encode gửi đi —
    // không cache thì client join màn hình tĩnh sẽ đen vĩnh viễn.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> cachedTex;
    std::atomic<bool>     haveCached{false};
    std::atomic<uint64_t> lastFrameUs{0};

    // NAL vừa nén xong (thread FrameArrived) -> cắt gói -> UDP tới client.
    auto onPacket = [&](const uint8_t* data, size_t size, uint64_t tsUs, bool keyframe) {
        if (!session || session->state() != rgc::HostSession::State::Streaming) return;
        const uint64_t pp = peerPacked.load(std::memory_order_acquire);
        if (!pp) return;
        const NetAddr peer = NetAddr::Unpack(pp);
        packetizer.SetSessionId(session->sessionId());
        // Packetizer là single-thread (thread này). Thread Recv chỉ đặt ý muốn qua
        // atomic, việc bật/tắt thật diễn ra ở đây — khỏi cần khóa.
        packetizer.SetFecEnabled(wantFec.load(std::memory_order_relaxed));
        const size_t pkts = packetizer.SendFrame(std::span<const uint8_t>(data, size),
                                                 nextFrameId++, tsUs, keyframe,
            [&](std::span<const uint8_t> d) {
                sock.SendTo(peer, d.data(), d.size());
                bytesSent.fetch_add(d.size(), std::memory_order_relaxed);
            });
        if (pkts) framesSent.fetch_add(1, std::memory_order_relaxed);
    };

    // Tạo encoder nếu chưa có. GỌI DƯỚI encMutex. false = backend không dùng được.
    // `w`/`h` là kích thước NÉN (chẵn); `sw`/`sh` là kích thước texture thật.
    auto ensureEncoder = [&](uint32_t w, uint32_t h, uint32_t sw, uint32_t sh) -> bool {
        if (encoder) return true;
        EncoderConfig cfg;
        cfg.width = w;
        cfg.height = h;
        cfg.srcWidth = sw;
        cfg.srcHeight = sh;
        cfg.fps = opt.fps;
        cfg.bitrateBps = curBitrateBps.load(std::memory_order_relaxed);
        cfg.outputPath.clear(); // không file — NAL chỉ đi qua onPacket
        cfg.onPacket = onPacket;
        encoder = CreateEncoder(gpu.device.Get(), cfg);
        if (!encoder) {
            std::printf("[Agent] No usable encoder backend (NVENC + Media Foundation"
                        " both failed) — stopping.\n");
            failed.store(true);
            return false;
        }
        return true;
    };

    auto onFrame = [&](const FrameInfo& fi) {
        captured.fetch_add(1, std::memory_order_relaxed);
        if (failed.load()) return;

        // NV12 lấy mẫu chroma 2x2 -> bề rộng/cao lẻ làm CreateTexture2D(NV12) trả
        // E_INVALIDARG. Nén ở kích thước chẵn nhỏ hơn; cột/hàng lẻ dư bị cắt.
        const uint32_t encW = fi.width & ~1u, encH = fi.height & ~1u;
        if (!encW || !encH) return;

        std::lock_guard<std::mutex> lk(encMutex);

        // Người dùng kéo đổi kích thước cửa sổ đang chia sẻ. Encoder và texture cache
        // đều gắn chặt với kích thước cũ -> vứt cả hai, dựng lại ngay ở frame này.
        // Cờ sizeChanged để thread Recv báo RECONFIG + IDR cho client.
        if (srcW.load() != encW || srcH.load() != encH) {
            if (srcW.load())
                std::printf("[Agent] Source resized %ux%u -> %ux%u, rebuilding encoder.\n",
                            srcW.load(), srcH.load(), encW, encH);
            srcW.store(encW);
            srcH.store(encH);
            srcTexW.store(fi.width);
            srcTexH.store(fi.height);
            encoder.reset();
            cachedTex.Reset();
            haveCached.store(false, std::memory_order_release);
            sizeChanged.store(true, std::memory_order_release);
        }

        // Lưu bản sao frame cuối (texture của WGC chỉ sống trong callback).
        if (!cachedTex) {
            D3D11_TEXTURE2D_DESC d{};
            fi.texture->GetDesc(&d);
            d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            d.MiscFlags = 0;
            d.Usage = D3D11_USAGE_DEFAULT;
            d.CPUAccessFlags = 0;
            if (FAILED(gpu.device->CreateTexture2D(&d, nullptr, cachedTex.GetAddressOf())))
                cachedTex.Reset();
        }
        if (cachedTex) {
            gpu.context->CopyResource(cachedTex.Get(), fi.texture);
            haveCached.store(true, std::memory_order_release);
        }
        lastFrameUs.store(QpcUs(), std::memory_order_relaxed);

        if (!netReady.load(std::memory_order_acquire)) return;
        if (!ensureEncoder(encW, encH, fi.width, fi.height)) return;
        // Encode liên tục kể cả khi chưa có client (đơn giản, VBV ổn định);
        // NAL bị bỏ ở onPacket nếu chưa STREAMING.
        encoder->Encode(fi.texture, QpcUs(), forceIdr.exchange(false));
    };

    WindowCapture capture;
    if (!capture.Start(target, gpu.device.Get(), onFrame)) return 1;

    // Đợi frame đầu để biết kích thước nguồn (offer trong HELLO_ACK).
    for (int i = 0; i < 1000 && !srcW.load() && !capture.Closed() && !g_ctrlC.load(); ++i)
        Sleep(10);
    if (!srcW.load()) {
        std::printf("[Agent] Did not receive first frame from window — stopping.\n");
        capture.Stop();
        return 1;
    }

    rgc::StreamParams offer;
    offer.width      = uint16_t(srcW.load());
    offer.height     = uint16_t(srcH.load());
    offer.fps        = uint8_t(opt.fps);
    offer.bitrateBps = opt.bitrateMbps * 1'000'000u;
    std::printf("[Agent] Source %ux%u @%ufps, %u Mbps. Waiting for client...\n",
                offer.width, offer.height, opt.fps, opt.bitrateMbps);

    // GD4: bơm input từ client vào cửa sổ đang chia sẻ. Chỉ thread Recv chạm vào.
    InputInjector injector;
    if (opt.allowInput) {
        injector.SetEnabled(injector.Init(target));
        std::printf("[Agent] Client control allowed (mouse + keyboard).\n");
    } else {
        injector.SetEnabled(false);
        std::printf("[Agent] VIEW ONLY - input from client is ignored (--noinput).\n");
    }

    NetAddr replyAddr; // địa chỉ nguồn của gói đang xử lý (chỉ thread Recv dùng)
    rgc::HostCallbacks cb;
    cb.send = [&](std::span<const uint8_t> d) { sock.SendTo(replyAddr, d.data(), d.size()); };
    cb.onStart = [&] {
        forceIdr.store(true); // IDR mở màn (kèm SPS/PPS — repeatSPSPPS=1)
        std::printf("[Agent] Client START — beginning video push.\n");
    };
    cb.onKeyframeRequest = [&] { forceIdr.store(true); };
    // GD5 congestion control. Mất gói trên UDP gần như luôn là hàng đợi router đầy,
    // nên phản ứng đúng là GIẢM NHANH (nhân) rồi bò lên lại từ từ (cộng) — nới nhanh
    // sau khi vừa mất gói chỉ làm nghẽn trở lại theo chu kỳ.
    // Ngưỡng 2%/5%: dưới 2% H.264 tự che được bằng IDR sau loss, trên 5% là hình vỡ rõ.
    const uint32_t maxBitrate = opt.bitrateMbps * 1'000'000u;
    const uint32_t minBitrate = 1'000'000u; // dưới mức này hình nát, thà bỏ frame
    uint64_t lastDecreaseUs = 0;
    int cleanSeconds = 0; // số lần FEEDBACK liên tiếp không mất gói
    cb.onFeedback = [&, maxBitrate, minBitrate](const rgc::Feedback& fb) {
        const uint64_t now = QpcUs();

        // FEC tốn 1/kFecGroupSize băng thông nên chỉ bật khi đang thực sự mất gói.
        // Tắt phải chậm hơn bật (5 giây sạch): mất gói thường đến theo cụm, tắt ngay
        // sau cụm đầu là vừa kịp không có parity cho cụm sau.
        if (fb.lossPct >= 1) {
            cleanSeconds = 0;
            if (!wantFec.exchange(true, std::memory_order_relaxed))
                std::printf("[Agent] FEC on (loss %u%%).\n", fb.lossPct);
        } else if (++cleanSeconds >= 5) {
            if (wantFec.exchange(false, std::memory_order_relaxed))
                std::printf("[Agent] FEC off (link clean).\n");
        }
        const uint32_t cur = curBitrateBps.load(std::memory_order_relaxed);
        uint32_t next = cur;
        if (fb.lossPct >= 5) {
            next = cur - cur / 4;              // ×0.75
            lastDecreaseUs = now;
        } else if (fb.lossPct >= 2) {
            next = cur - cur / 10;             // ×0.90
            lastDecreaseUs = now;
        } else if (fb.lossPct <= 1 && now - lastDecreaseUs > 2'000'000) {
            next = cur + maxBitrate / 20;      // +5% trần mỗi giây
        }
        if (next > maxBitrate) next = maxBitrate;
        if (next < minBitrate) next = minBitrate;
        // Bỏ qua thay đổi vụn: mỗi lần gọi encoder là một lần đàm phán lại rate control.
        if (next == cur || (next > cur ? next - cur : cur - next) < cur / 50) return;

        std::lock_guard<std::mutex> lk(encMutex);
        if (encoder && encoder->SetBitrate(next)) {
            curBitrateBps.store(next, std::memory_order_relaxed);
            std::printf("[Agent] Bitrate %.1f -> %.1f Mbps (loss %u%%, RTT %u ms)\n",
                        cur / 1e6, next / 1e6, fb.lossPct, fb.rttMs);
        }
    };
    cb.onInput = [&](const rgc::InputEvent& e) { injector.Apply(e); };
    cb.onDisconnect = [&] {
        peerPacked.store(0, std::memory_order_release);
        injector.ReleaseAll(); // BẮT BUỘC: mất kết nối giữa lúc giữ phím = kẹt phím
        std::printf("[Agent] Client left (BYE/timeout) — waiting for a new client.\n");
    };
    session = std::make_unique<rgc::HostSession>(cb, offer);
    netReady.store(true, std::memory_order_release);

    // --- Vòng Recv (thread chính) ---
    uint8_t buf[rgc::kMaxDatagram];
    uint64_t lastStatUs = QpcUs();
    uint32_t lastCaptured = 0;
    uint64_t lastBytes = 0, lastFrames = 0;

    while (!g_ctrlC.load() && !failed.load() && !capture.Closed()) {
        NetAddr from;
        const int n = sock.RecvFrom(buf, sizeof(buf), from);
        const uint64_t now = QpcUs();
        if (n < 0) { std::printf("[Agent] Socket error — stopping.\n"); break; }
        if (n > 0) {
            replyAddr = from;
            if (session->HandlePacket(std::span<const uint8_t>(buf, size_t(n)), now)) {
                // Gói hợp lệ thuộc phiên — cập nhật peer (client roaming đổi IP/port).
                const uint64_t pk = from.Pack();
                if (peerPacked.load(std::memory_order_relaxed) != pk) {
                    peerPacked.store(pk, std::memory_order_release);
                    std::printf("[Agent] Peer: %s\n", from.ToString().c_str());
                }
            }
        }
        session->Tick(now);

        // Cửa sổ nguồn vừa đổi kích thước (thread FrameArrived đã dựng lại encoder).
        // Báo client kích thước mới + IDR: stream đổi SPS giữa chừng, không có IDR
        // thì decoder client chỉ có rác cho tới keyframe kế tiếp.
        if (sizeChanged.exchange(false, std::memory_order_acq_rel)) {
            rgc::StreamParams np = offer;
            np.width  = uint16_t(srcW.load());
            np.height = uint16_t(srcH.load());
            np.bitrateBps = curBitrateBps.load(std::memory_order_relaxed);
            offer = np;
            session->SetOffer(np); // HELLO phát lại sau này phải mang số mới
            const uint64_t pp = peerPacked.load(std::memory_order_acquire);
            if (pp && session->state() == rgc::HostSession::State::Streaming) {
                rgc::Reconfig rc{np.width, np.height, np.bitrateBps};
                uint8_t rbuf[rgc::kMaxDatagram];
                const size_t rn = rgc::BuildReconfig(rbuf, session->sessionId(), rc);
                if (rn) sock.SendTo(NetAddr::Unpack(pp), rbuf, rn);
                forceIdr.store(true);
            }
        }

        // Yêu cầu IDR đang treo mà nguồn đang TĨNH (>200ms không có FrameArrived —
        // WGC chỉ phát khi nội dung đổi) -> encode lại frame cache để client có hình.
        if (session->state() == rgc::HostSession::State::Streaming &&
            forceIdr.load() && haveCached.load(std::memory_order_acquire) &&
            now - lastFrameUs.load(std::memory_order_relaxed) > 200'000) {
            std::lock_guard<std::mutex> lk(encMutex);
            if (ensureEncoder(srcW.load(), srcH.load(), srcTexW.load(), srcTexH.load()) &&
                forceIdr.exchange(false))
                encoder->Encode(cachedTex.Get(), QpcUs(), true);
        }

        if (now - lastStatUs >= 1'000'000) {
            const double secs = (now - lastStatUs) / 1e6;
            const uint32_t cap = captured.load();
            const uint64_t by = bytesSent.load(), fr = framesSent.load();
            const auto& ist = session->inputStats();
            std::printf("[Agent] %-9s | capture %.0f fps | send %.0f fps, %.0f kbps"
                        " | input %llu (lost %llu)\n",
                        StateName(session->state()),
                        (cap - lastCaptured) / secs,
                        (fr - lastFrames) / secs,
                        (by - lastBytes) * 8.0 / 1000.0 / secs,
                        (unsigned long long)ist.applied,
                        (unsigned long long)ist.lost);
            lastStatUs = now;
            lastCaptured = cap;
            lastBytes = by;
            lastFrames = fr;
        }
    }

    injector.ReleaseAll(); // thoát giữa lúc client đang giữ phím -> nhả ra

    // Chia tay tử tế: báo BYE cho client nếu còn phiên.
    if (session->state() != rgc::HostSession::State::Idle) {
        const uint64_t pp = peerPacked.load();
        if (pp) {
            const NetAddr peer = NetAddr::Unpack(pp);
            uint8_t bye[rgc::kCommonHeaderSize];
            const size_t bn = rgc::BuildBye(bye, session->sessionId());
            if (bn) sock.SendTo(peer, bye, bn);
        }
    }

    capture.Stop(); // hết callback rồi mới dọn encoder
    {
        std::lock_guard<std::mutex> lk(encMutex);
        if (encoder) encoder->Finish();
    }
    netReady.store(false);
    std::printf("[Agent] Stopped. Total: %llu frames sent, %.2f MB.\n",
                (unsigned long long)framesSent.load(), bytesSent.load() / 1e6);
    SetConsoleCtrlHandler(CtrlHandler, FALSE);
    return failed.load() ? 1 : 0;
}
