**English** · [Tiếng Việt](THIRD_PARTY_NOTICES.vi.md)

# Third-party notices

Deskhub itself is distributed under the MIT License — see [`LICENSE`](LICENSE).

This file lists the third-party components Deskhub links against, and the obligations
that come with them. Nothing here is licensed under the GPL, and no component restricts
redistribution of Deskhub under the MIT License.

## Linux app (`client/linux`)

| Component | License | Linkage |
| --- | --- | --- |
| [FFmpeg](https://ffmpeg.org) 8.0 — `libavcodec`, `libavutil` | LGPL-2.1-or-later | **static** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (NVENC SDK 13.0 headers) | MIT | headers only |
| [GTK](https://www.gtk.org) 3 | LGPL-2.1-or-later | dynamic |
| [PipeWire](https://pipewire.org) 0.3 | MIT | dynamic |
| [libva](https://github.com/intel/libva) / `libva-drm` | MIT | dynamic |
| [libdrm](https://gitlab.freedesktop.org/mesa/drm) | MIT | dynamic |
| [libepoxy](https://github.com/anholt/libepoxy) | MIT | dynamic |
| EGL ([libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd) / Mesa) | MIT | dynamic |
| [libayatana-appindicator](https://github.com/AyatanaIndicators/libayatana-appindicator) 3 | LGPL-3.0 / GPL-3.0 | dynamic, optional (tray icon) |

### FFmpeg (LGPL-2.1-or-later, statically linked)

The Linux app is the only target that bundles FFmpeg. It is built from unmodified
upstream FFmpeg 8.0 sources by [`scripts/build-ffmpeg.sh`](scripts/build-ffmpeg.sh),
configured **without** `--enable-gpl` and **without** `--enable-nonfree`, with only the
native H.264 decoder and the VA-API hwaccel enabled. The resulting `libavcodec` and
`libavutil` are therefore covered by the GNU Lesser General Public License, version 2.1
or later — full text in [`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt).

Because these libraries are linked statically, LGPL-2.1 §6 requires that recipients be
able to relink the application against a modified version of FFmpeg. Deskhub satisfies
this by shipping complete source: the application sources are in this repository under
the MIT License, and `scripts/build-ffmpeg.sh` reproduces the exact FFmpeg build. Anyone
can substitute their own FFmpeg and rebuild with `make`.

FFmpeg is not modified in any way. Upstream sources: <https://ffmpeg.org/download.html>.

`third_party/nvenc-13.0` is a git submodule containing only the NVIDIA Video Codec SDK
API headers as redistributed by the FFmpeg project under the MIT License. The NVENC
implementation itself lives in the user's NVIDIA driver (`libnvidia-encode.so.1`,
`libcuda.so.1`) and is resolved at runtime; it is not bundled.

### GTK 3 (LGPL-2.1-or-later, dynamically linked)

Linked dynamically against the distribution-provided shared libraries. No GTK source is
bundled or modified; users may replace the system libraries freely.

## Command-line client (`client/cli`)

The command-line client links the same libraries as the desktop app of the system it is
built for, minus the GUI toolkit — no GTK on Linux, no wxWidgets on Windows. It draws a
remote screen in a window of its own, which on Linux adds two libraries the app does not
need:

| Component | License | Linkage |
| --- | --- | --- |
| [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11) | MIT | dynamic (Linux) |
| [libXfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes) | MIT | dynamic (Linux) |

On Windows it reuses the app's own Win32 viewer window, so it adds nothing. On macOS it
uses AppKit and ScreenCaptureKit from the Apple SDK, the same as the app.

## Windows app (`client/windows`)

| Component | License | Linkage |
| --- | --- | --- |
| [wxWidgets](https://www.wxwidgets.org) 3.3 | wxWindows Library Licence 3.1 | **static** |
| [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) (NVENC SDK 13.0 headers) | MIT | headers only |
| Media Foundation, Direct3D 11, DXGI | Microsoft Windows SDK | OS component |

### wxWidgets (wxWindows Library Licence, statically linked)

The Windows app builds unmodified upstream wxWidgets from the official release archive
(fetched by CMake at configure time, see `client/windows/win32/CMakeLists.txt`) and
links it statically into `Deskhub.exe`. The wxWindows Library Licence is the LGPL plus
an exception that explicitly permits distributing binaries linked against the library —
statically or dynamically — under the distributor's own terms, so the single-file MIT
distribution of Deskhub is unaffected. Licence text:
<https://www.wxwidgets.org/about/licence/>.

`third_party/nvenc-13.0` is a git submodule containing only the NVIDIA Video Codec SDK
API headers as redistributed by the FFmpeg project under the MIT License. The NVENC
implementation itself lives in the user's NVIDIA driver (`nvEncodeAPI64.dll`) and is
resolved at runtime; it is not bundled.

## Apple apps (`client/macos`, `client/ios`)

| Component | License | Linkage |
| --- | --- | --- |
| SwiftUI, AppKit, ScreenCaptureKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, CoreGraphics, ApplicationServices, ServiceManagement | Apple SDK | OS component (macOS) |
| SwiftUI, UIKit, ReplayKit, VideoToolbox, AVFoundation, CoreMedia, CoreVideo, Accelerate (vImage), Photos, UserNotifications | Apple SDK | OS component (iOS) |

## Android app (`client/android`)

| Component | License | Linkage |
| --- | --- | --- |
| [AndroidX](https://developer.android.com/jetpack/androidx) — Core KTX, Activity, Compose UI, Material 3 | Apache-2.0 | dynamic |
| [Kotlin](https://kotlinlang.org) standard library | Apache-2.0 | dynamic |
| MediaCodec, NDK media APIs | Android SDK / NDK | OS component |

## QUIC transport (all apps)

| Component | License | Linkage |
| --- | --- | --- |
| [quiche](https://github.com/cloudflare/quiche) 0.29.3 | BSD-2-Clause | **static** |
| [BoringSSL](https://boringssl.googlesource.com/boringssl/) (bundled by quiche) | OpenSSL / ISC-style | **static** |

Deskhub's encrypted transport embeds Cloudflare's quiche, built from unmodified
upstream sources pinned to an exact version and commit by
[`scripts/build-quiche.sh`](scripts/build-quiche.sh). quiche carries its own copy of
BoringSSL (via the `boring` crate), which provides TLS and the cryptography behind
Deskhub's pairing. Both are linked statically into every app. Neither library is
modified.

## Audio codec (all apps)

| Component | License | Linkage |
| --- | --- | --- |
| [libopus](https://opus-codec.org) 1.5.2 | BSD-3-Clause | **static** |

Deskhub's audio streaming embeds libopus, built from unmodified upstream sources pinned
to an exact version and checksum by [`scripts/build-opus.sh`](scripts/build-opus.sh) and
linked statically into every app. One build serves all five platforms: the same encoder
runs on the sharing machine and the same decoder on every viewer. The library is not
modified. Licence text: [`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt).

## Patents

H.264/AVC is covered by patents licensed through [Via LA](https://www.via-la.com/).
Patent rights are separate from the copyright licenses above and are not granted by the
MIT License. On Windows, macOS, iOS, and Android, encoding and decoding are performed by
the operating system's own codecs. On Linux, they are performed by the GPU vendor's
VA-API driver. Deskhub ships no video codec implementation of its own.

Opus, the audio codec, is the exception: libopus is bundled. Opus is published as a
royalty-free codec, and the patent licences that cover it are granted on royalty-free
terms — the declarations are listed in
[`licenses/BSD-3-Clause-opus.txt`](licenses/BSD-3-Clause-opus.txt) and at
<https://opus-codec.org/license/>.
