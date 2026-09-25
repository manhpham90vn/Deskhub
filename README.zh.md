[English](README.md) · [Tiếng Việt](README.vi.md) · **中文** · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### 你的机器，出现在你每一块屏幕上。

**Open-source、native、跨平台。使用体验接近本机的 remote desktop —— 快且直接，足以用于
远程游戏，这是普通远程桌面工具无法做到的。**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/runs%20on-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-平台)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[安装](#install)** · [从 source 构建](docs/BUILD.zh.md) ·
[Spec](docs/SPECIFICATION.zh.md) · [Architecture](docs/ARCHITECTURE.zh.md) ·
[Security](SECURITY.zh.md)

</div>

**目录:**

- [安装](#install)
- [演示](#demo)
- [关于](#about)
- [为什么](#why)
- [平台](#platforms)
- [里面有什么](#features)
- [文档](#docs)
- [许可证](#license)

<a id="install"></a>

## 📦 安装

通过 package manager 安装，并由它保持更新 —— Windows、macOS，以及 Ubuntu / Debian / Mint：

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

在 Ubuntu / Debian / Mint 上，`deskhub` package 同时安装 app 与 `deskhub-cli`：

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

其余平台请从 [Releases](https://github.com/manhpham90vn/Deskhub/releases) 获取：

- **Fedora / openSUSE** —— `sudo dnf install ./deskhub-v*-x86_64.rpm`（或 `zypper install`）
- **Arch 及其他 Linux** —— 免安装的 `deskhub-v*-linux-x86_64`，`chmod +x` 后运行
- **Android** —— `deskhub-v*-android.apk`，或 [Play beta](https://play.google.com/apps/testing/com.manhpham.deskhub)
- **iOS** —— [TestFlight](https://testflight.apple.com/join/7qY7wgpd)

各平台的详细说明与所需权限：[INSTALL.zh.md](docs/INSTALL.zh.md)。

<a id="demo"></a>

## 👀 演示

<div align="center">

<img src="docs/imgs/macos_1.png" alt="macOS 上 Deskhub 的 Host 页：Share on network 选择框、供其他机器 connect 的 Wi-Fi 与 Tailscale 地址、UDP port 47777 上的 Not sharing 横幅，以及 Start sharing 按钮上方已勾选 Terminal 的 source 列表" width="850">

<sub>开始共享前的 macOS host：选择允许离开本机的内容 —— 任意 display、shell，或两者 —— 然后按 <b>Start sharing</b>。</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="macOS 上 Deskhub 的 Client 页：host IP、UDP port、passcode 与本机名称输入框，用于选择 remote desktop、control 和 terminal 的勾选框，Connect 按钮，以及带 status、ping 和上次连接时间的设备表">
      <br><sub><b>Client</b> —— 输入 IP，或选择 scan 发现的机器，然后选择要打开的内容：屏幕、control 权限、shell，或其组合。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="macOS 上 Deskhub 的 Devices 页：已 pair 的机器及其 key、pair 时间和最后出现时间，Forget 与 Forget every machine 按钮，控制是否接受新 pair 的开关，以及本机的 SHA256 key">
      <br><sub><b>Devices</b> —— 列出所有被接受过的机器及其名称和 key，可逐个撤销。所需机器 pair 完成后，可关闭新的 pair。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="macOS 上 Deskhub 的 Settings 页：fps、bitrate 与 quality，UDP port，用于 pair 的 passcode，决定 viewer 能否 control 本机的开关，clipboard 与防休眠开关，Screen Recording 和 Accessibility permission 的当前状态，以及开机自启开关">
      <br><sub><b>Settings</b> —— fps、bitrate、quality、port、passcode、是否允许 viewer control 本机，以及各项 macOS permission 的当前状态。</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="iOS 上 Deskhub 的 Client 页：IP、port、passcode 和名称输入框，Connect 与 Terminal 按钮，control 远端机器的开关，以及报告已检查地址数量的 scan" width="195">
  <img src="docs/imgs/ios_2.png" alt="iOS 上 Deskhub 的 Host 页：用于 pair 的 passcode、Share on network、Start sharing，以及供其他机器 connect 的 IP 地址" width="195">
  <img src="docs/imgs/ios_3.png" alt="iOS 上 Deskhub 的 Devices 页：尚为空的已 pair 机器列表、允许新机器 pair 的开关，以及本设备的 SHA256 key" width="195">
  <img src="docs/imgs/ios_4.png" alt="iOS 上 Deskhub 的连接 settings 页：scan 检查的 UDP port，以及 clipboard 同步和保持唤醒两个开关" width="195">
</p>
<p align="center"><sub><b>iPhone</b> —— 同样的四个页面。执行 scan，选中一台机器，将画面作为 trackpad 进行操作；也可以 host 手机自身的屏幕，仅限 view-only。</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Android 上 Deskhub 的 Client 页：IP、port、passcode 和名称输入框，Connect 与 Terminal 按钮，control 勾选框，以及正在遍历 subnet 的 scan" width="195">
  <img src="docs/imgs/android_2.png" alt="Android 上 Deskhub 的 Host 页：用于 pair 的 passcode、Share on network、Start sharing，以及供其他机器 connect 的 IP 地址" width="195">
  <img src="docs/imgs/android_3.png" alt="Android 上 Deskhub 的 Devices 页：尚为空的已 pair 机器列表、允许新机器 pair 的勾选框，以及本设备的 SHA256 key" width="195">
  <img src="docs/imgs/android_4.png" alt="Android 上 Deskhub 的连接 settings 页：scan 检查的 UDP port，以及 clipboard 同步和保持唤醒两个勾选框" width="195">
</p>
<p align="center"><sub><b>Android</b> —— 同样的四个页面，采用 Material Design。作为 host 时，Android 10+ 仅支持 view-only 的屏幕共享。</sub></p>

<a id="about"></a>

## 📖 关于

一份 **C++20 core** 运行于所有平台，从 Windows 到 iPhone，无需重写 protocol。共享一块
display，在另一台机器上输入 IP，即可对其进行操作。每个平台都是相同的四个页面 ——
**Host**、**Client**、**Devices**、**Settings** —— 因此在 macOS 上熟悉之后即可直接使用
Android 上的 app。

| ⚡ 快 | 📦 一个文件 | 🎛️ 简单 |
| ------ | ---------- | --------- |
| 从 capture 到显示 **~3.5 ms**，60 fps。zero-copy pipeline 全程位于 VRAM 内，hot path 不经过 CPU。 | 没有 installer，没有 background service，无需账号。整个 Windows app 为一个 **~5.1 MB** 的 exe；macOS 为 **1.9 MB** 的 dmg。 | **Share** 一块 display，或 **Connect** 到一个 IP。桌面端还可共享一个 **shell**，并接收 viewer 发送的**文件**。手机也可以做 host，但仅限 view-only，因为没有任何移动 OS 允许 app inject input。 |

Session 在 **QUIC/TLS** 上以 end-to-end 方式 encrypt。陌生机器只有在证明自己知道 host
的 passcode 时才被接受 —— 通过 **SPAKE2** 完成，因此 passcode 本身不会被传输 —— 或者由
host 前的用户批准。即便如此，这仍然是一个开放 port 上的短密钥：请使用可信的 network 或
VPN，并且**不要对 UDP 47777 做 port-forward**。完整的 threat model 见
[`SECURITY.zh.md`](SECURITY.zh.md)。

<a id="why"></a>

## 💡 为什么

- 💻 **工作** —— 用配置较低的笔记本或 iPad，运行家中 PC 上的 Claude Code、VS Code 或 build。
- 🌐 **任何用途** —— 从任意设备操作 Chrome、Office 或仅在 PC 上提供的软件。
- 🎮 **游戏** —— 60 fps，relative mouse 与 DirectInput scancode，`F9` 触发 pointer lock。
- 🖥️ **多 display** —— 共享一块或多块 display，每块各自构成一个 session。

<a id="platforms"></a>

## 🚦 平台

| 平台 | Host | Client | 状态 |
| -------- | :--: | :----: | ------ |
| **Windows** | ✅ | ✅ | reference 实现 —— 日常通过 LAN 与 Tailscale（Internet/NAT）使用 |
| **macOS** | ✅ | ✅ | 两个角色均可运行（ScreenCaptureKit + VideoToolbox + CGEvent） |
| **Android** | ✅ | ✅ | Client：video 与 input（trackpad、keyboard）。Host：view-only 屏幕共享（MediaProjection + MediaCodec），Android 10+ —— 正在 Google Play 上测试 |
| **iOS** | ✅ | ✅ | Client：video 与 input（trackpad、keyboard）。Host：通过 Broadcast Upload Extension 实现 view-only 屏幕共享（ReplayKit + VideoToolbox）—— 正在 TestFlight 上测试 |
| **Linux** | ✅ | ✅ | 两个角色均可运行（PipeWire + VA-API + uinput + GTK3）—— 支持 Ubuntu、Debian、Mint、Fedora、openSUSE、Arch，通过 deb / rpm / 免安装 binary 提供；已在两台机器间通过 LAN 验证 |

<a id="features"></a>

## ✨ 里面有什么

- **全程 zero-copy** —— capture 直接进入 VRAM → NVENC → hardware decode → render。hot path 不经过 CPU。
- **基于 QUIC 的专用 protocol** —— 无限 GOP 配合按需 IDR、XOR FEC、adaptive bitrate，全部 multiplex 在一条已 encrypt 的 connection 上。
- **画面附带声音** —— 机器自身的 audio mix，Opus 64 kbps，每个 datagram 承载一个 20 ms frame。丢失一个 packet 仅损失零点几秒，且不影响画面。不会 capture microphone。
- **真实 input** —— relative mouse（Raw Input）以及面向 DirectInput 游戏的 scancode。host 本机的 mouse 和 keyboard 始终优先。
- **共享的 core** —— protocol、FEC 和 bitrate control 位于 `core/`，编译进每一个 client。
- **提供 command line** —— `deskhub-cli` 可共享屏幕、打开 remote shell，并从脚本或通过 SSH 操作 host，不需要 GUI toolkit。见 [Build](docs/BUILD.zh.md#command-line-client)。
- **经过充分测试** —— core 具备离线运行的 unit test。CI 另外运行 ASan、UBSan 和 TSan。七个 libFuzzer target 每晚检查 wire format、H.264 parse、reassembly、terminal 的 byte stream、UI 文案以及各个 session state machine。每个发现的 crash 都会补充为一个 regression test。

<a id="docs"></a>

## 📚 文档

每份文档均以英文发布，译本置于其旁：越南语 `*.vi.md`、中文 `*.zh.md`、日语 `*.ja.md`。
以英文版为准。

| 文档 | 内容 |
| --- | --- |
| [Install](docs/INSTALL.zh.md) ([en](docs/INSTALL.md) · [vi](docs/INSTALL.vi.md) · [ja](docs/INSTALL.ja.md)) | 在五个平台上分别安装 Deskhub |
| [Build](docs/BUILD.zh.md) ([en](docs/BUILD.md) · [vi](docs/BUILD.vi.md) · [ja](docs/BUILD.ja.md)) | 从 source 编译、测试、打包、发布 |
| [Specification](docs/SPECIFICATION.zh.md) ([en](docs/SPECIFICATION.md) · [vi](docs/SPECIFICATION.vi.md) · [ja](docs/SPECIFICATION.ja.md)) | Deskhub 做什么，不涉及实现细节 |
| [Architecture](docs/ARCHITECTURE.zh.md) ([en](docs/ARCHITECTURE.md) · [vi](docs/ARCHITECTURE.vi.md) · [ja](docs/ARCHITECTURE.ja.md)) | 分层、thread、wire protocol 与设计决策 |
| [`SECURITY.zh.md`](SECURITY.zh.md) ([en](SECURITY.md) · [vi](SECURITY.vi.md) · [ja](SECURITY.ja.md)) | Threat model 与漏洞报告方式 |
| [`PRIVACY.zh.md`](PRIVACY.zh.md) ([en](PRIVACY.md) · [vi](PRIVACY.vi.md) · [ja](PRIVACY.ja.md)) | 隐私政策 |
| [`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md) ([en](THIRD_PARTY_NOTICES.md) · [vi](THIRD_PARTY_NOTICES.vi.md) · [ja](THIRD_PARTY_NOTICES.ja.md)) | 第三方组件与 license |

问题反馈：[issues](https://github.com/manhpham90vn/Deskhub/issues) —— 请附上设备型号。

<a id="license"></a>

## 📄 许可证

MIT —— 见 [`LICENSE`](LICENSE)。第三方组件及其声明（包括 Linux app 中静态 link 的 LGPL
版 FFmpeg）列于
[`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md)。
