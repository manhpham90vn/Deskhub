# 11 — Nền tảng & Transport (ma trận client/host, chiến lược UDP/QUIC)

Những quyết định **cross-cutting** áp cho MỌI client/host, không riêng nền tảng nào — tách
khỏi các doc client (Windows, `08-android-client.md`, `10-web-client.md`) vì chúng bắt nguồn
từ một tính chất chung: **`core/` không biết transport**. Nhờ đó "ai làm host" và "dùng UDP
hay QUIC" là những lựa chọn độc lập với từng client, quyết một lần ở đây.

**Mục tiêu nền tảng của dự án:** agent (host) cho **Windows · macOS · Ubuntu**; client cho
**Windows · macOS · Ubuntu · iOS · Android · Web**. §3 là ma trận đầy đủ (ai làm được vai
gì, vì sao). Backend từng vai theo OS: agent ở `02-agent.md` §1b, client ở `03-client.md` §1b.

## 1. `core/` transport-agnostic — cái "seam"

`core/` chỉ biết một khái niệm: **"datagram = một mảng byte"**. Nó có đúng hai điểm chạm với
thế giới ngoài, cả hai đều là byte thuần, **không phải socket**:

- **Byte ra:** callback `send(std::span<const uint8_t>)` — core **giao** một datagram cho ai
  đó; core KHÔNG tự gửi. (`HostCallbacks.send` ghi rõ: "giao datagram cho tầng socket".)
- **Byte vào:** `HandlePacket(...)` — caller **bơm** datagram nhận được vào core.

Grep toàn `core/`: **0 chữ** `udp` / `quic` / `socket` / `sendto` / `webtransport`. Hệ quả:
**chỉ có MỘT `core/`, MỘT `ClientSession`, MỘT `Reassembler`** — dùng y hệt cho mọi
transport. "UDP hay QUIC" quyết **hoàn toàn bên ngoài core**, bằng việc nối hai callback vào
đối tượng transport nào. Thời gian cũng bơm từ ngoài (`nowUs`) nên core không thread, không
đồng hồ, test offline được (`core_tests`).

```
core (ClientSession / HostSession) — thuần C++20, KHÔNG socket
   send │ std::function<void(span<const uint8_t>)>   ← core đưa RA byte
        ▼
   UdpSocket (winsock/BSD): sendto()   HOẶC   WebTransportHost (msquic): QUIC DATAGRAM
        ▲
HandlePacket │  caller bơm byte nhận được VÀO core
```

UDP và QUIC là hai **người đưa thư** khác nhau chở cùng một phong bì; core chỉ viết và đọc
phong bì. Phần wire-level (định dạng gói, trần payload theo transport): `04-protocol.md` §11.

## 2. `IHostTransport` — binding trừu tượng ở host

Ranh giới sạch: một interface **`IHostTransport`** mà các transport hiện thực, tất cả bơm
byte vào một `HostSession`:

- **`UdpHostTransport`** — raw UDP, mỏng, **per-OS** (winsock trên Windows / BSD trên POSIX).
  Phải hai bản vì API raw socket mỗi OS một khác.
- **`WebTransportHost`** — QUIC/HTTP3, **viết một lần, dùng chung mọi OS**. Một thư viện QUIC
  đã bọc khác biệt socket bên trong nên KHÔNG viết lại theo OS như `UdpSocket`.

| Thư viện QUIC | Ưu | Nhược |
|----------|-----|-------|
| **msquic** (Microsoft, MIT) ✅ | **Native cả Windows + Linux + macOS**, C API gọn, cùng CMake/MSVC sẵn có, có lớp HTTP/3 | WebTransport có thể phải tự ghép trên lớp HTTP/3 của nó |
| quiche (Cloudflare, Rust + C API) | QUIC + HTTP/3 chín, đa nền tảng | Kéo toolchain Rust vào build C++/CMake |
| quic-go / các bản Go | Ví dụ WebTransport nhiều nhất | Ngôn ngữ khác, không hợp một-exe |

**Đề xuất: msquic** — chính vì native cả ba OS, `WebTransportHost` viết một lần dùng lại
được khi làm host macOS/Linux, đồng nhất toolchain. Chốt cuối để lại tới web-M2
(`10-web-client.md` §10) — cần xác nhận mức hỗ trợ WebTransport-over-HTTP/3 của msquic.

**Layout:** `WebTransportHost` cần thread/socket/msquic nên KHÔNG vào được `core/` (phá tính
thuần, mất `core_tests` offline), nhưng dùng chung mọi host OS nên KHÔNG nhân bản trong từng
`client/<os>`. Đặt ở một **module host dùng chung** (thư mục `host/` mới, hay mở rộng
`platform/`) mà mỗi `client/<os>` link vào — giữ ràng buộc một-exe-mỗi-OS. Thư mục cụ thể
chốt khi bắt tay code; nguyên tắc bất biến là "một lần, dùng chung".

## 3. Ma trận nền tảng: ai làm client, ai làm host

Vai **client** thì mọi nền tảng đều làm được (chỉ cần nhận + decode + gửi input). Vai
**host** thì KHÔNG — nó đòi bốn thứ, và mobile/web hỏng ở đúng hai thứ then chốt:

| Yêu cầu host | Win/mac/Linux | Android | iOS | Web |
|--------------|---------------|---------|-----|-----|
| Capture màn hình | ✅ | ✅ MediaProjection | 🔶 ReplayKit | ✅ getDisplayMedia |
| Encode HW | ✅ | ✅ MediaCodec | ✅ VideoToolbox | ✅ WebCodecs |
| **Inject input vào app khác** | ✅ | ❌ cần root | ❌ không thể | ❌ không thể |
| **Nghe kết nối vào (listen)** | ✅ | 🔶 vướng NAT/nền | 🔶 nền bị suspend | ❌ browser không listen |

- **Inject input** vào *ứng dụng khác* là đặc quyền chỉ desktop có. Android phải root; iOS
  và trang web bị sandbox tuyệt đối. Mà điều khiển từ xa chính là mục đích app — host không
  inject được thì vô nghĩa.
- **Listen**: trình duyệt **chỉ làm client, không mở được listen socket** → không thể là
  host mà kẻ khác kết nối tới. Mobile listen được nhưng thường sau CGNAT và bị OS treo nền.

Cộng điểm khái niệm: host là "máy đang chạy game cần điều khiển" — đó là PC.

**Kết luận:** host = **desktop** (Windows nay; macOS/Linux sau). iOS/Android/web là nền tảng
**client-only**. (Nếu sau này thêm mục tiêu "chỉ chia sẻ màn hình để xem, không điều khiển"
thì Android/iOS có thể làm host view-only — nhưng **web vẫn không**, vì không listen được —
và đó là một tính năng khác, không phải hướng hiện tại.)

## 4. Host trên nhiều OS (macOS / Ubuntu / …)

Thêm một host OS mới cần đúng ba mảnh **OS-specific mà một host UDP native trên OS đó đằng
nào cũng phải viết**:

| Mảng host | Windows (đang có) | macOS | Linux |
|-----------|-------------------|-------|-------|
| Capture | WGC | ScreenCaptureKit | PipeWire / X11 |
| Encode | NVENC / MF | VideoToolbox | VAAPI / NVENC |
| Inject input | `SendInput` | CGEvent | uinput / XTest |
| **Transport** | **`WebTransportHost` (dùng chung)** ← | ← **cùng một mã** | ← **cùng một mã** |
| Lõi giao thức | `core/` (dùng chung) | ← | ← |

Nói cách khác: **thêm một host OS mới = viết capture + encode + inject cho OS đó**; transport
(cả UDP lẫn WebTransport) và toàn bộ `core/` dùng lại.

## 5. Chiến lược transport: hybrid UDP + QUIC

**Quyết định (2026-07-22, "hướng A"): HYBRID.** UDP cho **native** (Windows/Android, và
macOS/iOS/Linux sau), QUIC/WebTransport **chỉ** cho **web** — nền tảng duy nhất không mở được
raw UDP. **Không** thống nhất mọi client về QUIC. Cả hai binding cùng bơm vào một
`HostSession` qua `IHostTransport` (§2), core không đổi.

Vì `core/` transport-agnostic, "UDP hay QUIC" là lựa chọn **hoãn được**, không phải chốt bây
giờ. So sánh:

| | Được | Mất |
|--|------|-----|
| **Thống nhất QUIC** | Mã hóa TLS 1.3 mặc định (xóa mục Mã hóa GĐ6); host một listener; connection migration + hợp Internet sẵn có | Kéo msquic vào **mọi** client native (APK/exe phình); lôi rắc rối chứng chỉ vào cả LAN native↔native (nay cắm-là-chạy); **vứt đường UDP đã test M1/M2**; overhead handshake + AEAD |
| **Hybrid (chọn)** ✅ | Giữ đường native nhẹ, không cert, đã kiểm chứng; web có QUIC nó cần | Host chạy hai listener; hai binding phải bảo trì |

Trên **LAN** — bối cảnh chính hiện tại — raw UDP "cắm là chạy", không cert, không handshake;
native chưa được lợi đủ từ QUIC để bù chi phí phụ thuộc + churn. Web thì đằng nào cũng phải
QUIC, nên hybrid không phát sinh việc thừa: mỗi bên dùng đúng transport rẻ nhất cho nó.

**Bản đồ transport theo client:**

| Client | Transport | Socket |
|--------|-----------|--------|
| Windows | UDP | winsock (`SIO_UDP_CONNRESET`…) |
| Android | UDP | BSD/POSIX — đã có |
| macOS / iOS / Linux (sau) | UDP | BSD/POSIX — **dùng lại `UdpSocket` của Android** |
| Web | **QUIC/WebTransport** | — trình duyệt không mở raw UDP |

Native mac/iOS/Linux đều POSIX nên **không phát sinh transport mới** — chỉ Windows khác
(winsock). Việc platform thật sự phải viết cho một client OS mới là **decode + render +
input**, không phải socket. Lưu ý iOS **làm client UDP bình thường** — rào cản host ở §3
(listen, inject input) không áp vào vai client.

**Khi nào nên xét lại:** phần thưởng lớn nhất của QUIC-everywhere là **mã hóa mặc định** —
nên thời điểm tự nhiên để thống nhất là lúc bắt tay mục mã hóa GĐ6, hoặc khi cần chạy thật
qua Internet (không chỉ LAN). Lúc đó di cư native UDP→QUIC **từng client một, không sửa
core** — chỉ đổi đối tượng nối vào `send`/`HandlePacket`.
