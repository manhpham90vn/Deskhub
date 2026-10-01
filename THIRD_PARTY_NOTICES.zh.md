[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · **中文** · [日本語](THIRD_PARTY_NOTICES.ja.md)

# 第三方声明

Deskhub 本身以 MIT License 分发 —— 见 [`LICENSE`](LICENSE)。

本文件列出 Deskhub 使用的第三方组件、link 方式及相应的 license 义务。Deskhub 自身的
source 仍采用 MIT License；表中各组件保留各自的 license。

本文件是 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 的译本；若两者有出入，以英
文版为准。

每个安装包都会附带本文件以及 [`licenses/`](licenses/) 中适用于它的许可证原文：

| 文件 | 适用于 | 随附于 |
| --- | --- | --- |
| [`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt) | quiche 与 `octets` | 所有安装包 |
| [`licenses/BoringSSL.txt`](licenses/BoringSSL.txt) | BoringSSL | 所有安装包 |
| [`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt) | Rust crate 与 Android 库所引用的 Apache License 2.0 原文 | 所有安装包 |
| [`licenses/rust-crates.txt`](licenses/rust-crates.txt) | 编译进 quiche 的每个 Rust crate 及其各自的声明 —— 由 [`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) 生成 | 所有安装包 |
| [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) | libopus | 所有安装包 |
| [`licenses/android-libraries.txt`](licenses/android-libraries.txt) | 打包进 Android APK 的每个 Java 与 Kotlin 库，附其许可证、项目、作者以及随附的 NOTICE——由 [`scripts/android-library-notices.py`](scripts/android-library-notices.py) 生成 | Android APK |
| [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt) | FFmpeg；也是 wxWindows Library Licence 的基础 | Linux app 与 CLI 安装包、Windows app 安装程序 |
| [`licenses/wxWindows.txt`](licenses/wxWindows.txt) | wxWidgets | Windows app 安装程序 |
| [`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt) | wxWidgets 自带的库 —— zlib、libpng、IJG libjpeg、LibTIFF、PCRE2、libwebp、NanoSVG | Windows app 安装程序 |

存放位置：app 与 command line client 的 `.deb` 和 `.rpm` 把本文件放在
`/usr/share/doc/<package>/`，把许可证原文放在 `/usr/share/doc/<package>/licenses/`（例如
LGPL 原文位于 `/usr/share/doc/deskhub/licenses/LGPL-2.1.txt`）；两个 Windows 安装程序把它们
放在可执行文件旁边，即安装文件夹及其 `licenses` 子文件夹中；macOS app 将它们放在 app bundle
的 `Contents/Resources` 中，iOS app 放在其 bundle 中，Android APK 放在其 assets 中。便携版
command line binary 旁边无法附带任何文件，因此每个 release 还提供
`deskhub-<tag>-licenses.zip`，内含 `LICENSE`、本文件以及整个 `licenses/` 文件夹。

## Linux app（`client/linux`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 —— `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 动态 |
| [GLib / GIO](https://gitlab.gnome.org/GNOME/glib)（`gio-2.0`、`gio-unix-2.0`；屏幕共享 portal） | LGPL-2.1-or-later | 动态 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 动态 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 动态 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 动态 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 动态 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 动态 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-2.1 或 LGPL-3.0；编译进其中的部分文件为 GPL-3.0 | 动态，可选（tray 图标） |

Linux 上的 command line client link 同一套媒体组件：FFmpeg 静态 link，GLib/GIO、
PipeWire、libva、libdrm、libepoxy 和 EGL 动态 link —— 见下文 command line client 一节。

### FFmpeg（LGPL-2.1-or-later，静态 link）

Linux app 和 Linux 上的 command line client 是仅有的两个捆绑 FFmpeg 的 target。它由
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh) 从未经修改、按 checksum 钉死的上游
FFmpeg 8.0 source 构建，配置时**不加** `--enable-gpl`、**不加** `--enable-nonfree`，只
启用 FFmpeg 原生的 H.264 decoder 和 VA-API hwaccel。因此产出的 `libavcodec` 和
`libavutil` 受 GNU Lesser General Public License 2.1 或更高版本约束 —— 全文见
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt)。

由于这些库是静态 link 的，LGPL-2.1 §6 要求接收者能够用修改过的 FFmpeg 重新 link 这个
应用。Deskhub 通过提供完整 source 来满足这一点：应用的 source 就在本 repo 里、以 MIT
License 提供，而 `scripts/build-ffmpeg.sh` 能把完全一样的 FFmpeg 构建复现到
`third_party/ffmpeg-min`。任何人都可以把自己的 FFmpeg 构建放到那里（或修改脚本），然后
用 `make build-linux` / `make release-linux` 重新构建 app，或用 `make build-cli` /
`make release-cli` 重新构建 command line client。

FFmpeg 没有被以任何方式修改。上游 source：<https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` 是一个 git submodule，里面只有 NVIDIA Video Codec SDK 的 API
头文件，与 FFmpeg 项目按 MIT License 再分发的版本相同。NVENC 的实现本身在用户的 NVIDIA
driver 里（`libnvidia-encode.so.1`、`libcuda.so.1`），在运行时解析；它没有被捆绑。

### GTK 3 与 GLib（LGPL-2.1-or-later，动态 link）

动态 link 到发行版提供的共享库。没有捆绑也没有修改任何 GTK 或 GLib source；用户可以自由
替换系统库。

### libayatana-appindicator（动态 link，可选）

仅用于 tray 图标，并且只在构建时能找到该库时才启用。上游声明该库本身采用 LGPL-2.1 或
LGPL-3.0，但编译进共享库的少数 source 文件（例如 `src/generate-id.c`）带有 GPL-3.0 的
文件头。Deskhub 动态 link 发行版提供的共享库，既不捆绑也不修改它。

## Command line client（`client/cli`）

command line client 不使用桌面 app 的 GUI toolkit：Linux 上不依赖 GTK，Windows 上
不依赖 wxWidgets。与所有 app 一样，它内嵌下文列出的 QUIC transport 和 audio codec。

在 Linux 上，它还 link 与 Linux app 相同的媒体组件 —— FFmpeg **静态** link，适用上文
Linux app 一节所述的 LGPL-2.1 条款；GLib/GIO、PipeWire、libva、libdrm、libepoxy 和 EGL
动态 link。`connect` 会打开远程屏幕窗口，这需要桌面 app 不使用的两个额外库：

| 组件 | License | link 方式 |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | 动态（Linux） |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | 动态（Linux） |

在 Windows 上，CLI 复用 app 的 Win32 viewer 窗口，不增加新的库。在 macOS 上，CLI
使用 Apple SDK 的 AppKit 和 ScreenCaptureKit，但不提供远程屏幕 viewer。

## Windows app（`client/windows`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3.1 | wxWindows Library Licence 3.1 | **静态** |
| 随 wxWidgets 附带的库 —— 见下文 | 多种宽松许可证 | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| [C++/WinRT](https://github.com/microsoft/cppwinrt) projection 头文件 | MIT | 仅头文件 |
| Media Foundation、Direct3D 11、DXGI、Windows.Graphics.Capture、WASAPI | Microsoft Windows SDK | OS 组件 |

### wxWidgets（wxWindows Library Licence，静态 link）

Windows app 从官方发布归档构建未经修改的上游 wxWidgets 3.3.1（configure 时由 CMake 抓
取，并与钉死的 hash 校验，见 `client/windows/win32/CMakeLists.txt`），并把它静态 link
进 `Deskhub.exe`。wxWindows Library Licence 就是 LGPL 加上一条例外，明确允许按分发者自
己的条款分发 link 了该库的 binary —— 静态或动态都可以 —— 所以 Deskhub 那份单文件的 MIT
分发不受影响。许可证原文：[`licenses/wxWindows.txt`](licenses/wxWindows.txt) 以及
<https://www.wxwidgets.org/about/licence/>。

Deskhub link 的是 `wx::base` 和 `wx::core`。在 Windows 上，wxWidgets 会自行构建下列库
并 link 进这两个库，因此它们可能出现在 `Deskhub.exe` 中。这些库都未经修改。

| 组件 | License |
| --- | --- |
| [zlib](https://zlib.net) | Zlib |
| [PCRE2](https://github.com/PCRE2Project/pcre2)（`wxRegEx`） | BSD-3-Clause WITH PCRE2-exception |
| [libpng](http://www.libpng.org/pub/png/libpng.html) | libpng-2.0 |
| [IJG libjpeg](https://www.ijg.org) 9f | IJG |
| [LibTIFF](https://libtiff.gitlab.io/libtiff/) | libtiff |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD-3-Clause |
| [NanoSVG](https://github.com/memononen/nanosvg) | Zlib |

IJG 许可证要求如下致谢声明：*this software is based in part on the work of the
Independent JPEG Group.* 全部七个库的声明见
[`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt)。

`third_party/nvenc-13.0` 是一个 git submodule，里面只有 NVIDIA Video Codec SDK 的 API
头文件，与 FFmpeg 项目按 MIT License 再分发的版本相同。NVENC 的实现本身在用户的 NVIDIA
driver 里（`nvEncodeAPI64.dll`），在运行时解析；它没有被捆绑。

## Apple app（`client/macos`、`client/ios`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、AudioToolbox、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement、IOKit、UserNotifications | Apple SDK | OS 组件（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、AudioToolbox、CoreMedia、CoreVideo、Accelerate (vImage)、Photos、PhotosUI、UniformTypeIdentifiers、UserNotifications | Apple SDK | OS 组件（iOS） |

## Android app（`client/android`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) —— Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | 编译进 APK |
| [Kotlin](https://kotlinlang.org) 标准库 | Apache-2.0 | 编译进 APK |
| [CameraX](https://developer.android.com/media/camera/camerax) 1.4 —— `camera-camera2`、`camera-lifecycle`、`camera-view`（扫描 QR） | Apache-2.0 | 编译进 APK |
| [ZXing](https://github.com/zxing/zxing) `core` 3.5（解码 QR） | Apache-2.0 | 编译进 APK |
| [kotlinx.coroutines](https://github.com/Kotlin/kotlinx.coroutines), [kotlinx.serialization](https://github.com/Kotlin/kotlinx.serialization) core, [Guava `listenablefuture`](https://github.com/google/guava), [AutoValue](https://github.com/google/auto) annotations, [JetBrains annotations](https://github.com/JetBrains/java-annotations), [JSpecify](https://jspecify.dev)——由上述库间接引入 | Apache-2.0 | 编译进 APK |
| [libyuv](https://chromium.googlesource.com/libyuv/libyuv/)，位于 CameraX `camera-core` 的 native 库中 | BSD-3-Clause | **静态**（`camera-core` 的 native 库） |
| [LLVM libc++](https://libcxx.llvm.org)（`ANDROID_STL=c++_static`） | Apache-2.0 WITH LLVM-exception | **静态**（native 库） |
| MediaCodec、AAudio、NDK 的 media API | Android SDK / NDK | OS 组件 |

APK 中 Java 与 Kotlin 库的完整清单——release 构建的每个传递依赖，附其版本、许可证、项目、作者、
随附的 NOTICE 文件以及 libyuv 的许可证原文——见 [`licenses/android-libraries.txt`](licenses/android-libraries.txt)，
需与 [`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt) 一并阅读。它由
[`scripts/android-library-notices.py`](scripts/android-library-notices.py) 根据 Gradle 的
`releaseRuntimeClasspath` 生成，因此每当 Android 依赖变化时都要重新运行。

## QUIC transport（所有 app）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静态** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（由 quiche 经 `boring-sys` 4.22.0 捆绑） | OpenSSL License AND SSLeay AND ISC AND MIT | **静态** |
| 编译进 quiche 的 Rust crate —— 见下文 | 见下文 | **静态** |
| [Rust 标准库](https://github.com/rust-lang/rust) | MIT OR Apache-2.0 | **静态** |

Deskhub 的加密 transport 内嵌 Cloudflare quiche，由
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) 固定版本和 commit。构建时应用
[`patches/quiche-key-exporter.patch`](patches/quiche-key-exporter.patch)，用于导出 TLS
密钥材料并通过 RustCrypto `ssh-key` 解析 OpenSSH 私钥。quiche 自带 BoringSSL（通过
`boring` crate）。这些库静态 link 进每一个 app 以及 command line client。许可证原文：
[`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt)、
[`licenses/BoringSSL.txt`](licenses/BoringSSL.txt)，Rust crate 则见
[`licenses/rust-crates.txt`](licenses/rust-crates.txt) 与
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt)。

BoringSSL 是 OpenSSL 的分支，其 OpenSSL 与 SSLeay 许可证要求保留以下致谢声明（保持英文
原文）：

- This product includes software developed by the OpenSSL Project for use in the OpenSSL
  Toolkit (<http://www.openssl.org/>).
- This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).
  This product includes software written by Tim Hudson (tjh@cryptsoft.com).

### Rust crate

下表是在应用补丁后，对 Deskhub 构建的每个 target 运行
`cargo tree -e normal -p quiche --features ffi`、由 quiche 的 `Cargo.lock` 解析出的
crate。过程宏（`enum_dispatch`、`openssl-macros`、`foreign-types-macros`、
`curve25519-dalek-derive`，以及 `syn`、`quote`、`proc-macro2`、`unicode-ident`）只在编译
时运行，不会被 link。

| License | Crate |
| --- | --- |
| BSD-2-Clause | `quiche` 0.29.3、`octets` 0.3.6 |
| BSD-3-Clause | `curve25519-dalek` 4.1.3、`ed25519-dalek` 2.2.0、`subtle` 2.6.1 |
| Apache-2.0 | `boring` 4.22.0 |
| MIT | `boring-sys` 4.22.0、`bytes`、`debug_panic`、`generic-array`、`libm`、`memoffset`、`slab` |
| Unlicense OR MIT | `byteorder` |
| MIT OR Apache-2.0 | `ssh-key` 0.6.7、`ssh-cipher`、`ssh-encoding`、`ed25519`、`p256`、`ecdsa`、`elliptic-curve`、`primeorder`、`sec1`、`rfc6979`、`crypto-bigint`、`ff`、`group`、`signature`、`aes`、`aes-gcm`、`aead`、`ghash`、`polyval`、`universal-hash`、`chacha20`、`poly1305`、`cipher`、`ctr`、`cbc`、`inout`、`block-padding`、`bcrypt-pbkdf`、`blowfish`、`pbkdf2`、`hmac`、`sha2`、`digest`、`block-buffer`、`crypto-common`、`cpufeatures`、`opaque-debug`、`der`、`spki`、`pem-rfc7468`、`base16ct`、`base64ct`、`const-oid`、`rand_core`、`getrandom`、`zeroize`、`typenum`、`bitflags`、`cfg-if`、`either`、`foreign-types`、`foreign-types-shared`、`intrusive-collections`、`libc`、`log`、`smallvec`；仅 Windows：`windows-sys`、`windows-targets`、`windows_x86_64_msvc` |

BSD-3-Clause crate 的版权为：© 2016-2021 isis agora lovecruft 与 Henry de Valence
（`curve25519-dalek`，其中还含有源自 Adam Langley 的 Go 版 ed25519 的代码）、
© 2017-2019 isis agora lovecruft（`ed25519-dalek`）、© 2016-2024 Isis Agora Lovecruft
与 Henry de Valence（`subtle`）。每个 crate 的完整声明（复制自其已发布的 source）见
[`licenses/rust-crates.txt`](licenses/rust-crates.txt)；quiche 的 pin 变动时，
[`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) 会从 quiche 源码树重新生成它。

## Audio codec（所有 app）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静态** |

Deskhub 的音频 stream 内嵌了 libopus，由
[`scripts/build-opus.sh`](scripts/build-opus.sh) 从钉死到确切版本和 checksum 的、未经修
改的上游 source 构建，并静态 link 进每一个 app 以及 command line client。同一份钉死的
source 按 target 分别构建，因此全部六个程序 —— 五个 app 加 command line client —— 运行
相同的 encoder 和 decoder。这个库没有被修改。许可证原文：
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 专利

H.264/AVC 受通过 [Via LA](https://www.via-la.com/) 授权的专利覆盖。专利权与上面那些版权
license 是分开的，MIT License 并不授予专利权。在 Windows、macOS、iOS 和 Android 上，
encode 和 decode 由操作系统自己的 codec 完成。在 Linux 上，encode 由 GPU 厂商的 VA-API
driver 或 NVIDIA 的 NVENC driver 完成；decode 则经过 Deskhub 捆绑的 FFmpeg H.264
decoder：有 VA-API 时由 GPU 解码，没有时由 FFmpeg 软件解码。这个 FFmpeg decoder 是
Deskhub 发布的唯一 H.264 实现。

音频 codec Opus 同样是被捆绑进来的。Opus 作为免版税 codec 发布，覆盖它的专利许可也按免
版税条款授予 —— 相关声明列在
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) 以及
<https://opus-codec.org/license/>。
