[English](README.md) · **Tiếng Việt** · [中文](README.zh.md) · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### Máy của bạn, trên mọi màn hình bạn có.

**Mã nguồn mở. Native. Đa nền tảng. Remote desktop mượt như ngồi tại máy — nhanh và thô
đủ để chơi game từ xa thật sự, điều mà các công cụ remote desktop thông thường không làm nổi.**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/ch%E1%BA%A1y%20tr%C3%AAn-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-nền-tảng)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[Cài đặt](docs/INSTALL.vi.md)** · [Build từ mã nguồn](docs/BUILD.vi.md) ·
[Đặc tả](docs/SPECIFICATION.vi.md) · [Kiến trúc](docs/ARCHITECTURE.vi.md) ·
[Bảo mật](SECURITY.vi.md)

</div>

## 👀 Xem thử

<div align="center">

<img src="docs/imgs/macos_1.png" alt="Trang Host của Deskhub trên macOS: ô chọn Share on network, các địa chỉ Wi-Fi và Tailscale để máy khác kết nối tới, khung Not sharing trên cổng UDP 47777, và danh sách nguồn với Terminal đang được tick phía trên nút Start sharing" width="850">

<sub>Một host macOS, chỉ còn một cú tick nữa là chia sẻ: chọn thứ được phép rời khỏi máy này — màn hình bất kỳ, cái shell, hay cả hai — rồi bấm <b>Start sharing</b>.</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="Trang Client của Deskhub trên macOS: IP của host, cổng UDP, ô mật mã và tên máy, các ô tick chọn màn hình từ xa, điều khiển và terminal, nút Connect, cùng bảng thiết bị với trạng thái, ping và lần kết nối gần nhất">
      <br><sub><b>Client</b> — gõ một IP hoặc bấm vào máy mà bản quét mạng tìm thấy, rồi chọn mở cái gì: màn hình, quyền điều khiển, một shell, hay kết hợp tuỳ ý.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="Trang Devices của Deskhub trên macOS: các máy đã ghép cặp kèm khoá, thời điểm ghép cặp và lần thấy gần nhất, nút Forget và Forget every machine, công tắc cho phép ghép cặp mới, và khoá SHA256 của máy này">
      <br><sub><b>Devices</b> — mọi máy từng được cho vào, theo tên và khoá, gỡ được từng cái; tắt ghép cặp mới khi các máy của bạn đã nằm trong danh sách.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="Trang Settings của Deskhub trên macOS: fps, bitrate và chất lượng, cổng UDP, mật mã ghép cặp, công tắc cho phép người xem điều khiển máy này, các nút bật clipboard và chống ngủ, trạng thái hiện thời của quyền Screen Recording và Accessibility, cùng công tắc khởi động cùng máy">
      <br><sub><b>Settings</b> — fps, bitrate, chất lượng, cổng, mật mã, cho phép người xem điều khiển máy này hay không, và trạng thái hiện thời của các quyền macOS.</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="Trang Client của Deskhub trên iOS: các ô IP, cổng, mật mã và tên, nút Connect và Terminal, công tắc điều khiển máy từ xa, và bản quét mạng báo đã kiểm tra bao nhiêu địa chỉ" width="195">
  <img src="docs/imgs/ios_2.png" alt="Trang Host của Deskhub trên iOS: mật mã ghép cặp, Share on network, Start sharing, và các địa chỉ IP để máy khác kết nối tới" width="195">
  <img src="docs/imgs/ios_3.png" alt="Trang Devices của Deskhub trên iOS: danh sách máy đã ghép cặp còn trống, công tắc cho phép ghép cặp mới, và khoá SHA256 của thiết bị này" width="195">
  <img src="docs/imgs/ios_4.png" alt="Trang cài đặt kết nối của Deskhub trên iOS: cổng UDP mà bản quét tìm trên đó, cùng các công tắc đồng bộ clipboard và giữ máy thức" width="195">
</p>
<p align="center"><sub><b>iPhone</b> — vẫn bốn trang đó. Quét mạng, chạm vào một máy, dùng khung hình như bàn di chuột để điều khiển; hoặc chia sẻ màn hình của chính điện thoại, chỉ để xem.</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Trang Client của Deskhub trên Android: các ô IP, cổng, mật mã và tên, nút Connect và Terminal, ô tick điều khiển, và bản quét mạng đang chạy qua dải mạng" width="195">
  <img src="docs/imgs/android_2.png" alt="Trang Host của Deskhub trên Android: mật mã ghép cặp, Share on network, Start sharing, và các địa chỉ IP để máy khác kết nối tới" width="195">
  <img src="docs/imgs/android_3.png" alt="Trang Devices của Deskhub trên Android: danh sách máy đã ghép cặp còn trống, ô tick cho phép ghép cặp mới, và khoá SHA256 của thiết bị này" width="195">
  <img src="docs/imgs/android_4.png" alt="Trang cài đặt kết nối của Deskhub trên Android: cổng UDP mà bản quét tìm trên đó, cùng các ô tick đồng bộ clipboard và giữ máy thức" width="195">
</p>
<p align="center"><sub><b>Android</b> — vẫn bốn trang đó, trong lớp áo Material. Ở vai host, Android 10+ chỉ chia sẻ màn hình để xem.</sub></p>

## 📖 Giới thiệu

Một **lõi C++20** duy nhất chạy trên mọi nền tảng — từ Windows tới iPhone — không phải
viết lại giao thức lần nào. Chia sẻ một màn hình, gõ IP ở máy kia, và bạn đang điều khiển
nó. Bốn trang trên mọi nền tảng — **Host**, **Client**, **Devices**, **Settings** — nên
học trên máy Mac là biết luôn app Android.

| ⚡ Nhanh | 📦 Một file | 🎛️ Đơn giản |
| ------ | ---------- | --------- |
| **~3.5 ms** từ lúc thu hình tới lúc hiện hình, 60 fps. Đường dữ liệu đi thẳng trong VRAM — không đụng tới CPU. | Không cài đặt, không dịch vụ chạy nền, không tài khoản. Toàn bộ app Windows là một file exe **~5.1 MB**; macOS là file dmg **1.9 MB**. | **Share** một màn hình hoặc **Connect** tới một IP, hết. Máy desktop còn chia sẻ được cả một **shell** và nhận **tệp** người xem gửi tới. Điện thoại cũng chia sẻ được màn hình, nhưng chỉ để xem, vì không hệ điều hành di động nào cho app bơm thao tác điều khiển. |

Phiên làm việc được mã hoá đầu-cuối trên **QUIC/TLS**, và một máy lạ chỉ vào được khi
chứng minh nó biết mật mã của host — bằng **SPAKE2**, nên bản thân mã đó không bao giờ
truyền đi — hoặc được người ngồi tại host bấm đồng ý. Dù vậy đó vẫn là một bí mật nhỏ trên
một cổng đang mở: hãy dùng mạng bạn tin tưởng hoặc một VPN, và **đừng bao giờ mở
port-forward cho UDP 47777**. Mô hình mối đe doạ đầy đủ nằm ở [`SECURITY.vi.md`](SECURITY.vi.md).

## 💡 Vì sao

- 💻 **Công việc** — chạy Claude Code, VS Code hay build trên PC ở nhà, từ một laptop yếu hoặc một chiếc iPad ngoài quán cà phê.
- 🌐 **Mọi thứ** — điều khiển Chrome, Office hay phần mềm chỉ có trên PC, từ bất kỳ thiết bị nào.
- 🎮 **Game** — 60 fps, chuột tương đối + scancode DirectInput, khoá con trỏ bằng `F9`.
- 🖥️ **Nhiều màn hình** — chia sẻ một hoặc nhiều màn hình, mỗi cái là một phiên riêng.

## 🚦 Nền tảng

| Nền tảng | Host | Client | Trạng thái |
| -------- | :--: | :----: | ---------- |
| **Windows** | ✅ | ✅ | Bản tham chiếu — dùng hằng ngày qua LAN + Tailscale (Internet/NAT) |
| **macOS** | ✅ | ✅ | Cả hai vai đều chạy (ScreenCaptureKit + VideoToolbox + CGEvent) |
| **Android** | ✅ | ✅ | Client: hình ảnh + điều khiển (bàn di chuột, bàn phím). Host: chia sẻ màn hình chỉ để xem (MediaProjection + MediaCodec), Android 10+ — đang thử nghiệm trên Google Play |
| **iOS** | ✅ | ✅ | Client: hình ảnh + điều khiển (bàn di chuột, bàn phím). Host: chia sẻ màn hình chỉ để xem qua Broadcast Upload Extension (ReplayKit + VideoToolbox) — đang thử nghiệm qua TestFlight |
| **Linux** | ✅ | ✅ | Cả hai vai đều chạy (PipeWire + VA-API + uinput + GTK3) — Ubuntu, Debian, Mint, Fedora, openSUSE, Arch qua deb / rpm / bản chạy thẳng; đã kiểm chứng giữa hai máy trong LAN |

## ✨ Bên trong có gì

- **Zero-copy từ đầu tới cuối** — thu hình thẳng vào VRAM → NVENC → giải mã bằng phần cứng → vẽ ra màn hình; đường dữ liệu nóng không đụng tới CPU.
- **Giao thức viết riêng, chạy trên QUIC** — GOP vô hạn + IDR theo yêu cầu, FEC kiểu XOR, bitrate tự điều chỉnh, tất cả ghép chung trên một kết nối đã mã hoá.
- **Có hình thì có tiếng** — bản trộn âm thanh của chính máy đó, Opus 64 kbps, mỗi datagram một khung 20 ms; mất một gói chỉ mất một phần nhỏ của giây và không bao giờ làm hỏng hình. Không bao giờ là micro.
- **Điều khiển thật** — chuột tương đối (Raw Input) + scancode cho game DirectInput; chuột và bàn phím của chính máy host luôn được ưu tiên.
- **Một lõi dùng chung** — giao thức, FEC và điều khiển bitrate nằm trong `core/`, được biên dịch vào mọi client.
- **Có cả dòng lệnh** — `deskhub-cli` chia sẻ màn hình, mở shell từ xa và điều khiển host từ script hay qua SSH, không cần toolkit đồ hoạ nào. Xem [Build](docs/BUILD.vi.md#client-dòng-lệnh).
- **Bị hành cho ra bã** — lõi được kiểm thử offline, chạy dưới ASan, UBSan và TSan trong CI, và bảy mục tiêu libFuzzer nện vào định dạng gói tin, bộ phân tích H.264, khâu ghép lại gói, luồng byte của terminal, chuỗi giao diện và các máy trạng thái phiên mỗi đêm; mỗi lần tìm ra một cú crash là một bài kiểm thử hồi quy mới.

## 📚 Tài liệu

Mọi tài liệu đều được xuất bản bằng tiếng Anh kèm các bản dịch đặt bên cạnh — tiếng Việt
`*.vi.md`, tiếng Trung `*.zh.md`, tiếng Nhật `*.ja.md`. Bản tiếng Anh là bản chuẩn.

| Tài liệu | Nội dung |
| --- | --- |
| [Cài đặt](docs/INSTALL.vi.md) ([en](docs/INSTALL.md) · [zh](docs/INSTALL.zh.md) · [ja](docs/INSTALL.ja.md)) | Đưa Deskhub lên từng nền tảng trong năm nền tảng |
| [Build](docs/BUILD.vi.md) ([en](docs/BUILD.md) · [zh](docs/BUILD.zh.md) · [ja](docs/BUILD.ja.md)) | Biên dịch từ mã nguồn, kiểm thử, đóng gói, phát hành |
| [Đặc tả](docs/SPECIFICATION.vi.md) ([en](docs/SPECIFICATION.md) · [zh](docs/SPECIFICATION.zh.md) · [ja](docs/SPECIFICATION.ja.md)) | Deskhub làm được gì, không có chi tiết cài đặt |
| [Kiến trúc](docs/ARCHITECTURE.vi.md) ([en](docs/ARCHITECTURE.md) · [zh](docs/ARCHITECTURE.zh.md) · [ja](docs/ARCHITECTURE.ja.md)) | Các tầng, luồng, giao thức trên đường truyền, các quyết định thiết kế |
| [`SECURITY.vi.md`](SECURITY.vi.md) ([en](SECURITY.md) · [zh](SECURITY.zh.md) · [ja](SECURITY.ja.md)) | Mô hình mối đe doạ và cách báo lỗ hổng |
| [`PRIVACY.vi.md`](PRIVACY.vi.md) ([en](PRIVACY.md) · [zh](PRIVACY.zh.md) · [ja](PRIVACY.ja.md)) | Chính sách quyền riêng tư |
| [`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md) ([en](THIRD_PARTY_NOTICES.md) · [zh](THIRD_PARTY_NOTICES.zh.md) · [ja](THIRD_PARTY_NOTICES.ja.md)) | Thành phần bên thứ ba và giấy phép |

Báo lỗi và góp ý: [issues](https://github.com/manhpham90vn/Deskhub/issues) — nhớ ghi kèm
model thiết bị của bạn.

## 📄 Giấy phép

MIT — xem [`LICENSE`](LICENSE). Các thành phần bên thứ ba và thông báo giấy phép của
chúng (bao gồm bản FFmpeg LGPL được liên kết tĩnh trong app Linux) được liệt kê ở
[`THIRD_PARTY_NOTICES.vi.md`](THIRD_PARTY_NOTICES.vi.md).
