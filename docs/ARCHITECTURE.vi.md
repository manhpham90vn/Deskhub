[English](ARCHITECTURE.md) · **Tiếng Việt** · [中文](ARCHITECTURE.zh.md) · [日本語](ARCHITECTURE.ja.md)

# Deskhub — Architecture

Tài liệu này mô tả Deskhub được xây dựng **như thế nào**: các layer, process và thread,
wire protocol, cùng những quyết định thiết kế đứng sau chúng. Phần mô tả sản phẩm dưới góc
nhìn người dùng nằm trong [`SPECIFICATION.vi.md`](SPECIFICATION.vi.md); threat model nằm
trong [`SECURITY.vi.md`](../SECURITY.vi.md).

Đây là bản dịch của [`ARCHITECTURE.md`](ARCHITECTURE.md). Nếu hai bản có khác biệt, bản
tiếng Anh là bản chuẩn.

- **Trạng thái:** mô tả mã nguồn hiện tại.
- **Đối tượng:** những người sửa đổi mã nguồn này.

---

## 1. Các layer

Toàn bộ cấu trúc tuân theo một nguyên tắc: logic được viết một lần và dùng chung cho mọi
client.

```
core/       C++20 thuần, không OS header, không mã bên thứ ba, unit test offline
platform/   lớp abstraction mỏng cho OS, mỗi header cung cấp một API giống nhau (phụ thuộc core)
client/     app theo từng OS: windows, linux, macos, ios, android (phụ thuộc platform và core)
            cùng client/cli, một command line client cho cả ba nền tảng desktop
```

| Layer | Nội dung |
| --- | --- |
| `core/protocol` | Wire format (`Wire.h`), record framing cho stream (`RecordStream.h`), packet classifier phân biệt QUIC với datagram beacon của Deskhub |
| `core/transport` | Packetizer/Reassembler cho video, FEC, cache retransmit, send pacer |
| `core/session` | Các session state machine, chia theo vai trò: `session/host` (session theo từng viewer, bảng viewer, beacon, file receiver, auth throttle), `session/client` (screen client, file sender, terminal client, luồng connect), cùng các thành phần dùng chung đặt cạnh chúng (kiểu dữ liệu transfer, bảng terminal session, clipboard sync, link recovery) |
| `core/control` | Bitrate controller, quality ladder, tính kích thước stream, clock offset |
| `core/terminal` | VT emulator dùng chung cho mọi client: `VtParser`, `Screen`, `KeyEncoder`, `Palette` |
| `core/net` | Trust store (phía client), paired devices (phía host), chọn bind address, logic scan LAN |
| `core/ui` | Toàn bộ chuỗi hiển thị cho người dùng, phần parse settings, các builder dòng bảng, để cả năm client hiển thị cùng nội dung |
| `platform/net` | `UdpSocket` (theo từng OS), `QuicEndpoint` (quiche đặt sau pimpl), `SessionTransport` |
| `platform/auth` | `AuthNegotiation` — pairing/passcode handshake duy nhất mà cả hai phía sử dụng |
| `platform/client` | `HostLink` (dial, trust, auth, channel; dùng chung cho mọi giao diện), `ScreenViewer`, `TerminalViewer`, `FileTransferClient`, `SourceQuery`, host probe, LAN scanner |
| `platform/host` | `HostEngine`, `HostNetLoop`, `SharingHost`, `TerminalHost`, `FileHost`, `ViewerBroadcast` |
| `platform/system` | Clock, random, PTY (ConPTY / forkpty), host identity (key), file trust và paired-device, autostart, keep-awake |
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
            packet QUIC          datagram Deskhub
                 |                        |
   +-------------+------------+       chỉ beacon:
   |             |            |       LIST_SOURCES / PING được trả lời
 stream      datagram      (TLS)      ở dạng không encrypt; mọi packet
   |             |                    thô khác đều bị loại bỏ
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
- **UDP thô** chỉ phục vụ discovery: beacon trả lời các scanner không dùng QUIC, và các
  probe không được mời nhận về danh sách source rỗng. Packet thô đi vào không thuộc các
  loại discovery đều bị loại bỏ trước khi tới bất kỳ phần mã session nào.

`QuicEndpoint` che hoàn toàn quiche (pimpl; `QuicEndpointNone.cpp` thay bằng stub, nhưng
chỉ khi bản build chủ động dùng `-DDESKHUB_QUIC=OFF`; thiếu quiche sẽ làm fail bước
configure, vì một binary stub không thể share hay connect). Connection được nhận diện qua
địa chỉ peer; không có connection migration. Theo hợp đồng, một connection quiche là
single-threaded, nên mọi thao tác trên endpoint đều diễn ra dưới send mutex của transport.
Transport không giữ mutex này xuyên qua một lần chờ socket blocking: `WaitReadable` chạy
trước ở trạng thái không khoá, sau đó là một lần `Poll` ngắn có khoá. Giữ mutex xuyên qua
lần chờ sẽ chặn mọi bên gửi.

## 3. Cơ chế chấp nhận: pairing

Mỗi máy tạo một key ECDSA P-256 trong lần chạy đầu tiên (`HostIdentity`); hash SHA-256 của
SPKI chính là fingerprint mà người dùng nhìn thấy. TLS sử dụng một certificate tự ký trên
key đó. Bên trên TLS, một handshake ở tầng ứng dụng (`AuthNegotiation`) quyết định việc
chấp nhận theo từng connection. Transport thực thi handshake này và loại bỏ mọi message từ
connection chưa hoàn tất phần auth:

| Client cung cấp | Host có biết máy này không | Kết quả |
| --- | --- | --- |
| không cung cấp gì | đã pair | **Signature**: client ký một transcript gồm nonce và fingerprint của host bằng key của nó, và được chấp nhận không cần thao tác thêm. |
| không cung cấp gì | chưa biết | **Approval**: người dùng tại host được hỏi (*Let this machine in?*). |
| một passcode | host có passcode | **Passcode**: SPAKE2 trên một verifier đã salt. Mã không đi qua đường truyền, mỗi connection chỉ được thử một lần, cả hai phía cùng chứng minh, và MAC được ràng buộc với đúng host key mà client thực sự nhận được, nhờ đó vô hiệu hoá các cuộc tấn công relay. Mã đã nhập luôn được kiểm tra, bất kể máy đã pair hay chưa. |
| một passcode | host không có passcode | không có giá trị để đối chiếu → Signature nếu đã pair, ngược lại là Approval. |
| bất kỳ | pairing đã tắt | **Denied** (máy đã pair vẫn đi theo đường Signature). |

Khi thành công, client được ghi vào `paired_devices` của host; pairing dựa trên key, không
dựa trên địa chỉ. Ba lần nhập sai passcode sẽ khoá đường passcode trong 30 giây
(`AuthThrottle`). Đường approval không
cần throttle vì đã có người quyết định.

Ở phía client, `known_hosts` (`TrustStore`) ghim key của host. Một key **đã thay đổi** sẽ
chặn kết nối kèm cảnh báo rõ ràng; một key chưa biết được chính handshake xử lý — host đã
chứng minh được passcode sẽ được lưu mà không cần hỏi thêm.

Dữ liệu truyền đi là bản thân public key, không phải một fingerprint đơn lẻ: host tự hash
nội dung nhận được, nên việc mạo danh đòi hỏi phải ký bằng một key mà kẻ mạo danh không
có. Và vì việc chấp nhận chỉ được xử lý một lần cho mỗi connection, không thành phần nào
phía trên transport phải hỏi lại: một máy đã chứng minh danh tính không mang passcode
trong các message sau đó, và phần mã session coi toàn bộ connection là đã authenticate.

## 4. Phía host

```
HostEngine (mỗi app một instance, sở hữu SessionTransport)
 ├─ thread net-loop: RunHostNetLoop
 │    recv → trả lời beacon | nạp dữ liệu video | Chan::Terminal → TerminalHost
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
(`core/session/LinkPulse`): một datagram `Ping` với session id 0 được gửi mỗi giây, beacon
của host trả lời trên cùng connection mà không cần session, và timestamp phản hồi trở
thành RTT đã làm mượt, còn id của các pong không quay lại tạo thành tỷ lệ mất gói.
`ClassifyLinkQuality` tổng hợp hai giá trị này thành Good / Fair / Poor cho danh sách
thiết bị và cho panel đã nhận phản hồi của host — một cửa sổ riêng trên desktop, trang
connect trên Android và iOS. Các cửa sổ session không còn hiển thị chỉ số này; `HostLink`
cung cấp nó qua `onPulse` và `Pulse()`. Vì một ping là ack-eliciting nên nó đồng thời đóng
vai trò keepalive; timer keepalive thông thường chỉ còn ý nghĩa khi link đang ở trạng thái
`Deciding`. Host phiên bản cũ không trả lời được ping session-0 sẽ để chỉ số ở mức
Unknown, không gây ảnh hưởng nào khác. Trên một link đang phục hồi, pulse cũng là phép
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
blocking. UI vẫn đẩy các yêu cầu (phím, resize, chấp nhận fingerprint) vào các hàng đợi
lệnh. Một host key đã thay đổi sẽ giữ link ở trạng thái `Deciding` cho tới khi người dùng
chấp nhận hoặc từ chối. Cửa sổ terminal không parse escape sequence: `core/terminal`
chuyển byte stream thành lưới ô, còn cửa sổ chỉ vẽ ô và chuyển tiếp sự kiện phím. Hiện mỗi
cửa sổ vẫn giữ link riêng; việc dùng chung một link đã được chấp nhận cho mọi cửa sổ trỏ
tới cùng một host là bước tiếp theo đã dự kiến, và sẽ được bổ sung tại `HostLink` dưới
dạng một registry cùng cơ chế fan-out cho observer, không phải thêm một handshake mới.

## 6. Discovery

Beacon trả lời `LIST_SOURCES` và `PING` bằng UDP không encrypt, để một scanner quét được
cả subnet mà không cần 254 lần TLS handshake. Máy chưa được chấp nhận nhận về danh sách
rỗng; danh sách source thật chỉ được cung cấp trên một connection đã được chấp nhận. Phản
hồi này cũng cho biết host hỗ trợ những gì — có nhận input hay không, có share terminal
hay không — thông qua các flag trong header `SOURCE_LIST`, nhờ đó client biết trước khi mở
bất kỳ cửa sổ nào rằng một điện thoại chỉ có thể được xem. Host phát hành trước khi các
flag này tồn tại sẽ không đặt flag nào. Các thiết bị gần đây, trạng thái online của chúng
(probe ping/pong) và kết quả scan LAN được hợp nhất thành một danh sách thiết bị duy nhất,
dựng bởi `core/ui/DeviceRows` và hiển thị trên cả năm client.

## 7. Dữ liệu trên đĩa

Mọi dữ liệu nằm trong thư mục Deskhub của người dùng (`~/.deskhub`,
`%USERPROFILE%\.deskhub`): `host_key.pem` và `host_cert.pem` (identity), `known_hosts`
(các host mà máy này trust), `paired_devices` (các máy mà host này chấp nhận), `auth_salt`
(salt không bí mật cho verifier), `ui-settings.txt`, `recent-devices.txt` (địa chỉ và
passcode đã che), `portal-restore-token.txt` trên Linux (token của chính desktop cho những
màn hình đã chọn trong hộp thoại chia sẻ màn hình), cùng log theo từng lần chạy. Phần file
I/O nằm trong `platform/`; phần parse và các cấu trúc dữ liệu nằm trong `core/` và có unit
test.

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
| `make test` | offline, không socket | toàn bộ `core/`: wire, framing, FEC, session, VT emulator, settings, chuỗi văn bản, structured fuzzing tất định |
| `make test-platform` | socket loopback | QUIC handshake thật, SPAKE2 end-to-end, terminal host và viewer qua đường truyền, PTY với shell thật, lockout, approval |
| `make test-integration` | loopback, capture/encode giả lập | session host↔client đầy đủ: negotiation, video qua đường truyền, input, kiểm soát bằng passcode và approval, khả năng chịu dữ liệu không hợp lệ, và độ trễ dưới tải chéo — một phiên truyền file, một terminal có lượng output lớn và các phím gõ chạy song song với một stream đang hoạt động, mỗi hạng mục được kiểm theo độ trễ lớn nhất quan sát được |
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
  phần cũng phục vụ SPAKE2 và host identity, nên không cần thư viện mật mã thứ hai.
- **Không sử dụng connection migration.** Không thư viện ứng viên nào có hỗ trợ phía client
  dùng được. Cơ chế reconnect và reattach (tương tự tmux, vốn đã cần thiết cho việc app di
  động chạy nền) đã đáp ứng yêu cầu này; các shell đang được giữ cũng có thể được liệt kê (`TermList`) và resume theo id từ một client mới.
- **Sử dụng ECDSA P-256 thay vì Ed25519.** Phía server của BoringSSL không ký TLS
  handshake bằng Ed25519 thông qua quiche. Không nên chuyển lại. Một identity Ed25519 đã
  lưu sẽ bị thay khi load, vì nó làm fail mọi handshake với `QUICHE_ERR_TLS_FAIL` mà không
  có thông tin giải thích trên giao diện.
- **Verifier của passcode là một lần SHA-256, không phải một KDF tốn chi phí.** SPAKE2 đã
  giới hạn kẻ tấn công ở một lần thử online cho mỗi connection và không để lại transcript
  nào có thể crack offline, tức là đã đáp ứng đúng mục đích mà độ cứng của KDF hướng tới.
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
- **Passcode là cơ chế tự phục vụ, approval là phương án dự phòng.** Mã đã nhập luôn được
  xác minh; không có mã thì việc quyết định thuộc về người dùng. Passcode không đi qua
  network dưới bất kỳ hình thức nào mà kẻ tấn công có thể thu thập.
- **VT emulator do dự án tự triển khai.** Không có widget terminal nào của nền tảng vừa có
  mặt trên cả năm client vừa đi kèm giấy phép phù hợp, và việc tự triển khai giúp hành vi
  terminal test được offline và nhất quán trên mọi nền tảng.
- **Bản mirror shell phía host được cập nhật từ byte đầu tiên.** Output của PTY là một
  stream single-consumer có tính huỷ: byte đã đọc và gửi cho viewer không thể phát lại. Vì
  vậy lưới ký tự mà *Stop & attach* mở ra phải được dựng ngay khi byte đi qua, không phải
  tại thời điểm nhấn nút. Trong khi một viewer từ xa còn đang kết nối, các phản hồi cho
  terminal query của bản mirror bị loại bỏ: màn hình của viewer đã trả lời chúng, và shell
  không được nhận hai phản hồi.
- **Một port duy nhất.** Beacon, màn hình và terminal dùng chung một listener; QUIC đảm
  nhận việc multiplex connection và stream. Port thứ hai trước đây chỉ tồn tại vì đường
  màn hình ở giai đoạn trước QUIC chiếm dụng socket.
- **Một `HostLink` thay cho bốn handshake trước đây.** Dial, kiểm tra trust, auth và
  recovery trước đây được viết bốn lần ở phía client: truy vấn source, viewer, file sender,
  và terminal chạy trên một `QuicEndpoint` riêng. Đó là lý do phần gửi file biết về một
  host key đã thay đổi muộn hơn viewer ba bản vá. Hiện `HostLink` là phần mã duy nhất phía
  client thực hiện dial hoặc authenticate; mỗi service mở `Chan` của nó, nhận một hàng đợi
  inbox riêng và xử lý trên thread của chính nó. Cơ chế kết nối lại với backoff của
  terminal đã được chuyển vào link để mọi giao diện yêu cầu recovery đều thừa hưởng, và các
  quy tắc trust nằm ở một vị trí duy nhất: một key đã thay đổi giữ link ở trạng thái
  `Deciding` cho tới khi có phản hồi của người dùng (chỉ phần truy vấn source đi qua trực
  tiếp, với `trustGate=false` và không lưu gì, vì bên gọi nó không có giao diện để hiển thị
  hộp thoại), và chỉ một passcode mà host đã chứng minh bằng mật mã mới tự động ghim một
  key.
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
  `connection` trong `client/macos/app/swift/App.swift` — sở hữu địa chỉ, passcode,
  capability, danh sách source và tuỳ chọn control của host đó, nhờ đó trang connect vẫn
  sẵn sàng cho host tiếp theo. Cửa sổ chính chỉ giữ danh sách các cửa sổ đang mở, để đưa
  một cửa sổ lên trước khi cùng một host được kết nối lần thứ hai, để chuyển từng lượt
  probe status tới đúng cửa sổ có địa chỉ khớp, và để đóng toàn bộ khi thoát. Android và
  iOS giữ mô hình một kết nối theo chủ đích: màn hình điện thoại không đủ chỗ cho một panel
  thứ hai, và session mà nó mở vốn đã chiếm toàn màn hình. `ui::SameDeviceAddr` là định
  nghĩa thống nhất của "cùng một host" — xem mục ngay dưới.
- **Một host có hai cách viết địa chỉ, nhưng chỉ một phép so sánh.** `ScanAddressText` bỏ
  phần port khi đó là port mặc định, nên một dòng kết quả scan hiển thị `192.168.1.60`
  trong khi địa chỉ người dùng nhập và đã kết nối là `192.168.1.60:47777`. So sánh hai giá
  trị này dưới dạng chuỗi sẽ sai mà không báo lỗi, và mọi vị trí từng làm như vậy đều mất
  một chức năng: panel đã kết nối không tìm thấy dòng thiết bị tương ứng nên không hiển thị
  ping, còn `PasscodeForDevice` không tìm được mã đã lưu cho một host chọn từ danh sách
  scan. Vì vậy phép so sánh địa chỉ phải đi qua `ui::NormalizedDeviceAddr` và
  `ui::SameDeviceAddr` (`core/ui/Strings.h`), được cung cấp cho client Swift và Kotlin dưới
  tên `dh_same_device_addr`. Không so sánh hai địa chỉ thiết bị bằng `==`.

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
