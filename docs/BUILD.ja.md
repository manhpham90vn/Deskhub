[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · [中文](BUILD.zh.md) · **日本語**

# Deskhub — ビルドと開発

このガイドでは Deskhub の build、test の実行、release の準備を説明する。app を
インストールして使うだけなら [`INSTALL.ja.md`](INSTALL.ja.md) から始めてほしい。

本書は [`BUILD.md`](BUILD.md) の翻訳。食い違いがある場合は英語版が正文。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 一度だけ: この OS 向けの toolchain と依存関係
make test             # core suite を build し、オフラインで実行
make build-linux      # または build-windows / build-macos / build-ios / build-android
```

build するプラットフォームの target を明示すること。引数なしの `make` は
`make/help.txt` の target 一覧を表示するだけで、app を build しない。各 target の説明は
[`Makefile`](../Makefile) にある。

---

## 1. 事前に必要なもの

`make bootstrap` は自動で導入できるものを導入し、できないものは通知する。実行前に次を
自分で用意しておく必要がある。

| 手元の OS | 事前に導入するもの | bootstrap が導入するもの |
| --- | --- | --- |
| **Ubuntu / Debian** | apt 以外に不要。加えて [Rust](https://rustup.rs) | build-essential、clang、llvm、CMake（apt のものが 3.25 より古い場合は `pipx install "cmake>=3.25,<4"`）、ninja、JDK 17、pipx、python3-venv、rpm、nasm、GTK3 / PipeWire / VA-API / tray の `-dev` package、VA-API driver、GNOME portal、静的な最小構成 FFmpeg、cargo-ndk、quiche、opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode と command line tools、[Rust](https://rustup.rs) | cmake、ninja、pipx、Homebrew の LLVM（Apple clang には libFuzzer runtime が含まれない）、Temurin JDK 17、cargo-ndk、Apple 向けの quiche および opus |
| **Windows** | winget（App Installer）、[Git for Windows](https://git-scm.com/download/win)（build script はその Git Bash で動く）、Python 3 | C++ workload と *C++ Clang tools* を含む Visual Studio Build Tools、Rust、NASM、GNU make、Temurin JDK 17、Android Studio を winget 経由で導入する。`scripts/bootstrap.ps1` が実行する |

共有の CMake ツリーには CMake 3.25 以上が必要である。Windows の初回はまだ `make` が
ないため、`powershell -ExecutionPolicy Bypass -File scripts\bootstrap.ps1` を直接実行し、
その後新しい terminal を開く。

いずれの OS でも、bootstrap は CI が使う style と解析のツールを pin する。clang-format、
clang-tidy、ktlint、SwiftFormat、cppcheck、detekt、さらに macOS では SwiftLint が、それぞれ
固定バージョンで入る。ダウンロードするもの（ktlint、detekt、SwiftFormat、SwiftLint、
cppcheck の source）は checksum で検証する。SwiftLint（0.65.0）は Homebrew ではなく
`tools/swiftlint` に入り、macOS で見つからない場合は `make lint` が自らそこへ導入する。clang-format と clang-tidy は pin された
バージョンで PyPI から `tools/venv` に導入し、Windows の cppcheck は pin されたバージョンで
winget から導入する。これらを手動で導入してはならない。CI が突き合わせるのはこのバージョン
だからである。Swift の不要コードを探す Periphery は、`make lint-dead-swift` を初めて実行
したときに checksum の検証付きで取得される。

モバイル向けの target には追加の要件がある。`build-android` には NDK を含む Android
SDK が必要である（`ANDROID_HOME` が cmdline-tools のインストール先を指していれば、
bootstrap が SDK package —— platform-tools、platform、NDK、CMake —— を導入し、続けて
Android 向けの quiche と opus を build する）。`build-ios` には Simulator runtime を含む
Xcode が必要である。NDK のバージョンは Gradle のコマンドラインの
`-PandroidNdkVersion=<v>`、なければ環境変数 `ANDROID_NDK_VERSION`、なければ
`26.1.10909125` で決まり、bootstrap も同じものを導入する。

git submodule は `nvenc` のヘッダのみである。clone 時に `--recurse-submodules` を
付けるか、後から `git submodule update --init` を実行すればよい。`make bootstrap` も
これを同期する。

## 2. ツリーの構成

```
core/       プラットフォーム非依存の C++20 —— protocol、packetization、FEC、session state、
            input mapping、bitrate control、VT emulator。OS ヘッダなし。unit test あり。
platform/   OS 向けの薄い abstraction で、API は 1 種類 —— socket、clock、logging、
            random、source の列挙、および複数の app が共有する media・audio・input の
            コード。core に依存する。
client/     5 つの app: android、ios、linux、macos、windows。
            client/apple/ は macOS と iOS の app が共有する Swift であり、app ではない。
            client/cli/ は command line client。デスクトップ 3 種を binary 1 つで担う。
tests/integration/  loopback 上の host + viewer。capture と encode は模擬実装
cmake/      共有の CMake module: warning、quiche、opus
third_party/  quiche (QUIC)、opus (audio)、nvenc のヘッダ、最小構成の FFmpeg build
patches/    build 前に quiche に適用する patch
licenses/   パッケージに同梱するサードパーティのライセンス文
packaging/  Windows インストーラー、Homebrew の雛形、release notes、apt と Pages のサイト
assets/     すべての client アイコンの生成元となるアイコン
make/       プラットフォームごとに .mk を 1 つ。ルートの Makefile が include する
scripts/    bootstrap、パッケージング、coverage、style、CI 用の補助スクリプト
tools/      bootstrap が導入する pin 済みの style ツール（git 管理外）
.github/    workflow と、それらが共用する composite step（actions/）
```

ロジックは一度だけ記述して共有する。`client/*` の下に追加する前に、それが `core/`
（プラットフォーム非依存）または `platform/`（OS を必要とするが API はどこでも同一）に
属さないかを確認すること。layer の分割、threading model、wire protocol の説明は
[`ARCHITECTURE.ja.md`](ARCHITECTURE.ja.md) にあり、この repo が強制する規則は
`CLAUDE.md` に記載している。

## 3. 日常の手順

```bash
make test      # core suite。オフラインで、GPU も network も不要 —— 数秒
make lint      # C++・Kotlin・Swift の format を検査し、続けて不要コードを検査する
```

変更を終える前に両方を実行する。format を適用するときは `make format` を使い、
確認だけなら `make lint` を使う（ファイルは書き換えない）。formatter のバージョンは CI と合わせて固定している。

`core/` に追加したロジックには、`core/tests/` の対応するサブディレクトリに test が必要
である。

## 4. app の build と実行

| Target | 生成物 | 必要なもの |
| --- | --- | --- |
| `make build-windows` | `Deskhub.exe` 1 つ | Windows と MSVC |
| `make build-macos` | macOS app | macOS と Xcode |
| `make build-linux` | `deskhub` binary 1 つ | Ubuntu と各 `-dev` package |
| `make build-ios` | Simulator 向けの iOS app | macOS、Xcode、Simulator runtime |
| `make build-android` | debug APK | Android SDK、NDK、`adb` |

各 target には `release-<os>`（最適化）と `run-<os>`（build して起動）が対になって存在
する。`release-ios` も Simulator 向けであり、`release-android` は署名なしの release APK
を生成する。デスクトップの app は command line のフラグを一切解釈せず、選択はすべて 4 つの
ページで行う。`run-android` は adb 経由で接続中の端末または emulator にインストールして
起動し、`run-ios` は起動中の Simulator、なければ最初に使える iPhone simulator で同じ処理を
行う。`IOS_DEVICE=<udid>` で端末を指定できる。

macOS の target は、keychain に Apple Development identity がなければ ad hoc で署名する。
`MACOS_SIGN=adhoc` または `MACOS_SIGN=developerid` でモードを強制でき、`MACOS_TEAM` は
Developer ID 署名に使う team を、`MACOS_XCARGS` は `xcodebuild` に渡す追加の build 設定を
指定する。

Debug build はインストール済みの release の邪魔をしない。どのデスクトップ OS でも、app と
CLI の Debug build は key、許可済み client、信頼済み host、設定、ログを `~/.deskhub` ではなく
`~/.deskhub-dev`（Windows: `%USERPROFILE%\.deskhub-dev`）に保存する。macOS では
`build-macos`/`run-macos` が bundle id `com.deskhub.macos.debug` の **Deskhub Dev** を作る。
そのためローカル build を動かしても、インストール済みの app を終了させることも、そのデータに
触れることもなく、macOS のプライバシー権限も別に要求する。`release-<os>` の build は通常の
フォルダと bundle id を使う。

### Command line client

`client/cli/` は、app のページの代わりにコマンドで操作する `deskhub-cli` を build
する。SSH、スクリプト、systemd から使える。Windows と Linux の `connect` は viewer
ウィンドウを開くが、macOS 版はこのコマンドにまだ対応していない。

```bash
make build-cli                       # この OS 向けの debug build
make release-cli                     # 最適化版
make run-cli ARGS="host list"        # build したうえで、その引数で実行
make cli-smoke                       # build したうえで、loopback 上で自身を相手に実行
```

この target は `-DDESKHUB_CLI=ON` の後ろにあり（既定では無効）、app および sanitizer・
coverage・fuzz の preset には影響しない。有効にすると、OS ごとの media ライブラリが
任意から必須に変わる。capture も decode もできない client は client として成立しない
ためである。Linux ではこれに静的な最小構成 FFmpeg が含まれるため、CLI の target は
`build-linux` と同様に先に `ffmpeg-min` を build する。

`make cli-smoke` は build した CLI を headless で、loopback 上の自分自身を相手に実行する。
鍵の交換、承認を待つ未知の client、QR 招待、remote shell、host へのファイル送信を確認する。
Windows では POSIX signal を必要とする手順を省く。CI はデスクトップ 3 種すべてで実行する。

| コマンド | 機能 |
| --- | --- |
| `share` | このマシンを共有する —— 任意の display、shell、またはその両方 |
| `connect ADDRESS` | host の画面を表示して操作するウィンドウを開く（Windows と Linux） |
| `shell ADDRESS` | 現在の terminal 内で host 上の shell を開く |
| `send ADDRESS FILE...` | ファイルの受信を許可している host へファイルを送る |
| `displays`、`sources ADDRESS` | ローカルの display、および認証済み host が共有しているもの |
| `key public`、`access`、`host`、`host-key public` | このマシンの公開鍵、許可済み client と接続要求、保存済み host、この host の鍵 |
| `devices`、`trust`、`settings` | 同じ設定ファイルを使う従来のコマンド |
| `help [COMMAND]`、`version` | 使い方、およびこの build が報告するバージョン |

すべてのマシンは鍵を 1 つ持つ。その公開鍵は `key public` で表示し、host では
その一行を `access add --stdin` に入力して手動で許可する。許可されていない状態で
接続してきた client は host に**接続要求**を残す。`access requests` がそれを一覧し、
`access approve --fingerprint SHA256:...` または `access deny --fingerprint SHA256:...`
で決着させる。`share --qr` は QR code を表示し、その `deskhub://pair/...` リンクは
1 台のデバイスを単独で受け入れる。このリンクは `connect`、`sources`、`shell`、`send`
でアドレスの代わりに使える。
host の `host-key public` の出力を client に渡し、
`host add office --address 192.168.1.10:47777 --host-key-stdin`
に入力すれば host を事前に固定できる。その後 `connect office`、`sources office`、
`shell office`、`send office FILE` を使用できる。`host update office` でアドレスまたは
固定鍵を明示的に変更し、`host remove office` で profile を削除する。
`access remove --fingerprint SHA256:...` は client 鍵を取り消す。`sources`、`connect`、
`shell`、`send` はアドレス、招待リンク、または保存済みの alias を受け取り、
`--accept-new-host-key` で初めて見る host の鍵を保存し、`--approval-wait SEC` で host の
所有者の承認を待つ時間を変更する（既定は 120）。`--accept-new-host-key` を付けない場合、
未知の host は拒否されてその fingerprint が表示される。信頼は host の鍵に従うため、
アドレスが変わった host も信頼されたままである。network の scan も passcode のフラグも
存在しない。`--config-dir PATH` は全コマンドで同じ設定ディレクトリを選び、コマンドの
前後に置ける。

フラグは `deskhub-cli help COMMAND` が表示する。一覧コマンドは `--json` に対応し、
exit code が失敗の理由を示す。`1` その他の失敗、`2` フラグの誤り、`3` 応答なし、`4`
拒否または時間内に承認されなかった、`6` 共有または表示するものがない、`8` host が listen
を開始できなかった、`9` この build では未対応、`130` Ctrl-C で中断された。

Linux は上表のコマンドをすべて利用できる。Windows の `connect` はデスクトップ app の
ウィンドウコードを再利用する。macOS では share と remote shell を利用できるが、
`connect` はこの build で画面を見られないことを通知する。

`core/` と `platform/` のみを扱う場合は、共有の CMake ツリーのほうが高速である。

```bash
make debug        # debug preset を configure して build する
make release      # …release preset を
```

**quiche と opus は ABI ごとに build する。** QUIC transport は `third_party/quiche`
で build される Rust の静的ライブラリであり、これがないと share も connect もできない。
Opus audio codec は `third_party/opus` で build される C の静的ライブラリであり、これが
ないと共有に音声が含まれない。`debug`、`release`、すべての `build-*`・`release-*`・CLI
の target、および `test`、すべての `test-*` target と `lint-tidy` は必要な ABI を先に
build し、既に build 済みであれば何も行わない（`coverage`、`fuzz`、`fuzz-coverage` は
どちらも build しない。これらは `core/` だけを対象とする）。そのため、どの OS でも clone 直後に
`make bootstrap && make test` が動く。`make quiche`、`quiche-android`、`quiche-ios`、
`quiche-macos`、および対応する `opus`、`opus-android`、`opus-ios`、`opus-macos` は
これらの工程を単独で実行する。どちらかのライブラリの build が失敗すると `make` はそこで
停止する。quiche がないときに CMake が停止するのは意図的である。connect できない binary
の生成を拒否している。

**Hardening。** `cmake/DeskhubHardening.cmake` は GCC と Clang の build に
`-fstack-protector-strong` と、最適化 build では `-D_FORTIFY_SOURCE=3` を付加する ——
ただし Android は NDK の既定値 2 のままである。full RELRO（`-Wl,-z,relro,-z,now`）は
Linux でのみ付加する。MSVC の build は `/sdl` を使う。

## 5. テスト

| コマンド | 実行環境 | 対象範囲 |
| --- | --- | --- |
| `make test` | オフライン、socket なし | `core/` の全体: wire format、framing、FEC、session、VT emulator、settings、文字列 |
| `make test-platform` | loopback socket | 実際の QUIC handshake、end-to-end の鍵署名による認証、host key の固定、ネットワーク越しの terminal host と viewer、実 shell に対する PTY、不正な署名による lockout |
| `make test-integration` | loopback、capture/encode は模擬実装 | host↔client の session 一式: negotiation、ネットワーク越しの video、input、許可済み鍵による受け入れ、不正データへの耐性 |
| `make test-all` | 3 つの suite すべて。core が先 | |
| `make test-ctest` | 同じ test を CTest 経由で実行 | CI の呼び出し方と同一 |
| `make test-asan` | 3 つの suite を ASan と UBSan の下で実行 | clang/gcc のみ。MSVC は非対応 |
| `make test-tsan` | 3 つの suite を ThreadSanitizer の下で実行 | clang/gcc のみ。MSVC は非対応 |
| `make test-perf` | release build、オフラインと loopback | hot path を実測する: `core_perf` は packetize/reassemble/FEC、1080p の縮小、CRC とファイルのバッチ、VT parser と screen、wire の encode/decode、audio jitter buffer と PCM ring、input の path、record stream の framing を対象とし、`platform_perf` は loopback 上の実際の QUIC を対象とする |
| `make perf-build` | release build、実行はしない | `core_perf` と `platform_perf` を build するだけで実行しない |
| `make cli-smoke` | loopback、headless | command line client を自身と対向させる（上記参照） |

これらの test suite には、リモートの peer、GPU、network を必要とするものはない。

**Coverage。** `make coverage` は clang と llvm-cov で `core/` のレポートを生成する。
`scripts/check-coverage.sh` は CI と同じ基準を適用する。**line ≥ 90 %、branch ≥ 80 %**
である。

**Fuzzing。** `make fuzz` は libFuzzer の target を実行する。対象は wire、H.264、
reassembly、terminal の byte stream、UI テキストの parser、鍵・アクセス一覧・招待の
テキスト形式、QR encoder、および host 側と viewer 側の
session state machine である（clang、Linux/macOS。target ごとに `FUZZ_SECONDS=N`）。
各 target はまず `core/fuzz/regressions/<target>` を再生し、修正済みの crash が再発しな
いことを確認したうえで、commit 済みの seed と dictionary から fuzz を開始する。corpus
が core のどの行まで到達しているかは `make fuzz-coverage` で確認できる。検出された
crash はすべて regression の入力となる。

**パフォーマンス。** `make test-perf` は release preset で perf の binary を 2 つ build
して実行する。`core_perf` は純 C++ の hot path で 37 個の workload を測定し、続く
`platform_perf` は loopback 上の実際の QUIC でさらに 6 個を測定する。所要時間は合計で
数秒である。失敗の判定基準は 3 つあり、いずれも恣意的に定めたミリ秒の閾値ではない。

- **単位あたりの allocation 回数。** グローバルな `operator new` を差し替えて正確に
  計数する。packet ごと、あるいは frame ごとに allocate を始めた path は、どのマシン
  でも毎回失敗する。
- **コストの増え方。** `-scaling` の各行は入力を 4 倍にして同じ処理を実行し、所要時間が
  入力よりはるかに速く増加した場合に失敗する。これは意図せず混入した O(n²) の特徴で
  ある。
- **記録済み baseline からの乖離。** `make perf-baseline` が負荷のないマシンで
  `out/perf/baseline.txt` と `out/perf/platform-baseline.txt` を生成し、以後の実行は
  行ごとに変化を報告して、25 % を超えた時点で失敗する。これらのファイルは特定の 1 台を
  記述したものなので git には含めない。

計測時間側の調整には `DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、
`DESKHUB_PERF_BASELINE`、`DESKHUB_PERF_WRITE` を使用する。`make test` はこれを一切実行
しない。debug、ASan、coverage の build は production の速度を反映しないためである。CI は
Linux と macOS の release job で両 binary を実行するが、timing の baseline は特定の 1 台を
記述するものなので、失敗しうるのは allocation と scaling の判定だけである。pull request
では、base commit と変更後を同じ runner で build し、その間の乖離を comment として投稿する。
共有 runner は時間で失敗させるには揺らぎが大きすぎるため、警告のみである。

## 6. スタイルと静的解析

| コマンド | 検査内容 |
| --- | --- |
| `make format` | C++、Kotlin、Swift に format を適用する |
| `make lint` | 同じ検査を、ファイルを書き換えずに行い、続けて `lint-dead` を実行する —— CI が強制するのはこちら |
| `make lint-dead` | 不要コード：どこからも使われない C++ 関数、FFI 関数、文字列 id、Kotlin コード |
| `make lint-dead-swift` | 両 Apple アプリの不要な Swift コードを Periphery で検出する（macOS + Xcode） |
| `make lint-tidy` | `core/src` と `platform/src` に clang-tidy を適用する |

言語単位の派生もある。`format-cpp`、`lint-cpp`、`format-kotlin`、`lint-kotlin`、
`format-swift`、`lint-swift`。

不要コードはエラーとして扱う。Rust の `dead_code` lint と同じ考え方である。
`make lint-dead` は production がビルドするすべての C++ ファイルに cppcheck を適用し、
そこから一度も呼ばれない関数があれば失敗する。test、fuzzer、benchmark は数えないため、
test からしか呼ばれない関数も不要コードとなる。Swift、Kotlin、Objective-C++ からの呼び出し
は使用として数える。同じスクリプトは、どのアプリも呼ばない FFI 関数、どちらのアプリも表示
しない `DHStr*` 文字列 id、誰も読まない Kotlin 定数でも失敗し、detekt は使われていない
private な Kotlin コード、import、引数で失敗する。ある振る舞いを test が他の方法では
どうしても観察できない場合に限り、その accessor を残し、`scripts/dead-code-allow.txt` に
`name: どの test が必要とし、何を証明するか` を追記する。理由のない行や、production が
すでに呼ぶようになった関数の行も、検査を失敗させる。`make lint-dead-swift` は両 Apple
アプリをビルドして index を作り、その結果に Periphery を適用する。これらに加え、clang の
ビルドは使われない member function、template、exception 引数を警告し、`-Werror` が
それをエラーにする。`asan-msvc` 以外のすべての CMake preset が `DESKHUB_WERROR=ON` を
設定するため、ローカルの `make test` も CI と同じく warning で失敗する。

プロジェクトの規約（要約。完全版は `CLAUDE.md`）:

- C++20、コンパイラ拡張は使用しない。core は `deskhub`、platform は `deskhubp`。
- 関数と型は `PascalCase`、ローカル変数は `camelCase`、private メンバは末尾に
  アンダースコアを付ける。
- **どこにもコメントを書かない。** 代わりに説明的な名前、小さな関数、early return、
  名前付き定数を用いる。残す必要のある知識は、それがない場合に失敗する path のエラー
  メッセージ、または `ARCHITECTURE.md` に記載する。
- identifier と log メッセージはすべて英語で記述する。散文のドキュメントは 4 言語 ——
  英語、ベトナム語、中国語、日本語 —— で公開し、英語版を正文とする。

## 7. パッケージング

| コマンド | 生成物 |
| --- | --- |
| `make dist-macos` | Developer ID で sign し、notarize と staple を行った dmg |
| `make verify-macos` | 直前に build した成果物に対する Gatekeeper の検査 |
| `make dist-linux` | app と CLI それぞれの `.deb` と `.rpm`。各パッケージに uinput の udev rule を含む |

`dist-macos` には、keychain 内の *Developer ID Application* identity と、notarization
用の App Store Connect API key —— `ASC_KEY_P8`（`.p8` ファイルのパス）、`ASC_KEY_ID`、
`ASC_ISSUER_ID` —— が必要である。3 つのいずれかが欠けていれば build の前に停止する。

Release workflow は `packaging/windows/` から Windows app と CLI の Inno Setup インストーラー
も作成する。Windows ポータブル版と Linux app は引き続き単一ファイルである。

すべてのパッケージは `THIRD_PARTY_NOTICES.md` と、`licenses/` のうちそのパッケージに
該当するライセンス文を同梱する。対象は app と CLI の `.deb` と `.rpm`
（`/usr/share/doc/<package>/` の下に置き、LGPL を含むライセンス文はその `licenses/`
フォルダに置く）、両方の Windows インストーラー、macOS app の `Resources`、iOS の app
bundle、Android APK の assets である。Linux と Apple 向けの組は
`scripts/stage-licenses.sh` が配置し、Windows 向けは `packaging/windows/*.iss` が列挙し、
Android の build は自ら必要なものをコピーする。ポータブル版のバイナリには何も同梱され
ないため、リリースには `LICENSE`、notices、`licenses/` フォルダ全体を収めた
`deskhub-<tag>-licenses.zip` が加わる。`licenses/rust-crates.txt` は生成物であり、
quiche の pin が変わったら
`scripts/rust-crate-notices.py <quiche source dir> > licenses/rust-crates.txt` で書き直す。
APK には `licenses/android-libraries.txt`（Gradle が APK に同梱する Java / Kotlin ライブラリと
それぞれの notice）も入っており、これも生成物である。
`scripts/android-library-notices.py > licenses/android-libraries.txt` が `client/android` の
release runtime classpath を読む（`JAVA_HOME` を設定し、Gradle が artifact をキャッシュする
よう一度 Android をビルドしておく）ので、Android の依存が変わるたびに書き直す。

## 8. リリース

1. [`VERSION`](../VERSION) を更新する。tag とファイルが一致しない場合、
   `scripts/check-version.sh` が deploy を失敗させる。
2. 今回の変更が影響するドキュメントを、4 言語すべてで、同一の commit で更新する。
3. release notes（後述）を書いて commit する。
4. `vX.Y.Z` の tag を作成して push する。`.github/workflows/deploy.yml` が全
   プラットフォームを build し、GitHub Release を作成し、iOS を TestFlight へ、macOS
   を notarization へ、Android を Play の internal track へ送る。

**Release notes は手書きする。** tag ごとに 1 ファイル、`packaging/release-notes/vX.Y.Z.md`
で、ファイル名は tag と完全に一致させる。このファイルがそのまま GitHub Release の本文となり、
その後にインストールガイドとセキュリティモデルを指す固定の footer が続く。冒頭には更新後に
利用者が行うべきこと（breaking change、再接続の方法）を書き、その後に新機能と修正を書く。
tag に対応するファイルがない場合、`scripts/check-release-notes.sh` が何も build しないうちに
deploy を失敗させるので、tag を作る前に commit しておくこと。

### Package manager

GitHub Release の作成後、`deploy.yml` は tag を `publish-packages.yml` に渡し、その job が
公開を行う。新しい release を作らずに同じ tag を公開し直すには（これらの script を修正した
後など）、手動で `gh workflow run publish-packages.yml -f tag=vX.Y.Z` を実行する。

| Job | 公開先 | `stg` environment の secret |
| --- | --- | --- |
| `winget` | [komac](https://github.com/russellbanks/Komac)（`scripts/pinned-versions.txt` で pin）経由で、`ManhPham.Deskhub` と `ManhPham.DeskhubCLI` の pull request を `microsoft/winget-pkgs` に出す | `WINGET_TOKEN` —— `public_repo` を持つ classic PAT |
| `homebrew` | `manhpham90vn/homebrew-tap` の `Casks/deskhub.rb` と `Formula/deskhub-cli.rb`。`packaging/homebrew/` から生成する | `HOMEBREW_TAP_TOKEN` —— tap に Contents: write を持つ fine-grained PAT |
| `build-apt-repo` → `deploy-apt-repo` | 直近 3 つの release の deb を収めた署名付き apt repository。GitHub Pages の `/apt` に置く（`scripts/build-apt-repo.sh`） | `APT_GPG_PRIVATE_KEY`、`APT_GPG_PASSPHRASE` |

いずれも、それを実行する最初の tag より前に一度だけ設定が必要である。

- **winget** —— job は既存の package しか更新しないため、各 package の最初の版は手動で
  提出する。`ManhPham.Deskhub` には Inno Setup インストーラーを使用し、
  `ManhPham.DeskhubCLI` の portable command alias には `deskhub-cli` を選ぶ。
  Microsoft がその pull request を merge するまで、job は警告を出してスキップする。

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows-setup.exe
  ```

  `ManhPham.DeskhubCLI` と `deskhub-cli-vX.Y.Z-windows.exe` の URL でも同様に行う。
- **Homebrew** —— public repository `manhpham90vn/homebrew-tap` を作成する。中身は job が
  書き込む。
- **apt** —— signing key を一度だけ生成し、そのファイルを `APT_GPG_PRIVATE_KEY` として
  保存し、オフラインのバックアップも残す。全ユーザーがこの key を信頼しているため、
  差し替えると各ユーザーの `apt update` が失敗する。

  ```bash
  gpg --quick-gen-key "Deskhub APT <manhpv151090@gmail.com>" rsa4096 sign 5y
  gpg --armor --export-secret-keys "Deskhub APT" > deskhub-apt.key
  ```

  *Settings → Pages* で source を **GitHub Actions** にし、*Settings → Environments →
  github-pages* で `v*` に一致する tag を許可する。許可しないと Pages は tag からの
  deploy を拒否する。

## 9. CI が検査する内容

手元で `make test` と `make lint` が成功しても、それがすべてではない。各 pull request
では次を実行する。

- `core/src` と `platform/src` への clang-tidy、SwiftLint `--strict`、Android Lint
- workflow と `scripts/*.sh` への actionlint と shellcheck
- 不要コード：`scripts/dead-code.sh`（cppcheck、FFI / 文字列 id / Kotlin 定数の検査、
  detekt）と、両 Apple アプリへの Periphery
- 3 つの suite を Linux x64 と arm64（どちらも native build）、macOS、Windows で実行する。
  Linux では ASan/UBSan と TSan、macOS では ASan/UBSan の下でも実行し、Windows では
  platform と integration の suite を MSVC の ASan の下で実行する。さらに Android 向け
  （x86_64 は emulator で実行、arm64-v8a は build のみ）と iOS Simulator 向けに cross-build
  する
- `make cli-smoke` の script をデスクトップ 3 種すべての CLI に対して実行する
- Windows で integration suite 一式をさらに 3 回実行する。断続的に発生する memory
  corruption を特定するためであり、この問題は 3 回に 1 回程度しか現れず、1 回の実行では
  見落としやすい。crash が発生した frame はこの corruption の結果であって原因ではない
  ため、各 Windows job は symbol の隣に完全な minidump を書き出す。nightly では load
  test をさらに 2 巡する。1 巡は full page heap の下で、allocation を越える書き込みが
  それを引き起こした instruction で直ちに fault するようにする。もう 1 巡は Rust の
  debug assertion と overflow check を有効にして build した quiche を対象とする。
  quiche の内部を観察できる手段はこれだけである。ASan は Rust を instrument せず、
  page heap が保護するのは heap のみだからである
- core の coverage が line ≥ 90 %、branch ≥ 80 % であること
- libFuzzer の target を各 30 秒実行する（nightly は各 15 分）
- C++/Kotlin/Swift への CodeQL、履歴全体への gitleaks、および dependency review

## 10. 開発者向けツール

| コマンド | 機能 |
| --- | --- |
| `make icons` | `assets/icon_1024.png` からすべての client のアイコンを再生成する |
| `make quic-smoke` | quiche 静的ライブラリに対する単体の QUIC client と server |
| `make opus-smoke` | opus 静的ライブラリに対する単体の encode/decode 往復 —— 実際の bitrate、最大 packet、DTX の作動有無を報告する |
| `make screenshots` | macOS: iPhone/iPad simulator、Android emulator、macOS app でストア用スクリーンショットを再取得し、`docs/imgs` を更新する（一部のみの場合は `ARGS="ios android macos readme"`） |
| `make setup-linux-permissions` | `/dev/uinput` の udev rule と `input` group。source build から host するために使用する |
| `make reset-macos-permissions` | release（`com.deskhub.macos`）とローカルの Debug（`com.deskhub.macos.debug`）の両 bundle id の TCC の許可を消去し、app のコピーをそれぞれの署名方法とともに一覧する。build し直した後に Screen Recording や Accessibility が効かなくなったときに使う（`ARGS="--purge"` は build 済みのコピーも削除する） |
| `make ffmpeg-min` | Ubuntu: app と CLI が link する静的な最小構成 FFmpeg（`build-linux`、`release-linux`、CLI の target が自動的に実行する） |
| `make opus` | host target 用の Opus audio codec（`debug`、`release`、Linux・CLI・test の target が自動的に実行する） |
| `make clean` | `out/` を削除する |

## 11. build の問題への対処

- **CMake が quiche のライブラリがないとして停止する** —— 対応する `make quiche*`
  target を実行する。ABI ごとに専用のものが必要である。opus と `make opus*` も同様で
  ある。
- **macOS で `make fuzz` が libFuzzer を検出しない** —— Homebrew の LLVM が必要である。
  `make bootstrap` が導入し、それ以外は引き続き Xcode の toolchain で build される。
- **`make lint` の結果がエディタと一致しない** —— `make bootstrap` で CI と同じ
  バージョンのツールを入れ直し、`make format` を実行する。
- **Android の target が SDK を検出しない** —— `ANDROID_HOME` を設定したうえで
  `make bootstrap` を再実行する。別の NDK は `ANDROID_NDK_VERSION=<v>` で指定できる。
- **build し直したあと、またはローカル build とダウンロード版を切り替えたあと macOS の
  permission が正しく動作しない** —— `make reset-macos-permissions` を実行し、残す 1 つの
  コピーに改めて許可を与える。

バグ報告と質問: [issues](https://github.com/manhpham90vn/Deskhub/issues)。
