[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · **中文** · [日本語](INSTALL.ja.md)

# Deskhub —— 安装

所有平台集中于本页。其中没有任何步骤需要 checkout source，每个 release 都已预先 build。
如需自行编译，见 [`BUILD.zh.md`](BUILD.zh.md)。

所有下载文件位于
[Releases 页面](https://github.com/manhpham90vn/Deskhub/releases)。移动端 build 通过
TestFlight 和 Google Play 分发。

本文件是 [`INSTALL.md`](INSTALL.md) 的译本；若两者有出入，以英文版为准。

| 平台 | 文件 | 安装命令 |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows.exe` | 下载并运行 |
| 🍎 macOS | `deskhub-v*-macos.dmg` | 打开 dmg，将 app 拖入 Applications |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation 与 KDE spin） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch 及其他发行版 | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | 安装 apk，或加入 Play 的 beta |
| 📱 iOS | —— | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

此外还提供 `deskhub-cli`，它是同一个 client，只是没有自己的窗口。见
[Command line](#-command-line)。

---

## 🪟 Windows

下载 `deskhub-v*-windows.exe` 并运行。没有 installer，没有 background service，也不需要
账号：整个 app 就是这一个文件。

首次使用时会发生两件事：

- **启动时申请一次 Administrator。** 否则无法将 mouse 和 keyboard inject 进提权窗口。
- **添加一条 Windows Firewall 规则**，由 app 在首次 share 时自行添加。

卸载 Deskhub 只需删除该 exe。Settings 和 key 会保留在 `%USERPROFILE%\.deskhub`，直到该
文件夹被删除。

## 🍎 macOS

下载 `deskhub-v*-macos.dmg`，打开后将 app 拖入 *Applications*。该 dmg 已使用 Developer
ID 进行 sign 并通过 Apple 的 notarize，因此打开时不会出现 Gatekeeper 警告。

Host 一块屏幕需要两个 macOS permission。两者均可从 app 的 **Settings** 页申请；该页同时
显示它们的当前状态，并提供直接跳转到对应 System Settings 面板的按钮。

| Permission | 用途 |
| --- | --- |
| **Screen Recording** | capture 本机 Mac 的 display |
| **Accessibility** | 允许 viewer 操作本机 Mac 的 mouse 和 keyboard |

若仅用于观看其他机器，则两者均不需要。

## 🐧 Linux

**若仅需 connect 并观看，安装后即可使用。** app 仅 link GTK3、PipeWire 和 libva，这些库
在任何原装桌面环境中均已具备；H.264 decoder 已编译进 app，因此不依赖任何 FFmpeg
package。

deb 与 rpm 内容一致，选择系统 package manager 支持的一种即可。两者都会安装下文第 3 条
所述的 `/dev/uinput` udev rule，因此安装后 remote input 立即可用，无需修改 group，也无需
重新登录。免安装 binary 可在任何具备 glibc 2.35 及以上的 x86_64 发行版上运行（Ubuntu
22.04、Fedora 36、openSUSE 15.5、当前版本的 Arch）。

deb 与 rpm 还会将 `deskhub-cli` 安装为 `/usr/bin/deskhub-cli`。见下文
[Command line](#-command-line)。

**若需共享本机屏幕**，还需满足三个条件。

### 1. screen-capture portal

Deskhub 的 capture 一律经由 `xdg-desktop-portal`。选择共享哪块屏幕的对话框即由它弹出。
所做选择会被记住，因此该对话框仅在首次 share 时出现。如需更换屏幕，使用 Host 页上的
*Choose screens again*。

GNOME 与 KDE 在所有主流发行版上均自带 portal backend，因此在 Ubuntu、Kubuntu、Fedora
Workstation、Fedora KDE、openSUSE 以及运行 GNOME/KDE 的 Arch 上**无需任何操作**。独立的
window manager 则需要安装：

```bash
sudo apt install xdg-desktop-portal-wlr      # Debian 系上的 sway / river / Wayfire
sudo dnf install xdg-desktop-portal-wlr      # ……Fedora 上
sudo pacman -S xdg-desktop-portal-wlr        # ……Arch 上
```

sway、river 和 Wayfire 均为基于 **wlroots** 库构建的 Wayland compositor。与 GNOME/KDE
不同，它们不附带自己的 portal backend，而 `-wlr` 正是为这三者提供 screen capture 的
backend。Hyprland 另有 `xdg-desktop-portal-hyprland`。

### 2. VA-API driver

H.264 在 GPU 上 encode，没有 software fallback。

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA 另需: nvidia-vaapi-driver

# Fedora —— 原装 Mesa 已禁用 H.264；可用的 driver 位于 RPM Fusion:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # 另需对应 GPU 厂商的 VA-API driver

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# 随后，在所有发行版上:
vainfo | grep -E 'H264.*Enc'                 # 必须至少输出一行，否则本机无法作为 host
```

### 3. `/dev/uinput` 的写权限

mouse 和 keyboard 通过它 inject。deb 与 rpm 已安装相应的 udev rule，无需额外操作。使用
免安装 binary 时，一条命令即可完成设置，无需 clone，也无需重新登录桌面：

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

若希望在交给 sudo 之前先查看脚本内容，可先下载
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh)，该脚本仅十余行。若已 checkout
source，等效命令为 `make setup-linux-permissions`。

未授予 uinput 权限时 app 仍可运行并观看，只是无法将 mouse 或 keyboard inject 进本机。

### Firewall

若已启用 firewall，需放行 UDP 47777：

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### 卸载

```bash
sudo apt remove deskhub      # 或: sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # settings、key 与已 pair 的机器
```

免安装 binary 只是一个文件，删除即可。

## 🤖 Android

作为 host 时仅支持 view-only 的屏幕共享，并且需要 **Android 10+**。仅用于观看时，更早
的版本亦可。

**直接安装 apk** —— 从
[Releases](https://github.com/manhpham90vn/Deskhub/releases) 下载
`deskhub-v*-android.apk` 并安装。该文件与 Google Play 版本使用同一个 key 进行 sign。

**Play beta** —— 三个步骤，全部使用与设备 Play Store **相同的 Google 账号**：

1. 加入测试者群组：[groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. 成为测试者：[play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. 安装（Play 同步需要数分钟）：[play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

请将 beta 保留在设备上 **14 天以上**；这是 Google 对 app 正式上架的前置要求。

## 📱 iOS

ipa 无法 sideload，因此 beta 通过 TestFlight 分发：

1. 安装 [TestFlight](https://apps.apple.com/app/testflight/id899247664)。
2. 加入 beta：**[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

与 Android 相同，iPhone 和 iPad 仅支持 view-only 的 host：没有任何移动 OS 允许 app 向其
自身所在的设备 inject input。

---

## 💻 Command line

`deskhub-cli` 是同一个 client，只是没有自己的窗口。它可共享屏幕、打开 remote shell，并
从脚本或通过 SSH 操作 host。运行 `deskhub-cli help` 查看完整命令列表。它读写的全部内容
—— settings、已 pair 的机器、trust 的 host key —— 与 app 共用同一份，因此两者始终一致。

| 平台 | 文件 |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows.exe` —— 下载即可运行，没有 installer |
| 🍎 macOS | `deskhub-cli-v*-macos` —— 一个 binary 同时支持 Apple Silicon 与 Intel |
| 🐧 Linux | `deskhub-cli-v*-linux-x86_64`；若已安装 deb 或 rpm 则已存在 |

macOS 与 Linux 的文件下载后不带 executable 位，需执行一次 `chmod +x`。两者均未像 dmg
那样进行 sign 和 notarize：在 macOS 上首次运行需要
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`，或在 System Settings → Privacy &
Security 中选择 *Open Anyway*。

在 Linux 上，它通过 app 所需的同一个 portal 和 VA-API driver 共享屏幕，remote input 也
依赖同一条 `/dev/uinput` rule，因此 [Linux](#-linux) 一节同样适用。在 macOS 上它可以共享
屏幕并打开 shell，但无法观看其他机器：`connect` 需要 command line build 不具备的
window layer，程序会明确报告这一点。此类用途请使用 app。

---

## 🔒 共享屏幕之前

一个 session 承载的全部内容 —— video、按键、mouse、clipboard 和 terminal 流量 —— 均运行
在 **QUIC/TLS** 之上。陌生机器只能通过 pairing handshake 被接受：它必须通过 **SPAKE2**
证明自己知道 host 的 passcode（passcode 本身不会被传输，且每条 connection 只允许一次尝
试），或等待 host 前的用户回答 *Let this machine in?*。

已 encrypt 不等同于可以暴露在 Internet 上。该 port 仍会回应 discovery 探测，与素未连接
过的机器首次 pair 仍以初始信任为前提。请优先使用**可信的 network** 或 **VPN**：在两台
机器上安装 [Tailscale](https://tailscale.com)，并连接其 `100.x.y.z` 地址。**不要对 UDP
47777 做 port-forward。**

[`SECURITY.zh.md`](../SECURITY.zh.md) 给出完整的 threat model、保护范围以及漏洞报告
方式。

## 🆘 出现问题时

- **找不到可连接的机器** —— 两台机器必须位于同一 network（或同一 Tailscale tailnet），
  且 host 侧的 UDP 47777 必须开放。
- **Linux：share 立即失败** —— 运行 `vainfo | grep -E 'H264.*Enc'`。结果为空说明本机没
  有可用的 H.264 encoder，无法作为 host。
- **Linux：指针不移动** —— 缺少第 3 条中的 `/dev/uinput` rule。
- **macOS：黑屏或 input 无响应** —— 在 Settings 页检查 Screen Recording 与
  Accessibility。
- **其他情况** —— 提交一个 [issue](https://github.com/manhpham90vn/Deskhub/issues)，并
  附上设备型号、OS 版本，以及 Host 或 Client 页 status 行的内容。
