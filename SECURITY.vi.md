[English](SECURITY.md) · **Tiếng Việt**

# Chính sách bảo mật của Deskhub

_Cập nhật lần cuối: 15 tháng 8, 2026_

> Đây là bản dịch của [`SECURITY.md`](SECURITY.md). Nếu hai bản có khác biệt, **bản tiếng
> Anh là bản chuẩn**.

## ⚠️ Đọc phần này trước

**Deskhub mã hoá phiên làm việc. Mọi thứ một phiên chuyên chở — video, phím gõ, chuột,
clipboard và terminal — đều chạy trên QUIC/TLS, và quyền vào được quyết bằng một cuộc
bắt tay ghép đôi: máy lạ phải chứng minh nó biết passcode của host (bản thân mã không
bao giờ đi qua mạng) hoặc được người ngồi tại host phê duyệt.** Thứ duy nhất còn đi ở
bản rõ là beacon dò tìm, vốn không mang bí mật nào; mọi thứ khác tới ngoài kênh mã hoá
đều bị bỏ.

Mọi host cũng có thể chia sẻ ở chế độ **chỉ xem** (thao tác gửi tới bị bỏ đi thay vì được
đưa vào máy), và tắt được hoàn toàn việc ghép đôi mới để chỉ máy đã ghép mới vào được.

Mã hoá không đồng nghĩa với chịu được Internet: cổng vẫn trả lời gói dò, passcode 4 chữ
số vẫn là một bí mật nhỏ, lần gặp đầu vẫn là bước nhảy niềm tin, và không gì ở đây chống
được flood. Deskhub vẫn được xây cho các mạng bạn tin tưởng.

Nên quy tắc vẫn giữ nguyên:

> **Đừng bao giờ port-forward UDP 47777. Đừng bao giờ phơi máy đang chia sẻ trực tiếp ra
> Internet. Muốn truy cập từ xa, hãy dùng VPN — [Tailscale](https://tailscale.com) là thứ
> dự án này được kiểm thử cùng — và kết nối tới địa chỉ `100.x.y.z`.**

Nếu bạn theo đúng quy tắc đó, Deskhub an toàn để dùng. Nếu bạn phá vỡ nó, bạn đang trao
máy của mình cho Internet.

## Mô hình mối đe doạ

### Deskhub bảo vệ được những gì

| | |
|---|---|
| Dữ liệu tới tay nhà phát triển | Không có gì cả. Không máy chủ, không tài khoản, không telemetry, không SDK bên thứ ba. Xem [`PRIVACY.vi.md`](PRIVACY.vi.md). |
| Người đọc trộm lưu lượng của bạn | Mọi phiên chạy trong QUIC/TLS — khung hình video, phím gõ, văn bản clipboard và byte terminal đều được mã hoá giữa hai máy. Bắt gói tin chỉ thu được khối lượng và nhịp độ, không thu được nội dung. Gói không mã hoá tới cổng đều bị bỏ trừ khi là gói dò tìm. |
| Người xem từ xa tranh máy với bạn | "Host thắng": ngay khi bạn chạm vào chuột hoặc bàn phím thật, thao tác từ xa tạm dừng (đúng như vậy trên cả host Windows, macOS và Linux). |
| Phím bị kẹt ở trạng thái nhấn | Mọi phím mà phía từ xa đang giữ đều được nhả tự động khi phiên kết thúc hoặc người xem chuyển đi chỗ khác. |
| Người lạ tự tiện kết nối | Quyền vào là một cuộc bắt tay ghép đôi. Máy lạ phải chứng minh passcode của host qua SPAKE2 — mã không bao giờ đi qua mạng, kẻ nghe lén không mang được gì về để dò, và mỗi kết nối chỉ được đúng một lần đoán — hoặc, khi host không đặt mã, đứng chờ người tại host trả lời *Let this machine in?*. Đoán sai 3 lần thì phần ghép đôi khoá 30 giây. Vào được rồi thì thành máy đã ghép đôi: được nhận diện bằng khoá mật mã, hiện trong trang Devices của host, và thu hồi được ở đó. Beacon dò tìm không còn xác nhận được mã đoán — gói dò của người lạ nhận danh sách rỗng bất kể chứa gì, nên cái oracle dò mã ngày trước đã đóng. |
| Máy chen giữa ở các lần ghé sau | Mỗi máy có một khoá. Client ghi nhớ khoá của từng host nó đã ghép đôi và từ chối kết nối lại khi khoá đổi, cho tới khi người dùng chấp nhận rõ ràng. Proof passcode được trói vào đúng khoá host mà client đang thấy, nên proof chuyển tiếp qua máy chen giữa không khớp. |
| Những người xem tranh chuột với nhau | Tối đa 5 người xem một host, nhưng chỉ một người điều khiển: ai vào trước thì thắng, và thao tác của người vào sau bị bỏ qua cho tới khi người vào trước ngừng thao tác một giây. Người thứ 6 bị từ chối với lý do `Busy`. |
| Người xem mà bạn chỉ muốn cho xem màn hình | Chế độ chia sẻ chỉ xem, có trên mọi host, bỏ các gói điều khiển ngay tại host trước khi bất cứ thứ gì được đưa vào máy — nó không dựa vào việc yêu cầu client tự giác. Host Android và iOS luôn ở chế độ chỉ xem, không tắt được. |
| Điện thoại bị bỏ quên trong lúc đang chia sẻ | Chốt chặn là hệ điều hành chứ không phải Deskhub: Android giữ một thông báo thường trực và hỏi lại quyền quay màn hình ở từng lần chia sẻ, còn iOS luôn hiện chỉ báo broadcast. Cả hai đều dừng được phiên chia sẻ mà không cần mở app. |
| Máy đã ghép đôi ghi tệp lên máy bạn | Chỉ máy đã được cho vào mới gửi tệp được, và chỉ khi máy nhận đang mở chia sẻ tệp. Thứ tới nơi không thoát ra khỏi thư mục máy đó đã chọn: tên trên đường truyền bị cắt còn phần cuối cùng của đường dẫn và được chà sạch dấu phân cách, byte điều khiển, ký tự mà hệ tệp không nhận và các tên thiết bị dành riêng, trước khi mở bất kỳ tệp nào; mỗi tệp được ghi dưới tên `.deskhub-part` và chỉ được đổi tên khi đã tới đủ và CRC-32 khớp; và tên đã có sẵn thì được thêm số chứ không ghi đè lên thứ gì. Một lô tối đa 32 tệp, 8 GiB mỗi tệp và 32 GiB tổng cộng. Việc chà sạch đó cũng chạy trên điện thoại và máy tính bảng trước khi bất cứ thứ gì tới được thư viện ảnh hay thư mục Downloads của chúng. |
| Gói tin dị dạng | Mọi trường đều được kiểm tra biên trước khi đọc. Các bộ phân tích gói được phủ bởi unit test, chạy dưới AddressSanitizer, UndefinedBehaviorSanitizer và ThreadSanitizer trong CI, và được fuzz mỗi đêm bằng libFuzzer — bảy target bao phủ định dạng gói tin, phân tích H.264, ráp gói, luồng byte của terminal, văn bản UI, và các máy trạng thái phiên của cả host lẫn viewer. Crash do fuzz tìm ra được giữ lại trong repo làm regression test, và độ phủ mới được gộp ngược vào bộ seed corpus. |

### Deskhub **không** bảo vệ được những gì

Đây là danh sách thành thật. Không điều nào dưới đây được giải quyết ở thời điểm hiện
tại:

- **Lần gặp đầu tiên là một bước nhảy niềm tin.** Ghép đôi chặn được máy chen giữa xuất
  hiện *về sau* — khoá đã ghim, đổi khoá là bị chặn kèm cảnh báo. Nó không chặn được kẻ
  đã đứng sẵn ở giữa ngay lần tiếp xúc đầu: host không đặt mã thì client với tới ai,
  người đó được ghép; passcode nâng rào lên đúng bằng sức của một bí mật 4 chữ số. Nếu
  điều này quan trọng với bạn, hãy so dấu vân tay qua một kênh khác.
- **Phân tích lưu lượng vẫn hiệu quả.** Mã hoá giấu nội dung, không giấu sự tồn tại: kẻ
  quan sát vẫn thấy có một phiên đang chạy, video đang chảy nhiều hay ít, và bạn gõ phím
  lúc nào.
- **Không giới hạn tần suất, không chống DoS.** Làm ngập cổng sẽ phá hỏng phiên đang
  chạy; host không đặt mã còn có thể bị bắt hiện hộp thoại phê duyệt liên tục.
- **Beacon dò tìm vẫn trả lời bất kỳ ai.** Gói `LIST_SOURCES` hay `PING` nhận được phản
  hồi từ bất kỳ địa chỉ nguồn nào — người lạ nhận danh sách rỗng, và không gói dò nào
  còn xác nhận được passcode, nhưng máy vẫn bị phát hiện bằng cách quét và cổng vẫn dùng
  được như một bộ phản xạ UDP nhỏ. Một ngoại lệ: địa chỉ nguồn đang giữ một kết nối mã
  hoá sẽ không bao giờ được trả lời bằng bản rõ — máy đã chứng minh mình rồi thì mọi thứ
  nó nói phải tới qua kênh mã hoá, nên một gói `SOURCE_LIST` hay `PONG` bản rõ giả mạo
  không thể mạo danh máy đang kết nối.
- **Tên thiết bị được hiển thị và ghi lại.** Tên trong ô *Your name* nay đã được mã hoá
  trên đường truyền, nhưng vẫn hiện trên màn hình host, ghi vào nhật ký host và lưu trong
  danh sách máy đã ghép của host. Mặc định nó là hostname của máy — thường chứa tên thật
  của chủ máy. Hãy dùng biệt danh; đừng bao giờ đặt thông tin nhạy cảm vào đó. Xoá trắng
  ô nhập không ngăn được việc gửi tên; nó chỉ khôi phục lại tên mặc định.
- **Một chỗ người xem tự giải phóng sau 5 giây im lặng.** Nếu người xem của bạn rớt mạng,
  chỗ đó mở lại và gói `Hello` tới tiếp theo sẽ chiếm — bất kể ai gửi, chỉ phải qua cửa
  passcode.
- **Chia sẻ là phơi nguyên màn hình.** Không phải một cửa sổ: mọi thông báo, cửa sổ bật
  lên và cửa sổ trên màn hình đó. Xem [`PRIVACY.vi.md` §3.4](PRIVACY.vi.md).
- **Điện thoại hay máy tính bảng làm host là phơi nguyên cái máy.** Android và iOS nay
  cũng chia sẻ được, và thứ chúng phát là toàn bộ màn hình — ứng dụng ngân hàng, mã một
  lần, tin nhắn, mọi mật khẩu bạn gõ trong lúc chia sẻ. Luồng hình được mã hoá như mọi
  phiên khác, nhưng người xem nào bạn đã cho vào thì thấy hết. Host di động luôn ở chế độ
  chỉ xem, điều đó loại bỏ rủi ro bị điều khiển từ xa nhưng không loại bỏ chút nào rủi ro
  lộ nội dung.

## Chạy ở đâu thì an toàn

✅ **An toàn**

- Mạng LAN ở nhà hoặc mạng cá nhân mà bạn kiểm soát mọi thiết bị trong đó.
- Một tailnet Tailscale (hoặc đường hầm WireGuard/VPN khác) mà chỉ thiết bị của bạn tham
  gia. VPN bổ sung một lớp mã hoá thứ hai và chặn người lạ với tới cổng ngay từ đầu.
- Một máy chỉ đóng vai *client* (điện thoại, máy tính bảng, laptop không bao giờ chia sẻ
  màn hình). Client không nhận phiên vào.

❌ **Không an toàn — đừng làm**

- Port-forward UDP 47777 qua router, hoặc đặt máy đang chia sẻ vào DMZ.
- Chia sẻ màn hình trên Wi-Fi quán cà phê, khách sạn, sân bay, trường học, coworking hay
  hội nghị.
- Chia sẻ trên mạng LAN của văn phòng hay nhà trọ chung mà bạn không tin tưởng mọi thiết
  bị khác.
- Bất kỳ mạng nào có thiết bị khách, thiết bị IoT bạn không tự cấu hình, hay máy của bạn
  cùng phòng mà bạn không quản trị.
- Phơi cổng qua giao diện công cộng của một máy ảo cloud hay một dịch vụ tunnel công khai.

Mặc định socket lắng nghe trên mọi giao diện mạng (`INADDR_ANY`), nên nó tiếp cận được
từ mọi mạng mà máy đang nối vào — kể cả mạng bạn quên là mình đang nối. Cài đặt **Share
on network** thu hẹp điều này: chọn một địa chỉ của máy thì host chỉ bind đúng giao diện
đó, nên máy ở các mạng còn lại thậm chí không chạm được tới cổng. Hai lưu ý: nếu địa chỉ
đã chọn không còn tồn tại lúc bạn bắt đầu chia sẻ (rút cáp, DHCP cấp địa chỉ mới),
Deskhub quay về lắng nghe trên mọi giao diện và nói rõ điều đó trong dòng trạng thái
chia sẻ — hãy kiểm tra banner nếu bạn dựa vào tính năng này; và bind một giao diện cũng
chặn luôn viewer qua loopback (`127.0.0.1`) trên cùng máy. Trên Windows, app chạy với
quyền cao ngay từ lúc khởi động (nó xin một lần, để gõ được vào các cửa sổ quyền cao) và
tự mở luật tường lửa giúp bạn khi bạn chia sẻ — luật đó mở cho cả app trên mọi profile,
nên bind hẹp lại không làm tường lửa hẹp theo; chính sự tiện lợi đó làm cho quy tắc phía
trên trở nên quan trọng.

## Kẻ tấn công cùng mạng làm được gì

Nếu ai đó ở cùng mạng LAN với một máy đang chia sẻ màn hình, và Deskhub đang chạy, họ có
thể:

1. Phát hiện ra máy đó bằng cách quét cổng UDP 47777. Gói dò của máy chưa ghép nhận về
   danh sách rỗng, nhưng máy vẫn trả lời, nên nó vẫn tự để lộ mình.
2. Tìm cách vào. Họ không còn đọc được passcode trên đường truyền — mã không bao giờ đi
   qua mạng. Còn lại là đoán mã trực tuyến (mỗi kết nối một lần đoán, sai 3 lần thì ghép
   đôi khoá 30 giây), hoặc với host không đặt mã thì trông chờ người ngồi tại host bấm
   **Allow** trên hộp thoại phê duyệt.
3. Quan sát lưu lượng mà không vào được — và chỉ thu được khối lượng cùng nhịp độ. Nội
   dung phiên, kể cả video, đã được mã hoá; bắt gói không còn dựng lại được màn hình hay
   phím gõ.
4. Làm ngập cổng để phá phiên. Không gì giới hạn tần suất với kẻ với tới được máy.

Hành vi "host thắng" chỉ hạn chế được phá phách khi bạn *đang ngồi tại* máy. Nó không có
tác dụng gì khi bạn rời khỏi máy, mà đó mới là lúc quan trọng.

## Danh sách việc nên làm để giảm rủi ro

Nếu bạn muốn tiếp tục dùng Deskhub như hiện tại, những việc sau đáng làm:

- [ ] Chạy Tailscale trên cả hai máy và chỉ kết nối qua địa chỉ `100.x.y.z`.
- [ ] Kiểm tra chắc chắn router của bạn **không** có port-forward hay ánh xạ UPnP nào cho
      UDP 47777.
- [ ] Quyết định cách máy khác được vào: đặt passcode 4 chữ số trong Settings, hoặc để
      trống và tự mình trả lời hộp thoại phê duyệt. Thỉnh thoảng xem lại trang Devices và
      quên những máy bạn không còn nhận ra. Bỏ tích *Viewers can control this machine*
      mỗi khi bạn chỉ cần cho ai đó xem.
- [ ] Thoát Deskhub khi bạn không dùng. Nó không chạy như dịch vụ nền — đóng app là đóng
      lỗ hổng.
- [ ] Trên Linux, nếu bạn dùng `ufw`, hãy giới hạn phạm vi luật thay vì mở toang:
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` thay cho
      `sudo ufw allow 47777/udp`.
- [ ] Đừng để một phiên chia sẻ chạy trên chiếc laptop mà bạn mang sang mạng khác.
- [ ] Khoá máy khi bạn rời đi, để một phiên không người trông không bị chiếm quyền âm
      thầm.
- [ ] Với `deskhub-cli`, đừng đặt passcode ngay trên dòng lệnh. `--passcode 0417` hiện ra
      cho mọi tiến trình trên máy qua `ps` và `/proc/*/cmdline`, và nó nằm lại trong lịch
      sử shell. Hãy dùng `--passcode -` để đọc từ đầu vào chuẩn, `--passcode @FILE` để đọc
      từ một tệp chỉ mình bạn đọc được, hoặc đặt biến môi trường `DESKHUB_PASSCODE`.

## Dấu vết để lại trên máy

Nhật ký chẩn đoán được ghi ở dạng văn bản thuần dưới `~/.deskhub/`
(`%USERPROFILE%\.deskhub` trên Windows) trên Windows, macOS và Linux. Chúng chứa thống kê
kết nối và địa chỉ của máy đối diện, không chứa nội dung màn hình hay phím gõ.

Các app desktop và `deskhub-cli` dùng chung những tệp đó. Chúng còn giữ thêm: `ui-settings.txt` (fps, bitrate,
giới hạn độ phân giải, các cổng, công tắc chỉ xem và công tắc ghép đôi, passcode host nếu
bạn có đặt, và tên thiết bị hiển thị cho host), `recent-devices.txt` (10 địa chỉ gần nhất
bạn đã kết nối, thời điểm, và passcode dùng cho từng địa chỉ), `host_key.pem` +
`host_cert.pem` (khoá riêng và chứng chỉ tự ký của máy này — danh tính đứng sau dấu vân
tay của nó; ai sao chép được tệp khoá là mạo danh được máy này), `known_hosts` (khoá của
các host mà máy này đã tin), `paired_devices` (khoá, tên và mốc thời gian của các máy
được phép vào host này), `auth_salt` (salt không bí mật cho verifier của passcode) và,
trên Linux, `portal-restore-token.txt` (token của chính desktop cho những màn hình bạn đã
chọn, chỉ có ý nghĩa với phiên desktop của bạn và không bao giờ được truyền đi). Các
app di động giữ cài đặt của chúng trong vùng sandbox riêng — trên iOS là trong app group
container. Passcode được lưu bằng cách che đi với một khoá XOR cố định, đủ để nó không
hiện lên màn hình và không lộ ra khi mở tệp xem qua — **đó không phải mã hoá**, và ai có
mã nguồn cùng tệp đó khôi phục lại chúng trong vài giây. Hãy coi thư mục đó là đọc được
bởi mọi thứ chạy dưới danh nghĩa tài khoản của bạn.

Tệp mà máy khác gửi tới rơi ra ngoài thư mục đó, vào đúng thư mục máy nhận đã chọn cho
chúng (`Deskhub` trong thư mục nhà của người dùng nếu không chọn thư mục khác, lưu dưới
khoá `transfer_dir`). Trên điện thoại và máy tính bảng, chúng vào thư viện ảnh hoặc thư
mục Documents / Downloads của thiết bị, và còn nguyên ở đó sau khi gỡ cài đặt app. Hãy
coi mọi thứ được giao tới đó là tệp do một máy đã ghép đôi đặt lên thiết bị của bạn.

Không thứ gì trong số này được tải lên đâu cả; bạn xoá thư mục đó lúc nào cũng được.

## Các biện pháp giảm thiểu đã lên kế hoạch

Đang theo dõi, theo thứ tự dự định làm:

1. **Lưu passcode và khoá của host trong keychain của hệ điều hành** thay vì tệp.
2. **Làm im beacon dò tìm** để nó không trả lời gì cả trước một gói dò không mời, thay vì
   trả lời bằng danh sách rỗng.

Đã hoàn thành kể từ lần cập nhật trước của danh sách này: kênh truyền mã hoá (QUIC/TLS)
cho toàn bộ phiên — video, thao tác, clipboard lẫn terminal — kèm việc bỏ mọi gói không
mã hoá tới nơi trừ gói dò tìm; ghép đôi bằng SPAKE2 để passcode không bao giờ đi qua
mạng, không thu hoạch được và không dò offline được; hộp thoại phê duyệt trên host; danh
sách máy đã ghép kèm thu hồi; khoá máy với cảnh báo khoá đổi phía client; và khoá
3-lần-sai / 30-giây cho việc đoán passcode.

Danh sách này là tuyên bố về ý định, không phải lịch trình. Deskhub do một người duy trì
trong thời gian rảnh. Hãy coi hiện trạng là hiện trạng, không phải kế hoạch.

## Báo cáo lỗ hổng

Vui lòng báo cáo vấn đề bảo mật **một cách riêng tư** — đừng mở issue công khai trên
GitHub.

- **Email:** manhpv151090@gmail.com — ghi `[Deskhub security]` ở tiêu đề.
- **Hoặc:** mở một [security advisory riêng tư](https://github.com/manhpham90vn/Deskhub/security/advisories/new)
  trên GitHub.

Vui lòng nêu rõ bạn đang chạy gì (hệ điều hành, phiên bản Deskhub trên thanh tiêu đề hoặc
trong [`VERSION`](VERSION)), bạn đã làm gì, và chuyện gì đã xảy ra. Một proof of concept
giúp ích rất nhiều.

**Điều bạn có thể mong đợi:** một xác nhận đã nhận trong vòng 7 ngày và một đánh giá
trong vòng 30 ngày. Đây là dự án làm lúc rảnh của một người, nên mong bạn kiên nhẫn với
mốc thời gian — nhưng dù thế nào bạn cũng sẽ nhận được câu trả lời thẳng thắn. Nếu có bản
vá, bạn sẽ được ghi công trong release notes trừ khi bạn không muốn.

Không có chương trình thưởng lỗi; không có khoản chi trả nào.

**Những gì đã được ghi ở trên không phải là lỗ hổng.** Các giới hạn kể trên — bước nhảy
niềm tin ở lần gặp đầu, phân tích lưu lượng, beacon trả lời gói dò, không chống DoS — là
đã biết và đã liệt kê; một báo cáo nhắc lại chúng không cho chúng tôi biết điều gì mới. Những gì *đáng* báo cáo: lỗi hỏng bộ
nhớ hoặc crash có thể kích hoạt từ một gói tin dị dạng, một cách thoát ra khỏi mô hình
mối đe doạ đã ghi nhận, bất cứ thứ gì làm rò dữ liệu ra khỏi máy, hoặc một lỗ hổng trong
một biện pháp giảm thiểu sau khi nó được triển khai.

## Các phiên bản được hỗ trợ

Chỉ bản phát hành mới nhất trên [trang Releases](https://github.com/manhpham90vn/Deskhub/releases)
được hỗ trợ. Bản vá đi kèm bản phát hành mới; không có backport về các phiên bản cũ.

## Phạm vi

Chính sách này áp dụng cho mã nguồn Deskhub trong kho này và các bản dựng nhị phân được
công bố trên trang Releases, TestFlight và Google Play. Nó không áp dụng cho Tailscale, hệ
điều hành của bạn, router của bạn, hay bất kỳ phần mềm nào khác bạn chạy song song.
