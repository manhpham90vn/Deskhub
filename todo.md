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
- [x] Tách identity xác thực client khỏi khóa TLS host: tự tạo khóa Ed25519 riêng, giữ ổn định giữa các lần chạy và báo lỗi nếu file khóa hiện có bị hỏng.
- [x] Thêm import private key PEM/PKCS#8 Ed25519 hoặc ECDSA P-256 qua `devices import FILE`; khóa có passphrase nhận qua `--passphrase-stdin`, không truyền bí mật qua argv. Import tự ký và xác minh thử trước khi lưu; file khóa mới được ghi tạm với quyền POSIX `0600` rồi thay thế atomic.
- [x] `devices public` và API FFI nay xuất public key của identity client riêng; `trust public` xuất public key TLS host. `trust add ADDRESS -` nhận public key host qua stdin và suy ra fingerprint SPKI.
- [x] Client từ chối host chưa có pin hoặc đổi khóa trước khi gửi auth; cập nhật pin phải thực hiện chủ động qua cấu hình/lệnh trust.
- [x] Chặn nhánh xác thực bằng passcode và nhánh Approval tại handshake: host chỉ phát challenge chữ ký cho fingerprint đã cấp quyền, client từ chối mode cũ; host không phát callback tạo yêu cầu popup duyệt client. Kiểm tra lại quyền ngay trước khi nhận chữ ký. Đây mới là chặn ở tầng giao thức, chưa xóa hết UI/API và dữ liệu passcode cũ.
- [x] Cập nhật test handshake, HostLink, terminal, FFI và gửi file: cấp khóa client/ghim khóa host trước kết nối; kiểm tra client lạ và host đổi khóa bị từ chối, không mở yêu cầu duyệt hay tự cập nhật pin. Bản hiện tại qua `make test`, `make lint-cpp`, `make build-cli` và toàn bộ binary `platform_tests` có socket loopback.
- [x] Gỡ UI popup duyệt client và chấp nhận khóa host khi kết nối trên Apple, Android, Linux và Windows; gỡ prompt duyệt trong CLI. Màn gửi file trả lỗi khi khóa host đổi, không hiện nút chấp nhận để thử lại.
- [x] Gỡ công tắc cho phép pair máy lạ khỏi trang Devices trên Apple, Android, Linux và Windows. Bỏ đường Android JNI lấy/trả lời yêu cầu pairing, gồm nhánh tự chấp nhận máy đã từng pair; API FFI và dữ liệu cũ còn phải dọn tiếp.
- [x] Xóa hàng đợi approval trong `SharingHost`, callback và lệnh trả lời approval từ transport/host engine, FFI lấy/trả lời yêu cầu, cùng text popup pairing không còn dùng. Cập nhật hướng dẫn ở trang Devices: client được cấp quyền bằng public key nhập chủ động. Platform tests với QUIC loopback đã qua.
- [x] Chạy `make lint-dead`; xóa string ID, hằng số Kotlin, FFI helper và test chỉ phục vụ popup/approval đã bỏ. Xóa API passcode verifier/salt, SPAKE2 và MAC khỏi `platform/` vì handshake chữ ký không dùng chúng. Cập nhật script dead code để quét cả file mới chưa được Git track và bỏ qua file đã xóa; phần C++/FFI/string/Kotlin constants không còn finding.
- [x] Đóng đường nhận discovery plaintext ở QUIC endpoint và transport; `Beacon` không trả `SOURCE_LIST`/`PONG` cho peer chưa auth. CLI `sources`/`connect` không gửi probe UDP trước auth. Bỏ lệnh CLI `scan`; ngừng auto scan và status probe khi mở UI Apple, Android, Linux, Windows.
- [x] Xóa `LanScanner`, `HostProbe`, `DeviceStatusPoller`, logic chọn địa chỉ subnet, FFI/JNI quét và thăm dò, callback/timer scan còn sót ở Linux/Windows. Danh sách thiết bị giờ lấy từ recent đã lưu; status/ping không còn suy đoán bằng probe trước auth. Xóa test scanner/probe cũ; `make test`, `make build-cli`, `make lint-cpp`, `make lint-dead`, `make test-platform` và build macOS không ký đã qua sau đợt dọn này.
- [x] Xóa CLI `probe` vì thời gian đo toàn bộ truy vấn nguồn không phải RTT và `--timeout` cũ không được áp dụng. CLI `sources` vẫn truy vấn sau khi ghim khóa host và xác thực. Integration harness cấp quyền khóa client và ghim khóa host trước khi chạy; thay kịch bản passcode/approval cũ bằng test thu hồi khóa client. `make test-integration` đã qua.
- [x] Ngừng đọc/ghi passcode và cờ `allow_new_pairings` trong file UI settings; không tự sinh passcode khi nạp settings. Recent devices giữ địa chỉ từ file cũ nhưng bỏ passcode khi parse, touch và serialize. CLI `settings` không còn liệt kê hay xuất passcode. Xóa helper mã hóa/bộ sinh passcode nay không còn caller trong sản phẩm cùng test cũ của chúng. `make test`, `make test-platform`, `make build-cli`, `make lint-cpp` và `make lint-dead` đã qua; `make lint` bị chặn ở ktlint do thiếu Java runtime. Các API/field passcode tạm thời vẫn tồn tại ở các tầng khác và cần dọn tiếp.
- [x] Khi nạp file settings/recent cũ, ghi lại nội dung đã bỏ passcode bằng file tạm quyền `0600` trên POSIX rồi thay thế atomic; ghi lỗi giữ nguyên file cũ và lần nạp sau thử lại. Ghi settings/recent thông thường cũng dùng cùng đường ghi an toàn. Test migration settings dùng thư mục riêng trong `/tmp`.
- [x] Cập nhật `PRIVACY` phiên bản 2.6 ở bốn ngôn ngữ cho khóa client riêng, khóa TLS host, xác thực bằng khóa và cách làm sạch passcode settings/recent. Còn phải rà toàn bộ tài liệu sản phẩm khi mô hình host profile và `authorized_keys` hoàn tất.
- [x] Gỡ `--passcode`, `--pairing`, `--no-new-pairings` và `DESKHUB_PASSCODE` khỏi CLI, cùng parser, nguồn passcode stdin/file, help và passcode note; lệnh cũ trả lỗi tùy chọn không biết. Xóa cờ `allowNewPairings` không còn tác dụng khỏi settings/share options và cập nhật test. Cập nhật checklist CLI trong `SECURITY` bốn ngôn ngữ. Test terminal FFI dùng `/bin/sh` trên POSIX và in kết quả trên dòng riêng để không phụ thuộc wizard/prompt của shell mặc định.
- [x] Xóa throttle passcode không còn đường chạy, tham số kết quả `hostProvedPasscode` luôn false và trường passcode khỏi cấu hình auth client. Client báo `NotPaired` khi host từ chối khóa chưa được cấp quyền, thay cho `PairingDisabled`; dọn test transport/terminal tương ứng. Giới hạn auth bằng khóa vẫn cần thiết kế và triển khai riêng.
- [x] Định dạng bản tin auth có version riêng (`kAuthVersion = 6`): bỏ mode passcode/approval, salt, SPAKE2, MAC xác nhận và các mã lỗi passcode khỏi wire. `AuthStart` giữ byte tiền tố cố định bằng 0 và đặt version sau tên để client mới nhận được challenge từ host cũ rồi báo lỗi version; host mới trả `VersionMismatch` và đóng kết nối khi nhận start cũ. Thêm test parser, wire vectors và QUIC loopback cho cả hai chiều không tương thích.
- [x] Chuyển transcript chữ ký sang `core/auth`: mã hóa trường có độ dài rõ ràng và đưa domain, auth version, vai trò client, định danh phiên xuất từ QUIC/TLS, public key client và fingerprint khóa TLS host vào dữ liệu ký. Bổ sung TLS exporter vào C API của quiche 0.29.3 bằng bản vá được áp dụng khi build; bỏ nonce và thời hạn challenge riêng. Host chỉ nhận một chữ ký trên mỗi kết nối, và chữ ký của phiên khác không hợp lệ.
- [x] Siết trạng thái auth client: chỉ xử lý bản tin từ đúng peer, từ chối `Accepted` trước khi ký, challenge lặp và bản tin auth sai thứ tự; xóa auth inbox cũ trước một lần xác thực mới. Thêm test host gửi `Accepted` quá sớm qua QUIC loopback; `make test-platform` và `make lint` đã qua.
- [x] Giới hạn tối đa 8 phiên auth đang chờ chữ ký và đóng kết nối nếu quá 10 giây; xóa slot khi xác thực xong hoặc kết nối đóng. Test QUIC loopback xác nhận phiên thứ 9 bị từ chối, phiên im lặng hết hạn và slot được tái sử dụng.
- [x] Giới hạn chữ ký sai theo cặp public key client và IP nguồn: ba lần sai trong một phút chặn cặp đó 10 giây; chữ ký đúng xóa lỗi. Bộ đếm tối đa 64 cặp trong bộ nhớ, không ảnh hưởng khóa khác cùng IP. Unit test kiểm tra ngưỡng, thời hạn, tách khóa/IP và giới hạn bộ nhớ; QUIC loopback xác nhận lần thử thứ tư bị đóng trước khi xác minh chữ ký.
- [x] Ghi danh sách khóa được phép bằng thay thế atomic. Chỉ tăng generation khi lưu thành công; thao tác thu hồi toàn bộ ghi danh sách rỗng và CLI báo lỗi nếu không persist được. Test riêng kiểm tra lỗi ghi không được công bố là thay đổi quyền.
- [x] Danh tính TLS host chỉ được tạo khi cả file certificate và private key đều chưa tồn tại. Nếu file đã lưu hỏng, không hỗ trợ hoặc không khớp nhau, host từ chối khởi động mà không tự thay khóa; khi tạo mới ghi mỗi file bằng thay thế atomic với quyền tạm chặt. Test gồm cặp khóa không khớp, thiếu file và symlink hỏng.
- [x] Bổ sung identity client có tên mà không thay `client_key.pem` mặc định: API tạo/import/nạp/liệt kê chỉ trả public key khi liệt kê; tên được giới hạn để không thoát thư mục. `HostLink` và các viewer nhận tên khóa, CLI hỗ trợ `devices generate`, `devices import --name`, `devices identities`, `devices public NAME` và `--identity NAME` cho `sources`/`connect`/`shell`/`send`. Host profile và import OpenSSH đã bổ sung sau đó; backend bảo vệ khóa theo OS vẫn chưa xong.
- [x] Chặn mọi đường gửi record/datagram ứng dụng của host trước khi kết nối được auth; auth challenge/result dùng đường gửi nội bộ. Test QUIC xác nhận kết nối đã thiết lập nhưng chưa auth không nhận được dữ liệu host. Trust store nay ghi atomic và khóa thao tác đọc-sửa-ghi trong cùng tiến trình; cập nhật quyết định này ở bốn bản kiến trúc.
- [x] File `paired_devices` khi hỏng, đọc lỗi, trùng khóa hoặc vượt giới hạn sẽ từ chối toàn bộ admission; thao tác thêm/thu hồi thường không ghi đè file hỏng. Host kiểm tra lại danh sách theo chu kỳ ngắn để thu hồi cả khi tiến trình CLI khác sửa file mà generation nội bộ không đổi. Test parser và QUIC loopback đã bổ sung.
- [x] Trust pin nay chỉ có hiệu lực cho đúng địa chỉ/cổng đã lưu; cùng khóa TLS ở địa chỉ khác vẫn là host chưa tin cậy cho tới khi được ghim riêng. Test core và quyết định kiến trúc bốn ngôn ngữ đã cập nhật.
- [x] Thêm parser `authorized_keys` chứa public key text đầy đủ, giới hạn file và từ chối khóa trùng/dòng hỏng. Khi bật, file này thay danh sách fingerprint cũ; dấu kích hoạt ngăn xóa file làm sống lại quyền cũ. Host, CLI, FFI và danh sách Linux/Windows dùng chính sách mới; test migration và quyền rỗng đã bổ sung.
- [x] Host profile trong `known_hosts` lưu alias và tên khóa client cạnh địa chỉ/cổng và pin TLS. `trust add --name --identity` ghi cùng một lần; `HostLink` tự chọn khóa đã lưu nếu không có cờ ghi đè. Trust file hỏng từ chối toàn bộ pin; các file cấu hình quyền/trust được khóa liên tiến trình khi sửa và ghi bằng thay thế atomic.
- [x] `trust list` báo lỗi nếu `known_hosts` hỏng thay vì hiển thị danh sách rỗng. `trust forget all` nạp và kiểm tra file dưới file lock trước khi ghi danh sách rỗng, nên không ghi đè file hỏng; thông báo CLI sau khi thu hồi không còn ngụ ý host sẽ được tự tin cậy khi kết nối lại. Sửa test admission chờ mã `BadSignature` theo giao thức hiện tại. `make build-cli`, `make lint-cpp` và platform tests trên macOS qua sau khi áp dụng patch quiche cục bộ và chạy socket loopback ngoài sandbox.
- [x] Thao tác xóa toàn bộ `authorized_keys` hoặc `paired_devices` cũ kiểm tra file dưới khóa trước khi ghi; file đang kích hoạt nhưng bị thiếu/hỏng được giữ lại để sửa và thao tác thất bại không tăng generation. Bổ sung test reload lỗi và xóa toàn bộ. Test reconnect qua QUIC xác nhận phiên mới không kế thừa admission và phải ký lại; QUIC hiện không bật 0-RTT hay dùng session ticket.
- [x] Import OpenSSH private key Ed25519/P-256 qua parser RustCrypto `ssh-key` tích hợp trong patch quiche; hỗ trợ khóa mã hóa bằng passphrase nhập qua stdin. Test bằng khóa `ssh-keygen` thật, gồm passphrase sai, khóa RSA không hỗ trợ, dữ liệu cắt ngắn và cặp khóa không khớp. Bộ test platform, `make test`, `make lint-cpp`, build CLI và build quiche cho iOS arm64 đã qua.
- [x] Hoàn tất chính sách lưu khóa ở mục 4 theo lựa chọn file riêng cho host nền macOS/Linux: thư mục cấu hình POSIX kiểm tra theo file descriptor, Windows có ACL riêng; mọi đường ghi app data nay ghi tạm với quyền chặt rồi thay thế atomic. Broadcast iOS không tạo identity ngoài App Group khi container lỗi. Build iOS Simulator và Android Debug, platform tests, lint C++/Swift/Kotlin đã qua; build/test Windows cần gate Windows ở mục 9.
- [x] Gỡ passcode khỏi mọi tầng còn lại: `HostLinkConfig`/`ScreenViewerConfig`/`TerminalViewerConfig`/`FileTransferClientConfig`, `QuerySources`, `ShareOptions`, `UiSettings`, `RecentDevice`, `SettingField`, chuỗi UI và hằng số `kPasscodeDigits`/`IsValidPasscode`. FFI bỏ tham số passcode của `dh_list_sources`, `dh_screen_start`, `dh_send_start`, `dh_share_start`, `dh_term_open*`, `dh_recent_touch`, `dh_settings_save`, `dh_sharing_status`; xóa `dh_is_valid_passcode`, `dh_passcode_digits`, `dh_recent_passcode`, `dh_passcode_display` và string ID passcode. Màu thẻ đổi tên thành `InfoCard` cho thẻ cổng. Linux, Windows, Android, macOS/iOS bỏ ô passcode khi kết nối, thẻ passcode trên trang chia sẻ, dòng passcode trong Settings và hộp thoại nhập passcode; bấm thiết bị trong danh sách kết nối thẳng tới địa chỉ đã lưu. Sửa golden vector auth trong integration test theo `kAuthVersion = 6`; xóa `CheckPairedDevice`, `LoadPairedDevices`, string ID `ConnectPromptTitle`/`SettingsSectionSecurity` không còn nơi dùng; sửa ba lỗi clang-tidy ở `Transcript.cpp` và `ClientIdentity.cpp`. Mô tả App Store/Play Store chuyển sang cấp quyền bằng khóa. Trên Linux đã qua `make test`, platform tests, integration tests, `make lint`, `make lint-tidy`, `make build-linux` và `make build-android` (kèm Android Lint); Windows và macOS/iOS chưa build được cục bộ. GUI chưa có cách ghim khóa host (mục 6.1), nên client GUI chưa kết nối được tới host mới cho tới khi có trang “Host đã lưu”.
- [x] Đưa logic host profile ra khỏi CLI: `core/ui/HostProfiles` kiểm tra tên, chuẩn hóa địa chỉ `IP:port`, chống trùng tên/địa chỉ và giữ trường không đổi khi cập nhật; `platform/client/HostProfiles` đọc/ghi `known_hosts`, nhận khóa host dạng fingerprint `SHA256:` hoặc public key một dòng và kiểm tra khóa client tồn tại. CLI `host add/update/remove` và FFI mới `HostProfileFfi.h` dùng chung đường này. Trang Devices trên Linux, Windows, Android và macOS/iOS có mục “Saved hosts”: liệt kê, thêm (tên, địa chỉ, khóa host, chọn khóa client), xóa và kết nối thẳng từ một host đã lưu. Lỗi kết nối giờ hiện đúng nguyên nhân từ `HostLink` (không tới được, host chưa ghim, khóa host đổi, khóa client chưa được cấp quyền) thay cho mã `NotPaired` mặc định; `dh_list_sources` trả câu lỗi. `scripts/cli-smoke.sh` viết lại theo luồng khóa, thêm ca host chưa ghim và khóa chưa được cấp quyền. Trên Linux đã qua ba bộ test, `make lint`, `make lint-tidy`, cli-smoke, `make build-linux`, `make build-android`; Windows và macOS/iOS chưa build được cục bộ.

Đã có nhiều identity client riêng, host profile và import OpenSSH/PKCS#8. Luồng auth mới không mở popup xác nhận kết nối; hàng đợi và API trả lời approval đã được gỡ. Passcode đã được gỡ khỏi code sản phẩm (chỉ còn đường migration xóa khóa `passcode=` trong file settings cũ); các message beacon discovery còn trong code và phải gỡ tiếp. File settings/recent cũ được làm sạch khi nạp thành công; nếu ghi thất bại, file cũ còn nguyên để thử lại. Migration cấu hình mới có version vẫn cần làm.

Gate chưa qua trọn vẹn: build macOS có ký cần chứng chỉ phát triển. Sau khi chuyển wire auth sang version 6 và ràng buộc chữ ký với phiên QUIC/TLS, `make test`, `make lint`, `make test-platform`, `make test-integration` và `make build-cli` đã qua trên Linux. Lần chạy platform đầu sau thay đổi còn lỗi ở test terminal thiếu định danh phiên và đã sửa; một lần chạy kế tiếp lỗi timeout ở test ping/reconnect, lần lặp lại qua. Lỗi terminal FFI trước đó được truy ra shell mặc định mở wizard zsh trong HOME thử nghiệm; test đã dùng `/bin/sh` và output riêng dòng. Build macOS không ký đã qua ở lượt trước, chưa chạy lại sau thay đổi này.

## 1. Phạm vi đã chốt

- [x] Bỏ hoàn toàn passcode dùng để kết nối/pair, SPAKE2 và dữ liệu passcode được lưu. Tài liệu sản phẩm còn mô tả passcode và được xử lý ở mục 10.
- [x] Bỏ popup duyệt kết nối, hàng đợi yêu cầu duyệt và chế độ tự chấp nhận máy lạ. Cờ `allow_new_pairings` còn trong settings cũ nhưng không được handshake đọc; xóa cùng migration settings.
- [x] Mọi kết nối phải xác thực bằng chữ ký từ private key tương ứng với public key đã được cấp quyền.
- [x] Cho phép thiết bị tự tạo khóa hoặc import private key có sẵn bên ngoài (GUI “My keys” và CLI `key generate/import`).
- [x] Host nhận public key của client bằng text. GUI có ô dán; CLI nhận stdin; service sau này đọc cùng cấu hình text.
- [x] Client kiểm tra khóa host đã ghim trước khi xác thực; host mới được ghim theo quyết định TOFU (xác nhận fingerprint hoặc `--accept-new-host-key`), khóa đổi bị chặn.
- [x] Bỏ tìm máy trong LAN ở UI, CLI và đường xử lý discovery trên mạng.
- [x] Dùng chung logic cho desktop, mobile, CLI và service tương lai.

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

**Quyết định 2026-09-29 — tin host theo kiểu SSH (TOFU):** lần kết nối đầu tới một host chưa có trong `known_hosts`, GUI hiện fingerprint `SHA256:` của khóa TLS host và chỉ lưu khi người dùng xác nhận; CLI từ chối, in fingerprint và chỉ lưu khi có `--accept-new-host-key` (tương đương `StrictHostKeyChecking=accept-new`). Khóa host đổi luôn bị chặn cứng, không có nút bỏ qua; muốn kết nối lại phải xóa host trong “Host đã tin”. Dán khóa host trước (`host add --host-key-stdin`, form trong GUI) vẫn được giữ cho ai cần tránh MITM ở lần đầu. Quyết định này thay các mục “không tự tin cậy host lần đầu” và “không mở popup trust” bên dưới.

**Bố cục trang Devices:** chia hai khối theo vai. “Khi máy này là host”: khóa host của máy (fingerprint + Copy) và danh sách client được phép (dán public key, thu hồi). “Khi máy này là client”: “Khóa của tôi” (danh sách khóa, Copy public key từng khóa, tạo khóa mới, import) và “Host đã tin” (known_hosts: kết nối, xóa). GUI không có form thêm host thủ công; ghim trước và đổi tên/khóa client của host làm qua CLI `host add/update`.

- [x] Triển khai TOFU và bố cục Devices mới trên core/platform/CLI và cả 5 app: `QuerySources` trả fingerprint của host lạ, `TrustNewHost` lưu host với tên sinh từ địa chỉ, `HostLinkConfig.acceptNewHostKey` và cờ CLI `--accept-new-host-key`; khóa host đổi vẫn bị chặn kể cả khi có cờ. API tạo/import khóa client có mã lỗi (`deskhubp/system/ClientKeys.h`, `ClientKeyFfi.h`), `LoadClientKeys` luôn đảm bảo có khóa `default`, và `HostLink` tự tạo khóa `default` khi hồ sơ host trỏ tới nó. Bỏ `dh_own_public_key`, `dh_own_fingerprint`, `dh_client_identity_names`, `dh_host_profile_add`. Trên Linux đã qua ba bộ test, cli-smoke, `make lint`, `make lint-tidy`, `make build-linux`, `make build-android`; Windows và macOS/iOS chưa build được cục bộ.
- [x] Bỏ form thêm host thủ công khỏi GUI (Trusted hosts chỉ còn Connect/Remove; ghim trước qua CLI). Public key copy ra có nhãn giống SSH: `ssh-ed25519 AAAA… <tên thiết bị>` hoặc `<tên thiết bị> (<tên khóa>)` (`ui::ClientKeyLabel`, `deskhubp::ClientPublicKeyLine`, CLI `key public`), nên host hiện tên client thay cho “(unnamed)”. Tên thiết bị chỉ còn một chỗ nhập: mục “Device name” đầu khối General của Settings (layout dùng chung `SettingField::DeviceName`), bỏ ô “Your name” ở trang Client trên cả 5 app; host, client và nhãn public key cùng dùng tên này.
- [x] Không migration dữ liệu cũ (quyết định 2026-09-29): bỏ file `paired_devices`, file đánh dấu kích hoạt và đường fallback; admission chỉ dùng `authorized_keys` (thiếu file = từ chối tất cả). File `authorized_keys`/`known_hosts` không đọc được thì từ chối khi đọc và được tạo lại khi ghi; file cũ được xóa. Bỏ đoạn làm sạch passcode trong settings/recent và phía Apple/Android. `authorized_keys` không lưu thời gian nên bỏ cột “Paired/Last seen”. API mới: `ListAuthorizedClients`, `ForgetAuthorizedClient`, `ClearAuthorizedKeys` (sửa lỗi nút “Remove every client” trên Linux/Windows trước đây không thu hồi gì).
- [x] Gỡ phần discovery còn sót: `Beacon` đổi tên `SourceListResponder` (chỉ trả lời kết nối đã xác thực), bỏ `DeviceRows`/nguồn “On this network”/cột Where/status/ping, tách `DiscoveryFfi` thành `SettingsFfi` + `DevicesFfi`, đổi `DiscoveryModel` (Apple) thành `RecentDevicesModel`.
- [x] Fuzz target `fuzz_keys` cho parser public key, `authorized_keys`, `known_hosts`, tên host/nhãn khóa (seed từ khóa thật). Máy dev thiếu runtime libFuzzer của clang; đã chạy 400.000 input đột biến dưới ASan/UBSan (gcc) không lỗi. CI chạy `make fuzz` như cũ.
- [x] Tài liệu EN/VI/ZH/JA cập nhật theo mô hình khóa kiểu SSH, TOFU, bố cục Devices, tên thiết bị, không migration; PRIVACY lên bản 2.9. `cli-smoke.sh` viết lại theo luồng khóa.
- [x] Sửa lỗi Android không tạo được khóa client: `link()` bị SELinux chặn trong thư mục app, thay bằng kiểm tra tồn tại + `rename()` dưới file lock. Thêm xóa khóa trong “My keys” (không xóa `default`, không xóa khóa đang được host đã tin dùng).
- [x] Thông báo lỗi xác thực chỉ rõ bước tiếp theo (copy public key ở My keys, host dán vào Clients allowed to connect, kết nối lại; CLI in thêm lệnh `key public` / `access add --stdin`).
- [x] Host gửi tên thiết bị trong `SOURCE_LIST` (chỉ cho kết nối đã xác thực); danh sách recent ở trang Client hiện tên host, lưu trong `recent-hosts.txt` (quản lý tập trung ở `platform/system/RecentDevicesFile`, `dh_list_sources` tự ghi, bỏ `dh_recent_touch`). PRIVACY lên 2.10.

- [x] Không tự tin cậy host lần đầu dựa trên địa chỉ, tên máy hoặc việc TLS kết nối thành công. Theo quyết định TOFU ở trên, host lần đầu chỉ được lưu khi người dùng xác nhận fingerprint hoặc dùng `--accept-new-host-key`.
- [x] Host chưa được cấu hình khóa: dừng; GUI hỏi xác nhận fingerprint, CLI in fingerprint và hướng dẫn `--accept-new-host-key`.
- [x] Host đổi khóa: dừng và yêu cầu cập nhật chủ động trong cấu hình/Devices; không mở popup chấp nhận ngay khi Connect.
- [x] Client chưa được cấp quyền: từ chối; không tạo yêu cầu duyệt.
- [x] Public key chỉ định danh khóa. Tên/comment là nhãn hiển thị, không quyết định quyền truy cập.
- [x] Mỗi chiều truy cập được cấp quyền riêng; A vào được B không tự cấp quyền B vào A.

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
| CLI | `core/src/cli/Command.cpp`, `client/cli/StoreCommands.cpp`, `client/cli/SourcesCommands.cpp` | Quản lý khóa/host, bỏ scan/passcode |
| Discovery | `platform/src/client/LanScanner.cpp`, `platform/src/client/HostProbe.cpp`, `platform/src/ffi/DiscoveryFfi.cpp` | Xóa scanner/probe, chuyển phần quản lý host còn cần sang API phù hợp |
| Apple UI | `client/apple/swift/DevicesPage.swift`, `client/apple/swift/DiscoveryModel.swift` | Quản lý khóa và host cấu hình thủ công |

Logic thuần dữ liệu/giao thức đặt trong `core/`. Crypto, filesystem, credential store và transport đặt trong `platform/`. `client/` chỉ xử lý giao diện và tích hợp đặc thù OS. `core/` không phụ thuộc thư viện crypto hay header OS.

## 4. Mô hình khóa và định dạng cấu hình

### 4.1. Tách vai trò khóa

- [x] Giữ khóa TLS host P-256 hiện có để tương thích quiche/BoringSSL đang dùng. Không đổi TLS sang Ed25519 trong đợt này.
- [x] Thêm abstraction khóa xác thực client, độc lập với chứng chỉ TLS. Import khóa client không làm thay đổi khóa TLS host.
- [x] Mặc định mỗi thiết bị có một khóa xác thực riêng; CLI cho phép tạo/import nhiều khóa theo tên và hồ sơ host chọn khóa tương ứng.
- [x] Hỗ trợ Ed25519 và ECDSA P-256 cho khóa client được tạo hoặc import dạng PKCS#8 và dùng để xác thực. RSA, FIDO/security key, SSH certificate và ssh-agent là phần mở rộng riêng.
- [x] Public key dùng text OpenSSH một dòng: `<key-type> <base64-key-blob> <optional-label>`.
- [x] Private key import hỗ trợ OpenSSH và PKCS#8 cho các thuật toán đã nêu; từ chối định dạng/thuật toán chưa hỗ trợ.
- [x] Kiểm tra khóa private hợp lệ, suy ra public key và xác minh cặp khóa bằng ký/xác minh thử trước khi lưu.
- [x] Dùng thư viện crypto/parser đã được duy trì; BoringSSL xử lý PKCS#8, RustCrypto `ssh-key` 0.6.7 xử lý OpenSSH private key và KDF qua patch quiche. Không tự xây thuật toán mật mã/KDF.
- [x] Private key mã hóa bằng passphrase được mở khóa trong luồng import. CLI không tương tác nhận qua stdin với `--passphrase-stdin`; không truyền bí mật qua argv/log.
- [x] Passphrase chỉ mở private key bên ngoài trong lúc import; passcode kết nối không tham gia handshake bằng khóa. Sau import, identity lưu trong backend cục bộ và kết nối đọc lại mà không yêu cầu popup. Các ô nhập passcode cũ trong UI còn chờ dọn ở mục 6/8.

Parser PEM/PKCS#8 hiện có dùng BoringSSL. Import OpenSSH dùng RustCrypto `ssh-key` trong quiche static library; bản build không gọi tiến trình `ssh-keygen` nên dùng được trên mobile. `ssh-keygen` chỉ tạo test vector.

### 4.2. Ba nhóm cấu hình

Tên file và lệnh bên dưới là thiết kế dự kiến cần thống nhất với CLI hiện tại khi triển khai.

| Nhóm | Dữ liệu | Quy tắc |
| --- | --- | --- |
| Identity store | ID khóa, loại khóa, public key, tham chiếu private key được bảo vệ | Không trả private key qua API liệt kê |
| `authorized_keys` | Mỗi dòng một public key client, kèm comment tùy chọn | Danh sách rỗng từ chối tất cả |
| Host profiles | Alias, địa chỉ/cổng, khóa TLS host đã ghim, ID khóa client sử dụng | Không có pin thì không kết nối |

- [x] Parser `authorized_keys` giới hạn kích thước, kiểm tra base64 và tính nhất quán giữa loại khóa khai báo với blob; phát hiện khóa trùng sau chuẩn hóa.
- [x] Parser chỉ nhận cú pháp public key đã định nghĩa; không âm thầm bỏ qua các option `authorized_keys` của OpenSSH chưa hỗ trợ.
- [x] Fingerprint `SHA256:…` của Deskhub là SHA-256 trên DER SPKI; đã ghi rõ trong kiến trúc bốn ngôn ngữ. Fingerprint SSH trên key blob là giá trị khác, không so sánh trực tiếp.
- [x] UI Apple, Android, Linux và Windows hiển thị riêng public key/fingerprint xác thực client và fingerprint TLS host, đều ghi rõ SHA-256 trên SPKI.
- [x] Hồ sơ host lưu alias riêng với địa chỉ; thay IP/cổng không tự thay khóa tin cậy.
- [x] Host TLS public key/fingerprint có cách xuất text (`trust public`) và nhập trước trên client (`trust add ADDRESS PUBLIC_KEY|FINGERPRINT`); hai đầu dùng fingerprint SHA-256 của SPKI.
- [x] Quyền truy cập gắn với public key trong `authorized_keys`, không gắn IP. Chép cùng private key sang hai thiết bị khiến chúng dùng chung danh tính và bị thu hồi cùng nhau.

### 4.3. Lưu trữ và reload

- [x] `ClientIdentity` có đường ghi khóa riêng, tạo file tạm `0600` trên POSIX, ghi/flush rồi thay thế atomic; không dùng helper ghi text chung cho private key.
- [x] Backend theo OS và loại tiến trình đã chốt: Windows dùng DPAPI phạm vi tài khoản và ACL riêng; iOS dùng App Group container cùng Data Protection mặc định; Android dùng app internal storage được sandbox bảo vệ và mã hóa trên thiết bị hỗ trợ; macOS/Linux app/CLI/service dùng file riêng của tài khoản chạy (`0700`/`0600`) để host nền hoạt động không cần mở kho khóa tương tác. Đây là lựa chọn của người dùng; trên desktop không bật mã hóa ổ đĩa và Android 8/9 không bật mã hóa, lớp bảo vệ còn lại là quyền truy cập file/app sandbox.
- [x] iOS app và broadcast extension dùng cùng App Group container; extension dừng nếu container không khả dụng thay vì tạo identity ở đường dẫn mặc định. Desktop app/CLI dùng cùng thư mục của tài khoản chạy chúng.
- [x] POSIX mở thư mục theo file descriptor, từ chối symlink, kiểm tra chủ sở hữu và đặt `0700`; private key và file cấu hình ghi tạm `0600`. Windows từ chối reparse point và đặt ACL cho tài khoản hiện tại, SYSTEM và Administrators, áp dụng cho file mới qua inheritance. Còn cần build/test Windows thực tế trong gate đa nền tảng ở mục 9.
- [x] Private key, `authorized_keys`, `known_hosts` và `paired_devices` ghi atomic; thao tác sửa khóa bằng mutex và file lock liên tiến trình, chỉ báo thành công sau khi ghi xong.
- [x] Thư mục cấu hình có thể chỉ định riêng bằng `SetConfigDir` hoặc `DESKHUB_CONFIG_DIR`, để service không phụ thuộc thư mục log/tài khoản GUI; test xác nhận hai đường dẫn độc lập.
- [x] Host đọc `authorized_keys` từ đĩa khi xét admission và khi rà quyền các kết nối đang chạy; thay đổi bởi GUI/CLI hoặc tiến trình ngoài đều thu hồi kết nối tương ứng.
- [x] File thiếu/hỏng không được dẫn tới cho phép tất cả. Reload lỗi trả trạng thái cấu hình lỗi, từ chối admission mới; thao tác xóa toàn bộ không ghi đè file lỗi hay báo thu hồi thành công.
- [x] Khóa đã tồn tại nhưng không đọc được phải trả lỗi; không âm thầm sinh khóa mới. Không log private key hoặc passphrase.

## 5. Handshake chỉ bằng khóa

- [x] Định nghĩa phiên bản auth mới; client/server không hỗ trợ phải trả lỗi phiên bản và đóng kết nối. Không fallback về passcode, approval hay plaintext.
- [x] `AuthStart` gửi SPKI DER chứa OID thuật toán, public key client và tên hiển thị có giới hạn độ dài. Host chỉ nhận SPKI Ed25519/P-256 và đóng kết nối nếu khóa không hỗ trợ.
- [x] Host kiểm tra public key hợp lệ và có trong danh sách được phép trước khi tiếp tục xác thực.
- [x] Host chỉ nhận một phản hồi chữ ký trên mỗi kết nối; định danh phiên QUIC/TLS thay nonce challenge riêng.
- [x] Định nghĩa transcript bằng encoding không mơ hồ, gồm nhãn riêng của Deskhub, version, vai trò, định danh phiên, danh tính client và khóa TLS host.
- [x] Ràng buộc chữ ký với phiên transport bằng TLS exporter của quiche; bổ sung wrapper C và test. Không coi IP/port là session binding.
- [x] Client ký transcript bằng private key đã chọn; host xác minh bằng public key đã được cấp quyền. QUIC loopback đã kiểm tra khóa client được chọn và khóa khác bị từ chối.
- [x] Kiểm tra lại quyền ngay trước khi chấp nhận, tránh khóa bị thu hồi trong lúc handshake vẫn được cho vào.
- [x] Client chỉ nhận `Accepted` trong trạng thái đã hoàn thành các bước xác thực mong đợi; từ chối message sai thứ tự/lặp hoặc không thuộc kết nối hiện tại.
- [x] Auth thành công chỉ có hiệu lực cho kết nối đó. Reconnect phải ký lại; QUIC session resumption và 0-RTT hiện không được bật nên không có đường kế thừa admission hoặc thao tác đặc quyền trước auth. Test QUIC loopback xác nhận kết nối mới không được cấp quyền trước khi ký lại.
- [x] Transport host không chuyển message ứng dụng vào các handler và không gửi record/datagram ứng dụng trước khi auth hoàn tất; bao gồm các kênh screen, input, clipboard, terminal, file transfer và danh sách tài nguyên.
- [x] Thu hồi khóa đóng các kết nối đang được cấp quyền của khóa đó và chặn kết nối mới; host nhận cả thay đổi cùng tiến trình và file được tiến trình khác thay thế. QUIC loopback kiểm tra thu hồi đang chạy và thay đổi file bên ngoài.
- [x] Giới hạn thời gian/số handshake đang chờ và lưu lượng auth thất bại; thay cơ chế throttle passcode bằng giới hạn phù hợp với auth bằng khóa.
- [x] Trả mã lỗi ổn định: host chưa tin cậy, host đổi khóa, khóa client chưa được cấp quyền, chữ ký sai, khóa cục bộ không dùng được, version không tương thích, timeout, lỗi cấu hình; wire auth tăng lên version 6 cho mã lỗi mới.

## 6. GUI, CLI và API cho service

### 6.1. GUI

- [x] Trang “Khóa của tôi”: tạo khóa, import private key, xem/copy public key và fingerprint (mục “My keys” trong Devices).
- [x] Trang “Thiết bị được phép”: dán public key text, liệt kê và thu hồi khóa. Nhãn lấy từ phần comment của public key (tên thiết bị bên client); chưa có chỗ sửa nhãn trên host.
- [x] Trang “Host đã lưu”: nhập alias, địa chỉ/cổng, khóa host tin cậy và chọn khóa client. Sửa host đã lưu hiện chỉ có qua CLI `host update`; GUI xóa rồi thêm lại.
- [x] Kết nối từ danh sách host đã lưu; hiển thị lỗi trong trạng thái kết nối, không mở popup xin duyệt/trust.
- [x] Cập nhật Swift dùng chung macOS/iOS, Android, Linux và Windows cùng một hành vi. Windows và macOS/iOS chưa được build cục bộ.
- [x] Xóa trường passcode, nút scan, switch cho phép pair máy mới và các màn hình yêu cầu duyệt. Nút làm mới còn lại chỉ nạp lại danh sách đã lưu, không quét mạng.
- [x] Chỉ lưu/hiển thị “last seen” dựa trên tương tác đã xác thực; không probe plaintext để cập nhật trạng thái danh sách. `authorized_keys` không lưu thời gian; “Last connected” ở danh sách recent chỉ ghi khi `dh_list_sources`/`QuerySources` thành công sau xác thực.

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

- [x] Thêm cú pháp `key`, `access`, `host`, `host-key public` bên cạnh `devices`/`trust`; help, JSON và exit code dùng quy ước CLI hiện có.
- [x] `access add --stdin` nhận public key dạng text; `host add --host-key-stdin` nhận public key TLS host và lưu fingerprint đã xác minh định dạng.
- [x] Có `host update`/`host remove`; cập nhật địa chỉ, identity và pin qua API `platform/` ghi atomic.
- [x] `--config-dir` dùng chung cho mọi lệnh; thư mục không truy cập được trả lỗi rõ.
- [x] API lưu host trong `platform/` không gọi UI, không đọc stdin, không phụ thuộc event loop GUI.
- [x] `connect`, `sources`, `shell`, `send` nhận alias host đã lưu, dùng địa chỉ, identity và pin của cùng profile.
- [x] Bỏ `scan`, tùy chọn passcode, biến môi trường passcode, cơ chế approval/auto-allow và tùy chọn bỏ qua kiểm tra host key.
- [x] Bỏ lệnh `probe` và tùy chọn `--timeout` chỉ dùng cho probe; không còn probe UDP công khai.

## 7. Loại bỏ LAN discovery

- [x] Xóa `LanScanner` và các callback/thread/timer quét mạng, gồm tự scan khi mở app và rescan định kỳ.
- [x] Rà `HostProbe`, `DiscoveryFfi`, `DiscoveryModel`, UI từng OS và CLI để bỏ dependency vào kết quả scan.
- [x] Tách logic recent/saved host còn cần ra khỏi `DiscoveryFfi`; không xóa nhầm chức năng lưu host thủ công.
- [x] Bỏ ngoại lệ nhận beacon plaintext trong `SessionTransport::Deliver` và đường gửi/trả lời discovery plaintext tương ứng.
- [x] Host không trả `SOURCE_LIST` hay `PONG` cho các probe discovery chưa xác thực, kể cả trả danh sách rỗng.
- [x] Sửa `RunSources` và `RunConnect` đang gọi `ProbeHostRttMs`: đi thẳng vào QUIC/TLS, kiểm tra host, auth rồi mới query nguồn.
- [x] Rà `Beacon`, `HostNetLoop` và `ViewerBroadcast`: tách phần liệt kê nguồn trong phiên đã auth trước khi xóa chức năng discovery. Không xóa theo tên “Broadcast” vì còn broadcast media tới viewer hợp lệ.
- [x] Giữ `ListSources/SourceList` khi cần cho danh sách màn hình sau auth; giữ ping/pong, heartbeat, RTT trong phiên hợp lệ.
- [x] Bỏ import, file build, FFI, string ID, bản dịch và test chỉ phục vụ scan LAN. Đã xóa source/build/FFI/JNI/test scanner, probe và string helper quét; cần rà tiếp string ID, bản dịch và tài liệu cũ. Rà lại 2026-09-29: không còn string ID/FFI/test scan; `DiscoverCommands.cpp` đổi thành `SourcesCommands.cpp`.
- [x] Kiểm tra bằng packet capture: app idle không quét subnet; host không trả lời gói discovery Deskhub cũ; kết nối cấu hình thủ công vẫn hoạt động. Không có quyền root cho tcpdump; thay bằng strace: app Linux idle 20 s (display broadway, HOME riêng) không gửi gói IPv4/IPv6 nào (chỉ AF_UNIX/AF_NETLINK); host CLI đang lắng nghe không trả lời LIST_SOURCES/PING/HELLO plaintext hay gói rác; kết nối thủ công qua cli-smoke vẫn chạy. Packet capture trên mạng thật vẫn nên chạy trên máy có quyền root.

Bỏ discovery giảm dữ liệu công khai và đường xử lý trước auth. QUIC vẫn cần gói handshake mạng trước auth ứng dụng; cổng lắng nghe không trở nên vô hình trước quét mạng.

## 8. Chuyển đổi dữ liệu và tương thích

- [x] Giữ khóa TLS host hiện tại nếu hợp lệ để tránh đổi danh tính host ngoài ý muốn; file đã lưu nhưng không dùng được không bị tự thay thế.
- [x] ~~Version hóa cấu hình mới và migration~~ — bỏ theo quyết định không migration; file cũ không tương thích bị xóa/tạo lại.
- [x] ~~`paired_devices` cũ chỉ lưu fingerprint, không có public key để xuất thành `authorized_keys`. Không chuyển fingerprint thành một public key giả hoặc tự cấp quyền cho khóa do mạng cung cấp.~~ — không áp dụng (không migration).
- [x] ~~Cung cấp hướng dẫn/lệnh migration chủ động: xuất public key từ identity cũ trên từng client và thêm lại trên host, hoặc tạo khóa client mới rồi cấp quyền.~~ — không áp dụng (không migration).
- [x] ~~Có thể tái sử dụng khóa P-256 cũ cho danh tính client chuyển tiếp nếu parser mới hỗ trợ; phải là lựa chọn rõ ràng để sau này tách/đổi khóa client không làm đổi TLS host.~~ — không áp dụng (không migration).
- [x] ~~Giữ các pin host cũ nếu chuyển đổi được với đúng encoding fingerprint; đánh dấu rõ pin legacy và không làm mất kiểm tra khóa khi đọc cấu hình cũ.~~ — không áp dụng (không migration).
- [x] ~~Địa chỉ recent không kèm pin chỉ được chuyển thành mục chưa cấu hình xong; không tự trở thành host tin cậy.~~ — không áp dụng (không migration).
- [x] Xóa passcode khỏi settings/recent store, CLI/env, FFI, log và serialization; không tạo bản backup mới chứa passcode/private key dạng rõ.
- [x] Thông báo tương thích rõ cho client/server cũ. Bản mới không cho phép cơ chế cũ để giữ tương thích. Bản cũ nhận `VersionMismatch` và thông báo “Update Deskhub on both machines”; tài liệu ghi rõ không tương thích và không migration.
- [x] Khi danh sách khóa mới chưa được cấu hình, host từ chối truy cập và hướng dẫn quản trị viên thêm public key cục bộ. Host từ chối tất cả; danh sách trống trên Devices hướng dẫn xin public key, dán vào ô và bấm Allow.

## 9. Kiểm thử và tiêu chí nghiệm thu

### 9.1. Unit và parser

- [x] Parse/serialize public key text, chuẩn hóa, phát hiện duplicate, comment, ký tự điều khiển, dữ liệu hỏng/quá lớn và thuật toán không hỗ trợ. (`PublicKeyTextTests`: chuẩn hóa, dữ liệu hỏng, giới hạn 128 khóa.)
- [x] Kiểm thử fingerprint SSH/SPKI và migration để không lẫn encoding. (`ClientIdentityTests::TestAnOpenSshFingerprintIsNeverTheSpkiFingerprint`; phần migration không áp dụng.)
- [x] Test vector import OpenSSH/PKCS#8 thực tế cho Ed25519/P-256, gồm khóa mã hóa, sai passphrase, dữ liệu cắt ngắn và cặp khóa không hợp lệ. (mở rộng `ClientIdentityTests`, thêm fixture `p256_mismatched`.)
- [x] Test ghi cấu hình thất bại, reload lỗi, file thiếu và cập nhật đồng thời. Không tác động cấu hình thật của người dùng khi chạy test. (`AppDataFileTests`: ghi đồng thời, file lock liên tiến trình, `known_hosts` hỏng/thiếu/không ghi được; test chạy trong thư mục tạm riêng.)
- [x] Cập nhật `WireTests`, `CommandTests`, `AuthProofTests`, `HostIdentityTests`, `AuthNegotiationTests` và các test trust/store liên quan.

### 9.2. Transport và integration

- [x] Đúng khóa và đúng pin host kết nối được; chỉ biết public key không thể giả danh client. (`AuthNegotiationTests::TestAPublicKeyAloneCannotImpersonateAClient`.)
- [x] Chữ ký sai, sai challenge, replay cùng/khác kết nối, hết hạn, đổi public key giữa handshake và message sai thứ tự đều bị từ chối. (`AuthNegotiationTests`, `TransportAdmissionTests`: transcript, replay khác kết nối, đổi khóa giữa chừng, sai thứ tự, hết hạn.)
- [x] Host chưa biết/đổi khóa bị chặn; tên máy, IP giống nhau hoặc certificate tự ký không vượt qua pinning. (`HostLinkTests::TestAnImpostorOnThePinnedAddressIsRejected`: cert tự ký cùng CN, cùng IP:port.)
- [x] Khóa bị thu hồi trong lúc handshake hoặc đang có nhiều phiên không tiếp tục truy cập được. (`TransportAdmissionTests::TestRevokingAKeyClosesEveryConnectionItHolds`; thu hồi giữa handshake kiểm ở mức `HostAuth`.)
- [x] Không đọc nguồn màn hình, mở terminal, gửi file, input hay clipboard trước auth. (`TransportAdmissionTests::TestNothingReachesTheHostBeforeAuthentication`.)
- [x] Reconnect/resume/session resumption phải xác thực lại đúng policy. (`QuicEndpointTests::TestEveryConnectionBindsItsOwnAuthSession`; 0-RTT/resumption không được bật trong code.)
- [x] Gói discovery plaintext cũ không nhận phản hồi; ping/pong và source query sau auth vẫn hoạt động. (`SessionTransportTests::TestPlaintextDiscoveryIsIgnored` + kiểm tay với host thật.)
- [ ] Kiểm tra kết nối thủ công trên macOS, Windows, Linux, Android, iOS và CLI; service được mô phỏng bằng caller không có UI dùng cùng API/config.
- [x] Fuzz parser key/config (`fuzz_keys`); message auth đã nằm trong `fuzz_wire`.

### 9.3. Gate hoàn tất

- [x] Chạy `make test`, `make lint` và `make test-all`; sửa mọi lỗi liên quan.
- [ ] Chạy `make lint-tidy` và các build/test CI đa nền tảng liên quan thay đổi crypto/FFI/UI. Ghi rõ gate nào chưa chạy được cục bộ. Đã chạy cục bộ 2026-09-29: `make lint-tidy`, `make fuzz` (8 target × 20 s, gồm `fuzz_keys`, không lỗi), gitleaks 8.30.1 trên toàn lịch sử với `.gitleaks.toml` mới (allowlist fixture OpenSSH và một dương tính giả trong commit 0d3c5d58) — không còn phát hiện. Chưa chạy cục bộ: ASan/TSan đầy đủ, build Windows/macOS/iOS/arm64, CodeQL.
- [ ] Kiểm tra packet capture và cấu hình thực tế theo các tiêu chí ở trên.
- [x] Rà lại repo để không còn hành vi passcode, approval popup, auto-trust hoặc LAN scan trong sản phẩm; tránh xóa nhầm passphrase import khóa và heartbeat. Quét 2026-09-29: chỉ còn trùng tên vô hại (scancode, biến probe trong CI); mô tả App Store/Play Store đã sửa theo TOFU.

## 10. Tài liệu và thứ tự triển khai

- [x] Cập nhật README, SPECIFICATION, ARCHITECTURE, SECURITY, CLI help và hướng dẫn cài đặt/cấu hình bị ảnh hưởng.
- [x] Cập nhật PRIVACY về dữ liệu khóa/host được lưu và loại bỏ discovery/passcode; theo quy ước tài liệu hiện có, cập nhật version/ngày hiệu lực/changelog khi thay đổi hành vi lưu hoặc truyền dữ liệu.
- [x] Đồng bộ các tài liệu sản phẩm được sửa ở EN/VI/ZH/JA. Ảnh chụp README vẫn là ảnh cũ, cần chụp lại.
- [x] Hướng dẫn tạo/import khóa, thêm public key text, ghim khóa host, chạy CLI không tương tác, thu hồi/đổi khóa và chuyển dữ liệu cũ. (`docs/INSTALL*` mục “Keys and access”, 4 ngôn ngữ; CLI thêm `key delete`, `access clear`, exit code 3 khi không tới được host.)

Triển khai theo các đợt phụ thuộc sau:

1. Chốt schema, fingerprint, thuật toán/định dạng, backend lưu khóa và phương án session binding; tạo test vector.
2. Xây key store, parser text, authorized key store và host profile API dùng chung.
3. Thay handshake và trust policy; xác minh kết nối thực qua QUIC bằng integration test.
4. Nối UI/CLI/FFI vào API mới, bổ sung migration và thu hồi phiên.
5. Gỡ passcode/approval/discovery, sửa các call site probe và build manifests.
6. Hoàn tất test đa nền tảng, kiểm tra traffic, tài liệu và hướng dẫn nâng cấp.

Không phát hành trạng thái trung gian cho phép bỏ qua kiểm tra khóa hoặc tự động chuyển về phương thức xác thực cũ.
