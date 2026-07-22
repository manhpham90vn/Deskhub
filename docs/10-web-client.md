# 10 — Client Web (WebTransport + WebCodecs)

Client chạy thẳng trong trình duyệt, **chỉ xem + gửi input** (giống Android v1 — chưa
làm vai trò host). Không cài đặt, mở một URL là dùng được.

Ba trụ cột, mỗi cái thay đúng một mảnh platform-specific của client Windows:

| Mảng | Windows | Web |
|------|---------|-----|
| Transport | `UdpSocket` (raw UDP) | **WebTransport** (QUIC datagram) |
| Giải mã | `MfDecoder` + `Renderer` (D3D11) | **WebCodecs `VideoDecoder`** + canvas/WebGL |
| Lõi giao thức | `core/` (C++20) | **`core/` biên dịch WASM** (Emscripten) — dùng lại y nguyên |

## 1. Vì sao WebTransport, không phải WebSocket / WebRTC

Trình duyệt **không mở được raw UDP socket** — đây là rào cản gốc khiến giao thức UDP
hiện tại không nói chuyện trực tiếp được với web (xem `04-protocol.md`). Ba lối ra:

| Phương án | Transport | Việc phải làm | Độ trễ | Tái dùng `core/` |
|-----------|-----------|---------------|--------|------------------|
| WebSocket | TCP | Host thêm WS server, gửi nguyên frame, bỏ packetize/FEC (TCP đã tin cậy) | Tốt LAN, kém khi mất gói (HOL blocking) | Một phần (bỏ transport) |
| **WebTransport** ✅ | **QUIC datagram (UDP)** | Host thêm QUIC/HTTP3 server + chứng chỉ | Xuất sắc, gần bằng UDP native | **Gần trọn vẹn** |
| WebRTC | RTP/SRTP | Remux H264→RTP, ICE/STUN/DTLS, SDP; vứt phần lớn control tự chế | Xuất sắc | Rất ít |

**Chọn WebTransport** vì **datagram không tin cậy của QUIC ánh xạ 1-1 với mô hình
UDP datagram hiện tại**: `Packetizer` / `Reassembler` / FEC XOR / máy trạng thái phiên
giữ nguyên byte. Đây là điểm khác biệt quyết định so với WebRTC — WebRTC buộc đóng gói
lại H.264 thành RTP và tự chạy jitter buffer/NACK riêng, ta mất gần hết giao thức đã
thiết kế. WebSocket thì đơn giản hơn để dựng nhưng TCP head-of-line blocking đúng thứ
mục 6 của `01-architecture.md` đã bác bỏ khi chọn UDP.

**Tương thích trình duyệt (07/2026): WebTransport đã Baseline** — Chrome 97+, Edge 98+,
Firefox 114+, **Safari 26.4+**, Opera, Samsung Internet. Lúc khảo sát ban đầu Safari còn
thiếu WebTransport (rủi ro lớn nhất của phương án này); nay đã có sẵn nên rủi ro đó biến
mất. WebCodecs cũng đã phổ cập trên cùng bộ trình duyệt này.

## 2. Phân chia JS / WASM — tái dùng `core/`

`core/` đã là C++20 không đụng header hệ điều hành (điều kiện `core/CMakeLists.txt` đặt
ra để build được bằng NDK) — nên **biên dịch sang WASM bằng Emscripten và dùng lại
`Wire` / `Reassembler` / `ClientSession` / FEC y hệt Android**. Đối ứng 1-1:

| Windows | Web | Vai trò |
|---------|-----|---------|
| `UdpSocket.cpp` (winsock) | `wt-transport.js` (WebTransport) → cầu WASM | datagram vào/ra |
| `MfDecoder` + `Renderer` | `video-decoder.js` (WebCodecs) + `<canvas>` | H.264 → màn hình |
| `InputCapture` (Raw Input) | `input.js` (Pointer Lock + KeyboardEvent) | bắt chuột/phím |
| `MainMenuWindow` | `index.html` (ô nhập địa chỉ) | nhập host + kết nối |
| cửa sổ preview | `<canvas>` + overlay HTML | hiển thị + số liệu |

Ranh giới cố ý mỏng, gói trong **một lớp cầu Embind/`EM_JS`** (đối ứng `JniBridge.cpp`
bên Android): WASM giữ toàn bộ logic giao thức; JS chỉ lo ba việc trình duyệt độc quyền
làm được — mở WebTransport, gọi WebCodecs, bắt input. **Không một frame video nào phải
đi qua JS heap để copy**: datagram đến từ WebTransport được ghi thẳng vào bộ nhớ WASM,
`Reassembler` ghép trong WASM, NAL hoàn chỉnh trả về dưới dạng view rồi đưa vào
`VideoDecoder.decode()` — chỉ một tham chiếu vùng nhớ, không sao chép nội dung.

> Có thể viết lại `Reassembler`/`Wire` bằng JS/TS thuần cho nhẹ (không cần WASM). Bác bỏ:
> đó là chính đường nóng dễ sai nhất (ghép mảnh, khử trùng, khôi phục FEC), viết lại là mở
> ra một bản thứ ba phải giữ đồng bộ với Windows + Android. WASM tái dùng đúng code đã có
> test `core_tests` bảo chứng.

## 3. Ràng buộc transport: tất cả đi bằng datagram (v1)

WebTransport cho cả **datagram (không tin cậy)** lẫn **stream (tin cậy, QUIC tự
retransmit)**. Cám dỗ: đẩy control (HELLO/START/REQUEST_KEYFRAME/RECONFIG) và input sang
stream tin cậy để **bỏ hẳn** logic retry-500ms và gửi-lặp-chống-kẹt-phím — QUIC lo tin cậy
giùm.

**Quyết định v1: gửi MỌI THỨ bằng datagram**, kể cả control và input. Lý do: đó chính là
cái giữ cho `core/` không đổi một dòng — `HostSession`/`ClientSession`/`InputSender`/
`InputReceiver` vốn xây trên giả định "mọi thứ là datagram không tin cậy, tự lo retry".
Trộn stream vào là phải rẽ nhánh máy trạng thái theo transport. Đây là **tối ưu để dành**:
khi web client chạy ổn, chuyển control+input sang stream tin cậy và lược bớt logic gửi-lặp
là một cải tiến độc lập, không phải điều kiện để có bản chạy được.

**Kích thước datagram — thay đổi bắt buộc ở core.** QUIC datagram phải nằm gọn trong một
gói QUIC; phần payload dùng được **nhỏ hơn 1200** vì QUIC nuốt mất header gói + tag AEAD
16 byte. Trình duyệt báo trị thật qua `transport.datagrams.maxDatagramSize` (thường
~1180–1200 tùy đường). Hiện `kMaxVideoPayload` là **hằng số biên dịch 1174** (xem
`04-protocol.md` §5). Phải **biến nó thành tham số runtime** đặt lúc handshake theo
`maxDatagramSize` — packetizer cắt NAL theo trần này, reassembler không cần biết. Client
UDP native truyền trần cũ; client web truyền trần QUIC báo về. Đây là **thay đổi core duy
nhất** mà phương án này đòi hỏi.

> QUIC datagram **có** chịu điều tiết tắc nghẽn (RFC 9221: tính vào cửa sổ tắc nghẽn,
> nghẽn thì **rớt** chứ không xếp hàng, và **không** retransmit) — nên `BitrateController`
> + kênh FEEDBACK vẫn nguyên giá trị, không bị QUIC làm thừa.

## 4. Giải mã video: WebCodecs

`VideoDecoder` nhận thẳng NAL H.264 và giải mã bằng phần cứng, xuất `VideoFrame` vẽ lên
`<canvas>` (WebGL hoặc `drawImage`). Thay cho cả `MfDecoder` **lẫn** `Renderer` — WebCodecs
xuất frame đã ở không gian màu hiển thị được, không cần bước NV12→BGRA thủ công.

- **Cấu hình:** `codec: 'avc1.<profile><level>'` (vd. `avc1.640028` cho High). Stream hiện
  là **Annex-B** (start code `00 00 00 01`), khai `avc: { format: 'annexb' }` để khỏi remux
  sang AVCC.
- **SPS/PPS:** NVENC bật `repeatSPSPPS`, mỗi IDR mang sẵn tham số in-band — WebCodecs
  annex-b nuốt được thẳng, không cần tách `description` riêng như MSE.
- **Keyframe:** `EncodedVideoChunk({ type: 'key' | 'delta' })` — cờ IDR ở header chung
  (`04-protocol.md` §5) map thẳng sang `type`. Sau mất gói mà `Reassembler` bỏ frame →
  `ClientSession` xin REQUEST_KEYFRAME như cũ.
- **RECONFIG:** khác `MfDecoder` (tự đàm phán qua `MF_E_TRANSFORM_STREAM_CHANGE`), giống
  MediaCodec Android — `onReconfig` phải gọi lại `decoder.configure()` với kích thước mới.
  Host gửi kèm IDR nên không mất gì.
- **Độ trễ:** đặt `optimizeForLatency: true` để decoder không gom nhiều frame trước khi
  xuất — đúng tinh thần `MF_LOW_LATENCY` bên Windows.

## 5. Input: Pointer Lock + scancode

Bắt input trong trình duyệt là mảng khớp tốt bất ngờ với thiết kế INPUT_EVENT
(`04-protocol.md` §6):

- **Chuột tương đối (game FPS):** **Pointer Lock API** khóa + ẩn con trỏ, `mousemove` trả
  `movementX/movementY` — đúng `dx/dy` với cờ `absolute=0`. Đây là bản web của chế độ F9.
  Chuột tuyệt đối (mặc định): lấy toạ độ trong canvas chuẩn hoá ×65535, `absolute=1`.
- **Bàn phím — scancode là bắt buộc.** Game đọc DirectInput/Raw Input theo **scancode**,
  không phải vkCode (`07-phase4-input.md` §5). Trình duyệt cho `KeyboardEvent.code`
  (physical key, vd. `"KeyW"`, `"ArrowUp"`) — độc lập layout, ánh xạ được sang scancode
  Windows (bao gồm bit cờ E0 cho phím mở rộng, khớp trường `b`). **`event.key` thì không
  dùng được** (đã qua layout). Cần một **bảng tra `code` → scancode PS/2** đặt ở JS.
- **Chống kẹt phím:** giữ nguyên ba lớp của core (redundancy trong gói + phát lại khi rảnh
  + `ReleaseAll` ở host). Thêm một lưới an toàn riêng của web: bắt sự kiện `blur` /
  `visibilitychange` (chuyển tab) → phát key-up cho mọi phím đang giữ, vì trình duyệt
  **không** gửi `keyup` khi cửa sổ mất focus.
- **Vướng của trình duyệt:** vài tổ hợp bị OS/trình duyệt nuốt trước (Cmd/Win, một số
  F-key, Esc thoát Pointer Lock). Ghi vào phần hạn chế; không vượt được từ trang web thường.

## 6. Chứng chỉ TLS + thiết lập kết nối (phần khó nhất)

Trình duyệt đòi WebTransport chạy trên **secure context** với chứng chỉ hợp lệ. App này
là ngang hàng trong LAN, không tên miền, không CA công cộng — nên dùng nhánh
**`serverCertificateHashes`** của WebTransport, cho phép chứng chỉ tự ký với ràng buộc
chặt (chính là cơ chế libp2p/thiết bị-LAN dùng):

- Chứng chỉ **X.509v3**, khóa **ECDSA secp256r1 (NIST P-256)** — **không** RSA.
- Hạn hiệu lực **< 14 ngày** (chống dùng hash để theo dõi dài hạn).
- Hash **SHA-256** (thuật toán duy nhất spec liệt kê hiện nay).

Luồng v1:

1. Host sinh **chứng chỉ ECDSA P-256 tạm thời** (hạn ~13 ngày), tự ký, tính SHA-256 của
   nó. Xoay vòng trước khi hết hạn.
2. Host **in / phục vụ** cặp `ip:port` + hash — cùng chỗ người dùng lấy địa chỉ để kết nối
   (đối ứng "ID kiểu AnyDesk"). Đóng gói gọn thành **một chuỗi kết nối** hoặc **QR** để chép
   một lần.
3. Trang web mở:
   ```js
   new WebTransport(`https://${ip}:${port}/deskhub`, {
     serverCertificateHashes: [{ algorithm: 'sha-256', value: <ArrayBuffer 32B> }]
   })
   ```
   QUIC xác minh chứng chỉ server khớp đúng hash đã ghim — MITM tráo chứng chỉ khác sẽ bị
   từ chối bắt tay.

**Phục vụ chính trang web ở đâu** (secure-context): đơn giản và chắc nhất là **host tự
phục vụ bundle web tĩnh qua chính HTTP/3 server đó**. Trình duyệt vào `https://<ip>:<port>/`,
trang và endpoint WebTransport **cùng origin, cùng chứng chỉ** — một server, một cổng, một
chứng chỉ. Phương án khác (bundle đặt trên CDN/artifact ngoài, chỉ mở WebTransport về host)
cũng chạy nhưng người dùng vẫn phải nạp hash bằng tay.

**Hạn chế v1 (ghi rõ):** nếu hash được lấy qua kênh không xác thực (vd. GET plaintext trong
LAN), kẻ chen giữa có thể tráo cả hash lẫn chứng chỉ. Chấp nhận được cho **LAN tin cậy**
như các giai đoạn trước; lời giải thật là truyền hash out-of-band (QR/chép tay) hoặc CA nội
bộ — để chung nhóm với "Mã hóa (DTLS/AEAD)" ở `05-roadmap.md` GĐ6.

## 7. Phía host: WebTransport server

Để phục vụ web client, host thêm một **WebTransport server** (QUIC + HTTP/3, đàm phán qua
HTTP/3 CONNECT) bên cạnh listener UDP hiện có. Nó nhận datagram từ WebTransport rồi **bơm
cùng chuỗi byte vào `HostSession`** như UDP — máy trạng thái phiên không phân biệt transport.

Đây là một binding **`IHostTransport` dùng chung mọi host OS** (viết một lần trên msquic,
không per-OS như `UdpSocket`). Thiết kế đầy đủ — interface, so sánh thư viện QUIC, layout
module, và **vì sao chỉ web dùng QUIC còn native giữ UDP (hybrid)** — ở
**`11-platform-transport.md`** (§2 và §5). Chốt thư viện QUIC ở web-M2 (§10).

Điểm **web-specific** của server này (khác một QUIC server thường): nó vừa **phục vụ bundle
web tĩnh** vừa mở **endpoint WebTransport**, dùng **cùng một chứng chỉ**, để trang web và
kết nối WebTransport cùng origin — thỏa secure-context mà không cần cert công cộng (§6).

## 8. Thay đổi ở `core/`

Gần như không đụng — đó là toàn bộ lý do chọn WebTransport. Đúng một thay đổi bắt buộc:

- **`kMaxVideoPayload` từ hằng biên dịch → tham số runtime.** Đặt lúc handshake theo
  `maxDatagramSize` client báo (§3). Packetizer nhận trần này qua tham số; reassembler
  không đổi. Client UDP native truyền trần cũ (1174) nên hành vi Windows/Android không đổi.

Mọi thứ khác (`Wire`, `Reassembler`, `HostSession`, `ClientSession`, FEC, `InputSender/
Receiver`) build sang WASM và chạy nguyên trạng.

## 9. Cấu trúc file dự kiến

```
client/web/                 bundle web tĩnh (chưa tồn tại — tạo ở GĐ này)
  index.html                ô nhập host + canvas + overlay số liệu
  src/
    main.js                 vòng đời: kết nối → nhận datagram → decode → render
    wt-transport.js         WebTransport: mở, gửi/nhận datagram, đọc maxDatagramSize
    video-decoder.js        WebCodecs VideoDecoder + vẽ canvas
    input.js                Pointer Lock + bảng code→scancode + gửi INPUT_EVENT
    core-bridge.js          cầu Embind/EM_JS vào core.wasm
  wasm/                      output Emscripten của core/ (core.js + core.wasm)

core/                       chỉ sửa trần payload thành runtime (xem §8)
(module host `WebTransportHost` — dùng chung mọi OS, layout ở 11-platform-transport.md §2)
```

`core/CMakeLists.txt` cần thêm nhánh toolchain **Emscripten** (song song với NDK) để xuất
`core.wasm`. `client/web` build bằng bundler nhẹ hoặc chỉ ES module thuần — chưa quyết,
để M1. Phía host, `WebTransportHost` đặt ở module host dùng chung (không trong `core/`);
xem `11-platform-transport.md` §2.

## 10. Lộ trình / mốc kiểm chứng

- ⬜ **M1 — WASM + loopback trong tab.** Build `core/` sang WASM; một trang test bơm
  datagram giả (dump từ `core_tests`) vào `Reassembler` WASM → NAL → `VideoDecoder` →
  canvas. Chứng minh chuỗi WASM↔WebCodecs chạy, **chưa cần mạng**. Đối ứng "loopback GĐ2".
- ⬜ **M2 — WebTransport echo + chứng chỉ.** Host msquic phục vụ bundle + endpoint
  WebTransport trên chứng chỉ ECDSA tạm; trình duyệt kết nối được qua `serverCertificateHashes`,
  handshake HELLO/HELLO_ACK đi trọn vòng bằng datagram. Chốt thư viện QUIC ở đây.
- ⬜ **M3 — Video thật e2e LAN.** Host chia sẻ cửa sổ → web client hiện hình, đo fps/kbps/
  mất gói/RTT/trễ e2e như overlay Windows. So mốc trễ với UDP native.
- ⬜ **M4 — Input.** Pointer Lock + bàn phím scancode điều khiển ứng dụng thường rồi game
  thật, đo trễ input. Bật lưới `blur`/`visibilitychange`.

## 11. Rủi ro / câu hỏi mở

1. **Phân phối hash chứng chỉ** (cao — về UX): làm sao người dùng lấy hash gọn gàng và
   an toàn. Quyết định giữa chuỗi-kết-nối/QR (out-of-band) và host-tự-phục-vụ (tiện nhưng
   hash qua kênh LAN). Xem §6.
2. **Mức hỗ trợ WebTransport của msquic** (trung bình): cần xác nhận lúc M2; nếu thiếu,
   cân nhắc quiche.
3. **Chỉnh trần datagram** (thấp): `maxDatagramSize` thay đổi theo đường; chọn lấy trị lúc
   handshake và cố định phiên, hay cập nhật động khi RECONFIG.
4. **Kích thước/hiệu năng core.wasm** (thấp): logic giao thức nhẹ, không phải đường nóng
   pixel — dự kiến không đáng ngại, nhưng đo ở M1.
5. **Phím bị trình duyệt/OS nuốt** (thấp): một số tổ hợp không bắt được từ trang web
   thường (§5). Chấp nhận cho v1 view+input.

## 12. Vì sao thiết kế thế này (tóm)

- **WebTransport datagram** vì nó là transport trình duyệt duy nhất ánh xạ 1-1 với mô hình
  UDP hiện tại → giữ trọn `core/` và toàn bộ thiết kế giao thức v1.
- **Tất cả bằng datagram ở v1** vì đó là điều kiện để `core/` không đổi; dời control/input
  sang stream tin cậy là tối ưu để dành.
- **WASM tái dùng core** thay vì viết lại JS — cùng lý do Android tái dùng: không mở bản
  thứ ba của đường nóng dễ sai nhất.
- **WebCodecs** vì nó giải mã phần cứng và xuất frame hiển thị được, gộp vai trò
  decoder + renderer thành một.
- **`serverCertificateHashes`** vì app là ngang hàng LAN không có CA công cộng — đúng ca sử
  dụng mà nhánh này của WebTransport sinh ra để phục vụ.
