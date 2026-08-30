[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · [中文](BUILD.zh.md) · **日本語**

# Deskhub — ビルドと開発

Deskhub を自分でコンパイルし、テストスイートを走らせ、リリースを切るために必要なすべて。
アプリを*使う*だけなら、ビルド済みのものを [`INSTALL.ja.md`](INSTALL.ja.md) から取れば
いい。

本書は [`BUILD.md`](BUILD.md) の翻訳である。相違がある場合は英語版が正典となる。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 一度だけ：この OS 向けのツールチェーンと依存
make test             # コアスイートをオフラインでビルドして実行
make build-linux      # または build-windows / build-macos / build-ios / build-android
```

暗黙にビルドされるプラットフォームはない。素の `make` はターゲット一覧を表示するだけで
何もビルドしない。すべてのターゲットは [`Makefile`](../Makefile) の先頭に完全な形で
記されており、素の `make` が表示するのは `make/help.txt` である。

---

## 1. 先に必要なもの

`make bootstrap` は入れられるものを入れ、入れられないものを教えてくれる。走らせる前に
これらは自分で入れておくこと：

| ホスト OS | 先に入れるもの | bootstrap がやること |
| --- | --- | --- |
| **Ubuntu / Debian** | apt 以外に必要なし。[Rust](https://rustup.rs) | build-essential、clang、llvm、cmake、ninja、JDK 17、GTK3 / PipeWire / VA-API / トレイの `-dev` パッケージ、VA-API ドライバ、GNOME portal、静的な最小 FFmpeg、quiche、opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode + コマンドラインツール、[Rust](https://rustup.rs) | cmake、ninja、swiftlint、pipx、Homebrew の LLVM（Apple clang には libFuzzer ランタイムが入っていない）、Temurin JDK 17、Apple と Android 向けの quiche と opus |
| **Windows** | winget（App Installer）、C++ ツールチェーンと *C++ Clang tools* コンポーネント入りの Visual Studio、[Rust](https://rustup.rs) | 残りは `scripts/bootstrap.ps1` が winget で入れる |

どの OS でもスタイルツールをピン留めする：clang-format、clang-tidy、ktlint、SwiftFormat
をそれぞれ固定バージョンでチェックサム検証つきで入れる — 手で入れてはいけない。CI が
比較するのはまさにこれらのバージョンである。

モバイル向けはさらに要る。`build-android` には NDK 入りの Android SDK が必要で
（`ANDROID_HOME` が cmdline-tools のインストール先を指していれば bootstrap が SDK
パッケージを入れる）、`build-ios` にはシミュレータランタイム入りの Xcode が必要。

git サブモジュールは `nvenc` ヘッダだけ。クローン時の `--recurse-submodules`、または
`git submodule update --init` で足りる。`make bootstrap` も同期してくれる。

## 2. ツリーの構成

```
core/       プラットフォーム非依存の C++20 — プロトコル、パケット化、FEC、セッション状態、
            入力マッピング、ビットレート制御、VT エミュレータ。OS ヘッダなし。ユニットテスト済み。
platform/   ひとつの共通 API の背後にある薄い OS 抽象 — ソケット、クロック、ログ、
            乱数、ソース列挙。core に依存。
client/     5 つのアプリ：android、ios、linux、macos、windows。
            client/apple/ は macOS と iOS のアプリが共有する Swift で、それ自体はアプリではない。
            client/cli/ はコマンドラインクライアント。デスクトップ 3 種で 1 バイナリ。
third_party/  quiche（QUIC）、opus（音声）、nvenc ヘッダ、最小 FFmpeg ビルド
make/       プラットフォームごとに 1 つの .mk。ルート Makefile が include する
scripts/    bootstrap、パッケージング、カバレッジ、スタイル、CI 補助
.github/    ワークフローと、それらが共用する複合ステップ actions/
```

ロジックは一度書いて共有する。`client/*` の下に何かを足す前に、それが `core/`
（プラットフォーム非依存）か `platform/`（OS が要るが API はどこでも同じ）に属さないかを
確かめること。[`ARCHITECTURE.ja.md`](ARCHITECTURE.ja.md) がレイヤ、スレッドモデル、
ワイヤプロトコルを説明し、`CLAUDE.md` がこのリポジトリで強制されるルールを定めている。

## 3. 日々のループ

```bash
make test      # コアスイート。オフライン、GPU もネットワークも不要 — 数秒
make lint      # C++、Kotlin、Swift の書式チェック。書き換えはしない
```

変更を「終わった」と見なす前に両方を走らせる。`make format` はチェックではなく書式を
適用する — 手で整形しないこと。ツールがピン留めされているのには理由がある。

`core/` に新しいロジックを足したら、対応する `core/tests/` のサブディレクトリにテストが
要る。

## 4. アプリのビルドと実行

| ターゲット | 作るもの | 必要なもの |
| --- | --- | --- |
| `make build-windows` | `Deskhub.exe` 1 つ | Windows + MSVC |
| `make build-macos` | macOS アプリ | macOS + Xcode |
| `make build-linux` | `deskhub` バイナリ 1 つ | Ubuntu + `-dev` パッケージ |
| `make build-ios` | シミュレータ向け iOS アプリ | macOS + Xcode + シミュレータランタイム |
| `make build-android` | デバッグ APK | Android SDK + NDK、`adb` |

それぞれに `release-<os>`（最適化）と `run-<os>`（ビルドして起動）の兄弟がある。
デスクトップアプリはコマンドラインフラグを一切解釈しない — すべては 4 つの画面上で
選ぶ。`run-android` は adb 経由で接続中の実機かエミュレータにインストールして開き、
`run-ios` はシミュレータで同じことをする。

### コマンドラインクライアント

`client/cli/` は GUI ツールキットなしで同じ仕事をする `deskhub-cli` バイナリを 1 つ
ビルドする。画面ではなくフラグで駆動する。SSH 越し、スクリプトの中、あるいは systemd の
下で Deskhub を動かすならこれである。

```bash
make build-cli                       # この OS 向けのデバッグビルド
make release-cli                     # 最適化
make run-cli ARGS="scan"             # ビルドしてその引数で実行
```

`-DDESKHUB_CLI=ON`（既定はオフ）の後ろにあるので、アプリ本体もサニタイザ・カバレッジ・
fuzz の各プリセットもこれに影響されない。オンにすると OS ごとのメディアライブラリが
任意ではなく必須になる。キャプチャもデコードもできないクライアントはクライアントでは
ないからである。

| コマンド | 動作 |
| --- | --- |
| `share` | このマシンを共有する — 任意のディスプレイ、シェル、または両方 |
| `connect ADDRESS` | ホストの画面をウィンドウで開いて操作する |
| `shell ADDRESS` | いま居るターミナルの中に、ホストのシェルを開く |
| `displays`、`scan`、`sources`、`probe` | 何が共有でき、外に誰がいるか |
| `devices`、`trust`、`settings` | デスクトップアプリが読み書きするのと同じファイル |

`deskhub-cli help COMMAND` がフラグを表示する。すべてのコマンドが `--json` に応じ、
終了コードが何が悪かったかを示す：`2` フラグ不正、`3` 応答なし、`4` 拒否、`5` ホスト鍵が
変わった、`9` このビルドではできない。

現在の OS ごとの状況：Linux はすべてできる。Windows はデスクトップアプリが既に使っている
のと同じウィンドウコードで共有と接続をする。macOS は共有とシェルはできるが、`connect`
にはまだ書かれていないウィンドウ層が必要で、その旨を報告する。

`core/` と `platform/` だけを触るなら、共有 CMake ツリーのほうが速い：

```bash
make debug        # デバッグプリセットの configure + build
make release      # …リリースプリセット
```

**quiche と opus は ABI ごと。** QUIC トランスポートは `third_party/quiche` にビルド
される Rust の静的ライブラリで、これなしでは共有も接続もできない。Opus 音声コーデックは
`third_party/opus` にビルドされる C の静的ライブラリで、これなしでは共有に音が乗らない。
`debug`、`release`、すべての `build-*` ターゲットは必要な ABI を先にビルドし、一度
ビルドされていれば何もしない — `make quiche`、`quiche-android`、`quiche-ios`、
`quiche-macos` と、対応する `opus`、`opus-android`、`opus-ios`、`opus-macos` で単独に
実行することもできる。CMake が quiche 不在のエラーで止まるなら、それは意図的である。
決して接続できないバイナリを作ることを拒んでいる。

## 5. テスト

| コマンド | 実行環境 | 対象 |
| --- | --- | --- |
| `make test` | オフライン、ソケットなし | `core/` のすべて：ワイヤフォーマット、フレーミング、FEC、セッション、VT エミュレータ、設定、文字列 |
| `make test-platform` | ループバックソケット | 実際の QUIC ハンドシェイク、SPAKE2 のエンドツーエンド、ターミナルのホスト + ビューア、実シェルに対する PTY、ロックアウト、承認 |
| `make test-integration` | ループバック、疑似キャプチャ/エンコード | ホスト↔クライアントの完全なセッション：ネゴシエーション、映像の伝送、入力、パスコードと承認のゲート、ゴミ耐性 |
| `make test-all` | 3 つすべて、core が先 | |
| `make test-ctest` | 同じテストを CTest 経由で | CI がまさにこう呼び出す |
| `make test-asan` | 3 つすべてを ASan + UBSan の下で | clang/gcc のみ、MSVC は不可 |
| `make test-tsan` | 3 つすべてを ThreadSanitizer の下で | clang/gcc のみ、MSVC は不可 |
| `make test-perf` | リリースビルド、オフライン + ループバック | ホットパスを走らせるだけでなく測る：`core_perf` がパケット化/再組み立て/FEC、1080p 縮小、CRC とファイルのバッチ、VT パーサと画面、ワイヤの符号化/復号、音声ジッタバッファを、`platform_perf` がループバック上の実 QUIC を覆う |

テストスイートのどれも、対向のピアも GPU もネットワークも必要としない。

**カバレッジ。** `make coverage` は clang + llvm-cov で `core/` のレポートを作る。
`scripts/check-coverage.sh` が CI と同じゲートをかける — **行 90 % 以上、分岐 80 %
以上**。

**ファジング。** `make fuzz` は、ワイヤ、H.264、再組み立て、ターミナルバイト、UI テキスト
の各パーサと、ホストおよびビューアのセッション状態機械に対して libFuzzer ターゲットを
走らせる（clang、Linux/macOS。ターゲットごとに `FUZZ_SECONDS=N`）。各ターゲットはまず
`core/fuzz/regressions/<target>` を再生して、直したクラッシュが戻らないことを確かめ、
それからコミット済みのシードと辞書からファジングする。`make fuzz-coverage` はコーパスが
実際に到達している core の行を示す。見つかったクラッシュはすべて回帰入力になる。

**性能。** `make test-perf` はリリースプリセットで 2 つの性能バイナリをビルドして走らせる：
`core_perf` が純 C++ のホットパスで 37 のワークロードを測り、続いて `platform_perf` が
ループバック上の実 QUIC でさらに 6 つを測る。全体で数秒。どちらかを落とす条件は 3 つあり、
どれも空から降ってきたミリ秒の数字ではない：

- **単位あたりの確保回数**。グローバルな `operator new` を差し替えて厳密に数える。
  パケットごと・フレームごとに確保し始めた経路は、どのマシンでも、どの実行でも落ちる。
- **コストの伸び方**。`-scaling` の各行は同じ仕事を 4 倍の入力で走らせ、時間が入力より
  はるかに速く伸びたら落とす — うっかり書いた O(n²) の形である。
- **記録したベースラインからのずれ**。`make perf-baseline` がアイドルなマシンで
  `out/perf/baseline.txt` を書き、以後の実行は各行の変化を報告して 25 % を超えたら
  落ちる。このファイルはその 1 台を記述するものなので、git には入れない。

`DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、`DESKHUB_PERF_BASELINE`、
`DESKHUB_PERF_WRITE` が計測側を調整する。`make test` も CI もこれらは一切走らせない。
デバッグ、ASan、カバレッジのビルドは製品の速度について何も語らないからである。

## 6. スタイルと静的解析

| コマンド | 何を見るか |
| --- | --- |
| `make format` | C++、Kotlin、Swift に書式を適用する |
| `make lint` | 書き換えずに同じ検査 — CI が強制するもの |
| `make lint-tidy` | `core/src` + `platform/src` に clang-tidy |

単一言語版もある：`format-cpp`、`lint-cpp`、`format-kotlin`、`lint-kotlin`、
`format-swift`、`lint-swift`。

ハウスルールの要約 — 完全版は `CLAUDE.md` にある：

- C++20、コンパイラ拡張なし。core は `deskhub`、platform は `deskhubp`。
- 関数と型は `PascalCase`、ローカルは `camelCase`、private メンバは末尾にアンダースコア。
- **どこにもコメントを書かない。** 代わりに説明的な名前、小さな関数、早期 return、
  名前付き定数を使う。失われては困る知識は、それがないと失敗する経路のエラーメッセージか
  `ARCHITECTURE.md` に入れる。
- 識別子とログメッセージはすべて英語。散文の文書は 4 言語 — 英語、ベトナム語、中国語、
  日本語 — で出し、英語が正典。

## 7. パッケージング

| コマンド | 作るもの |
| --- | --- |
| `make dist-macos` | Developer ID で署名し、公証してステープルした dmg |
| `make verify-macos` | いまビルドしたものに対する Gatekeeper チェック |
| `make dist-linux` | `.deb` と `.rpm`。どちらも uinput の udev 規則を入れる |

Windows と Linux のアプリはそれぞれ 1 ファイル。ビルドすべきインストーラはない。

## 8. リリース

1. [`VERSION`](../VERSION) を上げる — タグとファイルが食い違うと
   `scripts/check-version.sh` がデプロイを落とす。
2. 変更が触れる文書を、全言語分、同じコミットで更新する。
3. `vX.Y.Z` のタグを打って push する。`.github/workflows/deploy.yml` が全プラットフォームを
   ビルドし、GitHub Release を作り、iOS を TestFlight へ、macOS を公証へ、Android を Play
   の internal トラックへ送る。

**リリースノートは、前のタグから今回のタグまでのコミット件名から** `scripts/changelog.sh`
が生成する。あるタグが何を生むかはローカルで確認できる：

```bash
scripts/changelog.sh v5.0.0     # 引数なしなら HEAD のタグに対して
```

つまりコミット件名はユーザーの目に触れるものであり、その前に付く conventional-commit の
型がどの節に入るかを決める。完全な対応表、それを上書きするルール、実例は
[`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md) にある — 件名を
書く前に読むこと。空の節はリリースから省かれ、`INCLUDE_INTERNAL=1 scripts/changelog.sh`
で省かれたコミットが見える。

## 9. CI が守らせること

ローカルの `make test` + `make lint` が緑でも話は終わらない。すべてのプルリクエストで：

- `core/src` + `platform/src` への clang-tidy、SwiftLint `--strict`、Android Lint
- ワークフローと `scripts/*.sh` への actionlint + shellcheck
- 3 つのスイートを ASan/UBSan と TSan の下で。さらに arm64 Linux、Android エミュレータ、
  iOS シミュレータ向けのクロスビルド
- 統合スイート全体を Windows でさらに 3 回。3 回に 1 回ほど現れて 1 回の実行では
  すり抜ける、`DrainStreams` の間欠的なスタック破壊を追うため
- core のカバレッジが行 90 % / 分岐 80 % 以上
- libFuzzer の各ターゲットを 30 秒ずつ（夜間は各 15 分）
- C++/Kotlin/Swift への CodeQL、履歴全体への gitleaks、依存関係レビュー

## 10. 開発ツール

| コマンド | 動作 |
| --- | --- |
| `make icons` | `assets/icon_1024.png` から全クライアントのアイコンを再生成 |
| `make quic-smoke` | quiche 静的ライブラリに対する単体の QUIC クライアント + サーバ |
| `make opus-smoke` | opus 静的ライブラリに対する単体のエンコード/デコード往復 — 実ビットレート、最大パケット、DTX が効いているかを報告 |
| `make screenshots` | macOS：iPhone/iPad シミュレータ、Android エミュレータ、macOS アプリでストア用スクリーンショットを撮り直し、`docs/imgs` を更新（一部だけなら `ARGS="ios android macos readme"`） |
| `make setup-linux-permissions` | ソースビルドからホストするための `/dev/uinput` udev 規則 + `input` グループ |
| `make reset-macos-permissions` | ローカルビルドとダウンロード版が同じ bundle id を取り合うときに TCC の許可を消す（`ARGS="--purge"` はビルド済みのコピーも削除） |
| `make ffmpeg-min` | Ubuntu：アプリがリンクする静的な最小 FFmpeg（`build-linux` が自動で実行） |
| `make opus` | ホストターゲット向けの Opus 音声コーデック（`debug`、`release`、`build-linux` が自動で実行） |
| `make clean` | `out/` を削除 |

## 11. ビルドが手向かってきたら

- **CMake が quiche ライブラリ不在で止まる** — 対応する `make quiche*` ターゲットを
  走らせる。ABI ごとに必要である。opus と `make opus*` も同じ。
- **macOS で `make fuzz` が libFuzzer を見つけない** — Homebrew の LLVM が要る。
  `make bootstrap` が入れる。それ以外は Xcode のツールチェーンでビルドし続ける。
- **`make lint` がエディタと食い違う** — ピン留めしたツールが正しい。`make bootstrap` を
  もう一度走らせて正確なバージョンを取り、`make format` する。
- **Android ターゲットが SDK を見つけられない** — `ANDROID_HOME` を設定して
  `make bootstrap` をやり直す。`ANDROID_NDK_VERSION=<v>` で別の NDK を選べる。
- **ローカルビルドとダウンロード版を行き来した後、macOS の権限がおかしい** —
  `make reset-macos-permissions`。

不具合と質問：[issues](https://github.com/manhpham90vn/Deskhub/issues)。
