[English](README.md) · [Tiếng Việt](README.vi.md) · **中文** · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### 需要时，就能用上自己的电脑。

**Deskhub 让你从另一台设备查看并操作自己的电脑。它开源、在五个平台上原生运行，
兼顾日常工作与远程游戏所需的响应速度。**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/runs%20on-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#platforms)

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

在 Windows、macOS、Ubuntu、Debian 或 Mint 上，可以通过 package manager 安装：

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

在 Ubuntu / Debian / Mint 上，桌面应用与 CLI 使用独立软件包。添加一次 apt repository，
再按需安装其中一个或两个：

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update
sudo apt install deskhub       # 桌面应用
sudo apt install deskhub-cli   # CLI；也可以单独安装
```

桌面软件包会在应用菜单添加 Deskhub，并将 `deskhub` 安装到 `/usr/bin`；CLI 软件包将
`deskhub-cli` 安装到 `/usr/bin`。Windows 的应用安装程序创建开始菜单和桌面快捷方式；
独立的 CLI 安装程序将命令加入用户 `PATH`，而 winget 以便携版 exe 的形式安装 CLI 并放入
`PATH`。Homebrew 会将 macOS CLI 加入 `PATH`。

其余平台请从 [Releases](https://github.com/manhpham90vn/Deskhub/releases) 获取：

- **Windows 下载版** —— 应用安装程序 `deskhub-v*-windows-setup.exe` 与 CLI 安装程序
  `deskhub-cli-v*-windows-setup.exe`；也提供便携版 exe
- **macOS 下载版** —— 应用为 `deskhub-v*-macos.dmg`，CLI 为 `deskhub-cli-v*-macos`
- **不使用 repository 的 Ubuntu / Debian / Mint** —— `deskhub-v*-amd64.deb` 与
  `deskhub-cli-v*-amd64.deb`
- **Fedora / openSUSE** —— 应用与 CLI 使用独立 RPM：按需使用 `dnf` 或 `zypper`
  安装 `deskhub-v*-x86_64.rpm`、`deskhub-cli-v*-x86_64.rpm`
- **Arch 及其他 Linux** —— 免安装的 `deskhub-v*-linux-x86_64` 与
  `deskhub-cli-v*-linux-x86_64`，`chmod +x` 后运行
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
      <img src="docs/imgs/macos_2.png" alt="macOS 上 Deskhub 的 Client 页：host 地址与 UDP port 输入框，用于选择 remote desktop、control 和 terminal 的勾选框，Connect 按钮，以及最近使用的地址列表及各自的上次连接时间">
      <br><sub><b>Client</b> —— 输入地址、粘贴 host 的 QR 链接，或选择最近使用的 host 或受信任的 host，然后选择要打开的内容：屏幕、control 权限、shell，或其组合。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="macOS 上 Deskhub 的 Devices 页：一个区域对应本机作为 host，显示其 SHA256 key fingerprint、Copy public key 按钮和允许 connect 的 client key；另一个区域对应本机作为 client，显示受信任的 host">
      <br><sub><b>Devices</b> —— 作为 host：本机的 key，以及允许接入的设备——经批准、扫码或粘贴加入——可逐个移除。作为 client：你信任的 host，以 key 为准。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="macOS 上 Deskhub 的 Settings 页：设备名称，fps、bitrate 与 quality，UDP port，决定 viewer 能否 control 本机的开关，clipboard 与防休眠开关，Screen Recording 和 Accessibility permission 的当前状态，以及开机自启开关">
      <br><sub><b>Settings</b> —— 设备名称、fps、bitrate、quality、port、是否允许 viewer control 本机，以及各项 macOS permission 的当前状态。</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="iOS 上 Deskhub 的 Client 页：地址和 port 输入框，Connect 与 Terminal 按钮，control 远端机器的开关，以及最近使用的地址列表" width="195">
  <img src="docs/imgs/ios_2.png" alt="iOS 上 Deskhub 的 Host 页：Share on network、Start sharing，以及供其他机器 connect 的 IP 地址" width="195">
  <img src="docs/imgs/ios_3.png" alt="iOS 上 Deskhub 的 Devices 页：本设备的 SHA256 key、允许 connect 的 client key，以及受信任的 host" width="195">
  <img src="docs/imgs/ios_4.png" alt="iOS 上 Deskhub 的连接 settings 页：UDP port，以及 clipboard 同步和保持唤醒两个开关" width="195">
</p>
<p align="center"><sub><b>iPhone</b> —— 同样的四个页面。扫描 host 的 QR code 即可 connect，将画面作为 trackpad 进行操作；也可以 host 手机自身的屏幕，仅限 view-only。</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Android 上 Deskhub 的 Client 页：地址和 port 输入框，Connect 与 Terminal 按钮，control 勾选框，以及最近使用的地址列表" width="195">
  <img src="docs/imgs/android_2.png" alt="Android 上 Deskhub 的 Host 页：Share on network、Start sharing，以及供其他机器 connect 的 IP 地址" width="195">
  <img src="docs/imgs/android_3.png" alt="Android 上 Deskhub 的 Devices 页：本设备的 SHA256 key、允许 connect 的 client key，以及受信任的 host" width="195">
  <img src="docs/imgs/android_4.png" alt="Android 上 Deskhub 的连接 settings 页：UDP port，以及 clipboard 同步和保持唤醒两个勾选框" width="195">
</p>
<p align="center"><sub><b>Android</b> —— 同样的四个页面，采用 Material Design。作为 host 时，Android 10+ 仅支持 view-only 的屏幕共享。</sub></p>

<a id="about"></a>

## 📖 关于

在一台设备上选择要 Share 的 display，再从另一台设备输入 IP 地址并 Connect。
**Host**、**Client**、**Devices** 和 **Settings** 四个页面在各平台保持一致，
从 Mac 换到 PC 或手机时也容易上手。五个平台共用一套 **C++20 core** 来处理 protocol。

| ⚡ 快 | 📦 安装方便 | 🎛️ 简单 |
| ------ | ---------- | --------- |
| 默认以 60 fps stream，在适用的硬件上最高可达 240 fps。video 处理会在可用时使用 GPU 内存。 | 可通过 package manager 安装，也可下载 release。无需账号或 background service。 | **Share** 一块 display，或 **Connect** 到一个 IP。桌面端还可共享 **shell**，并且每个 host —— 包括手机 —— 都能接收**文件**；手机以 view-only 模式共享屏幕。 |

Session 在 **QUIC/TLS** 上以 end-to-end 方式 encrypt，访问方式与 SSH 相同：每台机器只有
一把 key，client 只有在 host 的所有者放行了这把 key 之后才能接入——批准它的 connection
request、在共享时向它展示 **QR code**，或粘贴它的 public key。client 会 pin 住 host 的 key，
并在发送任何内容之前先核对；信任跟随 key 而非地址，因此 host 更换地址后信任依然保留。没有
passcode，没有 discovery，也没有任何放陌生人进来的开关。请使用可信的 network 或 VPN，并且
**不要对 UDP 47777 做 port-forward**。完整的 threat model 见
[`SECURITY.zh.md`](SECURITY.zh.md)。

<a id="why"></a>

## 💡 为什么

- 💻 **工作** —— 用配置较低的笔记本或 iPad，运行家中 PC 上的 Claude Code、VS Code 或 build。
- 🌐 **桌面应用** —— 从另一台设备使用电脑上的 Chrome、Office 或其他桌面软件。
- 🎮 **游戏** —— 最高 240 fps，relative mouse 与 DirectInput scancode，`F9` 触发 pointer lock。
- 🖥️ **多 display** —— 共享一块或多块 display，每块各自构成一个 session。

<a id="platforms"></a>

## 🚦 平台

| 平台 | Host | Client | 状态 |
| -------- | :--: | :----: | ------ |
| **Windows** | ✅ | ✅ | reference 实现 —— 日常通过 LAN 与 Tailscale（Internet/NAT）使用 |
| **macOS** | ✅ | ✅ | 两个角色均可运行（ScreenCaptureKit + VideoToolbox + CGEvent） |
| **Android** | ✅ | ✅ | Client：video 与 input（trackpad、keyboard）。Host：view-only 屏幕共享（MediaProjection + MediaCodec），Android 10+ —— 正在 Google Play 上测试 |
| **iOS** | ✅ | ✅ | Client：video 与 input（trackpad、keyboard）。Host：通过 Broadcast Upload Extension 实现 view-only 屏幕共享（ReplayKit + VideoToolbox）—— 正在 TestFlight 上测试 |
| **Linux** | ✅ | ✅ | 两个角色均可运行（PipeWire + VA-API/NVENC + uinput + GTK3）—— 支持 Ubuntu、Debian、Mint、Fedora、openSUSE、Arch，通过 deb / rpm / 免安装 binary 提供；已在两台机器间通过 LAN 验证 |

<a id="features"></a>

## ✨ 里面有什么

- **GPU video 路径** —— capture、encode、decode 和 render 会在可用时使用平台硬件；Windows 的 NVENC 路径避免让 frame 经由 CPU 复制，Linux host 则通过 VA-API 编码，在 NVIDIA GPU 上改用 NVENC。
- **基于 QUIC 的专用 protocol** —— 无限 GOP 配合按需 IDR、XOR FEC 加丢失 packet 的重传、adaptive bitrate，全部 multiplex 在一条已 encrypt 的 connection 上。
- **画面附带声音** —— 机器自身的 audio mix，Opus 64 kbps，每个 datagram 承载一个 20 ms frame。单个丢失的 packet 通常可通过 Opus 的 in-band FEC 由下一个 packet 恢复，更长的中断损失零点几秒，两者都不影响画面。不会 capture microphone。
- **不止是屏幕** —— remote shell、向任意 host 发送文件、双向同步纯文本 clipboard，以及在 session 期间保持唤醒。
- **真实 input** —— relative mouse（Raw Input）以及面向 DirectInput 游戏的 scancode。host 本机的 mouse 和 keyboard 始终优先（在 Linux 上需要用户加入 `input` 组 —— 见 [Install](docs/INSTALL.zh.md#host-wins)）。
- **共享的 core** —— protocol、FEC 和 bitrate control 位于 `core/`，编译进每一个 client。
- **command line 工具** —— `deskhub-cli` 可共享屏幕、打开 remote shell、发送文件，并管理 key、允许的设备、受信任的 host 和 settings，可从脚本或 SSH 使用；列表类命令支持 `--json`。在 Windows 和 Linux 上，它还能打开远程屏幕窗口。见 [Build](docs/BUILD.zh.md#command-line-client)。
- **覆盖核心功能的测试** —— core 有离线 unit test，CI 运行 ASan、UBSan 和 TSan。九个 libFuzzer target —— 每个 pull request 上各运行 30 秒，每晚各运行 15 分钟 —— 检查 wire format、H.264 parse、reassembly、terminal 的 byte stream、UI 文案、key 与邀请链接的 parse、QR 编码和 session state machine。发现的 crash 会加入 regression test。

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

MIT —— 见 [`LICENSE`](LICENSE)。第三方组件及其声明（包括 Linux app 与 CLI 中静态 link 的
LGPL 版 FFmpeg）列于
[`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md)。
