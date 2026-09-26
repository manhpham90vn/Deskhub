# TODO: Xác thực bằng khóa và loại bỏ LAN discovery

Ngày lập: 2026-09-27.

Trạng thái: đang triển khai; các ô chưa đánh dấu vẫn còn phải hoàn thành.

## 0. Đã làm trong đợt triển khai đầu

- [x] Thêm parser/formatter public key OpenSSH dạng một dòng cho Ed25519 và ECDSA P-256 trong `core/`. Parser kiểm tra base64, cấu trúc blob, loại khóa, giới hạn độ dài và từ chối dữ liệu thừa hoặc nhiều dòng.
- [x] Thêm chuyển đổi public key OpenSSH text sang SPKI dùng bởi handshake hiện tại, và xuất public key P-256 của identity hiện tại thành text trong `platform/`.
- [x] Cập nhật bước xác minh chữ ký để dùng digest phù hợp với Ed25519 hoặc ECDSA P-256.
- [x] Thêm CLI `devices public`, `devices add PUBLIC_KEY|-` (dấu `-` đọc stdin) và `trust add ADDRESS FINGERPRINT`. `devices add` hiện chuyển khóa text thành fingerprint rồi lưu vào `paired_devices` cũ.
- [x] Thêm API FFI lấy public key và thêm public key bằng text; trang Devices dùng chung macOS/iOS đã có ô dán khóa và hiển thị public key của máy.
- [x] Thêm test parser, test chuyển đổi khóa và test cú pháp CLI. `make test`, `make test-platform`, `make build-cli`, build macOS không ký, `make lint-cpp` và `make lint-swift` đã qua.

Các việc trên mới giúp trao đổi và nhập **public key**. Ứng dụng vẫn dùng chung khóa P-256 TLS làm identity xác thực, chưa import private key bên ngoài, chưa có host profile, chưa bắt buộc ghim khóa host trước khi kết nối. Passcode, popup approval và LAN discovery vẫn hoạt động. Chưa coi đây là cơ chế mới hoàn tất.

Gate chưa qua trọn vẹn: `make lint`/`make lint-dead` cần Java cho phần Kotlin trong môi trường hiện tại; build macOS có ký cần chứng chỉ phát triển. Build macOS không ký đã qua.

## 1. Phạm vi đã chốt

- [ ] Bỏ hoàn toàn passcode dùng để kết nối/pair, SPAKE2 và dữ liệu passcode được lưu.
- [ ] Bỏ popup duyệt kết nối, hàng đợi yêu cầu duyệt và chế độ tự chấp nhận máy lạ.
- [ ] Mọi kết nối phải xác thực bằng chữ ký từ private key tương ứng với public key đã được cấp quyền.
- [ ] Cho phép thiết bị tự tạo khóa hoặc import private key có sẵn bên ngoài.
- [ ] Host nhận public key của client bằng text. GUI có ô dán; CLI nhận stdin; service sau này đọc cùng cấu hình text.
- [ ] Client kết nối tới địa chỉ được cấu hình trước và kiểm tra khóa host đã ghim.
- [ ] Bỏ tìm máy trong LAN ở UI, CLI và đường xử lý discovery trên mạng.
- [ ] Dùng chung logic cho desktop, mobile, CLI và service tương lai.

Chỉ triển khai nền tảng cấu hình/xác thực để service sử dụng sau này. Việc tạo daemon, cài system service và quản lý vòng đời service nằm ngoài đợt này.

QR, gói mời, link đăng ký và file trao đổi thiết bị riêng không thuộc thiết kế. File cấu hình text vẫn cần cho CLI/service.

## 2. Luồng sử dụng bắt buộc

Giả sử A muốn kết nối tới B:

1. A tạo hoặc import private key; ứng dụng suy ra public key tương ứng.
2. A hiển thị public key dạng text một dòng để người dùng copy.
3. Chủ B dán public key đó vào danh sách được phép hoặc thêm bằng CLI. Thao tác này cấp quyền cho khóa A.
4. B cung cấp địa chỉ, cổng và public key/fingerprint của khóa TLS host bằng text.
5. Trên A, người dùng thêm host B bằng thông tin trên và chọn khóa của A dùng để kết nối.
6. A thiết lập QUIC/TLS và kiểm tra khóa host B trước khi gửi xác thực ứng dụng.
7. B kiểm tra chữ ký của A và quyền truy cập của khóa đó, sau đó mới cho phép dùng dịch vụ.

Public key và thông tin host phải được chuyển qua kênh mà chủ thiết bị tin cậy. Một public key tự ký hoặc lấy trực tiếp từ host chưa biết không tự tạo ra sự tin cậy.

- [ ] Không tự tin cậy host lần đầu dựa trên địa chỉ, tên máy hoặc việc TLS kết nối thành công.
- [ ] Host chưa được cấu hình khóa: dừng và trả lỗi hướng dẫn thêm host.
- [ ] Host đổi khóa: dừng và yêu cầu cập nhật chủ động trong cấu hình/Devices; không mở popup chấp nhận ngay khi Connect.
- [ ] Client chưa được cấp quyền: từ chối; không tạo yêu cầu duyệt.
- [ ] Public key chỉ định danh khóa. Tên/comment là nhãn hiển thị, không quyết định quyền truy cập.
- [ ] Mỗi chiều truy cập được cấp quyền riêng; A vào được B không tự cấp quyền B vào A.

## 3. Điểm xuất phát trong code

| Thành phần | File hiện tại | Hướng sửa |
| --- | --- | --- |
| Sinh khóa và chứng chỉ TLS P-256 | `platform/src/system/HostIdentity.cpp` | Giữ vai trò TLS, tách khóa xác thực client |
| Ký/xác minh và SPAKE2 | `platform/src/system/AuthProof.cpp` | Giữ/tổ chức lại chữ ký, bỏ phần passcode |
| Handshake xác thực | `platform/src/auth/AuthNegotiation.cpp` | Chỉ xác thực bằng public key đã cấp quyền |
| Giao thức | `core/include/deskhub/protocol/Wire.h`, `core/src/protocol/` | Version mới, message chỉ phục vụ cơ chế khóa |
| Chặn dữ liệu trước xác thực | `platform/src/net/SessionTransport.cpp` | Giữ admission theo kết nối, bỏ ngoại lệ discovery plaintext |
| Kiểm tra host | `platform/src/client/HostLink.cpp` | Bắt buộc ghim khóa, bỏ trust popup và tự trust qua passcode |
| Danh sách client/host | `core/src/net/PairedDevices.cpp`, `core/src/net/TrustStore.cpp` | Mô hình cấu hình và migration mới |
| Lưu cấu hình | `platform/src/system/PairedDevicesFile.cpp`, `platform/src/system/TrustStoreFile.cpp` | Ghi an toàn, reload, báo lỗi rõ |
| CLI | `core/src/cli/Command.cpp`, `client/cli/StoreCommands.cpp`, `client/cli/DiscoverCommands.cpp` | Quản lý khóa/host, bỏ scan/passcode |
| Discovery | `platform/src/client/LanScanner.cpp`, `platform/src/client/HostProbe.cpp`, `platform/src/ffi/DiscoveryFfi.cpp` | Xóa scanner/probe, chuyển phần quản lý host còn cần sang API phù hợp |
| Apple UI | `client/apple/swift/DevicesPage.swift`, `client/apple/swift/DiscoveryModel.swift` | Quản lý khóa và host cấu hình thủ công |

Logic thuần dữ liệu/giao thức đặt trong `core/`. Crypto, filesystem, credential store và transport đặt trong `platform/`. `client/` chỉ xử lý giao diện và tích hợp đặc thù OS. `core/` không phụ thuộc thư viện crypto hay header OS.

## 4. Mô hình khóa và định dạng cấu hình

### 4.1. Tách vai trò khóa

- [ ] Giữ khóa TLS host P-256 hiện có để tương thích quiche/BoringSSL đang dùng. Không đổi TLS sang Ed25519 trong đợt này.
- [ ] Thêm abstraction khóa xác thực client, độc lập với chứng chỉ TLS. Import khóa client không làm thay đổi khóa TLS host.
- [ ] Mặc định mỗi thiết bị có một khóa xác thực riêng; cho phép lưu nhiều khóa import và chọn khóa cho từng host.
- [ ] Hỗ trợ Ed25519 và ECDSA P-256 trong đợt đầu. RSA, FIDO/security key, SSH certificate và ssh-agent là phần mở rộng riêng.
- [x] Public key dùng text OpenSSH một dòng: `<key-type> <base64-key-blob> <optional-label>`.
- [ ] Private key import hỗ trợ OpenSSH và PKCS#8 cho các thuật toán đã nêu; từ chối rõ định dạng/thuật toán chưa hỗ trợ.
- [ ] Kiểm tra khóa private hợp lệ, suy ra public key và xác minh cặp khóa bằng ký/xác minh thử trước khi lưu.
- [ ] Dùng thư viện crypto/parser đã được duy trì; khảo sát khả năng thư viện hiện có trước khi chọn dependency đọc OpenSSH private key. Không tự xây thuật toán mật mã/KDF.
- [ ] Nếu private key import có passphrase, yêu cầu mở khóa trong luồng import. CLI không tương tác nhận qua stdin/file descriptor riêng và báo lỗi nếu thiếu; không truyền bí mật qua argv/log.
- [ ] Phân biệt passphrase bảo vệ file private key với passcode kết nối đã bị loại bỏ. Sau import, khóa được bảo vệ bằng cơ chế lưu cục bộ để kết nối không cần popup.

### 4.2. Ba nhóm cấu hình

Tên file và lệnh bên dưới là thiết kế dự kiến cần thống nhất với CLI hiện tại khi triển khai.

| Nhóm | Dữ liệu | Quy tắc |
| --- | --- | --- |
| Identity store | ID khóa, loại khóa, public key, tham chiếu private key được bảo vệ | Không trả private key qua API liệt kê |
| `authorized_keys` | Mỗi dòng một public key client, kèm comment tùy chọn | Danh sách rỗng từ chối tất cả |
| Host profiles | Alias, địa chỉ/cổng, khóa TLS host đã ghim, ID khóa client sử dụng | Không có pin thì không kết nối |

- [ ] Parser giới hạn kích thước, kiểm tra base64 và tính nhất quán giữa loại khóa khai báo với blob; phát hiện khóa trùng sau chuẩn hóa.
- [x] Parser chỉ nhận cú pháp public key đã định nghĩa; không âm thầm bỏ qua các option `authorized_keys` của OpenSSH chưa hỗ trợ.
- [ ] Chuẩn hóa fingerprint và ghi rõ encoding được hash. Fingerprint SPKI hiện tại khác fingerprint trên SSH key blob; không so sánh hai loại như cùng một giá trị.
- [ ] UI hiển thị rõ fingerprint khóa client và fingerprint TLS host để tránh copy nhầm.
- [ ] Hồ sơ host lưu alias riêng với địa chỉ; thay IP/cổng không tự thay khóa tin cậy.
- [ ] Host TLS public key/fingerprint có cách xuất text và nhập trước trên client; cả hai đầu dùng cùng quy ước fingerprint.
- [ ] Quyền truy cập gắn với khóa, không gắn IP. Chép cùng private key sang hai thiết bị sẽ khiến chúng dùng chung danh tính và bị thu hồi cùng nhau.

### 4.3. Lưu trữ và reload

- [ ] Tạo API lưu khóa riêng trong `platform/`, không ghi private key bằng helper ghi text chung hiện tại.
- [ ] Chọn backend lưu cục bộ cho từng OS: credential store hoặc mã hóa bằng khóa được OS bảo vệ; với file service phải giới hạn quyền đọc cho tài khoản chạy service.
- [ ] Xem xét iOS app/broadcast extension và desktop app/CLI dùng chung danh tính: backend phải hỗ trợ đúng app group/tài khoản, không vô tình tạo hai danh tính khác nhau.
- [ ] POSIX đặt quyền phù hợp cho thư mục/file nhạy cảm; Windows đặt ACL phù hợp. Tạo file tạm với quyền chặt ngay từ đầu.
- [ ] Ghi atomic, serialize thao tác sửa và xử lý app/CLI truy cập cùng cấu hình. Chỉ báo thành công sau khi persist thành công.
- [ ] Tách thư mục cấu hình có thể chỉ định để service không phụ thuộc tài khoản GUI hoặc thư mục log.
- [ ] Reload danh sách public key khi GUI/CLI cập nhật hoặc khi service yêu cầu reload. Đồng bộ thay đổi tới các transport đang chạy.
- [ ] File thiếu/hỏng không được dẫn tới cho phép tất cả. Reload lỗi phải báo rõ, từ chối admission mới và không báo đã áp dụng việc thu hồi khi chưa áp dụng được.
- [ ] Khóa đã tồn tại nhưng không đọc được phải trả lỗi; không âm thầm sinh khóa mới. Không log private key hoặc passphrase.

## 5. Handshake chỉ bằng khóa

- [ ] Định nghĩa phiên bản auth mới; client/server không hỗ trợ phải trả lỗi phiên bản và đóng kết nối. Không fallback về passcode, approval hay plaintext.
- [ ] `AuthStart` gửi thuật toán, public key client và tên hiển thị có giới hạn độ dài.
- [ ] Host kiểm tra public key hợp lệ và có trong danh sách được phép trước khi tiếp tục xác thực.
- [ ] Host phát challenge bằng CSPRNG, dùng một lần, có thời hạn và gắn với đúng kết nối.
- [ ] Định nghĩa transcript bằng encoding không mơ hồ, gồm nhãn riêng của Deskhub, version, vai trò, nonce, danh tính client và khóa TLS host.
- [ ] Ràng buộc chữ ký với phiên transport. Kiểm tra API quiche C hiện có có xuất TLS keying material hay không; nếu cần, bổ sung wrapper và test. Không coi IP/port là session binding.
- [ ] Client ký transcript bằng private key đã chọn; host xác minh bằng public key đã được cấp quyền.
- [ ] Kiểm tra lại quyền ngay trước khi chấp nhận, tránh khóa bị thu hồi trong lúc handshake vẫn được cho vào.
- [ ] Client chỉ nhận `Accepted` trong trạng thái đã hoàn thành các bước xác thực mong đợi; từ chối message sai thứ tự/lặp hoặc không thuộc kết nối hiện tại.
- [ ] Auth thành công chỉ có hiệu lực cho kết nối đó. Reconnect, resume hoặc QUIC session resumption không tự kế thừa admission; không chạy thao tác đặc quyền bằng 0-RTT trước auth.
- [ ] Không cho phép screen, input, clipboard, terminal, file transfer hoặc danh sách tài nguyên trước khi auth hoàn tất.
- [ ] Thu hồi khóa đóng tất cả kết nối của khóa đó và chặn lần kết nối tiếp theo, kể cả khi cập nhật bằng CLI/service.
- [ ] Giới hạn thời gian/số handshake đang chờ và lưu lượng auth thất bại; thay cơ chế throttle passcode bằng giới hạn phù hợp với auth bằng khóa.
- [ ] Trả mã lỗi ổn định: host chưa tin cậy, host đổi khóa, khóa client chưa được cấp quyền, chữ ký sai, khóa cục bộ không dùng được, version không tương thích, timeout, lỗi cấu hình.

## 6. GUI, CLI và API cho service

### 6.1. GUI

- [ ] Trang “Khóa của tôi”: tạo khóa, import private key, xem/copy public key và fingerprint.
- [ ] Trang “Thiết bị được phép”: dán public key text, đặt nhãn, liệt kê và thu hồi khóa.
- [ ] Trang “Host đã lưu”: nhập alias, địa chỉ/cổng, khóa host tin cậy và chọn khóa client.
- [ ] Kết nối từ danh sách host đã lưu; hiển thị lỗi trong trạng thái kết nối, không mở popup xin duyệt/trust.
- [ ] Cập nhật Swift dùng chung macOS/iOS, Android, Linux và Windows cùng một hành vi.
- [ ] Xóa trường passcode, nút scan, switch cho phép pair máy mới và các màn hình yêu cầu duyệt.
- [ ] Chỉ lưu/hiển thị “last seen” dựa trên tương tác đã xác thực; không probe plaintext để cập nhật trạng thái danh sách.

### 6.2. CLI dự kiến

```text
deskhub-cli key generate --name laptop-a
deskhub-cli key import --name imported-key --file /path/to/private-key
deskhub-cli key public --name laptop-a
deskhub-cli access add --name laptop-a --stdin
deskhub-cli access list --json
deskhub-cli access remove --fingerprint <fingerprint>
deskhub-cli host-key public
deskhub-cli host add office --address <address:port> --identity laptop-a --host-key-stdin
deskhub-cli host list --json
deskhub-cli connect office
```

- [ ] Đối chiếu tên lệnh với parser hiện tại rồi thống nhất help, JSON schema và exit code. Các ví dụ trên chưa phải lệnh có sẵn.
- [ ] `access add --stdin` nhận public key dạng text; `host add --host-key-stdin` nhận khóa TLS host theo định dạng đã quy định.
- [ ] Cung cấp thao tác cập nhật/xóa host và đổi khóa ghim chủ động, có cùng API cho GUI.
- [ ] Hỗ trợ `--config-dir` hoặc cơ chế tương đương dùng nhất quán cho mọi lệnh; trả lỗi rõ khi quyền đọc/ghi không đủ.
- [ ] Public API trong `platform/` không gọi UI, không đọc stdin, không phụ thuộc event loop GUI. CLI/GUI cung cấp dữ liệu cho API.
- [ ] `connect`, `sources`, `shell`, `send` và các luồng truy cập khác đều dùng host profile, identity và trust policy chung.
- [ ] Bỏ `scan`, tùy chọn passcode, biến môi trường passcode, cơ chế approval/auto-allow và tùy chọn bỏ qua kiểm tra host key.
- [ ] Nếu giữ lệnh `probe`, chuyển nó sang kết nối tới host được chỉ định với kiểm tra khóa và auth đầy đủ; không còn probe UDP công khai.

## 7. Loại bỏ LAN discovery

- [ ] Xóa `LanScanner` và các callback/thread/timer quét mạng, gồm tự scan khi mở app và rescan định kỳ.
- [ ] Rà `HostProbe`, `DiscoveryFfi`, `DiscoveryModel`, UI từng OS và CLI để bỏ dependency vào kết quả scan.
- [ ] Tách logic recent/saved host còn cần ra khỏi `DiscoveryFfi`; không xóa nhầm chức năng lưu host thủ công.
- [ ] Bỏ ngoại lệ nhận beacon plaintext trong `SessionTransport::Deliver` và đường gửi/trả lời discovery plaintext tương ứng.
- [ ] Host không trả `SOURCE_LIST` hay `PONG` cho các probe discovery chưa xác thực, kể cả trả danh sách rỗng.
- [ ] Sửa `RunSources` và các luồng kết nối khác đang gọi `ProbeHostRttMs`: đi thẳng vào QUIC/TLS, kiểm tra host, auth rồi mới query nguồn.
- [ ] Rà `Beacon`, `HostNetLoop` và `ViewerBroadcast`: tách phần liệt kê nguồn trong phiên đã auth trước khi xóa chức năng discovery. Không xóa theo tên “Broadcast” vì còn broadcast media tới viewer hợp lệ.
- [ ] Giữ `ListSources/SourceList` khi cần cho danh sách màn hình sau auth; giữ ping/pong, heartbeat, RTT trong phiên hợp lệ.
- [ ] Bỏ import, file build, FFI, string ID, bản dịch và test chỉ phục vụ scan LAN.
- [ ] Kiểm tra bằng packet capture: app idle không quét subnet; host không trả lời gói discovery Deskhub cũ; kết nối cấu hình thủ công vẫn hoạt động.

Bỏ discovery giảm dữ liệu công khai và đường xử lý trước auth. QUIC vẫn cần gói handshake mạng trước auth ứng dụng; cổng lắng nghe không trở nên vô hình trước quét mạng.

## 8. Chuyển đổi dữ liệu và tương thích

- [ ] Giữ khóa TLS host hiện tại nếu hợp lệ để tránh đổi danh tính host ngoài ý muốn.
- [ ] Version hóa cấu hình mới; migration có thể chạy lại an toàn, chỉ đánh dấu hoàn tất sau khi ghi thành công.
- [ ] `paired_devices` cũ chỉ lưu fingerprint, không có public key để xuất thành `authorized_keys`. Không chuyển fingerprint thành một public key giả hoặc tự cấp quyền cho khóa do mạng cung cấp.
- [ ] Cung cấp hướng dẫn/lệnh migration chủ động: xuất public key từ identity cũ trên từng client và thêm lại trên host, hoặc tạo khóa client mới rồi cấp quyền.
- [ ] Có thể tái sử dụng khóa P-256 cũ cho danh tính client chuyển tiếp nếu parser mới hỗ trợ; phải là lựa chọn rõ ràng để sau này tách/đổi khóa client không làm đổi TLS host.
- [ ] Giữ các pin host cũ nếu chuyển đổi được với đúng encoding fingerprint; đánh dấu rõ pin legacy và không làm mất kiểm tra khóa khi đọc cấu hình cũ.
- [ ] Địa chỉ recent không kèm pin chỉ được chuyển thành mục chưa cấu hình xong; không tự trở thành host tin cậy.
- [ ] Xóa passcode khỏi settings/recent store, CLI/env, FFI, log và serialization; không tạo bản backup mới chứa passcode/private key dạng rõ.
- [ ] Thông báo tương thích rõ cho client/server cũ. Bản mới không cho phép cơ chế cũ để giữ tương thích.
- [ ] Khi danh sách khóa mới chưa được cấu hình, host từ chối truy cập và hướng dẫn quản trị viên thêm public key cục bộ.

## 9. Kiểm thử và tiêu chí nghiệm thu

### 9.1. Unit và parser

- [ ] Parse/serialize public key text, chuẩn hóa, phát hiện duplicate, comment, ký tự điều khiển, dữ liệu hỏng/quá lớn và thuật toán không hỗ trợ.
- [ ] Kiểm thử fingerprint SSH/SPKI và migration để không lẫn encoding.
- [ ] Test vector import OpenSSH/PKCS#8 thực tế cho Ed25519/P-256, gồm khóa mã hóa, sai passphrase, dữ liệu cắt ngắn và cặp khóa không hợp lệ.
- [ ] Test ghi cấu hình thất bại, reload lỗi, file thiếu và cập nhật đồng thời. Không tác động cấu hình thật của người dùng khi chạy test.
- [ ] Cập nhật `WireTests`, `CommandTests`, `AuthProofTests`, `HostIdentityTests`, `AuthNegotiationTests` và các test trust/store liên quan.

### 9.2. Transport và integration

- [ ] Đúng khóa và đúng pin host kết nối được; chỉ biết public key không thể giả danh client.
- [ ] Chữ ký sai, sai challenge, replay cùng/khác kết nối, hết hạn, đổi public key giữa handshake và message sai thứ tự đều bị từ chối.
- [ ] Host chưa biết/đổi khóa bị chặn; tên máy, IP giống nhau hoặc certificate tự ký không vượt qua pinning.
- [ ] Khóa bị thu hồi trong lúc handshake hoặc đang có nhiều phiên không tiếp tục truy cập được.
- [ ] Không đọc nguồn màn hình, mở terminal, gửi file, input hay clipboard trước auth.
- [ ] Reconnect/resume/session resumption phải xác thực lại đúng policy.
- [ ] Gói discovery plaintext cũ không nhận phản hồi; ping/pong và source query sau auth vẫn hoạt động.
- [ ] Kiểm tra kết nối thủ công trên macOS, Windows, Linux, Android, iOS và CLI; service được mô phỏng bằng caller không có UI dùng cùng API/config.
- [ ] Fuzz parser key/config và message auth mới; cập nhật seed corpus và xóa vector chỉ dành cho giao thức cũ khi phù hợp.

### 9.3. Gate hoàn tất

- [ ] Chạy `make test`, `make lint` và `make test-all`; sửa mọi lỗi liên quan.
- [ ] Chạy `make lint-tidy` và các build/test CI đa nền tảng liên quan thay đổi crypto/FFI/UI. Ghi rõ gate nào chưa chạy được cục bộ.
- [ ] Kiểm tra packet capture và cấu hình thực tế theo các tiêu chí ở trên.
- [ ] Rà lại repo để không còn hành vi passcode, approval popup, auto-trust hoặc LAN scan trong sản phẩm; tránh xóa nhầm passphrase import khóa và heartbeat.

## 10. Tài liệu và thứ tự triển khai

- [ ] Cập nhật README, SPECIFICATION, ARCHITECTURE, SECURITY, CLI help và hướng dẫn cài đặt/cấu hình bị ảnh hưởng.
- [ ] Cập nhật PRIVACY về dữ liệu khóa/host được lưu và loại bỏ discovery/passcode; theo quy ước tài liệu hiện có, cập nhật version/ngày hiệu lực/changelog khi thay đổi hành vi lưu hoặc truyền dữ liệu.
- [ ] Đồng bộ các tài liệu sản phẩm được sửa ở EN/VI/ZH/JA; thay ảnh chụp còn scan/passcode nếu cần.
- [ ] Hướng dẫn tạo/import khóa, thêm public key text, ghim khóa host, chạy CLI không tương tác, thu hồi/đổi khóa và chuyển dữ liệu cũ.

Triển khai theo các đợt phụ thuộc sau:

1. Chốt schema, fingerprint, thuật toán/định dạng, backend lưu khóa và phương án session binding; tạo test vector.
2. Xây key store, parser text, authorized key store và host profile API dùng chung.
3. Thay handshake và trust policy; xác minh kết nối thực qua QUIC bằng integration test.
4. Nối UI/CLI/FFI vào API mới, bổ sung migration và thu hồi phiên.
5. Gỡ passcode/approval/discovery, sửa các call site probe và build manifests.
6. Hoàn tất test đa nền tảng, kiểm tra traffic, tài liệu và hướng dẫn nâng cấp.

Không phát hành trạng thái trung gian cho phép bỏ qua kiểm tra khóa hoặc tự động chuyển về phương thức xác thực cũ.
