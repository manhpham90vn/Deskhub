[English](THIRD_PARTY_NOTICES.md) · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md) · [中文](THIRD_PARTY_NOTICES.zh.md) · **日本語**

# サードパーティに関する表示

Deskhub 自体は MIT License で配布している —— [`LICENSE`](LICENSE) を参照。

本書では、Deskhub が使用するサードパーティ製コンポーネント、その link 方法、適用される
license 上の義務を一覧にする。Deskhub 自体の source は MIT License のままで、
各コンポーネントにはそれぞれの license が適用される。

本書は [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) の翻訳。食い違いがある場合は
英語版が正文。

すべてのパッケージには、本書と、[`licenses/`](licenses/) にあるライセンス本文のうち
そのパッケージに該当するものが同梱される。

| ファイル | 対象 | 同梱先 |
| --- | --- | --- |
| [`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt) | quiche と `octets` | すべてのパッケージ |
| [`licenses/BoringSSL.txt`](licenses/BoringSSL.txt) | BoringSSL | すべてのパッケージ |
| [`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt) | Rust crate と Android ライブラリが参照する Apache License 2.0 の本文 | すべてのパッケージ |
| [`licenses/rust-crates.txt`](licenses/rust-crates.txt) | quiche に組み込まれる各 Rust crate とその表示 —— [`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) が生成 | すべてのパッケージ |
| [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) | libopus | すべてのパッケージ |
| [`licenses/android-libraries.txt`](licenses/android-libraries.txt) | Android APK に同梱される各 Java / Kotlin ライブラリと、そのライセンス・プロジェクト・作者・同梱の NOTICE — [`scripts/android-library-notices.py`](scripts/android-library-notices.py) で生成 | Android APK |
| [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt) | FFmpeg。wxWindows Library Licence の基礎でもある | Linux の app と CLI のパッケージ、Windows app のインストーラー |
| [`licenses/wxWindows.txt`](licenses/wxWindows.txt) | wxWidgets | Windows app のインストーラー |
| [`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt) | wxWidgets が同梱するライブラリ —— zlib、libpng、IJG libjpeg、LibTIFF、PCRE2、libwebp、NanoSVG | Windows app のインストーラー |

置き場所: app と command line client の `.deb` と `.rpm` は本書を
`/usr/share/doc/<package>/` に、本文を `/usr/share/doc/<package>/licenses/` に置く（たとえば
LGPL の本文は `/usr/share/doc/deskhub/licenses/LGPL-2.1.txt`）。両方の Windows
インストーラーは実行ファイルの隣、すなわちインストール先フォルダとその `licenses`
サブフォルダに置く。macOS app は app bundle の `Contents/Resources` に、iOS app はその
bundle に、Android APK はその assets に収める。ポータブル版の command line バイナリは隣に
何も置けないため、各リリースは `LICENSE`、本書、`licenses/` フォルダ全体を収めた
`deskhub-<tag>-licenses.zip` も提供する。

## Linux app（`client/linux`）

| コンポーネント | License | link |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 —— `libavcodec`、`libavutil` | LGPL-2.1-or-later | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 のヘッダ） | MIT | ヘッダのみ |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | 動的 |
| [GLib / GIO](https://gitlab.gnome.org/GNOME/glib)（`gio-2.0`、`gio-unix-2.0`。画面共有 portal） | LGPL-2.1-or-later | 動的 |
| [PipeWire](https://pipewire.org) 0.3 | MIT | 動的 |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | 動的 |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | 動的 |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | 動的 |
| EGL（[libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa） | MIT | 動的 |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-2.1 または LGPL-3.0。組み込まれる一部のファイルは GPL-3.0 | 動的、任意（tray アイコン） |

Linux の command line client も同じメディアスタックを link する。FFmpeg は静的に、
GLib/GIO、PipeWire、libva、libdrm、libepoxy、EGL は動的に link する —— 後述の
command line client の節を参照。

### FFmpeg（LGPL-2.1-or-later、静的 link）

FFmpeg を同梱している target は Linux app と Linux の command line client だけだ。
checksum で固定した、改変していない上流 FFmpeg 8.0 の source から
[`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh) が build しており、
`--enable-gpl` **なし**、`--enable-nonfree` **なし**、有効なのは FFmpeg ネイティブの
H.264 decoder と VA-API の hwaccel だけだ。したがってできあがる `libavcodec` と
`libavutil` は GNU Lesser General Public License バージョン 2.1 以降の対象になる ——
全文は [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt) に。

これらのライブラリは静的に link されているため、LGPL-2.1 §6 は、受け取った人が改変した
FFmpeg でアプリケーションを再 link できることを求める。Deskhub は完全な source を
提供することでこれを満たしている。アプリケーションの source は本 repo に MIT License
で置かれており、`scripts/build-ffmpeg.sh` はまったく同じ FFmpeg の build を
`third_party/ffmpeg-min` に再現する。誰でもそこに自分の FFmpeg の build を置き（または
スクリプトを変更し）、app なら `make build-linux` / `make release-linux`、command line
client なら `make build-cli` / `make release-cli` で build し直せる。

FFmpeg には一切手を加えていない。上流の source: <https://ffmpeg.org/download.html>。

`third_party/nvenc-13.0` は git submodule で、中身は NVIDIA Video Codec SDK の API
ヘッダだけだ。FFmpeg プロジェクトが MIT License で再配布しているものと同じものである。
NVENC の実装そのものはユーザーの NVIDIA driver（`libnvidia-encode.so.1`、
`libcuda.so.1`）にあり、実行時に解決される。同梱はしていない。

### GTK 3 と GLib（LGPL-2.1-or-later、動的 link）

ディストリビューションが提供する共有ライブラリに動的に link している。GTK や GLib の
source を同梱も改変もしていない。ユーザーはシステムのライブラリを自由に差し替えられる。

### libayatana-appindicator（動的 link、任意）

tray アイコンにだけ使い、build 時にライブラリがある場合に限って有効になる。上流はライブ
ラリ自体を LGPL-2.1 または LGPL-3.0 としているが、共有ライブラリに組み込まれる一部の
source ファイル（例: `src/generate-id.c`）には GPL-3.0 のヘッダが付いている。Deskhub は
ディストリビューションの共有ライブラリに動的に link しており、同梱も改変もしていない。

## Command line client（`client/cli`）

command line client はデスクトップ app の GUI toolkit を使わず、Linux では GTK、
Windows では wxWidgets に依存しない。すべての app と同じく、後述の QUIC transport と
audio codec を組み込んでいる。

Linux では Linux app と同じメディアスタックも link する —— FFmpeg は上記 Linux app の
節で述べた LGPL-2.1 の条件のもとで**静的**に、GLib/GIO、PipeWire、libva、libdrm、
libepoxy、EGL は動的に link する。`connect` はリモート画面のウィンドウを開き、そのために
デスクトップ app が使わないライブラリが 2 つ必要になる。

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
| [wxWidgets](https://www.wxwidgets.org) 3.3.1 | wxWindows Library Licence 3.1 | **静的** |
| wxWidgets に同梱されるライブラリ —— 後述 | 各種の寛容なライセンス | **静的** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers)（NVENC SDK 13.0 のヘッダ） | MIT | ヘッダのみ |
| [C++/WinRT](https://github.com/microsoft/cppwinrt) の projection ヘッダ | MIT | ヘッダのみ |
| Media Foundation、Direct3D 11、DXGI、Windows.Graphics.Capture、WASAPI | Microsoft Windows SDK | OS コンポーネント |

### wxWidgets（wxWindows Library Licence、静的 link）

Windows app は公式のリリースアーカイブから改変していない上流の wxWidgets 3.3.1 を build
し（configure 時に CMake が取得し、固定した hash と照合する。
`client/windows/win32/CMakeLists.txt` を参照）、`Deskhub.exe` に静的に link する。
wxWindows Library Licence は LGPL に例外を足したもので、そのライブラリに link した
binary を —— 静的でも動的でも —— 配布者自身の条件で配布することを明示的に認めている。
したがって Deskhub の単一ファイル・MIT の配布には影響がない。ライセンス本文:
[`licenses/wxWindows.txt`](licenses/wxWindows.txt) および
<https://www.wxwidgets.org/about/licence/>。

Deskhub が link するのは `wx::base` と `wx::core` だ。Windows では、wxWidgets は次の
ライブラリを自前で build してこの 2 つに link するため、これらが `Deskhub.exe` に含まれる
ことがある。いずれも改変していない。

| コンポーネント | License |
| --- | --- |
| [zlib](https://zlib.net) | Zlib |
| [PCRE2](https://github.com/PCRE2Project/pcre2)（`wxRegEx`） | BSD-3-Clause WITH PCRE2-exception |
| [libpng](http://www.libpng.org/pub/png/libpng.html) | libpng-2.0 |
| [IJG libjpeg](https://www.ijg.org) 9f | IJG |
| [LibTIFF](https://libtiff.gitlab.io/libtiff/) | libtiff |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | BSD-3-Clause |
| [NanoSVG](https://github.com/memononen/nanosvg) | Zlib |

IJG のライセンスは次の謝辞を求めている: *this software is based in part on the work of the
Independent JPEG Group.* 7 つのライブラリすべての表示は
[`licenses/wxWidgets-bundled.txt`](licenses/wxWidgets-bundled.txt) にある。

`third_party/nvenc-13.0` は git submodule で、中身は NVIDIA Video Codec SDK の API
ヘッダだけだ。FFmpeg プロジェクトが MIT License で再配布しているものと同じものである。
NVENC の実装そのものはユーザーの NVIDIA driver（`nvEncodeAPI64.dll`）にあり、実行時に
解決される。同梱はしていない。

## Apple の app（`client/macos`、`client/ios`）

| コンポーネント | License | link |
| --- | --- | --- |
| SwiftUI、AppKit、ScreenCaptureKit、VideoToolbox、AVFoundation、AudioToolbox、CoreMedia、CoreVideo、CoreGraphics、ApplicationServices、ServiceManagement、IOKit、UserNotifications | Apple SDK | OS コンポーネント（macOS） |
| SwiftUI、UIKit、ReplayKit、VideoToolbox、AVFoundation、AudioToolbox、CoreMedia、CoreVideo、Accelerate (vImage)、Photos、PhotosUI、UniformTypeIdentifiers、UserNotifications | Apple SDK | OS コンポーネント（iOS） |

## Android app（`client/android`）

| コンポーネント | License | link |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) —— Core KTX、Activity、Compose UI、Material 3 | Apache-2.0 | APK に組み込み |
| [Kotlin](https://kotlinlang.org) 標準ライブラリ | Apache-2.0 | APK に組み込み |
| [CameraX](https://developer.android.com/media/camera/camerax) 1.4 —— `camera-camera2`、`camera-lifecycle`、`camera-view`（QR スキャン） | Apache-2.0 | APK に組み込み |
| [ZXing](https://github.com/zxing/zxing) `core` 3.5（QR デコード） | Apache-2.0 | APK に組み込み |
| [kotlinx.coroutines](https://github.com/Kotlin/kotlinx.coroutines), [kotlinx.serialization](https://github.com/Kotlin/kotlinx.serialization) core, [Guava `listenablefuture`](https://github.com/google/guava), [AutoValue](https://github.com/google/auto) annotations, [JetBrains annotations](https://github.com/JetBrains/java-annotations), [JSpecify](https://jspecify.dev) — 上記のライブラリが依存するもの | Apache-2.0 | APK に組み込み |
| [libyuv](https://chromium.googlesource.com/libyuv/libyuv/)（CameraX の `camera-core` の native ライブラリ内） | BSD-3-Clause | **静的**（`camera-core` の native ライブラリ） |
| [LLVM libc++](https://libcxx.llvm.org)（`ANDROID_STL=c++_static`） | Apache-2.0 WITH LLVM-exception | **静的**（native ライブラリ） |
| MediaCodec、AAudio、NDK の media API | Android SDK / NDK | OS コンポーネント |

APK に含まれる Java / Kotlin ライブラリの完全な一覧 — release ビルドの推移的な依存すべてと、
そのバージョン・ライセンス・プロジェクト・作者・同梱の NOTICE ファイル、そして libyuv の
ライセンス本文 — は [`licenses/android-libraries.txt`](licenses/android-libraries.txt) にあり、
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt) と合わせて読みます。これは
[`scripts/android-library-notices.py`](scripts/android-library-notices.py) が Gradle の
`releaseRuntimeClasspath` から生成するので、Android の依存が変わるたびに再実行してください。

## QUIC transport（すべての app）

| コンポーネント | License | link |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **静的** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/)（quiche が `boring-sys` 4.22.0 経由で同梱） | OpenSSL License AND SSLeay AND ISC AND MIT | **静的** |
| quiche に組み込まれる Rust crate —— 後述 | 後述 | **静的** |
| [Rust 標準ライブラリ](https://github.com/rust-lang/rust) | MIT OR Apache-2.0 | **静的** |

Deskhub の暗号化 transport は Cloudflare quiche を組み込んでいる。
[`scripts/build-quiche.sh`](scripts/build-quiche.sh) がバージョンと commit を固定し、
ビルド時に [`patches/quiche-key-exporter.patch`](patches/quiche-key-exporter.patch) を
適用して TLS キー材料を公開し、RustCrypto `ssh-key` で OpenSSH 秘密鍵を解析する。
quiche は BoringSSL（`boring` crate 経由）を同梱する。これらのライブラリはすべての
app と command line client に静的に link される。ライセンス本文:
[`licenses/BSD-2-Clause-quiche.txt`](licenses/BSD-2-Clause-quiche.txt)、
[`licenses/BoringSSL.txt`](licenses/BoringSSL.txt)、そして Rust crate については
[`licenses/rust-crates.txt`](licenses/rust-crates.txt) と
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt)。

BoringSSL は OpenSSL のフォークで、その OpenSSL ライセンスと SSLeay ライセンスは次の謝辞
を求めている（英語原文のまま記載する）。

- This product includes software developed by the OpenSSL Project for use in the OpenSSL
  Toolkit (<http://www.openssl.org/>).
- This product includes cryptographic software written by Eric Young (eay@cryptsoft.com).
  This product includes software written by Tim Hudson (tjh@cryptsoft.com).

### Rust crate

次の表は、パッチ適用後の quiche の `Cargo.lock` から、Deskhub が build するすべての
target について `cargo tree -e normal -p quiche --features ffi` が解決する crate だ。
手続き型マクロ（`enum_dispatch`、`openssl-macros`、`foreign-types-macros`、
`curve25519-dalek-derive`、および `syn`、`quote`、`proc-macro2`、`unicode-ident`）は
コンパイル時にだけ動き、link されない。

| License | Crate |
| --- | --- |
| BSD-2-Clause | `quiche` 0.29.3、`octets` 0.3.6 |
| BSD-3-Clause | `curve25519-dalek` 4.1.3、`ed25519-dalek` 2.2.0、`subtle` 2.6.1 |
| Apache-2.0 | `boring` 4.22.0 |
| MIT | `boring-sys` 4.22.0、`bytes`、`debug_panic`、`generic-array`、`libm`、`memoffset`、`slab` |
| Unlicense OR MIT | `byteorder` |
| MIT OR Apache-2.0 | `ssh-key` 0.6.7、`ssh-cipher`、`ssh-encoding`、`ed25519`、`p256`、`ecdsa`、`elliptic-curve`、`primeorder`、`sec1`、`rfc6979`、`crypto-bigint`、`ff`、`group`、`signature`、`aes`、`aes-gcm`、`aead`、`ghash`、`polyval`、`universal-hash`、`chacha20`、`poly1305`、`cipher`、`ctr`、`cbc`、`inout`、`block-padding`、`bcrypt-pbkdf`、`blowfish`、`pbkdf2`、`hmac`、`sha2`、`digest`、`block-buffer`、`crypto-common`、`cpufeatures`、`opaque-debug`、`der`、`spki`、`pem-rfc7468`、`base16ct`、`base64ct`、`const-oid`、`rand_core`、`getrandom`、`zeroize`、`typenum`、`bitflags`、`cfg-if`、`either`、`foreign-types`、`foreign-types-shared`、`intrusive-collections`、`libc`、`log`、`smallvec`。Windows のみ: `windows-sys`、`windows-targets`、`windows_x86_64_msvc` |

BSD-3-Clause の crate の著作権は、© 2016-2021 isis agora lovecruft と Henry de Valence
（`curve25519-dalek`。Adam Langley の Go 版 ed25519 から派生したコードも含む）、
© 2017-2019 isis agora lovecruft（`ed25519-dalek`）、© 2016-2024 Isis Agora Lovecruft
と Henry de Valence（`subtle`）。各 crate の公開 source からコピーした表示の全文は
[`licenses/rust-crates.txt`](licenses/rust-crates.txt) にあり、quiche の pin が変わると
[`scripts/rust-crate-notices.py`](scripts/rust-crate-notices.py) が quiche の source
ツリーからこれを再生成する。

## Audio codec（すべての app）

| コンポーネント | License | link |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **静的** |

Deskhub の音声 stream は libopus を組み込んでいる。
[`scripts/build-opus.sh`](scripts/build-opus.sh) が正確なバージョンと checksum に固定
した、改変していない上流の source から build し、すべての app と command line client に
静的に link している。固定した同じ source を target ごとに build するので、6 つの
プログラム —— 5 つの app と command line client —— はすべて同じ encoder と decoder を
動かす。このライブラリは改変していない。
ライセンス本文: [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt)。

## 特許

H.264/AVC は [Via LA](https://www.via-la.com/) を通じてライセンスされる特許の対象だ。
特許の権利は上記の著作権 license とは別であり、MIT License によって付与されるもので
はない。Windows、macOS、iOS、Android では、encode と decode は OS 自身の codec が行う。
Linux では、encode は GPU ベンダーの VA-API driver か NVIDIA の NVENC driver が行う。
Linux の decode は Deskhub が同梱する FFmpeg の H.264 decoder を通る。VA-API があれば
GPU がデコードし、なければ FFmpeg がソフトウェアでデコードする。この FFmpeg の decoder
が、Deskhub が出荷する唯一の H.264 実装だ。

音声の codec である Opus も同梱している。Opus はロイヤリティフリーの codec として公開
されており、それを覆う特許ライセンスもロイヤリティフリーの条件で付与されている ——
宣言は [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) と
<https://opus-codec.org/license/> に列挙されている。
