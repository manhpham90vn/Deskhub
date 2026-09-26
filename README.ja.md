[English](README.md) · [Tiếng Việt](README.vi.md) · [中文](README.zh.md) · **日本語**

<div align="center">

# 🖥️ Deskhub

### 必要な場所から、自分のパソコンを使える。

**Deskhub を使えば、別の端末から自分のパソコンの画面を見て操作できる。オープンソースで、
5 つのプラットフォームで native に動作し、作業にもゲームにも応えられる操作感を目指している。**

[![Release](https://img.shields.io/github/v/release/manhpham90vn/Deskhub?label=release&color=2563eb)](https://github.com/manhpham90vn/Deskhub/releases)
[![License: MIT](https://img.shields.io/github/license/manhpham90vn/Deskhub?color=2563eb)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-2563eb)](CMakeLists.txt)
[![Platforms](https://img.shields.io/badge/runs%20on-Windows%20·%20macOS%20·%20Linux%20·%20Android%20·%20iOS-2563eb)](#-対応プラットフォーム)

[![ci](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/ci.yml)
[![lint](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/lint.yml)
[![codeql](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/codeql.yml)
[![nightly](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml/badge.svg)](https://github.com/manhpham90vn/Deskhub/actions/workflows/nightly.yml)

**[インストール](#install)** · [source から build](docs/BUILD.ja.md) ·
[Spec](docs/SPECIFICATION.ja.md) · [Architecture](docs/ARCHITECTURE.ja.md) ·
[Security](SECURITY.ja.md)

</div>

**目次:**

- [インストール](#install)
- [デモ](#demo)
- [概要](#about)
- [用途](#why)
- [対応プラットフォーム](#platforms)
- [中身](#features)
- [ドキュメント](#docs)
- [ライセンス](#license)

<a id="install"></a>

## 📦 インストール

Windows、macOS、Ubuntu、Debian、Mint では package manager からインストールできる。

```bash
winget install ManhPham.Deskhub                  # Windows · app
winget install ManhPham.DeskhubCLI               # Windows · deskhub-cli
brew install --cask manhpham90vn/tap/deskhub     # macOS · app
brew install manhpham90vn/tap/deskhub-cli        # macOS · deskhub-cli
```

Ubuntu / Debian / Mint では、デスクトップ app と CLI は別パッケージである。
apt repository を一度追加し、必要な方だけ、または両方をインストールする。

```bash
sudo install -d /etc/apt/keyrings
curl -fsSL https://manhpham90vn.github.io/Deskhub/apt/deskhub.gpg | sudo tee /etc/apt/keyrings/deskhub.gpg >/dev/null
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/deskhub.gpg] https://manhpham90vn.github.io/Deskhub/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/deskhub.list
sudo apt update
sudo apt install deskhub       # デスクトップ app
sudo apt install deskhub-cli   # CLI。単独でもインストール可能
```

デスクトップパッケージはアプリケーションメニューに DeskHub を追加し、`deskhub` を
`/usr/bin` にインストールする。CLI パッケージは `deskhub-cli` を `/usr/bin` に
インストールする。Windows の app インストーラーはスタートメニューとデスクトップに
ショートカットを作成し、別の CLI インストーラーはユーザーの `PATH` にコマンドを
追加する。macOS の CLI は Homebrew により `PATH` に配置される。

その他のプラットフォームは [Releases](https://github.com/manhpham90vn/Deskhub/releases) から：

- **Windows 直接ダウンロード** —— app 用 `deskhub-v*-windows-setup.exe` と CLI 用
  `deskhub-cli-v*-windows-setup.exe`。portable 版 exe も利用可能
- **Fedora / openSUSE** —— app と CLI は別の RPM。`deskhub-v*-x86_64.rpm` と
  `deskhub-cli-v*-x86_64.rpm` を必要に応じて `dnf` または `zypper` でインストール
- **Arch、その他の Linux** —— portable 版 `deskhub-v*-linux-x86_64` を `chmod +x` して実行
- **Android** —— `deskhub-v*-android.apk`、または [Play の beta](https://play.google.com/apps/testing/com.manhpham.deskhub)
- **iOS** —— [TestFlight](https://testflight.apple.com/join/7qY7wgpd)

プラットフォームごとの詳細と必要な権限：[INSTALL.ja.md](docs/INSTALL.ja.md)。

<a id="demo"></a>

## 👀 デモ

<div align="center">

<img src="docs/imgs/macos_1.png" alt="macOS の Deskhub Host ページ。Share on network の選択欄、他のマシンが connect してくる Wi-Fi と Tailscale のアドレス、UDP port 47777 の Not sharing バナー、Start sharing ボタンの上にある Terminal を選択済みの source 一覧" width="850">

<sub>共有を開始する前の macOS host。このマシンから出すものを選択する —— 任意の display、shell、またはその両方 —— そのうえで <b>Start sharing</b> を押す。</sub>

</div>

<table>
  <tr>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_2.png" alt="macOS の Deskhub Client ページ。host IP、UDP port、passcode と自分の名前の入力欄、remote desktop・control・terminal を選ぶチェックボックス、Connect ボタン、status・ping・最終接続時刻を並べたデバイス表">
      <br><sub><b>Client</b> —— IP を入力するか、scan が検出したマシンを選択し、開く対象を選ぶ。画面、control 権限、shell、またはその組み合わせ。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_3.png" alt="macOS の Deskhub Devices ページ。pair 済みマシンと各 key、pair した時刻と最後に確認された時刻、Forget と Forget every machine のボタン、新規 pair を許可するスイッチ、本マシンの SHA256 key">
      <br><sub><b>Devices</b> —— 受け入れたことのあるマシンを名前と key とともに一覧表示し、個別に取り消せる。必要なマシンの pair が済んだら、新規 pair を無効にできる。</sub>
    </td>
    <td align="center" width="33%">
      <img src="docs/imgs/macos_4.png" alt="macOS の Deskhub Settings ページ。fps、bitrate、quality、UDP port、pair に使う passcode、viewer が本マシンを control できるかのスイッチ、clipboard とスリープ防止のトグル、Screen Recording と Accessibility permission の現在の状態、ログイン時起動のスイッチ">
      <br><sub><b>Settings</b> —— fps、bitrate、quality、port、passcode、viewer による control の可否、および macOS permission の現在の状態。</sub>
    </td>
  </tr>
</table>

<p align="center">
  <img src="docs/imgs/ios_1.png" alt="iOS の Deskhub Client ページ。IP・port・passcode・名前の入力欄、Connect と Terminal のボタン、リモートのマシンを control するスイッチ、確認済みアドレス数を報告する scan" width="195">
  <img src="docs/imgs/ios_2.png" alt="iOS の Deskhub Host ページ。pair に使う passcode、Share on network、Start sharing、他のマシンが connect に使う IP アドレス" width="195">
  <img src="docs/imgs/ios_3.png" alt="iOS の Deskhub Devices ページ。まだ空の pair 済みマシン一覧、新しいマシンの pair を許可するスイッチ、本デバイスの SHA256 key" width="195">
  <img src="docs/imgs/ios_4.png" alt="iOS の Deskhub 接続 settings ページ。scan が確認する UDP port、clipboard 同期とスリープ防止のスイッチ" width="195">
</p>
<p align="center"><sub><b>iPhone</b> —— 同じ 4 ページ。scan を実行してマシンを選択し、映像を trackpad として操作する。iPhone 自身の画面を host することもできるが、view-only に限られる。</sub></p>

<p align="center">
  <img src="docs/imgs/android_1.png" alt="Android の Deskhub Client ページ。IP・port・passcode・名前の入力欄、Connect と Terminal のボタン、control のチェックボックス、subnet を走査中の scan" width="195">
  <img src="docs/imgs/android_2.png" alt="Android の Deskhub Host ページ。pair に使う passcode、Share on network、Start sharing、他のマシンが connect に使う IP アドレス" width="195">
  <img src="docs/imgs/android_3.png" alt="Android の Deskhub Devices ページ。まだ空の pair 済みマシン一覧、新しいマシンの pair を許可するチェックボックス、本デバイスの SHA256 key" width="195">
  <img src="docs/imgs/android_4.png" alt="Android の Deskhub 接続 settings ページ。scan が確認する UDP port、clipboard 同期とスリープ防止のチェックボックス" width="195">
</p>
<p align="center"><sub><b>Android</b> —— 同じ 4 ページを Material Design で構成している。host としては、Android 10+ で view-only の画面共有のみを行う。</sub></p>

<a id="about"></a>

## 📖 概要

一方の端末で共有する display を選び、もう一方で IP アドレスを入力して Connect する。
**Host**、**Client**、**Devices**、**Settings** の 4 ページはどのプラットフォームでも
共通なので、Mac から PC やスマートフォンに移っても操作に迷いにくい。protocol は 5 つの
プラットフォームで共通の **C++20 core** が処理する。

| ⚡ 速い | 📦 簡単インストール | 🎛️ シンプル |
| ------ | ---------- | --------- |
| 対応するハードウェアでは 60 fps で stream できる。video 処理には、利用できる場合に GPU メモリを使う。 | package manager からインストールするか、release をダウンロードできる。アカウントも background service も不要。 | display を **Share** するか、IP アドレスへ **Connect** する。デスクトップでは **shell** の共有と**ファイル**の受信も可能。スマートフォンは view-only で画面を共有できる。 |

Session は **QUIC/TLS** 上で end-to-end に encrypt される。未知のマシンが受け入れられる
のは、host の passcode を知っていることを証明した場合 —— **SPAKE2** を用いるため
passcode 自体は送信されない —— または host 側の利用者が承認した場合に限られる。ただし
これは開いた port 上の短い秘密であることに変わりはない。信頼できる network か VPN を
使用し、**UDP 47777 を port-forward しないこと**。完全な threat model は
[`SECURITY.ja.md`](SECURITY.ja.md) を参照。

<a id="why"></a>

## 💡 用途

- 💻 **作業** —— 性能の低いノート PC や iPad から、自宅 PC の Claude Code、VS Code、build を実行する。
- 🌐 **デスクトップアプリ** —— 別の端末から、パソコン上の Chrome、Office などを使う。
- 🎮 **ゲーム** —— 60 fps、relative mouse と DirectInput scancode、`F9` による pointer lock。
- 🖥️ **マルチ display** —— display を 1 つまたは複数共有し、それぞれが独立した session となる。

<a id="platforms"></a>

## 🚦 対応プラットフォーム

| プラットフォーム | Host | Client | 状態 |
| -------- | :--: | :----: | ------ |
| **Windows** | ✅ | ✅ | reference 実装 —— LAN および Tailscale（Internet/NAT）で日常的に使用 |
| **macOS** | ✅ | ✅ | 両方の役割が動作（ScreenCaptureKit + VideoToolbox + CGEvent） |
| **Android** | ✅ | ✅ | Client: video と input（trackpad、keyboard）。Host: view-only の画面共有（MediaProjection + MediaCodec）、Android 10+ —— Google Play でテスト中 |
| **iOS** | ✅ | ✅ | Client: video と input（trackpad、keyboard）。Host: Broadcast Upload Extension による view-only の画面共有（ReplayKit + VideoToolbox）—— TestFlight でテスト中 |
| **Linux** | ✅ | ✅ | 両方の役割が動作（PipeWire + VA-API + uinput + GTK3）—— Ubuntu、Debian、Mint、Fedora、openSUSE、Arch に deb / rpm / portable binary で提供。2 台間の LAN で検証済み |

<a id="features"></a>

## ✨ 中身

- **GPU を使う video 処理** —— 対応する環境では capture、encode、decode、render に各プラットフォームのハードウェアを使う。Windows の NVENC 経路では frame を CPU 経由でコピーしない。
- **QUIC 上の専用 protocol** —— 無限 GOP と必要時の IDR、XOR FEC、adaptive bitrate を、encrypt 済みの connection 1 本に multiplex する。
- **画面に音声が伴う** —— マシン自身の audio mix を Opus 64 kbps で送る。1 datagram につき 20 ms の frame を 1 つ。packet を 1 つ失っても損失は数十ミリ秒にとどまり、映像には影響しない。microphone は capture しない。
- **実際の input** —— relative mouse（Raw Input）と、DirectInput ゲーム向けの scancode。host 自身の mouse と keyboard が常に優先される。
- **共有された core** —— protocol、FEC、bitrate control は `core/` にあり、すべての client にコンパイルされる。
- **command line ツール** —— `deskhub-cli` で画面を共有したり、remote shell を開いたり、スクリプトや SSH から操作したりできる。Windows と Linux ではリモート画面のウィンドウも開ける。[Build](docs/BUILD.ja.md#command-line-client) を参照。
- **core を継続的にテスト** —— オフラインの unit test に加え、CI で ASan、UBSan、TSan を実行する。7 つの libFuzzer target が wire format、H.264 の parse、reassembly、terminal の byte stream、UI の文字列、session state machine を毎晩検査し、見つかった crash は regression test に加える。

<a id="docs"></a>

## 📚 ドキュメント

すべてのドキュメントは英語で公開し、その隣に訳を置いている。ベトナム語 `*.vi.md`、
中国語 `*.zh.md`、日本語 `*.ja.md`。正文は英語版である。

| ドキュメント | 内容 |
| --- | --- |
| [Install](docs/INSTALL.ja.md) ([en](docs/INSTALL.md) · [vi](docs/INSTALL.vi.md) · [zh](docs/INSTALL.zh.md)) | 5 つのプラットフォームそれぞれへの Deskhub の導入 |
| [Build](docs/BUILD.ja.md) ([en](docs/BUILD.md) · [vi](docs/BUILD.vi.md) · [zh](docs/BUILD.zh.md)) | source からのコンパイル、test、パッケージング、release |
| [Specification](docs/SPECIFICATION.ja.md) ([en](docs/SPECIFICATION.md) · [vi](docs/SPECIFICATION.vi.md) · [zh](docs/SPECIFICATION.zh.md)) | Deskhub が何をするか。実装の詳細は扱わない |
| [Architecture](docs/ARCHITECTURE.ja.md) ([en](docs/ARCHITECTURE.md) · [vi](docs/ARCHITECTURE.vi.md) · [zh](docs/ARCHITECTURE.zh.md)) | layer、thread、wire protocol、設計判断 |
| [`SECURITY.ja.md`](SECURITY.ja.md) ([en](SECURITY.md) · [vi](SECURITY.vi.md) · [zh](SECURITY.zh.md)) | Threat model と脆弱性の報告方法 |
| [`PRIVACY.ja.md`](PRIVACY.ja.md) ([en](PRIVACY.md) · [vi](PRIVACY.vi.md) · [zh](PRIVACY.zh.md)) | プライバシーポリシー |
| [`THIRD_PARTY_NOTICES.ja.md`](THIRD_PARTY_NOTICES.ja.md) ([en](THIRD_PARTY_NOTICES.md) · [vi](THIRD_PARTY_NOTICES.vi.md) · [zh](THIRD_PARTY_NOTICES.zh.md)) | サードパーティ製コンポーネントと license |

バグ報告と意見: [issues](https://github.com/manhpham90vn/Deskhub/issues) —— 端末の機種名
を添えてほしい。

<a id="license"></a>

## 📄 ライセンス

MIT —— [`LICENSE`](LICENSE) を参照。サードパーティ製コンポーネントとその表示（Linux
app に静的 link されている LGPL ビルドの FFmpeg を含む）は
[`THIRD_PARTY_NOTICES.ja.md`](THIRD_PARTY_NOTICES.ja.md) に一覧がある。
