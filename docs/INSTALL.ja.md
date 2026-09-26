[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · [中文](INSTALL.zh.md) · **日本語**

# Deskhub — インストール

全プラットフォームを 1 ページにまとめている。ここには source の checkout を必要とする
手順はない。release はすべて build 済みで配布している。自分でコンパイルする場合は
[`BUILD.ja.md`](BUILD.ja.md) を参照。

ダウンロードはすべて
[Releases ページ](https://github.com/manhpham90vn/Deskhub/releases) にある。モバイルの
build は TestFlight と Google Play から配布している。

本書は [`INSTALL.md`](INSTALL.md) の翻訳。食い違いがある場合は英語版が正文。

| プラットフォーム | ファイル | インストール手順 |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | ダウンロードしてインストールし、スタートメニューまたはデスクトップから起動 |
| 🍎 macOS | `deskhub-v*-macos.dmg` | dmg を開き、app を Applications にドラッグ |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation と KDE spin） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch およびその他のディストリビューション | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | apk をインストール、または Play の beta に参加 |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

このほかに `deskhub-cli` がある。同じ client で、自前のウィンドウを持たない点だけが
異なる。[Command line](#-command-line) を参照。

**Package manager** を使えば Deskhub は自動で最新に保たれる。Windows では
`winget install ManhPham.Deskhub`、macOS では `brew install --cask manhpham90vn/tap/deskhub`、
Ubuntu、Kubuntu、Debian、Mint では [apt repository](#-linux) を使う。

---

## 🪟 Windows

`deskhub-v*-windows-setup.exe` をダウンロードして実行する。スタートメニューに登録され、
デスクトップのショートカットも既定で作成される。インストール後すぐに起動することもできる。
アカウントや background service は不要。ポータブル版の `deskhub-v*-windows.exe` も引き続き利用できる。

winget に同じインストーラーを取得させ、最新に保つこともできる。`winget upgrade` で新しい release
を取得し、`winget uninstall ManhPham.Deskhub` で削除する。

```powershell
winget install ManhPham.Deskhub
```

以前の winget ポータブル版をインストール済みの場合は、
`winget uninstall ManhPham.Deskhub` を一度実行してから再インストールする。
`%USERPROFILE%\.deskhub` の設定と鍵は保持される。

初回の使用時には次の 2 点が発生する。

- **起動時に一度 Administrator を要求する。** これがないと、昇格したウィンドウへ mouse
  と keyboard を inject できない。
- **Windows Firewall のルールを 1 本追加する。** 初回の share 時に app 自身が追加する。

DeskHub は Windows の設定または `winget uninstall ManhPham.Deskhub` でアンインストールできる。
ポータブル版は exe を削除する。Settings と key は、フォルダを削除するまで
`%USERPROFILE%\.deskhub` に残る。

## 🍎 macOS

`deskhub-v*-macos.dmg` をダウンロードして開き、app を *Applications* にドラッグする。
この dmg は Developer ID で sign され、Apple の notarize も通っているため、Gatekeeper
の警告は表示されない。

Homebrew からもインストールでき、以後は `brew upgrade` で最新に保たれる。

```bash
brew install --cask manhpham90vn/tap/deskhub
```

画面を host するには macOS の permission が 2 つ必要である。いずれも app の
**Settings** ページから要求でき、同ページには現在の状態と、対応する System Settings の
ペインを直接開くボタンも用意されている。

| Permission | 用途 |
| --- | --- |
| **Screen Recording** | この Mac の display を capture する |
| **Accessibility** | viewer がこの Mac の mouse と keyboard を操作できるようにする |

他のマシンを観るだけであれば、どちらも不要である。

## 🐧 Linux

**connect して観るだけであれば、インストールするだけでよい。** app が link するのは
GTK3、PipeWire、libva のみで、いずれも標準的なデスクトップ環境に含まれている。H.264
decoder は app にコンパイル済みのため、FFmpeg の package には依存しない。

deb と rpm の内容は同一であり、利用中の package manager が扱えるほうを選べばよい。
どちらにも下記の要件 3 で説明する `/dev/uinput` の udev rule が含まれるため、インストール
直後から remote input が動作する。group の変更も再ログインも不要である。portable
binary は glibc 2.35 以上の x86_64 ディストリビューションで動作する（Ubuntu 22.04、
Fedora 36、openSUSE 15.5、現行の Arch）。

CLI には別の deb と rpm があり、デスクトップ app と独立して、または一緒にインストールできる。
両方ともコマンドを `/usr/bin` に配置し、デスクトップ app はアプリケーションメニューに
ランチャーも追加する。

Ubuntu、Kubuntu、Debian、Mint では、Deskhub の apt repository が同じ deb をインストール
し、以後の release は `sudo apt upgrade` で届く。対応するのは Ubuntu 22.04 以降と
Debian 12 以降である。

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update && sudo apt install deskhub
```

CLI は `sudo apt install deskhub-cli` で別途インストールする。以前の `deskhub`
パッケージに同梱されていた CLI を使い続けるには、アップグレード後にこのパッケージを入れる。

**このマシンの画面を共有する**には、さらに 3 つの条件が必要である。

### 1. screen-capture portal

Deskhub の capture は常に `xdg-desktop-portal` を経由する。共有する画面を選ぶダイアログ
を出しているのがこれである。選択結果は記憶されるため、ダイアログが表示されるのは初回の
share 時のみである。別の画面に変更する場合は、Host ページの *Choose screens again* を
使用する。

GNOME と KDE は主要なディストリビューションで portal backend を標準搭載しているため、
Ubuntu、Kubuntu、Fedora Workstation、Fedora KDE、openSUSE、および GNOME/KDE 上の Arch
では**追加の操作は不要**である。単体の window manager では導入が必要になる。

```bash
sudo apt install xdg-desktop-portal-wlr      # Debian 系の sway / river / Wayfire
sudo dnf install xdg-desktop-portal-wlr      # …Fedora の場合
sudo pacman -S xdg-desktop-portal-wlr        # …Arch の場合
```

sway、river、Wayfire はいずれも **wlroots** ライブラリ上に構築された Wayland
compositor である。GNOME/KDE と異なり自前の portal backend を持たず、`-wlr` がこの
3 つに screen capture を提供する backend にあたる。Hyprland には専用の
`xdg-desktop-portal-hyprland` がある。

### 2. VA-API driver

H.264 の encode は GPU 上で行う。software fallback はない。

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA では追加で: nvidia-vaapi-driver

# Fedora —— 標準の Mesa では H.264 が無効。使用可能な driver は RPM Fusion にある:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # 加えて GPU ベンダーの VA-API driver

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# 以上のうえで、すべてのディストリビューションで:
vainfo | grep -E 'H264.*Enc'                 # 1 行以上出力されること。出なければ host にはできない
```

### 3. `/dev/uinput` への書き込み権限

mouse と keyboard はここから inject される。deb と rpm は udev rule を導入するため、
追加の操作は不要である。portable binary の場合はコマンド 1 本で設定でき、clone も
デスクトップの再ログインも必要ない。

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

sudo に渡す前に内容を確認したい場合は、先に
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) をダウンロードするとよい。
十数行の短いスクリプトである。source を checkout 済みであれば、同等の操作は
`make setup-linux-permissions` である。

uinput の権限がなくても app は動作し、観ることもできる。このマシンへ mouse や
keyboard を inject できないだけである。

### Firewall

firewall を有効にしている場合は、UDP 47777 を開放する。

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### アンインストール

```bash
sudo apt remove deskhub      # または: sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # settings、key、pair 済みマシン
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # apt repository を追加した場合
```

portable binary はファイル 1 つであり、削除すればよい。

## 🤖 Android

host としては view-only の画面共有のみを行い、**Android 10+** を必要とする。観るだけで
あれば、より古いバージョンでも動作する。

**apk を直接インストール** —— [Releases](https://github.com/manhpham90vn/Deskhub/releases)
から `deskhub-v*-android.apk` をダウンロードしてインストールする。Google Play 版と同じ
key で sign されている。

**Play の beta** —— 3 段階で、いずれも端末の Play Store と**同じ Google アカウント**を
使用する。

1. テスターのグループに参加する: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. テスターとして登録する: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. インストールする（Play の同期に数分かかる）: [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

beta は **14 日以上**端末に残しておいてほしい。app を一般公開するために Google が求めて
いる条件である。

## 📱 iOS

ipa は sideload できないため、beta は TestFlight で配布している。

1. [TestFlight](https://apps.apple.com/app/testflight/id899247664) をインストールする。
2. beta に参加する: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Android と同様に、iPhone と iPad の host は view-only に限られる。app が自身の動作して
いる端末へ input を inject できるモバイル OS は存在しない。

---

## 💻 Command line

`deskhub-cli` は同じ client であり、自前のウィンドウを持たない点だけが異なる。画面を
共有し、remote shell を開き、スクリプトや SSH 経由で host を操作する。コマンドの一覧は
`deskhub-cli help` で表示する。読み書きする内容 —— settings、pair 済みマシン、trust
した host key —— はすべて app と共通であり、両者の状態は常に一致する。

| プラットフォーム | ファイル |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows-setup.exe` —— `deskhub-cli` をユーザーの `PATH` に追加 |
| 🍎 macOS | `deskhub-cli-v*-macos` —— Apple Silicon と Intel の両方に対応した binary 1 つ |
| 🐧 Linux | `deskhub-cli-v*-amd64.deb`、`deskhub-cli-v*-x86_64.rpm`、またはポータブル版 `deskhub-cli-v*-linux-x86_64` |

Linux のパッケージを直接ダウンロードした場合、Ubuntu/Debian では
`sudo apt install ./deskhub-cli-v*-amd64.deb`、Fedora では
`sudo dnf install ./deskhub-cli-v*-x86_64.rpm` でインストールする。デスクトップ app
をインストールしなくても、コマンドは `/usr/bin` から実行できる。

macOS と Linux のポータブル binary はダウンロード後に `chmod +x` が必要な場合がある。
`.deb` と `.rpm` は package manager でインストールする。macOS のポータブル binary は
dmg のように署名や notarize をしていないため、初回の実行時に
`xattr -d com.apple.quarantine deskhub-cli-v*-macos`、または System Settings →
Privacy & Security の *Open Anyway* が必要である。

Windows のインストーラーは管理者権限なしで CLI をユーザーの `PATH` に追加する。
ポータブル版 `deskhub-cli-v*-windows.exe` も利用できる。package manager 経由でも
`PATH` に配置される。Windows では
`winget install ManhPham.DeskhubCLI`、macOS では `brew install manhpham90vn/tap/deskhub-cli`
を使う。Homebrew は quarantine フラグを付けずにインストールする。Ubuntu や Debian では
上記の apt repository を追加した後、`sudo apt install deskhub-cli` を実行する。

Windows で winget からインストールした後は、新しい PowerShell ウィンドウを開いて
`deskhub-cli help` を実行する。`Get-Command deskhub-cli` で実際のファイルを確認できる。
直接ダウンロードしたポータブル版 exe は自動的に `PATH` に追加されない。

Linux では app と同じ portal および VA-API driver で画面を共有し、remote input も同じ
`/dev/uinput` rule に依存するため、[Linux](#-linux) の節がそのまま当てはまる。macOS で
は画面の共有と shell の起動はできるが、他のマシンを観ることはできない。`connect` には
command line build が持たない window layer が必要であり、その旨を報告する。その用途には
app を使用する。

---

## 🔒 画面を共有する前に

session が運ぶ内容 —— video、キー入力、mouse、clipboard、terminal のトラフィック ——
はすべて **QUIC/TLS** 上を通る。未知のマシンが受け入れられるのは pairing handshake を
通過した場合に限られる。**SPAKE2** によって host の passcode を知っていることを証明する
（passcode 自体は送信されず、1 つの connection につき試行は 1 回）か、host 側の利用者が
*Let this machine in?* に回答するかのいずれかである。

encrypt されていることと、Internet に露出してよいことは別である。port は discovery の
探索に応答し続けるし、面識のないマシンとの最初の pairing は初期的な信頼に基づく。
**信頼できる network** または **VPN** を優先すること。両方のマシンに
[Tailscale](https://tailscale.com) を導入し、`100.x.y.z` のアドレスに connect するのが
確実である。**UDP 47777 を port-forward してはならない。**

完全な threat model、保護される範囲、脆弱性の報告方法は
[`SECURITY.ja.md`](../SECURITY.ja.md) に記載している。

## 🆘 問題が起きたとき

- **接続先が見つからない** —— 2 台が同じ network（または同じ Tailscale tailnet）にあり、
  host 側で UDP 47777 が開いている必要がある。
- **Linux: share が直ちに失敗する** —— `vainfo | grep -E 'H264.*Enc'` を実行する。結果
  が空であれば、そのマシンに使用可能な H.264 encoder がなく、host にはできない。
- **Linux: ポインタが動かない** —— 要件 3 の `/dev/uinput` rule が導入されていない。
- **macOS: 画面が黒い、または input が効かない** —— Settings ページで Screen Recording
  と Accessibility を確認する。
- **その他の場合** —— [issue](https://github.com/manhpham90vn/Deskhub/issues) を作成し、
  端末の機種、OS のバージョン、Host または Client ページの status 行の内容を添えてほしい。
