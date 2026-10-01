[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · **中文** · [日本語](BUILD.ja.md)

# Deskhub —— 构建与开发

本指南介绍如何 build Deskhub、运行 test 并准备 release。如果只想安装和使用 app，
请先看 [`INSTALL.zh.md`](INSTALL.zh.md)。

本文件是 [`BUILD.md`](BUILD.md) 的译本；若两者有出入，以英文版为准。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 执行一次: 当前 OS 的 toolchain 与依赖
make test             # build 并离线运行 core suite
make build-linux      # 或 build-windows / build-macos / build-ios / build-android
```

请明确指定要 build 的平台 target。不带参数的 `make` 只会显示 `make/help.txt` 中的
target 列表，不会构建 app。各 target 的说明见 [`Makefile`](../Makefile)。

---

## 1. 前置准备

`make bootstrap` 会安装能够自动安装的部分，并提示其余部分。运行之前需先自行准备以下
内容：

| 本机 OS | 需先安装 | bootstrap 随后安装 |
| --- | --- | --- |
| **Ubuntu / Debian** | 除 apt 外无需其他；[Rust](https://rustup.rs) | build-essential、clang、llvm、CMake（apt 版本低于 3.25 时执行 `pipx install "cmake>=3.25,<4"`）、ninja、JDK 17、pipx、python3-venv、rpm、nasm，GTK3 / PipeWire / VA-API / tray 的 `-dev` package，VA-API driver，GNOME portal，静态的最小化 FFmpeg，cargo-ndk，quiche，opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode 与 command line tools、[Rust](https://rustup.rs) | cmake、ninja、pipx、Homebrew 的 LLVM（Apple clang 不含 libFuzzer runtime）、Temurin JDK 17、cargo-ndk，以及面向 Apple 的 quiche 和 opus |
| **Windows** | winget（App Installer）、[Git for Windows](https://git-scm.com/download/win)（build script 在其 Git Bash 下运行）、Python 3 | 通过 winget 安装含 C++ workload 与 *C++ Clang tools* 的 Visual Studio Build Tools、Rust、NASM、GNU make、Temurin JDK 17 和 Android Studio，由 `scripts/bootstrap.ps1` 执行 |

共享的 CMake tree 需要 CMake 3.25 或更新版本。Windows 上首次运行时还没有 `make`：请直接执行
`powershell -ExecutionPolicy Bypass -File scripts\bootstrap.ps1`，然后打开一个新的 terminal。

在所有 OS 上，bootstrap 还会 pin 住 CI 使用的 style 与分析工具 —— clang-format、clang-tidy、
ktlint、SwiftFormat、cppcheck 和 detekt，macOS 上另加 SwiftLint —— 各自固定版本。下载获取的
工具（ktlint、detekt、SwiftFormat、SwiftLint 以及 cppcheck 的源码）会校验 checksum；
SwiftLint（0.65.0）安装到 `tools/swiftlint` 而不是通过 Homebrew，在 macOS 上缺少它时
`make lint` 会自行把它安装到那里；
clang-format 与 clang-tidy 按 pin 住的版本从 PyPI 安装到 `tools/venv`；Windows 上的 cppcheck
则按 pin 住的版本通过 winget 安装。请勿手动安装这些工具 —— CI 比对的正是这些确切版本。
Swift 无用代码工具 Periphery 会在首次运行 `make lint-dead-swift` 时获取，并校验 checksum。

移动端 target 另有要求：`build-android` 需要含 NDK 的 Android SDK（当 `ANDROID_HOME`
指向一份 cmdline-tools 安装时，bootstrap 会安装相应 SDK package —— platform-tools、
platform、NDK 与 CMake —— 随后为 Android 构建 quiche 和 opus），`build-ios` 需要含
Simulator runtime 的 Xcode。NDK 版本依次取自 Gradle 命令行上的
`-PandroidNdkVersion=<v>`、环境变量 `ANDROID_NDK_VERSION`，否则为 `26.1.10909125`；
bootstrap 安装的也是同一版本。

git submodule 只有 `nvenc` 头文件一项。clone 时加 `--recurse-submodules`，或事后执行
`git submodule update --init` 即可。`make bootstrap` 也会同步该 submodule。

## 2. 目录结构

```
core/       与平台无关的 C++20 —— protocol、packetization、FEC、session state、
            input mapping、bitrate control、VT emulator。不含 OS 头文件。有 unit test。
platform/   面向 OS 的薄 abstraction，对外只有一套 API —— socket、clock、logging、
            random、source 枚举，以及多个 app 共用的 media、audio 与 input 代码。依赖 core。
client/     五个 app: android、ios、linux、macos、windows。
            client/apple/ 是 macOS 与 iOS app 共用的 Swift，本身不是 app。
            client/cli/ 是 command line client，一个 binary 覆盖三个桌面平台。
tests/integration/  基于 loopback 的 host + viewer，capture 与 encode 为模拟实现
cmake/      共享的 CMake module: warnings、quiche、opus
third_party/  quiche (QUIC)、opus (audio)、nvenc 头文件、最小化的 FFmpeg build
patches/    构建 quiche 之前应用的 patch
licenses/   随安装包发布的第三方 licence 文本
packaging/  Windows 安装程序、Homebrew 模板、release notes、apt 与 Pages 站点
assets/     生成所有 client 图标所用的源图标
make/       每个平台一个 .mk，由根 Makefile include
scripts/    bootstrap、打包、coverage、style 以及 CI 用的辅助脚本
tools/      bootstrap 安装的、已 pin 版本的 style 工具（被 git 忽略）
.github/    workflow，以及供其共用的 composite step（actions/）
```

逻辑只编写一次并共享使用。向 `client/*` 添加代码之前，需先判断它是否属于 `core/`（与平
台无关）或 `platform/`（需要 OS，但各平台 API 相同）。
[`ARCHITECTURE.zh.md`](ARCHITECTURE.zh.md) 说明分层、threading model 与 wire
protocol；`CLAUDE.md` 列出本 repo 强制执行的规则。

## 3. 日常流程

```bash
make test      # core suite，离线，无需 GPU 和 network —— 数秒完成
make lint      # 检查 C++、Kotlin 和 Swift 的 format，随后执行无用代码检查
```

完成改动前，请运行这两个命令。需要实际应用 format 时，使用 `make format`；
`make lint` 只负责检查，从不写回文件。repo 固定了与 CI 一致的 formatter 版本。

`core/` 中新增的逻辑，需在 `core/tests/` 对应的子目录中配套 test。

## 4. 构建与运行 app

| Target | 产物 | 要求 |
| --- | --- | --- |
| `make build-windows` | 一个 `Deskhub.exe` | Windows 与 MSVC |
| `make build-macos` | macOS app | macOS 与 Xcode |
| `make build-linux` | 一个 `deskhub` binary | Ubuntu 与相应 `-dev` package |
| `make build-ios` | 面向 Simulator 的 iOS app | macOS、Xcode 与一个 Simulator runtime |
| `make build-android` | 一个 debug APK | Android SDK、NDK、`adb` |

每个 target 都有配套的 `release-<os>`（优化版本）与 `run-<os>`（构建后启动）；
`release-ios` 仍面向 Simulator，`release-android` 生成未签名的 release APK。桌面 app
不解析任何 command line 参数，所有选择均在四个页面中完成。`run-android` 通过 adb 在已
连接的设备或 emulator 上安装并打开；`run-ios` 在已启动的 Simulator 上执行相同操作，若没有
则使用第一个可用的 iPhone simulator；`IOS_DEVICE=<udid>` 可指定其中一个。

当 keychain 中没有 Apple Development identity 时，macOS target 使用 ad hoc 签名。
`MACOS_SIGN=adhoc` 或 `MACOS_SIGN=developerid` 可强制指定方式，`MACOS_TEAM` 设置 Developer ID
签名所用的 team，`MACOS_XCARGS` 向 `xcodebuild` 传递额外的 build setting。

Debug 版本不会干扰已安装的正式版。在所有桌面系统上，app 与 CLI 的 Debug 版本把 key、允许的
client、受信任的 host、设置与日志保存在 `~/.deskhub-dev`（Windows：`%USERPROFILE%\.deskhub-dev`）
而不是 `~/.deskhub`；在 macOS 上，`build-macos`/`run-macos` 生成 bundle id 为
`com.deskhub.macos.debug` 的 **Deskhub Dev**。因此运行本地构建永远不会退出已安装的 app，
不会触碰它的数据，并会单独申请 macOS 隐私权限。`release-<os>` 构建使用常规的目录与 bundle id。

### Command line client

`client/cli/` 会构建 `deskhub-cli`，用命令代替 app 页面。它适合通过 SSH、脚本或
systemd 运行。在 Windows 和 Linux 上，`connect` 会打开 viewer 窗口；macOS 版本暂不
支持该命令。

```bash
make build-cli                       # 当前 OS 的 debug build
make release-cli                     # 优化版本
make run-cli ARGS="host list"        # 构建后以给定参数运行
make cli-smoke                       # 构建后让它在 loopback 上与自身对跑
```

该 target 位于 `-DDESKHUB_CLI=ON` 之后（默认关闭），因此 app 以及 sanitizer、coverage、
fuzz 等 preset 不受影响。启用后，各 OS 的 media 库由可选变为必需，因为无法 capture 也
无法 decode 的 client 不成其为 client。在 Linux 上这包括静态的最小化 FFmpeg，因此 CLI 的
target 会像 `build-linux` 一样先构建 `ffmpeg-min`。

`make cli-smoke` 以 headless 方式在 loopback 上让构建出的 CLI 与自身对跑：交换 key、陌生
client 等待批准、QR invite、remote shell，以及向 host 发送文件。在 Windows 上会跳过需要
POSIX signal 的步骤。CI 在三个桌面平台上都会运行它。

| 命令 | 功能 |
| --- | --- |
| `share` | 共享本机 —— 任意 display、shell，或两者 |
| `connect ADDRESS` | 打开窗口查看并操作 host 的屏幕（Windows 和 Linux） |
| `shell ADDRESS` | 在当前 terminal 中打开 host 上的一个 shell |
| `send ADDRESS FILE...` | 向允许接收文件的 host 发送文件 |
| `displays`、`sources ADDRESS` | 本地显示器，以及已认证 host 共享的内容 |
| `key public`、`access`、`host`、`host-key public` | 本机的公钥、允许的 client 与 connection request、已保存 host 和本机 host 密钥 |
| `devices`、`trust`、`settings` | 使用相同配置文件的旧命令 |
| `help [COMMAND]`、`version` | 用法说明，以及此 build 报告的版本 |

每台机器只有一把密钥。用 `key public` 显示其公钥；在 host 上将该行输入 `access add --stdin`
即可手动允许它。尚未获允许就连接的 client 会在 host 上留下一条 **connection request**：
`access requests` 列出它们，`access approve --fingerprint SHA256:...` 或
`access deny --fingerprint SHA256:...` 处理其中一条；`share --qr` 打印一个 QR code，其
`deskhub://pair/...` 链接可让一台设备自行接入，该链接也可在 `connect`、`sources`、`shell` 与
`send` 中代替地址使用。
把 host 的 `host-key public` 输出传给 client，再输入
`host add office --address 192.168.1.10:47777 --host-key-stdin` 即可预先固定一个 host。
随后可运行 `connect office`、`sources office`、`shell office` 或 `send office FILE`。
`host update office` 可明确更改地址或固定的密钥；`host remove office` 删除配置。
`access remove --fingerprint SHA256:...` 撤销 client 密钥。`sources`、`connect`、`shell` 与 `send`
接受地址、invite 链接或已保存的别名，使用 `--accept-new-host-key` 保存首次见到的 host 的
密钥，使用 `--approval-wait SEC` 更改等待 host 所有者批准的时长（默认 120）。不指定
`--accept-new-host-key` 时，未知 host 会被拒绝并打印其 fingerprint。信任跟随 host 的密钥，
因此更换地址的 host 依然受信任。没有 network scan，也没有 passcode flag。
`--config-dir PATH` 为所有命令指定同一配置目录，可放在命令前或命令后。

`deskhub-cli help COMMAND` 会打印可用 flag。列出信息的命令支持 `--json`，exit code 表明失败
原因：`1` 其他失败、`2` flag 有误、`3` 无响应、`4` 被拒绝或未及时获批、`6` 没有可共享或
可查看的内容、`8` host 无法开始监听、`9` 当前 build 不支持、`130` 被 Ctrl-C 中断。

Linux 支持上表中的全部命令。Windows 的 `connect` 复用桌面 app 的窗口代码。
macOS 可 share 并打开 remote shell；`connect` 会提示此 build 无法查看屏幕。

若只修改 `core/` 与 `platform/`，使用共享的 CMake tree 更快：

```bash
make debug        # configure 并 build debug preset
make release      # ……release preset
```

**quiche 与 opus 按 ABI 分别构建。** QUIC transport 是在 `third_party/quiche` 中构建的
Rust 静态库，缺少它则无法 share 也无法 connect。Opus audio codec 是在
`third_party/opus` 中构建的 C 静态库，缺少它则共享内容没有声音。`debug`、`release`、
所有 `build-*`、`release-*` 与 CLI target，以及 `test`、所有 `test-*` target 和
`lint-tidy`，都会先构建各自所需的 ABI，已构建过则不再重复（`coverage`、`fuzz` 与
`fuzz-coverage` 两者都不构建：它们只覆盖 `core/`）—— 因此在任何 OS 上，全新
clone 后执行 `make bootstrap && make test` 都能成功。`make quiche`、`quiche-android`、
`quiche-ios`、`quiche-macos` 以及对应的 `opus`、`opus-android`、`opus-ios`、
`opus-macos` 可单独执行这些步骤；任一库构建失败，`make` 就会在此停止。CMake 因缺少
quiche 而中止是有意为之：它拒绝产出一个根本无法 connect 的 binary。

**Hardening。** `cmake/DeskhubHardening.cmake` 为 GCC 与 Clang 构建加上
`-fstack-protector-strong`，优化构建另加 `-D_FORTIFY_SOURCE=3` —— Android 除外，它保留
NDK 默认的 2；full RELRO（`-Wl,-z,relro,-z,now`）只在 Linux 上启用。MSVC 构建使用 `/sdl`。

## 5. 测试

| 命令 | 运行环境 | 覆盖内容 |
| --- | --- | --- |
| `make test` | 离线，不使用 socket | 整个 `core/`: wire format、framing、FEC、session、VT emulator、settings、文案 |
| `make test-platform` | loopback socket | 真实的 QUIC handshake、端到端的 key 签名 authenticate、host key 固定、经由网络的 terminal host 与 viewer、面向真实 shell 的 PTY、无效签名导致的 lockout |
| `make test-integration` | loopback，capture/encode 为模拟实现 | 完整的 host↔client session: negotiation、经网络传输的视频、input、基于 authorized key 的准入、对无效数据的容错 |
| `make test-all` | 三个 suite 全部运行，core 在先 | |
| `make test-ctest` | 相同的 test，经由 CTest 运行 | 与 CI 的调用方式完全一致 |
| `make test-asan` | 三个 suite 在 ASan 与 UBSan 下运行 | 仅支持 clang/gcc，不支持 MSVC |
| `make test-tsan` | 三个 suite 在 ThreadSanitizer 下运行 | 仅支持 clang/gcc，不支持 MSVC |
| `make test-perf` | release build，离线与 loopback | 对 hot path 进行实测: `core_perf` 覆盖 packetize/reassemble/FEC、1080p 降采样、CRC 与文件批处理、VT parser 与 screen、wire 的 encode/decode、audio jitter buffer 与 PCM ring、input path、record stream 的 framing；`platform_perf` 覆盖 loopback 上的真实 QUIC |
| `make perf-build` | release build，不运行 | 构建 `core_perf` 与 `platform_perf`，但不运行它们 |
| `make cli-smoke` | loopback，headless | command line client 与自身对跑（见上文） |

这些 test suite 中没有任何一项需要远端 peer、GPU 或 network。

**Coverage。** `make coverage` 使用 clang 与 llvm-cov 生成 `core/` 的报告。
`scripts/check-coverage.sh` 执行与 CI 相同的门槛：**line ≥ 90 %，branch ≥ 80 %**。

**Fuzzing。** `make fuzz` 运行各 libFuzzer target，覆盖 wire、H.264、reassembly、
terminal byte stream 与 UI 文案的 parser，key、access list 与 invite 的文本格式，QR
encoder，以及 host 与 viewer 两侧的 session state machine（clang，Linux/macOS；每个 target 通过 `FUZZ_SECONDS=N` 控制时长）。每个 target
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
  `out/perf/baseline.txt` 与 `out/perf/platform-baseline.txt`，此后每次运行按行报告变化，
  超过 25 % 即失败。这些文件描述的是特定的一台机器，因此不纳入 git。

`DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、`DESKHUB_PERF_BASELINE` 与
`DESKHUB_PERF_WRITE` 用于调整计时部分。`make test` 不运行其中任何一项：debug、ASan 与
coverage 的 build 无法反映 production 的速度。CI 在 Linux 与 macOS 的 release job 上运行
两个 binary，但只有 allocation 与 scaling 两项可能失败，因为计时 baseline 只描述一台机器。
在 pull request 上，CI 还会在同一 runner 上分别构建 base commit 与本次改动，并将两者之间的
偏移作为评论发布 —— 仅为警告，因为共享 runner 噪声太大，无法按时间判定失败。

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
accessor，并在 `scripts/dead-code-allow.txt` 中加入 `name: 哪个 test 需要它以及它证明了
什么`；缺少理由的行，或其函数已被 production 调用的行，同样会使检查失败。
`make lint-dead-swift` 先构建两个 Apple app 以生成 index，再对结果运行 Periphery。此外，
clang 构建会对未使用的 member function、template 与 exception 参数发出警告，`-Werror`
会将其变为错误：除 `asan-msvc` 外的所有 CMake preset 都设置了 `DESKHUB_WERROR=ON`，因此
本地的 `make test` 也会像 CI 一样因 warning 而失败。

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
| `make dist-linux` | 分别构建 app 和 CLI 的 `.deb` 与 `.rpm`，每个包都有 uinput 的 udev rule |

`dist-macos` 需要 keychain 中有 *Developer ID Application* identity，并需要用于 notarization
的 App Store Connect API key：`ASC_KEY_P8`（`.p8` 文件的路径）、`ASC_KEY_ID` 与
`ASC_ISSUER_ID`。三者缺少任何一个，它都会在构建之前停止。

Release workflow 还会通过 `packaging/windows/` 构建 Windows app 和 CLI 的 Inno Setup
安装程序。Windows 便携版和 Linux app 仍是单文件。

每个安装包都附带 `THIRD_PARTY_NOTICES.md` 以及 `licenses/` 中适用于它的 licence 文本：
app 与 CLI 的 `.deb` 和 `.rpm`（位于 `/usr/share/doc/<package>/`，文本 —— 包括 LGPL ——
位于其 `licenses/` 文件夹中）、两个 Windows 安装程序、macOS app 的 `Resources`、iOS app
bundle 以及 Android APK 的 assets。`scripts/stage-licenses.sh` 准备 Linux 与 Apple 的文件集，
`packaging/windows/*.iss` 列出 Windows 的文件，Android 构建自行复制其文件。便携版 binary
无法附带任何内容，因此 release 另外提供 `deskhub-<tag>-licenses.zip`，内含 `LICENSE`、
notices 以及整个 `licenses/` 文件夹。`licenses/rust-crates.txt` 是生成的：quiche 的 pin
变动时，`scripts/rust-crate-notices.py <quiche source dir> > licenses/rust-crates.txt`
会重写它。APK 还附带 `licenses/android-libraries.txt`，即 Gradle 打包进 APK 的 Java 与
Kotlin 库及其各自的 notice；它同样是生成的：`scripts/android-library-notices.py >
licenses/android-libraries.txt` 读取 `client/android` 的 release runtime classpath（需设置
`JAVA_HOME`，并先构建一次 Android 让 Gradle 缓存好 artifact），每当 Android 依赖变化时重写它。

## 8. 发布

1. 提升 [`VERSION`](../VERSION)。当 tag 与该文件不一致时，`scripts/check-version.sh` 会
   使 deploy 失败。
2. 在同一个 commit 中，更新本次改动涉及的文档，包含全部四种语言版本。
3. 编写 release notes（见下文）并提交。
4. 打 `vX.Y.Z` tag 并 push。`.github/workflows/deploy.yml` 会构建所有平台、创建 GitHub
   Release、将 iOS 发送至 TestFlight、将 macOS 送经 notarization，并将 Android 推送至
   Play 的 internal track。

**Release notes 为手写**，每个 tag 一个文件：`packaging/release-notes/vX.Y.Z.md`，文件名须与
tag 完全一致。该文件即 GitHub Release 的正文，其后附一段固定的 footer，指向安装指南与安全
模型。开头应先写用户更新后必须做的事 —— breaking change、如何重新连接 —— 然后才是新增与
修复的内容。若 tag 没有对应文件，`scripts/check-release-notes.sh` 会在构建任何东西之前使
deploy 失败，因此须在打 tag 前提交该文件。

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

- **winget** —— 该 job 只更新已存在的 package，因此每个 package 的第一个版本需手动提交。
  `ManhPham.Deskhub` 使用 Inno Setup 安装程序；`ManhPham.DeskhubCLI` 的 portable command alias
  设为 `deskhub-cli`。在 Microsoft merge
  该 pull request 之前，job 只会警告并跳过。

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows-setup.exe
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
- 三个 suite 在 Linux x64 与 arm64（均为原生构建）、macOS 和 Windows 上运行；在 Linux 上
  于 ASan/UBSan 与 TSan 下运行，在 macOS 上于 ASan/UBSan 下运行；在 Windows 上 platform 与
  integration suite 于 MSVC 的 ASan 下运行；并为 Android 执行 cross-build —— 在 x86_64
  emulator 上运行，arm64-v8a 只构建不运行 —— 以及为 iOS Simulator 执行 cross-build
- 在三个桌面平台上对 CLI 运行 `make cli-smoke` 的 script
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
| `make reset-macos-permissions` | 清除正式版（`com.deskhub.macos`）与本地 Debug 版（`com.deskhub.macos.debug`）两个 bundle id 的 TCC 授权，并列出每份 app 副本及其签名方式 —— 用于重新构建后 Screen Recording 或 Accessibility 失效的情况（`ARGS="--purge"` 同时删除已构建的副本） |
| `make ffmpeg-min` | Ubuntu: app 与 CLI 所 link 的静态最小化 FFmpeg（由 `build-linux`、`release-linux` 与 CLI target 自动执行） |
| `make opus` | host target 所用的 Opus audio codec（由 `debug`、`release`、Linux、CLI 与 test target 自动执行） |
| `make clean` | 删除 `out/` |

## 11. 构建问题排查

- **CMake 因缺少 quiche 库而中止** —— 运行对应的 `make quiche*` target；每个 ABI 需要
  各自的版本。opus 与 `make opus*` 同理。
- **macOS 上 `make fuzz` 找不到 libFuzzer** —— 它需要 Homebrew 的 LLVM。
  `make bootstrap` 会安装，其余部分仍使用 Xcode 的 toolchain 构建。
- **`make lint` 的结果与编辑器不一致** —— 运行 `make bootstrap`，获取 CI 使用的工具
  版本，然后执行 `make format`。
- **Android target 找不到 SDK** —— 设置 `ANDROID_HOME`，然后重新运行 `make bootstrap`。
  `ANDROID_NDK_VERSION=<v>` 可选择其他 NDK。
- **重新构建后，或在本地 build 与下载版本之间切换后，macOS 的 permission 行为异常** ——
  执行 `make reset-macos-permissions`，然后只对保留的那一份副本重新授权。

问题与提问：[issues](https://github.com/manhpham90vn/Deskhub/issues)。
