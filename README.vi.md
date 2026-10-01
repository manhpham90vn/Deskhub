[English](README.md) · **Tiếng Việt** · [中文](README.zh.md) · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### Dùng máy tính của bạn, dù đang ở thiết bị nào.

**Deskhub giúp bạn xem và điều khiển máy tính từ một thiết bị khác. App mã nguồn mở,
chạy native trên năm nền tảng và hướng tới trải nghiệm mượt mà khi làm việc hoặc chơi game.**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/ch%E1%BA%A1y%20tr%C3%AAn-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#platforms)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[Cài đặt](#install)** · [Build từ source](docs/BUILD.vi.md) ·
[Spec](docs/SPECIFICATION.vi.md) · [Architecture](docs/ARCHITECTURE.vi.md) ·
[Security](SECURITY.vi.md)

</div>

**Mục lục:**

- [Cài đặt](#install)
- [Demo](#demo)
- [Giới thiệu](#about)
- [Vì sao](#why)
- [Nền tảng](#platforms)
- [Bên trong có gì](#features)
- [Tài liệu](#docs)
- [License](#license)

<a id="install"></a>

## 📦 Cài đặt

Có thể cài Deskhub qua package manager trên Windows, macOS, Ubuntu, Debian hoặc Mint:

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

> **winget chưa dùng được.** Các gói Deskhub vẫn đang chờ duyệt trên winget-pkgs
> ([#439628](https://github.com/microsoft/winget-pkgs/pull/439628) cho app, [#439629](https://github.com/microsoft/winget-pkgs/pull/439629) cho CLI), nên cả hai
> lệnh `winget install` sẽ báo lỗi cho tới khi chúng được merge. Trong lúc chờ, hãy tải bộ cài
> Windows từ [Releases](https://github.com/manhpham90vn/Deskhub/releases).

Trên Ubuntu / Debian / Mint, app desktop và CLI là hai gói riêng. Thêm apt repository
một lần, sau đó cài gói cần dùng hoặc cả hai:

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update
sudo apt install deskhub       # app desktop
sudo apt install deskhub-cli   # CLI; cũng cài riêng được
```

Gói desktop thêm Deskhub vào menu ứng dụng và cài lệnh `deskhub` trong `/usr/bin`; gói
CLI cài `deskhub-cli` trong `/usr/bin`. Bộ cài Windows tạo shortcut trong Start Menu và
Desktop; bộ cài CLI riêng thêm lệnh vào `PATH` của người dùng, còn winget cài CLI dưới
dạng exe portable nằm trên `PATH`. Homebrew đưa CLI trên macOS vào `PATH`.

Các nền tảng còn lại lấy trên [Releases](https://github.com/manhpham90vn/Deskhub/releases):

- **Bản tải cho Windows** — bộ cài app `deskhub-v*-windows-setup.exe` và bộ cài CLI
  `deskhub-cli-v*-windows-setup.exe`; vẫn có các file exe portable
- **Bản tải cho macOS** — app là `deskhub-v*-macos.dmg`, CLI là `deskhub-cli-v*-macos`
- **Ubuntu / Debian / Mint không dùng repository** — `deskhub-v*-amd64.deb` và
  `deskhub-cli-v*-amd64.deb`
- **Fedora / openSUSE** — RPM riêng cho app `deskhub-v*-x86_64.rpm` và CLI
  `deskhub-cli-v*-x86_64.rpm`; cài gói cần dùng bằng `dnf` hoặc `zypper`
- **Arch, Linux khác** — bản portable `deskhub-v*-linux-x86_64` và
  `deskhub-cli-v*-linux-x86_64`, `chmod +x` rồi chạy
- **Android** — `deskhub-v*-android.apk`, hoặc [bản beta trên Play](https://play.google.com/apps/testing/com.manhpham.deskhub)
- **iOS** — [TestFlight](https://testflight.apple.com/join/7qY7wgpd)

Chi tiết và quyền cần cấp trên từng nền tảng: [INSTALL.vi.md](docs/INSTALL.vi.md).

<a id="demo"></a>

## 👀 Demo

<div align="center">

<img src="docs/imgs/macos_1.png" alt="Trang Host của Deskhub trên macOS: ô chọn Share on network, các địa chỉ Wi-Fi và Tailscale để máy khác connect tới, banner Not sharing trên UDP port 47777, và danh sách source với Terminal được chọn, phía trên nút Start sharing" width="850">

<sub>Host macOS trước khi share: chọn những gì được phép rời khỏi máy — display bất kỳ, shell, hoặc cả hai — rồi bấm <b>Start sharing</b>.</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="Trang Client của Deskhub trên macOS: ô địa chỉ host và UDP port, các checkbox chọn remote desktop, control và terminal, nút Connect, cùng danh sách địa chỉ gần đây với thời điểm connect gần nhất của từng địa chỉ">
      <br><sub><b>Client</b> — nhập địa chỉ, dán link QR của một host, hoặc chọn một host gần đây hay đã trust, sau đó chọn nội dung cần mở: màn hình, quyền control, shell, hoặc kết hợp.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="Trang Devices của Deskhub trên macOS: một khu vực cho máy này khi làm host, với fingerprint key SHA256 của nó, nút Copy public key và các client key được phép connect, và một khu vực cho máy này khi làm client, với các host đã trust">
      <br><sub><b>Devices</b> — khi làm host: key của máy này và các thiết bị được phép vào — đã approve, đã scan hoặc đã dán — có thể gỡ từng thiết bị. Khi làm client: các host bạn đã trust, theo key.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="Trang Settings của Deskhub trên macOS: tên thiết bị, fps, bitrate và quality, UDP port, switch cho phép viewer control máy này, các switch clipboard và chống sleep, trạng thái hiện thời của permission Screen Recording và Accessibility, cùng switch khởi động khi đăng nhập">
      <br><sub><b>Settings</b> — tên thiết bị, fps, bitrate, quality, port, cho phép viewer control máy này hay không, và trạng thái hiện thời của các permission macOS.</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="Trang Client của Deskhub trên iOS: các ô địa chỉ và port, nút Connect và Terminal, switch control máy từ xa, và danh sách địa chỉ gần đây" width="195">
  <img src="docs/imgs/ios_2.png" alt="Trang Host của Deskhub trên iOS: Share on network, Start sharing, và các địa chỉ IP để máy khác connect tới" width="195">
  <img src="docs/imgs/ios_3.png" alt="Trang Devices của Deskhub trên iOS: key SHA256 của thiết bị này, các client key được phép connect và các host đã trust" width="195">
  <img src="docs/imgs/ios_4.png" alt="Trang settings kết nối của Deskhub trên iOS: UDP port, cùng các switch sync clipboard và giữ thiết bị không sleep" width="195">
</p>
<p align="center"><sub><b>iPhone</b> — vẫn bốn trang đó. Scan mã QR của một host để connect, dùng khung video như trackpad để điều khiển; hoặc host màn hình của chính điện thoại ở chế độ view-only.</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Trang Client của Deskhub trên Android: các ô địa chỉ và port, nút Connect và Terminal, checkbox control, và danh sách địa chỉ gần đây" width="195">
  <img src="docs/imgs/android_2.png" alt="Trang Host của Deskhub trên Android: Share on network, Start sharing, và các địa chỉ IP để máy khác connect tới" width="195">
  <img src="docs/imgs/android_3.png" alt="Trang Devices của Deskhub trên Android: key SHA256 của thiết bị này, các client key được phép connect và các host đã trust" width="195">
  <img src="docs/imgs/android_4.png" alt="Trang settings kết nối của Deskhub trên Android: UDP port, cùng các checkbox sync clipboard và giữ thiết bị không sleep" width="195">
</p>
<p align="center"><sub><b>Android</b> — vẫn bốn trang đó, theo Material Design. Khi làm host, Android 10+ chỉ share màn hình ở chế độ view-only.</sub></p>

<a id="about"></a>

## 📖 Giới thiệu

Chọn màn hình cần share trên một thiết bị, rồi nhập địa chỉ IP để connect từ thiết bị khác.
Bốn trang **Host**, **Client**, **Devices** và **Settings** có mặt trên mọi nền tảng, nên
cách dùng vẫn quen thuộc khi chuyển từ Mac sang PC hoặc điện thoại. Một **core C++20** dùng
chung xử lý protocol cho cả năm nền tảng.

| ⚡ Nhanh | 📦 Dễ cài đặt | 🎛️ Đơn giản |
| ------ | ---------- | --------- |
| Mặc định stream ở 60 fps, tối đa 240 fps trên phần cứng phù hợp. Đường xử lý video dùng bộ nhớ GPU khi có thể. | Cài qua package manager hoặc tải bản release. Không cần tài khoản hay background service. | **Share** một display hoặc **Connect** tới một IP. Máy desktop còn share được **shell**, và mọi host — kể cả điện thoại — đều nhận được **file**; điện thoại share màn hình ở chế độ view-only. |

Session được encrypt end-to-end trên **QUIC/TLS**, và quyền truy cập hoạt động như SSH: mỗi
máy có một key, và client chỉ được vào khi chủ host đã cho key đó vào — bằng cách approve
yêu cầu kết nối của nó, cho nó xem **mã QR** trong lúc share, hoặc dán public key của nó.
Client ghim key của host và kiểm tra trước khi gửi bất cứ thứ gì; trust đi theo key, nên
host vẫn giữ được trust khi đổi địa chỉ. Không passcode, không discovery, không công tắc nào
cho người lạ vào. Hãy dùng network tin cậy hoặc VPN, và **không port-forward UDP 47777**.
Threat model đầy đủ nằm trong [`SECURITY.vi.md`](SECURITY.vi.md).

<a id="why"></a>

## 💡 Vì sao

- 💻 **Công việc** — chạy Claude Code, VS Code hoặc build trên PC ở nhà, từ một laptop cấu hình thấp hoặc từ iPad.
- 🌐 **Ứng dụng desktop** — dùng Chrome, Office hoặc phần mềm chỉ có trên máy tính từ một thiết bị khác.
- 🎮 **Game** — tối đa 240 fps, relative mouse và scancode DirectInput, pointer lock bằng `F9`.
- 🖥️ **Nhiều display** — share một hoặc nhiều display, mỗi display là một session riêng.

<a id="platforms"></a>

## 🚦 Nền tảng

| Nền tảng | Host | Client | Trạng thái |
| -------- | :--: | :----: | ---------- |
| **Windows** | ✅ | ✅ | Bản reference — sử dụng hằng ngày qua LAN và Tailscale (Internet/NAT) |
| **macOS** | ✅ | ✅ | Cả hai vai trò đều hoạt động (ScreenCaptureKit + VideoToolbox + CGEvent) |
| **Android** | ✅ | ✅ | Client: video và input (trackpad, keyboard). Host: share màn hình view-only (MediaProjection + MediaCodec), Android 10+ — đang thử nghiệm trên Google Play |
| **iOS** | ✅ | ✅ | Client: video và input (trackpad, keyboard). Host: share màn hình view-only qua Broadcast Upload Extension (ReplayKit + VideoToolbox) — đang thử nghiệm qua TestFlight |
| **Linux** | ✅ | ✅ | Cả hai vai trò đều hoạt động (PipeWire + VA-API/NVENC + uinput + GTK3) — Ubuntu, Debian, Mint, Fedora, openSUSE, Arch qua deb / rpm / binary chạy trực tiếp; đã kiểm chứng giữa hai máy trong LAN |

<a id="features"></a>

## ✨ Bên trong có gì

- **Đường xử lý video trên GPU** — capture, encode, decode và render dùng phần cứng của từng nền tảng khi có thể; đường NVENC trên Windows tránh sao chép frame qua CPU, còn host Linux encode qua VA-API hoặc, trên GPU NVIDIA, qua NVENC.
- **Protocol riêng chạy trên QUIC** — GOP vô hạn kết hợp IDR theo yêu cầu, XOR FEC kèm gửi lại packet bị mất, adaptive bitrate, tất cả được multiplex trên một connection đã encrypt.
- **Âm thanh đi kèm hình ảnh** — audio mix của chính máy đó, Opus 64 kbps, mỗi datagram chứa một frame 20 ms. Mất một packet đơn lẻ thường được dựng lại từ packet kế tiếp nhờ FEC in-band của Opus, một khoảng mất dài hơn chỉ tốn một phần nhỏ của giây, và cả hai đều không ảnh hưởng tới hình ảnh. Không bao giờ capture microphone.
- **Không chỉ có màn hình** — remote shell, gửi file tới mọi host, sync clipboard dạng text thuần theo cả hai chiều, và giữ máy không sleep khi đang có session.
- **Input thật** — relative mouse (Raw Input) và scancode cho game DirectInput. Mouse và keyboard tại máy host luôn được ưu tiên (trên Linux, khi user đã ở trong nhóm `input` — xem [Install](docs/INSTALL.vi.md#host-wins)).
- **Core dùng chung** — protocol, FEC và bitrate control nằm trong `core/`, được compile vào mọi client.
- **Công cụ command line** — `deskhub-cli` share màn hình, mở remote shell, gửi file, quản lý key, thiết bị được phép, host đã trust và settings, chạy từ script hoặc qua SSH; các lệnh liệt kê có `--json`. Trên Windows và Linux, lệnh này còn mở được cửa sổ xem màn hình từ xa. Xem [Build](docs/BUILD.vi.md#command-line-client).
- **Được kiểm thử kỹ** — core có unit test chạy offline; CI chạy thêm ASan, UBSan và TSan. Chín libFuzzer target — 30 giây mỗi target trên mọi pull request, 15 phút mỗi target mỗi đêm — kiểm tra wire format, phần parse H.264, reassembly, byte stream của terminal, chuỗi UI, phần parse key và link mời, encode QR và các session state machine. Crash được phát hiện sẽ thành regression test.

<a id="docs"></a>

## 📚 Tài liệu

Mọi tài liệu đều được xuất bản bằng tiếng Anh, kèm bản dịch đặt bên cạnh: tiếng Việt
`*.vi.md`, tiếng Trung `*.zh.md`, tiếng Nhật `*.ja.md`. Bản tiếng Anh là bản chuẩn.

| Tài liệu | Nội dung |
| --- | --- |
| [Install](docs/INSTALL.vi.md) ([en](docs/INSTALL.md) · [zh](docs/INSTALL.zh.md) · [ja](docs/INSTALL.ja.md)) | Cài Deskhub trên từng nền tảng trong năm nền tảng |
| [Build](docs/BUILD.vi.md) ([en](docs/BUILD.md) · [zh](docs/BUILD.zh.md) · [ja](docs/BUILD.ja.md)) | Compile từ source, test, đóng gói, release |
| [Specification](docs/SPECIFICATION.vi.md) ([en](docs/SPECIFICATION.md) · [zh](docs/SPECIFICATION.zh.md) · [ja](docs/SPECIFICATION.ja.md)) | Deskhub làm được gì, không đề cập chi tiết triển khai |
| [Architecture](docs/ARCHITECTURE.vi.md) ([en](docs/ARCHITECTURE.md) · [zh](docs/ARCHITECTURE.zh.md) · [ja](docs/ARCHITECTURE.ja.md)) | Các layer, thread, wire protocol và các quyết định thiết kế |
| [`SECURITY.vi.md`](SECURITY.vi.md) ([en](SECURITY.md) · [zh](SECURITY.zh.md) · [ja](SECURITY.ja.md)) | Threat model và cách báo lỗ hổng |
| [`PRIVACY.vi.md`](PRIVACY.vi.md) ([en](PRIVACY.md) · [zh](PRIVACY.zh.md) · [ja](PRIVACY.ja.md)) | Chính sách quyền riêng tư |
| [`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md) ([en](THIRD_PARTY_NOTICES.md) · [zh](THIRD_PARTY_NOTICES.zh.md) · [ja](THIRD_PARTY_NOTICES.ja.md)) | Thành phần bên thứ ba và license |

Báo lỗi và góp ý: [issues](https://github.com/manhpham90vn/Deskhub/issues) — vui lòng ghi
kèm model thiết bị.

<a id="license"></a>

## 📄 License

MIT — xem [`LICENSE`](LICENSE). Các thành phần bên thứ ba cùng thông báo license của chúng
(bao gồm bản FFmpeg LGPL được link tĩnh trong app và CLI Linux) được liệt kê trong
[`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md).
