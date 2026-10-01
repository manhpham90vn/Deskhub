[English](SECURITY.md) · **Tiếng Việt** · [中文](SECURITY.zh.md) · [日本語](SECURITY.ja.md)

# Chính sách bảo mật của Deskhub

_Cập nhật lần cuối: 1 tháng 10 năm 2026_

Đây là bản dịch của [`SECURITY.md`](SECURITY.md). Nếu hai bản có khác biệt, bản tiếng Anh
là bản chuẩn.

## ⚠️ Cần đọc trước

**Hãy dùng Deskhub trên network tin cậy hoặc qua VPN. Không port-forward UDP 47777 và
không đưa máy đang share trực tiếp ra Internet.**

Session chạy trên QUIC/TLS, gồm video, phím gõ, mouse, clipboard và lưu lượng terminal.
Quyền truy cập hoạt động như SSH: client chỉ được vào khi public key của nó có trong
`authorized_keys` của host, và nó phải chứng minh mình giữ private key tương ứng. Một key
chỉ vào được danh sách đó qua hành động của chủ host — bấm **Approve** trên yêu cầu kết nối
của thiết bị, cho thiết bị xem **mã QR** trong lúc share, hoặc dán public key của thiết bị.
Không có passcode và không có công tắc nào cho máy lạ vào. Mỗi máy có một key, và client
ghim key của host — ở lần connect đầu tiên sau khi đối chiếu fingerprint, hoặc từ mã QR — và
kiểm tra nó trước khi gửi bất cứ thứ gì; host đổi địa chỉ vẫn được trust, còn một key khác
xuất hiện ở một địa chỉ đã biết được coi là một máy chưa từng gặp, kèm cảnh báo. Host không
trả lời packet plaintext nào: mọi dữ liệu nằm ngoài connection đã encrypt đều bị loại bỏ.

Mọi host cũng có thể share ở chế độ **view-only** (input bị loại bỏ thay vì được inject).

Encrypt không loại bỏ mọi rủi ro trên network. Lần connect đầu tiên tới một host sẽ trust
bất kỳ key nào được hiển thị, trừ khi bạn đối chiếu fingerprint, và app không có cơ chế
chống flooding.

Để truy cập từ xa, hãy dùng VPN. Dự án đã kiểm thử với
[Tailscale](https://tailscale.com); bạn có thể connect tới địa chỉ `100.x.y.z` của host.

Hãy giữ host trên một network tin cậy, và xem các giới hạn bên dưới trước khi share màn
hình hoặc terminal.

## Threat model

### Những gì Deskhub bảo vệ

| | |
|---|---|
| Dữ liệu tới tay người phát triển | Không có dữ liệu nào. Không server, không tài khoản, không telemetry, không SDK bên thứ ba nào thu thập dữ liệu. Xem [`PRIVACY.vi.md`](PRIVACY.vi.md). |
| Việc đọc trộm lưu lượng | Mọi session đều chạy bên trong QUIC/TLS: frame video, phím gõ, văn bản clipboard và byte terminal đều được encrypt giữa hai máy. Việc bắt gói chỉ cho biết khối lượng và thời điểm, không cho biết nội dung. Packet chưa encrypt tới port đều bị loại bỏ, và host không gửi gì ở tầng ứng dụng — kể cả nội dung đang share — trước khi client authenticate. |
| Viewer từ xa tranh quyền điều khiển | Host được ưu tiên: ngay khi bạn thao tác với mouse hoặc keyboard thật, remote input bị tạm dừng. Host trên Windows và macOS làm việc này ngay từ đầu. Host trên Linux chỉ làm được khi tài khoản chạy Deskhub đọc được `/dev/input/event*` — trên thực tế là phải thuộc nhóm `input`, điều mà các gói cài đặt không cấp; nếu không, log ghi lại một lần và remote input không bao giờ bị tạm dừng. |
| Phím bị kẹt | Mọi phím mà phía từ xa đang giữ đều được nhả tự động khi session kết thúc hoặc viewer chuyển sang cửa sổ khác. |
| Người lạ kết nối không được phép | Chỉ client có public key nằm trong `authorized_keys` của host mới được vào, và nó phải ký một transcript của chính connection này bằng private key tương ứng. Một key chỉ vào được danh sách đó theo đúng ba cách, đều nằm trong tay chủ host: chủ host bấm **Approve** trên yêu cầu kết nối của thiết bị — một hàng hiển thị tên thiết bị, fingerprint của key mà nó đã chứng minh là mình giữ và địa chỉ của nó — thiết bị scan mã QR mà chủ host hiển thị trong lúc share, với token ngẫu nhiên 32 byte chỉ dùng được một lần, trong 5 phút, và hết hiệu lực khi mã bị ẩn hoặc share dừng, hoặc chủ host dán public key của thiết bị. Người lạ kết nối tới không được vào: chỉ sau khi nó đã ký bằng key mà nó đưa ra, host mới ghi lại một yêu cầu mà chủ host có thể bỏ qua, và khoảng hai giây sau khi trả lời, host tự đóng connection, như nó làm với mọi connection nó từ chối. Không có passcode, khi thiếu file `authorized_keys` thì không ai vào được, và không có gì cho phép client nói chuyện để vượt qua chủ host. Host giữ tối đa 8 connection đang chờ authenticate — mọi connection chưa authenticate xong đều được tính, bất kể nó đưa ra key nào, và connection thứ chín không được chấp nhận — và loại bỏ từng connection 10 giây sau khi chấp nhận nó nếu tới lúc đó nó vẫn chưa authenticate; nó giữ tối đa 16 yêu cầu, tối đa một yêu cầu cho mỗi địa chỉ nguồn — một yêu cầu mới từ địa chỉ đã có yêu cầu sẽ thay thế yêu cầu đó — mỗi yêu cầu trong 10 phút; 3 chữ ký sai từ một key và một địa chỉ trong vòng một phút sẽ chặn cặp đó trong 10 giây, và một token QR sai, từ một thiết bị đã ký, được tính vào cùng giới hạn đó cho địa chỉ nguồn của nó. Việc chờ phê duyệt không bị tính là thất bại. Gỡ một key trên trang Devices của host cũng đóng ngay các session đang chạy của thiết bị đó. Việc được chấp nhận chỉ kéo dài bằng đúng connection đã giành được nó. |
| Tấn công xen giữa | Mỗi máy có một key. Client ghim key của host — ở lần connect đầu tiên sau khi người dùng đối chiếu fingerprint, hoặc từ mã QR, vốn mang sẵn key đó — và kiểm tra nó trước khi gửi bất cứ thứ gì ở mọi lần sau. Trust đi theo key, nên host ở địa chỉ mới vẫn là host đó; một key *khác* ở địa chỉ mà client đã biết bị từ chối tư cách đã trust và được hiển thị như một **New host** kèm cảnh báo nêu tên máy từng trả lời ở đó — nó không thể được chấp nhận như một "thay đổi", chỉ có thể được trust lại từ đầu với fingerprint hiển thị rõ. Client scan mã QR chỉ gửi token sau khi máy đang trả lời đã chứng minh, qua TLS handshake, rằng nó giữ key in trong mã; bất cứ thứ gì khác ở địa chỉ đó không nhận được gì. Chữ ký của client bao gồm một session identifier export từ phiên TLS và fingerprint host key mà client nhìn thấy, nên chữ ký bị relay sang host khác, hoặc bị phát lại trên connection khác, sẽ không hợp lệ. |
| Nhiều viewer tranh quyền điều khiển mouse | Tối đa 5 viewer cùng xem một host, nhưng chỉ một viewer điều khiển input: viewer tham gia sớm hơn được ưu tiên, và input của viewer tới sau bị loại bỏ cho tới khi viewer trước không thao tác trong một giây. Viewer thứ 6 bị từ chối với trạng thái `Busy`. |
| Viewer chỉ được phép xem | Chế độ share view-only, có trên mọi host, loại bỏ các packet input ngay tại host trước khi bất cứ thao tác nào được inject; cơ chế này không dựa vào việc client tự tuân thủ. Host trên Android và iOS luôn ở chế độ view-only. |
| Điện thoại bị bỏ quên trong trạng thái đang share | Cơ chế bảo vệ cuối cùng thuộc về hệ điều hành chứ không phải Deskhub: Android hiển thị một notification thường trực và yêu cầu đồng ý ghi màn hình ở từng phiên share, còn iOS giữ chỉ báo broadcast luôn hiển thị. Cả hai đều cho phép dừng share mà không cần mở app. |
| Client được phép ghi file vào máy của bạn | Chỉ máy đã được chấp nhận mới gửi được file, và chỉ khi máy nhận đang bật file transfer. Dữ liệu tới không thể thoát khỏi thư mục mà máy đó chọn: tên file trên đường truyền bị cắt còn phần cuối của đường dẫn và loại bỏ dấu phân cách, byte điều khiển, ký tự filesystem không chấp nhận cùng các tên thiết bị dành riêng, trước khi bất kỳ file nào được mở. Mỗi file được ghi dưới tên có hậu tố `.deskhub-part` và chỉ được đổi tên khi đã nhận đủ với CRC-32 khớp. Tên đã tồn tại sẽ được thêm số thứ tự thay vì ghi đè. Một batch bị giới hạn ở 32 file, 8 GiB mỗi file và 32 GiB tổng cộng. Cùng cơ chế xử lý tên này cũng chạy trên điện thoại và tablet trước khi dữ liệu tới thư viện ảnh hoặc thư mục Downloads. |
| Packet không hợp lệ | Mọi trường đều được kiểm tra giới hạn trước khi đọc. Các parser có unit test, chạy dưới AddressSanitizer, UndefinedBehaviorSanitizer và ThreadSanitizer trong CI, và được fuzz bằng libFuzzer — 30 giây cho mỗi target trên mọi pull request và 15 phút cho mỗi target mỗi đêm. Có chín target, bao phủ wire format, phần parse H.264 (Annex B và SPS), reassembly packet, session state machine phía host và phía viewer, chuỗi UI, byte stream terminal, các file key và trust đã lưu cùng pairing link, và bộ mã hoá mã QR. Các crash phát hiện qua fuzzing được lưu trong repo dưới dạng regression test, và phần coverage mới được bổ sung vào seed corpus. |

### Những gì Deskhub **không** bảo vệ

Đây là danh sách đầy đủ. Không mục nào dưới đây đã được giải quyết:

- **Shell được giữ lại thuộc về mọi client được phép, không thuộc riêng máy đã mở nó.**
  Một shell còn lại trên host sống lâu hơn kết nối đã mở nó, không có giới hạn thời gian,
  và mọi máy đã được nhận vào đều có thể liệt kê các shell host đang giữ, reattach một
  shell đã detach, và đóng bất kỳ shell nào. Id, kích thước và tên thiết bị của từng
  shell nằm trong danh sách đó. Vì vậy một client thứ hai bạn cho phép —— hoặc một client
  mà bạn chưa gỡ key trên trang Devices —— có thể đọc lại những gì shell trước đó đang
  làm và tiếp tục trong đó. Hãy gỡ key bạn không còn tin tưởng, và đóng các shell đã dùng
  xong thay vì để lại.
- **Lần kết nối đầu tiên dựa trên tin cậy chưa được xác minh.** Việc ghim host key ngăn
  được kẻ xen giữa xuất hiện ở *các lần sau* — một key khác là một host khác, và hộp thoại
  nói rõ điều đó. Nó không ngăn được kẻ đã xen giữa ngay từ lần tiếp xúc đầu tiên, trừ khi
  bạn đối chiếu fingerprint mà hộp thoại *New host* hiển thị với fingerprint trên trang
  Devices của host, như hộp thoại yêu cầu. Scan mã QR của host làm việc đối chiếu đó thay
  bạn, vì mã mang sẵn fingerprint. `deskhub-cli` từ chối host chưa biết trừ khi được chạy
  với `--accept-new-host-key`, hoặc bạn có thể ghim key trước bằng
  `host add … --host-key-stdin`.
- **Approve nhầm yêu cầu là lỗi của chủ host, và Deskhub không bắt được lỗi đó.** Một yêu
  cầu kết nối hiển thị tên do thiết bị tự chọn, fingerprint của key nó giữ, và địa chỉ nó
  đến từ. Thiết bị đã chứng minh nó giữ key đó — một yêu cầu chỉ được ghi lại sau khi có
  chữ ký hợp lệ — nhưng tên không chứng minh được gì, và tạo key thì không tốn gì. Hãy đối
  chiếu fingerprint với trang Devices của chính thiết bị đó, và đối chiếu địa chỉ với nơi
  bạn mong đợi nó ở, trước khi bấm *Approve*. Bất kỳ ai trong network cũng để lại được một
  yêu cầu; chỉ bạn mới biến được yêu cầu thành quyền truy cập.
- **Yêu cầu có thể bị đẩy ra ngoài, nhưng không phải từ một địa chỉ.** Host giữ tối đa một
  yêu cầu cho mỗi địa chỉ nguồn, nên một yêu cầu từ địa chỉ đã có yêu cầu sẽ thay thế yêu
  cầu đó, và tổng cộng tối đa 16, bỏ yêu cầu cũ nhất để lấy chỗ. Vì vậy các key mới từ một
  máy chỉ ghi đè hàng của chính máy đó; muốn đẩy một yêu cầu thật ra khỏi danh sách cần 16
  địa chỉ khác nhau. Một thiết bị dùng chung địa chỉ với người khác — chẳng hạn sau cùng
  một NAT — vẫn có thể bị yêu cầu của họ thay thế yêu cầu của mình. Nếu không thấy một yêu
  cầu bạn đang chờ, hãy bảo thiết bị kết nối lại.
- **Mã QR là một bí mật trong năm phút, đối với bất kỳ ai nhìn thấy màn hình.** Nó cho một
  thiết bị vào mà không cần click nào trên host. Người chụp lại được mã — qua vai bạn, từ
  một screenshot, từ một màn hình đang share — có thể dùng nó thay bạn cho tới khi mã hết
  hạn, đã được dùng, hoặc bị ẩn. Chỉ cho người bạn định cho vào xem mã, và ẩn mã ngay khi
  họ đã kết nối; thiết bị được mã cho vào sau đó xuất hiện dưới *Devices allowed to connect to this machine*,
  nơi bạn có thể gỡ nó nếu đó không phải thiết bị bạn mong đợi.
- **Link `deskhub://` chỉ đáng tin bằng người đã gửi nó.** Link mà host hiển thị cạnh mã QR
  có thể được mở từ bất kỳ trang web, tin nhắn hay app nào trên điện thoại hoặc tablet (trên
  Android, chỉ link `deskhub://pair` tới được app). Khi mở theo cách đó, Deskhub không âm
  thầm trust nó: trừ khi host mà link nêu đã được trust, app hiển thị cùng hộp thoại xác
  nhận *New host* như ở lần connect đầu tiên, kèm fingerprint mà link nêu, và chỉ kết nối
  sau khi bạn chấp nhận. Nhưng fingerprint đó lấy từ chính link. Nếu chấp nhận một link do người
  khác tạo mà không đối chiếu fingerprint với trang Devices của host, bạn có thể đang trust
  máy của họ: máy đó nhận pairing token dành cho host thật, cho bạn xem bất kỳ màn hình nào
  nó muốn, và nhận những gì bạn gõ và gửi trong session đó. Scan mã QR bằng trình scan trong
  app thì trust mã mà không hỏi, vì bạn đang hướng camera vào chính màn hình của host.
- **Trên Linux, gói cài đặt mở rộng phạm vi những ai có thể inject input.** Điều khiển từ xa
  cần `/dev/uinput`, nên gói `.deb` và `.rpm` cài một udev rule cấp nó cho nhóm `input` và
  cho người dùng đang đăng nhập tại seat (`MODE="0660", GROUP="input", TAG+="uaccess"`).
  Khi đó mọi chương trình chạy dưới tài khoản đó đều có thể tạo keyboard hoặc mouse ảo,
  không riêng Deskhub. Và nhóm `input` mà cơ chế ưu tiên host cần cũng đọc được mọi keyboard
  trên máy — bất cứ thứ gì chạy dưới một thành viên của nhóm đều có thể ghi lại phím gõ. Chỉ
  thêm người dùng vào nhóm này nếu bạn chấp nhận điều đó.
- **Phân tích lưu lượng vẫn khả thi.** Việc encrypt che giấu nội dung chứ không che giấu
  sự tồn tại: người quan sát biết được có một session đang chạy, lượng video đang truyền,
  và thời điểm bạn gõ phím.
- **Không có rate limiting và không chống DoS.** Việc gửi lượng lớn dữ liệu tới port sẽ
  làm gián đoạn session. Giới hạn về số lần authenticate đang chờ, số yêu cầu đang chờ và
  số chữ ký sai chỉ ngăn việc dò đoán, không ngăn flooding.
- **QUIC handshake vẫn được trả lời.** Host không còn phản hồi packet plaintext nào,
  nhưng một QUIC/TLS handshake tới port vẫn hoàn tất trước khi client chứng minh được gì,
  nên người lạ biết địa chỉ vẫn có thể biết có dịch vụ đang lắng nghe, và thấy được
  certificate của host.
- **Tên thiết bị được hiển thị và ghi log.** Tên thiết bị mà client gửi được encrypt trên
  đường truyền, nhưng vẫn hiển thị trên màn hình của host, được ghi vào log của host, xuất
  hiện trong mọi yêu cầu kết nối mà máy để lại, và là nhãn của public key mà máy copy ra
  cũng như của key mà host lưu khi approve máy đó hoặc cho nó vào bằng mã QR — nên tên này
  nằm trong `authorized_keys` của mọi host cho phép key đó. Host cũng gửi tên thiết bị của chính nó tới mọi client đã
  authenticate bằng một key được phép — không bao giờ trước đó — và client đó giữ tên
  trong danh sách gần đây của nó. Giá trị mặc định là hostname của máy, thường trùng với tên thật của
  người dùng. Nên đặt một biệt danh trong Settings → General → *Device name* và không đặt
  thông tin nhạy cảm vào đó. Việc xoá trắng trường này không ngăn tên được gửi đi, mà chỉ
  khôi phục giá trị mặc định.
- **Vị trí viewer tự giải phóng sau 5 giây không có dữ liệu.** Nếu viewer của bạn
  mất kết nối, vị trí đó được mở lại và `Hello` tới tiếp theo sẽ chiếm chỗ, miễn là máy gửi
  đã qua admission bằng một key được phép.
- **Việc share phơi ra toàn bộ display.** Không phải một cửa sổ, mà là mọi notification,
  popup và cửa sổ trên màn hình đó. Xem [`PRIVACY.vi.md` §3.4](PRIVACY.vi.md).
- **Host là điện thoại hoặc tablet phơi ra toàn bộ thiết bị.** Android và iOS cũng host
  được, và nội dung chúng stream là toàn bộ màn hình: app ngân hàng, mã một lần, tin nhắn,
  mọi mật khẩu bạn nhập trong khi share. Stream được encrypt như mọi session khác, nhưng
  mọi viewer được chấp nhận đều nhìn thấy toàn bộ. Host di động luôn ở chế độ view-only,
  điều này loại bỏ rủi ro bị điều khiển từ xa nhưng không giảm rủi ro lộ thông tin.

## Mật mã

Từng thành phần được dựng từ gì, dành cho ai muốn kiểm tra thiết kế:

- **Key của thiết bị.** Mỗi thiết bị có một key ECDSA trên đường cong NIST P-256, do
  BoringSSL tạo ở lần chạy đầu tiên và lưu trong `host_key.pem`. Cùng key đó định danh thiết
  bị khi làm host và ký thay nó khi làm client. Fingerprint của nó là SHA-256 của
  SubjectPublicKeyInfo dạng DER của key, được viết là `SHA256:` theo sau là base64 không
  padding.
- **Certificate.** Host đưa ra một certificate X.509 v3 tự ký cho key đó: subject và issuer
  `CN=deskhub`, serial number ngẫu nhiên 63 bit, hiệu lực 20 năm kể từ lúc port mở, ký bằng
  ECDSA-SHA256. Certificate được tạo mới mỗi lần và không chứa tên hay địa chỉ nào.
- **Transport.** QUIC qua quiche của Cloudflare, với TLS 1.3 từ BoringSSL, ALPN `deskhub`.
  Không bên nào kiểm tra chuỗi certificate — không có certificate authority, nên việc
  verify peer bị tắt — thay vào đó client so SHA-256 của public key trong certificate với
  fingerprint đã ghim, trước khi gửi bất cứ thứ gì.
- **Authenticate client.** Hai đầu dẫn xuất một session identifier 32 byte bằng TLS
  exporter, label `EXPORTER-Deskhub-Auth-v5`. Client ký một transcript gồm một chuỗi domain
  cố định (`Deskhub/auth/signature`), phiên bản giao thức, vai trò của nó, session identifier
  đó, public key của chính nó và fingerprint key của host; host kiểm tra chữ ký dựa trên
  `authorized_keys`. Các key của chính Deskhub ký bằng ECDSA P-256 với SHA-256. Host cũng
  chấp nhận một public key `ssh-ed25519` được dán vào và kiểm tra chữ ký Ed25519 tạo bằng
  key đó; bản thân Deskhub không bao giờ tạo key như vậy, vì TLS cần key thiết bị là P-256.
- **Pairing token.** 32 byte lấy từ bộ sinh số ngẫu nhiên của hệ điều hành
  (`BCryptGenRandom` trên Windows, `arc4random_buf` trên các nền tảng Apple, `getrandom` trên
  Linux và Android, dự phòng bằng `/dev/urandom`), được so sánh trong thời gian hằng, dùng
  một lần, và sống 5 phút. Host giữ tối đa 4 token còn hiệu lực cùng lúc và bỏ token sắp hết
  hạn nhất để lấy chỗ cho token mới.

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
ngay từ khi khởi động, để inject được input vào các cửa sổ quyền cao: UAC hỏi mỗi lần bạn
mở app, trừ khi *Start Deskhub when you log in* khởi động nó qua scheduled task, vốn chạy
app ở quyền cao lúc đăng nhập mà không hỏi. App cũng tự mở firewall khi bạn share, bằng một
rule UDP chiều vào tên *Deskhub (host)* — rule này áp dụng cho toàn bộ app trên mọi profile,
nên việc thu hẹp bind không làm thu hẹp firewall; đây cũng là lý do nguyên tắc ở phần trên
có ý nghĩa. Deskhub chỉ thay thế đúng rule đó của chính nó. Rule chặn mà bạn tự tạo cho app
được giữ nguyên, và vì Windows cho rule chặn thắng rule cho phép, host sẽ không truy cập
được cho tới khi bạn gỡ rule đó.

## Khả năng của kẻ tấn công trong cùng network

Nếu một người ở cùng LAN với máy đang share màn hình và Deskhub đang chạy, họ có thể:

1. Tìm ra máy đó bằng cách thử QUIC handshake tới UDP 47777 trên từng địa chỉ. Không
   packet plaintext nào được trả lời, nhưng chính handshake thì có, nên máy vẫn bị phát
   hiện qua một lần quét có chủ đích.
2. Thử kết nối — việc này cần một private key có nửa public nằm trong `authorized_keys`,
   và chỉ *Approve* của chủ host, một token QR còn hiệu lực hoặc một lần dán mới đưa được
   key vào đó. Điều người lạ *có thể* làm là để lại một yêu cầu kết nối mà chủ host thấy
   trên trang Host, dưới một cái tên tự chọn và với một key vừa tạo; yêu cầu hết hạn sau
   10 phút, mỗi địa chỉ chỉ giữ một yêu cầu tại một thời điểm — nên các key mới từ một máy
   chỉ ghi đè hàng của chính nó — và chỉ một lần
   *Approve* mới biến nó thành quyền truy cập. Họ có thể thử đoán token QR, ký mỗi lần đoán bằng
   một key của riêng họ, nhưng token sai bị tính vào địa chỉ của họ như một chữ ký sai — 3 lần trong một phút là bị chặn 10 giây
   — và token là 32 byte ngẫu nhiên, sống 5 phút và chỉ dùng được một lần. Ngoài những
   điều đó, điều tối đa họ làm được là cố xen vào giữa lần connect *đầu tiên* của một client
   tới host, và việc đối chiếu fingerprint — hoặc fingerprint nằm trong mã QR — sẽ phát hiện
   điều đó.
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
- [ ] Chỉ cho phép những client key cần thiết trên host, và đối chiếu fingerprint host key
      ở lần connect đầu tiên từ mỗi client. Rà soát trang Devices và gỡ những key không
      còn nhận ra. Bỏ chọn *Viewers can control this machine* khi chỉ cần cho người khác xem.
- [ ] Chỉ approve những yêu cầu kết nối bạn đang mong đợi, và kiểm tra fingerprint cùng
      địa chỉ trong hàng đó trước khi bấm. Deny hoặc bỏ qua phần còn lại — chúng tự hết
      hạn, và một thiết bị đã bị deny không thể xin lại trong mười phút.
- [ ] Ẩn mã QR ngay khi thiết bị bạn cho xem đã kết nối, và không bao giờ hiển thị mã trên
      màn hình đang share hoặc đang trình chiếu.
- [ ] Thoát Deskhub khi không sử dụng. App không chạy như một background service, nên
      thoát app là đóng luôn điểm truy cập. Ba setting trong *Launch & background* thay đổi
      điều đó: *Start Deskhub when you log in* khởi động app ở mỗi lần đăng nhập (trên
      Windows qua một scheduled task chạy app ở quyền cao mà không có hộp thoại UAC),
      *Start sharing when Deskhub opens* sau đó share ngay, và *Keep running in the
      background (tray icon) when the window is closed* khiến việc đóng cửa sổ không còn
      thoát app. Kết hợp lại, chúng để một host lắng nghe ngay từ lúc bạn đăng nhập mà không
      có cửa sổ nào trên màn hình. Hãy để chúng tắt trừ khi bạn thực sự muốn đúng như vậy, và
      thoát từ icon trên tray khi icon đó đang hiển thị.
- [ ] Trên Linux, hãy cân nhắc kỹ trước khi tự thêm mình vào nhóm `input` để có cơ chế ưu
      tiên host: nó cũng cho mọi chương trình bạn chạy đọc được mọi keyboard.
- [ ] Trên Linux, nếu dùng `ufw`, hãy thu hẹp rule thay vì mở rộng:
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` thay cho
      `sudo ufw allow 47777/udp`.
- [ ] Không để một phiên share đang chạy trên laptop mà bạn mang sang các network khác.
- [ ] Khoá máy khi rời đi, để một session không người trông coi không bị chiếm quyền.
- [ ] Với `deskhub-cli`, dùng `key public` để xem public key của máy này và
      `host-key public` để xem key của nó khi làm host — đó là cùng một key. Chuyển key qua
      kênh tin cậy, rồi dùng `access add --stdin` trên host và `host add … --host-key-stdin`
      trên client. Để approve một yêu cầu từ terminal, đọc `access requests` và trả lời
      bằng `access approve --fingerprint SHA256:…` chỉ với fingerprint bạn mong đợi.

## Dữ liệu lưu trên máy

App desktop ghi log chẩn đoán ở dạng văn bản thuần trong `~/.deskhub/`
(`%USERPROFILE%\.deskhub` trên Windows), mỗi lần chạy một file tên
`deskhub-<date>-<time>-<pid>.log`; trên macOS và Linux, symlink `deskhub-latest.log` trỏ tới
file mới nhất. Trên macOS và Linux, mỗi log được tạo chỉ cho bạn đọc (`0600`) và không bao
giờ được ghi qua một symlink; trên Windows, nó nhận quyền hạn chế của thư mục chứa nó. Log
cũ bị xoá để chỉ giữ lại mười file mới nhất. Log chứa thống kê kết nối (các dòng `[DIAG]`: bitrate, mất gói, độ trễ, thời
gian xử lý frame), địa chỉ và port của peer, tên thiết bị mà peer gửi, fingerprint của key,
tên, kích thước và kết quả của từng file gửi hoặc nhận cùng thư mục mà file được lưu vào, và
các lỗi. Log không chứa nội dung màn hình, các phím viewer bấm — trong session điều khiển từ xa hay
trong terminal — hay vị trí con trỏ di chuyển,
văn bản clipboard hay byte terminal. Android và iOS chỉ ghi cùng các dòng đó vào log hệ thống
(logcat, Xcode console), không bao giờ ghi ra file.

App desktop và `deskhub-cli` dùng chung phần còn lại của thư mục đó; `DESKHUB_CONFIG_DIR`
hoặc `--config-dir` của CLI trỏ cả hai sang một thư mục khác cho các file này:
`ui-settings.txt` (fps, bitrate, giới hạn độ phân giải, port, switch view-only, tên thiết
bị, địa chỉ bind và các toggle khác), `recent-hosts.txt` (10 host kết nối gần nhất — địa chỉ,
thời điểm và tên mà mỗi host tự báo), `host_key.pem` (private key duy nhất của máy này —
danh tính đứng sau fingerprint của nó ở cả hai vai; người có được file này có thể mạo danh
máy này khi làm host *và* đăng nhập ở mọi nơi máy này được cho phép), `authorized_keys`
(các client public key được phép vào host này, mỗi key kèm nhãn), `known_hosts` (các host
mà máy này trust — fingerprint đã ghim, thời điểm lần đầu và lần cuối gặp host, địa chỉ và
port gần nhất mà host trả lời, và tên của nó), `access_requests` (các thiết bị đang chờ bạn
*Approve* — tên, public key, địa chỉ và thời điểm; tối đa 16, mỗi yêu cầu bị loại bỏ sau 10
phút), `pairing_tokens` (các token QR đang còn hiệu lực và thời điểm hết hạn của từng token —
một bản copy của file này có giá trị ngang với mã trên màn hình cho tới khi chúng hết hạn)
và, trên Linux, `portal-restore-token.txt` (token của chính desktop cho những màn hình bạn đã
chọn, chỉ có ý nghĩa với phiên desktop của bạn và không được truyền đi). Bên cạnh chúng bạn
có thể thấy một file `.lock` cho mỗi file `authorized_keys`, `known_hosts`,
`access_requests` và `pairing_tokens`, giúp app và CLI lần lượt ghi, cùng các file `.tmp-…`
tồn tại rất ngắn, là nửa của một lần ghi atomic.

Không certificate nào được giữ lại. Certificate TLS được dựng từ `host_key.pem` trong bộ nhớ
mỗi lần port mở, nhưng thư viện TLS chỉ nạp certificate từ file, nên nó được ghi trong chốc
lát ra `transport_cert.<random>.pem` trong cùng thư mục, tạo với quyền `0600`, và bị xoá
ngay sau khi được nạp; mọi file như vậy cũ hơn một phút, thứ chỉ crash mới để lại, sẽ bị
dọn ở lần tiếp theo máy này bắt đầu host. File đó chỉ chứa certificate công khai: thư viện
TLS đọc private key từ `host_key.pem`.

Không có passcode nào được lưu ở bất cứ đâu. Trên hệ POSIX, thư mục được tạo với quyền
`0700` và mọi file trong đó `0600`, được ghi atomic; trên Windows, thư mục chỉ cho phép tài
khoản của bạn, SYSTEM và Administrators truy cập, và các file trong đó thừa hưởng hạn chế
ấy. Nếu không dùng được thư mục home, bản
build POSIX chuyển sang `$TMPDIR/.deskhub`, rồi tới `.deskhub` trong thư mục hiện hành. Bản
build debug dùng `.deskhub-dev` thay cho `.deskhub`, nên không bao giờ chạm vào key của bản
release. App di động lưu cùng các file đó trong sandbox riêng — trên iOS là thư mục
`.deskhub` bên trong app group container, dùng chung giữa app và broadcast extension của nó,
và được loại khỏi backup iCloud và backup trên máy tính; trên Android là trong internal
storage của app, với backup bị tắt cho toàn bộ app. Thư mục trên desktop không được loại trừ
như vậy: công cụ backup nào copy thư mục home của bạn cũng copy `host_key.pem` theo. Hãy coi
thư mục đó là nội dung mà mọi tiến trình chạy dưới tài khoản của bạn đều đọc được.

File `authorized_keys` hoặc `known_hosts` không đọc được sẽ không bị suy đoán: trong khi
file không đọc được, host không cho ai vào và client từ chối mọi host, và lần thay đổi tiếp
theo sẽ ghi mới file đó. Dữ liệu từ phiên bản cũ không được chuyển đổi: danh sách máy đã
pair cũ, activation marker cũ và file `auth_salt` cũ bị xoá, còn dòng `passcode=` trong một
`ui-settings.txt` cũ bị gỡ ở lần đầu file được nạp. Các file `client_key*.pem` và
`host_cert.pem` mà 7.0.x đã ghi bị bỏ qua, không bao giờ được đọc; bạn có thể xoá chúng nếu
muốn.

File do máy khác gửi tới được lưu ngoài thư mục đó, trong thư mục mà máy nhận đã chọn
(`Deskhub` trong thư mục home của người dùng nếu không chọn khác, lưu dưới tên
`transfer_dir`). Trên điện thoại hoặc tablet, các file này nằm trong thư viện ảnh hoặc thư
mục Documents / Downloads của thiết bị, và vẫn tồn tại sau khi gỡ app. Hãy coi mọi nội
dung được gửi tới đó là file do một client được phép đặt lên thiết bị của bạn.

Không dữ liệu nào trong số này được upload; bạn có thể xoá thư mục bất cứ lúc nào.

## Cứng hoá khi build và phát hành

- **Compiler và linker.** Bản build C và C++ bằng GCC và Clang dùng
  `-fstack-protector-strong`, và bản build tối ưu dùng thêm `-D_FORTIFY_SOURCE=3` — trừ
  Android, nơi giữ mức 2 mặc định của NDK; binary Linux, và chỉ chúng, được link với full
  RELRO (`-z relro -z now`). Bản build MSVC dùng `/sdl`. Các cờ này nằm trong
  `cmake/DeskhubHardening.cmake`.
- **macOS.** App được ký với hardened runtime nhưng chạy **ngoài App Sandbox**
  (`com.apple.security.app-sandbox` là `false`): app trong sandbox không thể gửi sự kiện
  keyboard và mouse vào app khác, mà đó chính là điều khiển từ xa. Vì vậy app chạy với mọi
  thứ tài khoản người dùng của bạn truy cập được, như mọi app không sandbox khác.
- **Windows.** App xin quyền administrator ở mỗi lần mở (xem phần trên), nên một lỗi trong
  app là lỗi trong một tiến trình quyền cao.
- **Tích hợp liên tục.** Mọi pull request đều chạy ba bộ test dưới AddressSanitizer,
  UndefinedBehaviorSanitizer và ThreadSanitizer, clang-tidy, các fuzzer mô tả ở trên, CodeQL
  trên code C++, Kotlin và Swift, và một lượt quét gitleaks trên toàn bộ lịch sử Git để tìm
  secret đã bị commit; CodeQL và gitleaks cũng chạy ở mọi lần push lên `main` và hằng tuần.
  Pull request thêm một dependency có lỗ hổng mức high đã biết sẽ trượt bước dependency
  review. Các GitHub Action bên thứ ba được ghim theo commit hash, và các linter cũng như
  scanner mà workflow tải về được kiểm tra với SHA-256 đã ghim.

## Các biện pháp giảm nhẹ đã lên kế hoạch

Đang theo dõi, theo thứ tự dự kiến triển khai:

1. **Lưu key của máy trong keychain của hệ điều hành** thay vì trong file.

Đã hoàn thành kể từ lần cập nhật danh sách gần nhất: quyền truy cập kiểu SSH — client chỉ
được chấp nhận qua public key có trong `authorized_keys` của host, mỗi connection được ký
lại từ đầu; đã gỡ passcode và switch pairing; đã gỡ LAN discovery, nên host không trả lời
bất kỳ packet plaintext nào; giới hạn số lần authenticate đang chờ cũng như số chữ ký sai;
rồi, trong 8.0, một key cho mỗi máy với certificate không còn được lưu, trust đi theo key
của host thay vì địa chỉ (việc từ chối dứt khoát khi key thay đổi trở thành hộp thoại
*New host* nêu tên chủ cũ), yêu cầu kết nối mà chủ host approve theo fingerprint qua kênh đã
authenticate, và pairing bằng QR với token một lần mà client chỉ gửi tới máy có fingerprint
được nêu trong mã; và trong 9.0, yêu cầu kết nối chỉ được ghi lại khi bên yêu cầu đã ký và tối đa một cho mỗi địa chỉ nguồn, host
tự đóng connection bị từ chối, deadline cho mọi connection chưa authenticate, hộp thoại xác
nhận *New host* trước khi trust một link mở từ bên ngoài app, key thiết bị được loại khỏi
backup iOS, không còn mã phím trong log Windows từ điều khiển từ xa hay terminal, file log
chỉ chủ sở hữu đọc được và được dọn bớt còn mười file mới nhất, rule firewall không còn xoá
rule chặn bạn đã tạo, và các cờ compiler cùng linker được cứng hoá.

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

Các giới hạn phía trên — tin cậy ở lần connect đầu, phân tích lưu lượng, QUIC handshake
vẫn nhìn thấy được và việc không chống DoS — đã được ghi nhận. Vui lòng báo cáo nếu bạn có bằng
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
