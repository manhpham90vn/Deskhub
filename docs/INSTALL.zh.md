[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · **中文** · [日本語](INSTALL.ja.md)

# Deskhub —— 安装

所有平台，一页说完。这里没有任何一步需要下载源码 —— 每个发行版都是预编译好的。想自己
编译请看 [`BUILD.zh.md`](BUILD.zh.md)。

本文件是 [`INSTALL.md`](INSTALL.md) 的译本；若两者有出入，以英文版为准。

所有下载都在
[Releases 页面](https://github.com/manhpham90vn/Deskhub/releases)；移动端版本通过
TestFlight 和 Google Play 分发。

| 平台 | 文件 | 一行安装 |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows.exe` | 下载后直接运行 |
| 🍎 macOS | `deskhub-v*-macos.dmg` | 打开 dmg，把应用拖过去 |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation 与 KDE 版） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch 及其他 | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | 安装 apk，或加入 Play 测试 |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

还有一个 `deskhub-cli`，是同一个客户端，只是没有自己的窗口 —— 见
[命令行](#-命令行)。

---

## 🪟 Windows

下载 `deskhub-v*-windows.exe` 并运行。没有安装程序，没有后台服务，也不需要账号 ——
整个应用就是这一个文件。

第一次使用时会发生两件事：

- **启动时需要一次管理员权限。** 不这样就无法把鼠标和键盘注入到提权窗口里。
- **一条 Windows 防火墙规则**，由应用在你第一次共享时自己添加。

要卸载 Deskhub，删掉那个 exe 即可。设置和密钥会留在 `%USERPROFILE%\.deskhub`，直到你
把那个文件夹也删掉。

## 🍎 macOS

下载 `deskhub-v*-macos.dmg`，打开，把应用拖进 *Applications*。这个 dmg 用 Developer ID
签名并经过 Apple 公证，所以打开时不会有 Gatekeeper 警告。

共享屏幕需要两项 macOS 权限，都可以在应用的 **Settings** 页面申请；那里也会显示它们的
实时状态，并有一个直接跳到对应系统设置面板的按钮：

| 权限 | 用途 |
| --- | --- |
| **屏幕录制（Screen Recording）** | 采集这台 Mac 的画面 |
| **辅助功能（Accessibility）** | 让观看者移动这台 Mac 的鼠标和键盘 |

看别的机器则两项都不需要。

## 🐧 Linux

**只要连接和观看，装上就够了。** 应用只链接 GTK3、PipeWire 和 libva，这些每个原装桌面
都有；H.264 解码器是编译进去的，所以完全不涉及 FFmpeg 包。

deb 和 rpm 内容完全一致 —— 挑你的包管理器认识的那个。两者都带有下面第 3 条所说的
`/dev/uinput` udev 规则，所以装完就能远程输入，不用改用户组，也不用重新登录。免安装的
二进制可以在任何 glibc 2.35+ 的 x86_64 发行版上运行（Ubuntu 22.04、Fedora 36、
openSUSE 15.5、任何当前版本的 Arch）。

deb 和 rpm 还会把 `deskhub-cli` 装到 `/usr/bin/deskhub-cli` —— 见下面的
[命令行](#-命令行)。

**要共享这台机器的屏幕**，还需要再准备三样东西。

### 1. 一个屏幕采集 portal

Deskhub 始终通过 `xdg-desktop-portal` 采集 —— 就是它弹出"要共享哪块屏幕？"的对话框。
你的选择会被记住，所以这个对话框只在第一次共享时出现；想换一块屏幕时，Host 页面上的
*Choose screens again* 会把它叫回来。

GNOME 和 KDE 在所有主流发行版上都自带 portal 后端 —— 在装了 GNOME/KDE 的 Ubuntu、
Kubuntu、Fedora Workstation、Fedora KDE、openSUSE 或 Arch 上**什么都不用做**。独立的
窗口管理器则需要装一个：

```bash
sudo apt install xdg-desktop-portal-wlr      # Debian 系上的 sway / river / Wayfire
sudo dnf install xdg-desktop-portal-wlr      # ……在 Fedora 上
sudo pacman -S xdg-desktop-portal-wlr        # ……在 Arch 上
```

sway、river 和 Wayfire 是建立在 **wlroots** 库上的 Wayland 合成器；和 GNOME/KDE 不同，
它们不自带 portal 后端，而 `-wlr` 就是为它们全部实现屏幕采集的那个后端。Hyprland 有
自己的 `xdg-desktop-portal-hyprland`。

### 2. 一个 VA-API 驱动

H.264 在 GPU 上编码；没有软件回退。

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA 还需要：nvidia-vaapi-driver

# Fedora —— 官方 Mesa 关掉了 H.264；能用的驱动在 RPM Fusion 里：
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD（RPM Fusion）
sudo dnf install intel-media-driver          # Intel（RPM Fusion）
sudo dnf install nvidia-vaapi-driver         # NVIDIA（RPM Fusion）

# openSUSE
sudo zypper install libva-utils              # 再加上你的 GPU 厂商的 VA-API 驱动

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel：intel-media-driver · NVIDIA：libva-nvidia-driver

# 然后在每个发行版上都执行：
vainfo | grep -E 'H264.*Enc'                 # 必须打印出至少一行，否则这台机器无法做主机
```

### 3. `/dev/uinput` 的写权限

鼠标和键盘就是靠它注入的。deb 和 rpm 已经帮你装好了 udev 规则 —— 什么都不用做。用免安装
二进制的话，一条命令就能配好，不用克隆仓库，桌面上也不用重新登录：

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

不放心直接管道给 sudo？先下载
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) 看一遍 —— 一共十几行。从源码
检出的话，同样的事情是 `make setup-linux-permissions`。

没有 uinput 授权，应用照样能跑、照样能看 —— 只是无法把鼠标或键盘注入这台机器。

### 防火墙

如果你启用了防火墙，请开放 UDP 47777：

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### 卸载

```bash
sudo apt remove deskhub      # 或：sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # 设置、密钥和已配对的机器
```

免安装二进制就是一个文件 —— 删掉它。

## 🤖 Android

做主机是只能看的屏幕共享，需要 **Android 10+**。观看在更老的版本上也能用。

**直接装 apk** —— 从
[Releases](https://github.com/manhpham90vn/Deskhub/releases) 下载 `deskhub-v*-android.apk`
并安装。它和 Google Play 版本用的是同一个签名密钥。

**Play 测试** —— 三步，全部使用与手机 Play 商店**相同的 Google 账号**：

1. 加入测试者群组：[groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. 成为测试者：[play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. 安装（给 Play 几分钟同步）：[play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

请把测试版留在机器上 **14 天以上** —— Google 要求满足这一点，应用才能公开发布。

## 📱 iOS

ipa 无法侧载，所以测试版通过 TestFlight 发布：

1. 安装 [TestFlight](https://apps.apple.com/app/testflight/id899247664)。
2. 加入测试：**[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

和 Android 一样，iPhone 或 iPad 做主机时只能看 —— 没有哪个移动系统允许应用向自己所在的
设备注入输入。

---

## 💻 命令行

`deskhub-cli` 是同一个客户端，只是没有自己的窗口：它能共享屏幕、打开远程 shell，并从
脚本里或通过 SSH 操控主机。运行 `deskhub-cli help` 看命令列表。它读写的一切 —— 设置、
已配对机器、受信任的主机密钥 —— 都和应用共用，所以两者始终一致。

| 平台 | 文件 |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows.exe` —— 下载即用，无需安装 |
| 🍎 macOS | `deskhub-cli-v*-macos` —— 一个二进制同时支持 Apple Silicon 和 Intel |
| 🐧 Linux | `deskhub-cli-v*-linux-x86_64`，或者装了 deb / rpm 之后本来就有 |

macOS 和 Linux 的文件下载下来没有可执行位，所以要 `chmod +x` 一次。两者都没有像 dmg
那样签名和公证：在 macOS 上第一次运行需要
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`，或者在系统设置 → 隐私与安全性里
点 *仍要打开*。

在 Linux 上，它通过应用所需的同一个 portal 和 VA-API 驱动共享屏幕，远程输入也依赖同一条
`/dev/uinput` 规则，所以 [Linux](#-linux) 那一节对它同样适用。在 macOS 上它能共享屏幕、
打开 shell，但不能*观看*别人的屏幕 —— `connect` 需要一个命令行版本没有的窗口层，它会
明确告诉你；这种情况请用图形应用。

---

## 🔒 共享屏幕之前

一次会话承载的一切 —— 画面、按键、鼠标、剪贴板和终端流量 —— 都跑在 **QUIC/TLS** 上，
陌生机器只能通过配对握手进来：它必须用 **SPAKE2** 证明自己知道主机的通行码（通行码本身
从不上网，而且每条连接只允许猜一次），或者等主机前的人回答*让这台机器进来吗？*。

加密不等于能扛住互联网。端口仍然会回应发现探测，而与一台素未谋面的机器第一次配对，本身
就是一次信任的跳跃。请优先使用**你信任的网络**或 **VPN** —— 在两台机器上都装
[Tailscale](https://tailscale.com)，然后连 `100.x.y.z` 地址。**永远不要把 UDP 47777
做端口转发。**

[`SECURITY.zh.md`](../SECURITY.zh.md) 里有完整的威胁模型、哪些受保护哪些不受保护，以及
如何报告漏洞。

## 🆘 如果哪里不对劲

- **没有可连接的东西** —— 两台机器必须在同一个网络里（或同一个 Tailscale tailnet），
  并且主机上的 UDP 47777 必须开放。
- **Linux：一共享就失败** —— 运行 `vainfo | grep -E 'H264.*Enc'`；结果为空说明这台机器
  没有可用的 H.264 编码器，无法做主机。
- **Linux：指针不动** —— 缺少第 3 条里的 `/dev/uinput` 规则。
- **macOS：黑屏或输入无反应** —— 检查 Settings 页面上的屏幕录制和辅助功能权限。
- **还是卡住** —— 提一个
  [issue](https://github.com/manhpham90vn/Deskhub/issues)，并附上你的设备型号、系统版本，
  以及 Host 或 Client 页面状态行上显示的内容。
