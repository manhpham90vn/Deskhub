[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · [中文](INSTALL.zh.md) · **日本語**

# Deskhub — インストール

お使いのプラットフォームを選び、build 済みの release をインストールできる。source を
checkout する必要はない。自分でコンパイルする場合は [`BUILD.ja.md`](BUILD.ja.md) を参照。

デスクトップのインストーラーと Android APK は
[Releases ページ](https://github.com/manhpham90vn/Deskhub/releases) から入手できる。
モバイルの beta は TestFlight または Google Play でも配布している。

本書は [`INSTALL.md`](INSTALL.md) の翻訳。食い違いがある場合は英語版が正文。

| プラットフォーム | ファイル | インストール手順 |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows-setup.exe` | ダウンロードしてインストールし、スタートメニューまたはデスクトップから起動 |
| 🍎 macOS | `deskhub-v*-macos.dmg` | dmg を開き、Deskhub を Applications にドラッグ |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation と KDE spin） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE Tumbleweed | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch およびその他のディストリビューション | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | apk をインストール、または Play の beta に参加 |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

terminal からの操作やスクリプトには `deskhub-cli` も使える。Windows と Linux では、
CLI からリモート画面のウィンドウも開ける（Linux では X11 のウィンドウなので、Wayland の
デスクトップでは XWayland が必要）。[Command line](#-command-line) を参照。

Windows では `winget`、macOS では Homebrew、Ubuntu・Kubuntu・Debian・Mint では
[apt repository](#-linux) からもインストールできる。新しい release が必要になったら、
それぞれの package manager の更新コマンドを使う。

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

次の 2 点に注意する。

- **起動のたびに Administrator を要求する。** 起動するごとに Windows が UAC の確認を表示
  する。これがないと、昇格したウィンドウへ mouse と keyboard を inject できないためである。
  *Start Deskhub when you log in* をオンにすると、サインイン時に昇格したログオンタスクと
  して起動し、確認は表示されない。
- **Windows Firewall のルールを 1 本追加する。** 初回の share 時に app 自身が追加する。

DeskHub は Windows の設定または `winget uninstall ManhPham.Deskhub` でアンインストールできる。
ポータブル版は exe を削除する。Settings と key は、フォルダを削除するまで
`%USERPROFILE%\.deskhub` に残る。他のマシンから届いたファイルも、削除するまで
`%USERPROFILE%\Deskhub`（または選んだフォルダ）に残る。

## 🍎 macOS

Deskhub には macOS 14 Sonoma 以降が必要である。

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

他のマシンを観るだけであれば、どちらも不要である。System Settings で Screen Recording
をオンにした後は、Deskhub を終了して開き直す。macOS はこの許可を新しく起動したプロセスに
しか適用せず、Settings ページにもその旨が表示される。

このほか macOS 自身が 2 つの確認を出す。**Local Network**（ローカルネットワーク）への
アクセスは、共有でも connect でも相手のマシンに届くために必要である。**通知**の許可は、
connection request を知らせる必要が初めて生じたときに求められる。

## 🐧 Linux

**connect して画面を見るなら、まず app の package をインストールする。** H.264
decoder は app に含まれるため、FFmpeg の package を別途入れる必要はない。GTK3、
PipeWire、libva など、app が使うデスクトップ向けライブラリは OS 側に必要となる。

deb と rpm の内容は同一であり、利用中の package manager が扱えるほうを選べばよい。
どちらにも下記の要件 3 で説明する `/dev/uinput` の udev rule が含まれるため、インストール
直後から remote input が動作する。group の変更も再ログインも不要である。deb、rpm、
portable を問わず、すべての Linux 版は glibc 2.35 以上の x86_64 を必要とする（Ubuntu
22.04、Debian 12、Fedora 36、openSUSE Tumbleweed、現行の Arch）。openSUSE Leap 15.5 の
glibc は 2.31 のため動作しない。

CLI には別の deb と rpm があり、単独で、または同じかより新しいバージョンのデスクトップ
app と一緒にインストールできる。package manager は古い `deskhub` を新しい `deskhub-cli`
と並べて残さないため、両方をまとめて更新する。
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
share 時のみである。記憶済みの選択に含まれない画面を Host ページでチェックすると、
Deskhub はその選択を忘れ、ダイアログが自動的に再び表示される。手動で選択を消すには
`deskhub-cli displays --forget` を実行する。

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

### 2. ハードウェア H.264 encoder

H.264 の encode は GPU 上で行う。software fallback はない。host が使う encoder は、
デスクトップを描画している GPU によって決まる。

- **プロプライエタリ driver の NVIDIA** —— NVENC。driver 自身のライブラリを使い、driver
  が NVENC API 13.0 以降に対応していればよい。追加のインストールは不要で、こうしたマシン
  では `vainfo` が H.264 encoder を 1 つも示さなくても host になれる。
  `nvidia-vaapi-driver` と `libva-nvidia-driver` は decode 専用であり、観る側を速くするが
  共有には役立たない。
- **AMD、Intel、その他すべての GPU** —— VA-API。H.264 encoder を持つ driver が必要である。

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA（decode 専用）: nvidia-vaapi-driver

# Fedora —— 標準の Mesa では H.264 が無効。使用可能な driver は RPM Fusion にある:
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD (RPM Fusion)
sudo dnf install intel-media-driver          # Intel (RPM Fusion)
sudo dnf install nvidia-vaapi-driver         # NVIDIA、decode 専用 (RPM Fusion)

# openSUSE
sudo zypper install libva-utils              # 加えて GPU ベンダーの VA-API driver

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA（decode 専用）: libva-nvidia-driver

# 以上のうえで、AMD または Intel では:
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
`make setup-linux-permissions` である。このスクリプトは rule に加えて、sudo で実行した
ユーザーを `input` グループに追加する。これは次回のログインから有効になり、SSH や
headless の session が `/dev/uinput` に届くために必要で、host wins（後述）も有効にする。

uinput の権限がなくても app は動作し、観ることもできる。このマシンへ mouse や
keyboard を inject できないだけである。

<a id="host-wins"></a>

### マシンの前にいる人を優先させる

このマシン自身の mouse や keyboard を使っている間、Deskhub は remote input を一時停止す
る（"host wins"）。それを検知するために `/dev/input/event*` を読むが、これができるのは
`input` グループのメンバーだけであり、deb も rpm も誰もこのグループに追加しない。グループ
がなくても共有と remote input は動作するが、入力中に viewer の input は止まらない。有効に
するには次を実行する。

```bash
sudo usermod -aG input "$USER"               # その後ログアウトして再ログイン
```

`input` グループに入ると、Deskhub に限らず実行するすべてのプログラムが、このマシンの
すべての keyboard と mouse を読めるようになる。host wins よりそちらが重要な環境では
有効にしないこと。

### Firewall

firewall を有効にしている場合は、UDP 47777 を開放する。

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### アンインストール

```bash
sudo apt remove deskhub deskhub-cli   # インストールしたもの。または dnf remove / zypper remove
rm -rf ~/.deskhub            # settings、key、許可済み client、信頼済み host
sudo rm -f /etc/apt/sources.list.d/deskhub.list /etc/apt/keyrings/deskhub.gpg   # apt repository を追加した場合
```

他のマシンから届いたファイルは、削除するまで `~/Deskhub`（または選んだフォルダ）に残る。
portable binary はファイル 1 つであり、削除すればよい。`setup-uinput.sh` は udev rule、
module-load ファイル、`input` グループへの所属を残すので、ほかに必要なければ手動で削除
する。

```bash
sudo rm -f /etc/udev/rules.d/60-deskhub-uinput.rules /etc/modules-load.d/deskhub.conf
sudo gpasswd -d "$USER" input
```

## 🤖 Android

Deskhub には **Android 8.0** 以降が必要である。host としては view-only の画面共有のみを
行い、**Android 10+** を必要とする。

**apk を直接インストール** —— [Releases](https://github.com/manhpham90vn/Deskhub/releases)
から `deskhub-v*-android.apk` をダウンロードしてインストールする。Google Play 版の上に
（またはその逆に）インストールできないと Android に拒否された場合は、先にもう一方を
アンインストールする。その key も削除されるため、各 host で改めて許可してもらう必要がある。

**Play の beta** —— 3 段階で、いずれも端末の Play Store と**同じ Google アカウント**を
使用する。

1. テスターのグループに参加する: [groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. テスターとして登録する: [play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. インストールする（Play の同期に数分かかる）: [play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

beta は **14 日以上**端末に残しておいてほしい。app を一般公開するために Google が求めて
いる条件である。Play の beta は Releases の apk より遅れることがある。各 release はまず
Google Play の internal track に入り、promote されて初めて beta に届く。

## 📱 iOS

Deskhub には iOS または iPadOS 17 以降が必要である。ipa は sideload できないため、beta
は TestFlight で配布している。

1. [TestFlight](https://apps.apple.com/app/testflight/id899247664) をインストールする。
2. beta に参加する: **[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Android と同様に、iPhone と iPad の host は view-only に限られる。app が自身の動作して
いる端末へ input を inject できるモバイル OS は存在しない。

---

## 💻 Command line

`deskhub-cli` では画面の共有や remote shell の起動をコマンドで行える。スクリプトや
SSH からも利用可能。Windows と Linux の `connect` はリモート画面のウィンドウを開く。
Linux では X11 のウィンドウであり、Wayland のデスクトップでは XWayland 経由で表示される。
macOS で画面を見る場合はデスクトップ app を使う。コマンド一覧は `deskhub-cli help`
で確認できる。settings、マシンの key、許可済み client、接続要求、信頼済み host は app と
共通である。

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
ポータブル版 `deskhub-cli-v*-windows.exe` も利用できる。
すべてのインストーラーとパッケージは `THIRD_PARTY_NOTICES.md` と、中に含まれる
ライブラリのライセンス本文を同梱する。ポータブル版のバイナリにはそれができないため、
各リリースは `LICENSE`、`THIRD_PARTY_NOTICES.md` とすべてのライセンス本文を収めた
`deskhub-v*-licenses.zip` も提供する。package manager 経由でも
`PATH` に配置される。Windows では
`winget install ManhPham.DeskhubCLI`（winget はインストーラーではなくポータブル版 exe を
入れる）、macOS では `brew install manhpham90vn/tap/deskhub-cli`
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
はすべて **QUIC/TLS** 上を通り、アクセスは SSH と同じ仕組みで行われる。host は自分の
リストにある key を持つデバイスだけを通し、すべてのデバイスは key を 1 つ持つ。デバイスを
そのリストに載せる方法は 3 つあり、どれか 1 つで足りる。

- **QR code をスキャンする。** host が共有している間に、アドレス一覧の横の
  **Show QR code** を押す。スマートフォンでは Client ページの **Scan QR code** を押して画面に向ける。
  それ以外のデバイスでは、code の下のリンクをコピーしてアドレス欄に貼り付ける。デバイスは
  一度の手順で信頼・許可・接続される。
- **要求を承認する。** デバイスで host のアドレスを入力し *Connect* を押す。host は
  *Connection request* の通知を表示し、Host ページと Devices ページの **Connection requests** に、名前、
  key の fingerprint、アドレスとともに一覧する。**Approve** を押せば、デバイスは次の試行で接続される —— デバイスは 2 分間、
  自動的に再試行を続ける。
- **key を貼り付ける。** デバイスで **Devices** → **Copy public key**。host で **Devices**
  → **Devices allowed to connect to this machine** → *Allow*、貼り付けて完了。

アドレスで初めて Connect すると、**New host** ダイアログが host の key の fingerprint を
表示する。host の Devices ページの **This machine** にある fingerprint と照合したうえで、
*Trust and connect* を押す。app で QR code をスキャンした場合は、code が fingerprint を含む
ため、このダイアログを省く。以後、その host は **Trusted hosts** に表示され、アドレスが変わっても信頼されたままである。

各方法を app と CLI の両方で説明し、スクリプトと取り消しも扱う
[Key とアクセス](#-key-とアクセス) を参照。

passcode も、未知のマシンを受け入れるスイッチも存在しない。自分の *Approve*、自分が
見せた QR code、または自分が貼り付けた key なしに接続できる者はいない。Deskhub が
network を scan することもない。

Deskhub は**信頼できる network** または **VPN** 上で使い、UDP 47777 を
port-forward しないこと。Encrypt は session の内容を守るが、host への初回の
Connect では、fingerprint を照合しない限り、提示された key がそのまま信頼される。遠隔から
アクセスする場合は、両方のマシンに [Tailscale](https://tailscale.com) を導入し、
`100.x.y.z` のアドレスへ Connect できる。

完全な threat model、保護される範囲、脆弱性の報告方法は
[`SECURITY.ja.md`](../SECURITY.ja.md) に記載している。

## 🔑 Key とアクセス

Deskhub は SSH と同じく key ペアでサインインする。すべてのマシンは Deskhub の初回起動時に
作られる **1 つの key** を持ち、その key が、共有中でも connect 中でも、そのマシンそのもの
である。connect するときに照合する fingerprint と、host が受け入れる際に保存する public
key は同じ key である。host は自分のリストにある key だけを通し、client は key を信頼済みの
host にだけ connect する。操作はすべて **Devices** ページにあり、**This machine**、**Connection requests**、
**Devices allowed to connect to this machine**、**Trusted hosts** の 4 節に分かれている。以下の各手順には対応する
`deskhub-cli` コマンドがある。app と CLI は同じファイルを読むので、一方での変更はもう一方
にも反映される。

### 自分の key

**Devices** → **This machine** に本デバイスの名前と key の fingerprint `SHA256:…` が *Copy* ボタン付きで表示される。自分に connect してくる人が照合
するのはこれである。その横の **Copy public key**（CLI: `deskhub-cli key public`）は
`ecdsa-sha2-nistp256 AAAA… laptop` のような 1 行をコピーする。末尾のラベルはこのデバイスの
名前で、**Settings** → *General* → **Device name** で設定する。host の所有者はこれで誰の
key かを見分けられる。QR code も要求も都合が悪いときに host の所有者へ渡すのがこの行で
ある。共有しても安全である。private の側がマシンの外に出ることはなく、生成や import は
不要で、key が勝手に置き換えられることもない。`host_key.pem` を削除するとマシンは新しい
identity になる。host には改めて受け入れてもらう必要があり、このマシンを信頼していた
デバイスには新しい host として見える。

### デバイスの接続を許可する

判断するのは host の所有者で、方法は 3 つある。どれを使っても、デバイスは **Devices** →
**Devices allowed to connect to this machine** に名前を
ラベルとして表示される。横の *Remove* で外せる。*Remove every client* は確認のうえで
リストを空にする。

**QR code で接続する** —— 最も速く、fingerprint の照合が不要な唯一の方法。host が共有
している間に、アドレス一覧の横の **Show QR code** を押す。code には host のアドレスと
port、key の fingerprint、名前、そして 5 分間有効なランダムな 1 回限りの token が含まれる。

- スマートフォンやタブレットでは、Client ページを開いて **Scan QR code** を押す。初回は
  system がカメラの permission を求める。Deskhub がカメラを使うのはここだけで、frame は
  端末上で decode し、何も保存しない。system のカメラやメッセージから
  `deskhub://pair/…` リンクを開くこともできるが、そうしたリンクはどこからでも届きうるため、
  その host がまだ信頼されていなければ、app はまずリンク内の fingerprint を示す
  **New host** ダイアログを表示し、照合して *Trust and connect* を押したときにだけ接続
  する。既に信頼している host にはそのまま接続する。
- デスクトップを含むどのデバイスでも、code の下に表示されるリンクをコピーしてアドレス欄に
  貼り付け、*Connect* を押す（CLI: `deskhub-cli connect 'deskhub://pair/…'`。`sources`、
  `shell`、`send` もこのリンクを受け取る）。

デバイスは、応答したマシンが code に印字された key を保持していることを確認する ——
そのアドレスで別のマシンが応答した場合は停止し、QR code はそのマシンのものではないと
告げる —— その後 host を信頼し、token を送り、一度の手順で受け入れられて接続される。
各 code は一度だけ使える。**Hide QR code** を押すか共有を停止すると、誰も使っていなくても
無効になる。すでに許可されているデバイスは、host の現在のアドレスを取得するためだけに
code をスキャンすることもできる。

**接続要求を承認する** —— デバイスが同じ部屋にないとき。

1. デバイスで host のアドレス（`192.168.1.10`、host が 47777 以外を使う場合は
   `192.168.1.10:PORT`）を入力し、*Connect* を押す。**New host** ダイアログを後述のとおり
   確認する。ページには host の所有者の承認を待っている旨が表示され、2 分間自動的に
   再試行を続ける。*Cancel* で中止できる。
2. host では、Host ページの **Connection requests** にそのデバイスが表示される —— 名前、
   fingerprint の先頭部分、アドレスに、**Approve** と **Deny** が添えられる。fingerprint が
   デバイスの Devices ページのものと一致し、アドレスがデバイスのあるはずの場所であること
   を確認したうえで、*Approve* を押す。デバイスは次の試行で接続される。*Deny* は要求を
   破棄する。デバイスには、時間内に誰も承認しなかったとだけ伝わる。
3. 要求は 10 分間残り、host は最大 16 件を保持する。デバイスが諦めた後に承認した場合は、
   もう一度 *Connect* を押すだけでよい。

command line では、`deskhub-cli access requests`（スクリプト向けには `--json` を付ける）
が待機中の要求を一覧し、`deskhub-cli access approve --fingerprint SHA256:…` が受け入れ、
`deskhub-cli access deny --fingerprint SHA256:…` が破棄する。実行中の `deskhub-cli share`
は、新しい要求が届くたびに、別の terminal に貼り付けるための approve コマンドとともに
表示する。

**key を貼り付ける** —— key を自分で運びたいとき。

1. connect する人に、その Devices ページで **Copy public key** を押してもらい（[自分の
   key](#自分の-key) を参照）、その行を送ってもらう。
2. **Devices** → **Devices allowed to connect to this machine** を開き、その行を貼り付けて *Allow* を押す。受け付けるのは Ed25519 と
   ECDSA P-256 の public key だけである。

command line では、その行を `access add` に pipe する。

```sh
deskhub-cli key public                                         # デバイスで
deskhub-cli access add --stdin                                 # host で: 貼り付けて Ctrl-D
deskhub-cli key public | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # host で
deskhub-cli access remove --fingerprint SHA256:…               # host で
```

`deskhub-cli access clear` は *Remove every client* と同じ働きをする。

### 初めて connect する

QR code はこれを代わりに行う。アドレスで初めて connect する場合は SSH と同じ流れになる。

1. client で host のアドレスを入力し、*Connect* を押す。
2. **New host** ダイアログが host の key の fingerprint（`SHA256:…`）を表示する。その
   アドレスが以前は信頼済みの別の host のものだった場合、ダイアログはその旨を告げ、その
   host の名前を示す —— 既知のアドレスにある別の key は別のマシンなので、どのマシンが
   応答しているのかを確かめてから先へ進むこと。
3. host で **Devices** → **This machine** を開く（fingerprint 横の *Copy* でクリップ
   ボードにコピーできる）。2 つの fingerprint を、すでに信頼して
   いる経路 —— 対面、電話、相手本人と分かっているチャットなど —— で照合する。
4. 一致すれば *Trust and connect* を押す。一致しなければ *Cancel* を押す。

その host は名前、fingerprint、最後に応答したアドレスとともに **Trusted hosts** に表示
される。信頼はアドレスではなく key に従う。host のアドレスが変わったら、新しいアドレスを
入力して connect すればよい —— ダイアログも新たな承認も不要で、一覧は次回に向けて新しい
アドレスを記憶する。

**host を事前に固定する。** CLI を使えば、初回接続の前に host key を保存でき、ダイアログは
不要になる。

```sh
deskhub-cli host-key public                                    # host で
deskhub-cli host add office --address 192.168.1.10 --host-key-stdin
```

2 つ目のコマンド（client 側）に host の行を貼り付け、Ctrl-D を押す。`office` は自由に
決められる alias で、`connect office`、`sources office`、`shell office`、
`send office FILE` のいずれでも使える。`deskhub-cli host list` は保存済みのすべての host
を表示する。app で信頼した host も含まれ、それらには `192-168-1-10-47777` のようにアドレス
から作られた alias が付く。`host update ALIAS` はアドレス（`--address`）または固定した
host key（`--host-key-stdin`）を変更し、`host remove ALIAS` はその host を削除する。
`deskhub-cli trust forget` も fingerprint、alias、または最後のアドレスを指定して同じことを
行う。

### スクリプトと command line

- **未知の host は拒否される。** `sources`、`connect`、`shell`、`send` は key が保存されて
  いない host とは通信せず、代わりにその fingerprint を表示する。host と照合したうえで
  `--accept-new-host-key` を付けて再実行すると保存される。アドレスの代わりに招待リンクを
  渡した場合は、リンクから host を固定するのでフラグは不要である。
- **承認を待つ。** このマシンをまだ許可していない host には承認を求める。`connect`、
  `shell`、`send`、`sources` はその旨を 1 行表示し、所有者が *Approve* を押すまで最長
  2 分間待ち、誰も押さなければ理由を示して失敗する。
- **code を表示する。** `deskhub-cli share --qr` は、スマートフォンが terminal から
  スキャンできるように、QR code をブロック文字で招待リンクとともに表示する。
- **別の設定を使う。** `--config-dir DIR`（コマンドの前後どちらにも置ける）または環境変数
  `DESKHUB_CONFIG_DIR` で、key、許可済み client、要求、信頼済み host、settings を別の
  ディレクトリから読ませられる。service account やテスト環境に便利である。
- **8.0 で削除されたもの。** `key generate`、`key import`、`key delete`、`key list`、
  `key public --name`、`devices identities`、`--identity`、`host add --identity` はなくなった。
  マシンごとに key は 1 つである。exit code `5`（"the host key changed"）もなくなった。
  別の key は未知の host として扱われるためである。
- **exit code。** スクリプトは exit code で、接続の拒否をほかの失敗と区別できる。各コードは
  `BUILD.ja.md` の [Command line client](BUILD.ja.md#command-line-client) に記載している。
  一覧系のコマンドは `--json` に対応している。

### デバイスのアクセスを取り消す

host でその key の横の *Remove* を押すか、
`deskhub-cli access remove --fingerprint SHA256:…` を実行する。そのデバイスが開いている
session はすぐに終了し、改めて受け入れるまで —— 次の要求を承認する、QR code を見せる、
または key を貼り付ける —— connect できない。

**既知のアドレスで別の key が応答したとき。** host が Deskhub を入れ直したか settings
フォルダを失った場合に起こる。あるいは、別のマシンがそのアドレスを引き継いだ場合もある。
Deskhub は拒否しない。そのマシンを一度も会ったことのないものとして扱い、以前そこで応答して
いた host の名前を示す警告とともに **New host** ダイアログを表示する。理由を把握している
場合に限り信頼すること。以前の host は *Remove*（または `deskhub-cli host remove ALIAS`）
するまで **Trusted hosts** に残る。CLI では `host update ALIAS --host-key-stdin` で alias を
新しい key に直接固定し直せる。

### 以前の Deskhub から移行する

**8.0.0 から。** 9.0 は 8.0.0 より新しい認証バージョンを使うため、どちらか一方でも
8.0.0 の Deskhub だと connect できず、"That machine uses an incompatible authentication
version" として拒否される。両方のマシンを更新すること。それ以外は何も変わらない ——
key、許可したデバイス、信頼した host はすべて引き継がれる。

**7.0.x から。** 両方のマシンに 9.0 が必要である。どちらか一方でも 7.0.x の Deskhub だと
connect できず、"That machine uses an incompatible authentication version" として拒否
される。各マシンはすでに持っていた key と fingerprint をそのまま保つので、信頼していた
host は信頼されたままである。変わるのはデバイスがサインイン*する*key で、これが同じ
マシンの key になったため、すべてのデバイスを改めて受け入れる必要がある —— *Approve*
1 回、QR code のスキャン 1 回、または key の貼り付け 1 回。7.0.x が書き込んだ
`client_key*.pem` と `host_cert.pem` は無視される。その内容は読み取られず移行もされない
ので、削除してよい。

**6.x 以前から。** 移行されるものはない。passcode とペアリング済みデバイスのリストは引き
継がれない。上の手順どおりに各デバイスを受け入れ、各 host を改めて信頼すること。

## 🆘 問題が起きたとき

- **接続先が見つからない** —— 2 台が同じ network（または同じ Tailscale tailnet）にあり、
  host 側で UDP 47777 が開いている必要がある。
- **"Waiting for the owner of … to approve this device"** —— host がこのデバイスをまだ
  記載していない。所有者に、Host ページの **Connection requests** で *Approve* を押して
  もらうか、QR code を見せてもらうか、自分の key を貼り付けてもらう
  （[デバイスの接続を許可する](#デバイスの接続を許可する) を参照）。7.0.x で許可されて
  いたデバイスは、改めて受け入れる必要がある。
- **"The owner of that machine did not approve this device in time"** —— *Approve* なしに
  2 分が経過した。要求は host に 10 分間残るので、依頼したうえで再び *Connect* を押す。
- **"The machine that answered is not the one that made this QR code"** —— code に含まれる
  アドレスで別のものが応答している。host で code を再表示してもう一度スキャンする。繰り
  返し起こる場合は、どのマシンがそのアドレスを持っているかを確認する。
- **すでに信頼している host に対する New host ダイアログ** —— そのアドレスで別の key が
  応答している。[デバイスのアクセスを取り消す](#デバイスのアクセスを取り消す) を参照。
- **"That machine uses an incompatible authentication version"** —— どちらかが以前の
  Deskhub を使っている。両方のマシンを更新する。[以前の Deskhub から移行する](#以前の-deskhub-から移行する)
  を参照。
- **Linux: share が直ちに失敗する** —— AMD または Intel では
  `vainfo | grep -E 'H264.*Enc'` を実行する。結果が空であれば、そのマシンに使用可能な
  H.264 encoder がなく、host にはできない。NVIDIA では、`~/.deskhub` の log に driver の
  NVENC が古すぎるかどうかが記録される。
- **Linux: 入力中も viewer が操作を続ける** —— ユーザーが `input` グループに入っていない。
  [マシンの前にいる人を優先させる](#host-wins) を参照。
- **Linux: ポインタが動かない** —— 要件 3 の `/dev/uinput` rule が導入されていない。
- **macOS: 画面が黒い、または input が効かない** —— Settings ページで Screen Recording
  と Accessibility を確認する。
- **その他の場合** —— [issue](https://github.com/manhpham90vn/Deskhub/issues) を作成し、
  端末の機種、OS のバージョン、Host または Client ページの status 行の内容を添えてほしい。
