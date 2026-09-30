[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · **中文** · [日本語](INSTALL.ja.md)

# Deskhub —— 安装

按下面的平台说明安装已预先 build 的 release，无需 checkout source。如果想自行编译
Deskhub，请参阅 [`BUILD.zh.md`](BUILD.zh.md)。

桌面安装包和 Android APK 可从
[Releases 页面](https://github.com/manhpham90vn/Deskhub/releases) 下载。移动端 beta 也可
通过 TestFlight 或 Google Play 获取。

本文件是 [`INSTALL.md`](INSTALL.md) 的译本；若两者有出入，以英文版为准。

| 平台 | 文件 | 安装方式 |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | 下载并安装，然后从开始菜单或桌面打开 |
| 🍎 macOS | `deskhub-v*-macos.dmg` | 打开 dmg，将 Deskhub 拖入 Applications |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation 与 KDE spin） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch 及其他发行版 | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | 安装 apk，或加入 Play 的 beta |
| 📱 iOS | —— | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

还可以用 `deskhub-cli` 执行 terminal 命令或脚本。在 Windows 和 Linux 上，CLI 也能
打开远程屏幕窗口。见 [Command line](#-command-line)。

Windows 可通过 `winget`、macOS 可通过 Homebrew，Ubuntu、Kubuntu、Debian 和 Mint 可通过
[apt repository](#-linux) 安装。需要新版本时，使用相应 package manager 的升级命令。

---

## 🪟 Windows

下载并运行 `deskhub-v*-windows-setup.exe`。安装程序会将 DeskHub 添加到开始菜单，
默认创建桌面快捷方式，并可在安装完成后立即启动。无需账号或 background service。
需要便携版时仍可下载 `deskhub-v*-windows.exe`。

也可以让 winget 安装同一个安装包并保持更新：`winget upgrade` 获取每个新 release，
`winget uninstall ManhPham.Deskhub` 将其移除：

```powershell
winget install ManhPham.Deskhub
```

如果之前安装过 winget 便携版，请先运行一次 `winget uninstall ManhPham.Deskhub`，
然后重新安装。`%USERPROFILE%\.deskhub` 中的设置和密钥会保留。

首次使用时会发生两件事：

- **启动时申请一次 Administrator。** 否则无法将 mouse 和 keyboard inject 进提权窗口。
- **添加一条 Windows Firewall 规则**，由 app 在首次 share 时自行添加。

可在 Windows 设置中卸载 DeskHub，或运行 `winget uninstall ManhPham.Deskhub`。
便携版只需删除 exe。Settings 和 key 会保留在 `%USERPROFILE%\.deskhub`，直到删除该文件夹。

## 🍎 macOS

下载 `deskhub-v*-macos.dmg`，打开后将 app 拖入 *Applications*。该 dmg 已使用 Developer
ID 进行 sign 并通过 Apple 的 notarize，因此打开时不会出现 Gatekeeper 警告。

也可以通过 Homebrew 安装，之后由 `brew upgrade` 保持更新：

```bash
brew install --cask manhpham90vn/tap/deskhub
```

Host 一块屏幕需要两个 macOS permission。两者均可从 app 的 **Settings** 页申请；该页同时
显示它们的当前状态，并提供直接跳转到对应 System Settings 面板的按钮。

| Permission | 用途 |
| --- | --- |
| **Screen Recording** | capture 本机 Mac 的 display |
| **Accessibility** | 允许 viewer 操作本机 Mac 的 mouse 和 keyboard |

若仅用于观看其他机器，则两者均不需要。

## 🐧 Linux

**若只需 connect 并观看，先安装 app 软件包即可。** H.264 decoder 已包含在 app 中，
无需另装 FFmpeg package。系统仍需提供 app 使用的桌面库，包括 GTK3、PipeWire 和 libva。

deb 与 rpm 内容一致，选择系统 package manager 支持的一种即可。两者都会安装下文第 3 条
所述的 `/dev/uinput` udev rule，因此安装后 remote input 立即可用，无需修改 group，也无需
重新登录。免安装 binary 可在任何具备 glibc 2.35 及以上的 x86_64 发行版上运行（Ubuntu
22.04、Fedora 36、openSUSE 15.5、当前版本的 Arch）。

CLI 有单独的 deb 和 rpm 包，可独立安装或与桌面 app 同时安装。
两个包都将命令放在 `/usr/bin`，桌面 app 还会在应用菜单中创建启动器。

在 Ubuntu、Kubuntu、Debian 和 Mint 上，Deskhub 的 apt repository 安装的是同一个 deb，
并让 `sudo apt upgrade` 获取之后的每个 release。它支持 Ubuntu 22.04 及以上、Debian 12
及以上：

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

CLI 可单独运行 `sudo apt install deskhub-cli` 安装。从旧版包含 CLI 的 `deskhub`
包升级后，也需安装 `deskhub-cli` 才能保留该命令。

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
rm -rf ~/.deskhub            # settings、key、允许的 client 与受信任的 host
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # apt repository（若已添加）
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

`deskhub-cli` 提供共享屏幕、打开 remote shell 等命令，也可从脚本或 SSH 使用。
在 Windows 和 Linux 上，`connect` 会打开远程屏幕窗口；在 macOS 上，请用桌面 app
观看屏幕。运行 `deskhub-cli help` 可查看命令列表。CLI 与 app 共用 settings、机器 key、允许的
client、connection request 及受信任的 host。

| 平台 | 文件 |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows-setup.exe` —— 安装后可从 `PATH` 运行 `deskhub-cli` |
| 🍎 macOS | `deskhub-cli-v*-macos` —— 一个 binary 同时支持 Apple Silicon 与 Intel |
| 🐧 Linux | `deskhub-cli-v*-amd64.deb`、`deskhub-cli-v*-x86_64.rpm`，或便携版 `deskhub-cli-v*-linux-x86_64` |

直接下载 Linux 软件包时，在 Ubuntu/Debian 上运行
`sudo apt install ./deskhub-cli-v*-amd64.deb`，在 Fedora 上运行
`sudo dnf install ./deskhub-cli-v*-x86_64.rpm`。无需安装桌面应用，也能从 `/usr/bin`
运行该命令。

macOS 与 Linux 的便携版 binary 下载后可能需要执行一次 `chmod +x`；`.deb` 和 `.rpm`
应通过软件包管理器安装。macOS 便携版 binary 未像 dmg 一样签名或公证，首次运行可能需要
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`，或在 System Settings → Privacy &
Security 中选择 *Open Anyway*。

Windows 安装程序无需管理员权限即可将 CLI 加入用户的 `PATH`。
便携版 `deskhub-cli-v*-windows.exe` 仍可下载。通过 package manager 安装也会把命令加入 `PATH`：Windows 上用
`winget install ManhPham.DeskhubCLI`，macOS 上用 `brew install manhpham90vn/tap/deskhub-cli`
—— Homebrew 安装时不会附加 quarantine 标记。在 Ubuntu 或 Debian 上添加上述 apt
repository 后，运行 `sudo apt install deskhub-cli`。

在 Windows 上通过 winget 安装后，请打开新的 PowerShell 窗口运行
`deskhub-cli help`。用 `Get-Command deskhub-cli` 查看实际调用的文件。
直接下载的便携版 exe 不会自动加入 `PATH`。

在 Linux 上，它通过 app 所需的同一个 portal 和 VA-API driver 共享屏幕，remote input 也
依赖同一条 `/dev/uinput` rule，因此 [Linux](#-linux) 一节同样适用。在 macOS 上它可以共享
屏幕并打开 shell，但无法观看其他机器：`connect` 需要 command line build 不具备的
window layer，程序会明确报告这一点。此类用途请使用 app。

---

## 🔒 共享屏幕之前

一个 session 承载的全部内容 —— video、按键、mouse、clipboard 和 terminal 流量 —— 均运行
在 **QUIC/TLS** 之上，访问方式与 SSH 相同：host 只放行其列表中所列 key 的设备，而每台设备
只有一把 key。把一台设备加入该列表有三种方式，只需其中一种：

- **扫描 QR code。** host 共享期间，点击其地址列表旁的 **Show QR code**。在手机上，于
  Client 页点击 **Scan QR code** 并对准屏幕；在其他任何设备上，复制码下方的链接并粘贴到
  地址栏。该设备一步完成信任、放行与连接。
- **批准 request。** 在该设备上输入 host 的地址并点击 *Connect*。host 会显示一条
  *Connection request* 通知，并在 Host 页与 Devices 页的 **Connection requests** 下列出它，
  附其名称、key fingerprint 与地址；点击 **Approve**，
  该设备在下一次尝试时即可 connect —— 它会自行持续尝试两分钟。
- **粘贴 key。** 在该设备上，**Devices** → **Copy public key**；在 host 上，**Devices** →
  **Devices allowed to connect to this machine** → *Allow*，粘贴，完成。

首次按地址 connect 时会显示 **New host** 对话框，其中有 host 的 key fingerprint：将其与
host 的 Devices 页上 **This machine** 下的 fingerprint 核对，然后点击 *Trust and connect*。
QR code 会跳过该对话框，因为码中携带了 fingerprint。此后该 host 会列在 **Trusted hosts**
下，即使更换地址也依然受信任。

[Key 与访问权限](#-key-与访问权限)一节逐步介绍 app 与 CLI 中的每一种方式，以及脚本与撤销。

没有 passcode，也没有任何放未知机器进来的开关：没有你的 *Approve*、你展示的 QR code 或
你粘贴的 key，任何人都无法 connect。Deskhub 从不 scan network。

请在**可信的 network** 或 **VPN** 中使用 Deskhub，**不要对 UDP 47777 做
port-forward**。Encrypt 能保护 session 内容，但除非你核对 fingerprint，否则首次
connect 某个 host 时会信任其出示的 key。远程访问时，可在两台机器上安装
[Tailscale](https://tailscale.com)，再 Connect 到 `100.x.y.z` 地址。

[`SECURITY.zh.md`](../SECURITY.zh.md) 给出完整的 threat model、保护范围以及漏洞报告
方式。

## 🔑 Key 与访问权限

Deskhub 用密钥对登录，方式与 SSH 相同。每台机器只有**一把 key**，在 Deskhub 首次运行时
创建；无论共享还是 connect，这把 key 就代表这台机器：你 connect 到它时核对的 fingerprint，
与 host 放行它时保存的 public key，是同一把 key。host 只放行其列表中的 key，client 只
connect 到其已信任 key 的 host。所有相关操作都在 **Devices** 页上，分为四节：**This machine**、
**Connection requests**、**Devices allowed to connect to this machine** 与 **Trusted hosts**。下面每一步都有对应的
`deskhub-cli` 命令；app 与 CLI 读取同一组文件，因此在一边所做的更改会出现在另一边。

### 你的 key

**Devices** → **This machine** 显示本设备的名称与该 key 的 fingerprint，形如 `SHA256:…`，旁有 *Copy* 按钮 —— 这就是 connect 到你的人要核对的内容。
其旁的 **Copy public key**（CLI：`deskhub-cli key public`）复制一行形如
`ecdsa-sha2-nistp256 AAAA… laptop` 的内容；末尾的标签是本设备的名称，在 **Settings** →
*General* → **Device name** 中设置，host 的所有者可据此分辨这是谁的 key。当 QR code 与
request 都不方便时，这一行就是你交给 host 所有者的内容 —— 可以放心分享。private 部分从不
离开本机，没有任何需要生成或导入的东西，该 key 也从不自行更换。若你删除了
`host_key.pem`，本机会获得新的身份：host 需要重新放行它，此前信任它的设备会将其视为
新的 host。

### 允许设备 connect

由 host 的所有者决定，有三种方式。无论用哪一种，该设备随后都会出现在 **Devices** →
**Devices allowed to connect to this machine** 下，以其
名称为标签；其旁的 *Remove* 将其移除，*Remove every client* 在确认后清空整个列表。

**用 QR code connect** —— 最快，也是唯一无需核对 fingerprint 的方式。host 共享期间，点击其
地址列表旁的 **Show QR code**。码中携带 host 的地址与 port、其 key fingerprint、其名称，
以及一个有效五分钟的一次性随机 token。

- 在手机或平板上，打开 Client 页并点击 **Scan QR code**。首次使用时，系统会申请摄像头
  permission；Deskhub 只在此处使用摄像头，在设备上解码画面，不保存任何内容。用系统相机
  或从消息中打开 `deskhub://pair/…` 链接效果相同。
- 在任何设备上，包括桌面电脑，复制码下方显示的链接并粘贴到地址栏，然后点击 *Connect*
  （CLI：`deskhub-cli connect 'deskhub://pair/…'`；`sources`、`shell` 与 `send` 也接受
  该链接）。

该设备会检查应答的机器是否持有码中印出的 key —— 若该地址上应答的是另一台机器，它会停下
并提示该 QR code 不是来自那台机器 —— 然后信任该 host、发送 token，并一次完成放行与连接。
每个码只能使用一次；点击 **Hide QR code** 或停止共享，即使无人使用它也会失效。已获放行
的设备也可以扫描该码，仅用于获取 host 的当前地址。

**批准 connection request** —— 当设备不在同一房间时。

1. 在该设备上输入 host 的地址（`192.168.1.10`；host 不使用 47777 时写
   `192.168.1.10:PORT`），点击 *Connect*；按下文所述确认 **New host** 对话框。随后页面
   会显示正在等待 host 所有者批准，并自行持续尝试两分钟。*Cancel* 可停止。
2. 在 host 上，Host 页会在 **Connection requests** 下列出该设备 —— 其名称、fingerprint
   的开头及其地址，附 **Approve** 与 **Deny**。确认 fingerprint 与该设备 Devices 页上的
   一致、地址也与你预期该设备所在的位置相符，然后点击 *Approve*。该设备在下一次尝试时
   即可 connect。*Deny* 丢弃该 request；该设备只会被告知无人及时批准。
3. request 保留十分钟，host 最多保留十六条。若你在设备放弃之后才批准，它只需再点击
   一次 *Connect*。

在 command line 中：`deskhub-cli access requests`（脚本可加 `--json`）列出等待中的 request，
`deskhub-cli access approve --fingerprint SHA256:…` 放行其中一条，
`deskhub-cli access deny --fingerprint SHA256:…` 丢弃它。正在运行的 `deskhub-cli share` 会
在每条新 request 到达时打印它，并附上可粘贴到另一个 terminal 的 approve 命令。

**粘贴 key** —— 当你更愿意自己传递 key 时。

1. 请将要 connect 的人在其 Devices 页点击 **Copy public key**（见[你的 key](#你的-key)），
   并把那一行发给你。
2. 打开 **Devices** → **Devices allowed to connect to this machine**，粘贴该行并点击 *Allow*。仅接受 Ed25519 与 ECDSA P-256 public key。

在 command line 中，将该行 pipe 给 `access add`：

```sh
deskhub-cli key public                                         # 在设备上
deskhub-cli access add --stdin                                 # 在 host 上：粘贴后按 Ctrl-D
deskhub-cli key public | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # 在 host 上
deskhub-cli access remove --fingerprint SHA256:…               # 在 host 上
```

`deskhub-cli access clear` 的作用与 *Remove every client* 相同。

### 首次 connect

QR code 会替你完成这一步。首次按地址 connect 的流程与 SSH 相同：

1. 在 client 上输入 host 的地址，然后点击 *Connect*。
2. **New host** 对话框显示 host key 的 fingerprint，形如 `SHA256:…`。若该地址曾属于你
   信任的另一个 host，对话框会如此说明并指出它的名称 —— 已知地址上的另一把 key 就是另一台
   机器，继续之前请确认应答的到底是哪一台。
3. 在 host 上打开 **Devices** → **This machine**（fingerprint 旁的 *Copy* 可将其复制到
   剪贴板）。通过你已信任的渠道核对两个 fingerprint —— 当面、
   电话，或确认是对方本人的聊天。
4. 一致则点击 *Trust and connect*；不一致则点击 *Cancel*。

此后该 host 列在 **Trusted hosts** 下，附其名称、fingerprint 与最后一次应答的地址。信任
跟随 key 而非地址：host 更换地址后，输入新地址并 connect 即可 —— 没有对话框，也无需重新
批准 —— 列表会记住新地址供下次使用。

**提前固定 host。** 用 CLI 可在首次连接前保存 host key，从而无需对话框：

```sh
deskhub-cli host-key public                                    # 在 host 上
deskhub-cli host add office --address 192.168.1.10 --host-key-stdin
```

将 host 输出的那一行粘贴到第二条命令中（在 client 上），然后按 Ctrl-D。`office` 是你自选
的 alias：`connect office`、`sources office`、`shell office` 和 `send office FILE` 都接受
它。`deskhub-cli host list` 显示所有已保存的 host，包括在 app 中信任的 host —— 它们会得到
一个由地址生成的 alias，例如 `192-168-1-10-47777`。`host update ALIAS` 可修改地址
（`--address`）或固定的 host key（`--host-key-stdin`）；`host remove ALIAS` 删除该 host，
给定其 fingerprint、alias 或最后一次应答的地址的 `deskhub-cli trust forget` 也可以。

### 脚本与 command line

- **未知 host 会被拒绝。** `sources`、`connect`、`shell` 与 `send` 不会与尚未保存 key 的
  host 通信，而是打印其 fingerprint；与 host 核对后，加上 `--accept-new-host-key` 重新
  运行即可保存。若给出的是 invite 链接而非地址，它们会从链接中固定该 host，无需任何 flag。
- **等待批准。** 尚未放行本机的 host 会收到请求；`connect`、`shell`、`send` 与 `sources`
  会打印一行说明，并最多等待两分钟让 host 所有者点击 *Approve*；无人批准则带原因失败。
- **显示 QR code。** `deskhub-cli share --qr` 以方块字符打印 QR code 并附 invite 链接，
  供手机直接从 terminal 上扫描。
- **独立的配置。** `--config-dir DIR`（放在命令之前或之后均可）或环境变量
  `DESKHUB_CONFIG_DIR` 让 CLI 使用另一个目录中的 key、允许的 client、request、受信任的
  host 与 settings —— 适用于 service account 或测试环境。
- **8.0 中已移除。** `key generate`、`key import`、`key delete`、`key list`、`key public
  --name`、`devices identities`、`--identity` 与 `host add --identity` 已不存在：每台机器
  只有一把 key。exit code `5`（"host key 已变更"）也已移除，因为不同的 key 现在就是未知的
  host。
- **Exit code。** 脚本可通过 exit code 区分连接被拒与其他失败；各代码列在
  `BUILD.zh.md` 的 [Command line client](BUILD.zh.md#command-line-client) 一节。列出信息的
  命令支持 `--json`。

### 撤销某台设备

在 host 上点击其 key 旁的 *Remove*，或运行
`deskhub-cli access remove --fingerprint SHA256:…`。该设备打开的所有 session 立即结束，
在你重新放行它之前 —— 批准它的下一条 request、向它展示 QR code，或粘贴它的 key —— 无法
再次 connect。

**已知地址上出现另一把 key 应答时。** 这发生在 host 重装了 Deskhub 或丢失了 settings
文件夹时 —— 也可能是另一台机器占用了该地址。Deskhub 不会拒绝；它把这台机器当作你从未
见过的机器，显示 **New host** 对话框，并附警告指出以前在该地址应答的是哪个 host。只有在
确知原因时才信任它。旧的 host 会一直留在 **Trusted hosts** 下，直到你 *Remove* 它（或
`deskhub-cli host remove ALIAS`）；使用 CLI 时，`host update ALIAS --host-key-stdin` 可直接
把某个 alias 重新固定到新 key。

### 从旧版 Deskhub 升级

**从 7.0.x 升级。** 两台机器都需要 8.0 —— 任何一方运行 7.0.x 的 Deskhub 都无法 connect，
并会被拒绝，提示 "That machine uses an incompatible authentication version"。每台机器保留
其原有的 key 与 fingerprint，因此你信任过的 host 依然受信任。变化的是设备*用来*登录的
key：现在就是那把机器 key，因此每台设备都需要重新放行一次 —— 一次 *Approve*、一次扫描
QR code，或一次粘贴其 key。7.0.x 写入的 `client_key*.pem` 与 `host_cert.pem` 文件会被
忽略；其中的内容不会被读取或迁移，你可以删除它们。

**从 6.x 或更早版本升级。** 不做任何迁移。passcode 和已配对设备列表不会保留：请按上文
所述重新放行每台设备并重新信任每个 host。

## 🆘 出现问题时

- **找不到可连接的机器** —— 两台机器必须位于同一 network（或同一 Tailscale tailnet），
  且 host 侧的 UDP 47777 必须开放。
- **"Waiting for the owner of … to approve this device"** —— host 尚未列入本设备；其所有者
  需要在其 Host 页的 **Connection requests** 下点击 *Approve*、向你展示 QR code，或粘贴你的
  key —— 见[允许设备 connect](#允许设备-connect)。由 7.0.x 放行的设备必须重新放行。
- **"The owner of that machine did not approve this device in time"** —— 两分钟内没有
  *Approve*。该 request 在 host 上保留十分钟：请对方批准后，再次点击 *Connect*。
- **"The machine that answered is not the one that made this QR code"** —— 码中地址上应答
  的是别的东西。在 host 上重新显示该码并再扫描一次；若持续出现，请检查是哪台机器占用了
  该地址。
- **对已信任的 host 弹出 New host 对话框** —— 该地址上应答的是另一把 key；见
  [撤销某台设备](#撤销某台设备)。
- **"That machine uses an incompatible authentication version"** —— 有一方运行的是旧版
  Deskhub；请更新两台机器。见[从旧版 Deskhub 升级](#从旧版-deskhub-升级)。
- **Linux：share 立即失败** —— 运行 `vainfo | grep -E 'H264.*Enc'`。结果为空说明本机没
  有可用的 H.264 encoder，无法作为 host。
- **Linux：指针不移动** —— 缺少第 3 条中的 `/dev/uinput` rule。
- **macOS：黑屏或 input 无响应** —— 在 Settings 页检查 Screen Recording 与
  Accessibility。
- **其他情况** —— 提交一个 [issue](https://github.com/manhpham90vn/Deskhub/issues)，并
  附上设备型号、OS 版本，以及 Host 或 Client 页 status 行的内容。
