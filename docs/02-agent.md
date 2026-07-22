# 02 — Thiết kế Agent (host)

Agent chạy trên máy có ứng dụng cần điều khiển (game, IDE, trình duyệt…) — vai **host**, chỉ
trên **desktop: Windows · macOS · Ubuntu** (mobile/web không host được,
`11-platform-transport.md` §3). Trách nhiệm: bắt hình, nén, gửi video, nhận input và bơm vào
ứng dụng đích. Đây là thành phần phức tạp nhất của hệ.

Vai trò và điều phối (§1, §4, §6, §7) **giống nhau mọi OS**; chỉ ba backend phần cứng —
**capture · encode · inject** — đổi theo OS (§1b). Phần dưới mô tả **bản tham chiếu Windows**
(đã hiện thực GĐ0–GĐ5); cột macOS/Ubuntu là thứ tương đương sẽ viết khi mở nền tảng, cùng
`core/` và cùng giao thức.

## 1. Các module

```
Agent
├── CaptureModule      per-OS backend  (Win ✅ client/windows/capture/WindowCapture)
├── EncoderModule      per-OS backend  (nén texture GPU → NAL)
├── TransportModule    core packetize + per-OS socket  (UDP; xem 11 §2)
├── InputInjector      per-OS backend  (nhận input decode → bơm vào game)
├── SessionManager     core/  (handshake, state, thương lượng tham số — chung mọi OS)
└── ControlLoop        per-OS glue  (điều phối Capture → encode → send; xử lý control msg)
```

Ba module `per-OS backend` (Capture/Encoder/InputInjector) là toàn bộ việc phải viết lại cho
một agent OS mới; `SessionManager` + phần packetize của Transport nằm trong `core/` dùng lại.

### 1b. Backend theo OS

| | Windows (tham chiếu ✅) | macOS | Ubuntu |
|--|-------------------------|-------|--------|
| Capture | WGC (`Direct3D11CaptureFramePool`) | ScreenCaptureKit | PipeWire (Wayland) / X11 |
| Encode | NVENC → AMF/QSV → MF | VideoToolbox | VAAPI / NVENC |
| Inject | `SendInput` (+ ViGEm/Interception) | CGEvent (Quartz Event Services) | uinput / XTest |
| Device/texture | D3D11 (VRAM) | Metal (IOSurface) | VA-API surface / DMA-BUF |

Chi tiết ba backend Windows ở §2 (capture), §3 (encode), §5 (inject) — đọc như **bản tham
chiếu**; mac/Ubuntu thay đúng cột tương ứng, giữ nguyên interface `IVideoEncoder` (§3) và
ranh giới "texture GPU → NAL Annex-B".

## 2. CaptureModule ✅ (xong ở GĐ0)

- Event `FrameArrived` của `Direct3D11CaptureFramePool` (không polling) — giảm độ trễ và tải CPU.
- Tạo D3D11 device, `GraphicsCaptureItem` từ HWND, tắt cursor/border; winrt giấu sau PIMPL.
- `CopyToCpu` + `WriteBmp` tách ra `BmpWriter`, chỉ chạy khi có cờ `--save` (ngoài đường nóng).
- D3D11 device chia sẻ với EncoderModule qua `Device()`/`Context()` — không cross-device copy.

Đầu ra của module: `ID3D11Texture2D*` (BGRA, VRAM) + timestamp (từ `frame.SystemRelativeTime()`).

## 3. EncoderModule — mắt xích rủi ro nhất

> **Trạng thái (GĐ1-2 ✅):** backend **NVENC** (`NvencEncoder.cpp`) là ưu tiên — nạp DLL động,
> zero-copy texture D3D11, preset P4 + ULTRA_LOW_LATENCY, CBR, GOP vô hạn + IDR theo yêu cầu,
> xuất NAL Annex-B qua callback `onPacket`. **Media Foundation** (`MfEncoder.cpp`) là fallback
> (ghi file `.mp4`, chưa xuất NAL). `EncoderFactory` thử NVENC → MF theo GPU từ `GpuSelect`.

### Trừu tượng hóa
Định nghĩa interface chung để đổi backend theo GPU:

```cpp
class IVideoEncoder {
public:
    virtual bool Init(ID3D11Device* dev, UINT w, UINT h, const EncoderConfig& cfg) = 0;
    // Nhận texture VRAM, trả về 0..n gói NAL đã nén qua callback.
    virtual bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs,
                        bool forceKeyframe) = 0;
    virtual void SetBitrate(uint32_t bps) = 0;
    virtual void RequestKeyframe() = 0;   // khi client báo mất gói
    virtual ~IVideoEncoder() = default;
};

struct EncoderConfig {
    Codec    codec = Codec::H264;   // H264 | HEVC | AV1
    uint32_t bitrateBps = 20'000'000;
    uint32_t fps = 60;
    uint32_t gopLength = 120;        // khoảng cách IDR; 0 = chỉ IDR khi được yêu cầu
    RateControl rc = RateControl::CBR; // low-latency ưa CBR
    bool     lowLatency = true;      // preset độ trễ thấp, tắt B-frame
};
```

### Backend theo GPU

| GPU | API | Zero-copy input | Ghi chú |
|-----|-----|-----------------|---------|
| NVIDIA | **NVENC** (Video Codec SDK) | `nvEncRegisterResource` với D3D11 texture | Ưu tiên. Preset `P1..P7`, tuning `LOW_LATENCY`/`ULTRA_LOW_LATENCY`. |
| AMD | **AMF** | Submit D3D11 surface | `AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY`. |
| Intel | **QSV / Media Foundation** | MF sink nhận D3D11 | Đơn giản nhất qua Media Foundation. |
| Fallback | **Media Foundation H.264** | qua IMFDXGIDeviceManager | Chậm hơn nhưng chạy được đa số GPU. |

### Cấu hình low-latency bắt buộc
- **Tắt B-frame** (B-frame cần frame tương lai → thêm độ trễ).
- **CBR hoặc low-delay VBR** để bitrate ổn định trên đường mạng cố định.
- **Infinite GOP + IDR theo yêu cầu**: không phát IDR định kỳ tốn bitrate; chỉ phát khi
  client mất gói và xin lại (giảm băng thông, nhưng cần kênh feedback — xem protocol).
- **Slicing**: chia frame thành nhiều slice để mất một gói chỉ hỏng một dải, không cả frame.

### Đầu ra
NAL units (Annex-B hoặc length-prefixed) + metadata: `frameType (IDR/P)`, `timestampUs`,
`frameId`. Chuyển sang TransportModule để packetize.

## 4. TransportModule (phía Agent)

Phần **packetize/depacketize + pacing + phiên** nằm trong `core/` (chung mọi OS); chỉ lớp
socket là per-OS (winsock/BSD), và phía host còn có binding **WebTransport/QUIC** để phục vụ
web client — cả hai bơm vào cùng `HostSession` qua `IHostTransport` (`11-platform-transport.md`
§2). Nội dung dưới đây mô tả hành vi chung, không phụ thuộc socket cụ thể.

- **Gửi video**: nhận NAL, cắt theo MTU (~1200 byte payload để an toàn qua Internet),
  gắn header (xem protocol §video), gửi UDP. Không chờ ACK.
- **Nhận input/control**: đọc gói UDP đến, tách theo kênh, đẩy tới InputInjector hoặc SessionManager.
- **Pacing**: rải gói của một frame đều trong khoảng frame để tránh burst gây mất gói.
- **Congestion feedback**: nhận báo cáo mất gói/RTT từ client → điều chỉnh bitrate encoder (§control).

## 5. InputInjector — phần khó ngầm

> **Bản tham chiếu Windows.** macOS bơm bằng CGEvent (Quartz), Ubuntu bằng uinput/XTest
> (§1b) — cùng bài toán (fallback dần, ánh xạ toạ độ, giành focus, nhả phím an toàn), khác
> API. Anti-cheat kernel chặn được cả ba; đây là giới hạn chung, không riêng OS nào.

### Chiến lược nhiều tầng (fallback dần)

1. **`SendInput` (mặc định)**: bơm chuột/phím cấp Windows. Chạy với đa số ứng dụng và game
   dùng Windows message. Chuột dùng tọa độ tuyệt đối chuẩn hóa hoặc chuyển động tương đối.
2. **Nếu game bỏ qua** (Raw Input/DirectInput đọc thẳng thiết bị): dùng
   - **Interception driver** (bàn phím/chuột cấp driver), hoặc
   - **ViGEmBus** cho **gamepad ảo** — nhiều game console-port nhận gamepad tốt hơn chuột phím giả lập.
3. **Anti-cheat**: một số game (kernel anti-cheat) chặn cả driver ảo. Ghi rõ đây là giới hạn,
   không cố vượt — chỉ hỗ trợ game cho phép.

### Ánh xạ tọa độ
Client gửi tọa độ theo **hệ chuẩn hóa 0..1 tương đối với vùng client của cửa sổ game**, kèm
kích thước frame lúc client render. Agent quy đổi về pixel trong cửa sổ game (cửa sổ có thể
đã resize — dùng `GetClientRect` như code hiện tại đang làm cho capture).

### Vấn đề focus
`SendInput` gửi tới cửa sổ đang focus. Nếu game không focus, input đi sai chỗ. Giải pháp:
`SetForegroundWindow`/`AttachThreadInput` để đưa game lên trước, hoặc dùng `PostMessage`
trực tiếp tới HWND (kém tin cậy với game). Ghi nhận là điểm cần kiểm thử.

## 6. SessionManager

- Xử lý **handshake** (§protocol): thương lượng codec, độ phân giải, fps, bitrate, khả năng GPU.
- Quản lý **vòng đời phiên**: chờ client → thiết lập → streaming → teardown.
- Một phiên = một client (giai đoạn đầu). Multi-client để sau.

## 7. ControlLoop (điều phối)

```
Khi FrameArrived:
    tex, ts = capture.GetFrame()
    if tex is null: return                     # pool đổi size, bỏ frame
    force = keyframeRequested.exchange(false)
    encoder.Encode(tex, ts, force)             # callback → transport.SendVideo(nal)

Luồng nhận (song song):
    khi có gói input:  injector.Inject(decode(pkt))
    khi có control msg: sessionManager.Handle(pkt)   # đổi bitrate, xin IDR, ping...
```

## 8. Threading
- **Capture callback** (WGC free-threaded pool): lấy frame, đẩy encode. Giữ nhẹ.
- **Encoder**: có thể async; đọc kết quả trên luồng riêng.
- **Network RX**: một luồng đọc UDP, phân loại kênh.
- Dùng hàng đợi không khóa hoặc khóa ngắn giữa các luồng; tránh chặn capture callback.

## 9. Đầu vào dòng lệnh
Hiện `main` không tham số sẽ hiện picker chọn cửa sổ; đã có các mode `--encode`, `--loopback`.
GĐ3 thêm vai trò host:
```
client.exe <game.exe> --serve [--port 47777] [--bitrate 20] [--fps 60]
```
