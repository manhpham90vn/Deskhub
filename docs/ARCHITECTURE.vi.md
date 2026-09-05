[English](ARCHITECTURE.md) · **Tiếng Việt** · [中文](ARCHITECTURE.zh.md) · [日本語](ARCHITECTURE.ja.md)

# Deskhub — Kiến trúc

Tài liệu này mô tả Deskhub **được xây như thế nào**: các tầng, tiến trình và luồng,
giao thức trên đường truyền, và các quyết định thiết kế đứng sau. Sản phẩm làm được gì
dưới góc nhìn người dùng nằm ở [`SPECIFICATION.vi.md`](SPECIFICATION.vi.md); mô hình
mối đe doạ nằm ở [`SECURITY.vi.md`](../SECURITY.vi.md).

Đây là bản dịch của [`ARCHITECTURE.md`](ARCHITECTURE.md); khi hai bản khác nhau, bản
tiếng Anh là bản chuẩn.

- **Trạng thái:** mô tả mã nguồn hiện tại.
- **Đối tượng đọc:** bất kỳ ai sửa mã.

---

## 1. Các tầng

Một quy tắc chi phối toàn bộ bố cục: logic viết một lần và dùng chung cho mọi client.

```
core/       C++20 thuần, không header OS, không mã bên thứ ba, test offline được
platform/   lớp trừu tượng OS mỏng, mỗi header một API giống hệt nhau mọi nơi (phụ thuộc core)
client/     app theo từng OS: windows, linux, macos, ios, android (phụ thuộc platform + core)
            cộng thêm client/cli, một client dòng lệnh dùng chung cho cả ba máy để bàn
```

| Tầng | Nội dung |
| --- | --- |
| `core/protocol` | Định dạng gói (`Wire.h`), cắt khung record cho stream (`RecordStream.h`), bộ phân loại gói QUIC với gói beacon |
| `core/transport` | Packetizer/Reassembler cho video, FEC, cache gửi lại, bộ điều tốc gửi |
| `core/session` | Máy trạng thái phiên, chia theo vai trò: `session/host` (phiên theo viewer, bảng viewer, beacon, bên nhận file, khoá đếm lần đoán mã), `session/client` (screen client, bên gửi file, terminal client, luồng connect), và các mảnh dùng chung nằm cạnh (kiểu dữ liệu transfer, bảng phiên terminal, đồng bộ clipboard, khôi phục kết nối) |
| `core/control` | Điều khiển bitrate, thang chất lượng, cỡ luồng, lệch đồng hồ |
| `core/terminal` | Bộ giả lập VT mọi client dùng chung: `VtParser`, `Screen`, `KeyEncoder`, `Palette` |
| `core/net` | Trust store (phía client), danh sách máy đã ghép (phía host), chọn địa chỉ bind, logic quét LAN |
| `core/ui` | Mọi chuỗi hiển thị, đọc/ghi cài đặt, dựng dòng bảng — để cả năm client nói giống hệt nhau |
| `platform/net` | `UdpSocket` (theo OS), `QuicEndpoint` (quiche sau pimpl), `SessionTransport` |
| `platform/auth` | `AuthNegotiation` — một bắt tay ghép cặp/passcode duy nhất cả hai phía cùng nói |
| `platform/client` | `HostLink` (quay số + tin cậy + auth + kênh, mọi bề mặt dùng chung), `ScreenViewer`, `TerminalViewer`, `FileTransferClient`, `SourceQuery`, dò host, quét LAN |
| `platform/host` | `HostEngine`, `HostNetLoop`, `SharingHost`, `TerminalHost`, `FileHost`, `ViewerBroadcast` |
| `platform/system` | Đồng hồ, ngẫu nhiên, PTY (ConPTY / forkpty), danh tính máy (khoá), file trust/paired, autostart, giữ máy thức |
| `core/cli` | Ngữ pháp dòng lệnh và bộ ghi JSON của nó — vào là văn bản thuần, ra là một lệnh đã kiểm tra hợp lệ |
| `client/<os>` | Thu hình, mã hoá, giải mã, vẽ, cửa sổ, hộp thoại — không có gì mang hình dạng giao thức |
| `client/cli` | Từ cờ thành phiên: một binary vừa làm host, vừa kết nối, vừa mở shell, không cần toolkit đồ hoạ. Nó link đúng thư viện media theo OS mà app để bàn đang dùng |

`core/` phải test offline được, không mạng không GPU. `platform/` được đụng OS nhưng
API công khai phải giống hệt nhau trên mọi hệ. Nếu cùng một đoạn mã xuất hiện ở hai
client, nó thuộc về tầng thấp hơn.

## 2. Một cổng, một transport

Mọi thứ host cung cấp đi trên **một cổng UDP** (mặc định 47777) qua một
`SessionTransport` duy nhất, bọc một `QuicEndpoint`:

```
                      Cổng UDP 47777
                            |
                 ClassifyPacket (byte đầu)
                   /                    \
            gói QUIC              gói Deskhub thuần
                 |                        |
   +-------------+------------+       chỉ beacon:
   |             |            |       LIST_SOURCES / PING được trả lời
 stream      datagram      (TLS)      ở bản rõ; mọi gói thô khác
   |             |                    đều bị bỏ
 control      video
 input        audio       Stream chở các record có tiền tố độ dài
 clipboard                (RecordStream), tối đa 16 KiB mỗi record.
 terminal                 Mỗi datagram chở đúng một gói video hoặc
 files                    một gói audio (≤ 1200 B).
```

- **Stream** (tin cậy, đúng thứ tự): control, input, clipboard, terminal, files — mỗi kết
  nối dùng một stream hai chiều do client mở. Stream nghẽn ở kết nối này không làm
  đứng kết nối khác. Dữ liệu stream đến được rút theo ngân sách 64 KiB mỗi lượt phục
  vụ: bên tiêu thụ (nặng nhất là giả lập VT của terminal) phải trả vòng lặp lại cho
  ACK, keepalive và xử lý timeout giữa các lát, nên một trận `cat` không thể bỏ đói
  kết nối đến mức tự rơi vào idle timeout nữa.
- **Datagram** (không tin cậy, không thứ tự, vẫn mã hoá): gói video và gói audio.
  QUIC không bao giờ gửi lại datagram mất; với video thì FEC/NACK của app tự xử lý,
  còn với audio thì không gì xử lý cả — xem mục 9.
- **UDP thô** chỉ còn cho dò tìm: beacon trả lời máy quét không nói QUIC, và gói dò
  không mời nhận danh sách rỗng. Gói thô đến mà không phải loại dò tìm bị loại trước
  khi chạm tới bất kỳ mã phiên nào.

`QuicEndpoint` giấu kín quiche (pimpl; `QuicEndpointNone.cpp` thế chỗ, nhưng chỉ khi
build chủ động tắt bằng `-DDESKHUB_QUIC=OFF` — thiếu quiche thì configure lỗi ngay,
vì binary dùng stub không chia sẻ hay kết nối được). Kết nối được định danh bằng địa
chỉ peer; không có connection migration. Một
kết nối quiche chỉ dùng được từ một luồng, nên mọi lần chạm endpoint đều nằm dưới
mutex gửi của transport — và transport không bao giờ giữ mutex đó xuyên qua một lần
chờ socket (`WaitReadable` trước, không khoá; rồi `Poll` ngắn có khoá). Giữ khoá
xuyên qua lần chờ sẽ bỏ đói mọi bên gửi.

## 3. Quyền vào: ghép đôi

Mỗi máy sinh một khoá ECDSA P-256 ở lần chạy đầu (`HostIdentity`); băm SHA-256 của
SPKI là dấu vân tay người dùng nhìn thấy. TLS dùng chứng chỉ tự ký trên khoá đó. Trên
TLS, một cuộc bắt tay tầng ứng dụng (`AuthNegotiation`) quyết định quyền vào theo
từng kết nối. Transport chạy nó và bỏ mọi message từ kết nối chưa chốt xong auth:

| Client đưa ra | Host biết máy đó | Kết quả |
| --- | --- | --- |
| không gì cả | đã ghép đôi | **Signature**: client ký transcript nonce+dấu-vân-tay-host bằng khoá của nó. Vào êm. |
| không gì cả | máy lạ | **Approval**: người ngồi tại host được hỏi (*Let this machine in?*). |
| một passcode | host có mã | **Passcode**: SPAKE2 trên verifier có salt — mã không bao giờ đi qua mạng, mỗi kết nối một lần đoán, hai bên cùng chứng minh, MAC trói vào đúng khoá host mà client đang thấy (chặn chuyển tiếp). Mã đã gõ luôn bị kiểm, quen hay lạ. |
| một passcode | host không có mã | không có gì để đối chiếu → Signature nếu đã ghép, Approval nếu chưa. |
| bất kỳ | tắt ghép đôi mới | **Denied** (máy đã ghép vẫn đi đường Signature). |

Thành công thì client được ghi vào `paired_devices` của host; ghép đôi theo khoá,
không theo địa chỉ. Đoán sai passcode 3 lần khoá đường passcode 30 giây
(`AuthThrottle`, dùng chung hằng số với khoá phiên cũ); đường approval không cần
khoá — người thật là cái cổng.

Phía client, `known_hosts` (`TrustStore`) ghim khoá host. Khoá **đổi** thì chặn kết
nối sau một cảnh báo lớn; khoá chưa gặp được chính cuộc bắt tay phân xử (host chứng
minh được passcode thì được ghi nhớ mà không cần hỏi).

Trên wire là chính public key, không bao giờ là fingerprint trần — host tự hash thứ
nó nhận được, nên muốn khoác danh tính máy khác thì phải ký được bằng khoá mà kẻ mạo
danh không nắm. Và vì quyền vào chốt một lần cho mỗi kết nối, không tầng nào phía
trên transport hỏi lại: máy đã chứng minh mình không mang passcode trong bất kỳ
message nào về sau, và code phiên coi cả kết nối là đã xác thực.

## 4. Phía host

```
HostEngine (một cho cả app, sở hữu SessionTransport)
 ├─ luồng net-loop: RunHostNetLoop
 │    recv → trả lời beacon | nhận đường video | Chan::Terminal → TerminalHost
 │    Tick phiên theo từng nguồn, đẩy clipboard, reconfig, thống kê
 ├─ thu hình/mã hoá: theo từng nguồn, do callback thu hình của OS lái (tầng client)
 │    frame → encoder (mutex theo nguồn) → Packetizer → FEC → SendTo (datagram)
 ├─ luồng audio worker: callback thu âm → vòng khung lock-free → mã hoá Opus →
 │    datagram cho từng viewer (AudioBroadcaster)
 └─ TerminalHost (khách thuê, khi terminal được chia sẻ)
      ├─ HandleMessage trên luồng net-loop: TERM_OPEN/DATA/RESIZE/CLOSE → PTY
      └─ luồng bơm: đầu ra PTY → Screen mirror phía host + record TERM_DATA,
           hết hạn, kick
```

- Engine chạy khi bất kỳ thứ gì được chia sẻ. Không có nguồn màn hình mà terminal
  được tick thì nó chạy không-nguồn; vòng lặp sống chừng nào terminal còn sống.
- Mỗi nguồn màn hình là một `SourcePipelineState`: `ScreenHostSession` riêng (bảng viewer,
  thương lượng, phân xử input), encoder, thang chất lượng và chẩn đoán riêng. Một
  lần mã hoá nuôi mọi viewer của nguồn đó.
- Vòng phản hồi: viewer gửi `Feedback` (loss/RTT) mỗi giây, và host góp thêm một tín
  hiệu của chính nó — tuổi của frame lúc nó tới bộ gửi, đúng đại lượng mà `enc_lat_ms`
  báo cáo. `BitrateController` (AIMD) và `QualityLadder` chỉnh bitrate, độ phân giải,
  fps của encoder theo cả ba; FEC bật sẵn từ frame đầu và chỉ hạ xuống sau một chuỗi dài
  không mất gói, vì loss mà nó chống lại xuất hiện trước cả báo cáo đầu tiên — backlog
  không bao giờ bật FEC, vì gói parity chỉ làm hàng đợi dày thêm. CUBIC của quiche nằm
  dưới đường datagram; hai bộ hoạt động nối
  tiếp — quiche giới hạn thứ rời khỏi máy, app điều tốc encoder theo loss sinh ra.
- Input: "host thắng" — `LocalInputMonitor` tạm dừng input từ xa khi người ngồi tại
  máy động vào chuột thật; mỗi lúc một viewer điều khiển.
- Shell: mỗi shell một PTY (`ConPTY` trên Windows, `forkpty` nơi khác), tối đa 8;
  kết nối rớt thì shell được tách và PTY sống thêm 2 phút để đúng máy đó gắn lại.
  Mọi lần mở/đóng/tách/gắn lại đều ghi audit kèm địa chỉ, tên và khoá.
- Đầu ra của mỗi shell còn nuôi một Screen `core/terminal` phía host ngay từ lúc
  shell khởi động. *Stop & attach* ngắt client từ xa và mở mirror đó — scrollback
  còn nguyên — trong một cửa sổ terminal trên máy host; shell bị tiếp quản kiểu này
  thuộc về host, không bao giờ hết hạn, và kết thúc khi cửa sổ của host đóng.

## 5. Phía client

Mọi bề mặt client đi tới host qua cùng một mảnh, `HostLink`
(`platform/client/HostLink`): nó quay số kết nối QUIC, kiểm tra kho tin cậy, chạy
bắt tay auth, giữ kết nối sống, và — với bề mặt nào yêu cầu — tự quay số lại theo
backoff khi kết nối rơi. Không dịch vụ nào còn tự quay số hay tự auth; mỗi dịch vụ
mở một kênh theo `Chan` trên dây, nhận hàng đợi inbox riêng, và tự rút trên luồng
của chính nó:

```
HostLink (một cho mỗi bề mặt đang mở)
 ├─ luồng link: quay số → kiểm tra tin cậy → auth → bơm
 │   (chia record và datagram vào hàng đợi theo Chan của từng kênh;
 │    mạch đập của link; quay số lại theo backoff nơi bật khôi phục)
 ├─ Chan::Control/Video/Audio ─> ScreenViewer
 │    ├─ luồng net: HELLO/thương lượng, nhận video (Reassembler+FEC),
 │    │   NACK, feedback, clipboard
 │    └─ luồng giải mã: decoder + hàng đợi vẽ
 ├─ Chan::Terminal ─> luồng dịch vụ của TerminalViewer
 │    ├─ Screen của core/terminal giữ lưới ký tự
 │    └─ UI poll Snapshot(), post phím vào hàng đợi lệnh
 └─ Chan::File ─> luồng dịch vụ của FileTransferClient (vòng FileUpload)
```

Khi đã được nhận vào, link tự bắt mạch cho chính nó (`core/session/LinkPulse`):
mỗi giây một datagram `Ping` mang session id 0 đi ra, beacon của host trả lời nó
trên chính kết nối đó mà không cần phiên nào, timestamp được vọng lại trở thành
RTT đã làm mượt, còn những id không có pong quay về trở thành phần trăm mất gói.
`ClassifyLinkQuality` gộp hai con số thành Tốt / Khá / Kém cho danh sách thiết bị
và cho panel đã trả lời host — cửa sổ riêng trên desktop, trang kết nối trên Android
và iOS — các cửa sổ phiên không còn chở nó nữa — `HostLink` đưa số đo ra
qua `onPulse` và `Pulse()`, và vì ping là gói đòi ACK nên
nó kiêm luôn vai keepalive; bộ đếm keepalive thường chỉ còn có việc khi link đang
đỗ ở `Deciding`. Host quá cũ không trả lời ping session-0 thì số đo chỉ đứng ở
Unknown — không gì thoái lui. Trên link có bật khôi phục, mạch đập cũng là phép
thử sống: năm giây không có pong (và chỉ sau khi pong đầu tiên đã chứng minh host
có trả lời) là kết nối bị thả xuống đường quay số lại sẵn có.

Viewer màn hình giờ tham gia cơ chế khôi phục đó như terminal xưa nay: link rơi
hay câm lặng, hoặc phiên năm giây không nhận được gì, sẽ đỗ cửa sổ ở `Reattaching`
(khung hình cuối vẫn treo, dòng trạng thái chuyển sang chữ đang nối lại) thay vì
kết thúc nó. `HostLink::RequestRedial` ép quay số lại khi phía phiên nhận ra
trước, và khi link được nhận vào lại viewer chạy lại `HELLO` với đúng client id
cũ — host gắn lại slot viewer — rồi hình tiếp tục từ keyframe mới. Sau sáu mươi
giây (`kViewerReattachGraceUs`) không vào lại được, cửa sổ kết thúc với lý do như
thường lệ.

Truy vấn nguồn (`QuerySources`) đi cùng loại link đó ở dạng một-lần, chờ-kết-quả.
UI vẫn đăng ý định (phím, đổi cỡ, chấp nhận dấu vân tay) vào hàng đợi lệnh; key của
host đổi thì link đỗ ở `Deciding` cho tới khi người dùng chấp nhận hay từ chối. Cửa
sổ terminal không bao giờ tự phân tích escape sequence — `core/terminal` biến luồng
byte thành lưới ô, cửa sổ chỉ vẽ ô và chuyển tiếp sự kiện phím. Hiện mỗi cửa sổ vẫn
giữ link riêng; dùng chung một link đã được nhận vào cho mọi cửa sổ nhắm tới cùng
host là bước kế tiếp dự kiến, và nó cắm vào `HostLink` — một registry cộng fan-out
observer — chứ không phải thêm một bắt tay nữa.

## 6. Dò tìm

Beacon trả lời `LIST_SOURCES` và `PING` bằng UDP thuần để máy quét quét được cả dải
mạng mà không tốn 254 lần bắt tay TLS. Máy lạ nhận danh sách rỗng; danh sách nguồn
thật chỉ lộ qua kết nối đã được cho vào. Câu trả lời đó còn mang theo những gì host
làm được — có nhận thao tác không, có chia sẻ terminal không — trong các cờ ở header
`SOURCE_LIST`, nên client biết trước khi mở bất kỳ cửa sổ nào rằng một chiếc điện
thoại chỉ có thể xem. Host bản cũ, có từ trước khi có các cờ này, không bật cờ nào. Thiết bị gần đây, trạng thái online
(ping/pong) và kết quả quét LAN đổ vào một danh sách thiết bị gộp, do
`core/ui/DeviceRows` dựng và cả năm client đều hiển thị.

## 7. Dữ liệu trên đĩa

Tất cả nằm trong thư mục Deskhub của người dùng (`~/.deskhub`,
`%USERPROFILE%\.deskhub`): `host_key.pem` + `host_cert.pem` (danh tính),
`known_hosts` (host mà máy này tin), `paired_devices` (máy mà host này cho vào),
`auth_salt` (salt không bí mật), `ui-settings.txt`, `recent-devices.txt` (địa chỉ +
passcode che đi), `portal-restore-token.txt` trên Linux (token của chính desktop cho
những màn hình đã chọn trong hộp thoại chia sẻ của nó), và log theo từng lần chạy. I/O file nằm ở `platform/`; phần phân
tích và cấu trúc dữ liệu nằm ở `core/` và có unit test.

Tệp viewer gửi tới thì nằm ở chỗ khác hẳn: một thư mục do host chọn (`transfer_dir`
trong `ui-settings.txt`, mặc định là `Deskhub` trong thư mục nhà của người dùng).
`FileStore` ghi mỗi tệp thành `<tên>.deskhub-part` và chỉ đổi tên khi cả tệp đã tới
với CRC-32 khớp, nên tệp ghi dở không bao giờ xuất hiện dưới tên thật, và
`UniqueFileName` bảo đảm không có gì bị ghi đè. Tên đi trên dây được `SafeFileName`
của `core/` chà sạch — dấu phân cách đường dẫn, byte điều khiển, ký tự Windows không
nhận và tên thiết bị dành riêng đều bị loại — trước khi `platform/` chạm vào hệ tệp.

## 8. Kiểm thử

| Bộ | Chạy | Phủ |
| --- | --- | --- |
| `make test` | offline, không socket | toàn bộ `core/`: wire, framing, FEC, phiên, bộ giả lập VT, cài đặt, chuỗi, fuzz có cấu trúc |
| `make test-platform` | socket loopback | bắt tay QUIC thật, SPAKE2 đầu-cuối, terminal host + viewer qua mạng, PTY với shell thật, lockout, approval |
| `make test-integration` | loopback, thu/mã hoá giả | phiên host↔client đầy đủ: thương lượng, video qua mạng, input, cổng passcode/approval, chịu gói rác, và độ trễ dưới tải chéo — file transfer, terminal bị flood và phím gõ chạy cạnh stream đang phát, mỗi thứ bị chặn theo khoảng đứng tệ nhất quan sát được |
| các target fuzz | 30 giây mỗi target ở mọi PR, 15 phút mỗi target hằng đêm | parser cho wire, H.264, ráp gói, byte terminal và chuỗi UI, cộng máy trạng thái phiên phía host và viewer |
| `make test-perf` | bản release, offline + loopback | đo các đường nóng chứ không chỉ chạy chúng: `core_perf` cho phần C++ thuần, `platform_perf` cho QUIC thật qua loopback; cả hai fail theo số lần cấp phát trên mỗi đơn vị, chi phí khi đầu vào gấp 4, và độ lệch so với mốc ghi ngay trên máy đó |

`platform_tests` và `integration_tests` mỗi bộ giữ thư mục app data riêng trên mọi hệ điều
hành — khoá host, danh sách host tin cậy và file ghép đôi đều là file dùng chung duy nhất,
nên một bộ test đọc thư mục nhà của lập trình viên là đang chạy đua với app đã cài và mọi
tiến trình Deskhub khác trên máy.

CI còn ép clang-format và clang-tidy (đều ghim phiên bản), SwiftLint `--strict`,
Android Lint, actionlint + shellcheck, chạy cả ba bộ dưới ASan/TSan, CodeQL cho
C++/Kotlin/Swift, quét gitleaks toàn bộ lịch sử, và coverage `core/` ≥ 90% dòng / 80%
nhánh. Ba bộ test còn được biên dịch chéo và chạy trên Linux arm64, emulator Android và
iOS Simulator, và một job Windows chạy thêm ba lần bộ integration mỗi vòng để săn lỗi
hỏng stack chập chờn trong `DrainStreams`, thứ chỉ lộ ra khoảng một lần trong ba. Các job release trên Linux và macOS còn chạy `core_perf` và
`platform_perf` với hai cổng chặn cấp phát và độ tuyến tính (máy CI dùng chung không có
mốc thời gian), và mỗi pull request có thêm một báo cáo perf-và-lag đăng thành một
comment tự cập nhật: cả hai suite perf được A/B với commit gốc trên cùng một runner (độ
lệch chỉ là cảnh báo, không bao giờ đánh trượt), số đo tích hợp dưới tải của chính bản
pull request, và dòng coverage của `core/`.

## 9. Các quyết định đáng nhớ

Cuộc bake-off A1 đứng sau vài quyết định FEC bên dưới được viết lại cho người đọc ngoài dự án
trong [`docs/posts/fec-under-burst-loss.vi.md`](posts/fec-under-burst-loss.vi.md), với CSV thô mà
bài trích dẫn nằm ngay cạnh trong [`docs/data/bake-off/`](data/bake-off/).

- **Một phép dò năng lực trả về false có thể tắt hẳn cả một vòng điều khiển**: encoder
  Media Foundation trả `false` cho `SetBitrate` mỗi khi MFT không có
  `CODECAPI_AVEncCommonMeanBitRate`, và `ApplyFeedback` hoàn toàn đúng khi coi một lần từ
  chối là "không commit gì". Trên MFT Intel Quick Sync báo `MeanBitRate: NOT SUPPORTED`,
  hệ quả là host không bao giờ đổi bitrate: đo trên chính phần cứng này, 30 giây loss
  29-40 % liên tục không sinh ra một quyết định `Bitrate` nào, nên thang chất lượng cũng
  đứng im. Log khởi động ghi `NOT SUPPORTED` suốt thời gian đó mà không ai đọc nó thành
  "khả năng thích ứng đã chết". `SetFps` và `RequestKeyFrame` trong cùng file vốn đã lùi
  về `ReinitTransform()`; chỉ `SetBitrate` là bỏ cuộc, và giờ nó lùi về y như vậy —
  `ConfigureTransform` ghi `MF_MT_AVG_BITRATE` từ `cfg` nên việc dựng lại sẽ áp bitrate
  mới. Dựng lại tốn một IDR, nên đường `codecapi` trực tiếp vẫn được thử trước. Khi một
  năng lực tuỳ thiết bị chặn mất một đầu vào điều khiển, hãy bắt buộc phải có đường lùi:
  xuống cấp thành "chậm hơn" là một lựa chọn, âm thầm xuống cấp thành "không bao giờ"
  thì không.

- **Máy gửi không theo kịp trông y hệt một đường truyền sạch**: mọi đầu vào mà
  `BitrateController` có — loss, RTT, tốc độ nhận — đều đến từ viewer, nên không gì
  trong vòng lặp nói được "chính tôi đang tụt lại". Đo trên Pixel 4 làm host cho hai
  viewer: frame rời encoder khi đã cũ 15 s trong lúc viewer báo 0 % loss và RTT 15 ms,
  còn bộ điều khiển đọc đó là dư địa và bơm bitrate ngược lên trần 20 Mbps — bufferbloat
  nằm ngay trong máy gửi, càng thấy đường truyền sạch thì càng bơm mạnh. Host giờ đo tuổi
  frame ngay tại bước gửi và đưa vào cạnh các số của viewer: quá `kBacklogMs` thì lùi như
  gặp 2 % loss, quá `kSevereBacklogMs` thì lùi như gặp 5 % loss, và cả hai đều chặn nhánh
  tăng trong hai giây như thường lệ. Bitrate vẫn là biến điều khiển duy nhất, nên
  `QualityLadder` tụt bậc theo sau và mức trần fps đi theo. Vòng điều khiển nào chỉ được
  nuôi bằng số liệu từ đầu kia thì mù với đúng nửa đường ống mà nó sở hữu.

- **Chặn fps chỉ có tác dụng ở nơi thật sự có thứ gì đó bỏ frame**: bậc fps của thang
  chất lượng là một yêu cầu, và mỗi nền tảng phải thực thi nó ở chỗ frame có thể bị vứt
  đi. Windows và Linux chặn ngay tại capture bằng `FrameGate`; Android chặn đầu vào
  MediaCodec bằng `max-fps-to-encoder`; macOS cấu hình lại khoảng cách frame của
  ScreenCaptureKit. iOS thì không có chỗ nào: ReplayKit giao frame theo nhịp màn hình,
  còn `VtEncoder::SetFps` chỉ đặt `kVTCompressionPropertyKey_ExpectedFrameRate` — một
  gợi ý cho rate control, không bỏ frame nào cả. Đổi bậc ở đó chỉ chỉnh lại encoder chứ
  không thay đổi số frame nó phải nuốt. `OfferVtFrame` giờ chạy cùng một `FrameGate` cho
  cả hai app Apple, đặt sau khi cache dùng cho flush lúc màn hình tĩnh đã được làm mới,
  để màn hình đứng yên vẫn còn frame để gửi lại. Khi một núm vặn tồn tại trên mọi nền
  tảng, hãy kiểm tra từng nơi làm gì với nó trước khi tin vào thang chất lượng.

- **Bộ điều tốc gửi phải luôn cao hơn hẳn tốc độ ra của chính encoder**: `Pacer::Gate`
  ngủ ngay trên thread mà `SendEncodedFrame` đang chạy, và trên Android đó là vòng drain
  của MediaCodec — đúng vòng phải gọi `releaseOutputBuffer` trước khi encoder giao được
  frame kế tiếp. Vì vậy điều tốc quyết định tốc độ drain, không chỉ tốc độ trên dây, trong
  khi VirtualDisplay vẫn bơm frame mới vào theo nhịp màn hình. Việc siết
  `kPacingRateMultiple` từ 2 xuống 1.2 để làm mượt burst đã được đo trên Pixel 4: thời
  gian gửi mỗi frame tăng từ 20 ms lên 63 ms trung vị, và hàng đợi encoder phình vô hạn —
  `enc_lat_ms` vượt 46 s chỉ sau 100 s, viewer tụt lại 4.6 s. Với giá trị 2, cùng kịch
  bản giữ `enc_lat_ms` ở 0. Khoảng dư đó không phải phần thừa để thu hồi; nó là thứ giữ
  cho đường mã hoá rút nhanh hơn tốc độ nạp vào. Muốn giảm burst thì dùng bộ đệm socket
  hoặc tách điều tốc khỏi thread drain, tuyệt đối không siết con số này.

- **Bộ perf gate trên chi phí, nên cần một gate thứ hai canh kết quả**: `core_perf` đo
  số lần cấp phát trên mỗi packet và cách thời gian giãn theo input, và mọi workload
  reassembler của nó đều pass trong khi một packet mất làm mất 22 % số frame nguyên vẹn
  trên đường truyền thật. Nó không thể bắt được: vứt video tốt còn *rẻ hơn* giải mã nó,
  nên chính sách hỏng lại ghi điểm cao hơn ở mọi con số bộ suite theo dõi.
  `LossGoodputTests` là bộ đi kèm, fail khi code làm ít việc hơn mức đáng phải làm — một
  đường truyền mất gói đuôi mô phỏng với vòng truyền thật, gate trên tỉ lệ frame nhận đủ
  packet mà thực sự tới được decoder, và trên khoảng cách dài nhất giữa hai frame được
  giao. Cả hai đều độc lập với máy, nên đúng như nhau trên laptop, CI runner hay điện
  thoại. Hãy nghĩ tới goodput gate mỗi khi một chính sách có thể "thành công" bằng cách
  vứt bớt việc.

- **Mất một packet chỉ tốn một frame, không phải cả khung hình tới keyframe kế tiếp**:
  trước đây bộ ghép lại bật `waitingForIdr_` với mọi lần mất, nên chỉ một packet thiếu
  là vứt sạch mọi frame *nguyên vẹn* phía sau cho tới khi có IDR mới. Đo trên host điện
  thoại qua Wi-Fi, 64 frame thực sự thiếu đã kéo theo 381 frame bị vứt — 6.4 MB video
  giải mã được bị bỏ, hình đứng trung vị 146 ms và có lúc tới 1.4 s. Giờ chỉ frame thiếu
  bị bỏ; các frame sau đi thẳng tới decoder, nơi che khuyết tham chiếu đã mất, trong khi
  `InvalidateRef` báo cho host frame nào hỏng và yêu cầu keyframe sửa lại. Vài vệt
  macroblock ngắn là cái giá cố ý trả để không đứng hình. `waitingForIdr_` giữ lại đúng
  trường hợp nó đúng: viewer vào giữa luồng chưa có tham chiếu nào nên phải đợi IDR đầu.

- **Cửa sổ chờ phải dài hơn một vòng truyền lại, nếu không NACK chỉ là trang trí**:
  trước đây một frame chỉ được cho hai chu kỳ khung hình (33 ms ở 60 fps) trước khi bị
  coi là mất, trong khi RTT đo được trên cùng đường là 24-49 ms. NACK gửi đi và câu trả
  lời về sau khi frame đã bị vứt — thấy rõ qua `late_ms_avg=24` với 87 packet mỗi giây
  rơi vào những frame không còn tồn tại. `StallTimeoutUs` giờ lấy giá trị lớn hơn giữa
  cửa sổ theo nhịp khung hình và một vòng rưỡi RTT, vẫn bị chặn bởi hard timeout, nên
  việc yêu cầu truyền lại chỉ đáng giá trên đúng những đường cần nó.

- **`FileHost` không bao giờ gửi khi đang giữ khoá của chính nó**: vòng lặp phục vụ QUIC
  chạy `QuicEndpoint::Poll` dưới `SessionTransport::sendMutex_`, và một kết nối đóng lại ở
  đó sẽ gọi thẳng ngược vào `FileHost::OnPeerGone`, nơi lấy `FileHost::mutex_`. Nghĩa là
  thứ tự `sendMutex_ -> mutex_` đã bị tầng vận chuyển ấn định. Bất kỳ đường nào lấy
  `mutex_` trước rồi mới gửi — `FileReceiver` phát một accept, một ack hay một cancel qua
  `hooks.send` — đều khép kín vòng lặp, và TSan bắt được nó dưới dạng lock-order inversion
  giữa vòng lặp nhận và một luồng UI gạt `SetAccepting(false)` khi đang có transfer chạy.
  Vì vậy các bản ghi mà receiver phát ra được xếp vào `outbox_` dưới `mutex_` rồi chỉ gửi
  sau khi đã nhả khoá, với `outboxMutex_` giữ suốt cả hai nửa để phía bên kia vẫn nhận
  đúng thứ tự chúng được sinh ra. Riêng `OnPeerGone` thì không thể gửi gì: nó vốn đã chạy
  dưới `sendMutex_`, nên nó bỏ đi những gì đã xếp hàng.

- **Bộ đo hiệu năng chặn theo số lần cấp phát và hình dạng chi phí, không theo mili-giây**:
  cả ba bộ test đều dựng bản debug, và CI còn chạy lại chúng dưới ASan, TSan và coverage —
  nơi một hạn mức thời gian đo chính sanitizer chứ không đo mã. Vì vậy `core_perf` (preset
  release, `make test-perf`) fail theo hai thứ độc lập với máy — số lần cấp phát trên mỗi
  gói, mỗi khung hình hay mỗi KB, đếm bằng cách thay `operator new` toàn cục, và một dòng
  `-scaling` có thời gian tăng nhanh hơn hẳn đầu vào — còn phần đo thời gian chỉ so với
  `out/perf/baseline.txt`, ghi riêng cho từng máy bằng `make perf-baseline` và không bao
  giờ commit. Chính cách chia đó cho phép bộ đo bắt được hồi quy kiểu "khâu ghép gói giờ
  chép mỗi mảnh hai lần" trên laptop, trên máy CI hay trên điện thoại như nhau, mà vẫn in
  ra ns mỗi đơn vị và MB/s cho những đường mà bản thân con số mới là thứ ta cần. CI chạy
  đúng hai cổng chặn độc-lập-với-máy đó trên các job release Linux và macOS; Windows chỉ
  build binary, vì `deque` của MSVC cấp phát một khối cho mỗi phần tử lớn hơn 16 byte,
  nên cùng đoạn mã lại ra số lần cấp phát khác. Pull request còn được so thời gian theo
  cách mà nhiễu của runner dùng chung không phá được — commit gốc và pull request đo trên
  cùng một runner, dung sai 50%, chỉ cảnh báo. `platform_perf` kéo dài đúng các cổng chặn
  đó xuống QUIC thật qua loopback, nơi thời gian đo chính là nhịp của vòng service —
  budget drain stream 64 KiB nhân với tick poll 1 ms — nên budget bị thu nhỏ, vòng drain
  mất tuyến tính, hay một cấp phát mới trong vòng poll đều hiện thành cú nhảy, dù chi phí
  CPU của cùng khối việc gần như không đổi.

- **Client dòng lệnh là mặt tiền thứ tư, không phải bản cài đặt thứ hai**: nó phân tích cờ
  trong `core/cli`, rồi điều khiển đúng những mảnh mà app để bàn điều khiển — `SharingHost`
  để làm host, `ScreenViewer` để xem, `TerminalViewer` để mở shell. Thứ duy nhất của riêng
  nó là cửa sổ: X11 + EGL trên Linux, còn trên Windows là chính `RunViewer` của app để bàn.
  Đó là lý do phần `cpp/` của mỗi client được tách thành thư viện tĩnh
  (`deskhub_linux_core`, `deskhub_win_core`, `deskhub_win_view`, `deskhub_mac_core`) và
  phần giao diện nằm bên trên — tách như vậy để CLI link được đường ống media mà không phải
  kéo theo GTK hay wxWidgets.

- **`preflight` chỉ chạy khi thật sự có màn hình để chụp**: mọi client dùng nó để kiểm tra
  đường chụp hình — portal xdg trên Linux, quyền Screen Recording trên macOS, thiết bị
  D3D11 trên Windows. Một phiên chia sẻ chỉ có shell thì không cần gì trong số đó, nên hỏi
  vẫn hỏi làm `share --terminal` trên máy không màn hình báo "quyền chụp màn hình đã mất".
  `HostEngine::Start` nay bỏ qua nó khi danh sách nguồn rỗng.

- **Host chỉ chia sẻ shell mà không có màn hình vẫn phải sống**: vòng lặp mạng kết thúc
  phiên khi không còn nguồn nào sống, mà phiên chỉ có shell thì theo định nghĩa là không có
  nguồn nào. `keepAlive` nay trả lời theo ý định của người gọi (`ShareOptions::terminal`),
  chứ không theo con trỏ `TerminalHost` vốn chỉ được gắn vào sau khi vòng lặp đã chạy.

- **Cổng khung hình đếm tới một mốc hạn, không đếm từ khung nó vừa giữ**: một compositor
  đưa sang 40 fps trong khi mục tiêu là 30 fps thì hầu hết các mốc 33 ms đều không có
  khung nào rơi đúng vào, nên một cổng chỉ hỏi "khung này có cách khung tôi giữ đủ xa
  không?" sẽ loại một khung xen kẽ và dừng ở 20 fps — vừa dưới mục tiêu, vừa lởm chởm,
  tức là giật hình chứ không phải luồng chậm hơn. `FrameGate` thay vào đó mang theo một
  mốc hạn chạy đều: mỗi lần nhận khung, mốc tiến đúng một chu kỳ, nên phần dư được giữ
  lại và 40 vào cho ra 30. Capture chậm hơn mục tiêu không bao giờ bị chặt bớt, và một
  mốc hạn đã tụt lại sau thời gian thực sẽ đồng bộ lại thay vì tích lũy, nên một quãng
  lặng không mua được một cú dồn khung về sau.

- **Host Linux encode trên thread riêng, và đưa cho thread đó khung hình nhỏ chứ không
  phải khung lớn**: encode ngay trong callback `process` của PipeWire từng ghìm capture
  xuống `1000 / enc_ms` fps và biến mọi dao động thời gian encode thành rung nhịp khung
  hình phía client. Giờ encode chạy trên thread riêng, được nạp qua `FrameMailbox`, một
  hàng đợi một-chỗ kiểu mới-nhất-thắng — khi encoder chậm chân, frame mới nhất thắng và
  frame cũ được đếm chứ không xếp hàng. Thứ đi qua hàng đợi là khung hình đã thu nhỏ về
  kích thước encode, còn khoảng một phần bảy số byte. Chuyển khung nguyên độ phân giải
  qua đó tốn hơn nhiều so với bản thân phép copy: 20 MB cache line bị bỏ lại ở trạng
  thái dirty trong core capture, và core encode phải kéo sang, đo được 16 ms so với
  3,4 ms cho cùng phép đọc trên vùng nhớ nó không sở hữu. Thread capture đằng nào cũng
  phải chạm mỗi pixel nguồn đúng một lần, nên đó là chỗ đúng để tiêu lượt duyệt duy
  nhất ấy. Frame dma-buf vẫn encode tại chỗ: compositor tái dùng bộ nhớ của chúng ngay
  khi callback trả về nên chúng không sống lâu hơn callback, và VA-API đằng nào cũng
  thu nhỏ chúng trên GPU.
- **Host Linux chọn encoder theo nơi frame nằm, không phải theo thứ được cài**: frame
  dma-buf đi vào VA-API, nơi import được zero-copy trên đúng GPU đã tạo ra nó; frame
  mapped (CPU) đi vào NVENC khi có driver NVIDIA, vì trên desktop do GPU NVIDIA render,
  compositor thương lượng lại screencast về shared memory, và việc encode khi đó thuộc
  về card lấy được pixel thẳng từ bộ nhớ hệ thống. `HwEncoder` đưa ra lựa chọn đó mỗi
  lần dựng lại encoder, và một frame khác loại đến sau sẽ trả về `false` — đó là tín
  hiệu để dựng lại.
- **Phần thu nhỏ ảnh trước NVENC là của chúng ta, không phải của swscale**: NVENC nhận
  pixel packed 32-bit nhưng không tự resize, còn ảnh capture là nguyên độ phân giải màn
  hình. `libswscale` đo được 9,2 ms cho 3440x1440 → 1280x534 — khoảng 2 GB/s, kém băng
  thông bộ nhớ của máy này cả một bậc, vì rescale packed-RGB rơi ra ngoài các đường đã
  tối ưu của nó. `RgbDownscale` trong `core/` là một bộ trung bình theo vùng viết đúng
  cho hình dạng này: mỗi pixel nguồn một lần nạp 32-bit, cộng dồn số nguyên, 4,0 ms cho
  cùng khung hình đó, và khử răng cưa đúng cách thay vì một mẫu bilinear như swscale.
  Tổng chi phí NVENC cho cả khung rơi vào ~5 ms, nên 60 fps còn dư chỗ.
- **Các con số hiệu năng chỉ có ý nghĩa khi đo trên bản release**: `make build-linux`
  và `make run-linux` cấu hình preset `x64-debug`, tức `-O0`, trong khi đường encode giờ
  là số học pixel nằm trong `core/`. Cùng một khung hình tốn ~19 ms ở đó so với ~5 ms từ
  `make release-linux`. Một báo cáo giật hình đo trên binary debug là đang đo kiểu build.

- **Viewer Apple hiển thị video theo PTS trên một control timebase, và pacer không bao
  giờ tin chính nó**: hiển thị mỗi frame ngay lúc nó đến khiến jitter Wi-Fi hiện ra
  thành giật hình trong khi mọi con số độ trễ vẫn đẹp — nhịp không phải là độ trễ.
  `VideoPacer` (core, test offline được) ánh xạ PTS của host sang giờ hiển thị local
  theo đúng cách metric e2e làm — minimum theo cửa sổ của `arrival − pts` — cộng một
  khoảng đệm ~33 ms để jitter được trả từ đó, và `VtDecoder` lái control timebase của
  `AVSampleBufferDisplayLayer` theo nó, chỉ resync khi lệch quá 250 ms. Một cú nhảy pts
  quá 2 s được đọc là stream mới chứ không phải jitter, nên ánh xạ được dựng lại thay
  vì đứng hình suốt một cửa sổ. Vì không thể chứng minh từ đây rằng renderer tôn trọng
  timebase ngoài trên mọi phiên bản OS, decoder tự canh lưng mình: một chuỗi frame bị
  hàng đợi renderer đầy nuốt mất sẽ lật về display-immediately và flush — thà mất phần
  mượt còn hơn mất hình.

- **Audio là một khung một datagram, và mất thì không đuổi theo**: một khung Opus 20 ms
  ở 64 kbps đo được khoảng 160 byte, rộng nhất 209 byte, so với 1180 byte một datagram
  chứa được — nên đường audio không có packetizer, không FEC, không reassembler, không
  NACK, tức là bỏ đi gần hết những gì đường video có. Mất gói được hấp thụ ở chỗ rẻ
  nhất: Opus mang sẵn FEC trong khung kế tiếp, và bên nhận bảo bộ giải mã che chỗ hổng
  mà jitter buffer báo. Gửi lại còn tệ hơn vô ích, vì một khung đến muộn 200 ms thì
  không phát được nữa nhưng vẫn kịp làm chậm mười khung sau nó. `make opus-smoke` đo
  đúng những con số đó trên bất kỳ máy nào dựng được thư viện.
- **Jitter buffer không có timer nào bên trong**: `AudioJitterBuffer` là trạng thái
  thuần, còn độ trễ mục tiêu chỉ là số khung nó nạp trước khi bắt đầu — 60 ms là ba
  khung. Nhờ vậy toàn bộ phần này test được offline mà không phải ngủ, và các kiểu
  hỏng đều hiện rõ: bùng gói thì bị chặn thay vì xếp hàng, hết gói thì nạp lại thay
  vì giật, số thứ tự nhảy xa thì coi là luồng mới thay vì hàng nghìn khung mất. Phần
  giữ nhịp nằm ở `AudioPlayer`: nó bơm một khung mỗi 20 ms đồng hồ tường vào một
  vòng PCM mà callback phát của sink rút ra.
- **Callback thu âm không bao giờ mã hoá**: PipeWire và ScreenCaptureKit giao audio
  trên thread real-time với deadline vài mili giây, và trễ deadline ở đó làm xrun cả
  phần phát của chính máy host, không riêng gì Deskhub. Mã hoá Opus mất 0.3–1.5 ms
  kèm lúc trồi sụt, và trước đây còn kéo theo một `sendto` cho mỗi viewer trên đúng
  thread đó. `AudioBroadcaster::Offer` giờ chỉ chép khung 20 ms vào một vòng slot
  lock-free cấp phát sẵn và đóng dấu thời điểm thu; một thread worker lo mã hoá,
  chẩn đoán và gửi cho từng viewer. Worker chậm chân thì tốn một lần rơi có đếm
  (`framesRefused`), không bao giờ thành tiếng rè trên máy host.
- **Tiếng cần cả hai đầu đồng ý, và client cũ không bao giờ nghe thấy**: viewer đặt bit
  0 của `Hello.features`, host quảng bá `kHostSharesAudio` trong phần năng lực của nó,
  và host chỉ gửi gói cho viewer nào có bit đó. Chính điều này giữ `kProtocolVersion` ở
  mức 2: viewer 5.0.x gửi `features = 0`, nên host 5.1 không bao giờ đặt lên dây một
  thông điệp mà nó không phân tích được.

- **Link terminal tự giữ sống và tự quay số lại**: viewer terminal có QUIC connection
  riêng, tách khỏi phiên video, nên không keepalive nào của đường video chạm tới nó.
  Để yên ở dấu nhắc thì nó không có lưu lượng gì cả và chết vì idle timeout 30 giây
  của QUIC; sau đó viewer dừng luôn thread ở trạng thái `Reattaching` mà không hề
  quay số lại — trong khi shell vẫn đang chờ trên host suốt 2 phút và không ai quay
  lại lấy. Giờ `TerminalViewer` gửi gói ack-eliciting theo chu kỳ và quay số lại có
  backoff, dùng `TerminalClient::Reattach()` (vốn đã viết và có test trong core,
  chỉ là chưa ai gọi) để lấy lại đúng shell cũ kèm scrollback.
  `deskhub::KeepaliveIntervalUs` / `ReconnectDelayUs` giữ các mốc thời gian trong
  core: keepalive tối đa bằng nửa idle timeout để mất một gói vẫn sống, và việc thử
  lại dừng đúng ở `kTerminalReattachGraceUs`, vì quá mốc đó host đã bỏ shell rồi,
  kết nối lại chỉ âm thầm mở một shell mới.
- **Một record lên stream thì lên trọn vẹn, còn client tụt lại thì được vẽ lại chứ
  không nhận từng byte**: mọi thứ tin cậy — control, auth, output terminal — đều là
  record có tiền tố độ dài dùng chung một QUIC stream, nên nửa record trên đường
  truyền làm lệch khung vĩnh viễn ở đầu bên kia; `RecordStream` không có cách nào
  đồng bộ lại và peer đóng luôn kết nối. `QuicEndpoint::SendStream` trước đây ghi
  được bao nhiêu hay bấy nhiêu rồi bỏ phần dư, chịu được cho tới khi một lệnh như
  `make test` chạy nhanh hơn đường truyền: cửa sổ stream 1 MiB đầy, phần đuôi của một
  record `TermData` bị bỏ, framer của viewer hỏng và shell "mất kết nối" một phút sau
  khi mở. Giờ nó từ chối cả record khi stream không còn chỗ, và đóng kết nối nếu vẫn
  lỡ ghi được một phần, vì stream đã rách thì không vá tại chỗ được. Ở tầng trên,
  `TerminalHost` giữ output chưa gửi trong hàng đợi riêng của từng shell và thử gửi
  lại ở mỗi nhịp, nên một đợt xả chỉ nhanh hơn đường truyền trong chốc lát — output
  của một lần build chẳng hạn — vẫn tới client đủ từng byte. Vượt `kMaxPendingBytes`
  thì hàng đợi bị bỏ chứ không
  phình thêm: mọi byte đều đã vào `Screen` gương phía host rồi, nên client được kéo
  cho kịp bằng `deskhub::term::RenderScreen` — vẽ lại lưới hiện tại, nhiều nhất một
  lần mỗi `kRepaintIntervalUs`. Phần output không ai kịp đọc thì bỏ chứ không đệm, nhờ
  vậy lệnh xả dữ liệu vẫn chạy đúng tốc độ của nó mà màn hình cuối cùng vẫn đúng.
  Client gắn lại phiên cũng nhận đúng bản vẽ lại đó, vì sau một quãng đứt thì vị trí
  của nó trong dòng byte không còn nghĩa gì.
- **Chia sẻ tự động phải chờ desktop, không phải liệt kê một lần rồi thôi**: Windows
  đăng ký autostart bằng scheduled task `ONLOGON`, chạy trước khi phiên có màn hình
  nào để liệt kê, nên một lần gọi `ListDisplays()` lúc dựng cửa sổ trả về rỗng và app
  báo là không có gì để chia sẻ. `deskhub::ui::AutoShareGate` (trong core, có unit
  test) giữ quy tắc thử lại — dò mỗi `kAutoShareProbeMs`, bỏ cuộc sau
  `kAutoShareGiveUpMs` — và mỗi client tự chạy nó bằng timer của mình, nên chính sách
  chỉ tồn tại một chỗ. `NextAutoShareStep` là đúng quy tắc đó nhưng không giữ trạng
  thái, và đó là thứ client Swift gọi qua `dh_auto_share_step`. Một lần chia sẻ tự
  động không bao giờ mở hộp thoại modal: lúc đăng nhập cửa sổ có thể đang ẩn trong
  khay hệ thống, nơi hộp thoại vừa không nhìn thấy vừa chặn việc chia sẻ vĩnh viễn,
  nên các lý do từ chối đi vào banner trang Host và log. Các client desktop cũng làm
  mới danh sách nguồn theo tín hiệu đổi màn hình của OS, nhờ vậy danh sách vẫn đúng
  khi cắm thêm màn hình về sau.
- **Chọn quiche thay vì msquic/ngtcp2**: thư viện QUIC duy nhất có bằng chứng chạy
  thật trên cả Android lẫn iOS. Nó mang theo BoringSSL, thứ phục vụ luôn SPAKE2 và
  danh tính máy — không cần thư viện mật mã thứ hai.
- **Không có connection migration**: không thư viện ứng viên nào hỗ trợ dùng được
  phía client. Kết-nối-lại-và-gắn-lại (kiểu tmux, vốn đã bắt buộc cho mobile chạy
  nền) là đủ.
- **ECDSA P-256, không phải Ed25519**: phía server của BoringSSL không ký bắt tay
  TLS bằng Ed25519 qua quiche. Đừng đổi lại. Khoá Ed25519 còn lưu trên đĩa được
  thay ngay khi nạp — để nguyên thì mọi bắt tay chết với `QUICHE_ERR_TLS_FAIL` mà
  màn hình không hiện gì.
- **Verifier của passcode là một lần SHA-256, không phải KDF đắt tiền**: SPAKE2 đã
  giới hạn kẻ tấn công còn đúng một lần đoán online mỗi kết nối và không để lại
  transcript nào đáng mang về crack offline — đó chính là việc mà độ nặng của KDF
  sinh ra để làm.
- **quiche là thư viện build sẵn, không phải FetchContent**: `scripts/build-quiche.sh`
  ghi mỗi rust target một thư mục dưới `third_party/quiche/` cộng một `include/`
  dùng chung — quiche.h và bộ header BoringSSL mà boring-sys vendor, được chép ra vì
  Deskhub gọi thẳng BoringSSL cho danh tính host và muốn một đường include duy nhất,
  không thư viện TLS thứ hai. `DeskhubQuiche.cmake` biến chỗ đó thành
  `deskhub::quiche`; thiếu thư viện là configure lỗi.
- **Apple link `libplatform_bundled.a`**: app Xcode tiêu thụ archive platform từ
  ngoài CMake, nơi link PRIVATE tới quiche không bao giờ tới được dòng link của
  chúng — nên một bước `libtool` gộp platform + quiche thành đúng một archive mà
  `.pbxproj` link.
- **Bãi mìn toolchain Windows đã được dọn — giữ nguyên như vậy**: quiche build với
  CRT tĩnh qua `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS` cho phần Rust cộng
  `/MT` trong `CFLAGS_x86_64_pc_windows_msvc` cho phần BoringSSL (mặc định của msvc
  là runtime DLL, còn ép flag qua `RUSTFLAGS` chung thì cargo build hỏng thẳng), cả
  cây pin `MultiThreaded` cho khớp để exe không cần VC++ Redistributable; wxWidgets
  re-pin `wxBUILD_USE_STATIC_RUNTIME` mỗi lần configure vì `wx_option()` cache vĩnh
  viễn. BoringSSL phải build dưới generator Visual Studio mặc định — crate cmake chỉ
  truyền được /MT qua flag per-config ở đó, nên ép `CMAKE_GENERATOR=Ninja` là
  BoringSSL âm thầm quay về /MD và bước link cuối chết với LNK2038; nếu MSBuild dính
  MSB6003 vì path dài thì bật Windows long paths thay vì đổi generator. `link.exe`
  của Git Bash trong `/usr/bin` che mất linker MSVC (đặt thư mục của `cl.exe` lên
  trước), cơ chế rewrite path của nó bóp méo tham số kiểu `/...`
  (`MSYS2_ARG_CONV_EXCL`), và installer của NASM không đụng vào PATH.
- **quiche cho Android bỏ qua cargo-ndk trên máy Windows**: cargo-ndk đưa cho
  boring-sys đường dẫn `clang` không có phần mở rộng, CMake trên Windows từ chối nó,
  nên `build-quiche.sh` tự đặt `CC_*`/`CXX_*`/`AR_*`, linker của cargo và `--target=`
  cho từng ABI rồi gọi cargo thuần. BoringSSL ở đó vẫn cần Ninja (generator Visual
  Studio không nhắm được NDK), còn bindgen lấy libclang của Visual Studio — nó tìm
  `stddef.h` cạnh binary của chính nó — nên `BINDGEN_EXTRA_CLANG_ARGS` trỏ sang
  resource header của NDK bằng dấu gạch chéo xuôi, vì bindgen tách biến đó theo luật
  shell và nuốt mất dấu gạch chéo ngược.
- **Mỗi app cross-compile tự build quiche của mình trước**: `build-android`,
  `build-ios`, `build-macos` và `build-linux` phụ thuộc vào một target quiche cho ABI
  của chúng, giống như `debug`/`release` làm cho host. quiche là per-ABI và bước
  configure của CMake thất bại nếu thiếu, nên một bản build bỏ qua bước này trông như
  hỏng toolchain chứ không như thiếu thư viện — và một app kẹt lại ở lần build thành
  công cuối cùng sẽ nói thứ giao thức mà các máy khác không còn trả lời.
- **quiche cho iOS pin `IPHONEOS_DEPLOYMENT_TARGET=17.0`**: clang của boring-sys
  trôi theo mặc định SDK trong khi rustc link theo minimum của riêng nó, và độ lệch
  hiện ra thành `___chkstk_darwin` undefined lúc link.
- **Hai đồng hồ, có chủ đích**: `NowUs()` là monotonic (giây uptime) cho khoảng
  thời gian; `NowUnixSeconds()` là cái duy nhất hiện ra thành ngày tháng. Trộn lẫn
  không kêu — một mốc monotonic đem lưu sẽ hiện thành một thời điểm nào đó trong
  ngày 1 tháng 1 năm 1970.
- **Tiến trình con của PTY Windows không nhận handle chuẩn nào**: khi stdout của
  chính host bị redirect, Windows truyền redirect đó xuống con bất chấp thuộc tính
  pseudo-console và shell nói chuyện với pipe; không đưa handle nào thì nó quay về
  console — chính là ConPTY vừa gắn.
- **`wxWANTS_CHARS` trên lưới terminal Windows**: thiếu nó thì điều hướng dialog
  của frame nuốt Enter, Tab và các phím mũi tên trước khi terminal kịp thấy.
- **TCC của macOS gắn quyền với chữ ký code**: app.app build tay (ký ad-hoc, đổi
  chữ ký mỗi lần build) và bản dmg ký Developer ID giành nhau đúng một dòng
  `com.deskhub.macos` — System Settings hiện đã cấp quyền trong khi bản vừa chạy bị
  từ chối, âm thầm với Accessibility. `make reset-macos-permissions` xoá mọi quyền
  để lần chạy sau hỏi lại.
- **macOS là một bản desktop trong CI và một bản đã ký khi phát hành, không bao giờ
  cả hai cùng lúc**: `build-desktop` biên dịch app với chữ ký ad-hoc ở mọi lần push,
  nên một thay đổi Cocoa không build được sẽ fail ngay tại pull request của nó;
  `deploy` đi tới cùng app đó qua `release-macos`, đường fastlane — Developer ID,
  notarize, dmg — thứ tạo ra bản người dùng mở được thật. Vì vậy workflow dùng lại
  bỏ qua job macOS của nó khi `for_release` được bật, nếu không một tag sẽ trả tiền
  cho một macOS runner thứ hai chỉ để dựng một bundle chẳng ai ship. `build-mobile`
  chỉ còn iOS và Android, cùng lý do và cùng cách tách.
- **Mọi workflow lấy quiche và opus từ một action duy nhất, và cache key chính là toàn
  bộ giao kèo**: `.github/actions/third-party` dựng cả hai thư viện cho bất kỳ tập
  target nào job khai báo, nhờ vậy mười chín bản sao của cùng một khối cache-rồi-build
  rút xuống còn một dòng mỗi job. Input `cache-key` của nó không phải để trang trí — đó
  là thứ duy nhất ngăn hai job khôi phục nhầm thư viện của nhau. Hai tập target thì khác
  nhau, mà hai runner image dựng cùng một triple cũng khác: một `libquiche.a` biên dịch
  trên ubuntu-latest rồi khôi phục trên ubuntu-22.04 sẽ link vào đúng cái glibc mà bản
  phát hành sinh ra để tránh. Thứ gì làm đổi kết quả build thì thuộc về key đó.
- **Một CRT release tĩnh trên Windows, cho mọi cấu hình**: cargo build quiche với
  CRT release tĩnh (mặc định của msvc — đừng bao giờ ép qua `RUSTFLAGS`, flag đó
  ngấm vào proc-macro và giết cargo), và cả cây CMake pin `MultiThreaded` cho khớp
  — cũng chính là thứ giữ app là một exe duy nhất không cần VC++ Redistributable.
  Rust không có bản debug-CRT nên Debug cũng phải khớp: `_ITERATOR_DEBUG_LEVEL=0`,
  `/U_DEBUG`, bỏ `/RTC1` — CRT release không có `_CrtDbgReport` lẫn hỗ trợ
  run-time check. Lệch bất kỳ chỗ nào là dính một tràng LNK2038.
- **Passcode = cửa tự phục vụ, approval = đường lui**: mã đã gõ luôn được kiểm;
  không mã thì người quyết. Passcode không bao giờ qua mạng dưới bất kỳ dạng nào kẻ
  tấn công mang về được.
- **Bộ giả lập VT là của dự án**: không widget terminal nào có mặt trên cả năm
  client với giấy phép dùng được, và tự sở hữu nó làm hành vi terminal test được
  offline và giống hệt nhau mọi nơi.
- **Mirror shell phía host được nuôi từ byte đầu tiên**: đầu ra PTY là luồng
  một-người-đọc và huỷ khi đọc — byte đã đọc và gửi cho viewer không phát lại được —
  nên lưới mà *Stop & attach* mở ra phải được dựng ngay khi byte đi qua, không phải
  lúc bấm nút. Khi viewer từ xa còn gắn, phản hồi truy vấn terminal của chính mirror
  bị vứt bỏ: màn hình của viewer đã trả lời rồi, và shell không được nghe hai câu
  trả lời.
- **Một cổng**: beacon, màn hình và terminal dùng chung một listener; QUIC ghép kênh
  kết nối và stream. Cổng thứ hai ngày trước tồn tại chỉ vì đường màn hình tiền-QUIC
  chiếm trọn socket.
- **Một `HostLink`, bốn bắt tay ngày trước**: quay số + kiểm tra tin cậy + auth +
  khôi phục từng được viết bốn lần ở phía client — truy vấn nguồn, viewer, bên gửi
  file, và terminal trên một `QuicEndpoint` thô của riêng nó — và đó là lý do cửa sổ
  gửi file biết đến chuyện key host đổi muộn hơn viewer tận ba lần sửa. Giờ
  `HostLink` là mảnh code phía client duy nhất quay số hay auth; mỗi dịch vụ mở
  `Chan` của nó, nhận hàng đợi inbox riêng và tự rút trên luồng của mình. Quay-số-lại
  theo backoff của terminal chuyển vào link để mọi bề mặt bật khôi phục đều hưởng, và
  luật tin cậy nằm ở đúng một chỗ: key đổi thì link đỗ ở `Deciding` chờ người trả lời
  (chỉ truy vấn nguồn cho đi qua, `trustGate=false`, không ghi nhớ gì — nơi gọi nó
  không có prompt để hiện), và chỉ passcode được host chứng minh bằng mật mã mới tự
  ghim key.
- **`HostLink` gửi qua `Send`, không phải `SendMessage`**: trên Windows, các header
  hệ điều hành phía sau tầng platform định nghĩa `SendMessage` là macro thay cho
  `SendMessageA`, và trong `HostLink.cpp` chúng vào sau phần khai báo class nhưng
  trước phần định nghĩa method — MSVC khi đó đòi định nghĩa cho một thành viên
  `SendMessageA` mà không header nào khai báo. Các tên API Win32 (`SendMessage`,
  `PostMessage`, `CreateWindow`, `GetObject`, …) không bao giờ an toàn làm tên method
  trong bất kỳ translation unit nào một header hệ điều hành với tới được; cách sửa là
  đổi tên, không phải `#undef`.
- **Phiên ScreenCast của portal sống chết theo một kết nối D-Bus**: GLib cache session
  bus dùng chung bằng tham chiếu yếu, nên `g_object_unref` trên handle cuối cùng sẽ huỷ
  luôn kết nối. `xdg-desktop-portal` khi đó bỏ phiên, compositor xoá node PipeWire, và
  node id mà portal vừa trao lại trỏ vào hư không — luồng đi tới `paused` rồi hỏng với
  *no target node available*. Vì vậy `PortalScreenCast` tự giữ `GDBusConnection` của nó
  suốt thời gian phiên còn mở, thay vì mượn một kết nối cho mỗi lời gọi. Ứng dụng desktop
  che lấp lỗi này rất lâu vì GTK giữ một tham chiếu tới session bus trong suốt vòng đời
  tiến trình; `deskhub-cli` không liên kết GTK nên không có tham chiếu nào.
- **Mọi icon đều được dẫn xuất, và chỉ một phần được bo góc**: `make icons` dựng lại
  toàn bộ bộ icon từ một file gốc duy nhất `assets/icon_1024.png`. macOS, iOS, trang
  Play Store và đường adaptive-icon của Android tự cắt artwork theo hình dạng riêng
  của chúng, nên các asset đó giữ nguyên hình vuông tràn viền; Windows, Linux và
  launcher Android trước API 26 vẽ đúng những gì được đưa, nên icon của chúng phải có
  sẵn góc bo và phần trong suốt nướng vào ảnh — nếu không, app hiện ra như một ô vuông
  xanh cứng cạnh mọi icon bo góc khác. `scripts/make-icons.py` cố ý chỉ dùng thư viện
  chuẩn: bootstrap không cài công cụ xử lý ảnh nào.
- **Client desktop giữ nhiều host cùng lúc; điện thoại giữ một**: trang kết nối trên
  Windows, Linux và macOS không giữ trạng thái đã-kết-nối nào của riêng nó. Host nào trả
  lời thì được một cửa sổ kết nối — `ConnectionFrame` trong
  `client/windows/win32/MainFrame.cpp`, `ConnectionWindow` trong
  `client/linux/gtk/MainWindow.cpp`, `WindowGroup` tên `connection` trong
  `client/macos/app/swift/App.swift` — nắm địa chỉ, passcode, khả năng, danh sách nguồn và
  ô control của riêng host đó, nhờ vậy trang kết nối luôn rảnh để gọi host tiếp theo. Cửa
  sổ chính chỉ giữ danh sách các cửa sổ đang mở, để đưa cửa sổ cũ lên trước khi cùng một
  host được gọi lần hai, để đẩy mỗi nhịp dò trạng thái tới đúng cửa sổ có địa chỉ khớp, và
  để đóng hết khi thoát app. Android và iOS cố ý giữ một kết nối: màn hình điện thoại
  không đủ chỗ cho một panel thứ hai, và phiên nó mở ra vốn đã chiếm toàn màn hình.
  `ui::SameDeviceAddr` là định nghĩa của "cùng một host" ở mọi nơi — xem mục dưới.
- **Một host, hai cách viết, một phép so sánh**: `ScanAddressText` bỏ cổng khi cổng là mặc
  định, nên một dòng quét được đọc là `192.168.1.60` trong khi địa chỉ người dùng gõ và
  kết nối lại là `192.168.1.60:47777`. So sánh hai chuỗi đó thất bại trong im lặng, và mọi
  chỗ từng làm vậy đều mất một thứ có thật: panel đã kết nối không tìm ra dòng thiết bị
  khớp nên không hiện ping, còn `PasscodeForDevice` không tìm ra mã đã lưu cho host được
  chọn từ danh sách quét. Vì vậy phép so sánh địa chỉ đi qua `ui::NormalizedDeviceAddr` /
  `ui::SameDeviceAddr` (`core/ui/Strings.h`), mở ra cho client Swift và Kotlin dưới tên
  `dh_same_device_addr`. Đừng bao giờ so sánh hai địa chỉ thiết bị bằng `==`.
- **Parity đo theo gói dài nhất trong group, không đo theo MTU**: mọi gói parity đều phát ở
  nguyên `Packetizer::kParityStride` (`kFecLenPrefix + kMaxVideoPayload`, 1176 B) bất kể các
  gói dữ liệu nó bảo vệ ngắn tới đâu, nên một frame vừa đúng một gói — hình hài thường thấy
  của delta frame ít chuyển động — vẫn mua trọn một gói parity full-MTU để bảo vệ vài trăm
  byte, tức overhead 100 %. Nay parity được cắt xuống `kFecLenPrefix` cộng gói dữ liệu dài
  nhất trong group; `BuildFecPacket` vốn đã nhận span độ dài thay đổi còn
  `Reassembler::TryRecover` vốn cũng chỉ cần đúng chừng đó và từ chối bất kỳ parity nào ngắn
  hơn, nên định dạng trên dây không phải đổi. Cần biết việc này **không** sửa được gì: vì
  packetizer rải bước (`i % numGroups`) và chỉ gói cuối của một frame mới ngắn, nên từ hai
  gói trở lên mọi group vẫn chứa một gói full-MTU và vẫn nhận parity nguyên stride. Trên kích
  thước đó, cái quyết định chi phí là **số** gói parity, `ceil(count / kFecGroupSize)` — frame
  9 gói trả 22 %, không phải 12,5 %. Overhead của FEC là một đường cong theo kích thước
  frame; nó không bao giờ là con số 1/8 duy nhất mà group size gợi ra.
- **Một datagram bị transport từ chối trông không khác gì một datagram mạng làm rớt**:
  `quiche_conn_dgram_send` thất bại khi congestion control của chính quiche hoặc
  `kDatagramQueue` của nó không nhận gói, còn `SendDatagram` rẽ nhánh trên kết quả đó chỉ để
  trả nó về — bản thân tín hiệu thì bị vứt đi. Gói như vậy không bao giờ rời khỏi máy, nhưng
  `Reassembler` bên viewer chỉ thấy một lỗ hổng bình thường và báo lên là mất gói, nên host
  có thể tốn parity FEC cho, và tự hạ bitrate encoder vì, chính những gói mình đã vứt.
  `QuicSendStats::datagramsRefused` đếm chúng, và dòng `evt=sum` của host mang `dgram_tx` và
  `dgram_refused` theo từng cửa sổ, ngay cạnh các con số mất gói. Hãy đọc hai số đó trước khi
  tin bất kỳ phép đo FEC hay điều khiển tắc nghẽn nào: congestion control của quiche nằm dưới
  đường datagram, nối tiếp với `BitrateController`, và không cái nào biết cái kia tồn tại.
- **Số group FEC nay đi trong chính byte header vốn luôn bằng không**: trước đây hai đầu đều
  tự suy ra `ceil(pktCount / kFecGroupSize)`, và điều đó buộc chặt hai thứ vốn tách rời: bao
  nhiêu gói dùng chung một gói parity, và hai gói liên tiếp cách nhau bao xa về group.
  Độ sâu interleave vì thế là hệ quả của kích thước frame chứ không phải một lựa chọn:
  keyframe 40 gói được depth 5, còn delta frame 8 gói — hình hài thường thấy ở 60 fps — chỉ
  có đúng một group và interleave bằng không, nên khả năng chống burst mạnh nhất lại nằm
  đúng chỗ ít cần nhất. `FecHeader::groups` nay mang con số đó trong byte mà
  `BuildFecPacket` trước kia luôn ghi 0; giá trị 0 vẫn có nghĩa "tự suy ra", nên header
  không đổi kích thước và `kProtocolVersion` không phải tăng. `FecGroupCount` là hàm duy
  nhất cả hai đầu gọi, và nó kẹp số group đã báo xuống theo số gói để hai bên không bao giờ
  bất đồng về chỉ số group. Cái giá phải trả: một gói dữ liệu không còn ánh xạ được vào
  group trước khi gói parity đầu tiên của frame đó tới — vô hại, vì muốn phục hồi thì đằng
  nào cũng cần parity. Đo trong `LossGoodputTests` ở mức 5% loss Gilbert-Elliott với burst
  trung bình 4 gói: số lần xin keyframe giảm từ 171/phút ở depth suy ra xuống 81/phút ở
  depth 8, và tỉ lệ frame hỏng được cứu tăng từ 34% lên 79%.
- **Sơ đồ FEC được cấu hình ở cả hai đầu, không bao giờ được thương lượng**: `FecScheme` là
  một interface với đúng một bản đăng ký, `xor`, và `Packetizer` cùng `Reassembler` mỗi bên
  giữ một cái. Trên dây không có gì nói gói parity do sơ đồ nào viết ra — định dạng payload
  là việc riêng của sơ đồ — nên một host và một viewer được đặt hai sơ đồ khác nhau sẽ không
  cứu được gì, và sự lệch đó chỉ lộ ra dưới dạng phục hồi không bao giờ chạy. Điều này là cố
  ý khi mới có một bản triển khai: `--fec NAME` trên `deskhub-cli share` và `connect` tồn
  tại để đo các phương án với nhau, bị từ chối ngay lúc phân tích tham số nếu bản build
  không có sơ đồ mang tên đó, và không phải một tuỳ chọn cho người dùng chọn. Việc báo sơ đồ
  trên dây là phần việc của bản nào thắng bake-off và trở thành đường chạy thật duy nhất;
  cho tới lúc đó `--fec` là một dụng cụ đo, phải đặt giống hệt nhau ở hai đầu cùng một phiên.
- **Chỉ mô hình mất gói theo cụm mới xếp hạng được các độ sâu interleave**: link sim trong
  `LossGoodputTests` bỏ gói theo một chuỗi Markov hai trạng thái có seed, tham số hoá bằng
  tỉ lệ mất và độ dài burst trung bình, và chạy song song một mô hình uniform random làm đối
  chứng. Đối chứng đó không phải để trang trí — nó là phép kiểm tra rằng mô hình burst thật
  sự đang tạo ra cụm. Ở cùng mức 5% loss với một gói parity mỗi group, mất gói rải đều được
  cứu 71%, còn mất gói theo cụm chỉ 34%, bởi vì mất lẻ rải rác đúng là thứ một gói parity
  hấp thụ được, còn một burst nằm gọn trong một group đúng là thứ nó không thể. Nếu hai mô
  hình cho cùng kết quả thì chuỗi Markov đã suy biến thành mất gói độc lập, và mọi thứ hạng
  về depth hay sơ đồ rút ra từ nó đều vô nghĩa. Mỗi điểm quét in ra một dòng `[csv]`, nên
  các con số đứng sau bất kỳ quyết định FEC nào cũng tái lập được bằng cách chạy lại test.
- **Thứ quyết định FEC có làm được gì hay không là chính sách bật nó, không phải sơ đồ FEC**:
  host bật parity khi viewer báo có mất gói và tắt lại sau `kCleanSecondsBeforeDroppingFec`.
  Hai chi tiết làm chính sách đó mù với đúng loại mất gói mà link thật có.
  `MakeFeedback` làm tròn loss về số nguyên phần trăm
  (`fb.lossPct = uint8_t(std::lround(w.lossPct))`), nên mọi giá trị dưới 0,5% tới
  `BitrateController` đều thành 0%; còn điều kiện bật là `fb.lossPct >= 1`, nên một link mất
  một gói mỗi giây — 0,1% của một giây 540 gói — không bao giờ bật FEC. Đo với host thật qua
  WiFi nhà: 5 lần mất gói đơn lẻ trong 196 giây, `fec_rx` bằng 0 ở cả 118 cửa sổ, và mỗi gói
  mất tối đa 1174 byte phải trả bằng một frame bị bỏ cộng một IDR nguyên khung ~150 KB. Với
  host thứ hai thì bắt được trọn chu kỳ: sạch 10 cửa sổ, một cửa sổ 0,5%, có parity đúng 10
  cửa sổ tiếp theo, rồi tắt — 1224 gói parity đổi lấy 1 gói vá được. `LossGoodputTests` nay
  chạy chính `BitrateController` thật qua chính `MakeFeedback` thật, nên sim tái hiện được
  điều này: tại điểm vận hành đo được (0,1% loss, burst 1), FEC luôn bật cứu 16/16 frame hỏng
  và không phải xin keyframe lần nào, còn chính sách đang ship chỉ bật 30% thời gian, cứu
  6/16, và xin 20 IDR mỗi phút. Mọi phép so sánh giữa các sơ đồ FEC giả định parity có trên
  dây đều đang đo một cấu hình mà chính sách này hầu như không bao giờ tạo ra.
- **Hàm mục tiêu phải đếm theo nguyên nhân, nếu không nó đếm nhầm thứ**: A1 chấm điểm FEC
  bằng số lần xin keyframe mỗi phút, nhưng chỉ `KeyframeReason::Loss` là do mạng gây ra.
  Trong 148 cửa sổ với hai viewer trên một link báo 0% loss và không có reconfig nào, host vẫn
  phát ra 10 IDR nguyên khung ~72 KB — tất cả đều do viewer xin sau khi hàng đợi decode của
  chính nó bị tràn. `q_overflow`, `dec_fail` và `display_congested` đến từ pipeline của
  client, còn `wait_idr` đến từ lúc khởi động; gộp chung lại sẽ thổi phồng điểm số của bất kỳ
  thay đổi transport nào đang được thử. Nguyên nhân vốn đã có trong dòng log mà không có
  trong bộ đếm nào, nên `KeyframeRequestLog` nhận một `KeyframeReason` có kiểu và in
  `evt=kf_sum` kèm số đếm từng nguyên nhân mỗi cửa sổ. Việc đó sửa được phía viewer nhưng để
  host mù: host mới là bên tiêu tốn cái IDR, mà `RequestKeyframe` lại là một datagram rỗng,
  nên mọi yêu cầu trông giống hệt nhau đối với chính cái máy mà điểm số đang nói về bitrate
  của nó. Bản tin nay chở nguyên nhân bằng một byte payload, `ScreenHostSession` chuyển nó cho
  `onKeyframeRequest`, và host in `evt=kf_req_sum` tách theo đúng cách đó. Hai chi tiết khiến
  nó dùng được: một peer dựng từ trước khi có byte này không gửi payload nào và rơi vào
  `unknown` chứ không rơi vào ô đầu tiên của enum, và đường tự nối lại giữa chừng của chính
  host được gán nhãn `viewer_join` chứ không mượn nguyên nhân của một viewer. Tách con số đó
  ra trước khi dùng nó chấm điểm bất cứ thứ gì.
- **Việc retransmit có cứu được gì hay không do thời gian giữ frame quyết định, không phải do
  đường retransmit**: `PlanNack` và `RetransmitCache` chạy đúng như viết — trong sim, host
  phục vụ đủ mọi chỉ số viewer hỏi, và mọi gói phục vụ đều tới nơi. Nó cứu được **không** frame
  nào. `PopReady` bỏ một frame non-IDR chưa đủ gói ngay khi có hai frame mới hơn đã hoàn
  chỉnh, ở 60 fps là khoảng 33 ms, trong khi một NACK tốn trọn một round trip cộng thêm 2 ms
  `kNackHoldUs`: ở round trip 40 ms của một link gia đình, gói vá về sau thời điểm frame nó
  định vá đã bị vứt khoảng 24 ms. Rút round trip xuống 4 ms trong cùng sim thì chính đường đó
  bắt đầu cứu được frame. Vậy phương án "NACK-only" của A1 hoàn toàn không phải câu hỏi về
  retransmit — nó là câu hỏi về `kStallTimeoutMultiple` và luật overtaken đặt cạnh RTT, và mọi
  phương án lai chuyển giữa FEC và NACK theo RTT thực chất đang chuyển theo việc frame có còn
  đó lúc gói vá tới hay không. Cũng cần biết: khi parity đang bật thì không frame nào thiếu gói
  đủ lâu để bị NACK — FEC và NACK không bao giờ tranh nhau cùng một lần vá, nên đóng góp của
  chúng cộng vào nhau chứ không chồng lên nhau.
- **Luật giữ frame mới là toàn bộ câu trả lời, và đọc nó ở đúng một điểm vận hành đã che mất
  điều đó**: đoạn ngay trên kết luận NACK-only là ngõ cụt. Nó được đo ở 0,1% loss với luật
  overtaken cố định ở hai frame mới hơn — nên nó đo *luật*, không đo retransmit. Một phép quét
  72 điểm trên chế độ vá × RTT × độ dài giữ nói khác hẳn. Ở 5% loss, burst 4, RTT 40 ms:
  NACK-only cứu 11 frame hỏng và xin 171 keyframe/phút dưới luật đang ship; để frame chờ đủ lâu
  cho chính bản vá của nó kịp về thì cùng đoạn code đó cứu **30** và chỉ xin **72** — tốt hơn
  `fec-only` ở cùng điểm (cứu 21, xin 171) mà **không gửi một gói parity nào**. Ở 80 ms quy luật
  vẫn giữ, và `fec+nack` đạt 63 lần/phút, con số tốt nhất trong cả lưới. Ở RTT 4 ms thì không
  đổi gì, vì bản vá vốn đã về kịp trong hai frame: cái lợi thuần tuý là hàm của RTT so với chu
  kỳ frame, và đó là lý do `OvertakenLimit()` **suy ra** con số từ cửa sổ vá chứ không nâng một
  hằng số. Thứ nó không mua được là độ trễ miễn phí — khoảng cách dài nhất giữa hai frame được
  giao dịch chuyển cả hai chiều trong lưới, chỗ tăng chỗ giảm, vì bớt được keyframe có thể trả
  thừa cho cái chờ dài hơn. Một
  kết quả âm tính đo ở đúng một điểm vận hành chỉ là phát biểu về điểm đó, và lần này nó đã che
  mất một hệ số ba.
- **Phần suy ra đó nay đã là mặc định đang ship, và chỉ dựa trên phép quét**: một thời gian dài
  cái cổng vẫn đóng — `OvertakenLimit()` trả về mức giữ hai frame cũ trừ khi có caller nâng trần
  lên trên nó, nên phép quét chạy được phần suy ra trong khi production giữ nguyên hành vi đã đo.
  `ScreenViewer::Config::overtakenLimit` nay mặc định bằng `kDefaultOvertakenLimit` (8 frame,
  133 ms ở 60 fps), và thế là cổng mở. Số 8 không phải tinh chỉnh: phép quét thấy 8 và 30 không
  phân biệt được ở mọi điểm, vì dưới khoảng RTT 130 ms thì chính giá trị suy ra mới là cái chặn,
  còn trần chỉ cắt cái đuôi — không có trần thì một link 300 ms sẽ giữ 29 frame, gần nửa giây độ
  trễ, chỉ để tiết kiệm một keyframe. Phần lợi có điều kiện và phải nói đủ: từ 40 ms trở lên nó
  giảm số keyframe đi 2–4 lần, và ở loss cao nó còn **rút ngắn** cả cái stall dài nhất, vì không
  tiêu một IDR là tiết kiệm luôn 120 ms mà keyframe ấy phải trả. Ở RTT 20 ms với loss 1% nó thắng
  nhẹ; ở 20 ms với loss 5% nó **không mua được gì** mà cộng thêm khoảng 34 ms vào stall dài nhất.
  Nghĩa là viewer trên LAN trả một chút cho cái mà viewer qua WAN được hưởng. ⚠️ **Toàn bộ chuyện
  này chỉ dựa trên mô phỏng.** Mọi con số trên đến từ mô hình có seed trong `core/tests`, vốn có
  độ trễ một chiều cố định, **không jitter, không reorder, không tắc nghẽn**, và chở byte ngẫu
  nhiên chứ không phải video. Nó chưa từng gặp một NIC nào. Nửa `netem` của phần xác nhận Phase 3
  mới là thứ khẳng định hay bác bỏ được điều này, và nó **chưa được chạy**.
- **Reed-Solomon cần nhiều hơn một gói parity mỗi group, và đó là đổi wire chứ không phải chi
  tiết cài đặt**: `FecHeader` đúng 16 byte và dùng hết (frameId 4, timestampUs 8, pktCount 2,
  groupIndex 1, groups 1), `Packetizer` phát đúng một gói FEC mỗi group, còn bên nhận lưu đúng
  một payload parity mỗi group. RS(k,n) với một hàng parity thì chỉ là XOR đắt tiền hơn, nên
  không tầng nào trong ba tầng đó chở nổi nó. Chỉ số parity nay đi trong flags byte của common
  header, nơi mới dùng bit 0 (`kVideoFlagIdr`) và bit 1 (`kVideoFlagFrameEnd`): sáu bit còn lại
  cho 64 gói parity mỗi group mà không phải tăng kích thước header hay `kProtocolVersion`. Một
  receiver không đọc các bit đó sẽ giữ gói parity đầu tiên của mỗi group và bỏ phần còn lại,
  tức suy về XOR chứ không làm hỏng dữ liệu. Parity phía nhận được khoá bằng group và index gộp
  vào một `uint16_t` thay vì vector lồng nhau — dạng lồng tốn thêm một cấp phát mỗi group và lộ
  ra ngay: `video/reassemble-fec-recovery` nhảy từ 1.30 lên 1.43 cấp phát mỗi gói. Chi phí đo
  được của chính sơ đồ, ở hai gói parity mỗi group: encode 143 µs mỗi frame so với 22,7 µs của
  XOR, tức 6,3 lần, còn phục hồi chỉ đắt hơn 1,4 lần vì nó chạy trên một group chứ không phải
  cả frame. Đó là cái giá của việc mua khả năng cứu hai gói mất mỗi group.
- **Bốn bộ điều khiển tắc nghẽn sau một interface, và mỗi cái thật sự nhìn thấy gì**: các
  phương án của A2 nay là `aimd` (bản AIMD đang chạy, không đổi), `delay-trend`, `scream` và
  `hybrid`, dựng bằng `MakeCongestionControl` và được `SourcePipelineState` giữ bằng con trỏ
  thay vì giá trị. Điều cần biết khi đọc chúng: `Feedback` của giao thức mỗi giây chỉ mang tỉ
  lệ mất gói, RTT và tốc độ nhận — không có dấu thời gian tới của từng gói, nên **không thể**
  dựng một bộ lọc delay-gradient đúng kiểu WebRTC trên biến thiên độ trễ giữa các nhóm ở đây.
  Vì vậy `delay-trend` làm việc trên độ trễ hàng đợi suy ra từ phần RTT vượt trên mức tối
  thiểu quan sát được, còn `scream` bám theo tốc độ nhận được báo về, bị chặn bởi chính độ trễ
  hàng đợi đó. Chúng là bản thích nghi với những tín hiệu mà dây này chở, không phải bản cài
  lại của các bài báo, nên đem so với số liệu GCC hay RFC 8298 đã công bố là so hai thuật toán
  khác nhau. `hybrid` lấy tốc độ thấp hơn trong hai và hợp của hai quyết định FEC. Cả bốn dùng
  chung một luật bật FEC, nên đổi bộ điều khiển không âm thầm đổi thời điểm parity được phát.
- **Backoff kết nối lại không có jitter đưa mọi viewer quay lại cùng một khoảnh khắc**:
  `ReconnectDelayUs` nhân đôi từ 500 ms tới trần 5 s như một hàm thuần theo số lần thử, nên
  các viewer mất cùng một host tại cùng một thời điểm sẽ thử lại đồng pha và đập vào nó cùng
  lúc ở mọi vòng. Nay nó nhận thêm một từ `jitter` do caller cấp và trả về độ trễ rút từ nửa
  trên của khoảng backoff danh nghĩa, vừa giữ tần suất thử lại có chặn vừa rải thời điểm quay
  lại. `core/` không được với tới `deskhubp/system/Random.h`, nên tính ngẫu nhiên buộc phải đi
  vào qua tham số — việc phải đổi chữ ký ở mọi caller chính là điểm mấu chốt, không phải phiền
  toái.
- **Reed-Solomon với một hàng parity chính là XOR, đo ra tới từng chữ số**: phép quét Phase 3
  chạy 180 điểm trên sơ đồ × số hàng parity × độ sâu interleave × tỉ lệ mất × độ dài burst ×
  round trip, với FEC ép bật để nó đo sơ đồ chứ không đo chính sách bật. Ở 5% loss với burst 4,
  `rs` một hàng parity và `xor` cho số liệu giống hệt nhau ở mọi mức depth — 33,9% frame hỏng
  được cứu và 171 lần xin keyframe mỗi phút ở depth dẫn xuất, 78,7% và 81 ở depth 8 — đúng như
  đại số bắt buộc phải vậy, và là một phép kiểm tra hữu ích rằng bản cài đặt đúng. Nó cũng có
  nghĩa RS không mang lại gì khi dưới hai hàng parity trong khi tốn 6,3× CPU encode, nên một
  hàng không bao giờ là cấu hình để đưa nó ra chạy thật. **Khi đã tính overhead thì không điểm nào trong lưới thắng được mặc định đang chạy, và đọc
  sweep mà bỏ qua cột đó chính là cách người ta kết luận ngược lại.** Tách độ sâu interleave
  không thêm CPU, và rất dễ gọi đó là miễn phí: nó đưa tỉ lệ cứu từ 33,9% lên 78,7% và số lần
  xin keyframe từ 171 xuống 81 mỗi phút. Nó cũng đưa overhead parity từ 15% lên **100%** — ở
  một group mỗi gói thì mỗi gói tự mang parity của chính mình, đó là nhân đôi chứ không phải mã
  hoá. Trên ngân sách 20 Mbps, việc đó tiêu khoảng một nửa bức ảnh để giảm một nửa số lần xin
  keyframe. `rs` ba hàng ở depth 4 đạt 36 lần mỗi phút với overhead **150%**. Vậy nên phép quét
  **không sinh ra bản thắng nào để đưa lên**; kết quả của nó là các mặc định đứng nguyên còn
  các bản dự thi ở lại làm tham chiếu sau `--fec`. `overhead_pct` nay là một cột riêng trong
  CSV, và có test khẳng định depth tốn hơn ba lần parity — bởi vì bản viết đầu tiên về đúng
  phép quét này đã gọi depth là đòn bẩy rẻ nhất trong lưới, và đã sai.
- **Một bản giữ lại xứng đáng có chỗ nhờ việc còn với tới được và còn được test, không phải nhờ
  được quét dưới mọi sanitizer**: phép quét Phase 3 không đưa ai lên, nên `rs` ở lại trong cây
  nguồn mà production không có đường nào chọn nó — `Packetizer` và `Reassembler` đều khởi động
  bằng `kDefaultFecScheme`, `ShareOptions` và `ScreenClientConfig` không gọi tên sơ đồ nào cho
  tới khi có thứ hỏi, và chỉ `deskhub-cli --fec=NAME` mới hỏi. Chính khả năng với tới đó là toàn
  bộ lý do một bản thua đáng được giữ, nên nay đã có test khẳng định nó thay vì phó mặc cho thói
  quen. Việc giữ cũng không miễn phí: `rs` mã hoá một frame mất 143,9 µs so với 22,8 µs của XOR,
  và phép quét chạy cả hai chiếm phần lớn thời gian của `core_tests` — 8,6 s cho ma trận đầy đủ
  so với 2,9 s khi chỉ có sơ đồ đang ship, trước khi ASan hay TSan nhân nó lên.
  `DESKHUB_FEC_MATRIX=shipping` chọn ma trận nhỏ hơn, và các job sanitizer đặt biến đó trừ khi
  diff có đụng vào `FecScheme` hoặc test của nó — nên một bản tham chiếu vẫn được quét dưới
  sanitizer đúng vào những thay đổi có thể làm nó hỏng, còn tám job unit thông thường vẫn phủ nó
  ở mọi commit. Một giá trị không nhận ra thì làm hỏng cả lần chạy chứ không lặng lẽ chọn giùm,
  bởi "lần chạy này thực sự đã phủ những bản nào" không phải câu hỏi mà một lỗi gõ được quyền
  trả lời.
- **Một núm vặn không có caller nào ngoài chính test của nó thì không phải núm vặn**: phép quét
  Phase 3 xoay ba trục — sơ đồ, số hàng parity và độ sâu interleave — mà từ một binary đã build
  chỉ chạm được đúng trục đầu. `--fec` chọn sơ đồ, và chính phép quét cho thấy đó là trục ít
  quan trọng nhất: `rs` ở một hàng parity tái lập `xor` tới từng chữ số. Hai trục thật sự làm
  con số dịch chuyển thì không với tới được. `Packetizer::SetFecGroups` không có caller nào
  ngoài `core/tests` — đúng hình dạng lỗi mà Phase 1 đã vấp với `SetVideoPath`. Tệ hơn, tỉ lệ
  parity thì với tới được nhưng **giữ** không được: `ViewerBroadcast` gọi `SetFecParityPerGroup`
  ở mỗi lần broadcast, lấy từ `wantFecParity`, mà `ApplyFeedback` lại ghi đè mỗi giây bằng
  `FecParityRowsFor(lossPct)` — 1 hàng khi dưới 3% loss, 2 khi dưới 6%, 3 khi trên. Nên trên
  link tốt chính sách trả về 1, và `--fec=rs` ở đó **chính là** `xor` cộng thêm 6,3× CPU encode.
  Ai đo trên WiFi nhà sẽ kết luận Reed-Solomon chẳng thay đổi gì, trong khi chưa một lần chạy nó
  ở cấu hình mà nó khác. Nay `--fec-parity` ghim tỉ lệ để chính sách không đè, `--fec-depth`
  chạm tới `SetFecGroups`, còn `--fec-arm always` giữ parity trên dây để cái được đo là sơ đồ
  chứ không phải chính sách bật — đúng như sim vẫn làm, và ý nghĩa của mấy cờ này là giờ có thể
  hỏi một link thật cùng câu hỏi đó. Trước khi tin một cờ tái lập được phép đo, hãy tìm dòng
  code đọc nó trong production; một hàm setter cộng một cái test không phải dòng đó. Mỗi source
  còn log lại chính cấu hình nó rơi vào, đọc ngược từ packetizer chứ không từ tuỳ chọn —
  `FEC measurement: scheme=xor parity=1 depth=4 arm=policy` sau khi bị xin ba hàng parity mới là
  câu trả lời trung thực, vì `xor` chỉ chở được một; và một dòng trong bảng quét dán nhãn theo
  cái đã xin thay vì cái đã chạy thì còn tệ hơn là không có dòng nào.
- **Đúng cái caller thiếu đó còn ở bốn chỗ nữa, và một chỗ tới giờ vẫn thiếu**: chạy lại bài
  kiểm tra ấy lên phần còn lại của Tier A thì thấy `SetCongestionControl`, `SetAdaptiveTarget`,
  `SetAdaptiveLead`, `SetDisplayIntervalUs` và `MakeClockOffsetEstimator` đều có **0** caller
  ngoài chính test của chúng. Ba trong bốn bộ điều khiển tắc nghẽn không chọn được, target âm
  thanh thích ứng không bật được, và cả ba bản ước lượng đồng hồ chỉ tồn tại trong một phép
  quét. Nay `--cc` chạm tới vòng điều khiển của host, còn `--audio-delay` / `--audio-adaptive`
  chạm tới jitter buffer của viewer, cả hai đều được log lại từ chính đối tượng đang chạy. Hai
  cái còn lại **cố ý để yên**: `VideoPacer` là nơi duy nhất dùng `ClockOffset`, mà `VtDecoder`
  lại là nơi duy nhất dùng `VideoPacer` — nên trên Windows và Linux không có pacer nào để cấu
  hình, và thêm `--clock` hay `--vsync` ở đó chính là chế tạo ra đúng cái núm vặn giả mà mục này
  đang nói. `RollingMinEstimator` đã xác nhận là **wrapper thuần** quanh `ClockOffset`, nên
  chuyển `VideoPacer` sang contract là thay đổi không đổi hành vi — làm khi nào có một viewer
  không-Apple bắt đầu dùng pacer, chứ không phải trước đó. Một contract không có caller
  production chỉ là một đồ gá cho phép quét đang khoác áo interface, và viết thêm impl phía sau
  nó không đổi được điều đó.
- **Pacer chưa bao giờ gọi tới đúng cái method mà ba bản ước lượng đồng hồ khác nhau**: sau khi
  chuyển `VideoPacer` sang contract `ClockOffsetEstimator` — không đổi hành vi, vì `rolling-min`
  là wrapper thuần quanh chính `ClockOffset` mà nó vốn nhúng — một phép quét 18 điểm chạy cả ba
  bản dưới wobble 0/5/20 ms, có và không có bước nhảy transit 30 ms. Chúng **không phân biệt
  được**: phase spread nằm trong 6898–6937 µs với mọi bản ở mọi điểm, và `kalman` tái lập
  `rolling-min` tới từng micro giây. Lý do nằm ở interface chứ không ở thuật toán. Pacer gọi
  `AddSample`, `ready`, `Reset` và `floorUs` — **không bao giờ gọi `LatencyUs`**, mà `LatencyUs`
  mới là method duy nhất ba bản cài khác nhau: `KalmanEstimator::floorUs()` trả về `lowest_`,
  tức **đúng là** rolling minimum. Nên A5 không chấm điểm được bằng judder; trục nó làm dịch
  chuyển là `e2e_abs_ms` công bố ra, và test riêng của nó vốn đã đo ở đó. Trước khi nối một
  contract vào một nơi tiêu thụ để chạy bake-off, hãy kiểm tra nơi đó có gọi cái method mà các
  bản dự thi khác nhau hay không — nếu không, phép quét chỉ đẻ ra một bảng gọn gàng của cùng
  một con số.
- **Xin gì ở encoder sau khi mất một reference là một chính sách, mà nó đang là hằng số**: gói
  `InvalidateRef` của viewer vốn đã đi hết chiều dài dây, và host trả lời bằng
  `forceIdr.store(true)` — nên "reference invalidation" trong mọi trường hợp chính là một IDR
  nguyên khung. `media::RecoveryPolicy` nay chọn giữa ba câu trả lời dựa trên thứ backend khai
  là mình làm được: tụt về long-term reference mới nhất còn cũ hơn frame đã mất, bắt đầu một
  lượt intra refresh cuộn, hoặc gửi keyframe. Hai luật đáng nhớ. Một reference **mới hơn** frame
  đã mất thì không bao giờ dùng được, vì viewer có thể chưa từng giải mã nó — chỉ cái cũ hơn mới
  an toàn. Và một báo cáo mất lần thứ hai tới trước khi kịp mã hoá frame nào nghĩa là câu trả
  lời rẻ đã không ăn thua, nên nó leo thang lên keyframe thay vì lặp mãi.
  `ReferenceInvalidatingEncoder` và `IntraRefreshEncoder` gia nhập nhóm concept tuỳ chọn trong
  `VideoContract.h`. Hai backend Windows nay thi hành chúng; bốn backend còn lại chưa khai gì, nên
  tập khả năng của chúng vẫn rỗng và hành vi không đổi — một chính sách chỉ tốt đúng bằng cái
  encoder thi hành được nó.
- **Thương lượng codec vốn chỉ là một phép thử bit, trong khi mask đã rộng 16 bit sẵn**: `Hello`
  mang `codecMask` và `HelloAck` mang `Codec`, nhưng host chỉ từng kiểm
  `codecMask & kCodecMaskH264` rồi trả về `Codec::H264`. Mask nay gọi tên H264 4:2:0, H264
  4:4:4, HEVC và AV1, còn `NegotiateCodec(hostMask, clientMask)` chọn mục đầu tiên trong một
  bảng ưu tiên tường minh mà cả hai đầu cùng có, tụt về baseline 4:2:0 nằm cuối bảng — nằm cuối
  đúng để nó là sàn chứ không bao giờ là lựa chọn đầu. Peer cũ chỉ báo mỗi bit 0 nên vẫn thoả
  thuận ra H264 và nhận về giá trị 0, không có gì trên dây phải dịch chuyển. Không encoder nào
  trong cây tạo ra thứ gì ngoài H264 4:2:0, nên cơ chế hôm nay là trơ — mà đó đúng là điều C3
  yêu cầu: một bảng khả năng và một cơ chế thương lượng, không phải một cuộc đua. Bản thân thứ
  tự ưu tiên là **tạm**: 4:4:4 cho chữ sắc nét có nên xếp trên AV1 cho ít bit hay không chính là
  câu hỏi C3 tồn tại để giải, và nó cần phép đo mà thứ tự này chưa có.
- **Một luồng một chiều không bao giờ cho ra được latency tuyệt đối, bộ ước lượng có tốt tới
  đâu cũng vậy**: cả ba bản ước lượng offset — rolling minimum, trendline, Kalman — đều nhận
  cùng một đầu vào là hiệu giữa dấu thời gian host của một frame và thời điểm nó tới máy mình,
  mà hiệu đó là offset đồng hồ **cộng** độ trễ một chiều hàn dính vào nhau. Cả ba đều trừ đi một
  mốc sàn rồi báo phần vượt, nên `e2e_ms` là con số về sự ứ đọng chứ không phải về độ trễ, và
  đặt nó cạnh số của một tool khác là vô nghĩa. Cách sửa là **thêm một dấu thời gian thứ hai**,
  không phải một bộ lọc tốt hơn: `PingPong` nay chở `hostTimeUs` cạnh `sendTimeUs` của client,
  còn `ClockSync` giữ lần trao đổi có round trip nhỏ nhất trong cửa sổ mười giây — lần ít ứ đọng
  nhất — để ước lượng offset đồng hồ. Latency đầu-cuối tuyệt đối khi đó là
  `(thời điểm tới - offset) - hostPts`, báo ra dưới tên `e2e_abs_ms` cạnh `e2e_ms` tương đối.
  Payload tăng từ 12 lên 20 byte, và điều đó an toàn vì common header không chở độ dài payload
  còn `ParsePingPong` xưa nay chỉ cần mười hai byte đầu: peer cũ đọc đủ ba trường nguyên bản rồi
  bỏ qua phần còn lại, peer mới đọc gói 12 byte cũ thì thấy `hostTimeUs == 0` và tụt về số tương
  đối. Tất cả dựa trên giả định hai chiều đối xứng — đó là giả định của NTP và là giới hạn của
  phương pháp, không phải của bản cài: một đường đi bất đối xứng sẽ đẩy thẳng một nửa độ bất đối
  xứng đó vào offset.
- **Một bộ lọc mũ dùng số không dấu đi ngược hướng ngay lần đầu đầu vào giảm xuống**: cả ước
  lượng jitter của audio lẫn của pacer đều viết là
  `jitterUs_ += (spread - jitterUs_) >> shift` với `spread` và `jitterUs_` đều là `uint64_t`.
  Với bất kỳ mẫu nào yên hơn trung bình đang chạy, phép trừ tràn về gần 2^64, phép dịch giữ lại
  gần như toàn bộ, và ước lượng **phồng lên** thay vì suy giảm. Nó vô hình cho tới khi đường
  cong độ trễ/gián đoạn của audio thực sự được vẽ ra và target thích ứng ngồi lì ở trần 500 ms
  trên một link chỉ wobble 15 ms. Cả hai nay tính bằng `int64_t`. Đường cong tìm ra nó; các
  unit test quanh đó lẫn hình dạng của code đều không.
- **Đường cong độ trễ ↔ gián đoạn nói target audio cố định vẫn là mặc định đúng**: sau khi sửa
  bộ lọc và cho playout chạy theo đồng hồ thay vì theo lúc gói tới, phép quét chạy sáu mức
  target trên ba mức jitter. Target thích ứng thắng dứt khoát trên link phẳng — giữ 20 ms thay
  vì 60 ms cho cùng đúng một gap khởi động — và **thua** khi có jitter: ở wobble 40 ms nó chọn
  đúng 60 ms như bản cố định nhưng trả bằng bốn lần che thay vì một. Chi phí nằm ở chính việc
  thích ứng: nâng target giữa chừng nghĩa là phải chờ nạp lại tới mức mới, và cái chờ đó là một
  underrun; chỉ nâng vào lúc hàng đợi đã đủ thì bớt được phần lớn nhưng lại khiến target thiếu
  hụt. Bật cái này mặc định là đổi một mức tăng gián đoạn đo được lấy một cái lợi về độ trễ mà
  phép quét chỉ xác nhận trên đúng những link vốn chẳng phải vấn đề.
- **Khớp vsync không phải một cuộc đánh đổi độ trễ, và đó là điều phép quét chứng minh**: A6
  đóng khung judder như thứ phải vẽ theo trục độ trễ cộng thêm, nên phép quét đổi lead của pacer
  từ 8 ms tới 66 ms, có và không khớp vsync. Không khớp thì pha mà một frame rơi vào bên trong
  chu kỳ quét 6944 µs trải rộng khoảng 6000 µs ở **mọi** mức lead — gấp tám lần độ trễ không thu
  hẹp được gì, vì độ trải đó đến từ việc nội dung 60 fps gặp panel 144 Hz, không phải từ wobble
  lúc tới. Khớp vsync đưa nó về đúng **0** và không tốn một chút độ trễ nào. Ở đây không có
  đường cong nào để đánh đổi dọc theo; chỉ có một khiếm khuyết và một cách sửa.
- **HRESULT không phải bool, và `ICodecAPI::IsSupported` trả `S_OK` nghĩa là có**: mọi thuộc tính
  điều khiển tốc độ mà encoder Media Foundation đặt đều đi qua
  `if (!codecApi->IsSupported(&api)) { report("NOT SUPPORTED"); return; }`. `S_OK` bằng 0, nên
  nhánh đó chạy đúng vào những thuộc tính MFT **có** hỗ trợ, còn `SetValue` chỉ được thử trên
  những thuộc tính nó không hỗ trợ. Đo trên máy này sau khi in thêm HRESULT thô: `MeanBitRate`,
  `RateControlMode=CBR`, `GOPSize` và `BufferSize(VBV)` đều trả `hr=0x00000000` và đều đã bị bỏ
  qua — nghĩa là backend MF suốt đời nó vẫn mã hoá bằng tham số mặc định của MFT: không CBR, không
  bitrate đích, không GOP vô hạn, không VBV — trong khi NVENC nhận trọn rate plan. Mọi cuộc
  bake-off giữa hai bên trước bản sửa này là đem một encoder đã cấu hình so với một encoder chưa
  cấu hình. Nó cũng lật ngược bằng chứng của mục đầu tiên trong phần này: dòng
  `MeanBitRate: NOT SUPPORTED` trích ở đó thật ra có nghĩa là MFT của Intel **có** thuộc tính này.
  Kết luận "fallback phải là bắt buộc" vẫn đúng; chỉ có lý do đưa ra cho nó là một dòng log đọc
  ngược. `SetBitrate` và `RequestKeyFrame` trong cùng file dùng cực ngược lại — đó là cái giá của
  việc đọc một giá trị trả về ba trạng thái như một bool: hai chỗ gọi không thể cùng đúng, và
  không có test nào phân biệt nổi.
- **Núm vặn của C1 thiếu đúng cái caller mà A1 từng thiếu**: `CreateEncoder` thử NVENC, rồi Media
  Foundation, và giữ cái nào khởi động được trước — nên trên mọi máy có driver NVIDIA thì đường MF
  không đo được chút nào. Nay `--encoder auto|nvenc|mf|vaapi|videotoolbox` gọi tên backend, và gọi
  tên một backend không khởi động được thì source **dừng** chứ không lặng lẽ đo cái còn lại — chính
  cái thất bại mà fallback sẽ giấu đi mới là phép đo. Chấm điểm nó cũng cần một bộ đếm mới:
  `enc_ms_avg`/`enc_ms_max` không thấy được cái đuôi, nên `evt=sum` nay chở `enc_us_p50` và
  `enc_us_p99` từ một histogram bước 512 µs. Mỗi backend cũng khai khả năng phục hồi **đọc từ
  driver** thay vì suy đoán: NVENC qua `nvEncGetEncodeCaps` (`max_ltr_frames=8`,
  `ref_pic_invalidation=1`, `intra_refresh=1` trên RTX 5070 Ti), Media Foundation qua
  `IsSupported` trên ba thuộc tính LTR và `GradualIntraRefresh`, tất cả đều có. Đó là câu trả lời
  A4 đang chờ — cả hai backend Windows đều giữ được long-term reference — nhưng caps khi đó chỉ được
  **ghi log** chứ chưa đưa vào `RecoveryPolicy`, cho tới khi có encoder thi hành được: khai khả
  năng trong lúc chưa ai tiêu thụ `invalidateBeforeFrame` hay `wantIntraRefresh` sẽ biến phục hồi
  mất gói thành vô tác dụng. Số đo đầu tiên, trên desktop rảnh chứ chưa phải một clip cố định: NVENC mã hoá ở
  p50 2,5-5,6 ms và p99 2,7-5,7 ms, Media Foundation ở p50 0,5-13,8 ms và p99 12,3-17,6 ms. Ở đây
  cả hai cùng chạm một phần cứng — trên máy này `mf` rơi vào "NVIDIA H.264 Encoder MFT" — nên đây
  chưa phải câu hỏi Intel-so-với-NVIDIA mà C1 đặt ra; đây là cái giá của việc đi vòng qua Media
  Foundation để tới cùng phần cứng đó. Một máy Windows thứ hai tách được hai vế ấy ra: một Intel
  UHD 750 hoàn toàn không có driver NVIDIA, nơi `mf` rơi vào "Intel Quick Sync Video H.264 Encoder
  MFT", báo đủ ba thuộc tính LTR cùng `GradualIntraRefresh` là có hỗ trợ, và mã hoá 1920 × 802 ở
  p50 1,5-2,6 ms và p99 2,5-8,4 ms, thỉnh thoảng có cửa sổ chạm 27 ms. Đó là cột Intel mà C1 đòi,
  và nó nói rằng Quick Sync cũng giữ được long-term reference — nhưng đây **chưa** phải một cuộc
  đua công bằng với dòng NVENC ở trên: khác máy, khác cỡ capture, khác nội dung màn hình, và không
  bên nào chạy trên một clip cố định. Cũng chính lần chạy đó xác nhận luật gọi tên từ đầu đến cuối:
  `--encoder nvenc` ở đấy in "Failed to load nvEncodeAPI64.dll" rồi dừng nguồn, thay vì lặng lẽ mã
  hoá bằng Quick Sync dưới tên NVENC, còn `--encoder auto` thì rơi tiếp xuống Media Foundation và
  nói rõ vì sao nó đi tiếp.
- **Mỗi lần `QuicEndpoint::Poll` đều kết thúc bằng một giấc ngủ, và chính việc gộp lô mới làm nó
  lộ ra**: vòng đọc gọi `RecvFrom` cho tới khi một lần trả về không có gì, mà "không có gì" chỉ
  quay lại sau khi `SO_RCVTIMEO` hết hạn — nên một lần poll đã vét sạch socket vẫn phải trả trọn
  cái sàn timeout đó, thường là 1 ms, ở mọi lần gọi. `SessionTransport::RecvFrom` vốn đã chặn
  chính `Poll` ấy sau `WaitReadable(10 ms)` của nó, nên giấc ngủ kia là phần cộng thêm thuần tuý
  trên mọi lần nhận. Đọc theo lô bằng `recvmmsg` bỏ hẳn lần đọc thăm dò: một lô trả về ngắn nghĩa
  là socket đã rỗng, nên vòng lặp dừng mà không hỏi lại. Còn lại `SetRecvTimeout(0)`, thứ mà POSIX
  hiểu là "chờ mãi mãi" và vì thế code cũ ép thành 1 ms; nay nó nghĩa là "đừng chờ" (`O_NONBLOCK`
  trên POSIX, `FIONBIO` trên Windows), đúng điều `Poll(now, 0)` vẫn luôn tự nhận. An toàn vì mọi
  caller truyền 0 đều đã có cái chờ của riêng mình bao quanh — `WaitEstablished` poll rồi chờ,
  `RecvFrom` chờ rồi poll — nên không chỗ nào biến thành vòng quay. Đo bằng `platform_perf` trên
  loopback: một poll rảnh 1 978 692 ns → 732 ns, một record terminal 512 byte 4 244 632 ns →
  9 201 ns, 64 KB stream 16,7 MB/s → 500,7 MB/s, một datagram QUIC 248 515 ns → 3 986 ns, và phép
  vét 64 KB→256 KB vẫn tuyến tính (3,78x → 3,84x cho 4x công việc). Loopback không nói gì về thông
  lượng của một đường thật, nhưng cái sàn 1 ms nó gỡ đi là thời gian đồng hồ trên mọi đường.
- **Việc gộp lô mua được ít hơn giấc ngủ mà nó phơi ra, và phía gửi mua được nhiều hơn phía nhận**:
  `sendmmsg` vốn đã gom cả burst vào một syscall, nên `UDP_SEGMENT` (GSO) mua phần việc trong
  kernel chứ không mua số syscall — một lượt đi qua ngăn xếp UDP/IP thay vì mười sáu. Trên loopback
  với 16 × 1200 byte: một `sendto` cộng một `recvfrom` cho mỗi datagram tốn 2036 ns/datagram, chỉ
  gộp phía gửi tốn 636 ns, gộp cả hai tốn 623 ns. Nghĩa là GSO đáng 3,2x ở đây, còn bước cuối nằm
  trong sai số giữa các lần chạy — hai dòng gộp lô đổi chỗ cho nhau giữa hai lần — vì trên loopback
  kernel đã giữ sẵn mọi gói và một lần nhận gần như miễn phí. `recvmmsg` vẫn xứng chỗ của nó vì nó
  gỡ đi lý do khiến vòng lặp phải thăm dò: đo trên một lần chạy `platform_tests` dưới `strace`,
  17 317 datagram về trong 1631 lần gọi có kết quả, 10,6 gói mỗi syscall. GSO chỉ áp dụng cho một dãy datagram cùng cỡ với gói cuối được phép ngắn hơn, đúng
  hình hài `quiche_conn_send` sinh ra, và kernel nào từ chối (`EIO`, `EINVAL`, `ENOPROTOOPT`,
  `EOPNOTSUPP`, `EMSGSIZE`) thì socket quay về `sendmmsg` vĩnh viễn chứ không thử lại từng burst.
- **Cấp phát của một vòng lặp nóng có thể núp sau chính giấc ngủ của nó**: `Service()` dựng một
  `std::vector` id kết nối ở mọi lần gọi, `DrainStreams` một buffer 16 KB mới, `DrainDatagrams`
  một buffer 1350 byte — khoảng 1,5 lần cấp phát mỗi `Poll`, trên một đường chạy theo từng gói.
  Không cái nào lộ ra khi mỗi poll còn ngủ 1 ms; vừa bỏ giấc ngủ thì `quic/terminal-record-delivery`
  nhảy từ 9 lên 27 lần cấp phát mỗi record, vì cùng một record nay tốn gấp ba số vòng poll và mỗi
  vòng đều cấp phát. Bản sửa là mảng trên stack chặn bởi `kMaxConnections` cho hai danh sách id và
  buffer do chính endpoint sở hữu cho hai chỗ vét, cộng với việc `DrainStreams` trả về trước khi
  chạm buffer nếu không stream nào đọc được. `quic/poll-idle` đi từ 3,00 lần cấp phát mỗi poll về
  0,00. Hình hài chung đáng giữ: một đường chạy theo từng gói mà có ngủ thì nó giấu chính chi phí
  của mình, và cái ngân sách cấp phát đạt chuẩn bao năm nay thật ra đang đo giấc ngủ.
- **Bộ đếm loss đang đo phần sống sót sau khi vá, không đo cái mà đường truyền thật sự làm**:
  `packetsLost` và histogram `lossRuns` đều được cộng bên trong `Drop()`, nên chúng chỉ từng đếm
  những gói vắng mặt trên các frame **đã bị vứt đi**. Một gói được FEC dựng lại, hay một gói NACK
  lấy về, không để lại dấu vết ở đâu cả. Một phiên năm phút có tải qua Tailscale, trung vị 5,7 Mbps,
  đo thẳng được vùng mù đó: **48 gói từng vắng mặt**, trong đó 41 gói không bao giờ tới và **7 gói
  được NACK lấy về kịp** cho frame hoàn thành. Đúng 7 gói đó — cộng một run dài 2 gói không bao giờ
  vào histogram — là thứ bộ đếm cũ không thể thấy, vì nó chỉ nhìn vào những frame đã bị vứt đi. Mọi
  tham số Gilbert-Elliott rút từ nó vì thế là tham số của loss **không cứu được**, và càng lệch khi
  FEC với NACK càng chạy tốt. (Bộ đếm `latePackets` là chuyện khác và không phải bằng chứng ở đây:
  nó đếm những lần vá tới **sau** khi frame đã bị bỏ, mà phần mất đó đã được tính lúc drop.) Bản sửa không đi quét tìm gap;
  nó đánh dấu một chỗ trống ở đúng ba thời điểm chỗ trống thật sự lộ ra — một gói lấp vào chỉ số
  thấp hơn chỉ số cao nhất đã thấy, `TryRecover` dựng lại một mảnh, và một frame rời hàng đợi mà
  mảnh vẫn trống (đây là lối duy nhất để nhận ra một cái đuôi bị mất, vì không có chỉ số nào cao
  hơn tới để phơi nó ra). Mỗi gói từng vắng mặt sau đó rơi vào đúng một trong bốn thùng: không bao
  giờ tới, FEC vá, vá sau khi hỏi bằng NACK, hoặc chỉ đơn thuần bị đảo thứ tự. Tách cái thùng cuối
  ra là việc bắt buộc chứ không phải trang trí — đảo thứ tự không phải mất gói, gộp vào sẽ thổi
  phồng mô hình burst — nên `wire_loss%` bằng `(everAbsent − reordered) / (received + neverArrived)`.
  Thiên lệch đã biết: `nacked[i]` được đặt ngay khi `PlanNack` chọn chỉ số đó, nên một gói vừa bị
  hỏi vừa chỉ tới muộn sẽ bị xếp là vá bằng NACK — tức nghiêng về phía coi là loss. `evt=sum` nay
  chở `wire_loss` / `absent` / `gone` / `nack_fix` / `reorder`, và một histogram `wire runs` đứng
  cạnh dòng `loss runs` cũ. Không dòng nào thay dòng nào: dòng cũ là thứ người xem phải chịu, dòng
  mới là thứ đường truyền đã làm, và một cuộc bake-off cần dòng thứ hai trong khi một báo cáo lỗi
  của người dùng cần dòng thứ nhất.
- **Cho encoder quyền thi hành trước, rồi mới để chính sách gọi tên hành động**: `RecoveryPolicy`
  đã hoàn chỉnh và nằm im từ A4, tập khả năng để rỗng một cách cố ý. Một host khai có long-term
  reference trong khi encoder chẳng giữ cái nào sẽ đáp lại một reference bị mất bằng cách đặt
  `invalidateBeforeFrame`, không ai đọc, và không còn xin cái IDR mà trước đây nó vẫn xin — phục
  hồi mất gói đi từ đắt sang không có. Nên phần thực thi vào trước, `SetCaps` vào sau cùng.
  `IVideoEncoder` có thêm `MarkLongTermReference`, `InvalidateReference` và `BeginIntraRefresh`,
  với `static_assert` buộc nó vào `ReferenceInvalidatingEncoder` và `IntraRefreshEncoder` — đúng
  công dụng mà hai concept tuỳ chọn ấy được viết ra. NVENC chạy LTR Per Picture (`enableLTR=1`,
  `ltrTrustMode=0`) trên một ring 4 slot với `maxNumRefFrames` nâng theo: mark bằng
  `ltrMarkFrame`/`ltrMarkFrameIdx`, vá bằng `ltrUseFrames`/`ltrUseFrameBitmap`, refresh bằng
  `forceIntraRefreshWithFrameCnt`. `nvEncInvalidateRefFrames` cố ý không dùng — bitmap là đường
  tất định, vì nó nói thẳng khung kế tiếp được phép tham chiếu cái gì, thay vì gọi tên cái đã hỏng
  rồi để phần còn lại cho suy đoán. Driver nào từ chối LTR ở `InitializeEncoder` thì được thử lại
  đúng một lần không có LTR, thay vì làm hỏng cả buổi share. Media Foundation đi qua
  `AVEncVideoLTRBufferControl` lúc init rồi `MarkLTRFrame`/`UseLTRFrame`/`GradualIntraRefresh`
  theo từng khung, và `RequestKeyFrame` nay quên ring, vì IDR xoá sạch DPB nên giữ lại cái record
  ấy là tự nói dối. `deskhubp/host/EncoderRecovery.h` là chỗ nối: `PrepareRecovery()` tiêu thụ
  `invalidateBeforeFrame` và `wantIntraRefresh`, rơi về IDR mỗi khi encoder không thi hành được,
  và mark đúng những khung mà chính sách sẽ gọi tên về sau — kể cả IDR, vì bỏ sót chỗ đó là để
  `core` tin vào một long-term reference mà encoder không giữ. Nó được bọc trong `if constexpr`
  theo hai concept, nên Linux, Apple và Android giữ nguyên hành vi hôm nay cho tới khi encoder của
  họ có ba hàm kia. Windows gọi `recovery.SetCaps(encoder->RecoveryCaps())` sau **mỗi** lần tạo
  encoder, vì `SetCaps` reset luôn chính sách — đúng thứ cần khi encoder vừa dựng lại và đã mất
  mọi reference nó từng giữ. Preamble nằm trong `EncodeTimed`, nên cả đường frame lẫn đường flush
  đều đi qua nó thay vì mỗi bên chép một bản.
- **Một lớp không ai gọi là một cuộc đua đang chờ caller đầu tiên**: `RecoveryPolicy` bị chạm từ
  hai luồng — `OnReferenceLost` trên net loop của host, `NoteEncoded` và `ShouldMarkLongTerm` trên
  luồng encode dưới `encMutex` — mà không có khoá nào, và điều đó không tốn gì suốt thời gian chưa
  backend nào thi hành được phục hồi và đường này chưa bao giờ chạy. Bật Windows lên là TSan báo
  ngay ở lần mất đầu tiên. Lớp này nay giữ mutex của riêng nó và không còn copy được nữa; không
  chỗ nào copy cả. Một thành phần đỗ sau một tập khả năng rỗng không được chứng minh là an toàn
  luồng bởi một lần chạy test xanh, nó chỉ chưa được động tới.
- **Một nửa của tối ưu đa nền tảng là cái nền tảng chưa bao giờ nhận được nó**: P2 nói đường gửi
  có gộp lô, và đúng là có — trên Linux. `UdpSocketWin::SendBatch` là một vòng `sendto`, mỗi
  datagram một syscall, nên host Windows trả trọn chi phí mỗi gói trong khi ghi chú ở trên mô tả
  một bên gửi đã gộp. Nay nó gọi `WSASendMsg` với control message `UDP_SEND_MSG_SIZE`, đối ứng
  Windows của `UDP_SEGMENT`, phân giải một lần qua `WSAID_WSASENDMSG` lúc `Open` và tắt vĩnh viễn
  — không phải theo từng burst — khi stack từ chối, rơi về đúng vòng một `sendto` mỗi datagram. Đó
  là nửa rẻ trong những gì P2 liệt kê, nên nó đi trước RIO. `LeadingRunOfEqualSegments` chuyển từ
  `UdpSocketPosix.cpp` lên `deskhubp/net/UdpSocket.h` để hai hệ điều hành dùng chung đúng một luật
  thay vì hai bản chép, và luật "gói ngắn chỉ được là segment cuối của một run" nay được giữ bằng
  một test đơn vị chứ không chỉ bằng một vòng loopback. Bảng loopback ở trên là số Linux:
  `platform_perf` chưa chạy trước/sau thay đổi này trên Windows, nên những con số đó không chuyển
  sang được.
- **`enc_lat_ms` chưa bao giờ là con số của capture, và cái đồng hồ capture thì không ai đọc**: C2
  hỏi Windows Graphics Capture trả bao nhiêu độ trễ so với DXGI Desktop Duplication, và phát hiện
  đầu tiên là con số đó **chưa tồn tại** chứ không phải chưa được in. `ScreenCapture.cpp` điền
  `fi.meta.timestampUs` từ `SystemRelativeTime` của WGC và không ai đọc nó: `SharingHost` chỉ lấy
  `width` với `height` từ khung rồi truyền cho `Encode` một `NowUs()` mới tinh, nên `enc_lat_ms` đo
  encoder và không nói gì về chặng capture→texture. Hai đồng hồ vốn đã khớp — `SystemRelativeTime`
  là QPC đơn vị 100 ns và `NowUs()` trên Windows cũng là QPC — nên hiệu hai số dùng được ngay,
  không cần quy đổi epoch, và đó là lý do cả câu hỏi chỉ tốn đúng một lời gọi.
  `SourceDiag::NoteCapture` nhận timestamp của khung cùng thời điểm nó tới host, cộng tuổi khung
  vào percentile `cap_us`, và đếm một khung được trao lại lần nữa thành `cap_repeat` thay vì đo
  lại nó — tuổi của một khung lặp tính từ lần capture gốc nên sẽ thổi phồng cái đuôi. Nó nằm trong
  `core/` để bốn backend capture còn lại nối vào bằng đúng một dòng mỗi cái, giấu sau
  `ShareDiagCaps::captureLatency` để tới lúc đó chúng không in một cột rỗng. Trả lời "sự tiện lợi
  của WGC tốn bao nhiêu" chỉ cần đúng cột ấy; chỉ khi con số xấu mới đáng viết backend Duplication,
  và nếu viết thì nó nhận cờ `--capture wgc|dxgi` và **dừng** khi backend được gọi tên không khởi
  động được — đúng luật `--encoder` đang theo, vì đúng lý do đó. Cột ấy nay đã đọc được, trên một
  Intel UHD 750 capture 3440 × 1440 rồi hạ xuống 1920 × 802: qua mười sáu cửa sổ một giây,
  `cap_us_p50` nằm ở 0,5-2 ms và `cap_us_p99` ở 2-20 ms, `cap_repeat=0` suốt — WGC trao một khung
  hết đúng khoảng thời gian encoder sau đó bỏ ra để nén nó (`enc_us_p50` 1,5-2,6 ms), nên sự tiện
  lợi ấy rẻ và **không đáng viết backend Duplication**. Ngoại lệ duy nhất là cửa sổ đầu tiên sau
  `Start`, nơi `cap_us_p99` đọc ra 242 ms: đó là tuổi của chính khung đầu tiên, không phải cái đuôi
  ở trạng thái ổn định.
- **Gộp gói phía nhận là tấm gương của phía gửi, và nó tốn đúng một trường trong giao kèo đọc**:
  nửa còn lại của P2 là GRO trên Linux và URO trên Windows, và cùng một thứ chặn cả hai — `RecvBatch`
  hứa mỗi slot một datagram. Với `UDP_GRO` (Linux) hay `UDP_RECV_MAX_COALESCED_SIZE` (Windows), một
  lần đọc trả về cả một dãy datagram cùng cỡ nằm trong một buffer, kèm cỡ segment trong control
  message; nên `InboundDatagram` có thêm trường `segment`, còn `DatagramsIn` / `DatagramAt` tách một
  slot trở lại thành đúng những datagram đã được gửi. `segment == 0` nghĩa là slot chứa đúng một
  datagram — tức mọi caller không xin gộp — nên giao kèo cũ là mặc định chứ không phải ngoại lệ.
  Gộp phải **xin mới có**, qua `EnableReceiveCoalescing()`, và không bao giờ tự bật, vì một slot quá
  nhỏ là mất dữ liệu: đo tại chỗ, một run GSO 16 × 1200 byte về thành một skb gộp 19 200 byte, đọc
  nó vào buffer 2048 byte thì trả về 2048 byte kèm cờ `MSG_TRUNC` và **vứt mất 17 152 byte còn
  lại** — lần đọc kế tiếp không thấy gì nữa. Kernel gộp được tới trọn payload IP 64 KB, nên chỉ
  caller nào có slot chứa nổi `kMaxCoalescedBytes` mới được bật gộp, và `RecvBatch` ghi ca
  `MSG_TRUNC` thành một lỗi nói thẳng đòi hỏi ấy thay vì để một burst biến mất lặng lẽ. Vì thế
  `QuicEndpoint` đổi số slot lấy cỡ slot — 16 × 1350 byte trên stack thành 4 × 65 535 byte do chính
  endpoint sở hữu — và hoá ra số slot không quan trọng: 4, 8 và 16 đều nằm trong nhiễu giữa các lần
  chạy của nhau, nên bản tốn ít bộ nhớ nhất thắng. Đo trên loopback, chạy xen kẽ hai binary để triệt
  cái trôi của máy, trung vị của bốn cặp: đọc một burst 16 datagram 573 → 203 ns/datagram (2,8x),
  `quic/stream-drain-scaling` 2095 → 1629 ns/KB, `quic/stream-throughput-64k` 2114 → 1785 ns/KB.
  Những dòng mà mã hai binary giống hệt nhau vẫn xê dịch 3,3-5,1% — đó là sàn nhiễu của máy này —
  nên `quic/handshake` +4,9% (13 → 15 lần cấp phát, một trong số đó là buffer đọc 256 KB) không tách
  khỏi nhiễu được, còn `quic/datagram-delivery` +0,6% nói rằng đọc control message không tốn gì cho
  đường không gộp. Nửa Windows viết theo đúng hình hài ấy bằng `WSARecvMsg` và `UDP_COALESCED_INFO`,
  và nay đã được biên dịch và chạy: stack nhận `UDP_RECV_MAX_COALESCED_SIZE`, nhưng trên loopback
  nó **không gộp gì cả** — một run USO 16 × 1200 byte về thành đúng mười sáu lần đọc riêng lẻ, mỗi
  lần mang `segment == 0`, nên một slot 2048 byte quá nhỏ cũng chẳng mất gì, vì không có run nào để
  mà cắt. Con số 2,8x ở trên vẫn là kết quả Linux. Trên Windows toàn bộ phần thắng đo được nằm ở
  phía gửi: USO kéo một burst 16 gói từ 7183 xuống 3942 ns/datagram (−45%, trung vị của mười một
  lần chạy), còn gộp phía nhận và URO đều rơi vào sàn nhiễu 45-60% — rộng gấp một bậc so với 5% của
  máy Linux, và rộng tới mức mọi thứ dưới khoảng 1,5x đều không đo nổi ở đó. URO có bao giờ nổ hay
  không thì phải có NIC thật mới biết; loopback không trả lời được.
- **Một cuộc bake-off cần một clip trước khi cần cột thứ hai**: C1 đã có ba cột encoder mà không so
  được cột nào, vì mỗi cột đo trên một máy khác, cỡ capture khác, nội dung desktop khác, và không
  cột nào chạy trên clip cố định. Tệ hơn: mọi con số đều là độ trễ, không có gì đo xem encoder đã
  đánh đổi cái gì để nhanh. `scripts/encoder-bake-off.sh` bịt cả hai lỗ hổng bằng một lệnh. Nó dựng
  clip (`testsrc2` xác định, hoặc `--clip FILE` cho clip thật), đưa **đúng cùng** một chuỗi frame
  thô, cùng cỡ, cùng fps, cùng bitrate cho mọi backend, rồi in VMAF, `enc_us_p50`, `enc_us_p99`,
  CPU% và GPU% trong một bảng, kèm SHA-256 của clip để hai máy chứng minh được là đã đo cùng một
  đống pixel. Encoder nhận `ID3D11Texture2D` chứ không nhận file, nên clip được nạp bởi
  `client/windows/cpp/bench/EncoderBench.cpp` — một binary bench upload frame BGRA qua vòng bốn
  texture và chỉ bấm giờ đúng lời gọi `Encode`. Đo `h264_qsv` và `h264_nvenc` của ffmpeg thay vào
  đó thì chỉ tốn một dòng script, nhưng trả lời một câu hỏi khác: đó không phải code Deskhub ship.
  Hai giới hạn được in ngay cạnh bảng chứ không giấu đi: `cpu_pct` và `gpu_pct` là số của cả tiến
  trình nên tính cả phần upload frame của chính harness, và `testsrc2` không phải nội dung desktop
  — clip tổng hợp làm các lần chạy so được với nhau, không làm chúng đại diện cho thực tế. Backend
  nào không khởi động được thì bị bỏ khỏi bảng chứ không bị đo dưới tên backend khác, đúng luật mà
  `--encoder` đã theo.
- **Mười giây im lặng trên loopback là một link đang chờ câu trả lời không ai đưa**: `platform_tests`
  hỏng đúng tám check, khoảng một trong năm lần chạy trên Windows, trải trên `HostLinkTests` và
  `FileTransferTests`, với log nói rằng một link QUIC đã im tiếng trên loopback hơn mười giây — mà
  tranh chấp CPU thì không giải thích nổi, nên deadline không phải thứ cần nới. Link không kẹt cũng
  không chậm: nó **bị đỗ lại**. `HostLink::SettleTrust` so khoá host đưa ra với `known_hosts`, và khi
  gặp `TrustVerdict::Changed` thì chuyển sang `Deciding` và chờ một con người chấp nhận hay từ chối.
  Link đang đỗ thì không gửi gì, nên cả hai đầu đều báo bên kia im lặng; test không cài
  `onTrustAsked` và không bao giờ chấp nhận, nên nó chờ hết deadline của chính mình. Mọi thứ còn lại
  trong sự cố suy ra từ đó: "a terminal record goes out" vẫn qua, vì kết nối QUIC vẫn sống; tiếng
  vọng quay về và bị vòng đọc của `Deciding` nuốt mất thay vì tới được channel, làm hỏng thêm hai
  check; và `FileTransferTests` hỏng thêm ba check trên cổng của nó vì đúng lý do ấy. Cắm một
  fingerprint hợp lệ nhưng khác cho `127.0.0.1:47845` và `127.0.0.1:47836` là dựng lại được cả tám
  lỗi đúng tên, đúng kiểu im lặng, gọi ra lúc nào cũng được.
  **Khoá khác nhau là vì trên Windows các test không có trạng thái riêng.**
  `KeepTestLogsOutOfTheDeveloperHome()` dời `HOME` sang chỗ khác trên POSIX và là một hàm rỗng trên
  Windows, nên cả hai binary test đọc ghi thẳng vào `%USERPROFILE%\.deskhub` — cùng một
  `host_cert.pem`, `known_hosts`, `paired_devices` mà app đã cài và mọi tiến trình Deskhub khác trên
  máy đang dùng. Thêm nữa, cái identity dùng chung ấy là giấy nháp của các bộ test: một lần chạy tạo
  ra khoảng năm mươi cái, mỗi cái chụp lại cặp trước rồi khôi phục sau, và ba trong bốn file làm việc
  này khôi phục ở cuối hàm với bốn tới sáu lệnh `return` sớm nằm giữa. Bất kỳ lối ra sớm nào, bất kỳ
  lần kill nào, hay bất kỳ lượt ghi nào từ một app đang chạy, đều để lại cho lần `HostLinkTests` kế
  tiếp một phép so khoá của lần này với bản ghi của lần trước. Bản sửa gồm bốn phần, không phần nào
  là nới deadline: cả hai test main nay trỏ `SetAppDataDir` vào một thư mục riêng trên Windows; guard
  RAII `SavedIdentity` mà `SessionTransportTests` đã có được dời vào `TestSupport.h` và thay cho mọi
  đoạn khôi phục thủ công; các test quay số tới một endpoint cố định nhận thêm guard `ForgottenHost`
  để phán quyết của chúng không phụ thuộc vào thứ lần chạy trước để lại; và `SettleTrust` nay ghi log
  đúng lúc nó đỗ lại, nêu rõ khoá nó thấy, để lần im lặng sau tự giải thích trong log thay vì trông
  như một cái bắt tay đã chết.
- **Một dòng log ghép từ ba lệnh `printf` không phải là một dòng**: chính bản log đó cho thấy
  `[Deskhub] [Deskhub] quic: …silencequic: …silence` — hai luồng lồng vào nhau giữa dòng, và nó tốn
  thời gian thật trong lúc truy vết vì cái tên check quan trọng nằm ngay trong chỗ hỏng. `LOGI` trên
  Windows là `printf("[Deskhub] ")`, rồi format của người gọi, rồi `printf("\n")`: ba cơ hội cho một
  luồng khác chen vào giữa, và CRT chỉ khoá `stdout` trong đúng một lời gọi. Nay nó format tag, thân
  và ký tự xuống dòng vào một buffer rồi phát ra bằng một `fputs` duy nhất, đúng hình dạng mà đường
  POSIX đã có sẵn. Đó cũng là lý do nhánh Android và Apple được để yên: `__android_log_print` và một
  `fprintf` vốn đã là một lời gọi.
