[English](INSTALL.md) · [Tiếng Việt](INSTALL.vi.md) · [中文](INSTALL.zh.md) · **日本語**

# Deskhub — インストール

全プラットフォームを 1 ページで。ここに書かれたことにソースの取得は一切要らない —
リリースはすべてビルド済み。自分でコンパイルする話は [`BUILD.ja.md`](BUILD.ja.md)。

本書は [`INSTALL.md`](INSTALL.md) の翻訳である。相違がある場合は英語版が正典となる。

ダウンロードはすべて
[Releases ページ](https://github.com/manhpham90vn/Deskhub/releases)にある。モバイル版は
TestFlight と Google Play で配布している。

| プラットフォーム | ファイル | 一行インストール |
| --- | --- | --- |
| 🪟 Windows | `deskhub-v*-windows.exe` | ダウンロードして実行するだけ |
| 🍎 macOS | `deskhub-v*-macos.dmg` | dmg を開き、アプリをドラッグ |
| 🐧 Ubuntu、Kubuntu、Debian、Mint | `deskhub-v*-amd64.deb` | `sudo apt install ./deskhub-v*-amd64.deb` |
| 🐧 Fedora（Workstation と KDE spin） | `deskhub-v*-x86_64.rpm` | `sudo dnf install ./deskhub-v*-x86_64.rpm` |
| 🐧 openSUSE | `deskhub-v*-x86_64.rpm` | `sudo zypper install ./deskhub-v*-x86_64.rpm` |
| 🐧 Arch、その他 | `deskhub-v*-linux-x86_64` | `chmod +x deskhub-v*-linux-x86_64 && ./deskhub-v*-linux-x86_64` |
| 🤖 Android | `deskhub-v*-android.apk` | apk を入れるか、Play ベータに参加 |
| 📱 iOS | — | [TestFlight](https://testflight.apple.com/join/7qY7wgpd) |

自前のウィンドウを持たない同じクライアント `deskhub-cli` もある —
[コマンドライン](#-コマンドライン)を参照。

---

## 🪟 Windows

`deskhub-v*-windows.exe` をダウンロードして実行する。インストーラも常駐サービスも
アカウントもない — アプリ全体がこの 1 ファイル。

初回だけ起きることが 2 つある：

- **起動時に一度だけ管理者権限。** これなしでは昇格されたウィンドウにマウスとキーボードを
  注入できない。
- **Windows ファイアウォールの規則**を、初めて共有したときにアプリ自身が追加する。

Deskhub を消すには exe を削除する。設定と鍵は、そのフォルダを削除するまで
`%USERPROFILE%\.deskhub` に残る。

## 🍎 macOS

`deskhub-v*-macos.dmg` をダウンロードして開き、アプリを *アプリケーション* にドラッグ
する。dmg は Developer ID で署名され Apple の公証を受けているので、Gatekeeper の警告
なしに開く。

画面のホストには macOS の権限が 2 つ要る。どちらもアプリの **Settings** 画面から要求
でき、そこには現在の状態と、対応するシステム設定パネルへ直行するボタンも出る：

| 権限 | 用途 |
| --- | --- |
| **画面収録（Screen Recording）** | この Mac のディスプレイをキャプチャする |
| **アクセシビリティ（Accessibility）** | 視聴者がこの Mac のマウスとキーボードを動かせるようにする |

他のマシンを見るだけならどちらも要らない。

## 🐧 Linux

**接続して見るだけなら、入れるだけでいい。** アプリがリンクするのは GTK3、PipeWire、
libva だけで、どれも素のデスクトップに最初から入っている。H.264 デコーダは組み込み
なので、FFmpeg のパッケージは一切関係しない。

deb と rpm の中身は同一 — パッケージマネージャが理解するほうを選ぶ。どちらにも下の
要件 3 で説明する `/dev/uinput` の udev 規則が入っているので、インストール直後から
リモート入力が動く。グループ変更もログインし直しも要らない。単体バイナリは glibc 2.35
以降の x86_64 ディストリなら動く（Ubuntu 22.04、Fedora 36、openSUSE 15.5、現行の Arch）。

deb と rpm は `deskhub-cli` も `/usr/bin/deskhub-cli` として入れる — 下の
[コマンドライン](#-コマンドライン)を参照。

**このマシンの画面を共有する**には、さらに 3 つ揃える必要がある。

### 1. 画面キャプチャ portal

Deskhub は常に `xdg-desktop-portal` 経由でキャプチャする — 「どの画面を共有する？」の
ダイアログを出しているのがそれ。選択は記憶されるので、ダイアログが出るのは最初に共有
したときだけ。別の画面にしたいときは Host 画面の *Choose screens again* で呼び戻せる。

GNOME と KDE は主要ディストロすべてで portal バックエンドを標準で持っている —
GNOME/KDE の Ubuntu、Kubuntu、Fedora Workstation、Fedora KDE、openSUSE、Arch では
**何もしなくていい**。単体のウィンドウマネージャでは必要になる：

```bash
sudo apt install xdg-desktop-portal-wlr      # Debian 系の sway / river / Wayfire
sudo dnf install xdg-desktop-portal-wlr      # …Fedora では
sudo pacman -S xdg-desktop-portal-wlr        # …Arch では
```

sway、river、Wayfire は **wlroots** ライブラリの上に作られた Wayland コンポジタで、
GNOME/KDE と違って自前の portal バックエンドを持たない。`-wlr` がそれらすべてのために
画面キャプチャを実装するバックエンドである。Hyprland には専用の
`xdg-desktop-portal-hyprland` がある。

### 2. VA-API ドライバ

H.264 は GPU でエンコードする。ソフトウェアへのフォールバックはない。

```bash
# Ubuntu / Debian / Mint
sudo apt install va-driver-all vainfo        # NVIDIA はさらに nvidia-vaapi-driver が必要

# Fedora — 標準の Mesa は H.264 が無効。動くドライバは RPM Fusion にある：
sudo dnf install libva-utils
sudo dnf install mesa-va-drivers-freeworld   # AMD（RPM Fusion）
sudo dnf install intel-media-driver          # Intel（RPM Fusion）
sudo dnf install nvidia-vaapi-driver         # NVIDIA（RPM Fusion）

# openSUSE
sudo zypper install libva-utils              # に加えて GPU ベンダの VA-API ドライバ

# Arch
sudo pacman -S libva-utils
sudo pacman -S libva-mesa-driver             # AMD · Intel: intel-media-driver · NVIDIA: libva-nvidia-driver

# そのうえで、どのディストロでも：
vainfo | grep -E 'H264.*Enc'                 # 1 行以上出ること。出なければこのマシンはホストになれない
```

### 3. `/dev/uinput` への書き込み権限

マウスとキーボードはこれ経由で注入される。deb と rpm は udev 規則を入れてくれる —
何もしなくていい。単体バイナリなら、コマンド 1 つで設定できる。クローンもデスクトップの
ログインし直しも不要：

```bash
curl -fsSL https://raw.githubusercontent.com/manhpham90vn/Deskhub/main/scripts/setup-uinput.sh | sudo bash
```

sudo にパイプする前に中身を読みたい？ 先に
[`scripts/setup-uinput.sh`](../scripts/setup-uinput.sh) を落として読めばいい — 十数行
しかない。ソースを取得済みなら同じことが `make setup-linux-permissions` でできる。

uinput の許可がなくてもアプリは動くし、見ることもできる — このマシンにマウスや
キーボードを注入できないだけである。

### ファイアウォール

ファイアウォールを有効にしているなら UDP 47777 を開ける：

```bash
sudo ufw allow 47777/udp                                  # Ubuntu / Debian / Mint
sudo firewall-cmd --add-port=47777/udp --permanent        # Fedora / openSUSE
```

### アンインストール

```bash
sudo apt remove deskhub      # または sudo dnf remove deskhub / sudo zypper remove deskhub
rm -rf ~/.deskhub            # 設定、鍵、ペアリング済みマシン
```

単体バイナリはファイル 1 つ — 削除するだけ。

## 🤖 Android

ホストは閲覧専用の画面共有で、**Android 10 以降**が必要。見るだけならもっと古い版でも
動く。

**apk を直接** — [Releases](https://github.com/manhpham90vn/Deskhub/releases) から
`deskhub-v*-android.apk` をダウンロードして入れる。Google Play 版と同じ鍵で署名されて
いる。

**Play ベータ** — 3 ステップ。すべて端末の Play ストアと**同じ Google アカウント**で：

1. テスターグループに参加：[groups.google.com/g/deskhub-test](https://groups.google.com/g/deskhub-test)
2. テスターになる：[play.google.com/apps/testing/com.manhpham.deskhub](https://play.google.com/apps/testing/com.manhpham.deskhub)
3. インストール（Play の同期に数分かかる）：[play.google.com/store/apps/details?id=com.manhpham.deskhub](https://play.google.com/store/apps/details?id=com.manhpham.deskhub)

ベータは **14 日以上**入れたままにしてほしい — 公開前提として Google が要求している。

## 📱 iOS

ipa はサイドロードできないため、ベータは TestFlight で配布している：

1. [TestFlight](https://apps.apple.com/app/testflight/id899247664) を入れる。
2. ベータに参加：**[testflight.apple.com/join/7qY7wgpd](https://testflight.apple.com/join/7qY7wgpd)**

Android と同じく、iPhone や iPad のホストは閲覧専用 — 自分が動いている端末にアプリが
入力を注入することを、どのモバイル OS も許していない。

---

## 💻 コマンドライン

`deskhub-cli` は自前のウィンドウを持たない同じクライアントで、画面を共有し、リモート
シェルを開き、スクリプトや SSH 越しにホストを操作する。一覧は `deskhub-cli help` で。
読み書きするもの — 設定、ペアリング済みマシン、信頼済みホスト鍵 — はアプリと同一なので、
両者は必ず一致する。

| プラットフォーム | ファイル |
| --- | --- |
| 🪟 Windows | `deskhub-cli-v*-windows.exe` — ダウンロードして実行、インストーラ不要 |
| 🍎 macOS | `deskhub-cli-v*-macos` — Apple Silicon と Intel 共通のバイナリ 1 つ |
| 🐧 Linux | `deskhub-cli-v*-linux-x86_64`、または deb / rpm を入れたなら既にある |

macOS と Linux のファイルは実行ビットが立っていないので、一度 `chmod +x` する。どちらも
dmg のような署名・公証はされていない。macOS では初回に
`xattr -d com.apple.quarantine deskhub-cli-v*-macos` が必要か、システム設定 →
プライバシーとセキュリティ の *このまま開く* を使う。

Linux ではアプリと同じ portal と VA-API ドライバで画面を共有し、リモート入力も同じ
`/dev/uinput` 規則に依存するので、[Linux](#-linux) の節がそのまま当てはまる。macOS では
画面共有とシェルは開けるが、他人の画面を*見る*ことはできない — `connect` にはコマンド
ライン版が持たないウィンドウ層が必要で、その旨を表示する。見るときはアプリを使うこと。

---

## 🔒 画面を共有する前に

セッションが運ぶすべて — 映像、キー入力、マウス、クリップボード、ターミナルの通信 —
は **QUIC/TLS** の上を流れ、未知のマシンはペアリングのハンドシェイクでしか入れない。
**SPAKE2** でホストのパスコードを知っていると証明する（コード自体は決して流れず、1 接続に
つき試行は 1 回だけ）か、ホストの前にいる人が *このマシンを入れますか？* に答えるのを
待つかである。

暗号化されていることと、インターネットに耐えることは別である。ポートは今も探索プローブに
応答するし、会ったことのないマシンとの最初のペアリングは信頼の跳躍でしかない。**信頼できる
ネットワーク**か **VPN** を使うこと — 両方のマシンに [Tailscale](https://tailscale.com)
を入れ、`100.x.y.z` のアドレスに接続する。**UDP 47777 は絶対にポート転送しないこと。**

[`SECURITY.ja.md`](../SECURITY.ja.md) に完全な脅威モデル、何が守られ何が守られないか、
脆弱性の報告方法がある。

## 🆘 うまくいかないとき

- **つなぐ相手が出てこない** — 両方のマシンが同じネットワーク（または同じ Tailscale
  tailnet）にいて、ホストの UDP 47777 が開いている必要がある。
- **Linux：共有がすぐ失敗する** — `vainfo | grep -E 'H264.*Enc'` を実行する。空なら、
  このマシンには使える H.264 エンコーダがなく、ホストになれない。
- **Linux：ポインタが動かない** — 要件 3 の `/dev/uinput` 規則が入っていない。
- **macOS：真っ黒な画面、または入力が効かない** — Settings 画面で画面収録と
  アクセシビリティを確認する。
- **それでも駄目** —
  [issue](https://github.com/manhpham90vn/Deskhub/issues) を立て、端末のモデル名、OS の
  バージョン、Host か Client 画面のステータス行に出ている内容を添えてほしい。
