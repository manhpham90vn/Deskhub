[English](SECURITY.md) · **Tiếng Việt** · [中文](SECURITY.zh.md) · [日本語](SECURITY.ja.md)

# Chính sách bảo mật của Deskhub

_Cập nhật lần cuối: 27 tháng 9 năm 2026_

Đây là bản dịch của [`SECURITY.md`](SECURITY.md). Nếu hai bản có khác biệt, bản tiếng Anh
là bản chuẩn.

## ⚠️ Cần đọc trước

**Hãy dùng Deskhub trên network tin cậy hoặc qua VPN. Không port-forward UDP 47777 và
không đưa máy đang share trực tiếp ra Internet.**

Session chạy trên QUIC/TLS, gồm video, phím gõ, mouse, clipboard và lưu lượng terminal.
Trước khi máy lạ được connect, nó phải chứng minh mình biết passcode của host mà không gửi
chính mã đó, hoặc được người dùng tại host chấp thuận. Probe và beacon discovery không
được encrypt nhưng không mang nội dung session. Những packet khác nằm ngoài connection
đã encrypt sẽ bị loại bỏ.

Mọi host cũng có thể share ở chế độ **view-only** (input bị loại bỏ thay vì được inject),
và có thể tắt hoàn toàn việc pair máy mới để chỉ chấp nhận những máy đã pair.

Encrypt không loại bỏ mọi rủi ro trên network. Port vẫn trả lời probe discovery,
passcode 4 chữ số vẫn ngắn, lần connect đầu tiên chưa có danh tính đã biết để đối chiếu,
và app không có cơ chế chống flooding.

Để truy cập từ xa, hãy dùng VPN. Dự án đã kiểm thử với
[Tailscale](https://tailscale.com); bạn có thể connect tới địa chỉ `100.x.y.z` của host.
Hãy xem các giới hạn bên dưới trước khi share màn hình hoặc terminal.

## Threat model

### Những gì Deskhub bảo vệ

| | |
|---|---|
| Dữ liệu tới tay người phát triển | Không có dữ liệu nào. Không server, không tài khoản, không telemetry, không SDK bên thứ ba. Xem [`PRIVACY.vi.md`](PRIVACY.vi.md). |
| Việc đọc trộm lưu lượng | Mọi session đều chạy bên trong QUIC/TLS: frame video, phím gõ, văn bản clipboard và byte terminal đều được encrypt giữa hai máy. Việc bắt gói chỉ cho biết khối lượng và thời điểm, không cho biết nội dung. Packet chưa encrypt tới port đều bị loại bỏ, trừ các probe discovery. |
| Viewer từ xa tranh quyền điều khiển | Host được ưu tiên: ngay khi bạn thao tác với mouse hoặc keyboard thật, remote input bị tạm dừng. Điều này áp dụng cho host trên Windows, macOS và Linux. |
| Phím bị kẹt | Mọi phím mà phía từ xa đang giữ đều được nhả tự động khi session kết thúc hoặc viewer chuyển sang cửa sổ khác. |
| Người lạ kết nối không được phép | Việc chấp nhận kết nối dựa trên pairing handshake. Một máy chưa biết phải chứng minh passcode của host qua SPAKE2 — mã không đi qua đường truyền, kẻ nghe lén không thu được dữ liệu nào để crack, và mỗi connection chỉ được thử một lần — hoặc, khi không đặt passcode, phải chờ người dùng tại host trả lời *Let this machine in?*. Ba lần thử sai sẽ khoá pairing trong 30 giây, và mỗi lần khoá liên tiếp sau đó dài gấp đôi, tối đa một giờ. Sau khi được chấp nhận, máy ở trạng thái đã pair: được nhận diện qua key mật mã, xuất hiện trên trang Devices của host, và có thể thu hồi tại đó; forget máy đó cũng đóng mọi connection nó đang mở. Việc được chấp nhận chỉ kéo dài bằng đúng connection đã giành được nó. Discovery beacon không còn xác nhận một mã đoán đúng hay sai: probe của máy lạ luôn nhận về danh sách rỗng, nên cơ chế dò mã trước đây không còn. |
| Tấn công xen giữa ở các lần kết nối sau | Mỗi máy có một key. Client lưu key của từng host đã pair và từ chối kết nối lại khi key thay đổi, cho tới khi người dùng chấp nhận một cách tường minh. Phần chứng minh passcode được ràng buộc với đúng host key mà client nhận được, nên một bản chứng minh bị relay sẽ không hợp lệ. |
| Nhiều viewer tranh quyền điều khiển mouse | Tối đa 5 viewer cùng xem một host, nhưng chỉ một viewer điều khiển input: viewer tham gia sớm hơn được ưu tiên, và input của viewer tới sau bị loại bỏ cho tới khi viewer trước không thao tác trong một giây. Viewer thứ 6 bị từ chối với trạng thái `Busy`. |
| Viewer chỉ được phép xem | Chế độ share view-only, có trên mọi host, loại bỏ các packet input ngay tại host trước khi bất cứ thao tác nào được inject; cơ chế này không dựa vào việc client tự tuân thủ. Host trên Android và iOS luôn ở chế độ view-only. |
| Điện thoại bị bỏ quên trong trạng thái đang share | Cơ chế bảo vệ cuối cùng thuộc về hệ điều hành chứ không phải Deskhub: Android hiển thị một notification thường trực và yêu cầu đồng ý ghi màn hình ở từng phiên share, còn iOS giữ chỉ báo broadcast luôn hiển thị. Cả hai đều cho phép dừng share mà không cần mở app. |
| Máy đã pair ghi file vào máy của bạn | Chỉ máy đã được chấp nhận mới gửi được file, và chỉ khi máy nhận đang bật file transfer. Dữ liệu tới không thể thoát khỏi thư mục mà máy đó chọn: tên file trên đường truyền bị cắt còn phần cuối của đường dẫn và loại bỏ dấu phân cách, byte điều khiển, ký tự filesystem không chấp nhận cùng các tên thiết bị dành riêng, trước khi bất kỳ file nào được mở. Mỗi file được ghi dưới tên có hậu tố `.deskhub-part` và chỉ được đổi tên khi đã nhận đủ với CRC-32 khớp. Tên đã tồn tại sẽ được thêm số thứ tự thay vì ghi đè. Một batch bị giới hạn ở 32 file, 8 GiB mỗi file và 32 GiB tổng cộng. Cùng cơ chế xử lý tên này cũng chạy trên điện thoại và tablet trước khi dữ liệu tới thư viện ảnh hoặc thư mục Downloads. |
| Packet không hợp lệ | Mọi trường đều được kiểm tra giới hạn trước khi đọc. Các parser có unit test, chạy dưới AddressSanitizer, UndefinedBehaviorSanitizer và ThreadSanitizer trong CI, và được fuzz mỗi đêm bằng libFuzzer với bảy target bao phủ wire format, phần parse H.264, reassembly packet, byte stream terminal, chuỗi UI, cùng session state machine phía host và phía viewer. Các crash phát hiện qua fuzzing được lưu trong repo dưới dạng regression test, và phần coverage mới được bổ sung vào seed corpus. |

### Những gì Deskhub **không** bảo vệ

Đây là danh sách đầy đủ. Không mục nào dưới đây đã được giải quyết:

- **Shell được giữ lại thuộc về cặp đã pair, không thuộc riêng máy đã mở nó.** Một
  shell còn lại trên host sống lâu hơn kết nối đã mở nó, không có giới hạn thời gian,
  và mọi máy đã được nhận vào đều có thể liệt kê các shell host đang giữ, reattach một
  shell đã detach, và đóng bất kỳ shell nào. Id, kích thước và tên thiết bị của từng
  shell nằm trong danh sách đó. Vì vậy một máy thứ hai bạn pair —— hoặc một máy mà bạn
  chưa thu hồi key trên trang Devices —— có thể đọc lại những gì shell trước đó đang
  làm và tiếp tục trong đó. Hãy thu hồi thiết bị bạn không còn tin tưởng, và đóng các
  shell đã dùng xong thay vì để lại.
- **Lần kết nối đầu tiên dựa trên tin cậy chưa được xác minh.** Pairing ngăn được kẻ xen
  giữa xuất hiện ở *các lần sau*: key đã được ghim và mọi thay đổi đều bị từ chối kèm cảnh
  báo rõ ràng. Nó không ngăn được kẻ đã xen giữa ngay từ lần tiếp xúc đầu tiên: khi không
  đặt passcode, máy mà client kết nối tới sẽ được pair; còn passcode chỉ nâng mức bảo vệ
  tương ứng với độ mạnh của một bí mật 4 chữ số. Nếu điều này quan trọng, hãy đối chiếu
  fingerprint qua một kênh khác.
- **Phân tích lưu lượng vẫn khả thi.** Việc encrypt che giấu nội dung chứ không che giấu
  sự tồn tại: người quan sát biết được có một session đang chạy, lượng video đang truyền,
  và thời điểm bạn gõ phím.
- **Không có rate limiting và không chống DoS.** Việc gửi lượng lớn dữ liệu tới port sẽ
  làm gián đoạn session; với host không đặt passcode, kẻ tấn công còn có thể khiến prompt
  phê duyệt hiển thị liên tục.
- **Discovery beacon vẫn trả lời mọi nguồn.** Một probe `LIST_SOURCES` hoặc `PING` từ bất
  kỳ địa chỉ nguồn nào đều nhận được phản hồi. Phản hồi cho máy lạ là danh sách rỗng, và
  không probe nào xác nhận được passcode, nhưng máy vẫn bị phát hiện qua việc quét và port
  vẫn có thể bị dùng làm một bộ phản xạ UDP nhỏ. Có một ngoại lệ: địa chỉ nguồn đang giữ
  một connection đã encrypt sẽ không bao giờ nhận phản hồi ở dạng không encrypt. Sau khi
  một máy đã chứng minh danh tính, mọi dữ liệu từ máy đó phải tới ở dạng encrypt, nên một
  `SOURCE_LIST` hay `PONG` giả mạo ở dạng không encrypt không thể mạo danh một peer đang
  kết nối.
- **Tên thiết bị được hiển thị và ghi log.** Giá trị *Your name* mà viewer gửi hiện đã
  được encrypt trên đường truyền, nhưng vẫn hiển thị trên màn hình của host, được ghi vào
  log của host và lưu trong danh sách paired-devices của host. Giá trị mặc định là hostname
  của máy, thường trùng với tên thật của người dùng. Nên dùng một biệt danh và không đặt
  thông tin nhạy cảm vào trường này. Việc xoá trắng trường này không ngăn tên được gửi đi,
  mà chỉ khôi phục giá trị mặc định.
- **Vị trí viewer tự giải phóng sau 5 giây không có dữ liệu.** Nếu viewer của bạn
  mất kết nối, vị trí đó được mở lại và `Hello` tới tiếp theo sẽ chiếm chỗ, miễn là máy gửi
  đã qua admission (pairing, passcode hoặc approval).
- **Việc share phơi ra toàn bộ display.** Không phải một cửa sổ, mà là mọi notification,
  popup và cửa sổ trên màn hình đó. Xem [`PRIVACY.vi.md` §3.4](PRIVACY.vi.md).
- **Host là điện thoại hoặc tablet phơi ra toàn bộ thiết bị.** Android và iOS cũng host
  được, và nội dung chúng stream là toàn bộ màn hình: app ngân hàng, mã một lần, tin nhắn,
  mọi mật khẩu bạn nhập trong khi share. Stream được encrypt như mọi session khác, nhưng
  mọi viewer được chấp nhận đều nhìn thấy toàn bộ. Host di động luôn ở chế độ view-only,
  điều này loại bỏ rủi ro bị điều khiển từ xa nhưng không giảm rủi ro lộ thông tin.

## Môi trường sử dụng Deskhub

✅ **Môi trường nên dùng**

- Mạng LAN gia đình hoặc cá nhân, nơi bạn kiểm soát mọi thiết bị.
- Một tailnet Tailscale (hoặc một đường hầm WireGuard/VPN khác) chỉ gồm thiết bị của bạn.
  VPN bổ sung một lớp encrypt và ngăn người lạ tiếp cận port.
- Một máy chỉ đóng vai *client* (điện thoại, tablet, laptop không share màn hình). Client
  không nhận session đi vào.

❌ **Môi trường nên tránh**

- Port-forward UDP 47777 qua router, hoặc đặt máy đang share vào DMZ.
- Share màn hình trên Wi-Fi của quán cà phê, khách sạn, sân bay, trường học, không gian
  làm việc chung hoặc hội nghị.
- Share trên mạng LAN văn phòng hoặc nhà ở chung, nơi bạn không kiểm soát các thiết bị
  khác.
- Bất kỳ network nào có thiết bị khách, thiết bị IoT không do bạn cấu hình, hoặc máy của
  người khác mà bạn không quản trị.
- Phơi port qua giao diện công khai của một cloud VM hoặc một dịch vụ tunnel công cộng.

Theo mặc định, socket bind vào mọi interface (`INADDR_ANY`), nên nó có thể truy cập được
từ mọi network mà máy đang kết nối, kể cả network bạn không còn để ý tới. Setting **Share
on network** thu hẹp phạm vi này: chọn một địa chỉ của máy thì host chỉ bind đúng
interface đó, và các máy trên network khác không tiếp cận được port. Có hai điểm cần lưu
ý. Thứ nhất, nếu địa chỉ đã chọn không còn tồn tại khi bắt đầu share (rút cáp, DHCP cấp
địa chỉ mới), Deskhub chuyển sang mọi interface và nêu rõ trong status khi share; cần theo
dõi banner nếu bạn phụ thuộc vào setting này. Thứ hai, việc bind một interface cũng chặn
các viewer qua loopback (`127.0.0.1`) trên cùng máy. Trên Windows, app chạy ở quyền cao
ngay từ khi khởi động — nó xin quyền một lần để inject được input vào các cửa sổ quyền cao
— và tự mở rule firewall khi bạn share. Rule này áp dụng cho toàn bộ app trên mọi profile,
nên việc thu hẹp bind không làm thu hẹp firewall; đây cũng là lý do nguyên tắc ở phần trên
có ý nghĩa.

## Khả năng của kẻ tấn công trong cùng network

Nếu một người ở cùng LAN với máy đang share màn hình và Deskhub đang chạy, họ có thể:

1. Phát hiện máy đó bằng cách quét UDP 47777. Probe của một máy chưa pair nhận về danh
   sách rỗng, nhưng máy vẫn phản hồi nên vẫn bị phát hiện.
2. Thử kết nối. Họ không đọc được passcode trên đường truyền vì mã không đi qua đó. Các
   phương án còn lại là thử trực tiếp (một lần cho mỗi connection, ba lần sai thì khoá
   pairing 30 giây, mỗi lần khoá tiếp theo dài gấp đôi tới tối đa một giờ, nên thử hết
   10.000 mã mất nhiều tháng), hoặc với host không đặt passcode, chờ người dùng tại host
   nhấn **Allow** trên prompt phê duyệt.
3. Quan sát lưu lượng mà không kết nối, và chỉ thu được thông tin về khối lượng và thời
   điểm. Nội dung của session, bao gồm video, đều được encrypt; việc bắt gói không dựng
   lại được màn hình hay các phím đã gõ.
4. Gửi lượng lớn dữ liệu tới port và làm gián đoạn session. Không có cơ chế rate limiting
   nào áp dụng cho một kẻ tấn công tiếp cận được máy.

Cơ chế ưu tiên host hạn chế được các thao tác ngoài ý muốn khi bạn đang ngồi tại máy.
Nó không có tác dụng khi bạn rời khỏi máy, và đó mới là thời điểm cần bảo vệ.

## Danh sách kiểm tra khi cứng hoá

Nếu bạn tiếp tục sử dụng Deskhub ở trạng thái hiện tại, nên thực hiện các mục sau:

- [ ] Chạy Tailscale trên cả hai máy và chỉ connect qua địa chỉ `100.x.y.z`.
- [ ] Xác nhận router **không** có port-forward hoặc mapping UPnP cho UDP 47777.
- [ ] Xác định cách máy khác được chấp nhận: đặt một passcode 4 chữ số trong Settings,
      hoặc để trống và tự trả lời prompt phê duyệt. Rà soát trang Devices định kỳ và gỡ
      những máy không còn nhận ra. Bỏ chọn *Viewers can control this machine* khi chỉ cần
      cho người khác xem.
- [ ] Thoát Deskhub khi không sử dụng. App không chạy như một background service, nên
      đóng app là đóng luôn điểm truy cập.
- [ ] Trên Linux, nếu dùng `ufw`, hãy thu hẹp rule thay vì mở rộng:
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` thay cho
      `sudo ufw allow 47777/udp`.
- [ ] Không để một phiên share đang chạy trên laptop mà bạn mang sang các network khác.
- [ ] Khoá máy khi rời đi, để một session không người trông coi không bị chiếm quyền.
- [ ] Với `deskhub-cli`, không đặt passcode trực tiếp trong câu lệnh. `--passcode 0417`
      hiển thị với mọi process trên máy qua `ps` và `/proc/*/cmdline`, đồng thời được lưu
      vào lịch sử shell. Hãy dùng `--passcode -` để đọc từ standard input,
      `--passcode @FILE` để đọc từ một file chỉ bạn đọc được, hoặc đặt biến môi trường
      `DESKHUB_PASSCODE`.

## Dữ liệu lưu trên máy

Log chẩn đoán được ghi ở dạng văn bản thuần trong `~/.deskhub/`
(`%USERPROFILE%\.deskhub` trên Windows) trên Windows, macOS và Linux. Log chứa thống kê
kết nối và địa chỉ peer, không chứa nội dung màn hình hay phím gõ.

App desktop và `deskhub-cli` dùng chung các file này. Thư mục đó còn chứa:
`ui-settings.txt` (fps, bitrate, giới hạn độ phân giải, port, các switch view-only và
pairing, passcode của host nếu có đặt, và tên thiết bị hiển thị cho host),
`recent-devices.txt` (10 địa chỉ kết nối gần nhất, thời điểm, và passcode dùng cho từng
địa chỉ), `host_key.pem` và `host_cert.pem` (private key và certificate tự ký của máy này,
tức danh tính đứng sau fingerprint của nó; người có được file key có thể mạo danh máy
này), `known_hosts` (key của các host mà máy này đã trust), `paired_devices` (key, tên và
mốc thời gian của những máy được host này chấp nhận), `auth_salt` (một salt không bí mật
cho verifier của passcode) và, trên Linux, `portal-restore-token.txt` (token của chính
desktop cho những màn hình bạn đã chọn, chỉ có ý nghĩa với phiên desktop của bạn và không
được truyền đi). App di động lưu settings trong sandbox riêng; trên iOS là trong app group
container. Passcode đã lưu được làm rối bằng một khoá XOR cố định, giúp chúng không hiển
thị trực tiếp khi mở file. **Đây không phải là encrypt**: người có source và file sẽ khôi
phục được chúng trong vài giây. Hãy coi thư mục đó là nội dung mà mọi tiến trình chạy dưới
tài khoản của bạn đều đọc được.

File do máy khác gửi tới được lưu ngoài thư mục đó, trong thư mục mà máy nhận đã chọn
(`Deskhub` trong thư mục home của người dùng nếu không chọn khác, lưu dưới tên
`transfer_dir`). Trên điện thoại hoặc tablet, các file này nằm trong thư viện ảnh hoặc thư
mục Documents / Downloads của thiết bị, và vẫn tồn tại sau khi gỡ app. Hãy coi mọi nội
dung được gửi tới đó là file do một máy đã pair đặt lên thiết bị của bạn.

Không dữ liệu nào trong số này được upload; bạn có thể xoá thư mục bất cứ lúc nào.

## Các biện pháp giảm nhẹ đã lên kế hoạch

Đang theo dõi, theo thứ tự dự kiến triển khai:

1. **Lưu passcode và host key trong keychain của hệ điều hành** thay vì trong file.
2. **Không để discovery beacon phản hồi** các probe không được yêu cầu, thay vì trả về
   danh sách rỗng.

Đã hoàn thành kể từ lần cập nhật danh sách gần nhất: một transport đã encrypt (QUIC/TLS)
cho toàn bộ session, bao gồm video, input, clipboard và terminal, cùng việc loại bỏ dữ
liệu chưa encrypt trừ các probe discovery; pairing bằng SPAKE2 để passcode không đi qua
đường truyền và không thể bị thu thập hay brute-force offline; prompt phê duyệt tại host;
danh sách máy đã pair kèm khả năng thu hồi; key cho từng máy cùng cảnh báo khi key thay
đổi ở phía client; và cơ chế khoá sau 3 lần nhập sai passcode, bắt đầu từ 30 giây và
tăng gấp đôi tới tối đa một giờ.

Danh sách này là tuyên bố về định hướng, không phải lịch trình. Deskhub do một người bảo
trì trong thời gian rảnh. Hãy đánh giá theo hiện trạng, không theo kế hoạch.

## Báo cáo lỗ hổng

Vui lòng báo cáo các vấn đề bảo mật **một cách riêng tư**, không mở issue công khai trên
GitHub.

- **Email:** manhpv151090@gmail.com — ghi `[Deskhub security]` trong tiêu đề.
- **Hoặc:** mở một [security advisory riêng tư](https://github.com/manhpham90vn/Deskhub/security/advisories/new)
  trên GitHub.

Vui lòng nêu rõ môi trường đang chạy (OS, phiên bản Deskhub trên thanh tiêu đề hoặc trong
[`VERSION`](VERSION)), các thao tác đã thực hiện, và kết quả quan sát được. Một bản proof
of concept sẽ hữu ích.

**Quy trình xử lý:** xác nhận trong vòng 7 ngày và đánh giá trong vòng 30 ngày. Đây là dự
án được một lập trình viên duy trì trong thời gian rảnh, nên mong bạn thông cảm về mốc
thời gian; trong mọi trường hợp bạn sẽ nhận được câu trả lời rõ ràng. Nếu bản vá được phát
hành, bạn sẽ được ghi nhận trong release notes, trừ khi bạn không muốn.

Dự án không có chương trình bug bounty và không chi trả phần thưởng.

Các giới hạn phía trên — tin cậy ở lần connect đầu, phân tích lưu lượng, phản hồi
discovery và việc không chống DoS — đã được ghi nhận. Vui lòng báo cáo nếu bạn có bằng
chứng mới về tác động của chúng, hoặc phát hiện vấn đề khác như memory corruption, crash
do packet không hợp lệ, dữ liệu rời khỏi thiết bị ngoài dự kiến, hay lỗi trong biện pháp
giảm nhẹ đã phát hành.

## Các phiên bản được hỗ trợ

Chỉ bản release mới nhất trên
[trang Releases](https://github.com/manhpham90vn/Deskhub/releases) được hỗ trợ. Bản vá
được phát hành kèm một release mới; không có backport cho các phiên bản cũ.

## Phạm vi

Chính sách này áp dụng cho phần source Deskhub trong repo này và các binary được phát hành
trên trang Releases, TestFlight và Google Play. Nó không áp dụng cho Tailscale, hệ điều
hành, router, hay bất kỳ phần mềm nào khác bạn sử dụng cùng.
