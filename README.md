**English** · [Tiếng Việt](README.vi.md) · [中文](README.zh.md) · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### Your computer, wherever you need it.

**Deskhub lets you view and control your computer from another device. It is open source,
runs natively across five platforms, and is built for responsive work and play.**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/runs%20on-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-platforms)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[Install](#install)** · [Build from source](docs/BUILD.md) ·
[Spec](docs/SPECIFICATION.md) · [Architecture](docs/ARCHITECTURE.md) ·
[Security](SECURITY.md)

</div>

**Contents:**

- [Install](#install)
- [Demo](#demo)
- [About](#about)
- [Why](#why)
- [Platforms](#platforms)
- [What's inside](#features)
- [Docs](#docs)
- [License](#license)

<a id="install"></a>

## 📦 Install

Install through a package manager on Windows, macOS, Ubuntu, Debian or Mint:

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

On Ubuntu / Debian / Mint, the desktop app and CLI are separate packages. Add the apt
repository once, then install either or both:

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update
sudo apt install deskhub       # desktop app
sudo apt install deskhub-cli   # CLI; also works on its own
```

The desktop package adds DeskHub to the application menu and installs `deskhub` in
`/usr/bin`; the CLI package installs `deskhub-cli` in `/usr/bin`. Windows setup adds
Start Menu and Desktop shortcuts; the separate CLI setup adds its command to the user
`PATH`. Homebrew places the macOS CLI on `PATH`.

Everything else is on [Releases](https://github.com/manhpham90vn/Deskhub/releases):

- **Windows downloads** — app setup `deskhub-v*-windows-setup.exe` and CLI setup
  `deskhub-cli-v*-windows-setup.exe`; portable exe files are also available
- **Fedora / openSUSE** — separate app and CLI RPMs: install
  `deskhub-v*-x86_64.rpm` or `deskhub-cli-v*-x86_64.rpm` with `dnf` or `zypper`
- **Arch, other Linux** — portable `deskhub-v*-linux-x86_64`, just `chmod +x` and run
- **Android** — `deskhub-v*-android.apk`, or the [Play beta](https://play.google.com/apps/testing/com.manhpham.deskhub)
- **iOS** — [TestFlight](https://testflight.apple.com/join/7qY7wgpd)

Details and permissions per platform: [INSTALL.md](docs/INSTALL.md).

<a id="demo"></a>

## 👀 Demo

<div align="center">

<img src="docs/imgs/macos_1.png" alt="Deskhub Host page on macOS: a Share on network picker, the Wi-Fi and Tailscale addresses others connect to, a Not sharing banner on UDP port 47777, and the source list with Terminal ticked above a Start sharing button" width="850">

<sub>A macOS host, one tick away from sharing: pick what leaves this machine — any display, the shell, or both — then press <b>Start sharing</b>.</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="Deskhub Client page on macOS: host IP, UDP port, passcode and your-name fields, tickboxes to pick remote desktop, control and terminal, a Connect button, and a device table with status, ping and last-connected columns">
      <br><sub><b>Client</b> — type an IP or click a machine the scan found, and pick what to open: the screen, control of it, a shell, or any mix.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="Deskhub Devices page on macOS: paired machines with key, paired and last-seen times, Forget and Forget every machine buttons, a switch for new pairings, and this machine's SHA256 key">
      <br><sub><b>Devices</b> — every machine ever let in, by name and key, each revocable; turn new pairings off once yours are listed.</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="Deskhub Settings page on macOS: fps, bitrate and quality, UDP port, pairing passcode, a switch for whether viewers can control this machine, clipboard and stay-awake toggles, the live state of the Screen Recording and Accessibility permissions, and a start-at-login switch">
      <br><sub><b>Settings</b> — fps, bitrate, quality, port, the passcode, whether viewers may control this machine, and the live state of the macOS permissions.</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="Deskhub iOS Client page: IP, port, passcode and name fields, Connect and Terminal buttons, a switch for controlling the remote machine, and the network scan reporting how many addresses it checked" width="195">
  <img src="docs/imgs/ios_2.png" alt="Deskhub iOS Host page: pairing passcode, Share on network, Start sharing, and the IP addresses others use to connect" width="195">
  <img src="docs/imgs/ios_3.png" alt="Deskhub iOS Devices page: an empty list of paired machines, the switch that lets new machines pair, and this device's SHA256 key" width="195">
  <img src="docs/imgs/ios_4.png" alt="Deskhub iOS Connection settings page: the UDP port the scan looks on, plus clipboard sync and keep-awake switches" width="195">
</p>
<p align="center"><sub><b>iPhone</b> — the same four pages. Scan, tap a machine, drive it with the video as a trackpad; or host the phone's own screen, view-only.</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Deskhub Android Client page: IP, port, passcode and name fields, Connect and Terminal buttons, a control tickbox, and the network scan working through the subnet" width="195">
  <img src="docs/imgs/android_2.png" alt="Deskhub Android Host page: pairing passcode, Share on network, Start sharing, and the IP addresses others use to connect" width="195">
  <img src="docs/imgs/android_3.png" alt="Deskhub Android Devices page: an empty list of paired machines, the tickbox that lets new machines pair, and this device's SHA256 key" width="195">
  <img src="docs/imgs/android_4.png" alt="Deskhub Android Connection settings page: the UDP port the scan looks on, plus clipboard sync and keep-awake tickboxes" width="195">
</p>
<p align="center"><sub><b>Android</b> — the same four pages in Material dress. Hosting is a view-only screen share on Android 10+.</sub></p>

<a id="about"></a>

## 📖 About

Share a screen on one device, then connect from another using its IP address. The same
four pages — **Host**, **Client**, **Devices** and **Settings** — appear on every platform,
so the controls stay familiar as you move between a Mac, PC, phone or tablet. A shared
**C++20 core** handles the protocol across all five platforms.

| ⚡ Fast | 📦 Easy install | 🎛️ Simple |
| ------ | ---------- | --------- |
| Streams at 60 fps on supported hardware. The video path uses GPU memory where available. | Install with a package manager or download a release. No account or background service is required. | **Share** a display or **Connect** to an IP. Desktops can also share a **shell** and receive **files**. Phones can share their screens in view-only mode. |

Sessions are encrypted end to end over **QUIC/TLS**, and an unknown machine only gets in
by proving it knows the host's passcode — via **SPAKE2**, so the code itself never travels
— or by being approved at the host. That is still a small secret on an open port: use a
network you trust or a VPN, and **never port-forward UDP 47777**. Full threat model in
[`SECURITY.md`](SECURITY.md).

<a id="why"></a>

## 💡 Why

- 💻 **Work** — run Claude Code, VS Code, or builds on your home PC from a weak laptop or an iPad at a café.
- 🌐 **Desktop apps** — use Chrome, Office, or software that only runs on your computer from another device.
- 🎮 **Games** — 60 fps, relative mouse + DirectInput scancodes, `F9` pointer lock.
- 🖥️ **Multi-monitor** — share one or several displays, each as its own session.

<a id="platforms"></a>

## 🚦 Platforms

| Platform | Host | Client | Status |
| -------- | :--: | :----: | ------ |
| **Windows** | ✅ | ✅ | Reference implementation — daily use over LAN + Tailscale (Internet/NAT) |
| **macOS** | ✅ | ✅ | Both roles working (ScreenCaptureKit + VideoToolbox + CGEvent) |
| **Android** | ✅ | ✅ | Client: video + input (trackpad, keyboard). Host: view-only screen share (MediaProjection + MediaCodec), Android 10+ — testing on Google Play |
| **iOS** | ✅ | ✅ | Client: video + input (trackpad, keyboard). Host: view-only screen share via a Broadcast Upload Extension (ReplayKit + VideoToolbox) — testing via TestFlight |
| **Linux** | ✅ | ✅ | Both roles working (PipeWire + VA-API + uinput + GTK3) — Ubuntu, Debian, Mint, Fedora, openSUSE, Arch via deb / rpm / portable binary; verified between two machines over LAN |

<a id="features"></a>

## ✨ What's inside

- **GPU video path** — capture, encode, decode and render use platform hardware where available; the Windows NVENC path avoids copying frames through the CPU.
- **Purpose-built protocol over QUIC** — infinite GOP + on-demand IDR, XOR FEC, adaptive bitrate, all multiplexed on one encrypted connection.
- **Sound comes with the screen** — the machine's own audio mix, Opus at 64 kbps, one 20 ms frame per datagram; a lost packet costs a fraction of a second and never disturbs the picture. Never a microphone.
- **Real input** — relative mouse (Raw Input) + scancodes for DirectInput games; host's own mouse/keyboard always wins.
- **One shared core** — protocol, FEC, and bitrate control live in `core/`, compiled into every client.
- **Command-line tools** — `deskhub-cli` can share a screen, open a remote shell and run from a script or over SSH. On Windows and Linux, it can also open a remote-screen window. See [Build](docs/BUILD.md#the-command-line-client).
- **Tested across the core** — offline unit tests, sanitizer runs in CI and seven nightly libFuzzer targets cover the wire format, H.264 parsing, reassembly, terminal bytes, UI text and session state machines. Crashes found by fuzzing become regression tests.

<a id="docs"></a>

## 📚 Docs

Every document is published in English with translations beside it — Vietnamese as
`*.vi.md`, Chinese as `*.zh.md`, Japanese as `*.ja.md`. The English text is the
authoritative one.

| Document | Covers |
| --- | --- |
| [Install](docs/INSTALL.md) ([vi](docs/INSTALL.vi.md) · [zh](docs/INSTALL.zh.md) · [ja](docs/INSTALL.ja.md)) | Getting Deskhub onto each of the five platforms |
| [Build](docs/BUILD.md) ([vi](docs/BUILD.vi.md) · [zh](docs/BUILD.zh.md) · [ja](docs/BUILD.ja.md)) | Compiling from source, tests, packaging, releasing |
| [Specification](docs/SPECIFICATION.md) ([vi](docs/SPECIFICATION.vi.md) · [zh](docs/SPECIFICATION.zh.md) · [ja](docs/SPECIFICATION.ja.md)) | What Deskhub does, with no implementation detail |
| [Architecture](docs/ARCHITECTURE.md) ([vi](docs/ARCHITECTURE.vi.md) · [zh](docs/ARCHITECTURE.zh.md) · [ja](docs/ARCHITECTURE.ja.md)) | Layers, threads, wire protocol, decisions |
| [`SECURITY.md`](SECURITY.md) ([vi](SECURITY.vi.md) · [zh](SECURITY.zh.md) · [ja](SECURITY.ja.md)) | Threat model and how to report a vulnerability |
| [`PRIVACY.md`](PRIVACY.md) ([vi](PRIVACY.vi.md) · [zh](PRIVACY.zh.md) · [ja](PRIVACY.ja.md)) | Privacy policy |
| [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) ([vi](THIRD_PARTY_NOTICES.vi.md) · [zh](THIRD_PARTY_NOTICES.zh.md) · [ja](THIRD_PARTY_NOTICES.ja.md)) | Third-party components and licences |

Bugs and feedback: [issues](https://github.com/manhpham90vn/Deskhub/issues) — include your
device model.

<a id="license"></a>

## 📄 License

MIT — see [`LICENSE`](LICENSE). Third-party components and their notices (including the
statically linked LGPL build of FFmpeg in the Linux app) are listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
