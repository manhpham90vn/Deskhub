[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · [中文](THIRD_PARTY_NOTICES.zh.md) · **日本語**

# サードパーティ告知

Deskhub 自体は MIT ライセンスで配布している — [`LICENSE`](LICENSE) を参照。

本書は Deskhub がリンクするサードパーティ製コンポーネントと、それに伴う義務を列挙する。
ここに GPL でライセンスされたものはなく、Deskhub を MIT ライセンスで再配布することを
制限するコンポーネントもない。

> 本書は [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) の翻訳である。相違がある
> 場合は**英語版が正典**となる。

## Linux アプリ（`client/linux`）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 — `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 ヘッダ） | MIT | ヘッダのみ |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 動的 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 動的 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 動的 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 動的 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 動的 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 動的 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | 動的、任意（トレイアイコン） |

### FFmpeg（LGPL-2.1-or-later、静的リンク）

FFmpeg を同梱するのは Linux アプリだけである。[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh)
が未改変の上流 FFmpeg 8.0 ソースからビルドし、`--enable-gpl` **なし**、
`--enable-nonfree` **なし**で、ネイティブの H.264 デコーダと VA-API hwaccel だけを
有効にする。したがって生成される `libavcodec` と `libavutil` は GNU 劣等一般公衆
ライセンス バージョン 2.1 以降の対象となる — 全文は
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt)。

これらのライブラリは静的にリンクされるため、LGPL-2.1 第 6 条は、受領者が改変版の FFmpeg
に対してアプリケーションを再リンクできることを要求する。Deskhub は完全なソースを提供する
ことでこれを満たしている。アプリケーションのソースは MIT ライセンスで本リポジトリにあり、
`scripts/build-ffmpeg.sh` がまったく同じ FFmpeg ビルドを再現する。誰でも自分の FFmpeg に
差し替えて `make` で再ビルドできる。

FFmpeg には一切改変を加えていない。上流ソース：<https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` は git サブモジュールで、FFmpeg プロジェクトが MIT ライセンスで
再配布している NVIDIA Video Codec SDK の API ヘッダのみを含む。NVENC の実装自体は
ユーザーの NVIDIA ドライバ（`libnvidia-encode.so.1`、`libcuda.so.1`）にあり、実行時に
解決される。同梱はしていない。

### GTK 3（LGPL-2.1-or-later、動的リンク）

ディストリビューションが提供する共有ライブラリに動的リンクする。GTK のソースは同梱も
改変もしていない。利用者はシステムのライブラリを自由に差し替えられる。

## コマンドラインクライアント（`client/cli`）

コマンドラインクライアントは、ビルド対象のシステムのデスクトップアプリと同じライブラリを
リンクし、GUI ツールキットだけを除く — Linux では GTK なし、Windows では wxWidgets なし。
リモート画面を自前のウィンドウに描くため、Linux ではアプリに不要なライブラリを 2 つ
追加する：

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | 動的（Linux） |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | 動的（Linux） |

Windows ではアプリ自身の Win32 ビューアウィンドウを再利用するので、追加はない。macOS では
アプリと同じく Apple SDK の AppKit と ScreenCaptureKit を使う。

## Windows アプリ（`client/windows`）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 ヘッダ） | MIT | ヘッダのみ |
| Media Foundation、Direct3D 11、DXGI | Microsoft Windows SDK | OS コンポーネント |

### wxWidgets（wxWindows Library Licence、静的リンク）

Windows アプリは公式リリースアーカイブから未改変の上流 wxWidgets をビルドし（CMake が
configure 時に取得する。`client/windows/win32/CMakeLists.txt` を参照）、`Deskhub.exe` へ
静的にリンクする。wxWindows Library Licence は LGPL に例外を加えたもので、ライブラリに
リンクしたバイナリを — 静的でも動的でも — 配布者自身の条件で配布することを明示的に
許している。したがって Deskhub を単一ファイルの MIT 配布にすることに影響はない。
ライセンス文：<https://www.wxwidgets.org/about/licence/>。

`third_party/nvenc-13.0` は git サブモジュールで、FFmpeg プロジェクトが MIT ライセンスで
再配布している NVIDIA Video Codec SDK の API ヘッダのみを含む。NVENC の実装自体は
ユーザーの NVIDIA ドライバ（`nvEncodeAPI64.dll`）にあり、実行時に解決される。同梱はして
いない。

## Apple アプリ（`client/macos`、`client/ios`）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement | Apple SDK | OS コンポーネント（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、CoreMedia、CoreVideo、Accelerate（vImage）、Photos、UserNotifications | Apple SDK | OS コンポーネント（iOS） |

## Android アプリ（`client/android`）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) — Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | 動的 |
| [Kotlin](https://kotlinlang.org) 標準ライブラリ | Apache-2.0 | 動的 |
| MediaCodec、NDK メディア API | Android SDK / NDK | OS コンポーネント |

## QUIC トランスポート（全アプリ）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静的** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（quiche が同梱） | OpenSSL / ISC 系 | **静的** |

Deskhub の暗号化トランスポートは Cloudflare の quiche を埋め込んでいる。
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) が正確なバージョンとコミットに固定
した未改変の上流ソースからビルドする。quiche は自前の BoringSSL を（`boring` クレート
経由で）持ち、TLS と Deskhub のペアリングを支える暗号を提供する。どちらも全アプリに静的に
リンクされる。いずれのライブラリも改変していない。

## 音声コーデック（全アプリ）

| コンポーネント | ライセンス | リンク |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静的** |

Deskhub の音声ストリーミングは libopus を埋め込んでいる。
[`scripts/build-opus.sh`](scripts/build-opus.sh) が正確なバージョンとチェックサムに固定
した未改変の上流ソースからビルドし、全アプリに静的にリンクする。1 つのビルドが 5 つの
プラットフォームすべてに対応し、共有する側では同じエンコーダが、どのビューアでも同じ
デコーダが動く。ライブラリは改変していない。ライセンス文：
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 特許

H.264/AVC は [Via LA](https://www.via-la.com/) を通じてライセンスされる特許の対象である。
特許権は上記の著作権ライセンスとは別のもので、MIT ライセンスによって付与されるものでは
ない。Windows、macOS、iOS、Android では、エンコードとデコードは OS 自身のコーデックが
行う。Linux では GPU ベンダの VA-API ドライバが行う。Deskhub は自前の映像コーデック実装を
一切同梱していない。

音声コーデックの Opus は例外で、libopus は同梱している。Opus はロイヤリティフリーの
コーデックとして公開されており、それを覆う特許ライセンスもロイヤリティフリーの条件で
付与されている — 宣言は
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) と
<https://opus-codec.org/license/> に列挙されている。
