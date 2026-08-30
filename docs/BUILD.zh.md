[English](BUILD.md) · [Tiếng Việt](BUILD.vi.md) · **中文** · [日本語](BUILD.ja.md)

# Deskhub —— 构建与开发

自己编译 Deskhub、跑测试套件、发布一个版本所需要的一切。如果你只想*用*这个应用，请从
[`INSTALL.zh.md`](INSTALL.zh.md) 取预编译版本。

本文件是 [`BUILD.md`](BUILD.md) 的译本；若两者有出入，以英文版为准。

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # 一次：本系统的工具链 + 依赖
make test             # 离线构建并运行核心测试套件
make build-linux      # 或 build-windows / build-macos / build-ios / build-android
```

任何平台都不会被隐式构建：直接运行 `make` 只会打印目标列表，什么都不构建。每个目标都在
[`Makefile`](../Makefile) 顶部有完整说明，而 `make/help.txt` 就是裸 `make` 打印的内容。

---

## 1. 先要准备什么

`make bootstrap` 会装它能装的，并告诉你它装不了什么。跑它之前，请先自己装好这些：

| 主机系统 | 先自行安装 | bootstrap 随后会做的事 |
| --- | --- | --- |
| **Ubuntu / Debian** | apt 之外无需别的；[Rust](https://rustup.rs) | build-essential、clang、llvm、cmake、ninja、JDK 17，GTK3 / PipeWire / VA-API / 托盘的 `-dev` 包，VA-API 驱动，GNOME portal，静态最小化 FFmpeg，quiche，opus |
| **macOS** | [Homebrew](https://brew.sh)、Xcode + 命令行工具、[Rust](https://rustup.rs) | cmake、ninja、swiftlint、pipx、Homebrew LLVM（Apple clang 不带 libFuzzer 运行时）、Temurin JDK 17，以及给 Apple 和 Android 用的 quiche 与 opus |
| **Windows** | winget（App Installer）、带 C++ 工具链和 *C++ Clang tools* 组件的 Visual Studio、[Rust](https://rustup.rs) | 其余由 `scripts/bootstrap.ps1` 驱动 winget 安装 |

在每个系统上它还会锁定代码风格工具：clang-format、clang-tidy、ktlint 和 SwiftFormat，
每个都固定版本并校验校验和 —— 千万别手动装这些，CI 比对的正是这些版本。

移动端目标还需要更多：`build-android` 需要带 NDK 的 Android SDK（只要 `ANDROID_HOME`
指向一份 cmdline-tools 安装，bootstrap 就会装好 SDK 组件），`build-ios` 需要带模拟器
运行时的 Xcode。

只有 `nvenc` 头文件是 git 子模块；克隆时加 `--recurse-submodules`，或者执行
`git submodule update --init` 即可。`make bootstrap` 也会同步它们。

## 2. 目录是怎么分的

```
core/       与平台无关的 C++20 —— 协议、分包、FEC、会话状态、
            输入映射、码率控制、VT 模拟器。不含 OS 头文件。有单元测试。
platform/   薄薄一层 OS 抽象，对外只有一套 API —— socket、时钟、日志、
            随机数、来源枚举。依赖 core。
client/     五个应用：android、ios、linux、macos、windows。
            client/apple/ 是 macOS 和 iOS 应用共用的 Swift，本身不是应用。
            client/cli/ 是命令行客户端，一个二进制覆盖三个桌面系统。
third_party/  quiche（QUIC）、opus（音频）、nvenc 头文件、最小化 FFmpeg 构建
make/       每个平台一个 .mk，由根 Makefile 引入
scripts/    bootstrap、打包、覆盖率、风格与 CI 辅助脚本
.github/    工作流，以及它们共用的组合步骤 actions/
```

逻辑只写一次，然后共享：在 `client/*` 下加任何东西之前，先看它是不是该放进 `core/`
（与平台无关）或 `platform/`（需要 OS，但各处 API 相同）。
[`ARCHITECTURE.zh.md`](ARCHITECTURE.zh.md) 讲清了分层、线程模型和线上协议；`CLAUDE.md`
写明了本仓库强制执行的规则。

## 3. 日常循环

```bash
make test      # 核心套件，离线，不用 GPU 也不用网络 —— 几秒钟
make lint      # 对 C++、Kotlin 和 Swift 做格式检查，不写回文件
```

在认为一处改动完成之前，两个都要跑。`make format` 会真的应用格式化而不只是检查 ——
永远别手工排版，工具锁定版本是有原因的。

`core/` 里的新逻辑需要在对应的 `core/tests/` 子目录里配一个测试。

## 4. 构建并运行一个应用

| 目标 | 产出 | 需要 |
| --- | --- | --- |
| `make build-windows` | 一个 `Deskhub.exe` | Windows + MSVC |
| `make build-macos` | macOS 应用 | macOS + Xcode |
| `make build-linux` | 一个 `deskhub` 二进制 | Ubuntu + 那些 `-dev` 包 |
| `make build-ios` | 面向模拟器的 iOS 应用 | macOS + Xcode + 模拟器运行时 |
| `make build-android` | 一个 debug APK | Android SDK + NDK、`adb` |

每个都有配套的 `release-<os>`（优化版）和 `run-<os>`（先构建，再启动）。桌面应用完全不
解析命令行参数 —— 一切都在那四个页面上选。`run-android` 通过 adb 在连接的设备或模拟器上
安装并打开，`run-ios` 在模拟器上做同样的事。

### 命令行客户端

`client/cli/` 构建出一个 `deskhub-cli` 二进制，不需要图形工具包就能做同样的事，用参数
代替页面来驱动。要通过 SSH、在脚本里或者在 systemd 下跑 Deskhub，靠的就是它。

```bash
make build-cli                       # 本系统的 debug 构建
make release-cli                     # 优化版
make run-cli ARGS="scan"             # 构建后带这些参数运行
```

它由 `-DDESKHUB_CLI=ON` 控制（默认关闭），所以应用本身以及 sanitizer、覆盖率和 fuzz 预设
都不受它影响。打开它之后，各系统的媒体库就从可选变成必需 —— 一个既不能采集也不能解码的
客户端算不上客户端。

| 命令 | 作用 |
| --- | --- |
| `share` | 共享这台机器 —— 任意显示器、shell，或两者 |
| `connect ADDRESS` | 打开一个窗口显示主机画面并操控它 |
| `shell ADDRESS` | 在你当前所在的终端里，打开主机上的一个 shell |
| `displays`、`scan`、`sources`、`probe` | 能共享什么，以及外面有谁 |
| `devices`、`trust`、`settings` | 与桌面应用读写的是同一批文件 |

`deskhub-cli help COMMAND` 会打印参数。每条命令都支持 `--json`，退出码说明哪里出了错：
`2` 参数不对，`3` 没人应答，`4` 被拒绝，`5` 主机密钥变了，`9` 这个构建做不了这件事。

各系统当前状态：Linux 全部功能都有。Windows 共享和连接走的是桌面应用已有的同一套窗口
代码。macOS 能共享、能开 shell，但 `connect` 需要一个尚未编写的窗口层，它会如实告知。

如果只想改 `core/` 和 `platform/`，用共享的 CMake 树更快：

```bash
make debug        # 配置 + 构建 debug 预设
make release      # ……release 预设
```

**quiche 和 opus 按 ABI 分别构建。** QUIC 传输是构建在 `third_party/quiche` 里的一个 Rust
静态库，没有它就既不能共享也不能连接。Opus 音频编解码器是构建在 `third_party/opus` 里的
一个 C 静态库，没有它共享出去就没有声音。`debug`、`release` 和每个 `build-*` 目标都会先
构建自己需要的那个 ABI，构建过一次之后就是空操作 —— `make quiche`、`quiche-android`、
`quiche-ios`、`quiche-macos` 以及对应的 `opus`、`opus-android`、`opus-ios`、`opus-macos`
也能单独跑这些步骤。如果 CMake 因为缺少 quiche 而停下，那是故意的：它拒绝产出一个永远
连不上的二进制。

## 5. 测试

| 命令 | 运行环境 | 覆盖内容 |
| --- | --- | --- |
| `make test` | 离线，不用 socket | 整个 `core/`：线格式、分帧、FEC、会话、VT 模拟器、设置、字符串 |
| `make test-platform` | 回环 socket | 真实的 QUIC 握手、端到端 SPAKE2、终端主机 + 观看端走真实链路、对着真 shell 的 PTY、锁定、审批 |
| `make test-integration` | 回环，假的采集/编码 | 完整的主机↔客户端会话：协商、视频过网、输入、通行码与审批门禁、抗垃圾数据 |
| `make test-all` | 三个都跑，core 优先 | |
| `make test-ctest` | 通过 CTest 跑同样的测试 | 与 CI 调用方式完全一致 |
| `make test-asan` | 三个都在 ASan + UBSan 下跑 | 仅限 clang/gcc，MSVC 不行 |
| `make test-tsan` | 三个都在 ThreadSanitizer 下跑 | 仅限 clang/gcc，MSVC 不行 |
| `make test-perf` | release 构建，离线 + 回环 | 真正测量而不只是跑一遍热路径：`core_perf` 覆盖分包/重组/FEC、1080p 缩放、CRC 与文件批处理、VT 解析器与屏幕、线上编解码、音频抖动缓冲；`platform_perf` 覆盖回环上的真实 QUIC |

测试套件里没有任何一项需要远端对等方、GPU 或网络。

**覆盖率。** `make coverage` 用 clang + llvm-cov 生成 `core/` 的报告；
`scripts/check-coverage.sh` 执行 CI 采用的那道门槛 —— **行 ≥ 90 %、分支 ≥ 80 %**。

**Fuzzing。** `make fuzz` 会针对线格式、H.264、重组、终端字节流和界面文本解析器，以及
主机与观看端的会话状态机运行 libFuzzer 目标（clang，Linux/macOS；每个目标可用
`FUZZ_SECONDS=N`）。每个目标先重放 `core/fuzz/regressions/<target>`，确保修好的崩溃不会
复活，然后从提交进仓库的种子和字典开始 fuzz。`make fuzz-coverage` 显示语料实际覆盖到
core 的哪些行。每找到一次崩溃，就变成一个回归输入。

**性能。** `make test-perf` 用 release 预设构建两个性能二进制并运行它们：`core_perf`
在纯 C++ 热路径上测 37 项负载，接着 `platform_perf` 在回环上的真实 QUIC 上再测 6 项。
总共几秒钟。有三件事会让其中任何一个失败，而且没有一件是凭空拍出来的毫秒数：

- **每单位的分配次数**，通过替换全局 `operator new` 精确计数。开始按包或按帧分配内存的
  路径，在任何机器上、任何一次运行里都会失败。
- **代价怎样随规模增长**：每个 `-scaling` 行会用 4 倍输入跑同样的活，当时间增长远快于
  输入时就失败 —— 那正是意外写出 O(n²) 时的形状。
- **相对基线的漂移**：`make perf-baseline` 会在空闲机器上写出
  `out/perf/baseline.txt`，之后每次运行都会报告每一行的变化，超过 25 % 就失败。这个文件
  描述的是那一台机器，所以不进 git。

`DESKHUB_PERF_TOLERANCE`、`DESKHUB_PERF_REPEATS`、`DESKHUB_PERF_BASELINE` 和
`DESKHUB_PERF_WRITE` 用来调节计时那一半。`make test` 和 CI 都不跑这些：debug、ASan 和
覆盖率构建说明不了生产环境的速度。

## 6. 代码风格与静态分析

| 命令 | 检查什么 |
| --- | --- |
| `make format` | 对 C++、Kotlin 和 Swift 应用格式化 |
| `make lint` | 同样的检查但不写回文件 —— CI 强制的就是这个 |
| `make lint-tidy` | 对 `core/src` + `platform/src` 跑 clang-tidy |

也有单语言变体：`format-cpp`、`lint-cpp`、`format-kotlin`、`lint-kotlin`、
`format-swift`、`lint-swift`。

内部规矩，简版 —— 完整版在 `CLAUDE.md`：

- C++20，不用编译器扩展。core 用 `deskhub`，platform 用 `deskhubp`。
- 函数和类型 `PascalCase`，局部变量 `camelCase`，私有成员末尾加下划线。
- **任何地方都不写注释。** 用有说明力的名字、小函数、提前返回和具名常量代替。必须留存的
  知识要写进那条缺了它就会失败的路径的错误消息里，或者写进 `ARCHITECTURE.md`。
- 所有标识符和日志消息用英文；每份散文文档都以四种语言发布 —— 英语、越南语、中文、
  日语 —— 以英文版为准。

## 7. 打包

| 命令 | 产出 |
| --- | --- |
| `make dist-macos` | 一个用 Developer ID 签名、经过公证并已装订的 dmg |
| `make verify-macos` | 对刚构建出的产物做一次 Gatekeeper 检查 |
| `make dist-linux` | `.deb` + `.rpm`，两者都会安装 uinput udev 规则 |

Windows 和 Linux 应用各自都是单个文件；没有安装程序要构建。

## 8. 发布

1. 提升 [`VERSION`](../VERSION) —— 如果标签和文件对不上，`scripts/check-version.sh` 会
   让部署失败。
2. 在同一个提交里，用所有语言更新这次改动涉及的文档。
3. 打上 `vX.Y.Z` 标签并推送。`.github/workflows/deploy.yml` 会构建每个平台、创建
   GitHub Release、把 iOS 发到 TestFlight、把 macOS 送去公证，把 Android 发到 Play 的
   internal 轨道。

**发布说明由 `scripts/changelog.sh` 从上一个标签到这个标签之间的提交标题生成。** 想看
某个标签会产出什么，在本地跑它：

```bash
scripts/changelog.sh v5.0.0     # 或者不带参数，用 HEAD 上的标签
```

也就是说，提交标题是给用户看的，而标题前面的 conventional-commit 类型决定它落在哪个
小节。完整的映射关系、覆盖它的规则和实例都在
[`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md) —— 写标题之前先读
一遍。空的小节不会出现在发布说明里，`INCLUDE_INTERNAL=1 scripts/changelog.sh` 可以看到
被略去的提交。

## 9. CI 把什么当门槛

本地 `make test` + `make lint` 全绿并不是全部。每个 pull request 上：

- 对 `core/src` + `platform/src` 跑 clang-tidy，SwiftLint `--strict`，Android Lint
- 对工作流和 `scripts/*.sh` 跑 actionlint + shellcheck
- 三个套件都在 ASan/UBSan 和 TSan 下跑，并交叉构建到 arm64 Linux、Android 模拟器和 iOS
  模拟器
- 整个集成套件在 Windows 上再多跑三遍，为的是抓 `DrainStreams` 里那个大约三次运行才出现
  一次、因而单跑一次会漏掉的间歇性栈破坏
- core 覆盖率行 ≥ 90 % / 分支 ≥ 80 %
- 每个 libFuzzer 目标跑 30 秒（每晚各跑 15 分钟）
- 对 C++/Kotlin/Swift 跑 CodeQL，对整个历史做一次 gitleaks 扫描，以及依赖审查

## 10. 开发者工具

| 命令 | 作用 |
| --- | --- |
| `make icons` | 从 `assets/icon_1024.png` 重新生成每个客户端的图标 |
| `make quic-smoke` | 一个独立的 QUIC 客户端 + 服务端，跑在 quiche 静态库上 |
| `make opus-smoke` | 一次独立的编解码往返，跑在 opus 静态库上 —— 报告真实码率、最大包和 DTX 是否生效 |
| `make screenshots` | macOS：在 iPhone/iPad 模拟器、Android 模拟器和 macOS 应用上重新拍商店截图，然后刷新 `docs/imgs`（`ARGS="ios android macos readme"` 可只做其中一部分） |
| `make setup-linux-permissions` | `/dev/uinput` udev 规则 + `input` 组，用于从源码构建做主机 |
| `make reset-macos-permissions` | 当本地构建和下载的构建为同一个 bundle id 打架时，清掉 TCC 授权（`ARGS="--purge"` 还会删掉已构建的副本） |
| `make ffmpeg-min` | Ubuntu：应用链接的那份静态最小化 FFmpeg（由 `build-linux` 自动运行） |
| `make opus` | 主机目标用的 Opus 音频编解码器（由 `debug`、`release` 和 `build-linux` 自动运行） |
| `make clean` | 删掉 `out/` |

## 11. 当构建跟你作对时

- **CMake 因为缺少 quiche 库而停下** —— 运行对应的 `make quiche*` 目标；每个 ABI 都要
  自己那一份。opus 和 `make opus*` 目标同理。
- **macOS 上 `make fuzz` 找不到 libFuzzer** —— 它需要 Homebrew LLVM；`make bootstrap`
  会装，其余部分仍然用 Xcode 工具链构建。
- **`make lint` 和你的编辑器意见不一致** —— 以锁定版本的工具为准。重新跑一次
  `make bootstrap` 拉到确切版本，然后 `make format`。
- **Android 目标找不到 SDK** —— 设置 `ANDROID_HOME`，然后重跑 `make bootstrap`；
  `ANDROID_NDK_VERSION=<v>` 可以选别的 NDK。
- **在本地构建和下载构建之间切换后 macOS 权限行为怪异** —— `make reset-macos-permissions`。

缺陷与提问：[issues](https://github.com/manhpham90vn/Deskhub/issues)。
