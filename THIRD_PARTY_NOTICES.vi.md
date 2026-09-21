[English](THIRD_PARTY_NOTICES.md) · **Tiếng Việt** · [中文](THIRD_PARTY_NOTICES.zh.md) · [日本語](THIRD_PARTY_NOTICES.ja.md)

# Thông báo về thành phần bên thứ ba

Bản thân Deskhub được phát hành theo MIT License — xem [`LICENSE`](LICENSE).

File này liệt kê các thành phần bên thứ ba mà Deskhub link tới, cùng những nghĩa vụ đi
kèm chúng. Không thứ nào ở đây dùng license GPL, và không thành phần nào hạn chế việc
phân phối lại Deskhub theo MIT License.

Đây là bản dịch của [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Nếu hai bản có
khác biệt, bản tiếng Anh là bản chuẩn.

## App Linux (`client/linux`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 — `libavcodec`, `libavutil` | LGPL-2.1-or-later | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | động |
| [PipeWire](https://pipewire.org) 0.3 | MIT | động |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | động |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | động |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | động |
| EGL ([libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa) | MIT | động |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | động, tuỳ chọn (icon tray) |

### FFmpeg (LGPL-2.1-or-later, link tĩnh)

App Linux là target duy nhất có đóng gói FFmpeg. Nó được build từ source FFmpeg 8.0 gốc,
không sửa đổi, bởi [`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh), cấu hình
**không** `--enable-gpl` và **không** `--enable-nonfree`, chỉ bật H.264 decoder gốc cùng
hwaccel VA-API. Vì thế `libavcodec` và `libavutil` tạo ra được bảo hộ bởi GNU Lesser
General Public License phiên bản 2.1 trở lên — toàn văn ở
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

Vì các thư viện này được link tĩnh, LGPL-2.1 §6 yêu cầu người nhận phải relink được ứng
dụng với một bản FFmpeg đã sửa đổi. Deskhub đáp ứng điều đó bằng cách cung cấp source đầy
đủ: phần source ứng dụng nằm trong repo này theo MIT License, còn
`scripts/build-ffmpeg.sh` tái tạo lại chính xác bản build FFmpeg. Người dùng có thể thay
FFmpeg của riêng mình và build lại bằng `make`.

FFmpeg không bị sửa đổi. Source gốc: <https://ffmpeg.org/download.html>.

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, đúng như dự án FFmpeg phân phối lại theo MIT License. Bản cài đặt NVENC thực sự
nằm trong driver NVIDIA của người dùng (`libnvidia-encode.so.1`, `libcuda.so.1`) và
được phân giải lúc chạy; nó không được đóng gói kèm.

### GTK 3 (LGPL-2.1-or-later, link động)

Link động tới các thư viện chia sẻ do distro cung cấp. Không source GTK nào được đóng gói
hay sửa đổi; người dùng có thể thay thế các thư viện hệ thống.

## Command line client (`client/cli`)

Command line client link đúng những thư viện mà app desktop của hệ thống nó được build
cho, trừ phần GUI toolkit — không GTK trên Linux, không wxWidgets trên Windows. Nó vẽ một
màn hình từ xa trong cửa sổ riêng của mình, và trên Linux điều đó thêm hai thư viện mà app
không cần:

| Thành phần | License | Cách link |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | động (Linux) |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | động (Linux) |

Trên Windows nó dùng lại chính cửa sổ viewer Win32 của app, nên không thêm gì. Trên macOS
nó dùng AppKit và ScreenCaptureKit từ Apple SDK, giống như app.

## App Windows (`client/windows`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| Media Foundation, Direct3D 11, DXGI | Microsoft Windows SDK | thành phần OS |

### wxWidgets (wxWindows Library Licence, link tĩnh)

App Windows build wxWidgets gốc không sửa đổi từ archive phát hành chính thức (CMake tự
tải về lúc configure, xem `client/windows/win32/CMakeLists.txt`) và link tĩnh nó vào
`Deskhub.exe`. wxWindows Library Licence chính là LGPL cộng thêm một ngoại lệ cho phép
phân phối binary có link tới thư viện — tĩnh hay động đều được — theo điều khoản của
chính người phân phối, nên việc Deskhub phát hành dưới dạng một file duy nhất theo MIT
không bị ảnh hưởng. Toàn văn license: <https://www.wxwidgets.org/about/licence/>.

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, đúng như dự án FFmpeg phân phối lại theo MIT License. Bản cài đặt NVENC thực sự
nằm trong driver NVIDIA của người dùng (`nvEncodeAPI64.dll`) và được phân giải lúc chạy;
nó không được đóng gói kèm.

## App Apple (`client/macos`, `client/ios`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| SwiftUI, AppKit, ScreenCaptureKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, CoreGraphics, ApplicationServices, ServiceManagement | Apple SDK | thành phần OS (macOS) |
| SwiftUI, UIKit, ReplayKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, Accelerate (vImage), Photos, UserNotifications | Apple SDK | thành phần OS (iOS) |

## App Android (`client/android`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) — Core KTX, Activity, Compose UI, Material 3 | Apache-2.0 | động |
| Thư viện chuẩn [Kotlin](https://kotlinlang.org) | Apache-2.0 | động |
| MediaCodec, các media API của NDK | Android SDK / NDK | thành phần OS |

## QUIC transport (mọi app)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **tĩnh** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/) (đi kèm quiche) | OpenSSL / kiểu ISC | **tĩnh** |

Transport đã encrypt của Deskhub nhúng quiche của Cloudflare, build từ source gốc không
sửa đổi, ghim vào đúng một phiên bản và một commit bởi
[`scripts/build-quiche.sh`](scripts/build-quiche.sh). quiche mang theo bản BoringSSL của
riêng nó (qua crate `boring`), thứ cung cấp TLS và phần mật mã đứng sau pairing của
Deskhub. Cả hai đều được link tĩnh vào mọi app. Không thư viện nào bị sửa đổi.

## Audio codec (mọi app)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **tĩnh** |

Phần stream audio của Deskhub nhúng libopus, build từ source gốc không sửa đổi, ghim vào
đúng một phiên bản và một checksum bởi [`scripts/build-opus.sh`](scripts/build-opus.sh)
và link tĩnh vào mọi app. Một bản build phục vụ cả năm nền tảng: cùng một encoder chạy
trên máy đang share và cùng một decoder chạy trên mọi viewer. Thư viện không bị sửa đổi.
Toàn văn license:
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt).

## Bằng sáng chế

H.264/AVC được bảo hộ bởi các bằng sáng chế cấp phép qua
[Via LA](https://www.via-la.com/). Quyền sáng chế tách biệt với các license bản quyền ở
trên và không được MIT License cấp. Trên Windows, macOS, iOS và Android, phần encode và
decode do chính codec của hệ điều hành thực hiện. Trên Linux thì do VA-API driver của
hãng GPU thực hiện. Deskhub không phát hành bản cài đặt video codec nào của riêng nó.

Opus, audio codec được sử dụng, là ngoại lệ: libopus được đóng gói kèm. Opus được công
bố như một codec miễn phí bản quyền, và các license sáng chế phủ nó đều được cấp theo điều khoản
miễn phí bản quyền — các tuyên bố được liệt kê trong
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) và tại
<https://opus-codec.org/license/>.
