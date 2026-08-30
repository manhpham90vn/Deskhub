[English](README.md) · [Tiếng Việt](README.vi.md) · **中文** · [日本語](README.ja.md)

<div align="center">

# 🖥️ Deskhub

### 你的电脑，出现在你拥有的每一块屏幕上。

**开源、原生、跨平台。像在本地一样流畅的远程桌面 —— 快到、直接到足以真正远程打游戏，
这是普通远程桌面工具做不到的。**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/runs%20on-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-平台)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[安装](docs/INSTALL.zh.md)** · [从源码构建](docs/BUILD.zh.md) ·
[规格说明](docs/SPECIFICATION.zh.md) · [架构](docs/ARCHITECTURE.zh.md) ·
[安全](SECURITY.zh.md)

</div>

## 👀 演示

<div align="center">

<img src="docs/imgs/macos_1.png" alt="macOS 上的 Deskhub 主机页：网络共享选择器、别人用来连接的 Wi-Fi 与 Tailscale 地址、UDP 47777 端口上的未共享横幅，以及勾选了终端的来源列表和开始共享按钮" width="850">

<sub>一台 macOS 主机，离共享只差一个勾选：选好哪些内容离开这台机器 —— 任意显示器、shell，或者两者都要 —— 然后按 <b>Start sharing</b>。</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="macOS 上的 Deskhub 客户端页：主机 IP、UDP 端口、通行码与自己名字的输入框，用于选择远程桌面、控制和终端的勾选框，一个连接按钮，以及带状态、延迟和上次连接时间的设备表">
      <br><sub><b>客户端</b> —— 输入 IP，或点一下扫描到的机器，再选择要打开什么：画面、对它的控制、shell，或任意组合。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="macOS 上的 Deskhub 设备页：已配对机器及其密钥、配对时间与最后出现时间，忘记与忘记所有机器按钮，新配对开关，以及本机的 SHA256 密钥">
      <br><sub><b>设备</b> —— 每一台曾被放进来的机器，按名称和密钥列出，随时可撤销；自己的机器都在列表里之后，就把新配对关掉。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="macOS 上的 Deskhub 设置页：帧率、码率与画质，UDP 端口，配对通行码，观看者能否控制本机的开关，剪贴板与防休眠开关，屏幕录制与辅助功能权限的实时状态，以及开机自启开关">
      <br><sub><b>设置</b> —— 帧率、码率、画质、端口、通行码、观看者是否可以控制本机，以及 macOS 各项权限的实时状态。</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="Deskhub iOS 客户端页：IP、端口、通行码与名字输入框，连接和终端按钮，控制远程机器的开关，以及报告已检查多少个地址的网络扫描" width="195">
  <img src="docs/imgs/ios_2.png" alt="Deskhub iOS 主机页：配对通行码、网络共享、开始共享，以及别人用来连接的 IP 地址" width="195">
  <img src="docs/imgs/ios_3.png" alt="Deskhub iOS 设备页：空的已配对机器列表、允许新机器配对的开关，以及本设备的 SHA256 密钥" width="195">
  <img src="docs/imgs/ios_4.png" alt="Deskhub iOS 连接设置页：扫描所用的 UDP 端口，以及剪贴板同步和防休眠开关" width="195">
</p>
<p align="center"><sub><b>iPhone</b> —— 同样的四个页面。扫描、点一台机器，把画面当触控板来操控它；也可以把手机自己的屏幕共享出去，只能看。</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Deskhub Android 客户端页：IP、端口、通行码与名字输入框，连接和终端按钮，控制勾选框，以及正在遍历子网的网络扫描" width="195">
  <img src="docs/imgs/android_2.png" alt="Deskhub Android 主机页：配对通行码、网络共享、开始共享，以及别人用来连接的 IP 地址" width="195">
  <img src="docs/imgs/android_3.png" alt="Deskhub Android 设备页：空的已配对机器列表、允许新机器配对的勾选框，以及本设备的 SHA256 密钥" width="195">
  <img src="docs/imgs/android_4.png" alt="Deskhub Android 连接设置页：扫描所用的 UDP 端口，以及剪贴板同步和防休眠勾选框" width="195">
</p>
<p align="center"><sub><b>Android</b> —— 同样的四个页面，换成 Material 外观。在 Android 10+ 上，做主机就是只能看的屏幕共享。</sub></p>

## 📖 关于

一套 **C++20 内核**跑遍所有地方 —— 从 Windows 到 iPhone —— 协议一行都不用重写。
共享一块屏幕，在另一台机器上输入 IP，你就已经在操控它了。每个平台都是四个页面 ——
**Host**、**Client**、**Devices**、**Settings** —— 所以在 Mac 上学会了，也就等于学会了
Android 版。

| ⚡ 快 | 📦 单文件 | 🎛️ 简单 |
| ------ | ---------- | --------- |
| 采集→显示 **约 3.5 毫秒**，60 fps。零拷贝 VRAM 流水线 —— 热路径全程不碰 CPU。 | 没有安装程序，没有后台服务，不用账号。整个 Windows 应用就是一个 **约 5.1 MB** 的 exe；macOS 是一个 **1.9 MB** 的 dmg。 | **共享**一块屏幕，或者**连接**到一个 IP，就这样。桌面端还能共享一个 **shell**，并接收观看者发来的**文件**。手机也能做主机，但只能看，因为没有哪个移动系统允许应用注入输入。 |

会话通过 **QUIC/TLS** 端到端加密，一台陌生机器要进来，只能靠证明自己知道主机的通行码 ——
通过 **SPAKE2**，所以通行码本身从不上网 —— 或者在主机上被手动批准。即便如此，那仍然是一个
开放端口上的小秘密：请使用你信任的网络或 VPN，并且**永远不要把 UDP 47777 做端口转发**。
完整威胁模型见 [`SECURITY.zh.md`](SECURITY.zh.md)。

## 💡 为什么

- 💻 **工作** —— 用一台性能孱弱的笔记本，或者咖啡馆里的 iPad，跑家里电脑上的 Claude Code、VS Code 或编译任务。
- 🌐 **什么都行** —— 从任意设备操作 Chrome、Office，或只有 PC 版的软件。
- 🎮 **游戏** —— 60 fps，相对鼠标 + DirectInput 扫描码，`F9` 锁定指针。
- 🖥️ **多显示器** —— 共享一块或多块显示器，每块各自一个会话。

## 🚦 平台

| 平台 | 主机 | 客户端 | 状态 |
| -------- | :--: | :----: | ------ |
| **Windows** | ✅ | ✅ | 参考实现 —— 日常在 LAN + Tailscale（互联网/NAT）上使用 |
| **macOS** | ✅ | ✅ | 两种角色都可用（ScreenCaptureKit + VideoToolbox + CGEvent） |
| **Android** | ✅ | ✅ | 客户端：画面 + 输入（触控板、键盘）。主机：只能看的屏幕共享（MediaProjection + MediaCodec），Android 10+ —— 正在 Google Play 测试 |
| **iOS** | ✅ | ✅ | 客户端：画面 + 输入（触控板、键盘）。主机：通过 Broadcast Upload Extension 实现的只读屏幕共享（ReplayKit + VideoToolbox）—— 正在 TestFlight 测试 |
| **Linux** | ✅ | ✅ | 两种角色都可用（PipeWire + VA-API + uinput + GTK3）—— Ubuntu、Debian、Mint、Fedora、openSUSE、Arch，通过 deb / rpm / 免安装二进制；已在两台机器间经 LAN 验证 |

## ✨ 里面有什么

- **端到端零拷贝** —— 直接采集进 VRAM → NVENC → 硬件解码 → 渲染；热路径全程不碰 CPU。
- **跑在 QUIC 上的专用协议** —— 无限 GOP + 按需 IDR、XOR FEC、自适应码率，全部复用在同一条加密连接上。
- **有画面就有声音** —— 机器自己的混音输出，Opus 64 kbps，每个数据报一帧 20 ms；丢一个包只损失几分之一秒，而且绝不干扰画面。永远不会是麦克风。
- **真正的输入** —— 相对鼠标（Raw Input）+ 面向 DirectInput 游戏的扫描码；主机自己的鼠标键盘永远优先。
- **一套共享内核** —— 协议、FEC 和码率控制都在 `core/` 里，编译进每一个客户端。
- **也有命令行** —— `deskhub-cli` 可以共享屏幕、打开远程 shell，并从脚本里或通过 SSH 操控主机，完全不需要任何图形工具包。见 [构建](docs/BUILD.zh.md#命令行客户端)。
- **专门被往死里折腾** —— 内核离线做单元测试，在 CI 里跑 ASan、UBSan 和 TSan，七个 libFuzzer 目标每晚轰击线格式、H.264 解析、重组、终端字节流、界面文本和会话状态机；每找到一次崩溃，就变成一个回归测试。

## 📚 文档

每份文档都以英文发布，旁边配有越南语 `*.vi.md`、中文 `*.zh.md` 和日语 `*.ja.md` 译本。
英文版本为准。

| 文档 | 内容 |
| --- | --- |
| [安装](docs/INSTALL.zh.md) ([en](docs/INSTALL.md)) | 把 Deskhub 装到五个平台中的每一个上 |
| [构建](docs/BUILD.zh.md) ([en](docs/BUILD.md)) | 从源码编译、测试、打包、发布 |
| [规格说明](docs/SPECIFICATION.zh.md) ([en](docs/SPECIFICATION.md)) | Deskhub 做什么，不含任何实现细节 |
| [架构](docs/ARCHITECTURE.zh.md) ([en](docs/ARCHITECTURE.md)) | 分层、线程、线上协议、设计决策 |
| [`SECURITY.zh.md`](SECURITY.zh.md) ([en](SECURITY.md)) | 威胁模型以及如何报告漏洞 |
| [`PRIVACY.zh.md`](PRIVACY.zh.md) ([en](PRIVACY.md)) | 隐私政策 |
| [`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md) ([en](THIRD_PARTY_NOTICES.md)) | 第三方组件与许可证 |

报告缺陷和反馈：[issues](https://github.com/manhpham90vn/Deskhub/issues) —— 请附上你的
设备型号。

## 📄 许可证

MIT —— 见 [`LICENSE`](LICENSE)。第三方组件及其声明（包括 Linux 应用中静态链接的 LGPL 版
FFmpeg）列在 [`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md)。
