[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · **中文** · [日本語](BUILD.ja.md)

# Deskhub —— 构建与开发

本文档说明自行编译 Deskhub、运行各 test suite 以及发布 release 所需的全部内容。若只需
*使用* app，请按 [`INSTALL.zh.md`](INSTALL.zh.md) 获取预先 build 好的版本。

本文件是 [`BUILD.md`](BUILD.md) 的译本；若两者有出入，以英文版为准。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 执行一次: 当前 OS 的 toolchain 与依赖
make test             # build 并离线运行 core suite
make build-linux      # 或 build-windows / build-macos / build-ios / build-android
```

任何平台都不会被隐式 build：不带参数的 `make` 只打印 target 列表，不执行构建。每个
target 在 [`Makefile`](../Makefile) 开头都有完整说明，而 `make/help.txt` 即为不带参数的
`make` 所显示的内容。

---

## 1. 前置准备

`make bootstrap` 会安装能够自动安装的部分，并提示其余部分。运行之前需先自行准备以下
内容：

| 本机 OS | 需先安装 | bootstrap 随后安装 |
| --- | --- | --- |
| **Ubuntu / Debian** | 除 apt 外无需其他；[Rust](https://rustup.rs) | build-essential、clang、llvm、cmake、ninja、JDK 17，GTK3 / PipeWire / VA-API / tray 的 `-dev` package，VA-API driver，GNOME portal，静态的最小化 FFmpeg，quiche，opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode 与 command line tools、[Rust](https://rustup.rs) | cmake、ninja、swiftlint、pipx、Homebrew 的 LLVM（Apple clang 不含 libFuzzer runtime）、Temurin JDK 17，以及面向 Apple 与 Android 的 quiche 和 opus |
| **Windows** | winget（App Installer）、含 C++ toolchain 与 *C++ Clang tools* 组件的 Visual Studio、[Rust](https://rustup.rs) | 其余通过 winget 安装，由 `scripts/bootstrap.ps1` 执行 |

在所有 OS 上，bootstrap 还会 pin 住 style 与分析工具：clang-format、clang-tidy、ktlint、
SwiftFormat、cppcheck 和 detekt，各自固定版本并校验 checksum。请勿手动安装这些工具，CI
比对的正是这些确切版本。Swift 无用代码工具 Periphery 会在首次运行
`make lint-dead-swift` 时以同样方式获取。

移动端 target 另有要求：`build-android` 需要含 NDK 的 Android SDK（当 `ANDROID_HOME`
指向一份 cmdline-tools 安装时，bootstrap 会安装相应 SDK package），`build-ios` 需要含
Simulator runtime 的 Xcode。

git submodule 只有 `nvenc` 头文件一项。clone 时加 `--recurse-submodules`，或事后执行
`git submodule update --init` 即可。`make bootstrap` 也会同步该 submodule。

## 2. 目录结构

```
core/       与平台无关的 C++20 —— protocol、packetization、FEC、session state、
            input mapping、bitrate control、VT emulator。不含 OS 头文件。有 unit test。
platform/   面向 OS 的薄 abstraction，对外只有一套 API —— socket、clock、logging、
            random、source 枚举。依赖 core。
client/     五个 app: android、ios、linux、macos、windows。
            client/apple/ 是 macOS 与 iOS app 共用的 Swift，本身不是 app。
            client/cli/ 是 command line client，一个 binary 覆盖三个桌面平台。
third_party/  quiche (QUIC)、opus (audio)、nvenc 头文件、最小化的 FFmpeg build
make/       每个平台一个 .mk，由根 Makefile include
scripts/    bootstrap、打包、coverage、style 以及 CI 用的辅助脚本
.github/    workflow，以及供其共用的 composite step（actions/）
```

逻辑只编写一次并共享使用。向 `client/*` 添加代码之前，需先判断它是否属于 `core/`（与平
台无关）或 `platform/`（需要 OS，但各平台 API 相同）。
[`ARCHITECTURE.zh.md`](ARCHITECTURE.zh.md) 说明分层、threading model 与 wire
protocol；`CLAUDE.md` 列出本 repo 强制执行的规则。

## 3. 日常流程

```bash
make test      # core suite，离线，无需 GPU 和 network —— 数秒完成
make lint      # 检查 C++、Kotlin 和 Swift 的 format，不写回文件
```

在认定一处改动完成之前，两者都需运行。`make format` 会实际应用 format，而不仅是检查。
请勿手动排版，工具固定版本是有意为之。

`core/` 中新增的逻辑，需在 `core/tests/` 对应的子目录中配套 test。

## 4. 构建与运行 app

| Target | 产物 | 要求 |
| --- | --- | --- |
| `make build-windows` | 一个 `Deskhub.exe` | Windows 与 MSVC |
| `make build-macos` | macOS app | macOS 与 Xcode |
| `make build-linux` | 一个 `deskhub` binary | Ubuntu 与相应 `-dev` package |
| `make build-ios` | 面向 Simulator 的 iOS app | macOS、Xcode 与一个 Simulator runtime |
| `make build-android` | 一个 debug APK | Android SDK、NDK、`adb` |

每个 target 都有配套的 `release-<os>`（优化版本）与 `run-<os>`（构建后启动）。桌面 app
不解析任何 command line 参数，所有选择均在四个页面中完成。`run-android` 通过 adb 在已
连接的设备或 emulator 上安装并打开；`run-ios` 在 Simulator 上执行相同操作。

### Command line client

`client/cli/` 构建出一个 `deskhub-cli` binary，实现相同功能但不需要 GUI toolkit，通过
flag 而非页面进行控制。需要经由 SSH、从脚本中，或在 systemd 下运行 Deskhub 时使用它。

```bash
make build-cli                       # 当前 OS 的 debug build
make release-cli                     # 优化版本
make run-cli ARGS="scan"             # 构建后以给定参数运行
```

该 target 位于 `-DDESKHUB_CLI=ON` 之后（默认关闭），因此 app 以及 sanitizer、coverage、
fuzz 等 preset 不受影响。启用后，各 OS 的 media 库由可选变为必需，因为无法 capture 也
无法 decode 的 client 不成其为 client。

| 命令 | 功能 |
| --- | --- |
| `share` | 共享本机 —— 任意 display、shell，或两者 |
| `connect ADDRESS` | 打开窗口显示 host 的屏幕并进行操作 |
| `shell ADDRESS` | 在当前 terminal 中打开 host 上的一个 shell |
| `displays`、`scan`、`sources`、`probe` | 可共享的内容，以及网络中存在的机器 |
| `devices`、`trust`、`settings` | 与桌面 app 读写同一批文件 |

`deskhub-cli help COMMAND` 会打印可用 flag。所有命令均支持 `--json`，exit code 表明失败
原因：`2` flag 有误、`3` 无响应、`4` 被拒绝、`5` host key 已变更、`9` 当前 build 不支持。

各 OS 的当前状态：Linux 支持全部功能。Windows 支持 share 与 connect，复用桌面 app 已有
的窗口代码。macOS 支持 share 与打开 shell，但 `connect` 需要尚未实现的 window layer，程
序会据实报告。

若只修改 `core/` 与 `platform/`，使用共享的 CMake tree 更快：

```bash
make debug        # configure 并 build debug preset
make release      # ……release preset
```

**quiche 与 opus 按 ABI 分别构建。** QUIC transport 是在 `third_party/quiche` 中构建的
Rust 静态库，缺少它则无法 share 也无法 connect。Opus audio codec 是在
`third_party/opus` 中构建的 C 静态库，缺少它则共享内容没有声音。`debug`、`release` 以及
所有 `build-*` target 都会先构建各自所需的 ABI，已构建过则不再重复。`make quiche`、
`quiche-android`、`quiche-ios`、`quiche-macos` 以及对应的 `opus`、`opus-android`、
`opus-ios`、`opus-macos` 可单独执行这些步骤。CMake 因缺少 quiche 而中止是有意为之：它
拒绝产出一个根本无法 connect 的 binary。

## 5. 测试

| 命令 | 运行环境 | 覆盖内容 |
| --- | --- | --- |
| `make test` | 离线，不使用 socket | 整个 `core/`: wire format、framing、FEC、session、VT emulator、settings、文案 |
| `make test-platform` | loopback socket | 真实的 QUIC handshake、端到端的 SPAKE2、经由网络的 terminal host 与 viewer、面向真实 shell 的 PTY、lockout、approval |
| `make test-integration` | loopback，capture/encode 为模拟实现 | 完整的 host↔client session: negotiation、经网络传输的视频、input、passcode 与 approval 的准入控制、对无效数据的容错 |
| `make test-all` | 三个 suite 全部运行，core 在先 | |
| `make test-ctest` | 相同的 test，经由 CTest 运行 | 与 CI 的调用方式完全一致 |
| `make test-asan` | 三个 suite 在 ASan 与 UBSan 下运行 | 仅支持 clang/gcc，不支持 MSVC |
| `make test-tsan` | 三个 suite 在 ThreadSanitizer 下运行 | 仅支持 clang/gcc，不支持 MSVC |
| `make test-perf` | release build，离线与 loopback | 对 hot path 进行实测: `core_perf` 覆盖 packetize/reassemble/FEC、1080p 降采样、CRC 与文件批处理、VT parser 与 screen、wire 的 encode/decode、audio jitter buffer；`platform_perf` 覆盖 loopback 上的真实 QUIC |

这些 test suite 中没有任何一项需要远端 peer、GPU 或 network。

**Coverage。** `make coverage` 使用 clang 与 llvm-cov 生成 `core/` 的报告。
`scripts/check-coverage.sh` 执行与 CI 相同的门槛：**line ≥ 90 %，branch ≥ 80 %**。

**Fuzzing。** `make fuzz` 运行各 libFuzzer target，覆盖 wire、H.264、reassembly、
terminal byte stream 与 UI 文案的 parser，以及 host 与 viewer 两侧的 session state
machine（clang，Linux/macOS；每个 target 通过 `FUZZ_SECONDS=N` 控制时长）。每个 target
先重放 `core/fuzz/regressions/<target>`，确保已修复的 crash 不再出现，然后从已提交的
seed 与 dictionary 开始 fuzz。`make fuzz-coverage` 显示 corpus 实际覆盖到 core 的哪些
行。发现的每个 crash 都会成为一份 regression 输入。

**性能。** `make test-perf` 使用 release preset 构建两个 perf binary 并运行。
`core_perf` 在纯 C++ 的 hot path 上测量 37 个 workload，`platform_perf` 随后在 loopback
的真实 QUIC 上再测量 6 个。总计数秒。有三项指标会导致失败，其中没有一项是随意设定的毫
秒阈值：

- **每单位的 allocation 次数**，通过替换全局 `operator new` 精确计数。若某条 path 开始
  按 packet 或按 frame 进行 allocation，则在任何机器、任何一次运行中都会失败。
- **开销随输入的增长方式**：每一行 `-scaling` 以 4 倍输入执行相同工作，当耗时的增长远
  快于输入时即判定失败 —— 这正是无意引入 O(n²) 时的特征。
- **相对已记录 baseline 的偏移**：`make perf-baseline` 在空闲机器上生成
  `out/perf/baseline.txt`，此后每次运行按行报告变化，超过 25 % 即失败。该文件描述的是
  特定的一台机器，因此不纳入 git。

`DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、`DESKHUB_PERF_BASELINE` 与
`DESKHUB_PERF_WRITE` 用于调整计时部分。`make test` 与 CI 均不运行这一部分：debug、ASan
与 coverage 的 build 无法反映 production 的速度。

## 6. 风格与静态分析

| 命令 | 检查内容 |
| --- | --- |
| `make format` | 为 C++、Kotlin 和 Swift 应用 format |
| `make lint` | 相同检查但不写回文件，随后运行 `lint-dead` —— CI 强制执行的即为此项 |
| `make lint-dead` | 无用代码：没有任何地方使用的 C++ 函数、FFI 函数、字符串 id 与 Kotlin 代码 |
| `make lint-dead-swift` | 两个 Apple app 中的无用 Swift 代码，通过 Periphery 检查（macOS + Xcode） |
| `make lint-tidy` | 对 `core/src` 与 `platform/src` 运行 clang-tidy |

另有按语言划分的变体：`format-cpp`、`lint-cpp`、`format-kotlin`、`lint-kotlin`、
`format-swift`、`lint-swift`。

无用代码即错误，与 Rust 的 `dead_code` lint 的做法一致。`make lint-dead` 对 production
构建的每个 C++ 文件运行 cppcheck，只要有函数在其中无人调用即失败 —— test、fuzzer 与
benchmark 不计入，因此只被 test 调用的函数同样算作无用代码。来自 Swift、Kotlin 与
Objective-C++ 的调用计为使用。同一脚本还会在以下情况失败：没有 app 调用的 FFI 函数、两个
app 都不显示的 `DHStr*` 字符串 id、无人读取的 Kotlin 常量；detekt 则在出现未使用的 private
Kotlin 代码、import 与参数时失败。当某个 test 确实无法以其他方式观察某一行为时，保留该
accessor，并在 `scripts/dead-code-allow.txt` 中加入 `名称: 哪个 test 需要它以及它证明了
什么`；缺少理由的行，或其函数已被 production 调用的行，同样会使检查失败。
`make lint-dead-swift` 先构建两个 Apple app 以生成 index，再对结果运行 Periphery。此外，
clang 构建会对未使用的 member function、template 与 exception 参数发出警告，CI 的
`-Werror` 会将其变为错误。

项目约定的简要版本，完整内容见 `CLAUDE.md`：

- C++20，不使用编译器扩展。core 使用 `deskhub`，platform 使用 `deskhubp`。
- 函数与类型采用 `PascalCase`，局部变量采用 `camelCase`，private 成员以下划线结尾。
- **任何位置都不写注释。** 改用具描述性的命名、小函数、early return 与具名常量。必须
  保留的信息应写入「缺少它即会失败」的那条 path 的错误信息，或写入 `ARCHITECTURE.md`。
- 所有 identifier 与 log 信息使用英文。所有散文类文档以四种语言发布 —— 英语、越南语、
  中文、日语 —— 以英文版为准。

## 7. 打包

| 命令 | 产物 |
| --- | --- |
| `make dist-macos` | 使用 Developer ID sign、经 notarize 并 staple 的 dmg |
| `make verify-macos` | 对刚构建的产物执行 Gatekeeper 检查 |
| `make dist-linux` | `.deb` 与 `.rpm`，两者均安装 uinput 的 udev rule |

Windows 与 Linux 的 app 各为单个文件，没有需要构建的 installer。

## 8. 发布

1. 提升 [`VERSION`](../VERSION)。当 tag 与该文件不一致时，`scripts/check-version.sh` 会
   使 deploy 失败。
2. 在同一个 commit 中，更新本次改动涉及的文档，包含所有语言版本。
3. 打 `vX.Y.Z` tag 并 push。`.github/workflows/deploy.yml` 会构建所有平台、创建 GitHub
   Release、将 iOS 发送至 TestFlight、将 macOS 送经 notarization，并将 Android 推送至
   Play 的 internal track。

**Release notes 由上一个 tag 到当前 tag 之间的 commit subject 生成**，由
`scripts/changelog.sh` 负责。可在本地查看某个 tag 将生成的内容：

```bash
scripts/changelog.sh v5.0.0     # 不带参数时使用 HEAD 上的 tag
```

因此 commit subject 是面向用户的内容，其前缀的 conventional-commit type 决定它归入哪一
节。完整的对应关系、覆盖这些规则的例外情形以及实例，见
[`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md)，撰写 subject 前
应先阅读。空的小节不会出现在 release 中，`INCLUDE_INTERNAL=1 scripts/changelog.sh` 可
查看被略去的 commit。

### Package manager

GitHub Release 创建后，`deploy.yml` 将 tag 交给 `publish-packages.yml`，由其中的 job 负责发布。
若要在不创建新 release 的情况下重新发布某个 tag（例如修复了其中某个 script 之后），可手动
运行：`gh workflow run publish-packages.yml -f tag=vX.Y.Z`。

| Job | 发布内容 | `stg` environment 中的 secret |
| --- | --- | --- |
| `winget` | 通过 [komac](https://github.com/russellbanks/Komac)（版本 pin 在 `scripts/pinned-versions.txt`）向 `microsoft/winget-pkgs` 提交 `ManhPham.Deskhub` 与 `ManhPham.DeskhubCLI` 的 pull request | `WINGET_TOKEN` —— 带 `public_repo` 的 classic PAT |
| `homebrew` | `manhpham90vn/homebrew-tap` 中的 `Casks/deskhub.rb` 与 `Formula/deskhub-cli.rb`，由 `packaging/homebrew/` 生成 | `HOMEBREW_TAP_TOKEN` —— 对该 tap 有 Contents: write 的 fine-grained PAT |
| `build-apt-repo` → `deploy-apt-repo` | 已签名的 apt repository，包含最近三个 release 的 deb，位于 GitHub Pages 的 `/apt`（`scripts/build-apt-repo.sh`） | `APT_GPG_PRIVATE_KEY`、`APT_GPG_PASSPHRASE` |

每个 job 在首次运行它的 tag 之前都需要一次性设置：

- **winget** —— 该 job 只更新已存在的 package，因此每个 package 的第一个版本需手动提交，
  komac 询问时将 portable command alias 设为 `deskhub` / `deskhub-cli`。在 Microsoft merge
  该 pull request 之前，job 只会警告并跳过。

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows.exe
  ```

  再对 `ManhPham.DeskhubCLI` 和 `deskhub-cli-vX.Y.Z-windows.exe` 的 URL 执行一次。
- **Homebrew** —— 创建 public repository `manhpham90vn/homebrew-tap`，由 job 填充内容。
- **apt** —— 生成一次 signing key，将该文件存为 `APT_GPG_PRIVATE_KEY`，并保留离线备份。
  所有用户都信任这把 key：更换它会让他们的 `apt update` 失败。

  ```bash
  gpg --quick-gen-key "Deskhub APT <manhpv151090@gmail.com>" rsa4096 sign 5y
  gpg --armor --export-secret-keys "Deskhub APT" > deskhub-apt.key
  ```

  在 *Settings → Pages* 中将 source 设为 **GitHub Actions**，并在 *Settings →
  Environments → github-pages* 中允许匹配 `v*` 的 tag —— 否则 Pages 会拒绝来自 tag 的
  deploy。

## 9. CI 的检查项

本地 `make test` 与 `make lint` 通过并不代表全部。每个 pull request 上均会执行：

- 对 `core/src` 与 `platform/src` 的 clang-tidy，SwiftLint `--strict`，Android Lint
- 对 workflow 与 `scripts/*.sh` 的 actionlint 与 shellcheck
- 无用代码：`scripts/dead-code.sh`（cppcheck、FFI / 字符串 id / Kotlin 常量检查与
  detekt），以及对两个 Apple app 的 Periphery
- 三个 suite 在 ASan/UBSan 与 TSan 下运行，并为 arm64 Linux、Android emulator 与 iOS
  Simulator 执行 cross-build
- 在 Windows 上将整个 integration suite 额外运行三次，用于定位一处间歇性的 memory
  corruption。该问题约每三次运行出现一次，单次运行容易漏过。发生 crash 的 frame 是该
  corruption 的受害者而非起因，因此每个 Windows job 都会在 symbol 旁写入完整的
  minidump；nightly 另将 load test 重复两轮：一轮启用 full page heap，使越过 allocation
  的写操作在触发它的 instruction 处立即 fault；另一轮针对启用 Rust debug assertion 与
  overflow check 构建的 quiche，这是唯一能够观察 quiche 内部的手段，因为 ASan 不
  instrument Rust，而 page heap 只保护 heap
- core 的 coverage 达到 line ≥ 90 % 与 branch ≥ 80 %
- 每个 libFuzzer target 运行 30 秒（nightly 为每个 15 分钟）
- 对 C++/Kotlin/Swift 的 CodeQL，对完整历史的 gitleaks 扫描，以及一次 dependency review

## 10. 开发者工具

| 命令 | 功能 |
| --- | --- |
| `make icons` | 从 `assets/icon_1024.png` 重新生成所有 client 的图标 |
| `make quic-smoke` | 基于 quiche 静态库的独立 QUIC client 与 server |
| `make opus-smoke` | 基于 opus 静态库的独立 encode/decode 往返测试 —— 报告实际 bitrate、最大 packet，以及 DTX 是否生效 |
| `make screenshots` | macOS: 在 iPhone/iPad simulator、Android emulator 与 macOS app 上重新采集商店截图，随后更新 `docs/imgs`（`ARGS="ios android macos readme"` 可指定子集） |
| `make setup-linux-permissions` | `/dev/uinput` 的 udev rule 与 `input` group，使从 source 构建的版本也能作为 host |
| `make reset-macos-permissions` | 当本地 build 与下载版本共用同一 bundle id 时清除 TCC 授权（`ARGS="--purge"` 同时删除已构建的副本） |
| `make ffmpeg-min` | Ubuntu: app 所 link 的静态最小化 FFmpeg（由 `build-linux` 自动执行） |
| `make opus` | host target 所用的 Opus audio codec（由 `debug`、`release` 与 `build-linux` 自动执行） |
| `make clean` | 删除 `out/` |

## 11. 构建问题排查

- **CMake 因缺少 quiche 库而中止** —— 运行对应的 `make quiche*` target；每个 ABI 需要
  各自的版本。opus 与 `make opus*` 同理。
- **macOS 上 `make fuzz` 找不到 libFuzzer** —— 它需要 Homebrew 的 LLVM。
  `make bootstrap` 会安装，其余部分仍使用 Xcode 的 toolchain 构建。
- **`make lint` 的结果与编辑器不一致** —— 以固定版本的工具为准。重新运行
  `make bootstrap` 获取确切版本，然后执行 `make format`。
- **Android target 找不到 SDK** —— 设置 `ANDROID_HOME`，然后重新运行 `make bootstrap`。
  `ANDROID_NDK_VERSION=<v>` 可选择其他 NDK。
- **在本地 build 与下载版本之间切换后，macOS 的 permission 行为异常** —— 执行
  `make reset-macos-permissions`。

问题与提问：[issues](https://github.com/manhpham90vn/Deskhub/issues)。
