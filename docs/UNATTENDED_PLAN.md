# Deskhub — Truy cập trước khi đăng nhập trên Linux và macOS: plan triển khai

Đây là plan đưa *Allow access before anyone signs in* (T-28 trong
[`SPECIFICATION.vi.md`](SPECIFICATION.vi.md)) từ Windows sang Linux và macOS. Mỗi phần nêu
quyết định đã chốt, file sẽ chạm, điểm phải xác minh bằng thử nghiệm trước khi viết nhiều
code, và cách kiểm tra kết quả. Số dòng lấy từ cây mã ngày 2026-10-17 và sẽ trôi; tên file thì
không. Cho tới khi làm xong, tài liệu này mô tả ý định, không phải hành vi:
[`SPECIFICATION.vi.md`](SPECIFICATION.vi.md) và [`ARCHITECTURE.vi.md`](ARCHITECTURE.vi.md) vẫn
là nơi ghi Deskhub thực sự làm gì. Tài liệu này chỉ có bản tiếng Việt và nằm ngoài quy tắc
bốn ngôn ngữ của repo; khi plan được triển khai xong, nội dung đi vào các tài liệu chính thức
và file này bị xoá.

## 0. Phần dùng chung, làm trước cả hai nền tảng

Windows đã có và hai nền tảng kia dùng lại nguyên:

- `core/include/deskhub/session/host/SecureDesktopWire.h`: `SecureDesktopRequest`
  (Agent / Takeover / Configure), `kSecureDesktopAck`, `UnattendedConfig` và codec của nó, đã
  có test trong `core/tests/session/SecureDesktopWireTests.cpp`.
- `core/include/deskhub/ui/UiSettings.h`: `unattended`, lưu dưới key `unattended`.
- `platform/include/deskhubp/host/SharingHost.h`: `HoldWhileSharing`, với `hold_` được reset
  trong `Stop()` và khi `StartEngine` thất bại. Đây là chỗ takeover cắm vào trên mọi OS.
- Vòng đời: host của hệ thống chạy khi app không share; app gửi Takeover trước khi bind, giữ
  kết nối tới khi `Stop()`; daemon chạy lại host khi kết nối đóng.

Refactor nhỏ cần làm trước để không chép code Windows sang hai nơi nữa:

1. Chuyển cờ tiến trình `SetUnattendedMode` / `UnattendedMode` từ
   `client/windows/cpp/secure/UnattendedMode.{h,cpp}` lên
   `platform/include/deskhubp/host/UnattendedMode.h` + `platform/src/host/UnattendedMode.cpp`.
   Windows include đường mới. Cờ không chạm API của OS nên `platform/` là lớp đúng.
2. Tách chuỗi gợi ý theo OS trong `core/include/deskhub/ui/Strings.h`: giữ
   `kUnattendedLabel`; đổi `kUnattendedHint` thành `kUnattendedHintWindows`; thêm
   `kUnattendedHintLinux` và `kUnattendedHintMac`; hai note ("chưa cài daemon", "daemon không
   nhận") viết chung, không nhắc `services.msc`. Cập nhật `core/tests/ui/StringsTests.cpp`.
3. `core/include/deskhub/session/host/UnattendedControl.h`: state machine thuần
   `UnattendedControlServer` nhận từng byte, trả
   `Action{Agent | Takeover | Configure(config) | Refuse}`. Linux và macOS dùng nó để phần
   đọc socket không mang logic; Windows có thể chuyển sang sau. Test trong
   `core/tests/session/`.
4. Một helper thư mục máy POSIX: `platform/include/deskhubp/system/MachineFolder.h` +
   `platform/src/system/MachineFolderPosix.cpp` với `EnterMachineFolder(path)` (gọi
   `SetAppDataDir` + `SetConfigDir`), `UnattendedEnabled()` (có `host_key.pem`) và
   `WipeMachineFolder()`. `EnsurePrivateConfigDir` POSIX đòi chủ thư mục là euid, nên thư mục
   phải do uid chạy host tạo ra: root ở cả hai nền tảng. Test ownership bên cạnh
   `platform/tests/system/AppDataFileTests.cpp`.
5. Một helper Unix domain socket: `platform/include/deskhubp/net/LocalSocket.h` +
   `LocalSocketPosix.cpp`: listen / accept / connect và `PeerIdentity{pid, uid, exePath}`. API
   giống nhau trên hai OS; phần khác nhau (`SO_PEERCRED` với `/proc` trên Linux,
   `LOCAL_PEERPID` với `proc_pidpath` trên macOS) nằm trong `.cpp`.

Kênh app → daemon ở cả hai nền tảng là Unix socket, không phải D-Bus hay XPC, để codec
`UnattendedConfig` chạy trên byte stream y như trên pipe Windows. Daemon tin peer theo đường
dẫn exe ở nơi chỉ root ghi được, tương đương kiểm tra image path trên Windows:

- Linux: chỉ `/usr/bin/deskhub` (và chính `deskhubd`). Build từ source ở chỗ khác bị từ chối,
  và note trong app nói rõ.
- macOS: kiểm tra chữ ký code của peer qua Security.framework với requirement gồm identifier
  `com.deskhub.macos` và Team ID `UPJRMYQ38F`. Bản debug (`com.deskhub.macos.debug`, adhoc) chỉ
  được daemon debug chấp nhận.

Mỗi OS thêm `secure/ControlSocket.{h,cpp}` ở lớp client với `TakeoverUnattendedHost()` (trả
`shared_ptr<void>` giữ fd), `SendUnattendedConfig(config)` và `UnattendedDaemonInstalled()`.
`HoldWhileSharing(TakeoverUnattendedHost())` đặt ngay trước `StartEngine` trong
`client/linux/cpp/SharingHost.cpp:273` và `client/macos/app/cpp/SharingHost.cpp:145`, bỏ qua
khi `UnattendedMode()` bật.

## 1. Linux

### Thực trạng

- Host Linux chỉ chạy trong phiên desktop đã đăng nhập: capture qua xdg-desktop-portal trên
  session bus với hộp thoại chọn màn hình (`platform/src/media/PortalScreenCast.cpp`,
  `client/linux/cpp/capture/ScreenCapture.cpp`); `preflight` bắt buộc portal mở
  (`client/linux/cpp/SharingHost.cpp:94-99`). Không có X11, DRM/KMS hay wlroots capture.
- Encode không phụ thuộc phiên: VA-API chọn render node trong
  `client/linux/cpp/encode/VaDisplay.cpp:41-102`, NVENC qua CUDA device 0. Cả hai chạy được
  với root.
- Input là uinput trong `client/linux/cpp/input/InputInjector.cpp`: thiết bị ảo toàn hệ
  thống, greeter cũng nhận. Udev rule đã có trong gói.
- Không có systemd unit, polkit, root helper hay Unix socket. GDBus system bus tới logind đã
  link trong `platform/src/system/KeepAwakeLinux.cpp`.
- Màn hình đăng nhập là greeter (GDM trên Wayland; SDDM và LightDM trên X11) chạy trong phiên
  riêng; portal không hoạt động ở đó.

### Quyết định

- **Backend capture mới cho host unattended: DRM/KMS.** Đọc framebuffer đang scanout với
  quyền root qua libdrm, export PRIME thành dma-buf, đưa thẳng vào đường
  `FrameMemory::DmaBuf` → VA-API đã có (zero copy). Không phụ thuộc greeter hay compositor,
  và sau khi user đăng nhập vẫn capture tiếp phiên user mà không cần portal. Con trỏ:
  composite từ cursor plane trên đường Mapped; ở phiên 1 đường dma-buf không có con trỏ, spec
  ghi rõ.
- X11 là fallback cho driver Nvidia đóng (PRIME export không ổn định ở đó), chỉ khi phiên
  greeter là X11. Phiên 2.
- **Daemon là binary mới `deskhubd`** trong gói app (không phải `/usr/bin/deskhub` để khỏi
  kéo GTK; không phải CLI vì CLI là gói riêng). Link `deskhub_linux_core` và backend mới. Hai
  chế độ: `deskhubd` (daemon, root: socket + supervisor) và `deskhubd --unattended-host`
  (host, root, do daemon spawn và kill khi takeover).
- Supervisor làm như Windows: chạy host khi enabled, không có hold, và có ít nhất một CRTC
  đang bật; host tự thoát khi mất display, daemon chạy lại sau 2 giây. Không theo dõi logind ở
  phiên 1.
- Thư mục máy `/var/lib/deskhub`, root 0700, chứa cả log của daemon và host. Socket
  `/run/deskhub/control.sock`.
- Semantics giống Windows: host unattended chạy bất cứ khi nào app không share, kể cả khi
  user đã đăng nhập. "Host wins" qua `LocalInputMonitor` chạy root nên không cần nhóm `input`.

### Bước làm

1. `client/linux/cpp/capture/KmsCapture.{h,cpp}`: mở card, tìm CRTC đang bật, lấy
   framebuffer, PRIME fd, format và modifier; cung cấp `LinuxFrameInfo` dma-buf theo nhịp fps;
   theo dõi thay đổi mode để restart. Hợp đồng theo
   `core/include/deskhub/media/VideoContract.h`.
2. `client/linux/cpp/SharingHost.cpp`: khi `UnattendedMode()`, `preflight` không đòi portal,
   `source.create` nhận CRTC id làm `targetId`, `startCapture` dùng `KmsCapture` thay
   `ScreenCapture`. Thêm `platform/src/media/DisplayEnumKms.cpp` liệt kê CRTC cho host
   unattended, chọn theo `UnattendedMode()` trong `DisplayEnumLinux.cpp`.
3. `client/linux/daemon/main.cpp`, `Daemon.cpp` (socket, dispatch qua
   `UnattendedControlServer`, supervisor, áp cấu hình như `ApplyConfig` trên Windows),
   `UnattendedHost.cpp` (logic của `client/windows/service/UnattendedHost.cpp:47-80`). Target
   CMake `deskhubd` trong `client/linux/CMakeLists.txt`.
4. App GTK: checkbox trong `client/linux/gtk/MainWindow.cpp` `BuildGeneralSettings` (mục
   Launch, cạnh `autostartCheck_`, 1686-1693); `SaveSettings` (1838-1868) đẩy cấu hình; đẩy
   thêm sau khi approve, forget hay add thiết bị, khi share dừng và một lần lúc khởi động, như
   Windows. Mờ checkbox kèm note khi `UnattendedDaemonInstalled()` sai.
5. Đóng gói: unit `/usr/lib/systemd/system/deskhubd.service` (Type=simple, root,
   `After=systemd-logind.service`, `WantedBy=multi-user.target`) do
   `scripts/stage-linux-pkgroot.sh` đặt vào; `postinst` / `%post` chạy
   `systemctl enable --now`, `postrm` / `%preun` disable và stop; purge xoá `/var/lib/deskhub`.
   Cập nhật `%files` trong `scripts/build-rpm.sh` và kiểm tra cài đặt trong job Linux của
   `build-desktop.yml` (`systemctl is-enabled deskhubd`).
6. Gói CLI vẫn không có daemon; ghi rõ như Windows.
7. Tài liệu bốn ngôn ngữ: T-28 mở rộng (Linux: greeter nào, không con trỏ ở phiên 1), P-3;
   ARCHITECTURE §4 bullet Linux, §7 và §9; SECURITY; PRIVACY tăng version; INSTALL.
8. Test: core cho `UnattendedControlServer`; platform cho `MachineFolder` và `LocalSocket`
   (loopback giữa hai tiến trình con); CI `build-desktop` cài gói và kiểm tra unit.

### Phải xác minh trước khi viết nhiều code

- PRIME export của framebuffer scanout trên Intel / AMD (Mesa) và trên Nvidia đóng; nếu
  Nvidia không export được, phiên 1 dùng đường Mapped (copy CPU) ở đó.
- VA-API nhận dma-buf với modifier (tiled) mà KMS báo, hay cần linear.
- Greeter GDM trên Wayland có thật sự hiện trên CRTC như mong đợi (một vài cấu hình tắt màn
  hình tới khi có input; host có thể phải gửi một sự kiện chuột để đánh thức).
- uinput ở greeter: SDDM / GDM nhận thiết bị tạo sau khi greeter khởi động (libinput hot plug).

## 2. macOS

### Thực trạng

- Không có LaunchDaemon, LaunchAgent, XPC hay pkg; chỉ `SMAppService.mainAppService` cho
  Login Item (`platform/src/system/AutostartMac.mm`). App không sandbox, hardened runtime, ký
  Developer ID Application; chưa có Developer ID Installer nên chưa ký pkg được.
- Host chạy trong tiến trình app qua FFI `dh_share_start`
  (`platform/src/ffi/ShareFfi.cpp:118`). Capture là ScreenCaptureKit
  (`client/macos/app/cpp/capture/ScreenCapture.mm`), encode là VideoToolbox
  (`platform/src/media/VtEncoderApple.mm`), input là `CGEventPost`
  (`client/macos/app/cpp/input/InputInjector.mm`). Tất cả giả định đang ở trong phiên GUI của
  user đã đăng nhập.
- `platform/src/input/LocalInputMac.mm` dùng `dispatch_sync` lên main thread trong `Start` và
  `Stop`: tiến trình không có run loop chính sẽ treo khi `Stop` gọi từ thread khác.
- Xcode project chỉ có một target `app`, tự biên dịch `app/cpp/*`; thư viện CMake
  `deskhub_mac_core` chỉ dùng cho CLI. Dmg kéo thả; cask Homebrew là
  `packaging/homebrew/deskhub.rb`.
- Trang Settings macOS vẽ `PermissionsSection` tại ô `SettingField::Permissions`
  (`client/macos/app/swift/SettingsPage.swift:113-114`), đúng ô Windows đặt checkbox.

### Điều kiện và giới hạn phải ghi vào spec

- **FileVault.** Mac Apple Silicon bật FileVault mặc định; sau khi khởi động lại máy đứng ở
  màn hình mở khoá đĩa trước khi hệ điều hành chạy, không phần mềm nào chạm tới. Tính năng vì
  thế chỉ có tác dụng khi FileVault tắt, hoặc khi lần khởi động lại đó là
  `fdesetup authrestart`. Spec và INSTALL nói thẳng; hành động "authenticated restart" cho
  viewer là việc về sau.
- **Quyền hệ thống.** Host trước đăng nhập cần Screen Recording và Accessibility như app.
  Người dùng cấp trong System Settings cho binary của daemon (nằm trong bundle app), hoặc
  profile MDM cấp trong môi trường quản lý. App hiện trạng thái hai quyền này cho daemon và
  mở đúng pane, như `PermissionsSection` làm cho app.
- **Phạm vi phiên 1.** Host trước đăng nhập chỉ chạy ở login window (và màn hình khoá khi
  không ai đăng nhập). Khi user đăng nhập, launchd tháo phiên LoginWindow và host của daemon
  kết thúc; truy cập chỉ tiếp tục với T-13 + T-11 (Login Item + auto share), như hiện nay.
  Khác Windows và Linux, nơi host của daemon chạy bất cứ khi nào app không share. Chạy host
  trong phiên Aqua của user khi app không share là phiên 2.

### Quyết định

- **Hai tiến trình, một binary `DeskhubDaemon`** trong bundle app
  (`Contents/MacOS/DeskhubDaemon`), ký cùng Team ID:
  - `DeskhubDaemon` chạy root, đăng ký qua `SMAppService.daemon(plistName:)` với plist trong
    `Contents/Library/LaunchDaemons/com.deskhub.macos.daemon.plist`. Nó giữ socket điều khiển,
    nhận Configure từ app, giữ thư mục máy, ghi và gỡ plist LaunchAgent cho phiên LoginWindow,
    và theo dõi hold Takeover. Người dùng xác nhận một lần trong System Settings → Login
    Items ("Allow in background"); không cần pkg.
  - `DeskhubDaemon --unattended-host` chạy trong phiên LoginWindow qua LaunchAgent
    `/Library/LaunchAgents/com.deskhub.macos.unattended.plist` (`LimitLoadToSessionType`
    `LoginWindow`, `RunAtLoad`, `KeepAlive`), do daemon ghi khi bật và xoá khi tắt. Nó chạy
    `SharingHost` như `client/windows/service/UnattendedHost.cpp:47-80`, có run loop chính để
    `LocalInputMonitor` an toàn, và thoát khi `Stop`.
- **Capture ở login window:** thử ScreenCaptureKit trước; nếu `SCShareableContent` không trả
  gì trong phiên LoginWindow thì dùng backend `CGDisplayStream` mới
  (`client/macos/app/cpp/capture/DisplayStreamCapture.mm`), chọn theo `UnattendedMode()`. Cả
  hai cho IOSurface vào `OfferVtFrame` như hiện nay. Đây là điểm xác minh đầu tiên.
- **Input:** `CGEventPost` như app. Xác minh gõ được vào ô mật khẩu của login window.
- Thư mục máy `/Library/Application Support/Deskhub`, root 0700; socket
  `/var/run/deskhub/control.sock`. Log của daemon và host vào thư mục máy.
- Takeover vẫn gửi trong `client/macos/app/cpp/SharingHost.cpp` trước `StartEngine` cho đồng
  nhất (ít gặp ở phiên 1 vì host đã kết thúc khi user đăng nhập, nhưng cần cho fast user
  switching).

### Bước làm

1. Xcode: target `daemon` (command-line tool) biên dịch `SharingHost.cpp`, hai backend
   capture, `InputInjector.mm`, `Permissions.mm` và các file mới; link `libcore` +
   `libplatform_bundled`; Copy Files vào `Contents/MacOS` và plist vào
   `Contents/Library/LaunchDaemons`. `make/macos.mk` ký binary nhúng trong `dist-macos`;
   `verify-macos` kiểm tra chữ ký của nó.
2. `client/macos/app/cpp/daemon/Daemon.mm` (socket, dispatch qua `UnattendedControlServer`,
   áp cấu hình, ghi / xoá plist LaunchAgent, hold) và `UnattendedHost.mm`.
3. `secure/ControlSocket.mm` phía app cùng FFI chỉ cho app trong
   `client/macos/app/cpp/DeskhubBridge.h`: `dh_unattended_installed`,
   `dh_unattended_enable(bool)`, `dh_unattended_push`, `dh_unattended_permissions` (trạng thái
   Screen Recording / Accessibility của daemon).
4. Swift: checkbox và hai dòng quyền trong `client/macos/app/swift/PermissionsSection.swift`;
   `SharingModel.save()`, `startSharing` và `stopSharing` gọi `SyncUnattended`; các model
   thiết bị (`client/apple/swift/AccessRequestsModel.swift`,
   `client/macos/app/swift/DevicesPage.swift`) gọi sau approve / forget / add.
5. Đóng gói: cask thêm `uninstall launchctl:` cho hai label và `zap` cho thư mục máy cùng
   plist LaunchAgent; `make dist-macos` giữ nguyên quy trình notarize.
6. Tài liệu bốn ngôn ngữ: T-28 mở rộng với phần macOS (FileVault, quyền, chỉ login window ở
   phiên 1), P-2; ARCHITECTURE §4 bullet macOS, §7 và §9 (vì sao daemon cộng agent
   LoginWindow, vì sao không pkg); SECURITY (daemon root, thư mục máy, daemon tin app qua chữ
   ký thế nào); PRIVACY tăng version; INSTALL mục macOS.
7. Test: core và platform dùng chung với Linux; không có Swift test. Job macOS của
   `build-desktop` kiểm tra binary nhúng có trong bundle và `codesign --verify --strict` qua.

### Phải xác minh trước khi viết nhiều code

- Spike 1: LaunchAgent LoginWindow chạy `DeskhubDaemon --unattended-host` adhoc trên Mac dev
  capture được login window bằng ScreenCaptureKit, hay cần `CGDisplayStream`, và cấp quyền
  cho binary đó trong System Settings là đủ.
- Spike 2: `CGEventPost` từ tiến trình đó gõ được vào ô mật khẩu login window.
- Spike 3: `SMAppService.daemon` với plist trong bundle hoạt động cho bản Developer ID và bản
  adhoc, và trải nghiệm "Allow in background" chấp nhận được.
- `LocalInputMonitor` với run loop chính trong host headless không treo khi `Stop`.

## 3. Thứ tự và ước lượng

| Việc | Phụ thuộc | Ước lượng |
| --- | --- | --- |
| §0 phần dùng chung | không | 1–2 ngày |
| Linux: KMS capture + daemon + gói | §0 | 5–8 ngày, cộng 2 nếu cần X11 fallback |
| Linux: UI app + docs | daemon | 2 ngày |
| macOS: spike 1–3 | §0 | 2 ngày, rồi quyết định đi tiếp hay dừng |
| macOS: daemon + agent + Xcode + cask | spike | 5–7 ngày |
| macOS: UI app + docs | daemon | 2 ngày |

Làm Linux trước nếu máy thử là Linux; làm macOS trước nếu spike cho kết quả tốt, vì khối lượng
macOS đồng đều hơn (một backend, một greeter) còn Linux phụ thuộc driver.

## 4. Rủi ro chung

- Thư mục máy chứa bản sao khoá riêng, chỉ root đọc: ghi vào SECURITY và PRIVACY như Windows,
  và gỡ cài đặt phải xoá.
- Daemon root nhận dữ liệu từ app: chỉ dưới dạng `UnattendedConfig` với giới hạn kích thước
  đã có trong core; không bao giờ thực thi gì dựa trên nội dung nhận được.
- Ctrl+Alt+Del không áp dụng cho hai nền tảng này; không có giới hạn tương đương.
- Cả hai nền tảng đều không bật được máy; wake-on-LAN vẫn ngoài phạm vi.
