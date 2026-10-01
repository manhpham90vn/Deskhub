[English](PRIVACY.md) · **Tiếng Việt** · [中文](PRIVACY.zh.md) · [日本語](PRIVACY.ja.md)

# Chính sách quyền riêng tư của Deskhub

_Ngày hiệu lực: 1 tháng 10 năm 2026 — Phiên bản 2.13_

> Đây là bản dịch của [`PRIVACY.md`](PRIVACY.md). Nếu nội dung có khác biệt, bản tiếng Anh
> là bản chuẩn.

## 1. Giới thiệu

Chính sách quyền riêng tư này mô tả cách **Deskhub** ("app", "chúng tôi") xử lý thông tin
khi bạn sử dụng các ứng dụng di động Deskhub (iOS, Android) và các ứng dụng desktop
Deskhub cho Windows, macOS và Linux (gọi chung là "Phần mềm").

Deskhub là một ứng dụng remote desktop: nó stream màn hình của một trong các máy tính của
bạn sang một thiết bị khác của bạn, và cho phép bạn điều khiển máy tính đó bằng mouse,
keyboard và thao tác chạm.

Phần mềm do một lập trình viên cá nhân phát triển và phát hành:

- **Lập trình viên:** Manh Pham
- **Liên hệ:** manhpv151090@gmail.com
- **Trang dự án:** https://github.com/manhpham90vn/Deskhub

## 2. Tóm tắt

**Lập trình viên không nhận hoặc lưu nội dung session hay dữ liệu sử dụng của bạn thông
qua Deskhub.** App vẫn xử lý và lưu một số thông tin trên chính thiết bị của bạn, như mô
tả bên dưới. Deskhub không có tài khoản người dùng, server do lập trình viên vận hành,
analytics, crash reporting, quảng cáo hay SDK bên thứ ba nào thu thập dữ liệu được nhúng vào app.

## 3. Thông tin Phần mềm xử lý

Để hoạt động, Phần mềm phải xử lý một số dữ liệu, **hoàn toàn trên và giữa các thiết bị
của chính bạn**. Không dữ liệu nào trong số đó được truyền tới lập trình viên hay bất kỳ
bên thứ ba nào.

| Dữ liệu | Mục đích | Nơi lưu chuyển | Thời gian lưu giữ |
|---|---|---|---|
| Nội dung màn hình của máy được share (frame video) | Hiển thị màn hình đó trên thiết bị khác của bạn | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS) | Không lưu trữ; chỉ tồn tại trong bộ nhớ trong thời gian session diễn ra |
| Âm thanh mà máy được share đang phát (chỉ khi máy đó share âm thanh và viewer yêu cầu) | Cho phép người đang xem nghe được âm thanh của máy đó | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS), ở dạng audio đã nén | Không lưu trữ; chỉ tồn tại trong bộ nhớ trong thời gian session diễn ra |
| Input từ mouse, keyboard và cảm ứng | Điều khiển máy được share từ thiết bị khác của bạn | Gửi trực tiếp từ thiết bị đang xem tới máy được share, encrypt trên đường truyền (QUIC/TLS) | Không lưu trữ, và không bao giờ được ghi vào log; được loại bỏ sau khi inject |
| Khóa của thiết bị này — một private key được tạo trong lần chạy đầu tiên | Chứng minh danh tính của thiết bị này theo cả hai chiều: với các thiết bị kết nối tới nó khi nó share, và với các host mà nó kết nối tới; người dùng nhìn thấy dưới dạng một fingerprint duy nhất (`SHA256:…`) | Ghi vào `host_key.pem` trong thư mục riêng của app. Private key không bao giờ được gửi tới thiết bị khác: khi share, một certificate được dựng từ khóa được gửi tới các thiết bị kết nối đến; khi kết nối, chỉ gửi public key và chữ ký. Certificate không được giữ lại — thư viện TLS chỉ nạp nó từ file, nên nó được ghi trong chốc lát ra `transport_cert.<random>.pem` trong cùng thư mục, chỉ bạn đọc được, và bị xoá ngay sau khi nạp; file còn sót lại do crash bị xoá ở lần tiếp theo thiết bị này bắt đầu share. File này chỉ chứa certificate công khai — private key được đọc từ `host_key.pem`. Trên iOS, thư mục được loại khỏi backup iCloud và backup trên máy tính, trên Android backup của app bị tắt; trên Windows, macOS và Linux, công cụ backup nào copy thư mục home của bạn cũng copy file khóa theo. *Copy public key* đặt public key vào clipboard, kèm nhãn là tên thiết bị này, để bạn đưa cho chủ của một host. Các file `host_cert.pem` và `client_key*.pem` của các phiên bản trước không còn được đọc | Lưu cho tới khi bạn xoá file; khóa không bao giờ tự động được thay. Việc xoá sẽ tạo danh tính mới cho thiết bị: host đã cho phép khóa cũ phải cho phép lại, và thiết bị đã trust nó sẽ thấy nó như một host mới |
| Host đã trust (fingerprint khóa đã ghim, tên, thời điểm thiết bị này kết nối tới host lần đầu và lần gần nhất, địa chỉ và port gần nhất mà host trả lời) | Nhận diện host mà thiết bị này đã trust, ở bất kỳ địa chỉ nào nó xuất hiện, và cảnh báo khi một khóa khác trả lời ở địa chỉ từng thuộc về một host đã trust | Ghi vào `known_hosts` trong cùng thư mục; không được truyền đi | Lưu cho tới khi bạn gỡ host hoặc xoá file; địa chỉ và thời điểm lần gần nhất được cập nhật ở mỗi lần kết nối |
| Public key của các client được phép kết nối tới host này, mỗi khóa kèm một nhãn | Chỉ nhận client chứng minh được mình giữ private key tương ứng | Ghi vào `authorized_keys` trong cùng thư mục, mỗi dòng một khóa và nhãn của nó; không ghi lại thời điểm nào. Không bao giờ được truyền đi. Một khóa được thêm khi bạn dán nó, khi bạn approve yêu cầu kết nối của thiết bị đó, hoặc khi thiết bị đó scan mã QR của host này — trong hai trường hợp sau, nhãn là tên mà thiết bị gửi | Lưu cho tới khi bạn gỡ khóa hoặc xoá file; không có file này thì không ai kết nối được |
| Yêu cầu kết nối — tên, public key, địa chỉ và thời điểm của mỗi thiết bị đã thử kết nối tới host này khi chưa được cho phép | Cho chủ của host này thấy ai đang xin vào và quyết định bằng *Approve* hoặc *Deny* | Thiết bị yêu cầu gửi tên và public key của nó qua kết nối đã encrypt và ký bằng private key tương ứng; chỉ khi chữ ký đó hợp lệ, host này mới ghi chúng, cùng địa chỉ nó nhìn thấy và thời điểm, vào `access_requests` trong cùng thư mục. Không bao giờ được truyền đi ngoài hai thiết bị; chỉ hiển thị trên màn hình của chính host này | Tối đa 16 yêu cầu. Mỗi yêu cầu bị xoá sau 10 phút, hoặc ngay khi bạn bấm *Approve* (chuyển khóa vào `authorized_keys`). *Deny* gỡ yêu cầu khỏi danh sách ngay lập tức nhưng vẫn giữ hàng đó, đánh dấu là đã bị từ chối, cho tới khi hết 10 phút của nó, để thiết bị đó không thể xin lại ngay |
| Token pairing QR — mã ngẫu nhiên dùng một lần mà host này phát hành trong khi hiển thị mã QR | Cho đúng một thiết bị scan mã vào mà không cần thêm bước nào | Ghi vào `pairing_tokens` trong cùng thư mục, kèm thời điểm hết hạn của từng token. Token nằm trong mã QR và link bạn hiển thị — bất kỳ ai nhìn thấy màn hình đó đều đọc được — và được gửi, một lần, qua kết nối đã encrypt từ thiết bị đã scan nó. Mã QR còn chứa các địa chỉ network và port của thiết bị này, fingerprint khóa và tên thiết bị của nó | Bị xoá khi mã được ẩn, khi share dừng, khi token được dùng, hoặc sau 5 phút |
| Khung hình camera trong khi bạn scan mã QR (chỉ Android và iOS) | Đọc mã QR của host trên màn hình của nó | Chỉ xử lý trên thiết bị, để tìm và giải mã mã QR; không lưu, không truyền và không hiển thị cho ai | Không bao giờ lưu; bị loại bỏ ngay khi từng khung hình được xem xét |
| Địa chỉ IPv4 (và port, nếu bạn nhập) mà bạn nhập — Deskhub không tra cứu tên host | Kết nối tới máy còn lại | Chỉ lưu trên thiết bị bạn đã nhập | Lưu cục bộ cho tới khi bạn thay đổi |
| 10 host kết nối gần nhất — địa chỉ, thời điểm của lần kết nối gần nhất và tên mà host tự báo | Điền danh sách *Recent devices* | Ghi vào `recent-hosts.txt`, mỗi host một dòng, trong thư mục riêng của app trên thiết bị của bạn: `%USERPROFILE%\.deskhub` trên Windows, `~/.deskhub` trên macOS và Linux, sandbox của app trên iOS và Android; không bao giờ được truyền đi. File `recent-devices.txt` của các phiên bản trước bị xoá, không được chuyển đổi | Lưu cho tới khi bạn kết nối tới 10 host mới hơn, hoặc xoá file |
| Tuỳ chọn share của bạn (frame rate, bitrate, giới hạn độ phân giải, port, network dùng để share, quyền điều khiển, việc thiết bị này có điều khiển các máy nó xem hay không, clipboard sync, share âm thanh, phát âm thanh của máy bạn đang xem, keep awake, khởi động cùng OS, tự động share và chế độ nền) | Khôi phục settings trong lần mở app tiếp theo | Ghi vào `ui-settings.txt` trong cùng thư mục; trên iOS, file nằm trong app group container dùng chung giữa app và broadcast extension | Lưu cho tới khi bạn thay đổi hoặc xoá file |
| Mục khởi động do *Start Deskhub when you log in* tạo ra (chỉ desktop) | Khởi động Deskhub khi bạn đăng nhập | Được tạo bằng cơ chế riêng của hệ điều hành, bên ngoài thư mục của app: một scheduled task tên *Deskhub* trên Windows (chạy lúc đăng nhập với quyền cao nhất), `~/.config/autostart/deskhub.desktop` trên Linux, một login item trên macOS. Mục này chỉ chứa đường dẫn tới app; không có gì được truyền đi | Bị gỡ khi bạn tắt setting |
| Token quyền màn hình do desktop Linux cấp sau khi bạn chọn display trong hộp thoại chia sẻ màn hình (chỉ Linux) | Cho phép các lần share sau sử dụng lại lựa chọn đó, nên hộp thoại chỉ xuất hiện trong lần đầu | Ghi vào `portal-restore-token.txt` trong cùng thư mục; token chỉ có ý nghĩa với phiên desktop của bạn trên máy này và không được truyền đi | Được thay thế sau mỗi lần share; bị xoá khi bạn chọn *Choose screens again* hoặc xoá file |
| Văn bản clipboard (chỉ khi switch clipboard sync được bật và có một session đang chạy) | Cho phép văn bản copy trên một thiết bị được dán trên các thiết bị khác | Gửi trực tiếp giữa các thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS), giới hạn 32 KiB mỗi lần copy; chỉ văn bản thuần, không bao gồm ảnh hay file | Deskhub không lưu trữ; văn bản chỉ tồn tại trong clipboard hệ thống của từng thiết bị |
| Trạng thái broadcast đang chạy hay không, số viewer đang kết nối và tên thiết bị của họ, mức bộ nhớ của broadcast extension tính bằng megabyte, và nội dung lỗi khởi động gần nhất (chỉ iOS) | Cho phép màn hình share của app hiển thị trạng thái của broadcast extension, thành phần mà iOS chạy như một process riêng và sẽ kết thúc nếu sử dụng quá nhiều bộ nhớ | Ghi vào `broadcast-status.txt` trong cùng app group container | Bị xoá khi broadcast kết thúc |
| Tên thiết bị trong Settings → General → *Device name*; khi để trống, tên của chính máy tính hoặc thiết bị được dùng (hostname trên Windows và Linux, tên máy trên macOS, tên thiết bị trên iOS, model trên Android) | Đặt tên cho thiết bị này: hiển thị cho viewer khi thiết bị share, hiển thị cho các client được phép kết nối tới thiết bị, hiển thị trên host bạn kết nối tới cạnh địa chỉ của thiết bị này, hiển thị trong yêu cầu kết nối mà host ghi lại cho nó, và được dùng làm nhãn của public key bạn copy | Lưu trong `ui-settings.txt` ở cùng thư mục, và gửi tới host khi bạn kết nối. Dữ liệu được encrypt trên đường truyền nhưng hiển thị trên màn hình của host và được ghi vào log của host, nên tên mặc định sẽ được truyền đi nếu bạn không đặt tên do bạn chọn. Host chưa cho phép thiết bị này hiển thị tên trong danh sách yêu cầu kết nối của nó và giữ tên ở đó tối đa 10 phút. Khi thiết bị này share, tên còn được gửi tới từng client đã xác thực bằng một khóa được phép — không bao giờ trước đó — client đó giữ tên trong danh sách gần đây của nó, và tên được ghi vào mã QR mà thiết bị này hiển thị. Thiết bị kết nối bằng mã QR hoặc link đó đặt tên này, làm server name, vào packet mở đầu của TLS handshake, vốn không được encrypt — bất kỳ ai đang quan sát network vào lúc đó đều đọc được. Tên này cũng được gắn vào public key bạn copy, và là nhãn mà host lưu trong `authorized_keys` của họ khi họ approve thiết bị này hoặc cho nó vào bằng mã QR | Lưu cho tới khi bạn thay đổi hoặc xoá file. Việc xoá trắng trường sẽ quay về giá trị mặc định chứ không loại bỏ tên |
| Các file bạn chọn gửi tới một máy tính đang kết nối (chỉ khi bạn chọn file và nhấn Send) | Chuyển một file từ thiết bị này của bạn sang thiết bị khác | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS); trên điện thoại hoặc tablet, một bản sao được tạo trong cache riêng của app để đọc trong quá trình gửi | Vị trí lưu file phụ thuộc vào thiết bị nhận. Máy tính ghi file vào thư mục đã chọn cho mục đích này, mặc định là `Deskhub` trong thư mục home của người dùng, và giữ cho tới khi người dùng đó xoá. Điện thoại và tablet không có thư mục tương ứng: ảnh và video được thêm vào thư viện ảnh của thiết bị (`Pictures/Deskhub` và `Movies/Deskhub` trên Android), còn các file khác được lưu ở nơi trình duyệt file của hệ thống truy cập được, tức thư mục Documents của app trên iOS và `Download/Deskhub` trên Android, và tồn tại cho tới khi bạn xoá. Trên iOS, ảnh mà thư viện từ chối sẽ được lưu vào Documents. Đường lưu qua media store yêu cầu Android 10: trên Android 9 trở xuống, file tới nơi được lưu trong thư mục riêng của Deskhub trên thiết bị, không xuất hiện trong gallery và trong Downloads. Bản sao tạm trên thiết bị gửi bị xoá khi cửa sổ gửi đóng lại |
| Tên, kích thước và checksum của từng file được đề nghị, cùng tên, địa chỉ và fingerprint key của thiết bị gửi | Cho phép máy tính nhận hiển thị nội dung đang tới, từ chối nội dung không thể lưu, và cho chủ máy biết ai đã gửi gì | Gửi giữa hai thiết bị của bạn, encrypt trên đường truyền; máy tính nhận ghi đề nghị, quyết định và kết quả vào session log cục bộ | Lưu trong file log của máy tính đó cho tới khi bạn xoá |
| Thư mục mà một máy tính dùng để lưu file nhận được | Khôi phục lựa chọn đó trong lần mở app tiếp theo | Ghi vào `ui-settings.txt` trong thư mục riêng của app; không được truyền đi | Lưu cho tới khi bạn thay đổi hoặc xoá file |
| Thống kê kết nối (bitrate, mất packet, latency) | Điều chỉnh quality của stream và hiển thị trên thanh status | Chỉ trao đổi giữa hai thiết bị của bạn. Trên Windows, macOS và Linux, chúng còn được ghi, dưới dạng các bản tóm tắt `[DIAG]` định kỳ, vào log chẩn đoán cục bộ mô tả bên dưới | Bị loại bỏ khi session kết thúc, trừ phần nằm trong log |
| Log chẩn đoán (Windows, macOS và Linux) — thống kê kết nối, địa chỉ và port của peer, tên thiết bị mà peer gửi, fingerprint khóa, tên, kích thước và kết quả của từng file gửi hoặc nhận cùng thư mục file được lưu vào, và các lỗi. Không bao giờ chứa nội dung màn hình, phím được bấm, chuyển động con trỏ, văn bản clipboard hay output terminal | Giúp bạn, hoặc người bạn chọn gửi log cho, tìm ra nguyên nhân sự cố | Ghi ở dạng văn bản thuần, mỗi lần chạy một file `deskhub-<date>-<time>-<pid>.log`, trong thư mục riêng của app, chỉ bạn đọc được (trên Windows là nhờ quyền của thư mục đó, mà file thừa hưởng); không bao giờ được truyền đi. Trên Android và iOS, cùng các dòng đó chỉ đi vào log hệ thống (logcat, Xcode console), không ghi ra file | Log cũ bị xoá tự động để chỉ giữ lại mười file mới nhất; bạn có thể xoá thư mục hoặc các file bất cứ lúc nào |

### 3.1 Kiến trúc peer-to-peer

Mọi hoạt động trao đổi dữ liệu đều diễn ra **trực tiếp giữa hai thiết bị của bạn**, qua:

- network cục bộ của bạn (Wi-Fi hoặc LAN), hoặc
- một VPN do **bạn** vận hành hoặc đăng ký (ví dụ Tailscale), nếu bạn chọn sử dụng để
  truy cập qua Internet.

Chúng tôi không vận hành relay server, signaling server hay bất kỳ backend nào. Phần mềm
không có phương tiện kỹ thuật để gửi dữ liệu tới lập trình viên.

### 3.2 Dữ liệu Phần mềm KHÔNG xử lý

Ngoài tên thiết bị đã mô tả ở trên, Deskhub không yêu cầu tên của bạn, địa chỉ email, số
điện thoại, danh bạ, vị trí hay định danh quảng cáo. App không sử dụng microphone. App
chỉ dùng camera trong khi bạn scan mã QR của một host trên điện thoại hoặc tablet, sau khi
bạn bấm *Scan QR code*: các khung hình được giải mã trên thiết bị để tìm mã và không được
lưu hay gửi đi đâu. App chỉ truy cập ảnh và file khi bạn chọn chúng để gửi, khi thiết bị khác gửi
chúng tới bạn, hoặc khi chúng xuất hiện trên màn hình bạn chọn share. Các mục phía trên
giải thích những file đó được lưu ở đâu và trong bao lâu.

### 3.3 Share màn hình điện thoại hoặc tablet

Thiết bị Android và iOS có thể share màn hình của chính chúng, đồng thời xem được máy
khác. Stream ở chế độ **view-only**: các packet mouse và keyboard đi vào đều bị loại bỏ,
vì không hệ điều hành di động nào cho phép app thông thường điều khiển thiết bị. Nội
dung được capture là **toàn bộ màn hình**, bao gồm mọi thứ xuất hiện trong khi share:
notification, các app khác, app ngân hàng, mật khẩu bạn nhập. Trên Android, hệ thống hiển
thị hộp thoại đồng ý ghi màn hình ở mỗi phiên share và một notification thường trực trong
khi share; trên iOS, chỉ báo broadcast của hệ thống luôn hiển thị. Cả hai đều là cơ chế
của hệ điều hành và đều có thể dùng để dừng share bất cứ lúc nào. Tương tự trên desktop,
video được gửi trực tiếp tới thiết bị còn lại của bạn và không được lưu trữ hay gửi tới
chúng tôi.

### 3.4 Phạm vi của việc share màn hình và điều khiển từ xa

Việc share stream **toàn bộ display đã chọn**: mọi nội dung xuất hiện trên màn hình đó
đều hiển thị với viewer đang kết nối, bao gồm notification, popup và mọi cửa sổ bạn mở
trong khi share. (Tính năng share riêng một cửa sổ ứng dụng đã được gỡ bỏ vào
2026-07-27; Phần mềm hiện chỉ share trọn display.) Khi bạn cho phép điều khiển từ xa,
input của viewer được inject như thể người đó đang ngồi tại PC và có thể tác động tới
**mọi ứng dụng hiển thị trên display đang share**, không giới hạn trong một cửa sổ. Trên
mọi host, bạn có thể tắt hoàn toàn chức năng điều khiển từ xa trong Settings; khi đó phiên
share chuyển sang view-only và input tới nơi sẽ bị loại bỏ thay vì được inject. Hai cơ chế
an toàn luôn hoạt động khi control được cho phép: nếu người dùng tại PC thao tác với mouse
hoặc keyboard thật, remote input sẽ tạm dừng (host được ưu tiên), và mọi phím mà phía từ
xa đang giữ đều được nhả tự động khi kết nối kết thúc hoặc viewer chuyển sang cửa sổ khác.
Tối đa năm viewer có thể xem cùng một PC, nhưng tại mỗi thời điểm chỉ một viewer điều
khiển mouse và keyboard.

## 4. Các permission mà app yêu cầu

| Nền tảng | Permission | Mục đích |
|---|---|---|
| iOS | Local Network | iOS yêu cầu để gửi và nhận dữ liệu với PC trên cùng network. Chỉ dùng cho session stream. |
| iOS | Ghi màn hình (broadcast) | Chỉ khi bạn bắt đầu share màn hình của thiết bị này, từ broadcast picker của hệ thống. iOS hỏi ở mỗi lần và hiển thị chỉ báo ghi màn hình trong suốt quá trình. |
| Android | `INTERNET`, trạng thái network | Cần để mở kết nối UDP tới PC của bạn. Chỉ dùng cho session stream. |
| Android | Đồng ý capture màn hình (`MediaProjection`) | Chỉ khi bạn bắt đầu share màn hình của thiết bị này. Android hỏi ở mỗi lần và không lưu câu trả lời. |
| Android | `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MEDIA_PROJECTION` | Duy trì phiên share khi app chuyển xuống nền hoặc màn hình tắt. Android yêu cầu để capture màn hình. |
| Android | `RECORD_AUDIO` | Android đặt API playback-capture sau permission này, và nội dung playback, tức âm thanh do chính thiết bị phát, là thứ duy nhất Deskhub capture. Permission được xin khi bắt đầu share; nếu từ chối, phiên share vẫn tiếp tục nhưng không có âm thanh. Deskhub không mở microphone. |
| Android | `POST_NOTIFICATIONS` | Hiển thị notification thường trực mà Android yêu cầu trong khi share màn hình, và nêu tên các file nhận được từ thiết bị khác, đồng thời báo khi có thiết bị xin kết nối tới máy này (notification *Connection request* kèm tên và địa chỉ của thiết bị đó). Không gửi notification nào khác. |
| iOS | Thư viện ảnh, chỉ thêm | Được xin trong lần đầu một ảnh hoặc video do người khác gửi tới thiết bị này, để lưu vào app Photos. Deskhub chỉ có thể thêm mục mới, không đọc, không sửa và không xoá nội dung có sẵn trong thư viện. Nếu từ chối, file được lưu vào thư mục Documents của app. |
| iOS | Notification | Nêu tên các file nhận được từ thiết bị khác, đồng thời báo khi có thiết bị xin kết nối tới máy này (notification *Connection request* kèm tên và địa chỉ của thiết bị đó). Không gửi notification nào khác. |
| macOS | Notification | Được xin lần đầu khi có thiết bị xin kết nối tới máy này, để hiển thị notification *Connection request* kèm tên và địa chỉ của thiết bị đó. Không thông báo gì khác. |
| Android | `CAMERA` | Chỉ được xin khi bạn bấm *Scan QR code* trên trang Client, để đọc mã QR của host trên màn hình của nó. Khung hình được giải mã trên thiết bị và không bao giờ được lưu hay gửi đi. Nếu từ chối, bạn có thể dán link của host vào trường địa chỉ thay thế. |
| iOS | Camera | Chỉ được xin khi bạn bấm *Scan QR code* trên trang Client, cho cùng mục đích và với cùng giới hạn như trên Android. Nếu từ chối, bạn có thể dán link của host thay thế. |
| macOS | Screen Recording | Cần để capture màn hình (và âm thanh nó phát) khi máy Mac này share. Cấp một lần trong System Settings → Privacy & Security. |
| macOS | Accessibility | Cần để inject input mouse và keyboard của viewer khi máy Mac này share với control được cho phép, và để nhận biết khi người ngồi tại máy Mac chạm vào nó (host được ưu tiên). |
| macOS | Local Network | macOS hỏi trong lần đầu Deskhub truy cập một thiết bị khác trên network của bạn. Chỉ dùng cho session. |
| Windows | Quyền administrator (UAC) | App chạy ở quyền cao để inject được input vào các cửa sổ quyền cao. UAC hỏi ở mỗi lần mở app, trừ khi *Start Deskhub when you log in* khởi động nó qua scheduled task. |
| Windows | Windows Firewall | Khi bạn share, app ở quyền cao thêm hoặc làm mới một rule UDP chiều vào tên *Deskhub (host)* cho chính nó, trên mọi network profile. App không thay đổi rule nào khác; rule chặn bạn tự tạo cho Deskhub vẫn được giữ và khiến nó không truy cập được. |
| Linux | Đồng ý chia sẻ màn hình (desktop portal) | Hộp thoại của chính desktop hỏi share màn hình nào trong lần đầu; câu trả lời được ghi nhớ qua token mô tả ở mục 3. |
| Linux | Thiết bị input ảo (`/dev/uinput`) | Cần để inject input của viewer. Gói `.deb` và `.rpm` cài một udev rule mở nó cho nhóm `input` và cho người dùng đang đăng nhập tại seat. |
| Linux | Đọc `/dev/input/event*` | Chỉ cần cho cơ chế host được ưu tiên — nhận biết người ngồi tại máy đang gõ phím hoặc di chuyển mouse. Được cấp khi bạn tự thêm tài khoản của mình vào nhóm `input`; gói cài đặt không làm việc này, và nếu thiếu, Deskhub vẫn hoạt động nhưng không bao giờ tạm dừng remote input. Deskhub chỉ đọc việc có input xảy ra hay không, và không ghi lại gì. |

Trên desktop, việc share âm thanh không cần permission riêng: Phần mềm capture nội dung
mà chính máy tính đang phát, không phải microphone. Android là ngoại lệ, và chỉ về mặt tên
gọi: API playback-capture nằm sau `RECORD_AUDIO`, nên một thiết bị Android muốn share âm
thanh phải có permission mà hệ thống gọi là *Microphone*. Deskhub không dùng permission
này cho mục đích nào khác. Phần mềm không ghi microphone, không có audio hai chiều, và
không xin permission microphone trên bất kỳ nền tảng nào khác.

Các app không yêu cầu permission nào ngoài những permission liệt kê ở trên. Nếu một phiên bản trong tương lai cần thêm
permission, permission đó sẽ được xin trong đúng ngữ cảnh và chính sách này sẽ được cập
nhật.

## 5. Analytics, quảng cáo và bên thứ ba

- **Analytics và telemetry:** không có.
- **Crash reporting:** không có sẵn trong Phần mềm. Log chẩn đoán chỉ tồn tại trên máy của
  bạn, trong output console của app và, trên Windows, macOS và Linux, trong các file văn
  bản thuần dưới `~/.deskhub/` (`%USERPROFILE%\.deskhub` trên Windows), chỉ bạn đọc được,
  và log cũ được xoá tự động. Mục 3 liệt kê nội dung của chúng. Các log này không được
  upload; chúng chỉ rời thiết bị của bạn nếu bạn tự sao chép và gửi đi, và bạn có
  thể xoá thư mục đó bất cứ lúc nào.
- **Quảng cáo:** không có.
- **SDK bên thứ ba:** không có SDK nào thu thập dữ liệu. Ngoài chính source code của nó
  (công bố tại trang dự án) và các framework của hệ điều hành, Phần mềm được build với các
  thư viện mã nguồn mở chạy hoàn toàn trên thiết bị của bạn và không tự gửi gì đi đâu:
  quiche và BoringSSL (encrypt và transport), Opus (âm thanh), FFmpeg (video trên Linux)
  và, trên Android, AndroidX, CameraX và ZXing (giao diện, camera và giải mã QR). Chúng
  được liệt kê kèm giấy phép trong [`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md).
- **Phân phối app:** app iOS được phát hành qua TestFlight của Apple và app Android qua
  Google Play. Apple và Google có thể thu thập dữ liệu cài đặt, sử dụng và crash theo
  chính sách quyền riêng tư của họ; việc thu thập này nằm ngoài tầm kiểm soát của chúng
  tôi. Qua TestFlight và Google Play Console, họ có thể cho lập trình viên xem crash report
  từ từng thiết bị mà chủ sở hữu đã cho phép chia sẻ — kèm model thiết bị, phiên bản hệ
  điều hành và phiên bản app — và, với TestFlight, mọi phản hồi mà tester chọn gửi, cùng
  các thống kê tổng hợp. Không dữ liệu nào trong số đó chứa nội dung session của bạn.
- **Tailscale và các VPN khác:** nếu bạn kết nối qua một VPN, dữ liệu của bạn được xử lý
  theo chính sách quyền riêng tư của nhà cung cấp đó. Deskhub không yêu cầu và không đóng
  gói kèm VPN nào.

## 6. Bảo mật

- Lưu lượng stream nằm trong network của bạn hoặc đường hầm VPN của bạn. Khi bạn sử dụng
  một VPN như Tailscale, dữ liệu giữa các thiết bị được chính VPN đó encrypt end-to-end
  (WireGuard).
- Deskhub encrypt lưu lượng session: video, control, input, clipboard và dữ liệu terminal
  đều chạy trên QUIC/TLS giữa các thiết bị của bạn. Client phải ký một transcript của
  kết nối bằng một khóa có trong `authorized_keys` của host, và client kiểm tra khóa host
  đã ghim trước khi gửi bất cứ thứ gì. Một khóa chỉ được liệt kê khi chủ của host approve
  yêu cầu kết nối của thiết bị đó, cho nó xem mã QR của host, hoặc dán public key của nó.
  Không có passcode nào được lưu hay truyền đi.
  Deskhub không bao giờ scan network của bạn và không trả lời request discovery không
  encrypt nào. Tên thiết bị được encrypt trên đường truyền nhưng hiển thị trên host, hiển
  thị trong các yêu cầu kết nối và được gắn vào public key bạn copy, nên
  không nên đặt thông tin nhạy cảm vào trường này. Không phơi Deskhub trực tiếp ra
  Internet. Threat model đầy đủ, gồm phạm vi được bảo vệ, phạm vi không được bảo vệ và
  cách báo lỗ hổng, nằm trong
  [`SECURITY.vi.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.vi.md).
- Dữ liệu do các phiên bản trước để lại không được chuyển đổi: danh sách `paired_devices`
  cũ, dấu kích hoạt cũ và file `auth_salt` cũ bị xoá, còn dòng passcode trong một
  `ui-settings.txt` cũ bị gỡ ở lần đầu file được nạp. Một file
  `authorized_keys` hoặc `known_hosts` không đọc được sẽ từ chối truy cập trong khi vẫn
  không đọc được, và được ghi mới ở lần thay đổi tiếp theo.
- Vì chúng tôi không lưu giữ dữ liệu nào về bạn, không tồn tại cơ sở dữ liệu phía lập
  trình viên có thể bị xâm phạm.

## 7. Lưu giữ và xoá dữ liệu

Chúng tôi không lưu giữ dữ liệu nào, nên không có dữ liệu nào để chúng tôi xoá. Toàn bộ
dữ liệu session biến mất khi session kết thúc. Địa chỉ lưu trong app được gỡ bỏ bằng cách
xoá trắng trường tương ứng hoặc gỡ cài đặt app. Danh sách thiết bị gần đây, các settings
đã lưu, khóa, các client được phép, các host đã trust, mọi yêu cầu kết nối đang chờ và mọi
token QR còn hiệu lực được gỡ bỏ bằng cách xoá thư mục của app
(`%USERPROFILE%\.deskhub` trên Windows, `~/.deskhub` trên macOS và Linux); app sẽ tạo lại
thư mục rỗng trong lần chạy tiếp theo; log chẩn đoán nằm trong cùng thư mục và bị xoá theo.
Trên iOS và Android, việc gỡ cài đặt app sẽ xoá các dữ liệu này. Mục khởi động do *Start
Deskhub when you log in* tạo ra nằm ngoài thư mục đó: hãy tắt setting trước khi xoá thư mục
hoặc gỡ cài đặt, hoặc tự gỡ mục đó.

File do thiết bị khác gửi tới thuộc về bạn, không thuộc về app. Sau khi được giao, chúng
nằm trong thư mục mà máy tính đó đã chọn, hoặc trong thư viện ảnh, Documents hay Downloads
trên điện thoại và tablet. Việc gỡ cài đặt Deskhub không ảnh hưởng tới các file này; bạn
cần xoá chúng tại vị trí tương ứng.

## 8. Quyền của bạn (GDPR, CCPA và các quy định tương tự)

Các quy định như Quy định chung về bảo vệ dữ liệu của EU (GDPR) và Đạo luật quyền riêng
tư người tiêu dùng California (CCPA) trao cho bạn các quyền đối với dữ liệu cá nhân: truy
cập, chỉnh sửa, xoá, di chuyển dữ liệu, phản đối và không bị phân biệt đối xử.

Vì Deskhub không thu thập và không lưu giữ dữ liệu cá nhân, không có dữ liệu nào để thực
hiện các quyền này. Nếu bạn cho rằng chúng tôi có lưu giữ dữ liệu về bạn, vui lòng liên hệ
theo địa chỉ bên dưới; chúng tôi sẽ phản hồi trong vòng 30 ngày.

Chúng tôi không "bán" và không "chia sẻ" thông tin cá nhân theo định nghĩa của CCPA.

## 9. Quyền riêng tư của trẻ em

Phần mềm không hướng tới trẻ em và, như đã mô tả ở trên, không thu thập dữ liệu từ bất kỳ
ai, bao gồm trẻ em dưới 13 tuổi (COPPA) và dưới 16 tuổi (GDPR).

## 10. Chuyển dữ liệu qua biên giới

Deskhub không gửi dữ liệu cho lập trình viên. Nếu bạn kết nối qua VPN, dữ liệu session đi
giữa các thiết bị của bạn trên network đó, như mô tả ở mục 3.1.

## 11. Thay đổi đối với chính sách này

Nếu cách Phần mềm xử lý dữ liệu thay đổi (ví dụ một phiên bản trong tương lai bổ sung
crash reporting tuỳ chọn), chính sách này sẽ được cập nhật **trước khi** thay đổi được
phát hành, kèm một ngày hiệu lực mới và một mục changelog bên dưới. Phiên bản hiện hành
luôn được công bố tại: https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md

| Phiên bản | Ngày | Nội dung thay đổi |
|---|---|---|
| 2.13 | 2026-10-01 | **Yêu cầu kết nối bị từ chối được giữ lại cho tới khi hết hạn.** Trước đây, bấm *Deny* trên một yêu cầu kết nối sẽ xoá hàng đó khỏi `access_requests` ngay lập tức. Giờ nó gỡ yêu cầu khỏi danh sách ngay lập tức nhưng vẫn giữ hàng đó — cùng tên, public key, địa chỉ và thời điểm, đánh dấu là đã bị từ chối — cho tới khi hết 10 phút của nó, để các lần thử tiếp theo của thiết bị bị từ chối đều bị khước từ thay vì tạo một yêu cầu mới. Không có gì mới được thu thập hay truyền đi. |
| 2.12 | 2026-10-01 | **Đính chính và xử lý dữ liệu cục bộ chặt chẽ hơn.** File log chẩn đoán trên Windows, macOS và Linux giờ chỉ bạn đọc được, và log cũ bị xoá tự động để chỉ giữ lại mười file mới nhất; app Windows không còn ghi các phím viewer bấm vào đó, dù trong điều khiển từ xa hay trong terminal; chính sách này giờ liệt kê nội dung của chúng, và trong đó không có phím mà viewer bấm. Thống kê kết nối được ghi vào các log đó, điều mà các phiên bản trước của chính sách chưa nêu. Một yêu cầu kết nối chỉ được ghi lại sau khi thiết bị yêu cầu đã ký bằng khóa của nó, nên khóa hiển thị đã được chứng minh. Certificate TLS dựng từ khóa của thiết bị này — chỉ phần công khai — được ghi trong chốc lát ra một file chỉ bạn đọc được và bị xoá ngay, file còn sót lại do crash bị xoá ở lần tiếp theo thiết bị này bắt đầu share; các phiên bản trước nói certificate không bao giờ được lưu. Trên iOS, thư mục chứa private key của thiết bị này giờ được loại khỏi backup iCloud và backup trên máy tính. File `auth_salt` cũ giờ bị xoá, và dòng passcode còn sót trong một `ui-settings.txt` cũ bị gỡ ngay khi file được nạp. Phiên bản này cũng đính chính các nội dung trước: `known_hosts` ghi thời điểm lần đầu và lần gần nhất kết nối tới mỗi host; file settings còn lưu việc thiết bị này có điều khiển các máy nó xem hay không và có phát âm thanh của chúng hay không; mục khởi động do *Start Deskhub when you log in* tạo ra giờ được liệt kê; file trạng thái broadcast trên iOS chứa tên các viewer đang kết nối; thiết bị kết nối bằng mã QR hoặc link gửi tên host không encrypt trong TLS handshake mở đầu; địa chỉ bạn nhập là địa chỉ IPv4, không bao giờ là tên host được tra cứu; bảng permission giờ bao gồm macOS, Windows và Linux; Phần mềm có chứa thư viện mã nguồn mở nhưng không có SDK nào thu thập dữ liệu; và app iOS được phát hành qua TestFlight, nơi mà, giống Google Play Console, có thể cho lập trình viên xem crash report theo từng thiết bị. |
| 2.11 | 2026-09-30 | **Một khóa cho mỗi thiết bị, yêu cầu kết nối và pairing bằng QR.** Mỗi thiết bị giờ có một khóa duy nhất (`host_key.pem`) là danh tính của nó cả khi share lẫn khi kết nối; các khóa client riêng (`client_key*.pem`) và certificate được lưu (`host_cert.pem`) không còn nữa — certificate được dựng trong bộ nhớ và không bao giờ được lưu, và các file còn sót lại bị bỏ qua, không được chuyển đổi. Host đã trust được ghi nhớ theo fingerprint khóa cùng địa chỉ gần nhất mà mỗi host trả lời, không còn theo địa chỉ. Hai file mới trong thư mục của app, cả hai đều không bao giờ được truyền đi ngoài hai thiết bị liên quan: `access_requests` giữ tên, public key, địa chỉ và thời điểm của mỗi thiết bị đã xin kết nối khi chưa được cho phép (tối đa 16, mỗi yêu cầu bị xoá sau 10 phút hoặc khi *Approve* / *Deny*), và `pairing_tokens` giữ các token ngẫu nhiên dùng một lần đứng sau mã QR mà host có thể hiển thị trong khi share (bị xoá khi mã được ẩn, được dùng hoặc hết hạn sau 5 phút). Bản thân mã QR chứa các địa chỉ, port, fingerprint khóa, tên thiết bị và token của host, và bất kỳ ai nhìn thấy màn hình đều đọc được. Tên của thiết bị giờ còn hiển thị trong yêu cầu kết nối mà nó để lại và trở thành nhãn của khóa khi host approve nó hoặc cho nó vào bằng mã QR. Trên Android và iOS, camera chỉ được dùng trong khi bạn scan mã QR, sau một permission được xin đúng lúc đó; khung hình được giải mã trên thiết bị và không bao giờ được lưu hay gửi đi. Một yêu cầu kết nối mới cũng phát một notification hệ thống trên thiết bị này, nêu tên và địa chỉ của thiết bị xin kết nối; notification đó do trung tâm thông báo của chính thiết bị hiển thị và không đi đâu khác. |
| 2.10 | 2026-09-29 | Host giờ gửi tên thiết bị của mình tới từng client đã xác thực bằng một khóa được phép — không gửi gì trước khi xác thực — và client giữ tên đó trong danh sách gần đây. Danh sách gần đây chuyển sang một file mới, `recent-hosts.txt` (địa chỉ, thời điểm của lần kết nối gần nhất, tên host; tối đa 10). File `recent-devices.txt` cũ bị xoá thay vì được chuyển đổi. |
| 2.9 | 2026-09-29 | **Passcode không còn nữa và quyền truy cập hoạt động như SSH.** Không còn passcode nào được lưu hay truyền đi ở bất cứ đâu. LAN discovery đã bị gỡ: Deskhub không bao giờ scan network của bạn, và host không trả lời request discovery dạng plaintext nào. Host giữ public key của các client được phép trong `authorized_keys`, mỗi khóa kèm một nhãn; client giữ các host đã trust trong `known_hosts` — fingerprint khóa host đã ghim, địa chỉ, tên và khóa client dùng cho host đó. Một tên thiết bị duy nhất, đặt trong Settings, được gửi tới các host bạn kết nối và được gắn vào các public key bạn copy. Không lưu thời điểm nào cho các client được phép. Các file dữ liệu từ phiên bản trước — passcode, danh sách `paired_devices` cũ, dấu kích hoạt cũ — bị xoá thay vì được chuyển đổi. |
| 2.8 | 2026-09-28 | Host có thể lưu public key được cấp quyền trong `authorized_keys` và giữ dấu kích hoạt cục bộ. Hồ sơ host đã lưu thêm alias và khóa client được chọn. Danh sách fingerprint cũ chỉ còn được dùng trước khi bật danh sách mới. |
| 2.7 | 2026-09-28 | CLI có thể tạo và import thêm khóa ký client có tên, rồi chọn khóa cho một kết nối. Mỗi private key có tên được lưu cục bộ trong file riêng; danh sách identity chỉ xuất thông tin public key. |
| 2.6 | 2026-09-28 | Quyền kết nối client nay yêu cầu khóa ký đã được cấp quyền và khóa host đã ghim. Recent devices và UI settings không còn lưu passcode; các trường cũ bị loại bỏ khi nạp và ghi lại file an toàn. Nếu ghi thất bại, file cũ được giữ để thử lại sau. |
| 2.5 | 2026-09-07 | Đây là một đính chính, không phải thay đổi về hành vi: Deskhub hoạt động như trước. Các phiên bản trước của chính sách này nêu rằng file tới trên điện thoại hoặc tablet Android được lưu vào `Pictures/Deskhub`, `Movies/Deskhub` hoặc `Download/Deskhub` qua media store của hệ thống. Điều này đúng với Android 10 trở lên. Đường lưu qua media store mà Deskhub sử dụng yêu cầu Android 10, nên trên Android 9 trở xuống, file tới nơi được lưu trong thư mục riêng của app trên thiết bị và không xuất hiện trong gallery hay trong Downloads. Trong cả hai trường hợp, file chỉ tới hai thiết bị liên quan. |
| 2.4 | 2026-08-28 | **Điện thoại và tablet hiện nhận file cũng như gửi file**, và vị trí lưu file trên các thiết bị này là nội dung mới. Trên iOS, ảnh và video được thêm vào thư viện ảnh; hệ thống xin permission Photos dạng chỉ-thêm trong lần đầu, và Deskhub chỉ có thể thêm mục mới, không đọc và không sửa nội dung có sẵn. Các file khác được lưu vào thư mục Documents của app, nơi app Files truy cập được. Trên Android, ảnh được lưu vào `Pictures/Deskhub`, video vào `Movies/Deskhub` và các file khác vào `Download/Deskhub`, đều qua media store của hệ thống. Cả hai nền tảng đều nêu tên file vừa tới trong một notification. Không dữ liệu nào trong số đó tới chúng tôi. Phiên bản này cũng đính chính hai điểm mà các phiên bản trước nêu chưa chính xác: Android luôn cần permission mà hệ thống gọi là *Microphone* (`RECORD_AUDIO`) để capture nội dung do chính thiết bị phát, tức phần share âm thanh mô tả ở phiên bản 2.1, trong khi Deskhub không ghi microphone; và các app desktop chưa bao giờ mất ô chọn *File transfer* mô tả ở phiên bản 2.3, chỉ phần setting được lưu đằng sau nó bị gỡ. |
| 2.3 | 2026-08-24 | **Việc nhận file không còn là một setting được lưu.** Tuỳ chọn *Take files viewers send* đã được gỡ khỏi `ui-settings.txt`: điện thoại và tablet nhận file bất cứ khi nào app hiển thị trên màn hình, còn máy tính đưa *File transfer* vào danh sách nội dung được share, được chọn sẵn ở mỗi lần và không được lưu, nên máy tính vẫn chỉ nhận file trong khi đang share. Cách xử lý một file tới nơi không thay đổi: vẫn yêu cầu bên gửi đã pair và đã được chấp nhận, vẫn được lưu vào nơi máy đó dành cho file nhận được, vẫn không ghi đè file đã tồn tại, và vẫn được ghi log cục bộ kèm tên, địa chỉ và fingerprint key của thiết bị gửi. Việc share màn hình vẫn là một thao tác chủ động sau nút riêng, và một máy tính đang share màn hình vẫn tiếp tục nhận file trong thời gian đó. |
| 2.2 | 2026-08-21 | Deskhub hỗ trợ **gửi file** giữa các thiết bị của bạn. Một máy tính đang share màn hình có thể đồng thời nhận file, và mọi thiết bị kết nối tới nó đều có thể chọn và gửi file. Trên Android và iOS, file được chọn từ photo picker hoặc trình duyệt file của hệ thống, và một bản sao được tạo trong cache riêng của app trong khi gửi, sau đó bị xoá. File được truyền trực tiếp giữa hai thiết bị của bạn trên cùng transport đã encrypt như phần hình ảnh, và không được gửi tới chúng tôi hay đi qua bất kỳ server nào của chúng tôi. Máy tính nhận ghi file vào thư mục do nó chọn, mặc định là `Deskhub` trong thư mục home của người dùng và được lưu cùng các tuỳ chọn khác trong `ui-settings.txt`, không ghi đè file đã tồn tại, và ghi từng đề nghị, quyết định cùng kết quả, kèm tên, địa chỉ và fingerprint key của thiết bị gửi, vào session log cục bộ. Việc nhận file mặc định tắt trừ khi người share bật lên, và điện thoại cùng tablet chỉ gửi file, không nhận. |
| 2.1 | 2026-08-19 | Việc share màn hình có thể kèm theo **âm thanh** của máy tính đó. Nội dung được capture là bản mix mà loa của chính máy tính đang phát, không phải microphone; Deskhub không có audio hai chiều và không xin permission microphone. Phần audio được nén, gửi trực tiếp tới những người đang xem trên cùng transport đã encrypt như phần hình ảnh, và không được lưu trữ. Âm thanh chỉ được truyền khi máy đang share bật *Share this device's sound* **và** viewer bật *Play the sound of the device you are watching*; tắt một trong hai switch sẽ dừng việc truyền. Cả hai switch được lưu cùng các tuỳ chọn khác trong `ui-settings.txt` và mặc định đều bật. |
| 2.0 | 2026-08-15 | Session chạy trên một transport đã encrypt (QUIC/TLS), áp dụng cho video, input, clipboard và lưu lượng terminal, và việc chấp nhận máy dựa trên pairing. Dữ liệu mới được lưu trên chính thiết bị của bạn, đều nằm trong thư mục của app và không được gửi tới chúng tôi: một cặp key là danh tính của máy này (`host_key.pem`, `host_cert.pem`), key của các host bạn đã trust (`known_hosts`), các máy được host này chấp nhận (`paired_devices`, gồm fingerprint key, tên do từng máy gửi và các mốc thời gian), và một salt không bí mật (`auth_salt`). Passcode trở thành tuỳ chọn và không được truyền đi: pairing handshake chứng minh mã mà không gửi mã. |
| 1.9 | 2026-08-14 | Trên Linux, lựa chọn màn hình trong hộp thoại chia sẻ màn hình của desktop được lưu lại: token quyền do desktop cấp được ghi vào `portal-restore-token.txt` để các lần share sau bỏ qua hộp thoại. Token chỉ có hiệu lực với phiên desktop của bạn trên máy này, không được truyền đi, được thay thế sau mỗi lần share, và bị xoá khi bạn chọn *Choose screens again* hoặc xoá file. |
| 1.8 | 2026-08-14 | Bổ sung switch *keep awake* (mặc định bật): trong khi bạn share hoặc xem, app yêu cầu hệ điều hành không chuyển máy sang sleep và không tắt display, đồng thời gỡ bỏ yêu cầu này khi session kết thúc. Chỉ trạng thái bật hoặc tắt được lưu, trong cùng file settings cục bộ; không dữ liệu nào liên quan được truyền đi, và không thiết lập sleep nào của hệ thống bị thay đổi. |
| 1.7 | 2026-08-13 | Clipboard sync hoạt động trên Android và iOS, với cùng switch, giới hạn 32 KiB và quy tắc chỉ văn bản thuần như trên desktop. Hệ điều hành có các giới hạn riêng: thiết bị Android chỉ đọc được clipboard của chính nó khi Deskhub ở tiền cảnh, còn văn bản nhận từ ngoài luôn được áp dụng; iOS có thể hiển thị prompt dán của hệ thống khi Deskhub đọc nội dung copy mới; thiết bị iOS đang host không tham gia sync clipboard, vì broadcast chạy trong một process riêng. Điện thoại và tablet cũng có tuỳ chọn chọn network để share như trên desktop, lưu trong cùng file settings cục bộ và không được gửi đi. Ngoài tuỳ chọn này, không dữ liệu mới nào được lưu trên thiết bị. |
| 1.6 | 2026-08-13 | App desktop bổ sung clipboard sync tuỳ chọn: khi switch được bật, văn bản thuần bạn copy trong một session được gửi giữa các thiết bị của bạn (chưa encrypt, giống phần lưu lượng còn lại tại thời điểm đó, giới hạn 32 KiB) và đặt vào clipboard của máy còn lại; Deskhub không lưu trữ nội dung này. Các settings mới được lưu cục bộ: network dùng để share, khởi động cùng OS, tự động share khi mở app, chế độ nền và tray, cùng chính switch clipboard. Bật tuỳ chọn khởi động cùng OS sẽ tạo mục khởi động của nền tảng tương ứng (một file autostart trên Linux, một scheduled task tên *Deskhub* trên Windows, một Login Item trên macOS); tắt tuỳ chọn sẽ gỡ mục đó. |
| 1.5 | 2026-08-13 | Mỗi client có thể đặt tên thiết bị qua trường *Your name* trên trang connect. Tên được lưu trong file settings `ui-settings.txt` trên thiết bị của bạn và gửi tới host khi bạn kết nối (chưa encrypt, giống phần lưu lượng còn lại tại thời điểm đó), để host hiển thị viewer này trong danh sách session, các dòng status và log. Host chỉ giữ tên trong bộ nhớ, trong thời gian bạn đang kết nối, và không lưu trữ. Trường này được điền sẵn bằng tên máy tính hoặc thiết bị của bạn, nên giá trị mặc định sẽ được truyền đi nếu bạn không thay bằng tên do bạn chọn. |
| 1.4 | 2026-08-12 | File trạng thái trên iOS mà broadcast extension dùng chung với app ghi thêm mức bộ nhớ của extension tính bằng megabyte, để màn hình share hiển thị giá trị này. Giá trị chỉ mô tả process broadcast của Deskhub, nằm trong app group container trên thiết bị của bạn, và bị xoá cùng phần còn lại của file trạng thái khi broadcast kết thúc. |
| 1.3 | 2026-08-12 | Thiết bị Android và iOS có thể share màn hình của chính chúng ở chế độ view-only, nên màn hình điện thoại hoặc tablet có thể được stream sang thiết bị khác của bạn. Thay đổi này bổ sung các permission capture màn hình mà từng OS yêu cầu (cùng một foreground service và notification tương ứng trên Android) và, trên iOS, một app group container dùng chung giữa app và broadcast extension để lưu passcode và port, cùng một file trạng thái tạm thời mà extension ghi vào đó để app hiển thị trạng thái của broadcast. Video vẫn chỉ được truyền giữa các thiết bị của bạn và không được lưu trữ. |
| 1.2 | 2026-08-07 | Passcode trở thành bắt buộc trên mọi host, được sinh ra trong lần chạy đầu tiên thay vì để trống, và mọi client đều có thể nhập. Settings share được lưu trên macOS và Linux bên cạnh Windows, còn danh sách thiết bị gần đây được lưu trên mọi nền tảng, trong thư mục cục bộ riêng của app. Không dữ liệu mới nào rời khỏi thiết bị của bạn. |
| 1.1 | 2026-08-05 | App Windows lưu dữ liệu giữa các lần chạy: danh sách 10 địa chỉ kết nối gần nhất, các settings share, và các passcode đã dùng. Toàn bộ dữ liệu nằm trong `%USERPROFILE%\.deskhub` trên máy của bạn và không được truyền đi. Phiên bản này cũng ghi nhận chế độ share view-only và giới hạn 5 viewer. |
| 1.0 | 2026-07-24 | Lần xuất bản đầu tiên. |

## 12. Liên hệ

Với mọi câu hỏi về chính sách này hoặc về quyền riêng tư trong Deskhub:

- **Email:** manhpv151090@gmail.com
- **Issues:** https://github.com/manhpham90vn/Deskhub/issues
