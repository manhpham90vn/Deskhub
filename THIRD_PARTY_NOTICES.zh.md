[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · **中文** · [日本語](THIRD_PARTY_NOTICES.ja.md)

# 第三方声明

Deskhub 本身按 MIT 许可证分发 —— 见 [`LICENSE`](LICENSE)。

本文件列出 Deskhub 所链接的第三方组件，以及随之而来的义务。这里没有任何组件采用 GPL，
也没有任何组件限制以 MIT 许可证再分发 Deskhub。

> 本文件是 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) 的译本。若两者有出入，
> **以英文版为准**。

## Linux 应用（`client/linux`）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 —— `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 动态 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 动态 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 动态 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 动态 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 动态 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 动态 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | 动态，可选（托盘图标） |

### FFmpeg（LGPL-2.1-or-later，静态链接）

Linux 应用是唯一捆绑 FFmpeg 的目标。它由
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh) 从未经修改的上游 FFmpeg 8.0 源码
构建，配置时**不带** `--enable-gpl`、**不带** `--enable-nonfree`，只启用原生 H.264
解码器和 VA-API 硬件加速。因此产出的 `libavcodec` 和 `libavutil` 受 GNU 宽通用公共许可证
第 2.1 版或更新版本约束 —— 全文见
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt)。

由于这些库是静态链接的，LGPL-2.1 第 6 条要求接收者能够把应用与修改过的 FFmpeg 重新链接。
Deskhub 通过提供完整源码来满足这一点：应用源码就在本仓库里，采用 MIT 许可证，而
`scripts/build-ffmpeg.sh` 能复现出完全相同的 FFmpeg 构建。任何人都可以换上自己的 FFmpeg
并用 `make` 重新构建。

FFmpeg 没有被做任何修改。上游源码：<https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` 是一个 git 子模块，仅包含 FFmpeg 项目按 MIT 许可证再分发的
NVIDIA Video Codec SDK API 头文件。NVENC 的实现本身位于用户的 NVIDIA 驱动中
（`libnvidia-encode.so.1`、`libcuda.so.1`），在运行时解析；并未被捆绑。

### GTK 3（LGPL-2.1-or-later，动态链接）

动态链接到发行版提供的共享库。没有捆绑或修改任何 GTK 源码；用户可以自由替换系统库。

## 命令行客户端（`client/cli`）

命令行客户端链接的库与其构建目标系统上的桌面应用相同，只是少了图形工具包 —— Linux 上没有
GTK，Windows 上没有 wxWidgets。它会在自己的窗口里绘制远端画面，这在 Linux 上比应用多用
了两个库：

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | 动态（Linux） |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | 动态（Linux） |

在 Windows 上它复用应用自己的 Win32 观看窗口，因此没有新增。在 macOS 上它使用 Apple SDK
里的 AppKit 和 ScreenCaptureKit，与应用相同。

## Windows 应用（`client/windows`）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **静态** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 头文件） | MIT | 仅头文件 |
| Media Foundation、Direct3D 11、DXGI | Microsoft Windows SDK | 系统组件 |

### wxWidgets（wxWindows Library Licence，静态链接）

Windows 应用从官方发布归档构建未经修改的上游 wxWidgets（由 CMake 在配置阶段抓取，见
`client/windows/win32/CMakeLists.txt`），并把它静态链接进 `Deskhub.exe`。wxWindows
Library Licence 是 LGPL 加上一条例外，明确允许按分发者自己的条款分发与该库链接的二进制
—— 无论静态还是动态 —— 所以 Deskhub 以 MIT 许可证做单文件分发不受影响。许可证文本：
<https://www.wxwidgets.org/about/licence/>。

`third_party/nvenc-13.0` 是一个 git 子模块，仅包含 FFmpeg 项目按 MIT 许可证再分发的
NVIDIA Video Codec SDK API 头文件。NVENC 的实现本身位于用户的 NVIDIA 驱动中
（`nvEncodeAPI64.dll`），在运行时解析；并未被捆绑。

## Apple 应用（`client/macos`、`client/ios`）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement | Apple SDK | 系统组件（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、Accelerate（vImage）、Photos、UserNotifications | Apple SDK | 系统组件（iOS） |

## Android 应用（`client/android`）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) —— Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | 动态 |
| [Kotlin](https://kotlinlang.org) 标准库 | Apache-2.0 | 动态 |
| MediaCodec、NDK media API | Android SDK / NDK | 系统组件 |

## QUIC 传输（所有应用）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静态** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（由 quiche 捆绑） | OpenSSL / ISC 风格 | **静态** |

Deskhub 的加密传输内嵌 Cloudflare 的 quiche，由
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) 从锁定到确切版本与提交的未修改上游
源码构建。quiche 自带一份 BoringSSL（通过 `boring` crate），它提供 TLS 以及 Deskhub
配对背后的密码学。两者都静态链接进每一个应用。两个库都未经修改。

## 音频编解码器（所有应用）

| 组件 | 许可证 | 链接方式 |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静态** |

Deskhub 的音频流内嵌 libopus，由 [`scripts/build-opus.sh`](scripts/build-opus.sh) 从锁定
到确切版本与校验和的未修改上游源码构建，并静态链接进每一个应用。一次构建服务全部五个
平台：共享端跑的是同一个编码器，每个观看端跑的是同一个解码器。该库未经修改。许可证文本：
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 专利

H.264/AVC 受专利保护，专利许可通过 [Via LA](https://www.via-la.com/) 授予。专利权与上面
的著作权许可是分开的，MIT 许可证并不授予专利权。在 Windows、macOS、iOS 和 Android 上，
编码和解码由操作系统自带的编解码器完成。在 Linux 上，由 GPU 厂商的 VA-API 驱动完成。
Deskhub 不附带自己的任何视频编解码器实现。

音频编解码器 Opus 是例外：libopus 是被捆绑的。Opus 作为免版税编解码器发布，覆盖它的专利
许可也按免版税条款授予 —— 相关声明列在
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) 和
<https://opus-codec.org/license/>。
