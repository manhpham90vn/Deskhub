[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · [中文](THIRD_PARTY_NOTICES.zh.md) · **日本語**

# サードパーティに関する表示

Deskhub 自体は MIT License で配布している —— [`LICENSE`](LICENSE) を参照。

本書では、Deskhub が使用するサードパーティ製コンポーネント、その link 方法、適用される
license 上の義務を一覧にする。Deskhub 自体の source は MIT License のままで、
各コンポーネントにはそれぞれの license が適用される。

本書は [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) の翻訳。食い違いがある場合は
英語版が正文。

## Linux app（`client/linux`）

| コンポーネント | License | link |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 —— `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 のヘッダ） | MIT | ヘッダのみ |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 動的 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 動的 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 動的 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 動的 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 動的 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 動的 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | 動的、任意（tray アイコン） |

### FFmpeg（LGPL-2.1-or-later、静的 link）

FFmpeg を同梱している target は Linux app だけだ。改変していない上流 FFmpeg 8.0 の
source から [`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh) が build しており、
`--enable-gpl` **なし**、`--enable-nonfree` **なし**、有効なのはネイティブの H.264
decoder と VA-API の hwaccel だけだ。したがってできあがる `libavcodec` と
`libavutil` は GNU Lesser General Public License バージョン 2.1 以降の対象になる ——
全文は [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt) に。

これらのライブラリは静的に link されているため、LGPL-2.1 §6 は、受け取った人が改変した
FFmpeg でアプリケーションを再 link できることを求める。Deskhub は完全な source を
提供することでこれを満たしている。アプリケーションの source は本 repo に MIT License
で置かれており、`scripts/build-ffmpeg.sh` はまったく同じ FFmpeg の build を再現する。
誰でも自分の FFmpeg に差し替えて `make` で build し直せる。

FFmpeg には一切手を加えていない。上流の source: <https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` は git submodule で、中身は NVIDIA Video Codec SDK の API
ヘッダだけだ。FFmpeg プロジェクトが MIT License で再配布しているものと同じものである。
NVENC の実装そのものはユーザーの NVIDIA driver（`libnvidia-encode.so.1`、
`libcuda.so.1`）にあり、実行時に解決される。同梱はしていない。

### GTK 3（LGPL-2.1-or-later、動的 link）

ディストリビューションが提供する共有ライブラリに動的に link している。GTK の source を
同梱も改変もしていない。ユーザーはシステムのライブラリを自由に差し替えられる。

## Command line client（`client/cli`）

command line client はデスクトップ app の GUI toolkit を使わず、Linux では GTK、
Windows では wxWidgets に依存しない。両プラットフォームの `connect` はリモート画面の
ウィンドウを開く。Linux では、そのためにデスクトップ app が使わないライブラリが
2 つ必要になる。

| コンポーネント | License | link |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | 動的（Linux） |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | 動的（Linux） |

Windows では app の Win32 viewer ウィンドウを再利用するため、追加のライブラリは
不要。macOS の CLI は Apple SDK の AppKit と ScreenCaptureKit を使うが、リモート
画面の viewer は提供しない。

## Windows app（`client/windows`）

| コンポーネント | License | link |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 のヘッダ） | MIT | ヘッダのみ |
| Media Foundation、Direct3D 11、DXGI | Microsoft Windows SDK | OS コンポーネント |

### wxWidgets（wxWindows Library Licence、静的 link）

Windows app は公式のリリースアーカイブから改変していない上流の wxWidgets を build し
（configure 時に CMake が取得する。`client/windows/win32/CMakeLists.txt` を参照）、
`Deskhub.exe` に静的に link する。wxWindows Library Licence は LGPL に例外を足したもの
で、そのライブラリに link した binary を —— 静的でも動的でも —— 配布者自身の条件で配布
することを明示的に認めている。したがって Deskhub の単一ファイル・MIT の配布には影響が
ない。ライセンス本文: <https://www.wxwidgets.org/about/licence/>。

`third_party/nvenc-13.0` は git submodule で、中身は NVIDIA Video Codec SDK の API
ヘッダだけだ。FFmpeg プロジェクトが MIT License で再配布しているものと同じものである。
NVENC の実装そのものはユーザーの NVIDIA driver（`nvEncodeAPI64.dll`）にあり、実行時に
解決される。同梱はしていない。

## Apple の app（`client/macos`、`client/ios`）

| コンポーネント | License | link |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement | Apple SDK | OS コンポーネント（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、Accelerate (vImage)、Photos、UserNotifications | Apple SDK | OS コンポーネント（iOS） |

## Android app（`client/android`）

| コンポーネント | License | link |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) —— Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | 動的 |
| [Kotlin](https://kotlinlang.org) 標準ライブラリ | Apache-2.0 | 動的 |
| MediaCodec、NDK の media API | Android SDK / NDK | OS コンポーネント |

## QUIC transport（すべての app）

| コンポーネント | License | link |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静的** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（quiche が同梱） | OpenSSL / ISC 系 | **静的** |

Deskhub の encrypt された transport は Cloudflare の quiche を組み込んでいる。
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) が正確なバージョンと commit に
固定した、改変していない上流の source から build している。quiche は自前の BoringSSL
を（`boring` crate 経由で）持っており、それが TLS と Deskhub の pairing の背後にある
暗号を提供する。どちらもすべての app に静的に link される。いずれのライブラリも改変
していない。

## Audio codec（すべての app）

| コンポーネント | License | link |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静的** |

Deskhub の音声 stream は libopus を組み込んでいる。
[`scripts/build-opus.sh`](scripts/build-opus.sh) が正確なバージョンと checksum に固定
した、改変していない上流の source から build し、すべての app に静的に link している。
1 つの build が 5 つのプラットフォームすべてをまかなう。共有するマシンでは同じ
encoder が、どの viewer でも同じ decoder が動く。このライブラリは改変していない。
ライセンス本文: [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 特許

H.264/AVC は [Via LA](https://www.via-la.com/) を通じてライセンスされる特許の対象だ。
特許の権利は上記の著作権 license とは別であり、MIT License によって付与されるもので
はない。Windows、macOS、iOS、Android では、encode と decode は OS 自身の codec が行う。
Linux では GPU ベンダーの VA-API driver が行う。Deskhub は自前の video codec の実装を
一切出荷していない。

音声の codec である Opus は例外で、libopus は同梱している。Opus はロイヤリティフリーの
codec として公開されており、それを覆う特許ライセンスもロイヤリティフリーの条件で付与
されている —— 宣言は
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) と
<https://opus-codec.org/license/> に列挙されている。
