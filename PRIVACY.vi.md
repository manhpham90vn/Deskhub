[English](PRIVACY.md) · **Tiếng Việt** · [中文](PRIVACY.zh.md) · [日本語](PRIVACY.ja.md)

# Chính sách quyền riêng tư của Deskhub

_Ngày hiệu lực: 28 tháng 8, 2026 — Phiên bản 2.4_

> Đây là bản dịch của [`PRIVACY.md`](PRIVACY.md). Nếu hai bản có khác biệt, **bản tiếng
> Anh là bản có giá trị pháp lý**.

## 1. Giới thiệu

Chính sách quyền riêng tư này mô tả cách **Deskhub** ("ứng dụng", "chúng tôi") xử lý
thông tin khi bạn sử dụng các ứng dụng di động Deskhub (iOS, Android) và các ứng dụng
desktop Deskhub cho Windows, macOS và Linux (gọi chung là "Phần mềm").

Deskhub là một ứng dụng remote desktop: nó truyền màn hình của một trong các máy tính của
bạn tới một thiết bị khác của bạn và cho phép bạn điều khiển máy tính đó bằng chuột, bàn
phím và thao tác chạm.

Phần mềm được phát triển và phát hành bởi một nhà phát triển cá nhân:

- **Nhà phát triển:** Manh Pham
- **Liên hệ:** manhpv151090@gmail.com
- **Trang dự án:** https://github.com/manhpham90vn/Deskhub

## 2. Bản tóm tắt

**Deskhub không thu thập, lưu trữ, bán hay chia sẻ bất kỳ dữ liệu cá nhân nào. Chúng tôi
không vận hành máy chủ nào, và không dữ liệu nào về bạn hay về cách bạn sử dụng từng đi
tới chúng tôi hay bất kỳ bên thứ ba nào thông qua Phần mềm.** Không có tài khoản người
dùng, không analytics, không báo cáo sự cố, không quảng cáo, và không có SDK bên thứ ba
nào được nhúng trong Phần mềm.

## 3. Thông tin mà Phần mềm xử lý

Để hoạt động, Phần mềm phải xử lý một số dữ liệu **hoàn toàn trên và giữa các thiết bị
của chính bạn**. Không dữ liệu nào trong số đó được truyền tới nhà phát triển hay bên thứ
ba.

| Dữ liệu | Mục đích | Nó đi đâu | Thời gian lưu |
|---|---|---|---|
| Nội dung màn hình của máy đang chia sẻ (các khung hình) | Hiển thị màn hình đó trên thiết bị còn lại của bạn | Gửi trực tiếp giữa hai thiết bị của bạn, mã hoá trên đường truyền (QUIC/TLS) | Không bao giờ lưu; chỉ tồn tại trong bộ nhớ trong lúc phiên đang chạy |
| Tiếng mà máy đang chia sẻ phát ra (chỉ khi máy đó bật chia sẻ tiếng và có viewer xin nghe) | Để người đang xem nghe được máy đó | Gửi trực tiếp giữa hai thiết bị của bạn, mã hoá trên đường truyền (QUIC/TLS), dưới dạng âm thanh đã nén | Không bao giờ lưu; chỉ tồn tại trong bộ nhớ trong lúc phiên đang chạy |
| Thao tác chuột, bàn phím và chạm | Điều khiển máy đang chia sẻ từ thiết bị còn lại | Gửi trực tiếp từ thiết bị đang xem tới máy đang chia sẻ, mã hoá trên đường truyền (QUIC/TLS) | Không bao giờ lưu; bỏ đi ngay sau khi được đưa vào máy |
| Cặp khoá của máy này — khoá riêng và chứng chỉ tự ký sinh ra ở lần chạy đầu | Chứng minh danh tính máy này với các máy nó kết nối tới; người dùng thấy nó dưới dạng dấu vân tay (`SHA256:…`) | Ghi vào `host_key.pem` và `host_cert.pem` trong thư mục riêng của app; chỉ nửa công khai (chứng chỉ) được trình cho máy bạn kết nối tới | Giữ cho tới khi bạn xoá tệp; xoá đi thì máy mang danh tính mới, và các máy từng biết danh tính cũ sẽ cảnh báo |
| Khoá của các host mà thiết bị này đã tin (dấu vân tay, địa chỉ, nhãn, lần gặp đầu/cuối) | Nhận ra host quen và cảnh báo lớn nếu khoá của nó đổi | Ghi vào `known_hosts` trong cùng thư mục; không bao giờ được truyền đi | Giữ cho tới khi bạn xoá tệp |
| Các máy đã ghép đôi với host này — dấu vân tay khoá, tên chúng gửi, thời điểm ghép và lần gặp cuối | Cho máy đã ghép vào lại không cần passcode, và liệt kê trên trang Devices để bạn quên chúng được | Ghi vào `paired_devices` trong cùng thư mục; không bao giờ được truyền đi | Giữ cho tới khi bạn bấm quên máy đó trên trang Devices hoặc xoá tệp |
| Một salt ngẫu nhiên cho verifier của passcode | Biến passcode thành giá trị mà cuộc bắt tay ghép đôi kiểm tra, để bản thân mã không bao giờ phải đi qua mạng | Ghi vào `auth_salt` trong cùng thư mục; salt được gửi cho máy đang kết nối trong lúc bắt tay (nó không phải bí mật) | Giữ cho tới khi bạn xoá tệp |
| Địa chỉ (IP/hostname) bạn nhập | Kết nối tới máy kia | Nằm lại trên chính thiết bị bạn đã nhập | Giữ cục bộ cho tới khi bạn thay đổi |
| 10 địa chỉ gần nhất bạn đã kết nối, thời điểm của từng lần, và mã passcode bạn dùng cho từng địa chỉ | Điền vào danh sách *Recent devices* để bạn kết nối lại bằng một cú bấm | Ghi vào `recent-devices.txt` trong thư mục riêng của app trên thiết bị bạn — `%USERPROFILE%\.deskhub` trên Windows, `~/.deskhub` trên macOS và Linux, vùng sandbox của app trên iOS và Android | Giữ cho tới khi bạn kết nối tới 10 địa chỉ mới hơn, hoặc bạn xoá tệp đó |
| Tuỳ chọn chia sẻ của bạn (tốc độ khung hình, bitrate, giới hạn độ phân giải, cổng, địa chỉ mạng để chia sẻ, có cho người xem điều khiển máy hay không, các công tắc đồng bộ clipboard, giữ máy thức, khởi động cùng hệ điều hành, tự chia sẻ và chế độ chạy nền, và mã passcode bạn yêu cầu người xem nhập) | Khôi phục cài đặt cho lần mở app kế tiếp | Ghi vào `ui-settings.txt` trong cùng thư mục. Trên iOS tệp này nằm trong app group container mà app và broadcast extension dùng chung, để cả hai phần thống nhất passcode và cổng | Giữ cho tới khi bạn thay đổi hoặc xoá tệp |
| Token cấp quyền màn hình mà desktop Linux phát ra sau khi bạn chọn màn hình trong hộp thoại chia sẻ của nó (chỉ Linux) | Để các lần chia sẻ sau dùng lại lựa chọn của bạn trong im lặng, nên hộp thoại chỉ hiện lần đầu | Ghi vào `portal-restore-token.txt` trong cùng thư mục; token chỉ có ý nghĩa với phiên desktop của chính bạn trên máy này và không bao giờ được truyền đi | Được thay mới sau mỗi lần chia sẻ; bị xoá khi bạn bấm *Choose screens again* hoặc xoá tệp |
| Văn bản clipboard (chỉ khi công tắc đồng bộ clipboard đang bật và có phiên đang chạy) | Để văn bản copy trên một thiết bị dán được trên các thiết bị kia | Gửi trực tiếp giữa các thiết bị của bạn, mã hoá trên đường truyền (QUIC/TLS), giới hạn 32 KiB mỗi lần copy; chỉ văn bản thuần, không bao giờ là ảnh hay tệp | Deskhub không bao giờ lưu; chỉ nằm trong clipboard hệ thống bình thường của mỗi thiết bị |
| Việc có đang phát hay không, số người xem đang kết nối, mức bộ nhớ tính bằng megabyte mà chính broadcast extension đang dùng, và nội dung lỗi khởi động gần nhất (chỉ trên iOS) | Để màn hình chia sẻ của app báo được trạng thái của broadcast extension, thứ mà iOS chạy như một tiến trình riêng và sẽ chấm dứt nếu nó dùng quá nhiều bộ nhớ | Ghi vào `broadcast-status.txt` trong cùng app group container đó | Xoá đi khi buổi phát kết thúc |
| Tên thiết bị trong ô *Your name* — được điền sẵn tên của chính máy hoặc thiết bị này (hostname trên Windows và Linux, tên máy tính trên macOS, tên thiết bị trên iOS, model trên Android) cho tới khi bạn sửa nó | Hiển thị trên host mà bạn kết nối tới, bên cạnh địa chỉ của thiết bị này, để người đang chia sẻ phân biệt được các người xem | Lưu vào `ui-settings.txt` trong cùng thư mục, và gửi tới host khi bạn kết nối — mã hoá trên đường truyền, nhưng hiển thị trên màn hình host và ghi vào nhật ký của host — nên tên mặc định này được gửi đi trừ khi bạn thay nó bằng một tên do bạn tự chọn; xoá trắng ô chỉ khôi phục lại tên mặc định, và tên đó vẫn được lưu và gửi đi. Khi hai máy ghép đôi, host còn lưu tên này trong danh sách `paired_devices` của nó cho tới khi bạn bị quên ở đó | Mặc định là tên của máy hoặc thiết bị; giữ cho tới khi bạn đổi nó hoặc xoá tệp. Xoá trắng ô chỉ khôi phục tên mặc định chứ không bỏ được tên |
| Tệp bạn chọn để gửi sang máy đang kết nối (chỉ khi bạn chọn và bấm Send) | Đưa một tệp từ thiết bị này của bạn sang thiết bị kia | Gửi thẳng giữa hai thiết bị của bạn, mã hoá trên đường truyền (QUIC/TLS); trên điện thoại hay máy tính bảng, một bản sao được chuẩn bị trước trong cache riêng của app để đọc trong lúc gửi | Tệp rơi vào đâu là tuỳ thiết bị nhận. Máy tính ghi vào thư mục nó đã chọn cho việc này — `Deskhub` trong thư mục nhà của người dùng đó nếu không chọn thư mục khác — và giữ ở đó cho đến khi người dùng đó xoá. Điện thoại hay máy tính bảng không có thư mục như vậy: ảnh và video được thêm vào thư viện ảnh của chính thiết bị (`Pictures/Deskhub` và `Movies/Deskhub` trên Android), mọi tệp khác được đặt vào nơi trình duyệt tệp của hệ thống nhìn thấy — thư mục Documents của app trên iOS, `Download/Deskhub` trên Android — và nằm đó cho tới khi bạn xoá. Trên iOS, tệp mà thư viện ảnh từ chối sẽ vào Documents. Bản sao tạm trên điện thoại hay máy tính bảng gửi đi bị xoá khi đóng cửa sổ gửi |
| Tên, kích thước và checksum của mỗi tệp được chào, cùng tên, địa chỉ và vân tay khoá của thiết bị gửi | Để máy nhận hiện được thứ đang tới và từ chối thứ nó không lưu được, và để chủ máy thấy ai đã gửi gì | Gửi giữa hai thiết bị của bạn, mã hoá trên đường truyền; máy nhận ghi lời chào, phán quyết và kết quả vào nhật ký phiên của chính nó | Nằm trong tệp nhật ký của máy đó cho đến khi bạn xoá |
| Thư mục một máy dùng để lưu tệp nhận được | Khôi phục lựa chọn đó lần sau bạn mở app | Ghi vào `ui-settings.txt` trong thư mục riêng của app; không bao giờ được gửi đi | Giữ đến khi bạn đổi hoặc xoá tệp |
| Thống kê kết nối (bitrate, tỉ lệ mất gói, độ trễ) | Điều chỉnh chất lượng luồng; hiển thị trên thanh trạng thái | Chỉ trao đổi giữa hai thiết bị của bạn | Không bao giờ lưu; bỏ đi khi phiên kết thúc |

### 3.1 Ngang hàng theo thiết kế

Toàn bộ liên lạc diễn ra **trực tiếp giữa hai thiết bị của chính bạn**, qua:

- mạng nội bộ của bạn (Wi-Fi/LAN), hoặc
- một VPN do **bạn** vận hành hoặc thuê bao (ví dụ Tailscale), nếu bạn chọn dùng để truy
  cập qua Internet.

Chúng tôi không vận hành máy chủ trung chuyển, máy chủ báo hiệu, hay bất kỳ backend nào
khác. Phần mềm không có phương tiện kỹ thuật nào để gửi dữ liệu về nhà phát triển.

### 3.2 Dữ liệu chúng tôi KHÔNG xử lý

Phần mềm không truy cập hay xử lý: tên của bạn (ngoài tên thiết bị đã mô tả ở trên,
mặc định là tên của chính máy hoặc thiết bị bạn), địa chỉ email, số điện thoại, danh bạ, vị trí,
ảnh, tệp (ngoài những gì hiển thị trên màn hình PC mà chính bạn chọn để truyền),
micro, camera, mã định danh quảng cáo, hay bất kỳ mã định danh thiết bị nào vượt quá mức
hệ điều hành cần để chạy ứng dụng.

### 3.3 Chia sẻ màn hình điện thoại hoặc máy tính bảng

Thiết bị Android và iOS vừa xem được máy khác, vừa chia sẻ được màn hình của chính nó.
Luồng này là **chỉ xem**: các gói chuột và bàn phím gửi tới đều bị bỏ đi, vì không hệ điều
hành nào trong hai hệ này cho một ứng dụng thường điều khiển máy. Thứ được thu là **toàn
bộ màn hình**, gồm cả những gì hiện lên trong lúc chia sẻ — thông báo, ứng dụng khác, ứng
dụng ngân hàng, mật khẩu bạn gõ. Trên Android, hệ thống hiện hộp thoại xin quyền quay màn
hình cho từng lần chia sẻ và một thông báo thường trực trong suốt thời gian chạy; trên
iOS, chỉ báo broadcast của hệ thống luôn hiển thị. Cả hai đều là tín hiệu của chính hệ điều
hành, và đều dùng để dừng chia sẻ bất cứ lúc nào. Giống như trên máy tính, hình ảnh đi
thẳng tới thiết bị kia của bạn, không bao giờ được lưu lại hay gửi cho chúng tôi.

### 3.4 Phạm vi của việc chia sẻ màn hình và điều khiển từ xa

Chia sẻ sẽ truyền **toàn bộ màn hình được chọn**: mọi thứ xuất hiện trên màn hình đó đều
hiện ra với người xem đang kết nối, bao gồm thông báo, cửa sổ bật lên, và bất kỳ cửa sổ
nào bạn mở trong lúc đang chia sẻ. (Tính năng chia sẻ riêng một cửa sổ ứng dụng đã bị gỡ
ngày 2026-07-27; Phần mềm hiện chỉ chia sẻ nguyên màn hình.) Khi bạn cho phép điều khiển
từ xa, thao tác của người xem được đưa vào máy như thể họ đang ngồi tại PC và có thể chạm
tới **mọi ứng dụng hiển thị trên màn hình đang chia sẻ** — không còn giới hạn trong một
cửa sổ nữa. Trên mọi host bạn đều có thể tắt hẳn điều khiển từ xa trong mục Settings,
khiến phiên chia sẻ thành chỉ xem: thao tác gửi tới sẽ bị bỏ đi thay vì được đưa vào máy. Hai
cơ chế an toàn luôn hoạt động khi điều khiển được bật: nếu người ngồi tại PC chạm vào
chuột hoặc bàn phím thật, thao tác từ xa tạm dừng ("host thắng"), và mọi phím mà phía từ
xa đang giữ sẽ được tự động nhả khi kết nối kết thúc hoặc người xem chuyển đi chỗ khác.
Tối đa năm người xem có thể cùng xem một PC, nhưng tại mỗi thời điểm chỉ một trong số họ
điều khiển chuột và bàn phím.

## 4. Các quyền mà ứng dụng yêu cầu

| Nền tảng | Quyền | Lý do |
|---|---|---|
| iOS | Local Network | iOS bắt buộc phải có để gửi/nhận dữ liệu tới PC của bạn trong cùng mạng. Chỉ dùng cho phiên truyền hình ảnh. |
| iOS | Quay màn hình (broadcast) | Chỉ khi bạn bắt đầu chia sẻ màn hình của thiết bị này, từ bộ chọn broadcast của hệ thống. iOS hỏi mỗi lần và hiện chỉ báo đang quay suốt thời gian đó. |
| Android | `INTERNET`, trạng thái mạng | Cần để mở kết nối UDP tới PC của bạn. Chỉ dùng cho phiên truyền hình ảnh. |
| Android | Đồng ý thu màn hình (`MediaProjection`) | Chỉ khi bạn bắt đầu chia sẻ màn hình của thiết bị này. Android hỏi mỗi lần; câu trả lời không thể ghi nhớ. |
| Android | `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MEDIA_PROJECTION` | Giữ cho phiên chia sẻ tiếp tục chạy khi app xuống nền hoặc màn hình tắt. Android bắt buộc phải có để thu màn hình. |
| Android | `RECORD_AUDIO` | Android đặt API thu tiếng phát lại sau quyền này, mà tiếng phát lại — thứ chính thiết bị đang phát — là thứ duy nhất Deskhub thu. Được hỏi khi bạn bắt đầu chia sẻ; từ chối thì phiên chia sẻ vẫn chạy, chỉ là không có tiếng. Deskhub không bao giờ mở micro. |
| Android | `POST_NOTIFICATIONS` | Hiện thông báo thường trực mà Android bắt buộc phải có khi đang chia sẻ màn hình, và nêu tên các tệp vừa tới khi một thiết bị khác gửi cho bạn. Không gửi thông báo nào khác. |
| iOS | Thư viện ảnh, chỉ thêm | Được hỏi ở lần đầu một ảnh hoặc video ai đó gửi tới thiết bị này, để đưa được nó vào app Photos. Deskhub chỉ có thể thêm: nó không bao giờ đọc, sửa hay xoá thứ đã có trong thư viện của bạn. Từ chối thì tệp vào thư mục Documents của app. |
| iOS | Thông báo | Nêu tên các tệp vừa tới khi một thiết bị khác gửi cho bạn. Không gửi thông báo nào khác. |

Việc chia sẻ tiếng không cần quyền riêng nào trên các máy để bàn: nó thu đúng thứ máy
tính đang phát ra, không phải micro. Android là ngoại lệ, và chỉ là ngoại lệ về tên gọi:
API thu tiếng phát lại của nó nằm sau `RECORD_AUDIO`, nên một thiết bị Android muốn chia
sẻ tiếng buộc phải có quyền mà hệ thống ghi nhãn là *Micro*. Deskhub không dùng quyền đó
vào việc gì khác. Nó không bao giờ thu micro, không có âm thanh hai chiều, và không xin
quyền micro trên bất kỳ nền tảng nào khác.

Ứng dụng không yêu cầu quyền nào khác. Nếu một phiên bản sau này cần thêm quyền mới,
quyền đó sẽ được xin đúng ngữ cảnh và chính sách này sẽ được cập nhật.

## 5. Analytics, quảng cáo và bên thứ ba

- **Analytics / telemetry:** không có.
- **Báo cáo sự cố:** không có. Nhật ký chẩn đoán (`[DIAG]`) chỉ tồn tại trên chính máy
  bạn — trong cửa sổ console của app và, trên Windows, macOS và Linux, trong các tệp văn
  bản thuần dưới `~/.deskhub/` (`%USERPROFILE%\.deskhub` trên Windows). Chúng không bao
  giờ được tải lên đâu cả; chúng chỉ rời khỏi thiết bị của bạn nếu chính bạn sao chép và
  gửi đi, và bạn có thể xoá thư mục đó bất cứ lúc nào.
- **Quảng cáo:** không có.
- **SDK bên thứ ba:** không có. Phần mềm chỉ được xây từ mã nguồn của chính nó (công khai
  tại trang dự án) và các framework của hệ điều hành.
- **Chợ ứng dụng:** ứng dụng được phân phối qua Apple App Store và Google Play. Apple và
  Google có thể thu thập thống kê cài đặt/sử dụng theo chính sách quyền riêng tư của
  riêng họ; việc thu thập đó nằm ngoài tầm kiểm soát của chúng tôi và chúng tôi chỉ nhận
  được những thống kê tổng hợp, ẩn danh mà các nền tảng đó hiển thị cho mọi nhà phát
  triển.
- **Tailscale hoặc các VPN khác:** nếu bạn chọn kết nối qua VPN, lưu lượng của bạn được
  xử lý theo chính sách quyền riêng tư của nhà cung cấp đó. Deskhub không yêu cầu cũng
  không đóng gói kèm VPN nào.

## 6. Bảo mật

- Lưu lượng truyền hình ảnh nằm trong chính mạng của bạn hoặc trong đường hầm VPN của
  bạn. Khi bạn dùng VPN như Tailscale, lưu lượng giữa các thiết bị được VPN đó mã hoá
  đầu-cuối (WireGuard).
- Deskhub mã hoá lưu lượng phiên — video, điều khiển, thao tác, clipboard và terminal
  đều chạy trên QUIC/TLS giữa các thiết bị của bạn. Quyền vào được quyết bằng một cuộc
  bắt tay ghép đôi: máy lạ phải chứng minh nó biết mã 4 chữ số tuỳ chọn của host (bản
  thân mã không bao giờ được truyền đi) hoặc được người ngồi tại host phê duyệt. Tên
  thiết bị được mã hoá trên đường truyền nhưng hiển thị trên host, nên đừng đặt vào đó
  bất kỳ thông tin nhạy cảm nào. Đừng bao giờ phơi Deskhub trực tiếp ra Internet. Mô
  hình mối đe doạ đầy đủ — cái gì được bảo vệ, cái gì không, và cách báo lỗ hổng — nằm
  trong
  [`SECURITY.vi.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.vi.md).
- Các mã passcode lưu trong `recent-devices.txt` và `ui-settings.txt` được che đi bằng
  một khoá cố định để không đọc được ngay khi nhìn vào. Đó không phải mã hoá và không
  nhằm chống lại người vốn đã truy cập được tài khoản người dùng của bạn.
- Vì chúng tôi không giữ dữ liệu nào về bạn, nên không có cơ sở dữ liệu nào phía nhà phát
  triển để mà bị rò rỉ.

## 7. Lưu trữ và xoá dữ liệu

Chúng tôi không giữ gì cả, nên chúng tôi không có gì để xoá. Toàn bộ dữ liệu phiên biến
mất khi phiên kết thúc. Địa chỉ lưu trong app được xoá bằng cách xoá trắng ô nhập hoặc gỡ
cài đặt app. Danh sách thiết bị gần đây và các cài đặt đã lưu — bao gồm mọi passcode —
được xoá bằng cách xoá thư mục riêng của app (`%USERPROFILE%\.deskhub` trên Windows,
`~/.deskhub` trên macOS và Linux), app sẽ tạo lại thư mục rỗng ở lần khởi động kế tiếp;
trên iOS và Android, gỡ cài đặt app là xoá hết.

Tệp mà thiết bị khác gửi cho bạn là của bạn, không phải của app. Khi đã được giao, chúng
nằm trong thư mục máy tính đó đã chọn, hoặc trong thư viện ảnh, Documents hay Downloads
trên điện thoại và máy tính bảng, và gỡ cài đặt Deskhub không đụng tới chúng; muốn xoá thì
xoá ở đó.

## 8. Quyền của bạn (GDPR, CCPA và các luật tương tự)

Các đạo luật như Quy định bảo vệ dữ liệu chung của EU (GDPR) và Đạo luật quyền riêng tư
người tiêu dùng California (CCPA) trao cho bạn các quyền đối với dữ liệu cá nhân — truy
cập, chỉnh sửa, xoá, di chuyển, phản đối, và không bị phân biệt đối xử.

Vì Deskhub không thu thập hay nắm giữ bất kỳ dữ liệu cá nhân nào, nên không có dữ liệu
nào để thực thi các quyền đó. Nếu bạn cho rằng chúng tôi có giữ dữ liệu về bạn, hãy liên
hệ theo địa chỉ bên dưới và chúng tôi sẽ phản hồi trong vòng 30 ngày.

Chúng tôi không "bán" hay "chia sẻ" thông tin cá nhân theo định nghĩa của CCPA.

## 9. Quyền riêng tư của trẻ em

Phần mềm không hướng tới trẻ em và, như đã nêu ở trên, không thu thập dữ liệu từ bất kỳ
ai, kể cả trẻ em dưới 13 tuổi (COPPA) hay dưới 16 tuổi (GDPR).

## 10. Truyền dữ liệu xuyên biên giới

Không có. Dữ liệu của bạn không bao giờ rời khỏi các thiết bị và mạng của chính bạn thông
qua Phần mềm.

## 11. Thay đổi chính sách này

Nếu cách xử lý dữ liệu của Phần mềm có thay đổi (ví dụ một phiên bản sau này thêm tính
năng báo cáo sự cố tuỳ chọn), chính sách này sẽ được cập nhật **trước khi** thay đổi đó
được phát hành, với ngày hiệu lực mới và một dòng trong bảng lịch sử bên dưới. Phiên bản
hiện hành luôn được công bố tại:
https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md
(bản tiếng Việt: https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.vi.md)

| Phiên bản | Ngày | Thay đổi |
|---|---|---|
| 2.4 | 2026-08-28 | **Điện thoại và máy tính bảng nay nhận tệp chứ không chỉ gửi**, và chỗ tệp rơi xuống trên chúng là điều mới. Trên iOS, ảnh và video được thêm vào thư viện ảnh của bạn, việc này xin quyền Photos chỉ-thêm của hệ thống ở lần đầu — Deskhub chỉ có thể thêm, không bao giờ đọc hay sửa thứ đã có — còn mọi tệp khác vào thư mục Documents của app, nơi app Files nhìn thấy. Trên Android, ảnh vào `Pictures/Deskhub`, video vào `Movies/Deskhub`, mọi thứ còn lại vào `Download/Deskhub`, tất cả đều qua kho media của hệ thống. Cả hai đều hiện thông báo nêu tên thứ vừa tới. Không thứ nào trong đó tới chỗ chúng tôi. Bản này cũng đính chính hai điều các phiên bản trước của chính sách này nói sai: Android xưa nay vẫn cần quyền mà hệ thống ghi nhãn là *Micro* (`RECORD_AUDIO`) để thu thứ chính thiết bị đang phát, tức đúng thứ mà bản 2.1 mô tả là chia sẻ tiếng — Deskhub vẫn không bao giờ thu micro — và các app desktop chưa bao giờ mất ô tick *File transfer* mà bản 2.3 nói tới, thứ mất đi chỉ là cài đặt được lưu đằng sau nó. |
| 2.3 | 2026-08-24 | **Việc nhận tệp thôi không còn là một cài đặt được lưu.** Tuỳ chọn *Nhận tệp người xem gửi* đã bị bỏ khỏi `ui-settings.txt`: điện thoại hay máy tính bảng nhận tệp bất cứ khi nào app đang hiện trên màn hình, còn máy tính đưa *File transfer* vào danh sách những thứ nó chia sẻ, mặc định được tick mỗi lần và không được ghi nhớ, nên nó vẫn chỉ nhận tệp trong lúc đang chia sẻ. Điều xảy ra với tệp đến thì không đổi: vẫn cần người gửi đã ghép đôi và được cho vào, vẫn rơi vào nơi máy đó để tệp nhận được, vẫn không bao giờ ghi đè tệp đã có ở đó, và vẫn được ghi vào nhật ký cục bộ kèm tên, địa chỉ và vân tay khoá của thiết bị gửi. Chia sẻ màn hình vẫn là hành động có chủ đích sau nút riêng của nó, và một máy đang chia sẻ màn hình vẫn tiếp tục nhận tệp thay vì tắt việc đó trong suốt thời gian chia sẻ. |
| 2.2 | 2026-08-21 | Deskhub nay **gửi tệp** được giữa các thiết bị của chính bạn. Một máy đang chia sẻ màn hình có thể nhận tệp, và bất kỳ thiết bị nào đang kết nối tới nó đều có thể chọn tệp để gửi; trên Android và iOS tệp đến từ bộ chọn ảnh của hệ thống hoặc trình duyệt tệp của hệ thống, và một bản sao được chuẩn bị trong cache riêng của app trong lúc gửi rồi bị xoá. Tệp đi thẳng giữa hai thiết bị của bạn qua đúng kênh mã hoá chở hình ảnh, và không bao giờ được gửi cho chúng tôi hay qua bất kỳ máy chủ nào của chúng tôi. Máy nhận ghi tệp vào thư mục nó chọn — `Deskhub` trong thư mục nhà của người dùng đó nếu không chọn thư mục khác, được lưu cùng các tuỳ chọn khác trong `ui-settings.txt` — không bao giờ ghi đè tệp đã có ở đó, và ghi mỗi lời chào, phán quyết cùng kết quả, kèm tên, địa chỉ và vân tay khoá của thiết bị gửi, vào nhật ký phiên cục bộ của nó. Việc nhận tệp mặc định tắt cho tới khi người chia sẻ tích chọn, và điện thoại hay máy tính bảng chỉ gửi tệp: chúng không bao giờ nhận. |
| 2.1 | 2026-08-19 | Chia sẻ màn hình nay chia sẻ được cả **tiếng** của máy đó. Thứ được thu là bản trộn mà loa của chính máy đang phát — không bao giờ là micro; Deskhub không có âm thanh hai chiều và không xin quyền micro. Âm thanh được nén, gửi thẳng tới người đang xem qua đúng kênh mã hoá chở hình ảnh, và không bao giờ được lưu. Nó chỉ đi khi máy chia sẻ bật *Chia sẻ tiếng của máy này* **và** viewer bật *Phát tiếng của máy đang xem*; tắt một trong hai là hết. Cả hai công tắc được lưu cùng các tuỳ chọn khác trong `ui-settings.txt` và mặc định đều bật. |
| 2.0 | 2026-08-15 | Phiên làm việc nay chạy trên kênh truyền mã hoá (QUIC/TLS) — video, thao tác, clipboard lẫn terminal — và máy được cho vào bằng cơ chế ghép đôi. Dữ liệu mới lưu trên chính thiết bị của bạn, tất cả trong thư mục của app và không bao giờ gửi cho chúng tôi: cặp khoá là danh tính của máy này (`host_key.pem`, `host_cert.pem`), khoá của các host bạn đã tin (`known_hosts`), các máy được phép vào host này (`paired_devices` — dấu vân tay khoá, tên mỗi máy gửi, mốc thời gian), và một salt không bí mật (`auth_salt`). Passcode nay là tuỳ chọn và không bao giờ được truyền đi: cuộc bắt tay ghép đôi chứng minh mã mà không gửi mã. |
| 1.9 | 2026-08-14 | Trên Linux, lựa chọn màn hình trong hộp thoại chia sẻ của desktop nay được ghi nhớ: token cấp quyền mà desktop phát ra được lưu vào `portal-restore-token.txt` để các lần chia sẻ sau bỏ qua hộp thoại. Token chỉ hoạt động với phiên desktop của chính bạn trên máy này, không bao giờ được truyền đi, được thay mới sau mỗi lần chia sẻ, và bị xoá khi bạn bấm *Choose screens again* hoặc xoá tệp. |
| 1.8 | 2026-08-14 | Công tắc *giữ máy thức* mới (mặc định bật): trong lúc bạn đang chia sẻ hoặc đang xem, app yêu cầu hệ điều hành không đưa máy vào giấc ngủ và không tắt màn hình, và nhả yêu cầu đó khi phiên kết thúc. Chỉ lựa chọn bật/tắt được lưu, trong cùng tệp cài đặt cục bộ; không có gì về nó được truyền đi, và không cài đặt ngủ nào của hệ thống bị thay đổi. |
| 1.7 | 2026-08-13 | Đồng bộ clipboard nay hoạt động cả trên Android và iOS, với cùng công tắc, giới hạn 32 KiB và quy tắc chỉ-văn-bản-thuần như trên desktop. Hệ điều hành có giới hạn: thiết bị Android chỉ đọc được clipboard của chính nó khi Deskhub là ứng dụng đang ở nền trước (văn bản gửi tới thì được áp dụng bất cứ lúc nào); iOS có thể hiện hộp thoại dán của hệ thống khi Deskhub đọc một bản copy mới; thiết bị iOS đang làm host không bao giờ đồng bộ clipboard, vì broadcast của nó chạy trong một tiến trình riêng. Điện thoại và máy tính bảng cũng có thêm lựa chọn địa chỉ mạng để chia sẻ như trên desktop, lưu trong cùng tệp cài đặt cục bộ và không bao giờ được gửi đi đâu. Ngoài lựa chọn đó, không có gì mới được lưu trên bất kỳ thiết bị nào. |
| 1.6 | 2026-08-13 | Các app desktop có thêm đồng bộ clipboard tuỳ chọn: khi công tắc bật, văn bản thuần bạn copy trong một phiên được gửi giữa các thiết bị của bạn (không mã hoá, như phần còn lại của lưu lượng, giới hạn 32 KiB) và đặt vào clipboard của máy kia; Deskhub không bao giờ lưu nó. Các cài đặt mới lưu cục bộ: địa chỉ mạng để chia sẻ, khởi động cùng hệ điều hành, tự chia sẻ khi mở app, chế độ chạy nền/khay, và chính công tắc clipboard. Bật khởi động cùng hệ điều hành sẽ tạo mục khởi động của chính nền tảng đó (tệp autostart trên Linux, scheduled task tên *Deskhub* trên Windows, Login Item trên macOS); tắt đi sẽ gỡ bỏ nó. |
| 1.5 | 2026-08-13 | Mỗi client nay có thể đặt một tên thiết bị (ô *Your name* trên trang kết nối). Tên được lưu trong tệp cài đặt `ui-settings.txt` sẵn có trên chính thiết bị của bạn và được gửi tới host khi bạn kết nối — không mã hoá, như phần còn lại của lưu lượng — để host gắn nhãn người xem này trong bảng phiên, dòng trạng thái và nhật ký của nó. Host chỉ giữ tên trong bộ nhớ, chỉ trong lúc bạn còn kết nối, và không bao giờ lưu lại. Ô này được điền sẵn tên của chính máy hoặc thiết bị bạn, nên tên mặc định đó được gửi đi trừ khi bạn thay nó bằng một tên do bạn tự chọn. |
| 1.4 | 2026-08-12 | Tệp trạng thái trên iOS mà broadcast extension chia sẻ với app nay ghi thêm mức bộ nhớ tính bằng megabyte mà chính extension đang dùng, để màn hình chia sẻ hiển thị được con số đó. Giá trị này chỉ mô tả tiến trình broadcast của Deskhub, nằm nguyên trong app group container trên thiết bị của bạn, và bị xoá cùng phần còn lại của tệp trạng thái khi buổi phát kết thúc. |
| 1.3 | 2026-08-12 | Thiết bị Android và iOS nay chia sẻ được màn hình của chính nó ở chế độ chỉ xem, nên màn hình điện thoại hay máy tính bảng có thể phát sang một thiết bị khác của bạn. Thay đổi này thêm các quyền thu màn hình mà mỗi hệ điều hành yêu cầu (kèm foreground service và thông báo của nó trên Android) và, trên iOS, một app group container dùng chung giữa app và broadcast extension cho passcode và cổng, kèm một tệp trạng thái ngắn hạn mà extension ghi vào đó để app biết buổi phát có đang chạy hay không. Hình ảnh vẫn chỉ đi giữa các thiết bị của bạn và không bao giờ được lưu lại. |
| 1.2 | 2026-08-07 | Passcode nay là bắt buộc trên mọi host, được sinh ra ở lần chạy đầu thay vì để trống, và mọi client đều nhập được. Cài đặt chia sẻ nay được lưu trên macOS và Linux chứ không chỉ Windows, còn danh sách thiết bị gần đây được lưu trên mọi nền tảng, đều nằm trong thư mục cục bộ của chính app. Không có dữ liệu mới nào rời khỏi thiết bị của bạn. |
| 1.1 | 2026-08-05 | App Windows nay lưu dữ liệu giữa các lần chạy: danh sách 10 địa chỉ gần nhất bạn đã kết nối, cài đặt chia sẻ của bạn, và các passcode dùng với chúng. Tất cả nằm trong `%USERPROFILE%\.deskhub` trên chính máy bạn; không có gì được truyền đi đâu cả. Ghi nhận thêm chế độ chia sẻ chỉ xem và giới hạn 5 người xem. |
| 1.0 | 2026-07-24 | Công bố lần đầu. |

## 12. Liên hệ

Mọi thắc mắc về chính sách này hoặc về quyền riêng tư trong Deskhub:

- **Email:** manhpv151090@gmail.com
- **Issues:** https://github.com/manhpham90vn/Deskhub/issues
