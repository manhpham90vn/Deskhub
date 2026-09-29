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
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch およびその他のディストリビューション | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | apk をインストール、または Play の beta に参加 |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

terminal からの操作やスクリプトには `deskhub-cli` も使える。Windows と Linux では、
CLI からリモート画面のウィンドウも開ける。[Command line](#-command-line) を参照。

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

**connect して画面を見るなら、まず app の package をインストールする。** H.264
decoder は app に含まれるため、FFmpeg の package を別途入れる必要はない。GTK3、
PipeWire、libva など、app が使うデスクトップ向けライブラリは OS 側に必要となる。

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
rm -rf ~/.deskhub            # settings、key、許可済み client、信頼済み host
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

`deskhub-cli` では画面の共有や remote shell の起動をコマンドで行える。スクリプトや
SSH からも利用可能。Windows と Linux の `connect` はリモート画面のウィンドウを開く。
macOS で画面を見る場合はデスクトップ app を使う。コマンド一覧は `deskhub-cli help`
で確認できる。settings、client key、許可済み client、信頼済み host は app と共通である。

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
はすべて **QUIC/TLS** 上を通り、アクセスは SSH と同じ仕組みで行われる。デバイスが
connect できるようにするには、次のようにする。

1. connect する側のデバイスで **Devices** → *When this machine is the client* →
   **My keys** を開き、*Copy public key* を押す（CLI: `deskhub-cli key public --name default`）。
2. host で **Devices** → *When this machine is the host* → **Clients allowed to
   connect to this machine** を開き、*Allow* を押してその key を貼り付ける（CLI:
   `deskhub-cli access add --stdin`）。
3. アドレスで Connect する。初回は **New host** ダイアログが host の key の fingerprint
   を表示する。host の Devices ページの **This machine's host key** と照合したうえで、
   *Trust and connect* を押す。以後、その host は **Trusted hosts** に表示される。

各手順を app と CLI の両方で説明し、key の追加、スクリプト、取り消しと入れ替えも扱う
[Key とアクセス](#-key-とアクセス) を参照。

network 越しに何かを承認することはなく、Deskhub が network を scan することもない。
信頼済み host の key が変わった場合、接続は即座に拒否される。key が変わった理由を把握
している場合に限り、その host を *Trusted hosts* から削除し、改めて信頼すること。

Deskhub は**信頼できる network** または **VPN** 上で使い、**UDP 47777 を
port-forward しないこと**。Encrypt は session の内容を守るが、host への初回の
Connect では、fingerprint を照合しない限り、提示された key がそのまま信頼される。遠隔から
アクセスする場合は、両方のマシンに [Tailscale](https://tailscale.com) を導入し、
`100.x.y.z` のアドレスへ Connect できる。

完全な threat model、保護される範囲、脆弱性の報告方法は
[`SECURITY.ja.md`](../SECURITY.ja.md) に記載している。

## 🔑 Key とアクセス

Deskhub は SSH と同じく key ペアでサインインする。connect する各デバイスは **client key**
を持ち、各 host は **host key** を持つ。host は自分のリストにある client key だけを通し、
client は key を信頼済みの host にだけ connect する。操作はすべて **Devices** ページにあり、
*When this machine is the host* と *When this machine is the client* に分かれている。
以下の各手順には対応する `deskhub-cli` コマンドがある。app と CLI は同じファイルを読むので、
一方での変更はもう一方にも反映される。

### 自分の key

自分の key は **Devices** → *When this machine is the client* → **My keys** にある。
private の側がマシンの外に出ることはない。

- **`default` key。** app は Devices ページを初めて開いたときに `default` という key を
  作成し、CLI はどこかへ初めて connect したとき、または
  `deskhub-cli key public --name default` で表示したときに作成する。ほとんどの場合、ほかの
  key は必要ない。
- **key を追加する。** *New key* を押して名前を付ける（CLI:
  `deskhub-cli key generate --name NAME`）。ある host のアクセスだけをほかに影響を与えずに
  取り消したい場合は、別の key を使う。
- **既存の key を使う。** *Import key…* を押し、private key のファイルを選んで名前を付け、
  ファイルが暗号化されていれば passphrase を入力する（CLI:
  `deskhub-cli key import --name NAME --file PATH`。passphrase を標準入力から読むには
  `--passphrase-stdin` を付ける）。Deskhub が読めるのは、Ed25519 または ECDSA P-256 の
  key を含む OpenSSH（`ssh-keygen`）形式と PKCS#8 形式のファイルである。RSA には対応して
  いない。passphrase は import 時にファイルを開くためだけに使われ、保存されず、host へ
  送られることもない。
- **自分の public key。** key の横の *Copy public key* を押す（CLI:
  `deskhub-cli key public --name NAME`）。`ssh-ed25519 AAAA… laptop` のような 1 行が得られ
  る。末尾のラベルはこのデバイスの名前で、**Settings** → *General* → **Device name** で
  設定する。host の所有者はこれで誰の key かを見分けられる。この行を host の所有者に送る。
  共有しても安全である。
- **key を削除する。** 横の *Delete* を押す。`default` key は削除できず、信頼済み host が
  まだ使っている key も削除できない。先にその host を別の key に切り替えること。削除した
  private key は復元できない。CLI では `deskhub-cli key list` で key を一覧し、
  `deskhub-cli key delete --name NAME` で削除できる（同じ制約が適用される）。

### デバイスの接続を許可する

host で次のようにする。

1. connect する人に public key の行をもらう（[自分の key](#自分の-key) を参照）。
2. **Devices** → *When this machine is the host* → **Clients allowed to connect to this
   machine** を開き、その行を貼り付けて *Allow* を押す。受け付けるのは Ed25519 と
   ECDSA P-256 の public key だけである。
3. key がラベル付きでリストに表示される。key の横の *Remove* で外せる。*Remove every
   client* は確認のうえでリストを空にする。

command line では、その行を `access add` に pipe する。

```sh
deskhub-cli key public --name default                          # client で
deskhub-cli access add --stdin                                 # host で: 貼り付けて Ctrl-D
deskhub-cli key public --name default | ssh me@host deskhub-cli access add --stdin
deskhub-cli access list                                        # host で
deskhub-cli access remove --fingerprint SHA256:…               # host で
```

`deskhub-cli access clear` は *Remove every client* と同じ働きをする。

### 初めて connect する

1. client で host のアドレス（`192.168.1.10`、host が 47777 以外を使う場合は
   `192.168.1.10:PORT`）を入力し、*Connect* を押す。
2. **New host** ダイアログが host key の fingerprint（`SHA256:…`）を表示する。
3. host で **Devices** → *When this machine is the host* → **This machine's host key**
   を開く（*Copy* でクリップボードにコピーできる）。2 つの fingerprint を、すでに信頼して
   いる経路 —— 対面、電話、相手本人と分かっているチャットなど —— で照合する。
4. 一致すれば *Trust and connect* を押す。一致しなければ *Cancel* を押す。

その host は、使用する client key とともに **Trusted hosts** に表示され、以後の接続では
ダイアログが出ない。

**host を事前に固定する。** CLI を使えば、初回接続の前に host key を保存でき、ダイアログは
不要になる。

```sh
deskhub-cli host-key public                                    # host で
deskhub-cli host add office --address 192.168.1.10 --identity default --host-key-stdin
```

2 つ目のコマンド（client 側）に host の行を貼り付け、Ctrl-D を押す。`office` は自由に
決められる alias で、`connect office`、`sources office`、`shell office`、
`send office FILE` のいずれでも使える。`deskhub-cli host list` は保存済みのすべての host
を表示する。app で信頼した host も含まれ、それらには `192-168-1-10-47777` のようにアドレス
から作られた alias が付く。`host update ALIAS` はアドレス（`--address`）、client key
（`--identity`）、固定した host key（`--host-key-stdin`）を変更し、`host remove ALIAS` は
その host を削除する。app は新しい host に `default` で connect する。ある host に別の key
を使うには、ここで `--identity` で設定する。app もその key を使うようになる。

### スクリプトと command line

- **未知の host は拒否される。** `sources`、`connect`、`shell`、`send` は key が保存されて
  いない host とは通信せず、代わりにその fingerprint を表示する。host と照合したうえで
  `--accept-new-host-key` を付けて再実行すると保存される。このフラグが保存するのは初めて
  見た key だけで、key が *変わった* host は常に拒否される。
- **key を選ぶ。** `--identity NAME` はそのコマンドで使う client key を選ぶ。指定しない
  場合は保存済み host の key が使われ、それがない host では `default` が使われる。
- **別の設定を使う。** `--config-dir DIR`（コマンドの前後どちらにも置ける）または環境変数
  `DESKHUB_CONFIG_DIR` で、key、許可済み client、信頼済み host、settings を別のディレクトリ
  から読ませられる。service account やテスト環境に便利である。
- **exit code。** スクリプトは exit code で、接続の拒否や host key の変更をほかの失敗と
  区別できる。各コードは `BUILD.ja.md` の
  [Command line client](BUILD.ja.md#command-line-client) に記載している。一覧系のコマンドは
  `--json` に対応している。

### key の取り消しと入れ替え

**デバイスのアクセスを取り消す。** host でその key の横の *Remove* を押すか、
`deskhub-cli access remove --fingerprint SHA256:…` を実行する。そのデバイスが開いている
session はすぐに終了し、key が再び許可されるまで connect できない。

**client key を入れ替える。**

1. 新しい key を作る。*New key*、または `deskhub-cli key generate --name NAME`。
2. その key を受け入れるべき各 host で、public key を許可してもらう。
3. 信頼済み host が新しい key を使うようにする。`deskhub-cli host update ALIAS --identity NAME`、
   または host を削除してから `host add … --identity NAME` で追加し直す。一度 connect して
   確認する。
4. host の所有者に古い key を外してもらい、このマシンで古い key を *Delete* する。

**host の key が変わったとき。** Deskhub は connect を拒否し、"This host's key has changed"
と表示する。host が Deskhub を入れ直したか settings ディレクトリを失った場合に起こる。
あるいは、そのアドレスで別のマシンが応答している可能性もある。key が変わった理由を把握して
いる場合に限り、その host を **Trusted hosts** から削除し（*Remove*、または
`deskhub-cli host remove ALIAS`）、改めて connect して、初回と同じように新しい fingerprint
を照合する。CLI では `host update ALIAS --host-key-stdin` で新しい key を直接固定できる。

### 以前の Deskhub から移行する

移行されるものはない。以前の Deskhub の passcode とペアリング済みデバイスのリストは引き
継がれない。上の手順どおりに key を作成またはコピーし、各 host で許可し、各 host を改めて
信頼すること。両方のマシンにこのバージョンが必要である。どちらか一方でも以前の Deskhub
だと connect できず、"That machine uses an incompatible authentication version" として
拒否される。

## 🆘 問題が起きたとき

- **接続先が見つからない** —— 2 台が同じ network（または同じ Tailscale tailnet）にあり、
  host 側で UDP 47777 が開いている必要がある。
- **"This device's key is not authorized on that machine yet"** —— host がこの client key を
  記載していない。host の Devices ページでその public key を許可する
  （[デバイスの接続を許可する](#デバイスの接続を許可する) を参照）。以前の Deskhub で許可
  されていた client は、改めて許可する必要がある。
- **"This host's key has changed"** —— [key の取り消しと入れ替え](#key-の取り消しと入れ替え) を参照。
- **"That machine uses an incompatible authentication version"** —— どちらかが以前の
  Deskhub を使っている。両方のマシンを更新する。[以前の Deskhub から移行する](#以前の-deskhub-から移行する)
  を参照。
- **Linux: share が直ちに失敗する** —— `vainfo | grep -E 'H264.*Enc'` を実行する。結果
  が空であれば、そのマシンに使用可能な H.264 encoder がなく、host にはできない。
- **Linux: ポインタが動かない** —— 要件 3 の `/dev/uinput` rule が導入されていない。
- **macOS: 画面が黒い、または input が効かない** —— Settings ページで Screen Recording
  と Accessibility を確認する。
- **その他の場合** —— [issue](https://github.com/manhpham90vn/Deskhub/issues) を作成し、
  端末の機種、OS のバージョン、Host または Client ページの status 行の内容を添えてほしい。
