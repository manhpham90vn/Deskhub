# 01 — Kiến trúc tổng thể (đa nền tảng)

## 1. Mô hình hệ thống

Hai **vai trò**, giao tiếp qua mạng IP. Vai độc lập với OS — mỗi OS chỉ thay các *backend*
phần cứng bên dưới:

- **Agent** (host): chạy trên máy đang chạy game. Bắt hình, nén, gửi video; nhận input và
  bơm vào game. **Chỉ desktop**: Windows · macOS · Ubuntu.
- **Client** (controller): chạy trên máy người dùng. Nhận video, hiển thị; bắt chuột/phím
  và gửi đi. **Mọi nền tảng**: Windows · macOS · Ubuntu · iOS · Android · Web.

Trên **desktop, một app chứa cả hai vai** (kiểu AnyDesk); trên **mobile/web chỉ vai client**
(không host được — `11-platform-transport.md` §3).

```
┌─────────────────────────── AGENT (desktop: Win/mac/Ubuntu) ─────────────────────────┐
│                                                                                      │
│  Game window ──► Capture backend ──► [Texture trong VRAM] ──► HW Encoder             │
│                  (WGC/SCK/PipeWire)                            (NVENC/VideoToolbox/…) │
│                                                                     │                │
│                                                              H.264/HEVC NAL          │
│                                                                     │                │
│  Input Injector ◄── Input decode ◄── Transport RX          Transport TX              │
│  (SendInput/CGEvent/uinput)          (UDP)                  (UDP, packetizer)         │
│                                          ▲                       │                    │
└──────────────────────────────────────────┼─────────────────────┼─────────────────────┘
                                            │  input packets      │  video packets
                                            │                     ▼
┌─────────────────────── CLIENT (Win/mac/Ubuntu/iOS/Android/Web) ─────────────────────┐
│                                            ▲                     │                    │
│  Input Capture ──► Input encode ──► Transport TX          Transport RX               │
│  (RawInput/Pointer Lock/…)          (UDP · QUIC ở web)     (UDP · QUIC, de-packetize) │
│                                                                 │                    │
│                                                          H.264/HEVC NAL              │
│                                                                 │                    │
│  Display  ◄────────────  Renderer  ◄──────────────────  HW Decoder                   │
│  (swapchain/canvas)      (D3D11/Metal/WebGL)            (D3D11VA/VT/MediaCodec/WebCodecs)│
└──────────────────────────────────────────────────────────────────────────────────────┘
```

**Backend theo OS** (vai trò không đổi, chỉ đổi cột dưới):

| | Windows (tham chiếu) | macOS | Ubuntu | Android | iOS | Web |
|--|----------------------|-------|--------|---------|-----|-----|
| **Agent** capture | WGC | ScreenCaptureKit | PipeWire/X11 | — | — | — |
| **Agent** encode | NVENC/MF | VideoToolbox | VAAPI/NVENC | — | — | — |
| **Agent** inject | SendInput | CGEvent | uinput/XTest | — | — | — |
| **Client** decode | MF (D3D11VA) | VideoToolbox | VAAPI | MediaCodec | VideoToolbox | WebCodecs |
| **Client** render | D3D11 | Metal | OpenGL/Vulkan | Surface | Metal | canvas/WebGL |
| **Client** input | Raw Input | — | — | touch | touch | Pointer Lock |
| **Transport** | UDP (winsock) | UDP (BSD) | UDP (BSD) | UDP (BSD) | UDP (BSD) | **QUIC** |
| **Lõi** | `core/` (C++20) — **dùng chung tất cả**, biên dịch native hoặc WASM | ← | ← | ← | ← | ← (WASM) |

Chi tiết vai: `02-agent.md`, `03-client.md`. Chi tiết nền tảng & transport:
`11-platform-transport.md`.

## 2. Pipeline dữ liệu chi tiết (đường video)

| Bước | Vị trí | Dữ liệu vào | Dữ liệu ra | Ghi chú |
|------|--------|-------------|------------|---------|
| 1. Capture | Agent | Game window | Texture GPU (VRAM) | Event-driven. Backend theo OS (§1). |
| 2. Encode | Agent | Texture VRAM | NAL units nén | Zero-copy: encoder nhận thẳng texture GPU. |
| 3. Packetize | Agent | NAL units | Datagram ≤ MTU | `core/` — cắt NAL lớn, đánh số. Chung mọi OS. |
| 4. Transmit | Agent→Client | Datagram | — | UDP (native) / QUIC (web); không ACK cho video. |
| 5. Reassemble | Client | Datagram | NAL units | `core/` — ghép; bỏ frame thiếu gói (nếu không FEC). |
| 6. Decode | Client | NAL units | Frame GPU | Hardware decode. Backend theo nền tảng (§1). |
| 7. Render | Client | Frame GPU | Khung hình màn hình | Present qua swapchain/canvas. |

Bước 3–5 (`core/`) **giống hệt trên mọi nền tảng**; chỉ bước 1–2 (agent) và 6–7 (client)
đổi backend theo OS. Transport (bước 4) là UDP mọi nơi, trừ web dùng QUIC.

## 3. Pipeline dữ liệu (đường input — chiều ngược)

| Bước | Vị trí | Ghi chú |
|------|--------|---------|
| 1. Capture input | Client | Chuột/phím tương đối. Backend: Raw Input (Win) · Pointer Lock (Web) · touch (mobile). |
| 2. Encode | Client | `core/` — message nhỏ, cố định; timestamp + sequence. |
| 3. Transmit | Client→Agent | Kênh riêng; cần tin cậy nhẹ (gửi lặp key events). |
| 4. Inject | Agent | Backend: SendInput (Win) · CGEvent (mac) · uinput (Ubuntu). |

## 4. Mục tiêu hiệu năng

| Chỉ số | Mục tiêu LAN | Mục tiêu Internet |
|--------|--------------|-------------------|
| Độ trễ glass-to-glass | < 30–50 ms | < 80–120 ms |
| FPS | 60 (tùy chọn 120) | 30–60 |
| Bitrate 1080p60 | 15–30 Mbps | 8–20 Mbps |
| Frame drop | < 0.1% | < 1% (có FEC) |

**Ngân sách độ trễ (LAN, 60fps, mục tiêu ~40ms):**

```
Capture   ~2 ms  │ Encode   ~5–8 ms │ Network ~1–5 ms
Decode    ~5 ms  │ Render+present ~2ms + tối đa 1 frame chờ vsync (~16ms)
```

Kết luận: encode và vsync là hai khoản lớn nhất. Ưu tiên encoder low-latency preset
và cân nhắc tắt vsync ở client khi cần độ trễ tối thiểu.

## 5. Ràng buộc & giả định

- **Agent = desktop**: Windows 10 1903+ (WGC), macOS, Ubuntu — mỗi OS cần HW encoder
  (NVENC/AMF/QSV · VideoToolbox · VAAPI) và quyền inject input (SendInput · CGEvent ·
  uinput). Mobile/web **không** làm agent được (`11-platform-transport.md` §3).
- **Client = mọi nền tảng** (6 cái). Yêu cầu tối thiểu: HW decoder + màn hình + bắt input.
- **Một app mỗi desktop OS** chứa cả hai vai (kiểu AnyDesk); mobile/web là client-only.
  Phần dùng chung nằm ở `core/` (thuần C++20, biên dịch native hoặc WASM cho web).
- **Mạng**: UDP thông giữa hai máy (native); web dùng WebTransport/QUIC. NAT traversal để
  giai đoạn sau. Transport là **hybrid** — `11-platform-transport.md` §5.
- **Bảo mật**: giai đoạn đầu chạy LAN tin cậy; mã hóa (DTLS/AEAD, hoặc TLS 1.3 khi thống
  nhất QUIC) thêm sau — GĐ6.

## 6. Vì sao chọn các quyết định này

### Lõi `core/` chung, backend theo OS
Vai trò (capture→encode→gửi / nhận→decode→render) giống nhau trên mọi OS; chỉ *cách* làm
mỗi bước là khác. Tách phần bất biến (giao thức, packetize, phiên, input ordering, FEC) vào
`core/` thuần C++20 không header OS → viết một lần, test offline (`core_tests`), biên dịch
native cho desktop/mobile và **WASM cho web**. Mỗi nền tảng mới chỉ viết lớp mỏng backend,
không đụng logic giao thức. Đây là điều kiện để phủ 3 agent + 6 client mà không nhân bản lõi.

### Zero-copy VRAM → Encoder
Frame 1080p BGRA thô = ~8 MB. Kéo về CPU rồi đẩy lại GPU cho encoder tốn băng thông và độ
trễ. Encoder phần cứng nhận thẳng texture GPU (mọi OS đều có đường này). Copy về CPU chỉ
để debug, **không** nằm trong đường streaming.

### UDP thay vì TCP (native), QUIC cho web
TCP đảm bảo thứ tự bằng cách chặn (head-of-line blocking): một gói mất làm nghẽn mọi gói
sau. Video realtime thà bỏ frame cũ và tiến tới. UDP cho phép "bỏ qua và tiến". Web không
mở được raw UDP → dùng **QUIC datagram** (cùng mô hình không tin cậy), không phải WebSocket
(TCP) — `11-platform-transport.md` §1, `10-web-client.md` §1.

### Tách kênh video / input
Video: dữ liệu lớn, mất gói lẻ chấp nhận được. Input: dữ liệu nhỏ, mất một sự kiện phím là
lỗi cảm nhận được (nhân vật chạy mãi vì mất event key-up). Hai đặc tính trái ngược → hai
kênh với chính sách tin cậy khác nhau.

## 7. Rủi ro kiến trúc (xếp theo mức độ)

1. **Input injection bị game bỏ qua** (cao): nhiều game dùng Raw/DirectInput hoặc có
   anti-cheat, không nhận input tổng hợp. Phải thử sớm với game thật, từng OS. Dự phòng:
   ViGEm (Win, gamepad ảo), Interception; tương đương mac/Linux ở tầng driver.
2. **Encoder không zero-copy được** (trung bình): tùy GPU/driver/OS. Dự phòng: copy
   VRAM→VRAM sang định dạng encoder chấp nhận, vẫn nhanh hơn qua CPU.
3. **Độ trễ mạng biến động (jitter)** (trung bình): cần jitter buffer nhỏ ở client, đánh
   đổi độ trễ lấy mượt.
4. **Mất gói làm hỏng frame kéo dài** (trung bình): decode lỗi lan tới frame sau vì
   inter-frame. Dự phòng: yêu cầu IDR khi phát hiện mất, hoặc FEC.
5. **Chi phí nhân nền tảng** (trung bình): 3 agent + 6 client là nhiều backend. Giảm bằng
   `core/` chung + `platform/` mỏng + tái dùng POSIX (mac/iOS/Linux dùng lại socket BSD
   của Android). Ưu tiên hoàn thiện bản Windows làm tham chiếu trước khi nhân.
