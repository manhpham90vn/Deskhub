[English](ARCHITECTURE.md) · **Tiếng Việt** · [中文](ARCHITECTURE.zh.md) · [日本語](ARCHITECTURE.ja.md)

# Deskhub — Architecture

Tài liệu này dành cho người sửa mã nguồn Deskhub. Nó mô tả các layer, process, thread,
wire protocol và lý do đằng sau những quyết định thiết kế. Hành vi của sản phẩm nằm trong
[`SPECIFICATION.vi.md`](SPECIFICATION.vi.md); giới hạn bảo mật nằm trong
[`SECURITY.vi.md`](../SECURITY.vi.md).

Đây là bản dịch của [`ARCHITECTURE.md`](ARCHITECTURE.md). Nếu hai bản có khác biệt, bản
tiếng Anh là bản chuẩn.

- **Trạng thái:** mô tả mã nguồn hiện tại.
- **Đối tượng:** người đóng góp vào phần triển khai.

---

## 1. Các layer

Cấu trúc này theo một nguyên tắc: viết logic dùng chung một lần, rồi sử dụng ở mọi client.

```
core/       C++20 thuần, không OS header, không mã bên thứ ba, unit test offline
platform/   lớp abstraction mỏng cho OS, mỗi header cung cấp một API giống nhau (phụ thuộc core)
client/     app theo từng OS: windows, linux, macos, ios, android (phụ thuộc platform và core)
            cùng client/cli, một command line client cho cả ba nền tảng desktop
```

| Layer | Nội dung |
| --- | --- |
| `core/protocol` | Wire format (`Wire.h`), record framing cho stream (`RecordStream.h`), packet classifier phân biệt QUIC với mọi thứ khác |
| `core/transport` | Packetizer/Reassembler cho video, FEC, cache retransmit, send pacer |
| `core/session` | Các session state machine, chia theo vai trò: `session/host` (session theo từng viewer, bảng viewer, `SourceListResponder`, file receiver, auth throttle), `session/client` (screen client, file sender, terminal client, luồng connect), cùng các thành phần dùng chung đặt cạnh chúng (kiểu dữ liệu transfer, bảng terminal session, clipboard sync, link recovery) |
| `core/control` | Bitrate controller, quality ladder, tính kích thước stream, clock offset |
| `core/terminal` | VT emulator dùng chung cho mọi client: `VtParser`, `Screen`, `KeyEncoder`, `Palette` |
| `core/net` | Trust store theo fingerprint (phía client), authorized keys (phía host), các yêu cầu kết nối đang chờ (`AccessRequests`), record lời mời `deskhub://pair/` (`PairingInvite`), văn bản public key OpenSSH, một `Base64` duy nhất cho mọi nơi gọi, chọn bind address |
| `core/auth` | Transcript auth được ký (`Transcript`), bộ giới hạn thất bại theo key và địa chỉ, và các token QR dùng một lần (`PairingTokens`) |
| `core/qr` | `QrCode` — bộ encode QR mà mọi client và CLI dùng để vẽ mã pairing |
| `core/ui` | Toàn bộ chuỗi hiển thị cho người dùng, phần parse settings, các builder dòng bảng, thiết bị gần đây và host profile (`HostProfiles`), để cả năm client hiển thị cùng nội dung |
| `platform/net` | `UdpSocket` (theo từng OS), `QuicEndpoint` (quiche đặt sau pimpl), `SessionTransport` |
| `platform/auth` | `AuthNegotiation` — handshake chữ ký bằng key duy nhất mà cả hai phía sử dụng, với bốn kết cục phía host của nó (mục 3) |
| `platform/client` | `HostLink` (dial, trust, auth, chờ approve, channel; dùng chung cho mọi giao diện), `ScreenViewer`, `TerminalViewer`, `FileTransferClient`, `SourceQuery`, `HostProfiles` (các host đã trust theo fingerprint, kèm tên và địa chỉ gần nhất) |
| `platform/host` | `HostEngine`, `HostNetLoop`, `SharingHost`, `TerminalHost`, `FileHost`, `ViewerBroadcast`, `PairingInvite` (phát hành token và dựng lời mời mà host này hiển thị) |
| `platform/system` | Clock, random, PTY (ConPTY / forkpty), key của máy (`HostIdentity`), file `authorized_keys` và `known_hosts`, các yêu cầu đang chờ (`AccessRequestsFile`) và token QR còn hiệu lực (`PairingTokenFile`), danh sách gần đây (`RecentDevicesFile`), tên thiết bị, autostart, keep-awake |
| `platform/ffi` | Giao diện C mà app Swift và Kotlin gọi: `SettingsFfi` (settings, tên thiết bị), `DevicesFfi` (thiết bị gần đây, client được phép, yêu cầu kết nối, fingerprint và public key của máy này), `HostProfileFfi` (host đã trust), `PairingFfi` (lời mời, các module QR, thu hồi), cùng các giao diện share, screen, terminal và send |
| `core/cli` | Cú pháp command line và bộ ghi JSON của nó: nhận văn bản thuần, trả về command đã được kiểm tra |
| `client/<os>` | Capture, encode, decode, render, windowing, hộp thoại; không chứa thành phần nào thuộc protocol |
| `client/cli` | Từ cờ tới session: một binary có thể host, connect và mở shell mà không cần GUI toolkit. Nó link cùng thư viện media theo từng OS mà app desktop sử dụng |

`core/` phải luôn test được offline, không cần network và GPU. `platform/` được phép sử
dụng OS nhưng phải cung cấp một API giống nhau ở mọi nền tảng. Nếu cùng một đoạn mã xuất
hiện ở hai client, nó thuộc về layer thấp hơn.

## 2. Một port, một transport

Mọi dịch vụ mà host cung cấp đều chạy trên **một UDP port** (mặc định 47777) thông qua một
`SessionTransport`, bao bọc một `QuicEndpoint` duy nhất:

```
                      UDP port 47777
                            |
                 ClassifyPacket (byte đầu tiên)
                   /                    \
            packet QUIC          mọi thứ khác
                 |                        |
   +-------------+------------+        bị loại bỏ: không gì được
   |             |            |        trả lời ở dạng không encrypt
 stream      datagram      (TLS)
   |             |
 control      video
 input        audio       Stream mang các record đã framing (RecordStream):
 clipboard                message có length prefix, tối đa 16 KiB.
 terminal                 Mỗi datagram mang một packet video
 file                     hoặc audio (≤ 1200 B).
```

- **Stream** (tin cậy, đúng thứ tự): control, input, clipboard, terminal, file. Mỗi
  connection sử dụng một bidirectional stream do client mở. Một stream bị nghẽn trên
  connection này không làm nghẽn connection khác. Dữ liệu stream đi vào được xử lý theo
  hạn mức 64 KiB mỗi lượt service: thành phần tiêu thụ dữ liệu, chủ yếu là phần VT
  emulation của terminal, trả quyền điều khiển lại cho ACK, keepalive và xử lý timeout
  giữa các lát, nên một lượng output lớn từ terminal không còn khiến connection bị đóng
  do idle timeout.
- **Datagram** (không tin cậy, không bảo đảm thứ tự, vẫn được encrypt): packet video và
  audio. QUIC không retransmit các packet bị mất; với video, cơ chế FEC/NACK của app xử
  lý phần mất mát, còn với audio thì không có cơ chế nào — xem mục 9.
- **UDP thô** không bao giờ được trả lời. Không có discovery: packet đi vào không phải
  QUIC đều bị loại bỏ trước khi tới bất kỳ phần mã session nào, và `SourceListResponder`
  chỉ trả lời `LIST_SOURCES` và `PING` session-0 cho một connection đã authenticate.

`QuicEndpoint` che hoàn toàn quiche (pimpl; `QuicEndpointNone.cpp` thay bằng stub, nhưng
chỉ khi bản build chủ động dùng `-DDESKHUB_QUIC=OFF`; thiếu quiche sẽ làm fail bước
configure, vì một binary stub không thể share hay connect). Connection được nhận diện qua
địa chỉ peer; không có connection migration. Theo hợp đồng, một connection quiche là
single-threaded, nên mọi thao tác trên endpoint đều diễn ra dưới send mutex của transport.
Transport không giữ mutex này xuyên qua một lần chờ socket blocking: `WaitReadable` chạy
trước ở trạng thái không khoá, sau đó là một lần `Poll` ngắn có khoá. Giữ mutex xuyên qua
lần chờ sẽ chặn mọi bên gửi.

## 3. Cơ chế chấp nhận: key, như SSH

Mỗi máy tạo một key ECDSA P-256 duy nhất trong lần chạy đầu tiên (`HostIdentity`,
`host_key.pem`) và không bao giờ tự động thay nó. Key duy nhất đó chính là máy ở cả hai vai:
host đưa nó ra qua TLS, và client đăng nhập bằng nó. Hash SHA-256 của DER
SubjectPublicKeyInfo là fingerprint duy nhất mà người dùng nhìn thấy, trên trang Devices,
trong mã QR, trong danh sách yêu cầu kết nối, trong `authorized_keys` và `known_hosts` như
nhau. TLS cần một certificate X.509, nên mỗi lần port mở, `HostIdentity` dựng một
certificate tự ký quanh key đó **trong bộ nhớ** và giao cho quiche; không có gì được ghi ra.
Vì fingerprint hash SPKI chứ không hash certificate, một certificate mới ở mỗi lần khởi
động không làm thay đổi bất cứ thứ gì ai đó đã ghim, và file `host_cert.pem` mà các phiên
bản cũ lưu không được đọc cũng không cần tới. Host chỉ chấp nhận public key có trong
`authorized_keys` của nó (`AuthorizedKeys`, tối đa 128 dòng dạng
`ecdsa-sha2-nistp256 AAAA… label` — dòng Ed25519 vẫn được parse cho key dán bằng tay);
label chỉ là tên hiển thị, không bao giờ là quyền.

Bên trên TLS, một handshake ở tầng ứng dụng (`AuthNegotiation`, auth version 7) quyết
định việc chấp nhận theo từng connection. Transport thực thi handshake này, và host không
gửi gì ở tầng ứng dụng cho connection chưa hoàn tất phần auth:

1. QUIC/TLS hoàn tất. Client xác định trust với key của host **trước khi gửi bất cứ thứ
   gì** (`HostLink::SettleTrust`, xem bên dưới).
2. Client gửi `AuthStart`: `00 | u16 keyLen | key | u8 nameLen | name |
   u8 tokenLen | token | 07` — public key của nó, tên thiết bị của nó, token pairing 32 byte
   khi nó đến từ mã QR (`tokenLen` là 0 hoặc 32), và auth version ở cuối.
3. `HostAuth::Begin` trả lời bằng một trong bốn `AuthChallenge`:
   - key có trong `authorized_keys` → `Signature`;
   - key chưa biết và token khớp một mục còn hiệu lực trong `pairing_tokens` → key được
     nối vào `authorized_keys` với nhãn là tên của client, token bị tiêu thụ, và câu trả
     lời là `Signature`;
   - key chưa biết và có gửi token nhưng token sai → một lần thất bại được tính cho địa
     chỉ nguồn trong bộ giới hạn hiện có (3 lần mỗi phút, rồi chặn 10 giây), và lời mở đầu
     sau đó được xử lý như không mang token;
   - key chưa biết và không có token dùng được → một yêu cầu kết nối (tên, key,
     fingerprint, địa chỉ, thời điểm) được ghi vào `access_requests` và câu trả lời là
     `AwaitingApproval`. Connection bị đóng như một lần từ chối hiện nay; host không giữ
     connection chưa authenticate nào chờ một cú click.
4. Khi nhận `Signature`, client ký một transcript — domain label, auth version, vai trò,
   giá trị session export từ chính QUIC/TLS connection này, public key của nó và
   fingerprint TLS của host (`core/auth/Transcript`) — và host xác minh chữ ký với key đó.

Một chữ ký chỉ gắn với đúng một connection, nên mỗi lần kết nối lại phải ký lại; không có
0-RTT hay session resumption. Host giữ tối đa 8 connection đang chờ authenticate và loại
bỏ từng connection sau 10 giây; 3 chữ ký sai từ một key và một IP nguồn trong vòng một
phút sẽ chặn cặp đó trong 10 giây (`AuthThrottle`). `access_requests` giữ tối đa 16 yêu
cầu, mỗi key một yêu cầu (lần xin lại làm mới địa chỉ và thời điểm), mỗi yêu cầu trong 10
phút; *Approve* chuyển key vào `authorized_keys` kèm tên thiết bị, *Deny* xoá dòng đó và
không báo gì cho client.

Việc được chấp nhận gắn với một QUIC connection, không gắn với địa chỉ. Nó bị huỷ ngay khi
connection đó đóng, nên connection tiếp theo từ cùng địa chỉ và port phải chứng minh lại từ
đầu. Một `AuthStart` thứ hai trên connection đã bắt đầu handshake sẽ khiến connection bị
đóng: danh tính đã xác lập không thể bị tráo bằng một danh tính chưa từng được chứng minh,
và một key bị từ chối không thể được thử lại ngay trên connection đó. Gỡ một client key
trên trang Devices (hoặc `access remove`) cũng đóng mọi connection mà key đó đang mở.

Ở phía client, `known_hosts` (`TrustStore`) được đánh khoá theo **fingerprint** của host;
mỗi mục mang tên host, địa chỉ gần nhất mà host trả lời và thời điểm gặp lần đầu/lần cuối
(`HostProfiles`). `HostLink::SettleTrust` chạy ngay khi TLS xong, trên fingerprint của key
mà đầu kia đưa ra:

- Dial từ một lời mời QR: fingerprint phải bằng fingerprint trong lời mời. Bằng nghĩa là
  máy đang trả lời giữ private key của máy đã tạo mã, nên host được ghim âm thầm và token
  được gửi trong `AuthStart`. Khác nghĩa là có thứ khác trả lời ở địa chỉ đó: link thất
  bại với `InviteMismatch` và token không bao giờ rời khỏi client.
- Đã có trong `known_hosts`: địa chỉ gần nhất của mục đó được làm mới (`TouchTrustedHost`)
  và link tiếp tục — ở bất kỳ địa chỉ nào tới được host, vì không còn gì được đánh khoá
  theo địa chỉ nữa.
- Ngoài ra, link thất bại với trạng thái *chưa được trust* kèm fingerprint; app hiển thị
  hộp thoại *New host* và dial lại với `acceptNewHostKey` sau *Trust and connect*, còn CLI
  chỉ làm vậy khi có `--accept-new-host-key`. Nếu `FindByEndpoint` cho biết địa chỉ đó
  từng trả lời với tư cách một host đã trust khác, `PreviousOwnerWarningFor` thêm tên và
  fingerprint của host đó vào lời nhắc. Không có phán quyết *key đã thay đổi*: một key mới
  ở một địa chỉ cũ là một host mới.

Khi challenge là `AwaitingApproval`, `HostLink` đỗ lại ở trạng thái cùng tên, hiển thị
`AwaitingApprovalLine`, và dial lại với backoff mà các link đang recovery đã dùng, trong
tối đa `kDefaultApprovalWaitUs` (120 giây) hoặc cho tới khi nơi gọi huỷ; mỗi lần dial lại
là một connection đầy đủ và một `AuthStart` mới, nên lần đầu tiên sau *Approve* của chủ
host nhận được `Signature` và hoàn tất. Quá hạn, link thất bại với
`AuthResultCode::AwaitingApproval`, thông báo của nó bảo người dùng nhờ *Approve* rồi connect
lại.

Dữ liệu truyền đi là bản thân public key, không phải một fingerprint đơn lẻ: host tự hash
nội dung nhận được, nên việc mạo danh đòi hỏi phải ký bằng một key mà kẻ mạo danh không
có. Và vì việc chấp nhận chỉ được xử lý một lần cho mỗi connection, không thành phần nào
phía trên transport phải hỏi lại: phần mã session coi toàn bộ connection là đã
authenticate.

## 4. Phía host

```
HostEngine (mỗi app một instance, sở hữu SessionTransport)
 ├─ thread net-loop: RunHostNetLoop
 │    recv → trả lời source-list/pong (chỉ khi đã được chấp nhận) | nạp dữ liệu video | Chan::Terminal → TerminalHost
 │    Tick session theo từng source, flush clipboard, reconfig, thống kê
 ├─ capture/encode: theo từng source, do callback capture của OS điều khiển (layer client)
 │    frame → encoder (mutex theo source) → Packetizer → FEC → SendTo (datagram)
 ├─ audio worker: callback capture → ring frame lock-free → Opus encode →
 │    datagram theo từng viewer (AudioBroadcaster)
 └─ TerminalHost (chỉ tồn tại khi terminal được share)
      ├─ HandleMessage trên thread net-loop: TERM_OPEN/DATA/RESIZE/CLOSE/EXIT/LIST → PTY
      └─ thread pump: output của PTY → Screen mirror phía host và record TERM_DATA,
           tách shell khi mất peer, kicks
```

- Engine hoạt động bất cứ khi nào có nội dung được share. Khi không có screen source nào
  và terminal được chọn, engine chạy ở trạng thái không source; vòng lặp tồn tại chừng nào
  terminal còn hoạt động.
- Mỗi screen source là một `SourcePipelineState` với `ScreenHostSession` riêng (bảng
  viewer, negotiation, phân xử input), encoder, quality ladder và phần chẩn đoán riêng.
  Một lần encode phục vụ mọi viewer của source đó.
- Vòng phản hồi: viewer gửi `Feedback` (loss và RTT) mỗi giây một lần, và host bổ sung một
  tín hiệu của riêng nó là tuổi của frame tại thời điểm nó tới bên gửi, cũng chính là đại
  lượng `enc_lat_ms` báo cáo. `BitrateController` (AIMD) và `QualityLadder` điều chỉnh
  bitrate của encoder, độ phân giải và fps dựa trên cả ba tín hiệu. FEC được bật từ frame
  đầu tiên và chỉ tắt sau một khoảng dài không có mất mát, vì loại mất mát mà nó bảo vệ
  xuất hiện trước cả báo cáo đầu tiên; tình trạng tồn đọng không kích hoạt FEC, vì parity
  chỉ làm hàng đợi dài thêm. Congestion control CUBIC của quiche nằm bên dưới đường
  datagram; hai cơ chế hoạt động nối tiếp: quiche giới hạn lượng dữ liệu rời khỏi máy, còn
  app điều chỉnh encoder theo mức mất mát phát sinh.
- Input: host được ưu tiên. `LocalInputMonitor` tạm dừng remote input khi người dùng tại
  máy đang thao tác với mouse của họ; mỗi thời điểm chỉ một viewer điều khiển.
- Shell: mỗi shell một PTY (`ConPTY` trên Windows, `forkpty` trên các nền tảng khác), tối
  đa 8 shell. Khi mất kết nối, shell được tách ra và PTY được giữ sống cho đến khi tiến trình shell thoát hoặc shell bị đóng — không giới hạn thời gian. Mọi client đã được chấp nhận đều có thể liệt kê các shell đang được giữ (`TermList`/`TermListAck`) và reattach một shell theo id. Mọi thao tác open, close, detach và reattach đều được ghi vào audit log kèm
  địa chỉ, tên và key. `TERM_CLOSE` được trả lời trước guard theo peer mà các message
  data và resize phải đi qua, nên bất kỳ client đã được nhận vào cũng kết thúc được một
  shell bất kỳ theo id, và máy đang ở trong shell đó nhận `TERM_EXIT`.
- Một picker cho cả năm client: `core/ui/ShellPicker` biến một `TermSessionList` thành
  các dòng mà mọi client vẽ ra —— id và kích thước, shell thuộc về ai, và client này có
  được reattach hay đóng nó không. Chỉ shell đã detach mới reattach được, còn shell host
  đã tiếp quản thì không thuộc cả hai. App Apple và Android đọc đúng các dòng đó qua
  `DHTermSessionInfo`, nên không client nào tự format dòng shell của riêng mình.
- Output của mỗi shell đồng thời được đưa vào một `core/terminal` Screen phía host ngay từ
  khi shell bắt đầu. *Stop & attach* ngắt client từ xa và mở bản mirror đó, giữ nguyên
  scrollback, trong một cửa sổ terminal trên host. Một shell được tiếp quản theo cách này
  thuộc về host, không hết hạn, và kết thúc khi cửa sổ trên host đóng lại.

## 5. Phía client

Mọi giao diện client đều kết nối tới host thông qua cùng một thành phần là `HostLink`
(`platform/client/HostLink`): nó thiết lập connection QUIC, kiểm tra trust store, thực
hiện auth handshake, duy trì link, và với những giao diện có yêu cầu, thực hiện kết nối
lại với backoff khi link bị mất. Không service nào tự thiết lập kết nối hay tự
authenticate; mỗi service mở một channel theo `Chan` trên đường truyền, nhận một hàng đợi
inbox riêng và xử lý hàng đợi đó trên thread của chính nó:

```
HostLink (mỗi giao diện đang mở một instance)
 ├─ thread link: dial → kiểm tra trust → auth → pump
 │   (định tuyến record và datagram đi vào theo Chan tới các hàng đợi
 │    riêng của từng channel; link pulse; kết nối lại với backoff nếu bật recovery)
 ├─ Chan::Control/Video/Audio ─> ScreenViewer
 │    ├─ thread net: HELLO/negotiation, nạp video (Reassembler và FEC),
 │    │   NACK, feedback, clipboard
 │    └─ thread decode: decoder và hàng đợi render
 ├─ Chan::Terminal ─> thread service của TerminalViewer
 │    ├─ core/terminal Screen giữ lưới ký tự
 │    └─ UI poll Snapshot() và đẩy phím vào một hàng đợi lệnh
 └─ Chan::File ─> thread service của FileTransferClient (ring FileUpload)
```

Sau khi được chấp nhận, link tự theo dõi tình trạng của chính nó
(`core/session/LinkPulse`): một datagram `Ping` với session id 0 được gửi mỗi giây, và
`SourceListResponder` của host trả lời trên cùng connection đã authenticate đó mà không cần
session. Vì một ping là ack-eliciting nên nó đồng thời đóng vai trò keepalive; timer
keepalive thông thường chỉ còn ý nghĩa trước khi link được chấp nhận. Trên một link đang phục hồi, pulse cũng là phép
kiểm tra liveness: năm giây không nhận được pong, và chỉ tính sau khi đã có một pong đầu
tiên xác nhận host có phản hồi, sẽ đưa connection vào đường kết nối lại sẵn có. Năm giây
này được tính theo thời gian mà vòng lặp link thực sự theo dõi: `LinkPulse::Tick` chạy một
lần mỗi vòng của `HostLink::PumpReady`, và phần thời gian một vòng vượt quá
`kLinkWatchStepUs` được trừ khỏi khoảng im lặng, nên một máy bị treo không bị hiểu nhầm là
host đã ngừng phản hồi.

Screen viewer hiện cũng sử dụng cơ chế recovery này, giống terminal từ trước: một link bị
mất hoặc ngừng dữ liệu, hoặc một session năm giây không nhận được gì, sẽ đưa cửa sổ về
trạng thái `Reattaching` (khung hình cuối vẫn hiển thị, dòng status chuyển sang nội dung
đang reattach) thay vì kết thúc. `HostLink::RequestRedial` kích hoạt kết nối lại khi
session phát hiện vấn đề trước, và khi link được chấp nhận lại, viewer thực hiện lại
`HELLO` với cùng client id — host gắn lại vị trí của viewer — và việc stream tiếp tục từ
keyframe mới. Sau sáu mươi giây (`kViewerReattachGraceUs`) mà không kết nối lại được, cửa
sổ kết thúc kèm lý do như thông thường.

Phần truy vấn source (`QuerySources`) sử dụng cùng link đó theo hình thức một lần, dạng
blocking. UI vẫn đẩy các yêu cầu (phím, resize) vào các hàng đợi lệnh. Một
host key chưa biết khiến link thất bại kèm fingerprint của nó — và cảnh báo chủ cũ khi địa
chỉ đó từng trả lời với tư cách một host đã trust khác — để UI hiển thị trong hộp thoại
*New host*; host chưa cho phép key này đỗ link ở `AwaitingApproval`, mà dòng trạng thái của
nó được UI poll trong khi người dùng có thể huỷ. Cửa sổ terminal không parse escape sequence: `core/terminal`
chuyển byte stream thành lưới ô, còn cửa sổ chỉ vẽ ô và chuyển tiếp sự kiện phím. Hiện mỗi
cửa sổ vẫn giữ link riêng; việc dùng chung một link đã được chấp nhận cho mọi cửa sổ trỏ
tới cùng một host là bước tiếp theo đã dự kiến, và sẽ được bổ sung tại `HostLink` dưới
dạng một registry cùng cơ chế fan-out cho observer, không phải thêm một handshake mới.

## 6. Tìm host

Không có discovery: không thành phần nào scan network và host không trả lời packet
plaintext nào. Client dial tới một địa chỉ người dùng nhập, một host gần đây, một host
đã trust (`HostProfiles`) hoặc các địa chỉ trong một lời mời QR. `SourceListResponder` chỉ trả lời `LIST_SOURCES` trên một
connection đã được chấp nhận; phản hồi này cho biết host hỗ trợ những gì — có nhận input
hay không, có share terminal hay không — thông qua các flag trong header `SOURCE_LIST`,
nhờ đó client biết trước khi mở bất kỳ cửa sổ nào rằng một điện thoại chỉ có thể được xem.
Sau các record source, payload mang tên thiết bị của host (một byte độ dài và tối đa 64
byte UTF-8; được phép để trống), nên tên này chỉ tới được client đã authenticate. Client
parse nó bằng `ParseSourceListHostName`, hàm này đổi mọi byte điều khiển thành dấu cách.

Danh sách gần đây nằm trong `platform/system/RecentDevicesFile` (`recent-hosts.txt`), dựa
trên phần parse trong `core/ui/RecentDevices`: địa chỉ, thời điểm kết nối gần nhất và tên
host, tối đa 10. FFI `dh_list_sources` chỉ ghi một host vào đó khi host đã trả lời, nên
các app không còn tự cập nhật danh sách và `dh_recent_touch` đã bị bỏ. File
`recent-devices.txt` cũ bị xoá, không được chuyển đổi.

Mã QR là kênh out-of-band duy nhất, và nó luôn nằm ngoài băng: host không bao giờ truyền
nó, chủ host hiển thị nó và ai đó đọc từ màn hình hoặc dán link.
`deskhubp::BuildPairingInvite(port, bindIp, hostName)` phát hành một token ngẫu nhiên 32
byte (giữ trong `pairing_tokens` với hạn 5 phút, tối đa 4 token còn hiệu lực cùng lúc, tất
cả bị `RevokePairingTokens` thu hồi khi panel bị ẩn hoặc share dừng) và định dạng
`core/net/PairingInvite`: văn bản là `deskhub://pair/` theo sau là base64url của một record
nhị phân — một byte version, số endpoint `n`, rồi `n × (IPv4, port)` cho tối đa 4 địa chỉ
của host, fingerprint 32 byte, token 32 byte và tên host có tiền tố độ dài, tối đa 32 byte.
Record bị giới hạn ở 180 ký tự để ở mức sửa lỗi M nó vừa một mã QR version 10 hoặc nhỏ
hơn, thứ mà điện thoại đọc được từ màn hình laptop ở khoảng cách một tầm tay.
`core/qr/QrCode` (`EncodeQr`, cùng `RenderQrText` cho `share --qr` của CLI) là bộ encode
duy nhất; mọi client vẽ lưới module mà nó trả về, Android qua `dh_qr_encode`.
`ParsePairingInvite` ở phía client cho `HostLink` các endpoint, fingerprint cần đòi và
token cần gửi; `dh_pairing_invite_address` cho các app `ip:port` đầu tiên để hiển thị trong
ô địa chỉ. Scan là phần duy nhất theo từng nền tảng — CameraX + ZXing trên Android,
AVFoundation trên iOS — và cả hai chỉ trả về đúng văn bản đã giải mã.

## 7. Dữ liệu trên đĩa

Mọi dữ liệu nằm trong thư mục Deskhub của người dùng (`~/.deskhub`,
`%USERPROFILE%\.deskhub`, thư mục `.deskhub` bên trong App Group container trên iOS,
internal storage trên Android;
`DESKHUB_CONFIG_DIR` hoặc `--config-dir` của CLI ghi đè vị trí này): `host_key.pem` (key
duy nhất của máy — certificate TLS được dựng trong bộ nhớ ở mỗi lần khởi động, nên
`host_cert.pem` không còn được ghi và file còn sót bị bỏ qua), `authorized_keys` (các client
key mà host này chấp nhận), `known_hosts` (các host đã trust theo fingerprint, kèm tên và
địa chỉ gần nhất), `access_requests` (các yêu cầu kết nối đang chờ Approve hoặc Deny — tên,
public key, địa chỉ, thời điểm; tối đa 16, mỗi yêu cầu bị loại bỏ sau 10 phút),
`pairing_tokens` (các token QR đang còn hiệu lực, kèm hạn của chúng), `ui-settings.txt`
(bao gồm tên thiết bị), `recent-hosts.txt` (địa chỉ, thời điểm kết nối gần nhất và tên
host), `portal-restore-token.txt` trên Linux (token của chính desktop cho những màn hình đã
chọn trong hộp thoại chia sẻ màn hình), cùng log theo từng lần chạy. Không có passcode nào
được lưu ở bất cứ đâu, và không có `client_key*.pem`: các file mà phiên bản cũ giữ bị bỏ
qua, không được migrate. Trên POSIX, thư mục có quyền `0700` và file `0600`, được ghi
atomic; trên Windows, ACL chỉ cho phép tài khoản người dùng, SYSTEM và Administrators. Trên
iOS, app và broadcast extension dùng chung thư mục này, và đó là cách yêu cầu của extension
tới được danh sách của app và *Approve* của app tới được extension. Phần file I/O nằm trong
`platform/`; phần parse và các cấu trúc dữ liệu nằm trong `core/` và có unit test.

File do viewer gửi được lưu ở nơi khác: một thư mục do host chọn (trường `transfer_dir`
trong `ui-settings.txt`, mặc định là `Deskhub` trong thư mục home của người dùng).
`FileStore` ghi từng file dưới tên `<name>.deskhub-part` và chỉ đổi tên sau khi toàn bộ
file đã tới với CRC-32 khớp, nên một file ghi dở không bao giờ xuất hiện dưới tên thật;
`UniqueFileName` bảo đảm không file nào bị ghi đè. Tên file trên đường truyền được
`SafeFileName` trong `core/` xử lý — loại bỏ dấu phân cách đường dẫn, byte điều khiển, ký
tự Windows không chấp nhận và các tên thiết bị dành riêng — trước khi `platform/` thao tác
với filesystem.

## 8. Test

| Suite | Phạm vi chạy | Nội dung kiểm tra |
| --- | --- | --- |
| `make test` | offline, không socket | toàn bộ `core/`: wire (gồm trường token của `AuthStart`), framing, FEC, session, VT emulator, settings, chuỗi văn bản, structured fuzzing tất định, và các mảnh pairing — `Base64`, round trip và giới hạn của `PairingInvite`, phát hành/tiêu thụ/hết hạn của `PairingTokens`, sức chứa và hết hạn của `AccessRequests`, `QrCode` so với các encoding đã biết |
| `make test-platform` | socket loopback | QUIC handshake thật, xác thực bằng chữ ký key end-to-end, ghim host theo fingerprint, `AccessRequestsFile` (một yêu cầu được ghi, approve và deny) và `PairingTokenFile` (một token được phát hành, dùng một lần và thu hồi), chấp nhận bằng approve và bằng token qua `HostAuth` thật, terminal host và viewer qua đường truyền, PTY với shell thật, lockout khi chữ ký sai |
| `make test-integration` | loopback, capture/encode giả lập | session host↔client đầy đủ: negotiation, video qua đường truyền, input, chấp nhận theo authorized key, vector wire `AUTH_START_TOKEN` bên cạnh các golden message khác, khả năng chịu dữ liệu không hợp lệ, và độ trễ dưới tải chéo — một phiên truyền file, một terminal có lượng output lớn và các phím gõ chạy song song với một stream đang hoạt động, mỗi hạng mục được kiểm theo độ trễ lớn nhất quan sát được |
| fuzz target | 30 giây mỗi target trên mỗi PR, 15 phút mỗi target hằng đêm | parser cho wire, H.264, reassembly, byte terminal và chuỗi UI, cùng các session state machine phía host và phía viewer |
| `make test-perf` | bản release, offline và loopback | đo thực tế các hot path: `core_perf` bao phủ các đường thuần C++, `platform_perf` bao phủ QUIC thật qua loopback; cả hai fail theo số allocation trên mỗi đơn vị, theo chi phí ở mức input gấp 4 lần, và theo độ lệch so với baseline ghi trên chính máy đó |

CI còn áp dụng thêm clang-format và clang-tidy (cả hai đều pin phiên bản), SwiftLint
`--strict`, Android Lint, actionlint và shellcheck, các lượt chạy cả ba suite dưới ASan và
TSan, CodeQL trên C++/Kotlin/Swift, một lượt gitleaks quét toàn bộ lịch sử, cùng yêu cầu
coverage của `core/` đạt ≥ 90 % line và ≥ 80 % branch. Ba suite này còn được cross-build và
chạy trên Linux arm64, một Android emulator và iOS Simulator. Ngoài ra, một job Windows
chạy integration suite thêm ba lần mỗi vòng để tìm một lỗi memory corruption không thường
xuyên, xuất hiện khoảng một lần trong ba lần chạy. Frame xảy ra crash là hệ quả của lỗi
corruption chứ không phải nguyên nhân, nên cú crash bắt buộc phải để lại dump: binary test
tự ghi minidump đầy đủ cho mọi exception tới được handler, và vì fastfail không tới được
handler nào, mỗi job Windows còn bật Windows Error Reporting và xác nhận khả năng thu thập
bằng một lần fail-fast có chủ đích trước khi suite tương ứng bắt đầu chạy. Bản nightly chạy
lại các load test thêm hai lượt: một lượt dưới full page heap, và một lượt với quiche được
build kèm Rust debug assertion và overflow check — đây là cơ chế duy nhất quan sát được
bên trong quiche, vì ASan không instrument Rust còn page heap chỉ bảo vệ phần heap. Các
job release trên Linux và macOS cũng chạy `core_perf` và `platform_perf` với hai tiêu chí
allocation và scaling (trên runner dùng chung không có baseline thời gian). Mỗi pull
request còn nhận một báo cáo perf-and-lag dưới dạng một comment tự cập nhật, gồm: kết quả
A/B của cả hai perf suite so với base commit trên cùng runner (độ lệch chỉ là cảnh báo,
không gây fail), các số liệu integration dưới tải từ bản build của pull request, và dòng
coverage của core.

## 9. Những quyết định cần ghi nhớ

- **Host chỉ gửi dữ liệu ứng dụng sau khi cấp quyền**: `SessionTransport` từ chối gửi
  record và datagram cho tới khi kết nối đó xác thực bằng khóa xong. Bản tin challenge
  và kết quả dùng đường gửi auth nội bộ. Trust store tuần tự hóa thao tác đọc-sửa-ghi
  trong một tiến trình và lưu thay đổi bằng cách thay thế file atomic.

- **Danh sách khóa client hỏng không cấp quyền**: khi nạp `authorized_keys`, file không
  đọc được, quá lớn, sai định dạng hoặc trùng khóa đều làm toàn bộ cấu hình thất bại.
  Host kiểm tra lại quyền của kết nối đang chạy theo chu kỳ, nên file được thay từ
  tiến trình khác vẫn có thể thu hồi kết nối mà không cần generation nội bộ đổi.

- **Trust đi theo key, không theo địa chỉ**: `known_hosts` được đánh khoá theo
  fingerprint của host, và địa chỉ bên cạnh chỉ là địa chỉ gần nhất đã trả lời. Quy tắc
  trước đây — một pin gắn với một `ip:port`, và key thay đổi ở đó là chặn cứng không có
  nút chấp nhận — khiến mỗi lần DHCP đổi lease trông như một cuộc tấn công và tập cho
  người dùng thói quen gỡ rồi trust lại host theo phản xạ, chính là thói quen mà việc chặn
  sinh ra để ngăn. Host đã trust tới được ở địa chỉ mới giờ đơn giản là connect, và một
  key *khác* ở địa chỉ đã biết được xử lý đúng như bản chất của nó, một máy mà client này
  chưa từng gặp: hộp thoại *New host* kèm cảnh báo nêu tên host từng trả lời ở đó
  (`PreviousOwnerWarningFor`). Cảnh báo giữ lại tín hiệu duy nhất mà việc chặn mang theo —
  "thứ gì đó ở địa chỉ này không còn là ai nó từng là" — trong khi để nguyên host cũ trong
  *Trusted hosts*, nên việc trust host mới không bao giờ là một cú click qua một thay đổi,
  chỉ là một lần gặp đầu tiên với fingerprint hiển thị rõ.

- **Danh sách cấp quyền mới chứa public key đầy đủ**: `authorized_keys` nhận các dòng
  public key OpenSSH có giới hạn và từ chối dòng hỏng hoặc trùng khóa. Đây là danh
  sách cấp quyền duy nhất: thiếu file thì không ai được nhận. `known_hosts` lưu alias và
  địa chỉ gần nhất của từng host đã trust cạnh fingerprint của nó. Ghi cấu hình dùng khóa
  file liên tiến trình và thay thế file atomic.
  Service có thể chọn thư mục cấu hình qua `SetConfigDir` hoặc `DESKHUB_CONFIG_DIR`
  độc lập với thư mục log.

- **Fingerprint dựa trên SPKI**: mọi giá trị `SHA256:…` Deskhub hiển thị hoặc lưu là
  SHA-256 của DER SubjectPublicKeyInfo. Dòng public key OpenSSH mang SSH blob;
  fingerprint SSH thường dùng hash blob này nên không thể so trực tiếp với
  fingerprint SPKI của Deskhub. Khi nhập khóa text, app chuyển sang SPKI trước khi
  tính fingerprint Deskhub.

- **Chữ ký bao phủ một transcript auth không mơ hồ**: `core/auth/Transcript` mã hóa
  domain Deskhub, auth version, vai trò bên ký, giá trị 32 byte xuất từ QUIC/TLS,
  toàn bộ public key client và fingerprint khóa TLS host thành các trường có tiền tố
  độ dài. Bản vá nhỏ cho quiche 0.29.3 cung cấp TLS exporter qua C API. Hai đầu lấy
  cùng giá trị cho kết nối này; nếu không xuất được thì auth thất bại. Host chỉ nhận
  một phản hồi có chữ ký trên mỗi kết nối, ngăn phát lại trên phiên khác.
  Host giữ tối đa tám yêu cầu đang chờ chữ ký và đóng từng kết nối nếu không có phản hồi
  trong mười giây. Ba chữ ký sai từ cùng khóa và IP nguồn trong một phút sẽ tạm chặn
  cặp đó mười giây. Bảng trong bộ nhớ giữ tối đa 64 cặp; chữ ký đúng xóa bộ đếm lỗi.

- **Auth có phiên bản riêng trong protocol version 3**: `AuthStart` giữ một byte bằng 0
  trước khóa làm tiền tố tương thích và đặt auth version ở cuối — giờ là 7, sau trường
  token pairing mà version 6 chưa có: `00 | u16 keyLen | key | u8 nameLen | name |
  u8 tokenLen | token | 07`. Host cũ có thể đọc lời mở đầu và gửi challenge cũ; client mới
  nhận ra challenge không tương thích rồi đóng kết nối. Host mới từ chối lời mở đầu có byte
  cuối khác 7, gửi `VersionMismatch` rồi đóng kết nối — đó là lý do một thiết bị 7.0.x và
  một thiết bị 8.0 báo rằng phiên bản của chúng không khớp thay vì hoạt động nửa vời.
  `AuthMode` có thêm `AwaitingApproval`, và `AuthResultCode::AwaitingApproval` chỉ tồn tại
  ở phía client, để gọi tên kết cục của một lần chờ đã hết hạn. Challenge, response và
  result chỉ mang dữ liệu có version.

- **Trust on first use, cảnh báo khi thay đổi**: host key chưa biết được hiển thị cho
  người dùng một lần, như SSH, và chỉ được ghim khi người dùng chấp nhận
  (`--accept-new-host-key` trong CLI). Một key khác với key từng trả lời ở cùng địa chỉ
  không phải là một *thay đổi* để bấm qua — không có nút chấp-nhận-key-mới và không còn
  kết quả `HostKeyChanged` — mà là một host mà client này chưa từng trust, được gặp qua hộp
  thoại *New host* thông thường với tên chủ cũ được nêu ra. Pin của host cũ vẫn tồn tại,
  nên cú click đó không ghi đè gì; người dùng chỉ trust thêm một máy nữa, với fingerprint
  của nó ngay trước mắt.

- **Một tên thiết bị duy nhất**: Settings → General → *Device name* (để trống nghĩa là
  dùng tên của OS) là tên duy nhất của một máy — host hiển thị nó cho viewer và gửi nó tới
  các client mà host cho vào, client gửi nó khi connect, nó là nhãn của public key mà máy
  copy ra, và nó là nhãn mà host ghi vào `authorized_keys` khi approve yêu cầu của máy
  hoặc cho máy vào bằng token QR. Trang Client bỏ ô nhập tên riêng
  để tên mà host nhìn thấy và nhãn trong `authorized_keys` của nó luôn khớp nhau.

- **Không chuyển đổi dữ liệu cũ**: passcode, danh sách `paired_devices` cũ và dấu kích
  hoạt của nó không được chuyển đổi — không thứ gì trong đó chứng minh được client giữ
  một key — và các file còn sót lại bị xoá. Các file `client_key.pem`,
  `client_key.<name>.pem` và `host_cert.pem` của 7.0.x đơn giản bị bỏ qua: key của máy vốn
  đã là `host_key.pem`, nên fingerprint không đổi, và danh tính Ed25519 cũ của client không
  được mang sang danh tính mới — chủ host cho phép key của máy một lần, bằng Approve hoặc
  QR. File `authorized_keys` hoặc `known_hosts`
  không đọc được sẽ không bao giờ bị suy đoán: trong khi file không đọc được, host từ chối
  mọi client và client từ chối mọi host, và lần thay đổi tiếp theo sẽ ghi mới file đó.

- **Những gì một màn hình desktop hiển thị là dữ liệu trong `core/ui`, không phải code
  riêng của từng app**: bộ màu (`Theme.h`, mỗi màu có một giá trị sáng và một giá trị tối),
  các cột và kích thước của bảng host (`HostRows.h`), cùng các khung, section và thứ tự của
  trang Settings (`SettingsLayout.h`) chỉ được định nghĩa một lần. Windows đọc trực tiếp,
  macOS đọc qua `dh_theme_color`, `dh_host_columns` và `dh_settings_layout`, Android đọc qua
  `dh_theme_color`. Mỗi app chỉ quyết định control nào vẽ một `SettingField`. Trước đây mỗi
  app giữ một bản sao riêng của cùng các mã màu và thứ tự trang, nên chúng lệch dần: một
  nút đỏ trên Windows lại xám trên macOS, một setting có trên hai desktop lại thiếu trên
  desktop thứ ba. Test của layout kiểm tra mỗi setting được lưu xuất hiện đúng một lần, nên
  không app nào còn lặng lẽ bỏ sót được nữa.

- **Một capability probe trả về false có thể vô hiệu hoá cả một vòng điều khiển.** Encoder
  Media Foundation trả về `false` cho `SetBitrate` mỗi khi MFT không cung cấp
  `CODECAPI_AVEncCommonMeanBitRate`, và `ApplyFeedback` xử lý việc từ chối này đúng theo
  nghĩa "không có thay đổi nào được áp dụng". Trên một MFT Intel Quick Sync báo
  `MeanBitRate: NOT SUPPORTED`, kết quả là host không bao giờ thay đổi bitrate: đo trên
  chính phần cứng đó, 30 giây với mức mất gói 29-40 % liên tục không tạo ra quyết định
  `Bitrate` nào, và quality ladder cũng không thay đổi. Log khởi động hiển thị
  `NOT SUPPORTED` trong suốt thời gian đó nhưng không được hiểu là cơ chế thích ứng đã
  ngừng hoạt động. `SetFps` và `RequestKeyFrame` trong cùng file vốn đã có đường lui về
  `ReinitTransform()`; `SetBitrate` là trường hợp duy nhất không có, và nay đã được bổ
  sung tương tự: `ConfigureTransform` ghi `MF_MT_AVG_BITRATE` từ `cfg`, nên một lần dựng
  lại sẽ áp dụng tốc độ mới. Việc dựng lại tiêu tốn một IDR, vì vậy đường `codecapi` trực
  tiếp vẫn được thử trước. Khi một capability theo từng thiết bị kiểm soát một đầu vào
  điều khiển, cần bắt buộc có đường lui: chấp nhận hiệu năng thấp hơn là một lựa chọn,
  nhưng âm thầm vô hiệu hoá hoàn toàn thì không.

- **Một bên gửi không theo kịp có biểu hiện giống hệt một đường truyền không lỗi.** Mọi
  đầu vào của `BitrateController` — loss, RTT, tốc độ nhận — đều đến từ viewer, nên không
  thành phần nào trong vòng điều khiển xác định được rằng chính bên gửi đang chậm. Đo trên
  một Pixel 4 làm host cho hai viewer: frame rời encoder khi đã cũ 15 giây, trong khi
  viewer báo 0 % loss và RTT 15 ms; controller hiểu đó là dư địa và nâng bitrate trở lại
  mức trần 20 Mbps. Đây là hiện tượng bufferbloat bên trong bên gửi: đường truyền càng có
  vẻ tốt thì lượng dữ liệu đẩy ra càng nhiều. Hiện host đo tuổi của frame tại bước gửi và
  đưa giá trị này vào cùng các số liệu của viewer: vượt `kBacklogMs` thì giảm tương đương
  mức 2 % loss, vượt `kSevereBacklogMs` thì tương đương 5 % loss, và cả hai đều chặn việc
  tăng trở lại trong hai giây theo thông lệ. Bitrate vẫn là biến điều khiển duy nhất, nên
  `QualityLadder` giảm theo sau và trần fps điều chỉnh tương ứng. Một vòng điều khiển chỉ
  nhận dữ liệu từ đầu bên kia sẽ không quan sát được nửa pipeline mà nó trực tiếp quản lý.

- **Giới hạn fps chỉ có tác dụng ở nơi thực sự có frame bị loại bỏ.** Nấc fps của ladder
  là một yêu cầu, và mỗi nền tảng phải thực hiện nó tại một điểm có thể bỏ frame. Windows
  và Linux thực hiện tại bước capture bằng `FrameGate`; Android giới hạn đầu vào của
  MediaCodec bằng `max-fps-to-encoder`; macOS cấu hình lại frame interval của
  ScreenCaptureKit. iOS không có điểm tương ứng: ReplayKit cung cấp frame theo tốc độ màn
  hình, còn `VtEncoder::SetFps` chỉ đặt `kVTCompressionPropertyKey_ExpectedFrameRate`, vốn
  là một gợi ý cho rate control và không loại bỏ frame nào. Việc đổi nấc ở đó chỉ cấu hình
  lại encoder mà không thay đổi số frame nó phải xử lý. Hiện `OfferVtFrame` chạy cùng
  `FrameGate` đó cho cả hai app Apple, sau khi cache idle-flush được làm mới để một màn
  hình tĩnh vẫn còn frame để gửi lại. Khi một tham số tồn tại trên mọi nền tảng, cần kiểm
  tra cách từng nền tảng xử lý nó trước khi dựa vào ladder.

- **Send pacer phải cao hơn đáng kể so với tốc độ đầu ra của encoder.** `Pacer::Gate` chờ
  trên chính thread mà `SendEncodedFrame` đang chạy, và trên Android đó là vòng drain của
  MediaCodec, tức vòng phải gọi `releaseOutputBuffer` trước khi encoder có thể cung cấp
  frame tiếp theo. Do đó pacing quyết định cả tốc độ drain chứ không riêng tốc độ trên
  đường truyền, trong khi VirtualDisplay vẫn tiếp tục đưa frame mới vào theo tốc độ màn
  hình. Việc giảm `kPacingRateMultiple` từ 2 xuống 1.2 nhằm làm mượt các đợt gửi dồn đã
  được đo trên một Pixel 4: mức burst trên mỗi frame tăng từ 20 ms lên 63 ms ở giá trị
  trung vị, và lượng tồn đọng của encoder tăng không giới hạn — `enc_lat_ms` vượt 46 giây
  trong 100 giây, và viewer chậm 4,6 giây. Ở mức 2, cùng lượt chạy giữ `enc_lat_ms` ở 0.
  Khoảng dư này không phải phần có thể cắt giảm; nó là điều kiện để pipeline encode được
  giải phóng nhanh hơn tốc độ nạp vào. Cần xử lý các đợt gửi dồn bằng socket buffer hoặc
  bằng cách chuyển pacing ra khỏi thread drain, không phải bằng cách giảm giá trị này.

- **Perf suite kiểm theo chi phí, nên cần thêm một tiêu chí kiểm theo kết quả.**
  `core_perf` đo số allocation trên mỗi packet và cách thời gian tăng theo input, và toàn
  bộ workload reassembler của nó đều đạt trong khi một packet bị mất đang làm loại bỏ 22 %
  số frame còn nguyên vẹn trên một đường truyền thực tế. Suite này không thể phát hiện vấn
  đề đó: loại bỏ video hợp lệ có chi phí *thấp hơn* việc decode nó, nên chính sách sai lại
  đạt điểm tốt hơn trên mọi chỉ số mà suite theo dõi. `LossGoodputTests` là bài kiểm bổ
  sung, fail khi mã thực hiện ít công việc hơn mức cần thiết: nó mô phỏng một đường truyền
  mất gói ở phần đuôi với round trip thực tế, và kiểm theo tỷ lệ số frame có đủ packet mà
  thực sự tới được decoder, cùng khoảng trống dài nhất giữa hai frame được giao. Cả hai
  đều độc lập với phần cứng, nên kết quả nhất quán trên laptop, CI runner và điện thoại.
  Khi một chính sách có thể "thành công" bằng cách bỏ bớt công việc, cần bổ sung một tiêu
  chí kiểm theo goodput.

- **Một packet bị mất chỉ nên làm mất một frame, không phải toàn bộ hình ảnh cho tới
  keyframe tiếp theo.** Reassembler trước đây bật `waitingForIdr_` ở mọi lần mất gói, nên
  một packet thiếu làm loại bỏ mọi frame *hoàn chỉnh* theo sau cho tới khi có IDR mới. Đo
  trên một host là điện thoại qua Wi-Fi, điều này biến 64 frame thực sự không hoàn chỉnh
  thành 381 frame bị loại bỏ: 6,4 MB video có thể decode bị bỏ đi, và hình ảnh đứng yên
  trung vị 146 ms, cao nhất là 1,4 giây mỗi lần. Hiện chỉ frame không hoàn chỉnh bị loại
  bỏ; các frame sau đó được đưa thẳng tới decoder, decoder che phần reference bị thiếu
  trong khi `InvalidateRef` thông báo frame lỗi cho host và yêu cầu keyframe khắc phục.
  Một vài vệt macroblock ngắn là chi phí chấp nhận được để tránh đứng hình.
  `waitingForIdr_` vẫn được giữ cho trường hợp duy nhất mà nó đúng: một viewer tham gia
  giữa chừng không có reference nào và phải chờ IDR đầu tiên.

- **Khoảng chờ trước khi coi là mất gói phải dài hơn một lần retransmit, nếu không NACK
  không có tác dụng.** Trước đây một frame chỉ được chờ hai khoảng frame (33 ms ở 60 fps)
  trước khi bị coi là mất, trong khi RTT đo được trên cùng đường truyền là 24-49 ms. NACK
  được gửi đi nhưng phản hồi về tới nơi sau khi frame đã bị loại bỏ, thể hiện qua
  `late_ms_avg=24` cùng 87 packet mỗi giây thuộc về những frame không còn tồn tại. Hiện
  `StallTimeoutUs` lấy giá trị lớn hơn giữa khoảng chờ theo pacing và một lần rưỡi round
  trip, vẫn bị giới hạn bởi timeout cứng, nên việc yêu cầu retransmit chỉ diễn ra trên
  những đường truyền thực sự cần.

- **Performance suite kiểm theo số allocation và hình dạng chi phí, không theo mili-giây.**
  Ba test suite được build ở chế độ debug, và CI chạy lại chúng dưới ASan, TSan và
  coverage, nơi một ngưỡng thời gian thực tế đo sanitizer chứ không đo mã nguồn. Vì vậy
  `core_perf` (preset release, `make test-perf`) fail theo hai tiêu chí độc lập với phần
  cứng: số allocation trên mỗi packet, frame hoặc KB, đếm bằng cách thay thế `operator
  new` toàn cục; và một dòng `-scaling` có thời gian tăng nhanh hơn input rất nhiều. Phần
  đo thời gian được giữ như một phép so sánh với `out/perf/baseline.txt`, ghi theo từng
  máy bằng `make perf-baseline` và không được commit. Cách phân chia này cho phép suite
  phát hiện một hồi quy dạng "reassembler nay copy mỗi mảnh hai lần" trên laptop, CI runner
  và điện thoại như nhau, đồng thời vẫn in ra ns trên mỗi đơn vị và MB/s cho những đường mà
  bản thân con số là thông tin cần thiết. CI chạy hai tiêu chí độc lập với phần cứng này
  trên các job release Linux và macOS; Windows chỉ build binary, vì deque của MSVC allocate
  một block cho mỗi phần tử với kích thước lớn hơn 16 byte, nên cùng một đoạn mã có số
  allocation khác. Pull request còn nhận một phép so sánh thời gian không bị ảnh hưởng bởi
  nhiễu của runner dùng chung: base commit và pull request được đo trên cùng một runner,
  dung sai 50 %, chỉ ở mức cảnh báo. `platform_perf` mở rộng các tiêu chí này sang QUIC
  thật qua loopback, nơi thời gian thực tế phản ánh nhịp của service loop — hạn mức rút
  stream 64 KiB nhân với nhịp poll 1 ms — nên một hạn mức bị thu hẹp, một thao tác drain
  không còn tuyến tính, hoặc một allocation mới trong vòng poll đều xuất hiện dưới dạng một
  bước nhảy, dù chi phí CPU của cùng khối lượng công việc gần như không đổi.

- **`QuicEndpoint::Poll` không bao giờ kết thúc bằng một lần đọc chặn.** Nó chỉ chờ gói
  đầu tiên — 1 ms khi còn backlog, chính là nhịp điều tiết các đợt flush và các lần drain
  stream, còn không thì chờ đúng khoảng mà bên gọi yêu cầu — và chỉ lấy từng gói tiếp theo
  sau khi `WaitReadable(0)` báo đã có gói. Cách đọc cho đến khi hết receive timeout khiến
  mỗi lần `Poll` phải trả thêm một lần timeout, dưới `sendMutex_`, sau gói cuối cùng.
  Linux giữ đúng timeout 1 ms; trên Windows cùng lần chờ đó tốn hàng chục mili giây, và
  `platform_tests` mất 700 s ở đó so với 100 s trên Linux. Vì vậy `Poll` với thời gian chờ
  0 hoàn toàn không chờ trên một link đang rảnh: bên gọi poll trong vòng lặp phải chờ
  trước (`WaitReadable`, không giữ lock) hoặc truyền vào một khoảng chờ, như
  `platform_perf` đang làm. Tiêu chí drain-scaling của nó so sánh hai kích thước đều vượt
  hạn mức drain 64 KiB, vì một kích thước nhỏ hơn hoàn tất mà không tốn nhịp nào và khiến
  kích thước lớn trông như vượt tuyến tính.

- **`FileHost` không gửi dữ liệu khi đang giữ lock của chính nó.** QUIC service loop chạy
  `QuicEndpoint::Poll` dưới `SessionTransport::sendMutex_`, và một connection đóng lại tại
  đó sẽ gọi trực tiếp vào `FileHost::OnPeerGone`, hàm này lấy `FileHost::mutex_`. Như vậy
  thứ tự `sendMutex_ -> mutex_` đã được transport quy định. Bất kỳ đường nào lấy `mutex_`
  trước rồi mới gửi — `FileReceiver` phát ra accept, ack hoặc cancel qua
  `hooks.send` — đều tạo thành chu trình khoá, và TSan phát hiện đây là lock-order
  inversion giữa vòng nhận và một thread UI đang chuyển `SetAccepting(false)` trên một
  phiên truyền đang hoạt động. Vì vậy các record do receiver phát ra được đưa vào `outbox_`
  dưới `mutex_` và chỉ được gửi sau khi nhả mutex, với `outboxMutex_` giữ xuyên suốt cả hai
  giai đoạn để peer nhận được chúng theo đúng thứ tự phát sinh. `OnPeerGone` không gửi dữ
  liệu: nó vốn đã chạy dưới `sendMutex_`, nên nó loại bỏ những gì đã xếp hàng.

- **Command line client là giao diện thứ tư, không phải một bản triển khai thứ hai.** Nó
  parse cờ trong `core/cli`, sau đó điều khiển đúng các thành phần mà app desktop điều
  khiển: `SharingHost` để host, `ScreenViewer` để xem, `TerminalViewer` để mở shell. Phần
  duy nhất thuộc về riêng nó là cửa sổ hiển thị: X11 và EGL trên Linux, chính `RunViewer`
  của app desktop trên Windows. Đây là lý do cây `cpp/` của mỗi client là một thư viện tĩnh
  (`deskhub_linux_core`, `deskhub_win_core`, `deskhub_win_view`, `deskhub_mac_core`) và mã
  GUI nằm bên trên: cách phân chia này cho phép CLI link phần media pipeline mà không phải
  link GTK hay wxWidgets.

- **`preflight` chỉ chạy khi có màn hình cần capture.** Mọi client dùng nó để kiểm tra
  đường capture: xdg portal trên Linux, quyền Screen Recording trên macOS, một thiết bị
  D3D11 trên Windows. Một phiên share chỉ gồm shell không cần các điều kiện đó, nên việc
  kiểm tra bắt buộc đã khiến `share --terminal` trên một máy không có màn hình báo lỗi
  thiếu permission screen-capture. Hiện `HostEngine::Start` bỏ qua bước này khi danh sách
  source rỗng.

- **Một host có shell nhưng không có màn hình vẫn tiếp tục hoạt động.** Net loop kết thúc
  session khi không còn source nào hoạt động, mà một phiên share chỉ có terminal thì theo
  định nghĩa không có source. `keepAlive` được xác định từ ý định của bên gọi
  (`ShareOptions::terminal`), không phải từ con trỏ `TerminalHost` vốn chỉ được gắn sau khi
  vòng lặp đã chạy.

- **Frame gate đếm tới một thời điểm đến hạn, không đếm từ frame gần nhất được giữ lại.**
  Một compositor cung cấp 40 fps trong khi mục tiêu là 30 fps sẽ không có frame tại phần
  lớn các mốc 33 ms, nên một gate chỉ kiểm tra khoảng cách so với frame vừa giữ sẽ loại bỏ
  cứ một frame lại một frame và ổn định ở 20 fps: thấp hơn mục tiêu và không đều, tức là
  judder chứ không phải một stream chậm hơn. `FrameGate` thay vào đó duy trì một thời điểm
  đến hạn chạy dần: mỗi lần chấp nhận sẽ đẩy thời điểm này lên đúng một khoảng, nên phần dư
  được giữ lại và 40 frame đầu vào cho ra 30 frame đầu ra. Một lượt capture chậm hơn mục
  tiêu không bị lược bớt, và một thời điểm đến hạn đã chậm hơn thời gian thực sẽ đồng bộ
  lại thay vì tích luỹ, nên một khoảng thời gian ít hoạt động không tạo ra đợt dồn về sau.

- **Host Linux encode trên thread riêng và nhận frame đã được thu nhỏ, không phải frame
  đầy đủ.** Việc encode ngay trong callback `process` của PipeWire giới hạn capture ở mức
  `1000 / enc_ms` fps và biến mọi dao động thời gian encode thành dao động nhịp frame ở
  phía client. Hiện phần encode chạy trên thread riêng, nhận dữ liệu qua `FrameMailbox`,
  một hàng đợi một ô theo nguyên tắc giữ frame mới nhất: khi encoder chậm lại, frame mới
  nhất được giữ và frame cũ được đếm thay vì xếp hàng. Dữ liệu đi qua hàng đợi là frame đã
  downscale về kích thước encode, tương đương khoảng một phần bảy số byte. Việc copy frame
  ở độ phân giải đầy đủ tốn kém hơn nhiều so với bản thân thao tác copy: 20 MB cache line ở
  trạng thái dirty trong core thực hiện capture, mà core thực hiện encode sau đó phải nạp
  về, đo được 16 ms so với 3,4 ms cho cùng lượng dữ liệu mà nó sở hữu. Thread capture dù
  sao cũng phải xử lý mỗi pixel nguồn một lần, nên đây là vị trí phù hợp để thực hiện lượt
  quét duy nhất đó. Frame dma-buf vẫn được encode tại chỗ: compositor tái sử dụng vùng nhớ
  của chúng ngay khi callback trả về nên chúng không tồn tại lâu hơn callback, và VA-API
  thực hiện scale trực tiếp trên GPU.
- **Host Linux chọn encoder theo vị trí của frame, không theo phần mềm đã cài.** Một frame
  dma-buf được chuyển tới VA-API, thành phần có thể import nó theo cơ chế zero-copy trên
  chính GPU đã tạo ra nó. Một frame đã map vào bộ nhớ CPU được chuyển tới NVENC khi có
  driver NVIDIA, vì trên một desktop do GPU NVIDIA render, compositor sẽ thương lượng lại
  screencast sang shared memory, và khi đó việc encode thuộc về card có thể đọc pixel trực
  tiếp từ bộ nhớ hệ thống. `HwEncoder` đưa ra quyết định này ở mỗi lần dựng lại encoder, và
  một frame thuộc loại còn lại tới sau sẽ trả về `false`, đây là tín hiệu cần dựng lại.
- **Phần downscale trước NVENC do dự án tự triển khai, không dùng swscale.** NVENC nhận
  pixel 32-bit dạng packed nhưng không thực hiện resize, trong khi dữ liệu capture là màn
  hình ở độ phân giải đầy đủ. `libswscale` đo được 9,2 ms cho 3440x1440 → 1280x534, tương
  đương khoảng 2 GB/s, thấp hơn băng thông bộ nhớ của máy này một bậc độ lớn, vì phép
  rescale RGB dạng packed không nằm trong các đường đã tối ưu của nó. `RgbDownscale` trong
  `core/` là phép trung bình theo vùng viết riêng cho trường hợp này: mỗi pixel nguồn một
  lần load 32-bit, cộng dồn bằng số nguyên, đo được 4,0 ms cho cùng frame, và cho kết quả
  khử răng cưa đúng thay vì phép lấy mẫu bilinear một tap của swscale. Chi phí NVENC cho cả
  frame vào khoảng 5 ms, nên 60 fps vẫn còn dư địa.
- **Số liệu hiệu năng chỉ có ý nghĩa khi lấy từ bản release.** `make build-linux` và
  `make run-linux` cấu hình preset `x64-debug`, tức `-O0`, trong khi đường encode hiện là
  phép tính trên pixel nằm trong `core/`. Cùng một frame tốn khoảng 19 ms ở đó so với
  khoảng 5 ms từ `make release-linux`. Một báo cáo judder đo trên binary debug thực chất
  đang đo loại build.

- **Viewer trên Apple điều tiết video theo PTS trên một control timebase, và pacer không
  tự tin tưởng kết quả của chính nó.** Việc hiển thị mọi frame ngay khi nhận được làm
  jitter thời điểm đến của Wi-Fi biểu hiện thành judder, trong khi mọi chỉ số latency vẫn
  ở mức tốt: nhịp hiển thị không phải là latency. `VideoPacer` (thuộc core, có test
  offline) ánh xạ PTS của host sang thời gian hiển thị cục bộ theo cùng cách mà chỉ số e2e
  sử dụng — giá trị nhỏ nhất của `arrival − pts` trong một cửa sổ trượt — cộng thêm khoảng
  dẫn khoảng 33 ms để bù jitter, và `VtDecoder` điều khiển một control timebase của
  `AVSampleBufferDisplayLayer` từ giá trị đó, chỉ đồng bộ lại khi lệch quá 250 ms. Một bước
  nhảy pts lớn hơn 2 giây được hiểu là một stream mới chứ không phải jitter, nên phép ánh
  xạ được khởi tạo lại thay vì đứng yên trong một khoảng. Vì không thể xác nhận từ phía
  ứng dụng rằng renderer tuân thủ một timebase bên ngoài trên mọi phiên bản OS, decoder tự
  kiểm tra: một chuỗi frame đã điều tiết bị hàng đợi renderer đầy loại bỏ sẽ khiến nó
  chuyển về chế độ hiển thị ngay và thực hiện flush, chấp nhận mất phần làm mượt thay vì
  mất hình ảnh.

- **Audio truyền mỗi datagram một frame, và packet bị mất không được yêu cầu gửi lại.**
  Một frame Opus 20 ms ở 64 kbps có kích thước khoảng 160 byte, tối đa 209 byte, so với
  1180 byte mà một datagram chứa được. Do đó đường audio không có packetizer, không FEC,
  không reassembler và không NACK, tức là phần lớn những gì đường video có. Mất mát được
  xử lý ở nơi chi phí thấp nhất: Opus mang FEC in-band trong frame kế tiếp, và bên nhận
  yêu cầu decoder che khoảng trống mà jitter buffer báo về. Việc retransmit không mang lại
  lợi ích, vì một frame tới trễ 200 ms vừa không phát được vừa làm chậm mười frame sau đó.
  `make opus-smoke` đo các số liệu này trên bất kỳ máy nào build được thư viện.
- **Jitter buffer không chứa timer.** `AudioJitterBuffer` chỉ gồm state, và độ trễ mục
  tiêu đơn giản là số frame cần tích luỹ trước khi bắt đầu phát: 60 ms tương ứng ba frame.
  Nhờ đó toàn bộ thành phần này test được offline mà không cần chờ thời gian thực, và các
  trạng thái lỗi trở nên rõ ràng: một đợt dồn bị giới hạn thay vì xếp hàng, một buffer rỗng
  sẽ nạp lại thay vì phát ngắt quãng, và một bước nhảy số thứ tự được hiểu là stream mới
  chứ không phải hàng nghìn frame bị mất. Phần điều tiết nằm trong `AudioPlayer`, đưa một
  frame mỗi 20 ms thời gian thực vào một ring PCM mà callback render của sink đọc ra.
- **Callback capture không thực hiện encode.** PipeWire và ScreenCaptureKit cung cấp audio
  trên các thread real-time với deadline vài mili-giây, và việc vượt deadline tại đó gây
  xrun cho chính phần phát âm thanh của host, không chỉ của Deskhub. Opus encode mất
  0,3–1,5 ms kèm các đỉnh, và trước đây mỗi viewer còn có một lệnh `sendto` chạy tiếp sau
  trên cùng thread. Hiện `AudioBroadcaster::Offer` chỉ copy frame 20 ms vào một ring ô
  lock-free đã cấp phát sẵn và ghi nhận thời điểm capture; một thread worker thực hiện
  phần encode, phần chẩn đoán và các lượt gửi theo từng viewer. Khi worker không theo kịp,
  hệ quả là một lần drop được đếm (`framesRefused`), không phải hiện tượng nhiễu trong âm
  thanh của host.
- **Âm thanh cần cả hai phía cùng bật.** Viewer đặt bit 0 của `Hello.features`, host công
  bố `kHostSharesAudio` trong phần capability, và host chỉ gửi packet tới những viewer có
  bit này được đặt.
- **Protocol version 3 chỉ nói chuyện với chính nó.** `kProtocolVersion` lên 3 khi `Hello`,
  `LIST_SOURCES` và `TERM_OPEN` bỏ các byte passcode mà cơ chế admission đã khiến chúng vô
  dụng, và `Hello`/`HELLO_ACK` bỏ phần thương lượng codec mà việc chỉ stream H.264 chưa bao
  giờ dùng tới. Mọi parser giờ đòi đủ layout hiện tại — không còn dạng ngắn kiểu cũ, không
  còn byte đệm reserved — và `ClassifyPacket` chỉ nhận đúng version hiện tại, nên peer
  phiên bản cũ bị loại bỏ thay vì được hiểu nửa vời. Thay đổi trên wire thì tăng version,
  không bao giờ thêm nhánh tương thích.

- **Link terminal tự duy trì và tự kết nối lại.** Một terminal viewer sở hữu connection
  QUIC riêng, tách biệt với session video, nên không keepalive nào của đường video tới
  được nó. Khi không có thao tác tại dấu nhắc, connection này không có lưu lượng và bị
  đóng do idle timeout 30 giây của QUIC; sau đó viewer dừng thread ở trạng thái
  `Reattaching` mà không kết nối lại, trong khi shell vẫn được host giữ trong đủ 2 phút.
  Hiện `TerminalViewer` gửi một packet ack-eliciting theo timer và kết nối lại với backoff,
  sử dụng lại `TerminalClient::Reattach()` (vốn đã được viết và test trong core nhưng chưa
  từng được gọi), nhờ đó cùng một shell được khôi phục kèm scrollback.
  `deskhub::KeepaliveIntervalUs` và `ReconnectDelayUs` giữ các mốc thời gian trong core:
  keepalive tối đa bằng một nửa idle timeout để chịu được việc mất một packet, và việc thử
  lại dừng đúng tại `kTerminalReattachGraceUs`: sau mốc đó cửa sổ báo mất kết nối, nhưng bản thân shell vẫn ở trên host không giới hạn thời gian, sẵn sàng cho một lần resume tường minh thay vì bị huỷ.
- **Một record được đưa lên stream trọn vẹn hoặc không đưa, và một client bị chậm sẽ được
  vẽ lại thay vì nhận lại toàn bộ byte.** Mọi dữ liệu tin cậy — control, auth, output
  terminal — đều là các record có length prefix dùng chung một QUIC stream, nên một record
  chỉ được gửi một phần sẽ làm lệch framing ở đầu kia vĩnh viễn; `RecordStream` không có cơ
  chế đồng bộ lại và peer đóng connection. `QuicEndpoint::SendStream` trước đây ghi phần
  vừa đủ và bỏ phần còn lại, cách này chỉ đúng cho tới khi một lệnh như `make test` tạo ra
  lượng output vượt khả năng của link: cửa sổ stream 1 MiB bị đầy, phần cuối của một record
  `TermData` bị bỏ, framer của viewer fail và shell bị ngắt sau khi mở một phút. Hiện nó từ
  chối một record khi stream không còn đủ chỗ, và đóng connection nếu vẫn xảy ra một lần
  ghi một phần, vì một stream đã lệch framing không thể khắc phục tại chỗ. Bên trên,
  `TerminalHost` giữ output chưa gửi trong một hàng đợi theo từng shell và thử lại ở mỗi
  tick, nên một đợt output tạm thời vượt khả năng của link — chẳng hạn output của một lần
  build — vẫn tới được client đầy đủ. Khi vượt `kMaxPendingBytes`, hàng đợi bị loại bỏ thay
  vì tiếp tục tăng: mọi byte đều đã tới `Screen` mirror phía host, nên client được đồng bộ
  bằng `deskhub::term::RenderScreen`, tức vẽ lại lưới hiện thời một lần, tối đa mỗi
  `kRepaintIntervalUs`. Output mà người dùng không kịp đọc được bỏ qua thay vì đệm lại, nhờ
  đó một lệnh tạo output liên tục chạy ở tốc độ của nó và vẫn để lại đúng nội dung màn hình
  cuối. Một client đang reattach cũng nhận cùng thao tác vẽ lại, vì vị trí của nó trong
  byte stream không còn ý nghĩa sau một khoảng gián đoạn.
- **Một phiên share tự động chờ desktop thay vì liệt kê một lần duy nhất.** Windows đăng ký
  autostart dưới dạng một scheduled task `ONLOGON`, chạy trước khi session có màn hình để
  liệt kê, nên một lệnh `ListDisplays()` tại thời điểm khởi tạo trước đây trả về rỗng và
  app báo rằng không có nội dung nào để share. `deskhub::ui::AutoShareGate` (thuộc core, có
  unit test) chứa quy tắc thử lại — probe mỗi `kAutoShareProbeMs`, dừng sau
  `kAutoShareGiveUpMs` — và mỗi client điều khiển nó bằng timer riêng, nên quy tắc chỉ tồn
  tại một lần. `NextAutoShareStep` là cùng quy tắc đó ở dạng không giữ state, và đây là
  thành phần mà client Swift sử dụng qua `dh_auto_share_step`. Một phiên share tự động
  không mở hộp thoại modal: tại thời điểm đăng nhập, cửa sổ có thể đang ẩn trong tray, nơi
  một hộp thoại vừa không hiển thị vừa chặn phiên share vô thời hạn, nên các trường hợp từ
  chối được đưa vào banner ở trang Host và vào log. Các client desktop cũng làm mới danh
  sách chọn theo tín hiệu thay đổi display của OS, nhờ đó danh sách vẫn chính xác khi một
  màn hình được kết nối sau.
- **Chọn quiche thay vì msquic hoặc ngtcp2.** Đây là thư viện QUIC duy nhất có bằng chứng
  sử dụng trong môi trường production trên cả Android và iOS. Nó đi kèm BoringSSL, thành
  phần cũng phục vụ key của máy, certificate trong bộ nhớ của nó và chữ ký transcript, nên
  không cần thư viện mật mã thứ hai.
- **Không sử dụng connection migration.** Không thư viện ứng viên nào có hỗ trợ phía client
  dùng được. Cơ chế reconnect và reattach (tương tự tmux, vốn đã cần thiết cho việc app di
  động chạy nền) đã đáp ứng yêu cầu này; các shell đang được giữ cũng có thể được liệt kê (`TermList`) và resume theo id từ một client mới.
- **Sử dụng ECDSA P-256 thay vì Ed25519.** Phía server của BoringSSL không ký TLS
  handshake bằng Ed25519 thông qua quiche, và giờ một key phải phục vụ cả TLS lẫn chữ ký
  phía client. Một key đã lưu mà không phải key P-256 khiến host không khởi động và giữ
  nguyên file. Chỉ khi chưa có `host_key.pem`, ứng dụng mới tạo identity mới, nên
  fingerprint hiện có không bao giờ tự đổi.
- **quiche được build sẵn, không dùng FetchContent.** `scripts/build-quiche.sh` tạo một
  thư mục cho mỗi rust target dưới `third_party/quiche/` cùng một thư mục `include/` dùng
  chung, chứa quiche.h và các header BoringSSL do boring-sys cung cấp. Các header này được
  sao chép ra ngoài vì Deskhub gọi trực tiếp BoringSSL cho phần host identity và cần đúng
  một include path, không có thư viện TLS thứ hai. `DeskhubQuiche.cmake` chuyển phần này
  thành `deskhub::quiche`; thiếu thư viện sẽ làm fail bước configure.
- **Apple link `libplatform_bundled.a`.** Các app Xcode sử dụng archive platform từ bên
  ngoài CMake, nơi một lần link PRIVATE tới quiche không xuất hiện trong dòng link của
  chúng. Vì vậy một bước `libtool` gộp platform và quiche thành một archive duy nhất mà
  `.pbxproj` link tới.
- **Các vấn đề của toolchain Windows đã được xử lý và cần giữ nguyên trạng thái này.**
  quiche được build với CRT tĩnh thông qua
  `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS` cho phần object Rust, cùng `/MT` trong
  `CFLAGS_x86_64_pc_windows_msvc` cho phần object BoringSSL (mặc định của msvc là runtime
  dạng DLL, và việc truyền cờ này qua một `RUSTFLAGS` chung sẽ làm hỏng bản build cargo).
  Toàn bộ cây pin `MultiThreaded` để khớp, nhờ đó file exe phát hành không cần VC++
  Redistributable. wxWidgets pin lại `wxBUILD_USE_STATIC_RUNTIME` ở mỗi lần configure vì
  `wx_option()` lưu cache vĩnh viễn. BoringSSL phải được build dưới generator Visual Studio
  mặc định: ở đó cmake crate chỉ truyền được /MT thông qua các cờ theo từng config, nên đặt
  `CMAKE_GENERATOR=Ninja` sẽ đưa BoringSSL về /MD và bước link cuối cùng fail với LNK2038.
  Nếu MSBuild báo MSB6003 do đường dẫn dài, cần bật hỗ trợ long path của Windows.
  `/usr/bin/link.exe` của Git Bash che mất linker của MSVC, cần đặt thư mục chứa `cl.exe`
  lên trước; cơ chế viết lại đường dẫn của Git Bash làm hỏng các tham số dạng `/`, cần dùng
  `MSYS2_ARG_CONV_EXCL`; và installer của NASM không cập nhật PATH.
- **quiche cho Android không dùng cargo-ndk trên máy host Windows.** cargo-ndk cung cấp
  cho boring-sys một đường dẫn `clang` không có phần mở rộng, điều mà CMake không chấp nhận
  trên Windows. Vì vậy `build-quiche.sh` tự đặt `CC_*`, `CXX_*`, `AR_*`, linker của cargo
  và `--target=` cho ABI tương ứng, rồi gọi cargo trực tiếp. BoringSSL vẫn cần Ninja ở đây
  vì generator Visual Studio không nhắm được NDK, còn bindgen sử dụng libclang của Visual
  Studio, vốn tìm `stddef.h` cạnh binary của chính nó; `BINDGEN_EXTRA_CLANG_ARGS` trỏ nó
  tới các resource header của NDK bằng dấu gạch chéo xuôi, vì bindgen tách biến này theo
  quy tắc shell và loại bỏ dấu gạch chéo ngược.
- **Mọi app cross-compile đều build quiche của nó trước.** `build-android`, `build-ios`,
  `build-macos` và `build-linux` đều phụ thuộc vào một quiche target cho ABI tương ứng,
  giống như `debug` và `release` phụ thuộc vào ABI của host. quiche được build theo từng
  ABI và bước configure của CMake fail nếu thiếu, nên một bản build bỏ qua bước này có biểu
  hiện giống lỗi toolchain hơn là thiếu thư viện. Ngoài ra, một app giữ nguyên ở lần build
  thành công trước đó sẽ sử dụng một protocol mà các máy khác không còn hỗ trợ.
- **quiche cho iOS pin `IPHONEOS_DEPLOYMENT_TARGET=17.0`.** clang của boring-sys sử dụng
  giá trị mặc định của SDK trong khi rustc link theo mức tối thiểu của chính nó, và sự
  không khớp này biểu hiện thành lỗi `___chkstk_darwin` không xác định tại bước link.
- **Hai clock, theo chủ đích.** `NowUs()` là clock monotonic (số giây kể từ khi khởi động)
  dùng cho các khoảng thời gian; `NowUnixSeconds()` là clock duy nhất có thể hiển thị dưới
  dạng ngày tháng. Việc dùng lẫn hai clock không gây lỗi rõ ràng: một mốc monotonic đã lưu
  sẽ hiển thị thành một thời điểm vào ngày 1 tháng 1 năm 1970.
- **Tiến trình con PTY trên Windows không nhận handle chuẩn nào.** Khi stdout của host bị
  redirect, Windows chuyển tiếp redirect đó xuống dưới, vượt qua cả thuộc tính
  pseudo-console, và shell giao tiếp với pipe. Chỉ khi không truyền handle nào, shell mới
  quay lại sử dụng ConPTY đang gắn.
- **Lưới terminal trên Windows cần `wxWANTS_CHARS`.** Nếu thiếu, cơ chế điều hướng hộp
  thoại của frame sẽ nhận Enter, Tab và các phím mũi tên trước khi terminal xử lý.
- **TCC của macOS gắn việc cấp quyền với chữ ký mã.** Một bản app.app build tại máy
  (ad-hoc, ký lại ở mỗi lần build) và bản dmg Developer ID dùng chung một mục
  `com.deskhub.macos`: System Settings hiển thị quyền đã được cấp trong khi bản vừa chạy
  lại bị từ chối, và với Accessibility thì từ chối không kèm thông báo.
  `make reset-macos-permissions` xoá toàn bộ các lần cấp để lần chạy sau hỏi lại.
  Vì vậy bản Debug chạy với bundle id `com.deskhub.macos.debug` ("Deskhub Dev"), chỉ còn
  bản Release build tại máy là dùng chung mục đó với bản dmg.
- **Bản Debug không bao giờ đụng tới bản release đã cài**: mọi bản Debug của app và CLI
  (`DESKHUB_DEV_BUILD`, do CMake đặt cho cấu hình Debug) lưu dữ liệu trong `~/.deskhub-dev`
  thay vì `~/.deskhub`. Nếu không, khi chạy bản build mới trên máy đang cài một bản release
  cũ, bản mới đã xoá danh sách client được phép của bản release đó (bản mới xoá các file cũ)
  và trên macOS còn tắt luôn app đang chạy vì dùng chung bundle id.
- **iOS đặt thư mục Deskhub sâu thêm một cấp bên trong App Group container**: thư mục dữ
  liệu phải là thư mục Deskhub sở hữu và giữ được ở quyền `0700`, còn thư mục gốc của
  container thuộc về iOS, và iOS từ chối đổi quyền của nó. Khi trỏ thẳng vào thư mục gốc,
  mọi lần đọc hoặc ghi key, `known_hosts` và `authorized_keys` đều lỗi "cannot be read"
  hoặc "could not be saved". App và broadcast extension đều đưa container cho
  `SetAppDataDirInside`, hàm này nối thêm đúng tên `.deskhub` (hoặc `.deskhub-dev`) mà bản
  desktop dùng, nên cả hai vẫn dùng chung một thư mục.
- **macOS được build ở dạng desktop trong CI và ở dạng đã ký khi release, không đồng thời
  cả hai.** `build-desktop` compile app với chữ ký ad-hoc ở mỗi lần push, nên một thay đổi
  Cocoa không build được sẽ fail ngay trên pull request tương ứng. `deploy` xử lý cùng app
  đó qua `release-macos`, tức đường fastlane gồm Developer ID, notarization và dmg, tạo ra
  bản mà người dùng mở được. Vì vậy workflow dùng chung bỏ qua job macOS khi `for_release`
  được đặt; nếu không, một tag sẽ tiêu tốn thêm một runner macOS để tạo ra một bundle không
  được phát hành. `build-mobile` chỉ bao gồm iOS và Android, theo cùng lý do và cùng cách
  phân chia.
- **Mọi workflow lấy quiche và opus từ cùng một action, và cache key là toàn bộ điều kiện
  xác định.** `.github/actions/third-party` build cả hai thư viện cho các target mà một job
  chỉ định, nhờ đó mười chín bản sao của cùng một khối cache và build rút xuống còn một
  dòng cho mỗi job. Input `cache-key` của action này là thành phần duy nhất ngăn hai job
  khôi phục nhầm thư viện của nhau. Hai tập target khác nhau là khác biệt, và hai image
  runner cùng build một triple cũng là khác biệt: một `libquiche.a` compile trên
  ubuntu-latest rồi khôi phục trên ubuntu-22.04 sẽ link tới đúng phiên bản glibc mà bản
  release muốn tránh. Mọi yếu tố làm thay đổi sản phẩm build đều phải nằm trong key đó.
- **Một CRT release tĩnh trên Windows, cho mọi configuration.** cargo build quiche với CRT
  release tĩnh (mặc định của msvc; không nên ép qua `RUSTFLAGS`, vì giá trị này lan sang
  proc-macro và làm hỏng cargo), và toàn bộ cây CMake pin `MultiThreaded` để khớp. Đây cũng
  là điều giữ cho app là một file exe duy nhất không cần VC++ Redistributable. Rust không
  cung cấp bản build với CRT debug, nên cấu hình Debug cũng phải khớp:
  `_ITERATOR_DEBUG_LEVEL=0`, `/U_DEBUG`, loại bỏ `/RTC1`, vì CRT release không có
  `_CrtDbgReport` và không hỗ trợ run-time check. Mọi sai lệch đều dẫn tới một loạt lỗi
  LNK2038.
- **Passcode và scan LAN vẫn bị gỡ bỏ.** Mã 4 chữ số là một bí mật ngắn trên một port
  đang mở, và một phản hồi discovery không encrypt cho mọi người trên network biết có một
  host ở đó. Không gì trong 8.0 đưa hai thứ đó trở lại — mã QR được đọc từ màn hình, và
  một yêu cầu chỉ được ghi sau khi TLS handshake đã hoàn tất.

- **Approve đi trên kênh đã authenticate và hiển thị danh tính, không phải bí mật**: phản
  đối ngày 2026-09-28 với prompt phê duyệt là nó có thể bị người không đúng bấm cho máy
  không đúng — prompt passcode hiển thị một mã mà bất kỳ ai cũng có thể đã gõ. Một yêu cầu
  kết nối không hiển thị gì được gõ vào: tên thiết bị, fingerprint của key nó thực sự giữ
  (host đã hash key nhận được qua TLS) và địa chỉ nó đến từ, và *Approve* tác động lên
  fingerprint đó, không bao giờ lên vị trí dòng. Không gì đi trong plaintext và không gì
  đoán được; điều duy nhất chủ host có thể làm sai là approve một máy họ không mong đợi,
  và hàng đó ở đó để họ kiểm tra. Host cũng không giữ connection nào mở trong khi chờ —
  yêu cầu là một mục trong file, client dial lại — nên một trận lụt yêu cầu tốn 16 dòng,
  không phải 16 socket.

- **Mã QR mang fingerprint của host và một token dùng một lần, được ghim trước
  `AuthStart`**: token là một bí mật đáng bị đánh cắp trong năm phút, nên client chỉ tiêu
  nó cho một máy đã chứng minh, qua TLS handshake, rằng nó giữ private key có fingerprint
  in trong mã. Kẻ xen giữa ở địa chỉ trong mã không đưa ra được key đó, nên client dừng ở
  `InviteMismatch` và token không bao giờ đi qua đường truyền. Trên host, token được so
  sánh trong thời gian hằng, bị tiêu thụ ở lần dùng đầu, hết hạn sau 5 phút, chết cùng
  panel đã hiển thị nó, và một lần đoán sai được tính cho địa chỉ nguồn trong cùng bộ giới
  hạn đếm chữ ký sai — 3 lần mỗi phút, rồi chặn 10 giây — nên 2^256 khả năng không bao
  giờ được thử ở tốc độ cao.

- **Yêu cầu và token nằm trong file để một process thứ hai có thể tác động lên chúng**:
  trên iOS, broadcast extension nhận `AuthStart` trong khi app vẽ mã QR và danh sách yêu
  cầu; trong CLI, `share` chạy trong khi `access approve` được gõ ở terminal khác.
  `access_requests` và `pairing_tokens` nằm trong thư mục cấu hình dùng chung, dưới cùng
  khoá và cơ chế thay thế atomic như `authorized_keys`, `AccessRequestsGeneration` cho các
  poller một bộ đếm thay đổi rẻ, và một lần *Approve* chẳng là gì hơn một lần chuyển từ file
  này sang file khác mà `AuthStart` tiếp theo đọc lại.

- **Một key cho mỗi máy, certificate trong bộ nhớ**: hai key cho mỗi máy nghĩa là hai
  fingerprint, một trang *My keys*, code import và passphrase, một certificate được lưu có
  thể lệch với key của nó, và một `known_hosts` phải nhớ dùng client key nào ở đâu. Một
  key ECDSA P-256 trong `host_key.pem` phục vụ TLS ở phía host và chữ ký transcript ở phía
  client; X.509 mà TLS đòi hỏi được dựng quanh nó ở mỗi lần khởi động và không bao giờ
  được ghi. Fingerprint luôn là SHA-256 của SPKI, không bao giờ của certificate, nên host
  nâng cấp giữ nguyên fingerprint mà mọi client đã ghim; danh tính của client thì có đổi —
  từ Ed25519 sang key của máy — đó là lý do mọi client được cho phép thêm một lần, bằng
  Approve hoặc scan thay vì dán.

- **Bộ encode QR do dự án tự triển khai**: `core/` không nhận header bên thứ ba nào, và
  một thư viện QR cho mỗi nền tảng sẽ là năm cách vẽ của một mã cộng thêm cách thứ sáu cho
  CLI. `core/qr/QrCode` là bộ encode byte-mode ở mức sửa lỗi M, được test offline so với
  các encoding đã biết, và mọi client chỉ tô các ô vuông từ lưới module mà nó trả về. Giải
  mã là trường hợp ngược lại — cần camera và một bộ phát hiện nhanh — nên hai điện thoại
  dùng của chính nền tảng (CameraX + ZXing trên Android, AVFoundation trên iOS) và trả về
  một chuỗi.

- **Base64 nằm ở một nơi**: các dòng key OpenSSH và record lời mời đều cần nó, và hai bản
  copy đã bắt đầu lệch nhau. `core/net/Base64` là bộ encode và decode duy nhất, cho cả bảng
  chữ cái chuẩn lẫn URL-safe, với test riêng.
- **VT emulator do dự án tự triển khai.** Không có widget terminal nào của nền tảng vừa có
  mặt trên cả năm client vừa đi kèm giấy phép phù hợp, và việc tự triển khai giúp hành vi
  terminal test được offline và nhất quán trên mọi nền tảng.
- **Bản mirror shell phía host được cập nhật từ byte đầu tiên.** Output của PTY là một
  stream single-consumer có tính huỷ: byte đã đọc và gửi cho viewer không thể phát lại. Vì
  vậy lưới ký tự mà *Stop & attach* mở ra phải được dựng ngay khi byte đi qua, không phải
  tại thời điểm nhấn nút. Trong khi một viewer từ xa còn đang kết nối, các phản hồi cho
  terminal query của bản mirror bị loại bỏ: màn hình của viewer đã trả lời chúng, và shell
  không được nhận hai phản hồi.
- **Một port duy nhất.** Màn hình, terminal và file transfer dùng chung một listener; QUIC đảm
  nhận việc multiplex connection và stream. Port thứ hai trước đây chỉ tồn tại vì đường
  màn hình ở giai đoạn trước QUIC chiếm dụng socket.
- **Một `HostLink` thay cho bốn handshake trước đây.** Dial, kiểm tra trust, auth và
  recovery trước đây được viết bốn lần ở phía client: truy vấn source, viewer, file sender,
  và terminal chạy trên một `QuicEndpoint` riêng. Đó là lý do phần gửi file biết về một
  host key đã thay đổi muộn hơn viewer ba bản vá. Hiện `HostLink` là phần mã duy nhất phía
  client thực hiện dial hoặc authenticate; mỗi service mở `Chan` của nó, nhận một hàng đợi
  inbox riêng và xử lý trên thread của chính nó. Cơ chế kết nối lại với backoff của
  terminal đã được chuyển vào link để mọi giao diện yêu cầu recovery đều thừa hưởng — và
  việc chờ approve dùng lại chính cơ chế dial lại đó — và các quy tắc trust nằm ở một vị trí
  duy nhất: một key chưa biết khiến link thất bại cho tới khi người dùng trust nó
  (`acceptNewHostKey`), một lời mời chỉ ghim đúng key nó nêu tên, và một key đã biết được
  nhận ra ở bất kỳ địa chỉ nào.
- **`HostLink` gửi dữ liệu qua `Send`, không phải `SendMessage`.** Trên Windows, các OS
  header phía sau layer platform định nghĩa `SendMessage` thành một macro cho
  `SendMessageA`, và trong `HostLink.cpp` chúng xuất hiện sau phần khai báo lớp nhưng trước
  phần định nghĩa method, khiến MSVC yêu cầu định nghĩa cho một member `SendMessageA` không
  được khai báo ở đâu cả. Các tên API Win32 (`SendMessage`, `PostMessage`, `CreateWindow`,
  `GetObject`, …) không an toàn khi dùng làm tên method trong bất kỳ translation unit nào
  mà một OS header có thể tới được; giải pháp là đổi tên, không phải `#undef`.
- **Session ScreenCast của portal gắn liền với một kết nối D-Bus.** GLib cache session bus
  dùng chung bằng weak reference, nên `g_object_unref` trên handle cuối cùng sẽ huỷ luôn
  kết nối. Sau đó `xdg-desktop-portal` bỏ session, compositor huỷ node PipeWire, và node id
  mà portal vừa cung cấp không còn trỏ tới đâu; stream chuyển sang trạng thái `paused` và
  fail với thông báo *no target node available*. Vì vậy `PortalScreenCast` tự sở hữu
  `GDBusConnection` trong suốt thời gian session mở, thay vì mượn một kết nối cho mỗi lần
  gọi. App desktop che khuất vấn đề này trong thời gian dài vì GTK giữ một reference tới
  session bus trong suốt vòng đời process, còn `deskhub-cli` không link GTK nên không có
  reference đó.
- **Mỗi màn hình được chụp đều mở một PipeWire remote riêng**: một phiên portal chỉ trao
  ra một fd từ `OpenPipeWireRemote`, và nhân bản nó không tạo ra kết nối thứ hai — `dup`
  chỉ là một descriptor khác trỏ vào cùng một socket. Hai lời gọi `pw_context_connect_fd`
  trên các bản dup cho ra hai đối tượng `pw_core` với bảng proxy-id độc lập cùng ghi và
  đọc trên một luồng byte: id của chúng đụng nhau trong không gian id một-client duy nhất
  của daemon, và thread nào có epoll thức dậy trước sẽ nuốt luôn message dành cho thread
  kia, kể cả các descriptor `SCM_RIGHTS` mang theo bộ nhớ buffer. Luồng thua cuộc đua sẽ
  chết ở giai đoạn cấp phát của link với *Buffer allocation failed*, đó là lý do một màn
  hình thì luôn chạy còn hai màn hình thì hên xui. Vì vậy `ScreenCapture::Start` gọi
  `PortalScreenCast::OpenRemoteFd()` để có remote của riêng nó; portal cho phép gọi
  `OpenPipeWireRemote` nhiều lần trên một phiên đã start. Phiên chỉ giữ lại fd đầu tiên để
  chứng tỏ portal có trao remote và để làm chỗ dựa cho `isOpen()`.
- **Mọi icon đều được sinh ra từ một nguồn, và chỉ một số được bo góc.** `make icons` dựng
  lại toàn bộ bộ icon từ file gốc duy nhất `assets/icon_1024.png`. macOS, iOS, phần hiển
  thị trên Play Store và pipeline adaptive-icon của Android đều tự mask hình theo hình dạng
  riêng, nên các asset này giữ dạng vuông tràn viền. Windows, Linux và các launcher Android
  trước API 26 hiển thị đúng hình được cung cấp, nên icon của chúng đã có sẵn góc bo tròn
  và phần trong suốt; nếu không, app sẽ hiển thị thành một ô vuông đặc bên cạnh các icon bo
  tròn khác. `scripts/make-icons.py` chỉ sử dụng thư viện chuẩn theo chủ đích, vì bootstrap
  không cài công cụ xử lý ảnh nào.
- **Một client desktop giữ nhiều host cùng lúc; một điện thoại giữ một.** Trang connect
  trên Windows, Linux và macOS không giữ trạng thái kết nối nào. Mỗi host phản hồi sẽ nhận
  một cửa sổ kết nối — `ConnectionFrame` trong `client/windows/win32/MainFrame.cpp`,
  `ConnectionWindow` trong `client/linux/gtk/MainWindow.cpp`, và `WindowGroup` tên
  `connection` trong `client/macos/app/swift/App.swift` — sở hữu địa chỉ,
  capability, danh sách source và tuỳ chọn control của host đó, nhờ đó trang connect vẫn
  sẵn sàng cho host tiếp theo. Cửa sổ chính chỉ giữ danh sách các cửa sổ đang mở, để đưa
  một cửa sổ lên trước khi cùng một host được kết nối lần thứ hai, để chuyển từng lượt
  probe status tới đúng cửa sổ có địa chỉ khớp, và để đóng toàn bộ khi thoát. Android và
  iOS giữ mô hình một kết nối theo chủ đích: màn hình điện thoại không đủ chỗ cho một panel
  thứ hai, và session mà nó mở vốn đã chiếm toàn màn hình. `ui::SameDeviceAddr` là định
  nghĩa thống nhất của "cùng một host" — xem mục ngay dưới.
- **Một host có hai cách viết địa chỉ, nhưng chỉ một phép so sánh.** Một địa chỉ có thể
  được viết có hoặc không kèm port mặc định, nên `192.168.1.60` và `192.168.1.60:47777` chỉ
  cùng một host. So sánh hai giá trị này dưới dạng chuỗi sẽ sai mà không báo lỗi: panel đã
  kết nối không tìm thấy dòng gần đây tương ứng, và một cửa sổ kết nối bị mở hai lần cho
  cùng một host. Vì vậy phép so sánh địa chỉ phải đi qua `ui::NormalizedDeviceAddr` và
  `ui::SameDeviceAddr` (`core/ui/Strings.h`). Không so sánh hai địa chỉ thiết bị bằng `==`.

- **Một decoder vừa được mở chưa có reference frame.** `ScreenViewer` dựng lại decoder mỗi
  khi surface thay đổi, và app iOS trả surface về khi app rời khỏi màn hình; chỉ cần khoá
  điện thoại là đủ. Reassembler không biết điều đó: nó tiếp tục cung cấp các P-frame như
  trước, decoder mới không có dữ liệu để dự đoán, và host chỉ gửi IDR khi được yêu cầu, nên
  hình ảnh không hiển thị trong phần còn lại của session. Yêu cầu keyframe trước đây được
  gửi khi decoder *cũ* bị huỷ, đúng thời điểm không có surface để vẽ: IDR tới nơi, vòng
  decode loại bỏ nó vì thiếu surface, và `CancelKeyframeRequest` xoá yêu cầu đang chờ. Hiện
  `EnsureDecoder` đặt cờ yêu cầu cho mọi decoder mà nó mở, nên keyframe được yêu cầu vào
  thời điểm đã có nơi hiển thị. `MediaCodecDecoder` có lỗi tương ứng ở phía còn lại: nó đặt
  `sentCsd_` ngay ở frame đầu tiên nhận được, kể cả khi frame đó không mang parameter set,
  nên SPS/PPS của keyframe tiếp theo bị xếp hàng như dữ liệu thông thường và không cấu hình
  được codec; hiện nó chờ một frame thực sự chứa chúng. Thành phần nào mở decoder thì thành
  phần đó yêu cầu keyframe.

- **Một `AVSampleBufferDisplayLayer` từng ở background sẽ loại bỏ frame mà không báo lỗi.**
  iOS dừng việc decode của layer khi app rời khỏi màn hình và đặt
  `requiresFlushToResumeDecoding`; cho tới khi `flush` được gọi, mọi `enqueueSampleBuffer`
  đều được chấp nhận rồi loại bỏ. Không có dấu hiệu nào khác: `status` không phải `failed`,
  `isReadyForMoreMediaData` vẫn là true, và renderer không báo lỗi, nên viewer hoạt động
  bình thường về mặt số liệu nhưng không hiển thị hình. `VtDecoder` hiện kiểm tra cờ này
  khi mở trên một layer và kiểm tra lại trước mỗi frame, thực hiện flush, và cho frame đó
  fail để yêu cầu keyframe được gửi đi cùng lúc.

- **Mọi callback mà QUIC service loop gọi đều có thể xoá chính connection tương ứng.**
  `Service()` duyệt một snapshot các connection id và tra cứu lại từng id, vì
  `cb_.onConnected`, `cb_.onStream` và `cb_.onDatagram` đều chạy mã ứng dụng, vốn có thể
  đóng một peer và xoá nó khỏi `connections_`. `DrainStreams` kiểm tra lại sau mỗi callback
  — các guard tên `listStillIntact` tồn tại vì lý do này — nhưng nó chỉ return khỏi chính
  nó, nên `Service()` tiếp tục chạy vào `DrainDatagrams(id, entry)` với `entry` đã bị xoá
  và giải phóng, trong khi thao tác đầu tiên của hàm đó là truyền `entry.conn` cho
  `quiche_conn_dgram_recv`. Phép kiểm tra `Lookup(id) != &entry` lại nằm sau cả hai lượt
  drain, tức là muộn một bước. Trên CI Windows, lỗi này biểu hiện dưới dạng khoảng một
  trong ba lần chạy kết thúc với `0xc0000409` hoặc `0xc0000374`, và tồn tại lâu vì một cú
  fastfail không tới được `SetUnhandledExceptionFilter` trong
  `tests/integration/TestMain.cpp`, nên một lượt chạy fail chỉ để lại exit code. Ngoài ra,
  cả hai job được dựng để tìm lỗi này đều không phát hiện được: page heap không thấy vì
  block đã giải phóng thuộc về chính quiche và phần corruption là dữ liệu mà connection đã
  huỷ ghi sau đó; bản build Rust-checks cũng không thấy vì bên trong quiche không có lỗi.
  Job Windows ASan là công cụ cuối cùng xác định được frame gây lỗi. Cần kiểm tra lại entry
  sau mỗi lần gọi có thể chạy một callback, không chỉ kiểm tra một lần ở cuối khối.

- **Một watchdog liveness chỉ đo được đối tác khi vòng lặp của chính nó đang chạy.** Cửa sổ
  năm giây chờ pong của viewer tính theo đồng hồ thực, nên bất kỳ khoảng dừng nào ở phía
  này cũng được hiểu là host đã ngừng phản hồi. Trên job CI Windows ASan, một lượt upload
  32 MB chạy song song với một stream đang hoạt động đã làm treo cả process 3,7 giây: các
  dòng log đóng dấu `t=07:46:58` và `t=07:47:00` đều xuất hiện lúc 07:47:01, và bốn QUIC
  endpoint đồng thời báo khoảng trống poll nhiều giây, trong khi `HostLink` kết luận rằng
  một link hoàn toàn bình thường đã mất. Lượt kết nối lại sau đó chuyển client sang một
  source port mới, connection cũ ở phía host bị đóng do idle timeout 30 giây và kéo theo
  batch đang truyền (`transfer aborted ... link-lost`), khiến
  `TestInputStaysLiveDuringABigTransfer` chờ hết toàn bộ 120 giây deadline. Hiện
  `LinkPulse::Tick` chạy một lần mỗi vòng của `PumpReady` và trừ lại toàn bộ phần thời gian
  một vòng vượt quá `kLinkWatchStepUs`: khoảng im lặng chỉ được tính khi phía này thực sự
  đang theo dõi. Mọi watchdog đo một bên ở xa bằng đồng hồ cục bộ đều phải trừ đi khoảng
  thời gian nó không quan sát, nếu không thứ đầu tiên nó phát hiện sẽ là tình trạng của
  chính máy mình.

- **Một phiên truyền tồn tại lâu hơn connection của nó phải được thông báo.** `FileSender`
  chỉ rời trạng thái `Sending` khi nhận được ack, cancel hoặc `LinkLost()`, còn
  `FileUpload::Pump` coi một lần gửi bị từ chối là backpressure chứ không phải lỗi. Một
  lượt upload được nối với `onStreamBroken` và với thời điểm session kết thúc, nhưng không
  nối với việc `HostLink` của nó mất kết nối, từng kẹt ở `Sending` sau một lượt kết nối lại
  giữa phiên truyền trong khi ở đầu kia không còn thành phần nào có thể phản hồi: host đã
  huỷ batch, còn receiver trên connection mới chưa từng nhận được đề nghị. Vì vậy
  `FileTransferClient` không bao giờ kết nối lại giữa phiên truyền và cho lượt upload fail
  với `TransferReason::LinkLost` ngay khi link rời trạng thái `Ready`. Việc tiếp tục truyền
  qua một lượt kết nối lại đòi hỏi phát lại đề nghị trên connection mới; cho tới khi tính
  năng đó được bổ sung, việc kết thúc phiên truyền một cách rõ ràng tốt hơn một thanh tiến
  độ không còn thay đổi.

- **Socket mà host đã nhả vẫn còn bị mọi shell nó sinh ra giữ**: `Pty::Start` dùng
  `forkpty`, nên tiến trình con thừa kế mọi descriptor đang mở, và `ChildSetup` exec shell
  mà không đóng cái nào. Socket UDP của phiên đi theo luôn. Khi terminal host dừng,
  `Pty::Impl::Shutdown` gửi `SIGHUP` rồi reap bằng `WNOHANG` — không chờ — nên shell còn
  sống bao lâu thì cổng còn bị giữ bấy lâu, dù Deskhub đã đóng descriptor của mình. Dưới
  ASan, bộ test platform hỏng ở `bind(127.0.0.1:47793)` với `EADDRINUSE`: shell của test
  trước chưa kịp thoát. `UdpSocket::Open` giờ đặt `FD_CLOEXEC`, nên descriptor biến mất
  ngay khi shell exec và cổng chỉ thuộc về host. Bất kỳ descriptor sống lâu nào trong một
  tiến trình có fork shell của người dùng đều cần điều này; đóng ở tiến trình cha là chưa đủ.
  Tuy vậy close-on-exec vẫn để hở một khoảng: tiến trình con nằm giữa `forkpty` và `exec`
  vẫn giữ bản sao của nó, và chính lệnh `bind` đó hỏng ở mọi lần chạy ASan kể từ khi
  service loop không còn tốn một mili giây cho mỗi lần poll và test kế tiếp bắt đầu sớm
  hơn. Vì vậy `Shutdown` thu hồi tiến trình con trong tối đa 200 ms sau hangup và kill sau
  khoảng đó, nhờ vậy một terminal đã đóng cũng không còn để lại zombie. Master được đóng
  trước khoảng chờ đó chứ không phải sau: trên macOS, một shell đang thoát sẽ kẹt ở bước
  đóng terminal của nó cho tới khi phần output chưa ai đọc được xả hết, nên khi master còn
  mở thì ngay cả tiến trình con đã bị kill cũng không bao giờ thoát xong, lệnh `waitpid`
  cuối cùng không bao giờ trả về, và mọi job test trên macOS đều chạy tới hết timeout.
