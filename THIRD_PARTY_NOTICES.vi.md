[English](THIRD_PARTY_NOTICES.md) · **Tiếng Việt** · [中文](THIRD_PARTY_NOTICES.zh.md) · [日本語](THIRD_PARTY_NOTICES.ja.md)

# Thông báo về thành phần bên thứ ba

Bản thân Deskhub được phát hành theo Giấy phép MIT — xem [`LICENSE`](LICENSE).

Tệp này liệt kê các thành phần bên thứ ba mà Deskhub liên kết tới, cùng những nghĩa vụ đi
kèm chúng. Không thành phần nào ở đây được cấp phép theo GPL, và không thành phần nào hạn
chế việc phân phối lại Deskhub theo Giấy phép MIT.

> Đây là bản dịch của [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Nếu hai bản có
> khác biệt, **bản tiếng Anh là bản chuẩn**.

## App Linux (`client/linux`)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 — `libavcodec`, `libavutil` | LGPL-2.1-or-later | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | động |
| [PipeWire](https://pipewire.org) 0.3 | MIT | động |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | động |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | động |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | động |
| EGL ([libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa) | MIT | động |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | động, tùy chọn (biểu tượng khay) |

### FFmpeg (LGPL-2.1-or-later, liên kết tĩnh)

App Linux là mục tiêu build duy nhất có đóng gói kèm FFmpeg. Nó được dựng từ mã nguồn
FFmpeg 8.0 nguyên bản của thượng nguồn bằng
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh), cấu hình **không** có
`--enable-gpl` và **không** có `--enable-nonfree`, chỉ bật bộ giải mã H.264 nội tại và
hwaccel VA-API. Do đó `libavcodec` và `libavutil` thu được nằm dưới GNU Lesser General
Public License, phiên bản 2.1 hoặc mới hơn — toàn văn trong
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

Vì các thư viện này được liên kết tĩnh, LGPL-2.1 §6 yêu cầu người nhận phải có khả năng
liên kết lại ứng dụng với một bản FFmpeg đã chỉnh sửa. Deskhub đáp ứng yêu cầu này bằng
cách cung cấp mã nguồn đầy đủ: mã nguồn ứng dụng nằm trong kho này dưới Giấy phép MIT, và
`scripts/build-ffmpeg.sh` tái tạo chính xác bản dựng FFmpeg đó. Bất kỳ ai cũng có thể
thay bằng FFmpeg của riêng mình và dựng lại bằng `make`.

FFmpeg không bị chỉnh sửa dưới bất kỳ hình thức nào. Mã nguồn thượng nguồn:
<https://ffmpeg.org/download.html>.

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, do dự án FFmpeg phân phối lại dưới Giấy phép MIT. Bản thân phần hiện thực
NVENC nằm trong driver NVIDIA của người dùng (`libnvidia-encode.so.1`, `libcuda.so.1`)
và được nạp lúc chạy; nó không được đóng gói kèm.

### GTK 3 (LGPL-2.1-or-later, liên kết động)

Liên kết động tới các thư viện chia sẻ do bản phân phối cung cấp. Không có mã nguồn GTK
nào được đóng gói kèm hay chỉnh sửa; người dùng có thể thay thế thư viện hệ thống tuỳ ý.

## Client dòng lệnh (`client/cli`)

Client dòng lệnh liên kết đúng những thư viện mà app để bàn của hệ điều hành tương ứng
dùng, trừ toolkit đồ hoạ — không GTK trên Linux, không wxWidgets trên Windows. Nó vẽ màn
hình từ xa trong cửa sổ của riêng nó, nên trên Linux có thêm hai thư viện mà app không cần:

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | động (Linux) |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | động (Linux) |

Trên Windows nó dùng lại chính cửa sổ Win32 của app nên không thêm gì. Trên macOS nó dùng
AppKit và ScreenCaptureKit trong SDK của Apple, giống như app.

## App Windows (`client/windows`)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| Media Foundation, Direct3D 11, DXGI | Microsoft Windows SDK | thành phần hệ điều hành |

### wxWidgets (wxWindows Library Licence, liên kết tĩnh)

App Windows dựng wxWidgets nguyên bản của thượng nguồn từ gói phát hành chính thức (được
CMake tải về lúc configure, xem `client/windows/win32/CMakeLists.txt`) và liên kết tĩnh
vào `Deskhub.exe`. wxWindows Library Licence là LGPL cộng thêm một ngoại lệ cho phép rõ
ràng việc phân phối các bản nhị phân liên kết với thư viện — tĩnh hay động — theo điều
khoản của chính bên phân phối, nên bản phân phối MIT một-file của Deskhub không bị ảnh
hưởng. Toàn văn giấy phép: <https://www.wxwidgets.org/about/licence/>.

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, được dự án FFmpeg phân phối lại dưới Giấy phép MIT. Bản thân phần hiện thực
NVENC nằm trong driver NVIDIA của người dùng (`nvEncodeAPI64.dll`) và được nạp lúc chạy;
nó không được đóng gói kèm.

## App Apple (`client/macos`, `client/ios`)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| SwiftUI, AppKit, ScreenCaptureKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, CoreGraphics, ApplicationServices, ServiceManagement | Apple SDK | thành phần hệ điều hành (macOS) |
| SwiftUI, UIKit, ReplayKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, Accelerate (vImage), Photos, UserNotifications | Apple SDK | thành phần hệ điều hành (iOS) |

## App Android (`client/android`)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) — Core KTX, Activity, Compose UI, Material 3 | Apache-2.0 | động |
| Thư viện chuẩn [Kotlin](https://kotlinlang.org) | Apache-2.0 | động |
| MediaCodec, các API media của NDK | Android SDK / NDK | thành phần hệ điều hành |

## Tầng truyền tải QUIC (mọi app)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **tĩnh** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/) (đi kèm quiche) | OpenSSL / kiểu ISC | **tĩnh** |

Tầng truyền tải mã hoá của Deskhub nhúng quiche của Cloudflare, dựng từ mã nguồn
upstream không chỉnh sửa, ghim đúng phiên bản và commit bởi
[`scripts/build-quiche.sh`](scripts/build-quiche.sh). quiche mang theo bản BoringSSL của
riêng nó (qua crate `boring`), cung cấp TLS và phần mật mã đứng sau cơ chế ghép đôi của
Deskhub. Cả hai được liên kết tĩnh vào mọi app. Không thư viện nào bị chỉnh sửa.

## Codec âm thanh (mọi app)

| Thành phần | Giấy phép | Cách liên kết |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **tĩnh** |

Phần truyền âm thanh của Deskhub nhúng libopus, dựng từ mã nguồn upstream không chỉnh
sửa, ghim đúng phiên bản và mã kiểm tra bởi
[`scripts/build-opus.sh`](scripts/build-opus.sh), liên kết tĩnh vào mọi app. Một bản dựng
phục vụ cả năm nền tảng: cùng một bộ mã hoá chạy trên máy chia sẻ và cùng một bộ giải mã
chạy trên mọi máy xem. Thư viện không bị chỉnh sửa. Văn bản giấy phép:
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt).

## Bằng sáng chế

H.264/AVC được bảo hộ bởi các bằng sáng chế cấp phép qua
[Via LA](https://www.via-la.com/). Quyền sáng chế tách biệt với các giấy phép bản quyền ở
trên và không được cấp bởi Giấy phép MIT. Trên Windows, macOS, iOS và Android, việc mã
hoá và giải mã do chính codec của hệ điều hành thực hiện. Trên Linux, chúng do driver
VA-API của hãng GPU thực hiện. Deskhub không đóng gói kèm bất kỳ bản hiện thực codec
video nào của riêng mình.

Opus, codec âm thanh, là ngoại lệ: libopus được đóng gói kèm. Opus được công bố là codec
miễn phí bản quyền, và các giấy phép sáng chế bao phủ nó đều được cấp theo điều khoản
miễn phí — danh sách khai báo nằm trong
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) và tại
<https://opus-codec.org/license/>.
