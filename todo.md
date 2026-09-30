# Deskhub — danh sách cần kiểm tra

Kết quả rà soát tài liệu so với code ngày 2026-10-01. Mỗi mục có đường dẫn `file:dòng` làm bằng chứng; số dòng lấy tại commit `968a74ca`. Các mục do agent đọc code tìm ra, chưa được xác minh lại thủ công — hãy kiểm tra từng mục trước khi sửa.

## 1. Lỗi trong code, script và build

### Bảo mật (ưu tiên cao nhất)

- [ ] **Windows host ghi mọi phím viewer gõ vào file log.** `client/windows/cpp/input/InputInjector.cpp:55` có `LOGI("[InjectKey] vk=%d down=%d held=%zu", ...)`, và `client/windows/win32/main.cpp:13` chuyển stderr vào `%USERPROFILE%\.deskhub\deskhub-*.log`. Mật khẩu viewer gõ trên host Windows vì vậy nằm trong file văn bản thường dưới dạng mã virtual-key. Điều này trái với SECURITY.md:208-210 và PRIVACY.md:45. Injector macOS và Linux không ghi mã phím.
- [ ] **Host không tự đóng kết nối của người lạ.** Với `Denied` và `AwaitingApproval`, host chỉ gửi challenge rồi giữ trạng thái kết nối (`platform/src/net/SessionTransport.cpp:303-319`), không gọi `CloseConnection`. Một peer xấu ping liên tục có thể giữ kết nối mãi, vì chỉ bị giới hạn bởi idle timeout QUIC 30 s (`platform/include/deskhubp/net/QuicEndpoint.h:18`).
- [ ] **32 handshake nằm im có thể khoá mọi client hợp lệ.** `kMaxPendingAuth = 8` và deadline 10 s chỉ áp dụng cho key đã được phép (`SessionTransport.h:33-34`). Trước khi `AuthStart` tới thì không có deadline nào, và trần là `kMaxConnections = 32` (`QuicEndpoint.cpp:484,751`).
- [ ] **Link `deskhub://` tin host key một cách im lặng.** Android đăng ký scheme exported + BROWSABLE, và iOS đăng ký cùng scheme (`client/ios/Deskhub-Info.plist`). Mở link từ bất kỳ trang web hay app nào đều kết nối ngay (`client/ios/app/swift/HomeView.swift:55-63`, `MainActivity.kt:446-451`) và pin host key mà không hiện hộp thoại New-host (`platform/src/client/HostLink.cpp` `PinExpectedHostKey`).
- [ ] **Access request được ghi từ public key tự khai, trước khi có chữ ký** (`platform/src/auth/AuthNegotiation.cpp:58-64`). Ai cũng có thể mạo danh key của người khác và ghi đè tên cùng địa chỉ (`core/src/net/AccessRequests.cpp:37-41`). Request thứ 17 đẩy request cũ nhất ra (`:43-49`), nên có thể bị flood. Đường QR ghi key vào `authorized_keys` trước khi client chứng minh giữ private key (`AuthNegotiation.cpp:43-56`).
- [ ] **Firewall Windows xoá rule BLOCK do người dùng tạo** cho exe Deskhub mỗi lần chia sẻ (`client/windows/cpp/net/Firewall.cpp:85-126,183`).
- [ ] **iOS: `host_key.pem` không được loại khỏi backup.** Repo không có chỗ nào dùng `isExcludedFromBackup`, nên private key đi vào backup iCloud và Finder. Android đặt `allowBackup="false"`.
- [ ] **Chứng chỉ TLS có chạm đĩa.** `TransientCertificateFile` ghi `transport_cert.<hex>.pem` vào thư mục config rồi mới xoá (`platform/src/net/QuicEndpoint.cpp:80-123`); nếu crash ở giữa thì file còn lại.
- [ ] **Dữ liệu cũ không được dọn.** Dòng `passcode=` trong `ui-settings.txt` cũ chỉ mất ở lần lưu settings kế tiếp. `auth_salt` không bao giờ bị xoá, vì danh sách retired chỉ gồm `{"paired_devices", "authorized_keys_active"}` (`platform/src/system/AuthorizedKeysFile.cpp:19`).
- [ ] **File log tạo bằng `fopen(path, "w")`** (`platform/include/deskhubp/diag/LogFile.h:225`), nên có quyền 0644 và chỉ được bảo vệ nhờ thư mục 0700. Log mỗi lần chạy một file và không bao giờ bị dọn.
- [ ] **"Host wins" trên Linux không hoạt động mặc định**, vì cần quyền đọc `/dev/input/event*` (nhóm `input`) (`platform/src/input/LocalInputLinux.cpp:71-76`), mà gói chỉ cấp `/dev/uinput` (`scripts/stage-linux-pkgroot.sh:37`).
- [ ] **Chỉ bật `/sdl` trên MSVC** (`core/CMakeLists.txt:106`, `platform/CMakeLists.txt:302`); không đặt tường minh stack-protector, FORTIFY hay RELRO. App macOS chạy `app-sandbox=false`.

### Runtime

- [ ] **Opus in-band FEC được bật nhưng không bao giờ được giải mã.** Encoder bật `OPUS_SET_INBAND_FEC(1)` (`platform/src/media/OpusCodec.cpp:42`), nhưng `Decode` và `Conceal` đều gọi `opus_decode(..., decode_fec=0)` (`OpusCodec.cpp:102-103,113`) và `AudioPlayer::Fill` chỉ gọi `Conceal` khi mất gói (`platform/src/audio/AudioPlayer.cpp:86`). Gói bị mất chỉ được che bằng PLC; băng thông FEC bị phí.
- [ ] **Windows installer có thể lỗi 740 khi chạy app ở cuối setup.** `packaging/windows/Deskhub.iss:13` đặt `PrivilegesRequired=lowest`, còn `:32` chạy `Deskhub.exe` từ `[Run]` không có `shellexec`, trong khi exe yêu cầu `requireAdministrator` (`client/windows/win32/CMakeLists.txt:39-41`). CI không bắt được vì cài với `/VERYSILENT` nên `skipifsilent` bỏ qua bước này (`.github/workflows/build-desktop.yml:74`).
- [ ] **`RunClientAuth` giữ `sendMutex_` trong lúc poll socket** tới 2 ms (`platform/src/net/SessionTransport.cpp:15,502-505`; `platform/src/net/QuicEndpoint.cpp:728-747`), trái với nguyên tắc ghi ở ARCHITECTURE. `SendStats()`, `IsOpen()`, `LocalPort()` chạm endpoint không khoá (`SessionTransport.cpp:664-685`) — kiểm tra có race không.

### Chuỗi UI sai

- [ ] **iOS Settings hiện `kClientSettingsHint` nói về "device scan"** (`core/include/deskhub/ui/Strings.h:51-53`, `client/ios/app/swift/SettingsView.swift:46`), trong khi app không quét mạng (spec D-1).
- [ ] **`kAuthNotPaired` và `kAuthRefused` nhắc tới mục "Clients allowed to connect"** (`Strings.h:119-133`), nhưng tiêu đề thật là "Devices allowed to connect to this machine" (`Strings.h:175-176`).

### Build và bootstrap

- [ ] **Windows: `make bootstrap && make test` hỏng ở bước configure.** `scripts/bootstrap.ps1` không build quiche/opus, và `test`, `test-platform`, `test-integration`, `test-ctest`, `test-asan`, `test-tsan` (`make/core.mk:41-56,95-101,127-131`) cùng `lint-tidy` (`make/codestyle.mk:49-51`) không phụ thuộc `quiche`/`opus`, trong khi preset của chúng bắt buộc có (`platform/CMakeLists.txt:10-16,35-41`).
- [ ] **Linux: `build-cli`, `release-cli`, `run-cli`, `cli-smoke` không phụ thuộc `ffmpeg-min`** (`make/cli.mk:9,12`), nhưng bật `DESKHUB_CLI=ON` thì thiếu `third_party/ffmpeg-min` là lỗi fatal (`client/linux/CMakeLists.txt:4-5`, `client/linux/cpp/CMakeLists.txt:18-22`).
- [ ] **Mọi recipe quiche/opus có tiền tố `-@`** (`make/core.mk:30,33`, `make/android.mk:20,23`, `make/ios.mk:11,14`, `make/macos.mk:40,43`), nên build thư viện thất bại bị nuốt im lặng và chỉ lộ ra sau đó thành lỗi CMake.
- [ ] **Ubuntu 22.04: bootstrap cài `cmake` từ apt (3.22)** (`scripts/bootstrap.sh:141`), nhưng root `CMakeLists.txt:1` yêu cầu ≥ 3.25.
- [ ] **SwiftLint không pin phiên bản khi bootstrap** (`brew install swiftlint`, `scripts/bootstrap.sh:97`), trong khi CI pin 0.65.0 kèm SHA (`.github/workflows/lint.yml:112-115`). `swiftformat` có sẵn trên PATH được nhận mà không kiểm tra version (`scripts/tools.sh:106-108`, `scripts/pinned-tools.ps1:126-136`).
- [ ] **SwiftLint trên CI phủ hẹp hơn local.** CI chỉ lint `client/apple/swift` và `client/ios/app/swift` (`lint.yml:122`); local còn lint `client/ios/broadcast/swift`, `client/ios/shared`, `client/macos/app/swift` (`scripts/codestyle.sh:109`). Swift của app macOS không bị gate trên CI.
- [ ] **`ANDROID_NDK_VERSION` chỉ đổi NDK cho quiche/opus.** Gradle hard-code `ndkVersion = "26.1.10909125"` (`client/android/app/build.gradle.kts:9`), và `bootstrap.ps1:103` bỏ qua biến này.
- [ ] **wxWidgets tải qua `FetchContent` không có `URL_HASH`** (`client/windows/win32/CMakeLists.txt:20`), không được pin bằng checksum như FFmpeg và Opus.
- [ ] **`reset-macos-permissions` mặc định chỉ reset `com.deskhub.macos`** (`scripts/reset-macos-permissions.sh:5`), trong khi bản Debug dùng `com.deskhub.macos.debug` (`client/macos/Deskhub.xcodeproj/project.pbxproj:350,368`). Script không chạm tới quyền của bản `build-macos`/`run-macos` trừ khi truyền `--bundle-id`.
- [ ] **`make screenshots` không có guard macOS** (`make/tools.mk:10-11`); nó chỉ "macOS only" vì dùng `xcrun`.

### Tuân thủ giấy phép

- [ ] **Hầu hết gói phát hành không kèm văn bản giấy phép.** Chỉ `.deb`/`.rpm` Linux cài `THIRD_PARTY_NOTICES.md` và `LGPL-2.1.txt` (`scripts/stage-linux-pkgroot.sh:50-51`), và cả chúng cũng thiếu `licenses/BSD-3-Clause-opus.txt`. Installer Windows chỉ có exe (`packaging/windows/Deskhub.iss:22`, `DeskhubCLI.iss:21`); macOS/iOS/Android không kèm gì. Repo không có văn bản giấy phép cho quiche (BSD-2), BoringSSL, ssh-key, wxWidgets. BSD-2/BSD-3 yêu cầu kèm notice khi phân phối binary.

### Quy tắc repo

- [ ] **Còn comment cũ vi phạm Rule 1** tại `client/linux/CMakeLists.txt:52-53` ("# Uninstalled builds have no hicolor icon...").
- [ ] **Target `perf-build`** (`make/core.mk:58`) không có trong header `Makefile` lẫn `make/help.txt`.
- [ ] **Ví dụ `run-cli ARGS="scan"` sai**, vì CLI không có verb `scan` (`Makefile:40`, `make/help.txt:14`; danh sách verb ở `core/src/cli/Command.cpp:229-241`). Nên đổi thành `ARGS="host list"`.

## 2. Tài liệu lệch với code

### README.md

- [ ] `:179` — nói "seven nightly libFuzzer targets"; thực tế có **chín** (thêm `keys`, `qr`) (`core/CMakeLists.txt:280-288`), và fuzz cũng chạy 30 s trên mỗi PR (`.github/workflows/test.yml:389-401`). Ba bản dịch cũng ghi "seven".
- [ ] `:138` — "Desktops can also share a shell and receive files" ngụ ý điện thoại không nhận file; Android và iOS đều nhận file (`client/android/app/src/main/cpp/HostBridge.cpp:101-118`, `client/ios/app/swift/FilesHost.swift`).
- [ ] `:171-179` ("What's inside") — thiếu truyền file, terminal từ xa, clipboard sync, keep-awake, retransmission cạnh FEC.
- [ ] `:178` — CLI còn có `send`, `devices`, `trust`, `host`, `settings`, `displays`, `sources`, `--json` (`core/src/cli/Command.cpp:139,229-241`).
- [ ] `:167` — hàng Linux thiếu NVENC (`client/linux/cpp/CMakeLists.txt:33`, `DESKHUB_NVENC ... ON`).
- [ ] `:66-68` — winget `ManhPham.DeskhubCLI` dùng exe portable, không phải setup (`scripts/publish-winget.sh`).
- [ ] `:70-78` — danh sách asset Releases thiếu `.dmg` macOS, CLI macOS, CLI Linux portable, các `.deb` tải trực tiếp (`.github/workflows/deploy.yml:70-75,200`).
- [ ] `:206-208` — CLI Linux cũng link tĩnh FFmpeg LGPL (`client/cli/CMakeLists.txt` → `deskhub_linux_core` → `ffmpeg_min`).
- [ ] `:138,154` — "60 fps" chỉ là mặc định; tối đa là 240 (`core/include/deskhub/ui/UiSettings.h:12,32`).
- [ ] `:65` — menu Linux hiện "Deskhub", không phải "DeskHub" (`scripts/stage-linux-pkgroot.sh:27`).
- [ ] `:15` — badge trỏ tới `#-platforms` (slug tự sinh) thay vì id tường minh `#platforms`.

### docs/INSTALL.md

- [ ] `:141-161, 509-510` — bỏ qua NVENC trên Linux. `nvidia-vaapi-driver` chỉ giải mã, và máy NVIDIA có `vainfo` rỗng vẫn host được qua NVENC (`client/linux/cpp/encode/HwEncoder.h:17-33`, `client/linux/cpp/SharingHost.cpp:131`). Yêu cầu thật là driver hỗ trợ NVENC API ≥ 13.0.
- [ ] `:121-122` — nút *Choose screens again* đã bị xoá (commit `5ac9307d`). GTK tự quên lựa chọn portal (`client/linux/gtk/MainWindow.cpp:2255-2264`); cách reset thủ công là `deskhub-cli displays --forget`.
- [ ] `:18, 92-93` — "openSUSE 15.5" không chạy được, vì Leap 15.5 có glibc 2.31 còn bản build cần 2.35 (`.github/workflows/build-desktop.yml:134-138`).
- [ ] `:52-53` — hộp thoại UAC hiện ở **mỗi lần** chạy, không phải chỉ một lần (`requireAdministrator`).
- [ ] `:206-207` — APK "cùng key với bản Google Play" chỉ đúng nếu upload key cũng là app-signing key của Play App Signing; cần xác nhận.
- [ ] `:209-217` — tag chỉ đẩy lên track **internal** (`client/android/fastlane/Fastfile:29`); lên `alpha` cần `workflow_dispatch` thủ công (`deploy.yml:6-12,243`).
- [ ] Thiếu phiên bản OS tối thiểu: macOS 14, iOS 17, Android 8.0 / API 26 (`project.pbxproj` `MACOSX_DEPLOYMENT_TARGET`/`IPHONEOS_DEPLOYMENT_TARGET`, `client/android/app/build.gradle.kts:13`).
- [ ] `:72-81` — macOS còn có prompt Local Network, cần mở lại app sau khi cấp Screen Recording, và xin quyền thông báo.
- [ ] `:164-176, 190-198` — `setup-uinput.sh` còn thêm user vào nhóm `input`, và SSH/headless cần đăng nhập lại. Phần gỡ cài không xoá rule udev, file modules-load hay membership nhóm.
- [ ] `:192-196, 56-58` — gỡ cài Linux không gỡ `deskhub-cli`, không nhắc thư mục `~/Deskhub` (Windows: `%USERPROFILE%\Deskhub`).
- [ ] `:95-97, 112-113` — CLI và app chỉ cùng tồn tại khi app cùng version hoặc mới hơn (`Replaces/Breaks`, `scripts/build-cli-deb.sh:38-39`; `Conflicts`, `scripts/build-cli-rpm.sh:35`).
- [ ] `:23-24, 234` — viewer CLI trên Linux là X11, nên Wayland cần XWayland.
- [ ] `:255-258` — winget CLI dùng exe portable, không qua setup.

### docs/BUILD.md, header Makefile, make/help.txt

- [ ] `BUILD.md:213` — "Neither `make test` nor CI runs any of it" là sai. CI chạy perf gate (`test.yml:96-100`) và `perf-compare` trên PR (`test.yml:224-345`).
- [ ] `BUILD.md:11-12, 165` — "every `build-*` target builds the ABI it needs first" không đúng với target test/lint và CLI (xem mục 1).
- [ ] `BUILD.md:31` — tiên quyết Windows: VS và Rust thực ra do bootstrap cài; còn thiếu GNU make, Git for Windows/Git Bash, Python 3.
- [ ] `BUILD.md:33-35, 363-364` — "fixed version with a checksum" không đúng cho clang-format/tidy (pip), swiftformat, cppcheck (winget) và SwiftLint.
- [ ] `Makefile:80-85`, `help.txt:71-72`, `BUILD.md:352,367-368` — lý do của `reset-macos-permissions` đã cũ vì bản Debug dùng bundle id riêng.
- [ ] `BUILD.md:270` — "in both languages" phải là bốn ngôn ngữ.
- [ ] `BUILD.md:328-329` — arm64 Linux build native, không cross-build; thiếu job ASan macOS và ASan MSVC Windows; chỉ x86_64 chạy trên emulator Android.
- [ ] `BUILD.md:366` — `ANDROID_NDK_VERSION` không ảnh hưởng Gradle và bootstrap Windows.
- [ ] `BUILD.md:190-191`, `Makefile:108` — thiếu `fuzz_keys`, `fuzz_qr`.
- [ ] `BUILD.md:182` — nhóm workload của `core_perf` thiếu PCM ring, input path, record-stream framing.
- [ ] `BUILD.md:208-210` — `perf-baseline` còn ghi `out/perf/platform-baseline.txt`.
- [ ] `BUILD.md:70` — `make lint` còn chạy `lint-dead`, không chỉ kiểm tra format.
- [ ] `Makefile:135-137` — `lint-tidy` cần cả opus, không chỉ quiche.
- [ ] Thiếu: phiên bản CMake ≥ 3.25; `make cli-smoke` (và CI chạy nó); yêu cầu của `dist-macos` (`ASC_KEY_P8`, `ASC_KEY_ID`, `ASC_ISSUER_ID`, Developer ID); các biến `MACOS_SIGN`, `MACOS_TEAM`, `MACOS_XCARGS`, `IOS_DEVICE`.
- [ ] `BUILD.md:29-30` — danh sách bootstrap Ubuntu/macOS thiếu nhiều gói (pipx, python3-venv, rpm, nasm, cargo-ndk, Android SDK...).
- [ ] `BUILD.md:47-58` — cây thư mục thiếu `cmake/`, `tests/integration/`, `packaging/`, `patches/`, `licenses/`, `assets/`, `tools/`.
- [ ] `BUILD.md:239-241` — build local cũng bật `-Werror` (`DESKHUB_WERROR=ON` trong preset `x64-debug`).
- [ ] `BUILD.md:147-149` — mã thoát CLI thiếu 1, 6, 8, 130 (`core/include/deskhub/cli/Command.h:49-58`); bảng lệnh thiếu `version`.
- [ ] `Makefile:8-10` — mô tả `toolchain.mk` và `core.mk` chưa đủ.
- [ ] `Makefile:34` — `run-windows` và `run-linux` có truyền `$(ARGS)`.
- [ ] `BUILD.md:88` — `release-ios` chỉ build cho Simulator.

### docs/SPECIFICATION.md

- [ ] `:166-168` — Settings trên mobile còn có T-22 và T-23 (`client/ios/app/swift/SettingsView.swift:52-55`, `MainActivity.kt:1521-1535`).
- [ ] `:124` (V-2) — zoom 5× và pan chỉ có trên điện thoại/tablet; viewer Windows, Linux, macOS không có. Điều này cũng mâu thuẫn với `:54`.
- [ ] `:101` (C-5) — thông báo quá hạn duyệt không có địa chỉ, không nhắc trang Devices, QR hay public key (`Strings.h:128-130`).
- [ ] `:199` (T-16) — icon menu bar macOS hiện ngay cả khi cửa sổ đang mở (`client/macos/app/swift/App.swift:77-80`).
- [ ] `:145` (I-10) — "một người điều khiển" thực ra tính theo từng màn hình, không theo toàn host (`core/src/session/host/ViewerTable.cpp:83-89`).
- [ ] `:195` (T-8) — ô nhập desktop tự kẹp giá trị vào giới hạn thay vì từ chối.
- [ ] `:203` (T-27) — trên macOS/iOS, tên thật gửi đi lấy từ `gethostname()`, không phải tên hiển thị (`platform/src/system/DeviceNamePosix.cpp`).
- [ ] `:223` (P-5) — thiếu quyền `POST_NOTIFICATIONS` trên Android 13+.
- [ ] `:62-66` — CLI còn có `send`, `share --qr`, chia sẻ terminal/file, `host update`, `settings get/set`, `--approval-wait`.
- [ ] `:82` (H-9) — "5 viewers" thực ra là 5 phiên màn hình trên mọi display cộng lại.
- [ ] `:201` (T-20) — wake lock được giữ suốt thời gian chia sẻ, kể cả khi không có viewer; Android còn giữ màn hình sáng trong cửa sổ terminal.
- [ ] `:125, 210` — dòng trạng thái có cả loss %.
- [ ] Nhỏ: G-3 là thư mục ẩn `~/.deskhub`; T-14 tôn trọng `$XDG_CONFIG_HOME`; S-2/S-9 chấp nhận cả `ssh-ed25519`; cột Sound chỉ nói về host; thứ tự ID H-14 và H-15 lộn xộn.

### docs/ARCHITECTURE.md

- [ ] `:74-76` — mỗi kết nối dùng **hai** stream: control 0 và file 8, với urgency khác nhau (`platform/include/deskhubp/net/QuicEndpoint.h:19-23`).
- [ ] `:261-262` — link ping là record trên stream, không phải datagram.
- [ ] `:30, 140` — `AuthThrottle` không tồn tại; tên thật là `deskhub::AuthFailureLimiter` trong `core/auth` (`core/include/deskhub/auth/FailureLimiter.h:15`).
- [ ] `:209-212, 511-512` — `BitrateController` chỉ dùng loss và frame age; RTT và receive rate chỉ để hiển thị.
- [ ] `:126-128` — khi IP đã bị chặn, `AuthStart` có token bị đóng ngay (`SessionTransport.cpp:274-279`).
- [ ] `:121-132` — `AuthMode` còn có `Denied` và `ConfigError` (`core/include/deskhub/protocol/Wire.h:178-183`).
- [ ] `:131-132` — khi `AwaitingApproval`, host không tự đóng kết nối; client đóng.
- [ ] `:92-94` — xem mục `RunClientAuth` ở phần 1.
- [ ] `:166-170, 286-288` — `SettleTrust` không tính cảnh báo previous-owner; UI tự tính.
- [ ] `:160-162` — không có trạng thái `InviteMismatch` trên link; chỉ có `SourceQueryFailure::InviteMismatch`.
- [ ] `:129-130` — access request lưu `time address key-with-label`, không lưu fingerprint.
- [ ] `:455-456` — host transport cũng dùng `AwaitingApproval` trong `onRefused`.
- [ ] `:189-202` — sơ đồ host thiếu `FileHost`; `:199` — host không xử lý `TERM_EXIT`.
- [ ] `:204-205, 642-644` — `keepAlive` còn tính cả chia sẻ file và trạng thái sống của `TerminalHost`/`FileHost` (`platform/src/host/HostEngine.cpp:445-449`).
- [ ] `:301-303` — cờ `SOURCE_LIST` còn có `kHostSharesAudio` và `kHostAcceptsFiles`.
- [ ] `:371` — danh sách fuzz thiếu `FuzzQr`, `FuzzKeys`.
- [ ] `:488-490` — Linux đọc Theme trực tiếp; iOS dùng `dh_theme_color`. `:325-326` — Apple cũng dùng `dh_qr_encode`. `:42` — danh sách FFI thiếu `ClientFfi`.
- [ ] `:63-65` — câu bị cụt ("in the plain" thiếu "text"). `:84` — thiếu đường fallback `SendRaw`.
- [ ] `:26-45` — bảng layer thiếu nhiều thư mục: `core/input`, `core/media`, `core/diag`, `core/transfer`, `platform/audio`, `platform/input`, `platform/media`, `platform/diag`, cùng nhiều class trong `core/control`, `core/session`, `core/ui`, `platform/net`, `platform/client`, `platform/host`, `platform/system`.
- [ ] `:760-765` — `SendStream` giờ dùng outbox 4 MiB / 256 KiB, và lỗi ghi chỉ đóng stream đó (`BreakStream`), không đóng kết nối (`platform/src/net/QuicEndpoint.cpp:54-55,357-410`).
- [ ] `:886-888` — CRT tĩnh trên Windows **được ép** qua `RUSTFLAGS` (`scripts/build-quiche.sh:127-131`), mâu thuẫn với `:810-815`.
- [ ] `:709-711` — mất gói audio chỉ được che bằng PLC (xem mục Opus FEC ở phần 1).
- [ ] `:746-750` — keepalive và redial nằm trong `HostLink`, không phải `TerminalViewer`; chu kỳ là 1/3 idle timeout. Mâu thuẫn với `:968`.
- [ ] `:806-809` — `libplatform_bundled.a` còn gộp cả opus. `:804-805` — configure fail ở `platform/CMakeLists.txt`, không phải `DeskhubQuiche.cmake`.
- [ ] `:577-579` — `StallTimeoutUs` còn cộng thêm `kNackHoldUs` (2 ms).
- [ ] `:880` — hiện có 25 dòng `uses: ./.github/actions/third-party`, không phải "nineteen".
- [ ] `:876-877` — `build-mobile.yml` không có input `for_release`, và deploy không gọi nó.

### THIRD_PARTY_NOTICES.md

- [ ] `:27, 52-66` — CLI Linux cũng đóng gói FFmpeg tĩnh, cùng libva, libdrm, epoxy, EGL, PipeWire.
- [ ] `:139-141` — Linux **có** decoder H.264 phần mềm của FFmpeg (`--enable-decoder=h264`, `client/linux/cpp/decode/AvDecoder.cpp:18,33`), và encode còn qua NVENC.
- [ ] `:112-121` — thiếu các crate Rust link tĩnh, gồm cả những crate BSD-3-Clause (`ed25519-dalek`, `curve25519-dalek`, `subtle`) và MIT/Apache (`p256`, `aes`, `sha2`...).
- [ ] `:13-23` — thiếu GLib/GIO (`gio-2.0`, LGPL-2.1+) (`platform/CMakeLists.txt:166,306`).
- [ ] `:72-84` — wxWidgets là 3.3.1; thiếu các thư viện wx đóng gói kèm (zlib, libpng, libjpeg, libtiff, libwebp, PCRE2, expat, nanosvg).
- [ ] `:23` — giấy phép libayatana-appindicator có lẽ là LGPL-2.1/LGPL-3.0, không phải GPL-3.0; cần kiểm tra.
- [ ] `:102-105` — thư viện Android được compile vào APK, không link động; thiếu LLVM libc++ (`-DANDROID_STL=c++_static`).
- [ ] Thiếu các thành phần OS: WASAPI, Windows.Graphics.Capture/C++/WinRT; AudioToolbox, IOKit, UserNotifications (macOS); AudioToolbox, PhotosUI, UniformTypeIdentifiers (iOS); AAudio (Android).
- [ ] `:38` — "rebuild with `make`" sai; phải là `make build-linux` hoặc `make ffmpeg-min`.
- [ ] `:131` — Opus được build theo từng target, và CLI là nơi dùng thứ sáu.

### CLAUDE.md

- [ ] `:32-39, 55` — "five clients" thiếu `client/cli`; layout thiếu `client/ios/shared/`, `tests/integration`, `cmake/`, `patches/`, `licenses/`, `packaging/`.
- [ ] `:86-88` — thiếu target CLI: `build-cli`, `release-cli`, `run-cli`, `cli-smoke`.
- [ ] `:52-53, 59, 67-72` — encode/decode VideoToolbox, capture Linux và audio nằm ở `platform/`, không phải `client/`; phần phân vai không nhắc `ffi`, `media`, `audio`, `input`, `system`.
- [ ] `:62-63` — "one identical API" bị vi phạm: `ProtectWindowsConfigDir` chỉ khai báo dưới `#ifdef _WIN32` (`platform/include/deskhubp/system/AppDataFile.h:33-34`).
- [ ] `:111` — arm64 Linux build native, không cross-build.
- [ ] `:104` — phạm vi SwiftLint trên CI hẹp hơn local.
- [ ] `:189` — đường dẫn store listing là `client/android/fastlane/metadata/android/vi/` và `client/ios/fastlane/metadata/vi/`.
- [ ] `:192` — CLI Linux cũng link tĩnh FFmpeg.

## 3. Bản dịch

- [ ] `SECURITY.{vi,zh,ja}.md` (vi:31, zh:29, ja:34) — thiếu nửa đầu câu "Keep the host on a trusted network, and…" (`SECURITY.md:31`).
- [ ] `docs/SPECIFICATION.zh.md` §13 (~232) — "靠肉眼从屏幕读取" dịch sai; bản gốc nghĩa là QR được quét từ màn hình.
- [ ] `docs/BUILD.{vi,zh,ja}.md` §6 (vi:236, zh:224, ja:~243) — mẫu `name: ...` bị dịch cả tiền tố `name:` bên trong backtick, trong khi `scripts/dead-code-allow.txt` bắt buộc dùng `name:`.
- [ ] `docs/ARCHITECTURE.{zh,ja}.md` §4 — bullet `TERM_CLOSE` bị tách riêng và đảo thứ tự so với bản tiếng Anh (nội dung vẫn đủ).
- [ ] `docs/INSTALL.vi.md:298` — in đậm "Không port-forward UDP 47777" trong khi bản gốc không in đậm.
- [ ] Nhiều span backtick hoặc in nghiêng bị ngắt dòng (`ARCHITECTURE.zh.md:502`, `ARCHITECTURE.vi.md:580`, `INSTALL.ja.md:294`, `SECURITY.vi.md:259`), khiến grep không tìm thấy.

## 4. SECURITY.md và PRIVACY.md

Ba bản dịch lặp lại cùng các lỗi này, nên mỗi chỗ sửa cần mirror sang vi/zh/ja. `PRIVACY.md` là văn bản pháp lý: thay đổi mô tả dữ liệu lưu hoặc truyền đi cần version mới, ngày hiệu lực mới và một dòng changelog.

### SECURITY.md

- [ ] `:208-210` — log **có** chứa phím gõ trên Windows (xem phần 1). Log còn chứa tên thiết bị, fingerprint key, tên file và thư mục.
- [ ] `:46` — "the connection is closed" sai; host không đóng kết nối (xem phần 1).
- [ ] `:46, 89` — giới hạn "8 waiting, 10 seconds" chỉ áp dụng cho key đã được phép.
- [ ] `:191-192` — "not a background service" bỏ qua start-with-OS. Trên Windows đó là scheduled task `/SC ONLOGON /RL HIGHEST` (`core/src/ui/AutostartConfig.cpp:20-23`); cộng với `auto_share` và `start_hidden`, host có thể nghe từ lúc đăng nhập mà không có cửa sổ. Checklist hardening nên bảo người dùng tắt chúng.
- [ ] `:144-148` — không nhắc việc xoá rule BLOCK. `:144` "it asks once" sai, vì UAC hiện mỗi lần chạy.
- [ ] Thiếu: rủi ro của link `deskhub://` (`:70-71` chỉ trình bày QR như một tính năng).
- [ ] `:44` — "Host wins" không chạy mặc định trên Linux.
- [ ] Thiếu: rule uinput của gói Linux (`MODE="0660", GROUP="input", TAG+="uaccess"`) mở rộng quyền input cho mọi tiến trình của người dùng; vào nhóm `input` thì đọc được mọi bàn phím.
- [ ] `:46, 75-77` — fingerprint trong request **chưa được chứng minh**; có thể mạo danh và flood.
- [ ] `:219-220` — chứng chỉ có ghi tạm ra đĩa.
- [ ] `:227, 236-237` — "No passcode is stored anywhere" và "leftover files are deleted" chỉ đúng một phần.
- [ ] `:221-222` — `known_hosts` còn lưu thời điểm first-seen và last-seen (`core/src/net/TrustStore.cpp:214-229`).
- [ ] `:52` — có chín fuzz target, không phải bảy, và thiếu lượt fuzz 30 s trên mỗi PR.
- [ ] `:228-229` — "every file 0600" không đúng với log.
- [ ] `:212-232` — danh sách file thiếu `*.lock`, symlink `deskhub-latest.log`, `.tmp-*`, `transport_cert.*.pem`, `.deskhub-dev`, và fallback `$TMPDIR/.deskhub`.
- [ ] Thiếu hẳn mục primitive mật mã:
  - ECDSA P-256; fingerprint là SHA-256 của SPKI.
  - Cert X.509v3 tự ký CN "deskhub", hiệu lực 20 năm (`platform/src/system/HostIdentity.cpp:24-26,137-149`).
  - TLS 1.3 qua quiche/BoringSSL, `verifyPeer=false` và thay vào đó pin theo fingerprint.
  - Exporter label `EXPORTER-Deskhub-Auth-v5`.
  - Ed25519 được chấp nhận cho key dán vào.
  - Nguồn random; so sánh token constant-time; tối đa 4 token.
- [ ] Thiếu: các gate CI bảo mật (gitleaks, CodeQL, dependency review) và phần hardening (`/sdl`, `app-sandbox=false`).

### PRIVACY.md

- [ ] `:45` — "input never stored" sai trên Windows (xem phần 1).
- [ ] `:46` — "private key never leaves the device" sai trên iOS (backup). "No certificate stored" cũng không chính xác.
- [ ] `:116-140` — bảng quyền thiếu:
  - macOS: Screen Recording, Accessibility, Local Network.
  - Windows: UAC admin, sửa rule firewall.
  - Linux: đồng ý screencast qua portal, rule udev, quyền đọc `/dev/input/event*`.
  
  Câu "no other permissions" vì vậy sai.
- [ ] `:49` — fingerprint trong request chưa được chứng minh.
- [ ] `:47` — `known_hosts` lưu thời điểm first-seen và last-seen.
- [ ] `:62` — "Connection statistics … Never stored" mâu thuẫn với log `[DIAG]` của desktop.
- [ ] `:154-158` — iOS phát hành qua TestFlight, không phải App Store. TestFlight và Play Console cho developer crash report theo từng thiết bị, không chỉ thống kê ẩn danh.
- [ ] `:152-153` — "Third-party SDKs: none" sai theo nghĩa đen (ZXing, CameraX, AndroidX, quiche, Opus, FFmpeg); nên viết "không có SDK thu thập dữ liệu".
- [ ] `:57` — `broadcast-status.txt` còn lưu tên viewer (`client/ios/shared/BroadcastStatus.swift:9,52`).
- [ ] `:54` — danh sách preference thiếu `client_control` và `play_audio` (`core/src/ui/UiSettings.cpp:44,49`).
- [ ] Bảng §3 thiếu các mục autostart: scheduled task Windows, `~/.config/autostart/deskhub.desktop`, login item macOS.
- [ ] `:52` — "IP/hostname" sai; code không phân giải DNS, chỉ nhận IPv4.
- [ ] `:58` — tên thiết bị của host đi trong SNI dạng rõ khi kết nối từ invite (`platform/src/client/SourceQuery.cpp:72`).
- [ ] `:181-182` — dọn dữ liệu cũ chưa đầy đủ (passcode, `auth_salt`).
