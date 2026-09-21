[English](PRIVACY.md) · **Tiếng Việt** · [中文](PRIVACY.zh.md) · [日本語](PRIVACY.ja.md)

# Chính sách quyền riêng tư của Deskhub

_Ngày hiệu lực: 7 tháng 9 năm 2026 — Phiên bản 2.5_

> Các bản dịch có tại [`PRIVACY.vi.md`](PRIVACY.vi.md),
> [`PRIVACY.zh.md`](PRIVACY.zh.md) và [`PRIVACY.ja.md`](PRIVACY.ja.md). Bản tiếng Anh là
> bản chuẩn.

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

**Deskhub không thu thập, không lưu trữ, không bán và không chia sẻ bất kỳ dữ liệu cá
nhân nào. Chúng tôi không vận hành server nào, và không dữ liệu nào về bạn hoặc về cách
bạn sử dụng ứng dụng được chuyển tới chúng tôi hay bất kỳ bên thứ ba nào thông qua Phần
mềm.** Phần mềm không có tài khoản người dùng, không analytics, không crash reporting,
không quảng cáo và không nhúng SDK của bên thứ ba.

## 3. Thông tin Phần mềm xử lý

Để hoạt động, Phần mềm phải xử lý một số dữ liệu, **hoàn toàn trên và giữa các thiết bị
của chính bạn**. Không dữ liệu nào trong số đó được truyền tới lập trình viên hay bất kỳ
bên thứ ba nào.

| Dữ liệu | Mục đích | Nơi lưu chuyển | Thời gian lưu giữ |
|---|---|---|---|
| Nội dung màn hình của máy được share (frame video) | Hiển thị màn hình đó trên thiết bị khác của bạn | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS) | Không lưu trữ; chỉ tồn tại trong bộ nhớ trong thời gian session diễn ra |
| Âm thanh mà máy được share đang phát (chỉ khi máy đó share âm thanh và viewer yêu cầu) | Cho phép người đang xem nghe được âm thanh của máy đó | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS), ở dạng audio đã nén | Không lưu trữ; chỉ tồn tại trong bộ nhớ trong thời gian session diễn ra |
| Input từ mouse, keyboard và cảm ứng | Điều khiển máy được share từ thiết bị khác của bạn | Gửi trực tiếp từ thiết bị đang xem tới máy được share, encrypt trên đường truyền (QUIC/TLS) | Không lưu trữ; được loại bỏ sau khi inject |
| Cặp key của máy này, gồm một private key và một certificate tự ký được tạo trong lần chạy đầu tiên | Chứng minh danh tính của máy này với các máy mà nó kết nối tới; người dùng nhìn thấy dưới dạng fingerprint (`SHA256:…`) | Ghi vào `host_key.pem` và `host_cert.pem` trong thư mục riêng của app; chỉ phần công khai (certificate) được gửi tới các máy bạn kết nối | Lưu cho tới khi bạn xoá các file này; việc xoá sẽ tạo cho máy một danh tính mới, và các máy đã biết danh tính cũ sẽ hiển thị cảnh báo |
| Key của các host mà thiết bị này đã trust (fingerprint, địa chỉ, nhãn, thời điểm thấy lần đầu và lần cuối) | Nhận diện một host đã biết và cảnh báo rõ ràng nếu key của host đó thay đổi | Ghi vào `known_hosts` trong cùng thư mục; không được truyền đi | Lưu cho tới khi bạn xoá file |
| Các máy đã pair với host này: fingerprint key, tên do máy đó gửi, thời điểm pair và thời điểm thấy gần nhất | Cho phép máy đã pair kết nối lại mà không cần passcode, và liệt kê chúng trên trang Devices để bạn có thể gỡ bỏ | Ghi vào `paired_devices` trong cùng thư mục; không được truyền đi | Lưu cho tới khi bạn gỡ máy đó trên trang Devices hoặc xoá file |
| Một salt ngẫu nhiên cho verifier của passcode | Chuyển passcode thành giá trị mà pairing handshake đối chiếu, để bản thân mã không đi qua đường truyền | Ghi vào `auth_salt` trong cùng thư mục; salt được gửi tới máy đang kết nối trong quá trình handshake và không phải thông tin bí mật | Lưu cho tới khi bạn xoá file |
| Địa chỉ (IP hoặc hostname) bạn nhập | Kết nối tới máy còn lại | Chỉ lưu trên thiết bị bạn đã nhập | Lưu cục bộ cho tới khi bạn thay đổi |
| 10 địa chỉ kết nối gần nhất, thời điểm của từng lần, và passcode dùng cho từng địa chỉ | Điền danh sách *Recent devices* để bạn kết nối lại bằng một thao tác | Ghi vào `recent-devices.txt` trong thư mục riêng của app trên thiết bị của bạn: `%USERPROFILE%\.deskhub` trên Windows, `~/.deskhub` trên macOS và Linux, sandbox của app trên iOS và Android | Lưu cho tới khi bạn kết nối tới 10 địa chỉ mới hơn, hoặc xoá file |
| Tuỳ chọn share của bạn (frame rate, bitrate, giới hạn độ phân giải, port, network dùng để share, việc viewer có được điều khiển máy hay không, các switch clipboard sync, keep awake, khởi động cùng OS, tự động share và chế độ nền, cùng passcode bạn yêu cầu ở viewer) | Khôi phục settings trong lần mở app tiếp theo | Ghi vào `ui-settings.txt` trong cùng thư mục. Trên iOS, file nằm trong app group container dùng chung giữa app và broadcast extension, để hai thành phần thống nhất về passcode và port | Lưu cho tới khi bạn thay đổi hoặc xoá file |
| Token quyền màn hình do desktop Linux cấp sau khi bạn chọn display trong hộp thoại chia sẻ màn hình (chỉ Linux) | Cho phép các lần share sau sử dụng lại lựa chọn đó, nên hộp thoại chỉ xuất hiện trong lần đầu | Ghi vào `portal-restore-token.txt` trong cùng thư mục; token chỉ có ý nghĩa với phiên desktop của bạn trên máy này và không được truyền đi | Được thay thế sau mỗi lần share; bị xoá khi bạn chọn *Choose screens again* hoặc xoá file |
| Văn bản clipboard (chỉ khi switch clipboard sync được bật và có một session đang chạy) | Cho phép văn bản copy trên một thiết bị được dán trên các thiết bị khác | Gửi trực tiếp giữa các thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS), giới hạn 32 KiB mỗi lần copy; chỉ văn bản thuần, không bao gồm ảnh hay file | Deskhub không lưu trữ; văn bản chỉ tồn tại trong clipboard hệ thống của từng thiết bị |
| Trạng thái broadcast đang chạy hay không, số viewer đang kết nối, mức bộ nhớ của broadcast extension tính bằng megabyte, và nội dung lỗi khởi động gần nhất (chỉ iOS) | Cho phép màn hình share của app hiển thị trạng thái của broadcast extension, thành phần mà iOS chạy như một process riêng và sẽ kết thúc nếu sử dụng quá nhiều bộ nhớ | Ghi vào `broadcast-status.txt` trong cùng app group container | Bị xoá khi broadcast kết thúc |
| Tên thiết bị trong trường *Your name*, được điền sẵn bằng tên của chính máy tính hoặc thiết bị (hostname trên Windows và Linux, tên máy trên macOS, tên thiết bị trên iOS, model trên Android) cho tới khi bạn chỉnh sửa | Hiển thị trên host bạn kết nối tới, cạnh địa chỉ của thiết bị này, để người đang share phân biệt được các viewer | Lưu trong `ui-settings.txt` ở cùng thư mục, và gửi tới host khi bạn kết nối. Dữ liệu được encrypt trên đường truyền nhưng hiển thị trên màn hình của host và được ghi vào log của host, nên giá trị mặc định sẽ được truyền đi nếu bạn không thay bằng tên do bạn chọn; việc xoá trắng trường chỉ khôi phục giá trị mặc định, sau đó giá trị này được lưu và gửi đi. Khi hai máy pair, host cũng lưu tên này trong danh sách `paired_devices` cho tới khi máy bạn bị gỡ khỏi danh sách | Mặc định là tên máy tính hoặc thiết bị; lưu cho tới khi bạn thay đổi hoặc xoá file. Việc xoá trắng trường khôi phục giá trị mặc định chứ không loại bỏ tên |
| Các file bạn chọn gửi tới một máy tính đang kết nối (chỉ khi bạn chọn file và nhấn Send) | Chuyển một file từ thiết bị này của bạn sang thiết bị khác | Gửi trực tiếp giữa hai thiết bị của bạn, encrypt trên đường truyền (QUIC/TLS); trên điện thoại hoặc tablet, một bản sao được tạo trong cache riêng của app để đọc trong quá trình gửi | Vị trí lưu file phụ thuộc vào thiết bị nhận. Máy tính ghi file vào thư mục đã chọn cho mục đích này, mặc định là `Deskhub` trong thư mục home của người dùng, và giữ cho tới khi người dùng đó xoá. Điện thoại và tablet không có thư mục tương ứng: ảnh và video được thêm vào thư viện ảnh của thiết bị (`Pictures/Deskhub` và `Movies/Deskhub` trên Android), còn các file khác được lưu ở nơi trình duyệt file của hệ thống truy cập được, tức thư mục Documents của app trên iOS và `Download/Deskhub` trên Android, và tồn tại cho tới khi bạn xoá. Trên iOS, ảnh mà thư viện từ chối sẽ được lưu vào Documents. Đường lưu qua media store yêu cầu Android 10: trên Android 9 trở xuống, file tới nơi được lưu trong thư mục riêng của Deskhub trên thiết bị, không xuất hiện trong gallery và trong Downloads. Bản sao tạm trên thiết bị gửi bị xoá khi cửa sổ gửi đóng lại |
| Tên, kích thước và checksum của từng file được đề nghị, cùng tên, địa chỉ và fingerprint key của thiết bị gửi | Cho phép máy tính nhận hiển thị nội dung đang tới, từ chối nội dung không thể lưu, và cho chủ máy biết ai đã gửi gì | Gửi giữa hai thiết bị của bạn, encrypt trên đường truyền; máy tính nhận ghi đề nghị, quyết định và kết quả vào session log cục bộ | Lưu trong file log của máy tính đó cho tới khi bạn xoá |
| Thư mục mà một máy tính dùng để lưu file nhận được | Khôi phục lựa chọn đó trong lần mở app tiếp theo | Ghi vào `ui-settings.txt` trong thư mục riêng của app; không được truyền đi | Lưu cho tới khi bạn thay đổi hoặc xoá file |
| Thống kê kết nối (bitrate, mất packet, latency) | Điều chỉnh quality của stream và hiển thị trên thanh status | Chỉ trao đổi giữa hai thiết bị của bạn | Không lưu trữ; bị loại bỏ khi session kết thúc |

### 3.1 Kiến trúc peer-to-peer

Mọi hoạt động trao đổi dữ liệu đều diễn ra **trực tiếp giữa hai thiết bị của bạn**, qua:

- network cục bộ của bạn (Wi-Fi hoặc LAN), hoặc
- một VPN do **bạn** vận hành hoặc đăng ký (ví dụ Tailscale), nếu bạn chọn sử dụng để
  truy cập qua Internet.

Chúng tôi không vận hành relay server, signaling server hay bất kỳ backend nào. Phần mềm
không có phương tiện kỹ thuật để gửi dữ liệu tới lập trình viên.

### 3.2 Dữ liệu Phần mềm KHÔNG xử lý

Phần mềm không truy cập và không xử lý: tên của bạn (ngoài tên thiết bị đã mô tả ở trên,
vốn mặc định là tên của chính máy tính hoặc thiết bị), địa chỉ email, số điện thoại, danh
bạ, vị trí, ảnh, file (ngoài nội dung hiển thị trên màn hình PC mà bạn chọn stream),
microphone, camera, định danh quảng cáo, hay bất kỳ định danh thiết bị nào vượt quá mức
hệ điều hành cần để chạy app.

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
| Android | `POST_NOTIFICATIONS` | Hiển thị notification thường trực mà Android yêu cầu trong khi share màn hình, và nêu tên các file nhận được từ thiết bị khác. Không gửi notification nào khác. |
| iOS | Thư viện ảnh, chỉ thêm | Được xin trong lần đầu một ảnh hoặc video do người khác gửi tới thiết bị này, để lưu vào app Photos. Deskhub chỉ có thể thêm mục mới, không đọc, không sửa và không xoá nội dung có sẵn trong thư viện. Nếu từ chối, file được lưu vào thư mục Documents của app. |
| iOS | Notification | Nêu tên các file nhận được từ thiết bị khác. Không gửi notification nào khác. |

Trên desktop, việc share âm thanh không cần permission riêng: Phần mềm capture nội dung
mà chính máy tính đang phát, không phải microphone. Android là ngoại lệ, và chỉ về mặt tên
gọi: API playback-capture nằm sau `RECORD_AUDIO`, nên một thiết bị Android muốn share âm
thanh phải có permission mà hệ thống gọi là *Microphone*. Deskhub không dùng permission
này cho mục đích nào khác. Phần mềm không ghi microphone, không có audio hai chiều, và
không xin permission microphone trên bất kỳ nền tảng nào khác.

Các app không yêu cầu permission nào khác. Nếu một phiên bản trong tương lai cần thêm
permission, permission đó sẽ được xin trong đúng ngữ cảnh và chính sách này sẽ được cập
nhật.

## 5. Analytics, quảng cáo và bên thứ ba

- **Analytics và telemetry:** không có.
- **Crash reporting:** không có. Log chẩn đoán (`[DIAG]`) chỉ tồn tại trên máy của bạn,
  trong output console của app và, trên Windows, macOS và Linux, trong các file văn bản
  thuần dưới `~/.deskhub/` (`%USERPROFILE%\.deskhub` trên Windows). Các log này không
  được upload; chúng chỉ rời thiết bị của bạn nếu bạn tự sao chép và gửi đi, và bạn có
  thể xoá thư mục đó bất cứ lúc nào.
- **Quảng cáo:** không có.
- **SDK bên thứ ba:** không có. Phần mềm được build từ chính source code của nó (công bố
  tại trang dự án) và các framework của hệ điều hành.
- **App store:** các app được phát hành qua Apple App Store và Google Play. Apple và
  Google có thể thu thập thống kê cài đặt và sử dụng theo chính sách quyền riêng tư của
  họ; việc thu thập này nằm ngoài tầm kiểm soát của chúng tôi, và chúng tôi chỉ nhận được
  phần thống kê tổng hợp, ẩn danh mà các nền tảng đó cung cấp cho mọi lập trình viên.
- **Tailscale và các VPN khác:** nếu bạn kết nối qua một VPN, dữ liệu của bạn được xử lý
  theo chính sách quyền riêng tư của nhà cung cấp đó. Deskhub không yêu cầu và không đóng
  gói kèm VPN nào.

## 6. Bảo mật

- Lưu lượng stream nằm trong network của bạn hoặc đường hầm VPN của bạn. Khi bạn sử dụng
  một VPN như Tailscale, dữ liệu giữa các thiết bị được chính VPN đó encrypt end-to-end
  (WireGuard).
- Deskhub encrypt lưu lượng session: video, control, input, clipboard và dữ liệu terminal
  đều chạy trên QUIC/TLS giữa các thiết bị của bạn. Việc chấp nhận kết nối do một pairing
  handshake quyết định: máy chưa biết phải chứng minh được mình biết passcode 4 chữ số tuỳ
  chọn của host (bản thân mã không được truyền đi) hoặc phải được người dùng tại host
  phê duyệt. Tên thiết bị được encrypt trên đường truyền nhưng hiển thị trên host, nên
  không nên đặt thông tin nhạy cảm vào trường này. Không phơi Deskhub trực tiếp ra
  Internet. Threat model đầy đủ, gồm phạm vi được bảo vệ, phạm vi không được bảo vệ và
  cách báo lỗ hổng, nằm trong
  [`SECURITY.vi.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.vi.md).
- Các passcode lưu trong `recent-devices.txt` và `ui-settings.txt` được làm rối bằng một
  khoá cố định để không đọc được trực tiếp. Đây không phải encrypt và không nhằm chống lại
  người đã có quyền truy cập vào tài khoản người dùng của bạn.
- Vì chúng tôi không lưu giữ dữ liệu nào về bạn, không tồn tại cơ sở dữ liệu phía lập
  trình viên có thể bị xâm phạm.

## 7. Lưu giữ và xoá dữ liệu

Chúng tôi không lưu giữ dữ liệu nào, nên không có dữ liệu nào để chúng tôi xoá. Toàn bộ
dữ liệu session biến mất khi session kết thúc. Địa chỉ lưu trong app được gỡ bỏ bằng cách
xoá trắng trường tương ứng hoặc gỡ cài đặt app. Danh sách thiết bị gần đây và các settings
đã lưu, bao gồm mọi passcode, được gỡ bỏ bằng cách xoá thư mục của app
(`%USERPROFILE%\.deskhub` trên Windows, `~/.deskhub` trên macOS và Linux); app sẽ tạo lại
thư mục rỗng trong lần chạy tiếp theo. Trên iOS và Android, việc gỡ cài đặt app sẽ xoá
các dữ liệu này.

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

Không có. Dữ liệu của bạn không rời khỏi các thiết bị và network của chính bạn thông qua
Phần mềm.

## 11. Thay đổi đối với chính sách này

Nếu cách Phần mềm xử lý dữ liệu thay đổi (ví dụ một phiên bản trong tương lai bổ sung
crash reporting tuỳ chọn), chính sách này sẽ được cập nhật **trước khi** thay đổi được
phát hành, kèm một ngày hiệu lực mới và một mục changelog bên dưới. Phiên bản hiện hành
luôn được công bố tại: https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md

| Phiên bản | Ngày | Nội dung thay đổi |
|---|---|---|
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
