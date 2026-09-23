[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · [中文](BUILD.zh.md) · **日本語**

# Deskhub — ビルドと開発

本書は、Deskhub を自分でコンパイルし、test suite を実行し、release を作成するために
必要な事項をまとめたものである。app を*使用する*だけであれば、
[`INSTALL.ja.md`](INSTALL.ja.md) から build 済みのものを入手するほうがよい。

本書は [`BUILD.md`](BUILD.md) の翻訳。食い違いがある場合は英語版が正文。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 一度だけ: この OS 向けの toolchain と依存関係
make test             # core suite を build し、オフラインで実行
make build-linux      # または build-windows / build-macos / build-ios / build-android
```

暗黙に build されるプラットフォームはない。引数なしの `make` は target の一覧を表示
するだけで、build は行わない。各 target は [`Makefile`](../Makefile) の冒頭に完全な
説明がある。引数なしの `make` が表示するのは `make/help.txt` である。

---

## 1. 事前に必要なもの

`make bootstrap` は自動で導入できるものを導入し、できないものは通知する。実行前に次を
自分で用意しておく必要がある。

| 手元の OS | 事前に導入するもの | bootstrap が導入するもの |
| --- | --- | --- |
| **Ubuntu / Debian** | apt 以外に不要。加えて [Rust](https://rustup.rs) | build-essential、clang、llvm、cmake、ninja、JDK 17、GTK3 / PipeWire / VA-API / tray の `-dev` package、VA-API driver、GNOME portal、静的な最小構成 FFmpeg、quiche、opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode と command line tools、[Rust](https://rustup.rs) | cmake、ninja、swiftlint、pipx、Homebrew の LLVM（Apple clang には libFuzzer runtime が含まれない）、Temurin JDK 17、Apple 向けと Android 向けの quiche および opus |
| **Windows** | winget（App Installer）、C++ toolchain と *C++ Clang tools* コンポーネントを含む Visual Studio、[Rust](https://rustup.rs) | 残りは winget 経由で、`scripts/bootstrap.ps1` が実行する |

いずれの OS でも、bootstrap は style と解析のツールを pin する。clang-format、
clang-tidy、ktlint、SwiftFormat、cppcheck、detekt がそれぞれ固定バージョンで、checksum
の検証を伴う。これらを手動で導入してはならない。CI が突き合わせるのはこのバージョンだから
である。Swift の不要コードを探す Periphery は、`make lint-dead-swift` を初めて実行した
ときに同じ方法で取得される。

モバイル向けの target には追加の要件がある。`build-android` には NDK を含む Android
SDK が必要である（`ANDROID_HOME` が cmdline-tools のインストール先を指していれば、
bootstrap が SDK package を導入する）。`build-ios` には Simulator runtime を含む Xcode
が必要である。

git submodule は `nvenc` のヘッダのみである。clone 時に `--recurse-submodules` を
付けるか、後から `git submodule update --init` を実行すればよい。`make bootstrap` も
これを同期する。

## 2. ツリーの構成

```
core/       プラットフォーム非依存の C++20 —— protocol、packetization、FEC、session state、
            input mapping、bitrate control、VT emulator。OS ヘッダなし。unit test あり。
platform/   OS 向けの薄い abstraction で、API は 1 種類 —— socket、clock、logging、
            random、source の列挙。core に依存する。
client/     5 つの app: android、ios、linux、macos、windows。
            client/apple/ は macOS と iOS の app が共有する Swift であり、app ではない。
            client/cli/ は command line client。デスクトップ 3 種を binary 1 つで担う。
third_party/  quiche (QUIC)、opus (audio)、nvenc のヘッダ、最小構成の FFmpeg build
make/       プラットフォームごとに .mk を 1 つ。ルートの Makefile が include する
scripts/    bootstrap、パッケージング、coverage、style、CI 用の補助スクリプト
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
make lint      # C++・Kotlin・Swift の format を検査する。ファイルは書き換えない
```

変更を完了と判断する前に、両方を実行すること。`make format` は検査ではなく format を
実際に適用する。手作業での整形は行わないこと。ツールのバージョンを固定しているのには
理由がある。

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
する。デスクトップの app は command line のフラグを一切解釈せず、選択はすべて 4 つの
ページで行う。`run-android` は adb 経由で接続中の端末または emulator にインストールして
起動し、`run-ios` は Simulator で同じ処理を行う。

### Command line client

`client/cli/` は `deskhub-cli` という binary を 1 つ build する。同じ機能を GUI
toolkit なしで提供し、ページではなくフラグで制御する。SSH 経由、スクリプト内、
systemd 配下で Deskhub を動かす場合はこれを使用する。

```bash
make build-cli                       # この OS 向けの debug build
make release-cli                     # 最適化版
make run-cli ARGS="scan"             # build したうえで、その引数で実行
```

この target は `-DDESKHUB_CLI=ON` の後ろにあり（既定では無効）、app および sanitizer・
coverage・fuzz の preset には影響しない。有効にすると、OS ごとの media ライブラリが
任意から必須に変わる。capture も decode もできない client は client として成立しない
ためである。

| コマンド | 機能 |
| --- | --- |
| `share` | このマシンを共有する —— 任意の display、shell、またはその両方 |
| `connect ADDRESS` | host の画面を表示するウィンドウを開き、操作する |
| `shell ADDRESS` | 現在の terminal 内で host 上の shell を開く |
| `displays`、`scan`、`sources`、`probe` | 共有可能な対象と、ネットワーク上のマシン |
| `devices`、`trust`、`settings` | デスクトップ app が読み書きするのと同じファイル |

フラグは `deskhub-cli help COMMAND` が表示する。すべてのコマンドが `--json` に対応し、
exit code が失敗の理由を示す。`2` フラグの誤り、`3` 応答なし、`4` 拒否、`5` host key
の変更、`9` この build では未対応。

OS ごとの現状は次のとおりである。Linux はすべての機能に対応する。Windows は share と
connect に対応し、デスクトップ app 既存のウィンドウコードを再利用する。macOS は share
と shell の起動に対応するが、`connect` には未実装の window layer が必要であり、その旨
を報告する。

`core/` と `platform/` のみを扱う場合は、共有の CMake ツリーのほうが高速である。

```bash
make debug        # debug preset を configure して build する
make release      # …release preset を
```

**quiche と opus は ABI ごとに build する。** QUIC transport は `third_party/quiche`
で build される Rust の静的ライブラリであり、これがないと share も connect もできない。
Opus audio codec は `third_party/opus` で build される C の静的ライブラリであり、これが
ないと共有に音声が含まれない。`debug`、`release`、およびすべての `build-*` target は
必要な ABI を先に build し、既に build 済みであれば何も行わない。`make quiche`、
`quiche-android`、`quiche-ios`、`quiche-macos`、および対応する `opus`、
`opus-android`、`opus-ios`、`opus-macos` はこれらの工程を単独で実行する。quiche が
ないときに CMake が停止するのは意図的である。connect できない binary の生成を拒否して
いる。

## 5. テスト

| コマンド | 実行環境 | 対象範囲 |
| --- | --- | --- |
| `make test` | オフライン、socket なし | `core/` の全体: wire format、framing、FEC、session、VT emulator、settings、文字列 |
| `make test-platform` | loopback socket | 実際の QUIC handshake、end-to-end の SPAKE2、ネットワーク越しの terminal host と viewer、実 shell に対する PTY、lockout、approval |
| `make test-integration` | loopback、capture/encode は模擬実装 | host↔client の session 一式: negotiation、ネットワーク越しの video、input、passcode と approval による制御、不正データへの耐性 |
| `make test-all` | 3 つの suite すべて。core が先 | |
| `make test-ctest` | 同じ test を CTest 経由で実行 | CI の呼び出し方と同一 |
| `make test-asan` | 3 つの suite を ASan と UBSan の下で実行 | clang/gcc のみ。MSVC は非対応 |
| `make test-tsan` | 3 つの suite を ThreadSanitizer の下で実行 | clang/gcc のみ。MSVC は非対応 |
| `make test-perf` | release build、オフラインと loopback | hot path を実測する: `core_perf` は packetize/reassemble/FEC、1080p の縮小、CRC とファイルのバッチ、VT parser と screen、wire の encode/decode、audio jitter buffer を対象とし、`platform_perf` は loopback 上の実際の QUIC を対象とする |

これらの test suite には、リモートの peer、GPU、network を必要とするものはない。

**Coverage。** `make coverage` は clang と llvm-cov で `core/` のレポートを生成する。
`scripts/check-coverage.sh` は CI と同じ基準を適用する。**line ≥ 90 %、branch ≥ 80 %**
である。

**Fuzzing。** `make fuzz` は libFuzzer の target を実行する。対象は wire、H.264、
reassembly、terminal の byte stream、UI テキストの parser、および host 側と viewer 側の
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
  `out/perf/baseline.txt` を生成し、以後の実行は行ごとに変化を報告して、25 % を超えた
  時点で失敗する。このファイルは特定の 1 台を記述したものなので git には含めない。

計測時間側の調整には `DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、
`DESKHUB_PERF_BASELINE`、`DESKHUB_PERF_WRITE` を使用する。`make test` も CI もこの部分
は実行しない。debug、ASan、coverage の build は production の速度を反映しないためで
ある。

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
`名前: どの test が必要とし、何を証明するか` を追記する。理由のない行や、production が
すでに呼ぶようになった関数の行も、検査を失敗させる。`make lint-dead-swift` は両 Apple
アプリをビルドして index を作り、その結果に Periphery を適用する。これらに加え、clang の
ビルドは使われない member function、template、exception 引数を警告し、CI の `-Werror`
がそれをエラーにする。

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
| `make dist-linux` | `.deb` と `.rpm`。いずれも uinput の udev rule を導入する |

Windows と Linux の app はそれぞれファイル 1 つであり、build すべき installer はない。

## 8. リリース

1. [`VERSION`](../VERSION) を更新する。tag とファイルが一致しない場合、
   `scripts/check-version.sh` が deploy を失敗させる。
2. 今回の変更が影響するドキュメントを、全言語分、同一の commit で更新する。
3. `vX.Y.Z` の tag を作成して push する。`.github/workflows/deploy.yml` が全
   プラットフォームを build し、GitHub Release を作成し、iOS を TestFlight へ、macOS
   を notarization へ、Android を Play の internal track へ送る。

**Release notes は、前回の tag から今回の tag までの commit subject から生成される。**
担当するのは `scripts/changelog.sh` である。ある tag が何を生成するかは手元で確認でき
る。

```bash
scripts/changelog.sh v5.0.0     # 引数を省略すると HEAD の tag を使用する
```

したがって commit subject は利用者が読む文章であり、その先頭に付ける
conventional-commit の type が、どのセクションに分類されるかを決める。完全な対応表、
それを上書きする規則、実例は
[`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md) にある。subject
を書く前に参照すること。空のセクションは release に含まれず、
`INCLUDE_INTERNAL=1 scripts/changelog.sh` で省かれた commit を確認できる。

### Package manager

GitHub Release の作成後、`deploy.yml` のさらに 3 つの job がそれを公開する。

| Job | 公開先 | `stg` environment の secret |
| --- | --- | --- |
| `publish-winget` | [komac](https://github.com/russellbanks/Komac)（`scripts/pinned-versions.txt` で pin）経由で、`ManhPham.Deskhub` と `ManhPham.DeskhubCLI` の pull request を `microsoft/winget-pkgs` に出す | `WINGET_TOKEN` —— `public_repo` を持つ classic PAT |
| `publish-homebrew` | `manhpham90vn/homebrew-tap` の `Casks/deskhub.rb` と `Formula/deskhub-cli.rb`。`packaging/homebrew/` から生成する | `HOMEBREW_TAP_TOKEN` —— tap に Contents: write を持つ fine-grained PAT |
| `build-apt-repo` → `deploy-apt-repo` | 直近 3 つの release の deb を収めた署名付き apt repository。GitHub Pages の `/apt` に置く（`scripts/build-apt-repo.sh`） | `APT_GPG_PRIVATE_KEY`、`APT_GPG_PASSPHRASE` |

いずれも、それを実行する最初の tag より前に一度だけ設定が必要である。

- **winget** —— job は既存の package しか更新しないため、各 package の最初の版は手動で
  提出し、komac に聞かれたら portable command alias に `deskhub` / `deskhub-cli` を選ぶ。
  Microsoft がその pull request を merge するまで、job は警告を出してスキップする。

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows.exe
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
- 3 つの suite を ASan/UBSan と TSan の下で実行し、さらに arm64 Linux、Android
  emulator、iOS Simulator 向けに cross-build する
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
| `make reset-macos-permissions` | ローカル build とダウンロード版が同一の bundle id を共有する場合に TCC の許可を消去する（`ARGS="--purge"` は build 済みのコピーも削除する） |
| `make ffmpeg-min` | Ubuntu: app が link する静的な最小構成 FFmpeg（`build-linux` が自動的に実行する） |
| `make opus` | host target 用の Opus audio codec（`debug`、`release`、`build-linux` が自動的に実行する） |
| `make clean` | `out/` を削除する |

## 11. build の問題への対処

- **CMake が quiche のライブラリがないとして停止する** —— 対応する `make quiche*`
  target を実行する。ABI ごとに専用のものが必要である。opus と `make opus*` も同様で
  ある。
- **macOS で `make fuzz` が libFuzzer を検出しない** —— Homebrew の LLVM が必要である。
  `make bootstrap` が導入し、それ以外は引き続き Xcode の toolchain で build される。
- **`make lint` の結果がエディタと一致しない** —— バージョンを固定したツールが基準で
  ある。`make bootstrap` を再実行して正確なバージョンを取得し、`make format` を実行
  する。
- **Android の target が SDK を検出しない** —— `ANDROID_HOME` を設定したうえで
  `make bootstrap` を再実行する。別の NDK は `ANDROID_NDK_VERSION=<v>` で指定できる。
- **ローカル build とダウンロード版を切り替えたあと macOS の permission が正しく動作
  しない** —— `make reset-macos-permissions` を実行する。

バグ報告と質問: [issues](https://github.com/manhpham90vn/Deskhub/issues)。
