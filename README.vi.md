[English](README.md) · **Tiếng Việt** · [中文](README.zh.md) · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### Máy của bạn, trên mọi màn hình bạn có.

**Open-source. Native. Đa nền tảng. Remote desktop cho cảm giác như đang dùng máy tại chỗ
— nhanh tới mức chơi được game từ xa, điều mà các công cụ remote desktop thông thường
không làm được.**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/ch%E1%BA%A1y%20tr%C3%AAn-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-nền-tảng)

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

Cài qua package manager, vừa cài vừa được tự cập nhật — Windows, macOS, và Ubuntu / Debian / Mint:

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

Trên Ubuntu / Debian / Mint, package `deskhub` cài cả app lẫn `deskhub-cli`:

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

Các nền tảng còn lại lấy trên [Releases](https://github.com/manhpham90vn/Deskhub/releases):

- **Fedora / openSUSE** — `sudo dnf install ./deskhub-v*-x86_64.rpm` (hoặc `zypper install`)
- **Arch, Linux khác** — bản portable `deskhub-v*-linux-x86_64`, `chmod +x` rồi chạy
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
      <img src="docs/imgs/macos_2.png" alt="Trang Client của Deskhub trên macOS: IP của host, UDP port, ô passcode và tên máy, các checkbox chọn remote desktop, control và terminal, nút Connect, cùng bảng thiết bị với status, ping và thời điểm connect gần nhất">
      <br><sub><b>Client</b> — nhập IP hoặc chọn một máy mà scan tìm được, sau đó chọn nội dung cần mở: màn hình, quyền control, shell, hoặc kết hợp.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="Trang Devices của Deskhub trên macOS: các máy đã pair kèm key, thời điểm pair và thời điểm thấy gần nhất, nút Forget và Forget every machine, switch cho phép pair máy mới, và key SHA256 của máy này">
      <br><sub><b>Devices</b> — danh sách mọi máy đã được chấp nhận, kèm tên và key, có thể gỡ từng máy. Sau khi pair xong các máy cần dùng, có thể tắt việc pair máy mới.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="Trang Settings của Deskhub trên macOS: fps, bitrate và quality, UDP port, passcode dùng để pair, switch cho phép viewer control máy này, các switch clipboard và chống sleep, trạng thái hiện thời của permission Screen Recording và Accessibility, cùng switch khởi động khi đăng nhập">
      <br><sub><b>Settings</b> — fps, bitrate, quality, port, passcode, cho phép viewer control máy này hay không, và trạng thái hiện thời của các permission macOS.</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="Trang Client của Deskhub trên iOS: các ô IP, port, passcode và tên, nút Connect và Terminal, switch control máy từ xa, và scan báo số địa chỉ đã kiểm tra" width="195">
  <img src="docs/imgs/ios_2.png" alt="Trang Host của Deskhub trên iOS: passcode dùng để pair, Share on network, Start sharing, và các địa chỉ IP để máy khác connect tới" width="195">
  <img src="docs/imgs/ios_3.png" alt="Trang Devices của Deskhub trên iOS: danh sách máy đã pair còn trống, switch cho phép pair máy mới, và key SHA256 của thiết bị này" width="195">
  <img src="docs/imgs/ios_4.png" alt="Trang settings kết nối của Deskhub trên iOS: UDP port mà scan kiểm tra, cùng các switch sync clipboard và giữ thiết bị không sleep" width="195">
</p>
<p align="center"><sub><b>iPhone</b> — vẫn bốn trang đó. Scan, chọn một máy, dùng khung video như trackpad để điều khiển; hoặc host màn hình của chính điện thoại ở chế độ view-only.</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Trang Client của Deskhub trên Android: các ô IP, port, passcode và tên, nút Connect và Terminal, checkbox control, và scan đang chạy qua subnet" width="195">
  <img src="docs/imgs/android_2.png" alt="Trang Host của Deskhub trên Android: passcode dùng để pair, Share on network, Start sharing, và các địa chỉ IP để máy khác connect tới" width="195">
  <img src="docs/imgs/android_3.png" alt="Trang Devices của Deskhub trên Android: danh sách máy đã pair còn trống, checkbox cho phép pair máy mới, và key SHA256 của thiết bị này" width="195">
  <img src="docs/imgs/android_4.png" alt="Trang settings kết nối của Deskhub trên Android: UDP port mà scan kiểm tra, cùng các checkbox sync clipboard và giữ thiết bị không sleep" width="195">
</p>
<p align="center"><sub><b>Android</b> — vẫn bốn trang đó, theo Material Design. Khi làm host, Android 10+ chỉ share màn hình ở chế độ view-only.</sub></p>

<a id="about"></a>

## 📖 Giới thiệu

Một **core C++20** duy nhất chạy trên mọi nền tảng, từ Windows đến iPhone, không phải viết
lại protocol. Share một display, nhập IP trên máy còn lại là có thể điều khiển máy đó. Bốn
trang giống nhau trên mọi nền tảng — **Host**, **Client**, **Devices**, **Settings** —
nên khi đã quen trên macOS thì dùng được ngay app Android.

| ⚡ Nhanh | 📦 Một file | 🎛️ Đơn giản |
| ------ | ---------- | --------- |
| **~3.5 ms** từ capture đến khi hiển thị, 60 fps. Pipeline zero-copy nằm hoàn toàn trong VRAM; hot path không đi qua CPU. | Không installer, không background service, không tài khoản. Toàn bộ app Windows là một file exe **~5.1 MB**; bản macOS là file dmg **1.9 MB**. | **Share** một display hoặc **Connect** tới một IP. Máy desktop share thêm được một **shell** và nhận **file** do viewer gửi. Điện thoại cũng host được nhưng chỉ view-only, vì không OS di động nào cho phép app inject input. |

Session được encrypt end-to-end trên **QUIC/TLS**. Máy lạ chỉ được chấp nhận khi chứng
minh được mình biết passcode của host — thông qua **SPAKE2**, nên passcode không bao giờ
được truyền đi — hoặc khi người dùng tại host chấp thuận. Dù vậy đó vẫn chỉ là một chuỗi bí mật
ngắn nằm trên port đang mở: hãy dùng network tin cậy hoặc VPN, và **không port-forward
UDP 47777**. Threat model đầy đủ nằm trong [`SECURITY.vi.md`](SECURITY.vi.md).

<a id="why"></a>

## 💡 Vì sao

- 💻 **Công việc** — chạy Claude Code, VS Code hoặc build trên PC ở nhà, từ một laptop cấu hình thấp hoặc từ iPad.
- 🌐 **Mọi thứ** — điều khiển Chrome, Office hoặc phần mềm chỉ chạy trên PC, từ bất kỳ thiết bị nào.
- 🎮 **Game** — 60 fps, relative mouse và scancode DirectInput, pointer lock bằng `F9`.
- 🖥️ **Nhiều display** — share một hoặc nhiều display, mỗi display là một session riêng.

<a id="platforms"></a>

## 🚦 Nền tảng

| Nền tảng | Host | Client | Trạng thái |
| -------- | :--: | :----: | ---------- |
| **Windows** | ✅ | ✅ | Bản reference — sử dụng hằng ngày qua LAN và Tailscale (Internet/NAT) |
| **macOS** | ✅ | ✅ | Cả hai vai trò đều hoạt động (ScreenCaptureKit + VideoToolbox + CGEvent) |
| **Android** | ✅ | ✅ | Client: video và input (trackpad, keyboard). Host: share màn hình view-only (MediaProjection + MediaCodec), Android 10+ — đang thử nghiệm trên Google Play |
| **iOS** | ✅ | ✅ | Client: video và input (trackpad, keyboard). Host: share màn hình view-only qua Broadcast Upload Extension (ReplayKit + VideoToolbox) — đang thử nghiệm qua TestFlight |
| **Linux** | ✅ | ✅ | Cả hai vai trò đều hoạt động (PipeWire + VA-API + uinput + GTK3) — Ubuntu, Debian, Mint, Fedora, openSUSE, Arch qua deb / rpm / binary chạy trực tiếp; đã kiểm chứng giữa hai máy trong LAN |

<a id="features"></a>

## ✨ Bên trong có gì

- **Zero-copy từ đầu đến cuối** — capture trực tiếp vào VRAM → NVENC → hardware decode → render. Hot path không đi qua CPU.
- **Protocol riêng chạy trên QUIC** — GOP vô hạn kết hợp IDR theo yêu cầu, XOR FEC, adaptive bitrate, tất cả được multiplex trên một connection đã encrypt.
- **Âm thanh đi kèm hình ảnh** — audio mix của chính máy đó, Opus 64 kbps, mỗi datagram chứa một frame 20 ms. Mất một packet chỉ mất một phần nhỏ của giây và không ảnh hưởng tới hình ảnh. Không bao giờ capture microphone.
- **Input thật** — relative mouse (Raw Input) và scancode cho game DirectInput. Mouse và keyboard tại máy host luôn được ưu tiên.
- **Core dùng chung** — protocol, FEC và bitrate control nằm trong `core/`, được compile vào mọi client.
- **Có cả command line** — `deskhub-cli` share màn hình, mở remote shell và điều khiển host từ script hoặc qua SSH, không cần GUI toolkit. Xem [Build](docs/BUILD.vi.md#command-line-client).
- **Được kiểm thử kỹ** — core có unit test chạy offline. CI chạy thêm ASan, UBSan và TSan. Bảy libFuzzer target kiểm tra wire format, phần parse H.264, reassembly, byte stream của terminal, chuỗi UI và các session state machine mỗi đêm. Mỗi crash phát hiện được bổ sung thành một regression test.

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
(bao gồm bản FFmpeg LGPL được link tĩnh trong app Linux) được liệt kê trong
[`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md).
