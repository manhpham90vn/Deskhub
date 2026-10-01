[English](THIRD_PARTY_NOTICES.md) · **Tiếng Việt** · [中文](THIRD_PARTY_NOTICES.zh.md) · [日本語](THIRD_PARTY_NOTICES.ja.md)

# Thông báo về thành phần bên thứ ba

Bản thân Deskhub được phát hành theo MIT License — xem [`LICENSE`](LICENSE).

Tài liệu này liệt kê các thành phần bên thứ ba Deskhub sử dụng, cách chúng được link và
những nghĩa vụ license đi kèm. Mã nguồn của Deskhub vẫn theo MIT License; mỗi thành phần
trong danh sách giữ license riêng của nó.

Đây là bản dịch của [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Nếu hai bản có
khác biệt, bản tiếng Anh là bản chuẩn.

Mọi gói mang theo file này cùng các văn bản license trong [`licenses/`](licenses/) áp
dụng cho gói đó:

| File | Áp dụng cho | Có trong |
| --- | --- | --- |
| [`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt) | quiche và `octets` | mọi gói |
| [`licenses/BoringSSL.txt`](licenses/BoringSSL.txt) | BoringSSL | mọi gói |
| [`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt) | văn bản Apache License 2.0 mà các crate Rust và các thư viện Android tham chiếu | mọi gói |
| [`licenses/rust-crates.txt`](licenses/rust-crates.txt) | từng crate Rust được biên dịch vào quiche, kèm thông báo riêng của nó — sinh bởi [`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) | mọi gói |
| [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) | libopus | mọi gói |
| [`licenses/android-libraries.txt`](licenses/android-libraries.txt) | từng thư viện Java và Kotlin được đóng gói vào APK Android, kèm licence, dự án, tác giả và mọi NOTICE nó mang theo — sinh bởi [`scripts/android-library-notices.py`](scripts/android-library-notices.py) | APK Android |
| [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt) | FFmpeg; đồng thời là nền của wxWindows Library Licence | gói app và CLI Linux, bộ cài app Windows |
| [`licenses/wxWindows.txt`](licenses/wxWindows.txt) | wxWidgets | bộ cài app Windows |
| [`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt) | các thư viện mà wxWidgets đóng kèm — zlib, libpng, IJG libjpeg, LibTIFF, PCRE2, libwebp, NanoSVG | bộ cài app Windows |

Vị trí của chúng: gói `.deb` và `.rpm` của app và của command line client đặt file này ở
`/usr/share/doc/<package>/` và các văn bản ở `/usr/share/doc/<package>/licenses/` (ví dụ
văn bản LGPL nằm ở `/usr/share/doc/deskhub/licenses/LGPL-2.1.txt`); cả hai bộ cài Windows
đặt chúng cạnh file thực thi, trong thư mục cài đặt và thư mục con `licenses`; app macOS
mang chúng trong `Contents/Resources` của app bundle, app iOS trong bundle của nó, và APK
Android trong assets. Các binary command line portable không thể mang gì kèm theo, nên mỗi
bản release còn có `deskhub-<tag>-licenses.zip` chứa `LICENSE`, file này và toàn bộ thư mục
`licenses/`.

## App Linux (`client/linux`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 — `libavcodec`, `libavutil` | LGPL-2.1-or-later | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | động |
| [GLib / GIO](https://gitlab.gnome.org/GNOME/glib) (`gio-2.0`, `gio-unix-2.0`; portal screen cast) | LGPL-2.1-or-later | động |
| [PipeWire](https://pipewire.org) 0.3 | MIT | động |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | động |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | động |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | động |
| EGL ([libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa) | MIT | động |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-2.1 hoặc LGPL-3.0; vài file được compile vào nó theo GPL-3.0 | động, tuỳ chọn (icon tray) |

Command line client trên Linux link cùng bộ media này: FFmpeg link tĩnh, còn GLib/GIO,
PipeWire, libva, libdrm, libepoxy và EGL link động — xem phần command line client bên
dưới.

### FFmpeg (LGPL-2.1-or-later, link tĩnh)

App Linux và command line client trên Linux là hai target duy nhất có đóng gói FFmpeg.
Nó được build từ source FFmpeg 8.0 gốc, không sửa đổi, ghim bằng checksum, bởi
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh), cấu hình **không**
`--enable-gpl` và **không** `--enable-nonfree`, chỉ bật H.264 decoder gốc của FFmpeg cùng
hwaccel VA-API. Vì thế `libavcodec` và `libavutil` tạo ra được bảo hộ bởi GNU Lesser
General Public License phiên bản 2.1 trở lên — toàn văn ở
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

Vì các thư viện này được link tĩnh, LGPL-2.1 §6 yêu cầu người nhận phải relink được ứng
dụng với một bản FFmpeg đã sửa đổi. Deskhub đáp ứng điều đó bằng cách cung cấp source đầy
đủ: phần source ứng dụng nằm trong repo này theo MIT License, còn
`scripts/build-ffmpeg.sh` tái tạo lại chính xác bản build FFmpeg vào
`third_party/ffmpeg-min`. Bất kỳ ai cũng có thể đặt bản build FFmpeg của riêng mình vào
đó (hoặc sửa script) rồi build lại bằng `make build-linux` / `make release-linux` cho
app, hoặc `make build-cli` / `make release-cli` cho command line client.

FFmpeg không bị sửa đổi. Source gốc: <https://ffmpeg.org/download.html>.

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, đúng như dự án FFmpeg phân phối lại theo MIT License. Bản cài đặt NVENC thực sự
nằm trong driver NVIDIA của người dùng (`libnvidia-encode.so.1`, `libcuda.so.1`) và
được phân giải lúc chạy; nó không được đóng gói kèm.

### GTK 3 và GLib (LGPL-2.1-or-later, link động)

Link động tới các thư viện chia sẻ do distro cung cấp. Không source GTK hay GLib nào được
đóng gói hay sửa đổi; người dùng có thể thay thế các thư viện hệ thống.

### libayatana-appindicator (link động, tuỳ chọn)

Chỉ dùng cho icon tray, và chỉ khi thư viện có sẵn lúc build. Upstream khai báo bản thân
thư viện theo LGPL-2.1 hoặc LGPL-3.0, nhưng vài file source được compile vào thư viện
chia sẻ (ví dụ `src/generate-id.c`) mang header GPL-3.0. Deskhub link động tới thư viện
chia sẻ của distro, không đóng gói kèm và không sửa đổi nó.

## Command line client (`client/cli`)

Command line client không dùng GUI toolkit của app desktop: không phụ thuộc GTK trên
Linux hay wxWidgets trên Windows. Giống mọi app, nó nhúng QUIC transport và audio codec
liệt kê bên dưới.

Trên Linux, nó còn link cùng bộ media với app Linux — FFmpeg link **tĩnh** theo đúng các
điều khoản LGPL-2.1 đã nêu ở phần app Linux phía trên, cùng GLib/GIO, PipeWire, libva,
libdrm, libepoxy và EGL link động. Lệnh `connect` mở cửa sổ xem màn hình từ xa, và cửa
sổ đó cần thêm hai thư viện mà app desktop không dùng:

| Thành phần | License | Cách link |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | động (Linux) |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | động (Linux) |

Trên Windows, CLI dùng lại cửa sổ viewer Win32 của app nên không cần thêm thư viện. Trên
macOS, CLI dùng AppKit và ScreenCaptureKit từ Apple SDK nhưng không có viewer màn hình từ xa.

## App Windows (`client/windows`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3.1 | wxWindows Library Licence 3.1 | **tĩnh** |
| Các thư viện đi kèm wxWidgets — xem bên dưới | nhiều license dễ dãi | **tĩnh** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (header NVENC SDK 13.0) | MIT | chỉ header |
| Header projection [C++/WinRT](https://github.com/microsoft/cppwinrt) | MIT | chỉ header |
| Media Foundation, Direct3D 11, DXGI, Windows.Graphics.Capture, WASAPI | Microsoft Windows SDK | thành phần OS |

### wxWidgets (wxWindows Library Licence, link tĩnh)

App Windows build wxWidgets 3.3.1 gốc không sửa đổi từ archive phát hành chính thức
(CMake tự tải về lúc configure và kiểm tra với một hash đã ghim, xem
`client/windows/win32/CMakeLists.txt`) và link tĩnh nó vào `Deskhub.exe`. wxWindows
Library Licence chính là LGPL cộng thêm một ngoại lệ cho phép phân phối binary có link
tới thư viện — tĩnh hay động đều được — theo điều khoản của chính người phân phối, nên
việc Deskhub phát hành dưới dạng một file duy nhất theo MIT không bị ảnh hưởng. Toàn văn
license: [`licenses/wxWindows.txt`](licenses/wxWindows.txt) và
<https://www.wxwidgets.org/about/licence/>.

Deskhub link `wx::base` và `wx::core`. Trên Windows, wxWidgets tự build bản riêng của các
thư viện sau và link chúng vào hai thư viện đó, nên chúng có thể nằm trong
`Deskhub.exe`. Không thư viện nào bị sửa đổi.

| Thành phần | License |
| --- | --- |
| [zlib](https://zlib.net) | Zlib |
| [PCRE2](https://github.com/PCRE2Project/pcre2) (`wxRegEx`) | BSD-3-Clause WITH PCRE2-exception |
| [libpng](http://www.libpng.org/pub/png/libpng.html) | libpng-2.0 |
| [IJG libjpeg](https://www.ijg.org) 9f | IJG |
| [LibTIFF](https://libtiff.gitlab.io/libtiff/) | libtiff |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD-3-Clause |
| [NanoSVG](https://github.com/memononen/nanosvg) | Zlib |

License IJG yêu cầu lời ghi nhận sau: *this software is based in part on the work of
the Independent JPEG Group.* Thông báo của cả bảy thư viện nằm trong
[`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt).

`third_party/nvenc-13.0` là một git submodule chỉ chứa các header API của NVIDIA Video
Codec SDK, đúng như dự án FFmpeg phân phối lại theo MIT License. Bản cài đặt NVENC thực sự
nằm trong driver NVIDIA của người dùng (`nvEncodeAPI64.dll`) và được phân giải lúc chạy;
nó không được đóng gói kèm.

## App Apple (`client/macos`, `client/ios`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| SwiftUI, AppKit, ScreenCaptureKit, VideoToolbox, AVFoundation, AudioToolbox, CoreMedia, CoreVideo, CoreGraphics, ApplicationServices, ServiceManagement, IOKit, UserNotifications | Apple SDK | thành phần OS (macOS) |
| SwiftUI, UIKit, ReplayKit, VideoToolbox, AVFoundation, AudioToolbox, CoreMedia, CoreVideo, Accelerate (vImage), Photos, PhotosUI, UniformTypeIdentifiers, UserNotifications | Apple SDK | thành phần OS (iOS) |

## App Android (`client/android`)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) — Core KTX, Activity, Compose UI, Material 3 | Apache-2.0 | compile vào APK |
| Thư viện chuẩn [Kotlin](https://kotlinlang.org) | Apache-2.0 | compile vào APK |
| [CameraX](https://developer.android.com/media/camera/camerax) 1.4 — `camera-camera2`, `camera-lifecycle`, `camera-view` (quét QR) | Apache-2.0 | compile vào APK |
| [ZXing](https://github.com/zxing/zxing) `core` 3.5 (giải mã QR) | Apache-2.0 | compile vào APK |
| [kotlinx.coroutines](https://github.com/Kotlin/kotlinx.coroutines), [kotlinx.serialization](https://github.com/Kotlin/kotlinx.serialization) core, [Guava `listenablefuture`](https://github.com/google/guava), [AutoValue](https://github.com/google/auto) annotations, [JetBrains annotations](https://github.com/JetBrains/java-annotations), [JSpecify](https://jspecify.dev) — được các thư viện trên kéo theo | Apache-2.0 | compile vào APK |
| [libyuv](https://chromium.googlesource.com/libyuv/libyuv/), nằm trong thư viện native của `camera-core` (CameraX) | BSD-3-Clause | **tĩnh** (thư viện native của `camera-core`) |
| [LLVM libc++](https://libcxx.llvm.org) (`ANDROID_STL=c++_static`) | Apache-2.0 WITH LLVM-exception | **tĩnh** (thư viện native) |
| MediaCodec, AAudio, các media API của NDK | Android SDK / NDK | thành phần OS |

Danh sách đầy đủ các thư viện Java và Kotlin trong APK — mọi phụ thuộc bắc cầu của bản build
release, kèm phiên bản, licence, dự án, tác giả, mọi file NOTICE nó mang theo và văn bản licence
của libyuv — là [`licenses/android-libraries.txt`](licenses/android-libraries.txt), đọc cùng
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt). File này được sinh bởi
[`scripts/android-library-notices.py`](scripts/android-library-notices.py) từ `releaseRuntimeClasspath`
của Gradle, nên hãy chạy lại script mỗi khi một phụ thuộc Android thay đổi.

## QUIC transport (mọi app)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **tĩnh** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/) (đi kèm quiche qua `boring-sys` 4.22.0) | OpenSSL License AND SSLeay AND ISC AND MIT | **tĩnh** |
| Các crate Rust được compile vào quiche — xem bên dưới | xem bên dưới | **tĩnh** |
| [Thư viện chuẩn Rust](https://github.com/rust-lang/rust) | MIT OR Apache-2.0 | **tĩnh** |

Transport đã mã hóa của Deskhub nhúng quiche của Cloudflare, ghim vào một phiên bản
và commit bởi [`scripts/build-quiche.sh`](scripts/build-quiche.sh). Bản build áp dụng
[`patches/quiche-key-exporter.patch`](patches/quiche-key-exporter.patch) để xuất dữ liệu
khóa TLS và đọc private key OpenSSH qua RustCrypto `ssh-key`. quiche mang theo
BoringSSL (qua crate `boring`). Các thư viện này được link tĩnh vào mọi app và vào
command line client. Toàn văn license:
[`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt),
[`licenses/BoringSSL.txt`](licenses/BoringSSL.txt), và cho các crate Rust là
[`licenses/rust-crates.txt`](licenses/rust-crates.txt) cùng
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt).

BoringSSL là một nhánh của OpenSSL, và license OpenSSL cùng SSLeay của nó yêu cầu các
lời ghi nhận sau (giữ nguyên tiếng Anh):

- This product includes software developed by the OpenSSL Project for use in the OpenSSL
  Toolkit (<http://www.openssl.org/>).
- This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).
  This product includes software written by Tim Hudson (tjh@cryptsoft.com).

### Các crate Rust

Các crate dưới đây là kết quả `cargo tree -e normal -p quiche --features ffi` phân giải từ
`Cargo.lock` của quiche sau khi áp patch, cho mọi target Deskhub build. Các procedural
macro (`enum_dispatch`, `openssl-macros`, `foreign-types-macros`,
`curve25519-dalek-derive`, cùng `syn`, `quote`, `proc-macro2`, `unicode-ident`) chỉ chạy
lúc compile và không được link.

| License | Crate |
| --- | --- |
| BSD-2-Clause | `quiche` 0.29.3, `octets` 0.3.6 |
| BSD-3-Clause | `curve25519-dalek` 4.1.3, `ed25519-dalek` 2.2.0, `subtle` 2.6.1 |
| Apache-2.0 | `boring` 4.22.0 |
| MIT | `boring-sys` 4.22.0, `bytes`, `debug_panic`, `generic-array`, `libm`, `memoffset`, `slab` |
| Unlicense OR MIT | `byteorder` |
| MIT OR Apache-2.0 | `ssh-key` 0.6.7, `ssh-cipher`, `ssh-encoding`, `ed25519`, `p256`, `ecdsa`, `elliptic-curve`, `primeorder`, `sec1`, `rfc6979`, `crypto-bigint`, `ff`, `group`, `signature`, `aes`, `aes-gcm`, `aead`, `ghash`, `polyval`, `universal-hash`, `chacha20`, `poly1305`, `cipher`, `ctr`, `cbc`, `inout`, `block-padding`, `bcrypt-pbkdf`, `blowfish`, `pbkdf2`, `hmac`, `sha2`, `digest`, `block-buffer`, `crypto-common`, `cpufeatures`, `opaque-debug`, `der`, `spki`, `pem-rfc7468`, `base16ct`, `base64ct`, `const-oid`, `rand_core`, `getrandom`, `zeroize`, `typenum`, `bitflags`, `cfg-if`, `either`, `foreign-types`, `foreign-types-shared`, `intrusive-collections`, `libc`, `log`, `smallvec`; chỉ trên Windows: `windows-sys`, `windows-targets`, `windows_x86_64_msvc` |

Các crate BSD-3-Clause có bản quyền © 2016-2021 isis agora lovecruft và Henry de Valence
(`curve25519-dalek`, trong đó còn có mã phái sinh từ ed25519 viết bằng Go của Adam
Langley), © 2017-2019 isis agora lovecruft (`ed25519-dalek`), và © 2016-2024 Isis Agora
Lovecruft và Henry de Valence (`subtle`). Toàn văn thông báo của mọi crate, chép từ source
đã phát hành của nó, nằm trong [`licenses/rust-crates.txt`](licenses/rust-crates.txt), file
mà [`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) sinh lại từ cây source
quiche khi pin của quiche thay đổi.

## Audio codec (mọi app)

| Thành phần | License | Cách link |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **tĩnh** |

Phần stream audio của Deskhub nhúng libopus, build từ source gốc không sửa đổi, ghim vào
đúng một phiên bản và một checksum bởi [`scripts/build-opus.sh`](scripts/build-opus.sh)
và link tĩnh vào mọi app cùng command line client. Cùng một source đã ghim được build riêng
cho từng target, nên cả sáu chương trình — năm app và command line client — chạy cùng
một encoder và decoder. Thư viện không bị sửa đổi. Toàn văn license:
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt).

## Bằng sáng chế

H.264/AVC được bảo hộ bởi các bằng sáng chế cấp phép qua
[Via LA](https://www.via-la.com/). Quyền sáng chế tách biệt với các license bản quyền ở
trên và không được MIT License cấp. Trên Windows, macOS, iOS và Android, phần encode và
decode do chính codec của hệ điều hành thực hiện. Trên Linux, phần encode do VA-API
driver của hãng GPU hoặc driver NVENC của NVIDIA thực hiện. Phần decode trên Linux đi qua
H.264 decoder của FFmpeg mà Deskhub đóng gói kèm: khi có VA-API thì GPU giải mã, khi không
có thì FFmpeg giải mã bằng phần mềm. Decoder FFmpeg đó là bản cài đặt H.264 duy nhất
Deskhub phát hành.

Opus, audio codec được sử dụng, cũng được đóng gói kèm. Opus được công bố như một codec
miễn phí bản quyền, và các license sáng chế phủ nó đều được cấp theo điều khoản miễn phí
bản quyền — các tuyên bố được liệt kê trong
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) và tại
<https://opus-codec.org/license/>.
