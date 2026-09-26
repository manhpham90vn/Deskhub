[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · **中文** · [日本語](THIRD_PARTY_NOTICES.ja.md)

# 第三方声明

Deskhub 本身以 MIT License 分发 —— 见 [`LICENSE`](LICENSE)。

本文件列出 Deskhub 使用的第三方组件、link 方式及相应的 license 义务。Deskhub 自身的
source 仍采用 MIT License；表中各组件保留各自的 license。

本文件是 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 的译本；若两者有出入，以英
文版为准。

## Linux app（`client/linux`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 —— `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 动态 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 动态 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 动态 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 动态 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 动态 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 动态 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | 动态，可选（tray 图标） |

### FFmpeg（LGPL-2.1-or-later，静态 link）

Linux app 是唯一捆绑 FFmpeg 的 target。它由
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh) 从未经修改的上游 FFmpeg 8.0 source
构建，配置时**不加** `--enable-gpl`、**不加** `--enable-nonfree`，只启用原生的 H.264
decoder 和 VA-API hwaccel。因此产出的 `libavcodec` 和 `libavutil` 受 GNU Lesser General
Public License 2.1 或更高版本约束 —— 全文见
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt)。

由于这些库是静态 link 的，LGPL-2.1 §6 要求接收者能够用修改过的 FFmpeg 重新 link 这个
应用。Deskhub 通过提供完整 source 来满足这一点：应用的 source 就在本 repo 里、以 MIT
License 提供，而 `scripts/build-ffmpeg.sh` 能复现出完全一样的 FFmpeg 构建。任何人都可以
换上自己的 FFmpeg，然后用 `make` 重新构建。

FFmpeg 没有被以任何方式修改。上游 source：<https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` 是一个 git submodule，里面只有 NVIDIA Video Codec SDK 的 API
头文件，与 FFmpeg 项目按 MIT License 再分发的版本相同。NVENC 的实现本身在用户的 NVIDIA
driver 里（`libnvidia-encode.so.1`、`libcuda.so.1`），在运行时解析；它没有被捆绑。

### GTK 3（LGPL-2.1-or-later，动态 link）

动态 link 到发行版提供的共享库。没有捆绑也没有修改任何 GTK source；用户可以自由替换系统
库。

## Command line client（`client/cli`）

command line client 不使用桌面 app 的 GUI toolkit：Linux 上不依赖 GTK，Windows 上
不依赖 wxWidgets。`connect` 在这两个平台上会打开远程屏幕窗口；在 Linux 上，这需要
桌面 app 不使用的两个额外库：

| 组件 | License | link 方式 |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | 动态（Linux） |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | 动态（Linux） |

在 Windows 上，CLI 复用 app 的 Win32 viewer 窗口，不增加新的库。在 macOS 上，CLI
使用 Apple SDK 的 AppKit 和 ScreenCaptureKit，但不提供远程屏幕 viewer。

## Windows app（`client/windows`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| Media Foundation、Direct3D 11、DXGI | Microsoft Windows SDK | OS 组件 |

### wxWidgets（wxWindows Library Licence，静态 link）

Windows app 从官方发布归档构建未经修改的上游 wxWidgets（configure 时由 CMake 抓取，见
`client/windows/win32/CMakeLists.txt`），并把它静态 link 进 `Deskhub.exe`。wxWindows
Library Licence 就是 LGPL 加上一条例外，明确允许按分发者自己的条款分发 link 了该库的
binary —— 静态或动态都可以 —— 所以 Deskhub 那份单文件的 MIT 分发不受影响。许可证原文：
<https://www.wxwidgets.org/about/licence/>。

`third_party/nvenc-13.0` 是一个 git submodule，里面只有 NVIDIA Video Codec SDK 的 API
头文件，与 FFmpeg 项目按 MIT License 再分发的版本相同。NVENC 的实现本身在用户的 NVIDIA
driver 里（`nvEncodeAPI64.dll`），在运行时解析；它没有被捆绑。

## Apple app（`client/macos`、`client/ios`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement | Apple SDK | OS 组件（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、Accelerate (vImage)、Photos、UserNotifications | Apple SDK | OS 组件（iOS） |

## Android app（`client/android`）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) —— Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | 动态 |
| [Kotlin](https://kotlinlang.org) 标准库 | Apache-2.0 | 动态 |
| MediaCodec、NDK 的 media API | Android SDK / NDK | OS 组件 |

## QUIC transport（所有 app）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静态** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（由 quiche 捆绑） | OpenSSL / ISC 风格 | **静态** |

Deskhub 的 encrypt transport 内嵌了 Cloudflare 的 quiche，由
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) 从钉死到确切版本和 commit 的、未经
修改的上游 source 构建。quiche 自带一份 BoringSSL（通过 `boring` crate），它提供 TLS 以
及 Deskhub pairing 背后的密码学。两者都静态 link 进每一个 app。两个库都没有被修改。

## Audio codec（所有 app）

| 组件 | License | link 方式 |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静态** |

Deskhub 的音频 stream 内嵌了 libopus，由
[`scripts/build-opus.sh`](scripts/build-opus.sh) 从钉死到确切版本和 checksum 的、未经修
改的上游 source 构建，并静态 link 进每一个 app。一次构建服务五个平台：共享的机器上跑同
一个 encoder，每个 viewer 上跑同一个 decoder。这个库没有被修改。许可证原文：
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 专利

H.264/AVC 受通过 [Via LA](https://www.via-la.com/) 授权的专利覆盖。专利权与上面那些版权
license 是分开的，MIT License 并不授予专利权。在 Windows、macOS、iOS 和 Android 上，
encode 和 decode 由操作系统自己的 codec 完成。在 Linux 上则由 GPU 厂商的 VA-API driver
完成。Deskhub 不发布任何自己的 video codec 实现。

音频 codec Opus 是例外：libopus 是被捆绑进来的。Opus 作为免版税 codec 发布，覆盖它的专
利许可也按免版税条款授予 —— 相关声明列在
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) 以及
<https://opus-codec.org/license/>。
