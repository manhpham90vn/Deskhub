[English](ARCHITECTURE.md) · [Tiếng Việt](ARCHITECTURE.vi.md) · **中文** · [日本語](ARCHITECTURE.ja.md)

# Deskhub —— Architecture

本文档描述 Deskhub **是如何构建的**：layer 的划分、process 与 thread、wire protocol，
以及其背后的设计决策。产品在用户视角下的行为见
[`SPECIFICATION.zh.md`](SPECIFICATION.zh.md)；threat model 见
[`SECURITY.zh.md`](../SECURITY.zh.md)。

本文件是 [`ARCHITECTURE.md`](ARCHITECTURE.md) 的译本；若两者有出入，以英文版为准。

- **状态：** 描述当前代码。
- **读者：** 需要修改此代码的人员。

---

## 1. Layer

整体结构遵循一条原则：逻辑只编写一次，由所有 client 共享。

```
core/       纯 C++20，不含 OS 头文件与第三方代码，离线进行 unit test
platform/   面向 OS 的薄 abstraction，每个头文件对外提供同一套 API（依赖 core）
client/     各 OS 的 app: windows、linux、macos、ios、android（依赖 platform 与 core）
            另有 client/cli，面向三个桌面平台的 command line client
```

| Layer | 内容 |
| --- | --- |
| `core/protocol` | Wire format（`Wire.h`）、stream 的 record framing（`RecordStream.h`）、区分 QUIC 与 Deskhub beacon datagram 的 packet classifier |
| `core/transport` | 面向 video 的 Packetizer/Reassembler、FEC、retransmit 缓存、send pacer |
| `core/session` | session state machine，按角色划分：`session/host`（按 viewer 的 session、viewer 表、beacon、file receiver、auth throttle）、`session/client`（screen client、file sender、terminal client、connect 流程），以及置于其旁的共享组件（transfer 类型、terminal session 表、clipboard sync、link recovery） |
| `core/control` | Bitrate controller、quality ladder、stream 尺寸计算、clock offset |
| `core/terminal` | 所有 client 共享的 VT emulator: `VtParser`、`Screen`、`KeyEncoder`、`Palette` |
| `core/net` | Trust store（client 侧）、paired devices（host 侧）、bind 地址选择、LAN scan 逻辑 |
| `core/ui` | 全部面向用户的字符串、settings 解析、表格行构造器，使五个 client 呈现一致的内容 |
| `platform/net` | `UdpSocket`（按 OS 实现）、`QuicEndpoint`（quiche 置于 pimpl 之后）、`SessionTransport` |
| `platform/auth` | `AuthNegotiation` —— 双方共用的唯一 pairing/passcode handshake |
| `platform/client` | `HostLink`（dial、trust、auth、channel，由所有界面共用）、`ScreenViewer`、`TerminalViewer`、`FileTransferClient`、`SourceQuery`、host probe、LAN scanner |
| `platform/host` | `HostEngine`、`HostNetLoop`、`SharingHost`、`TerminalHost`、`FileHost`、`ViewerBroadcast` |
| `platform/system` | Clock、random、PTY（ConPTY / forkpty）、host identity（key）、trust 与 paired-device 文件、autostart、keep-awake |
| `core/cli` | command line 语法及其 JSON writer：输入纯文本，输出经校验的 command |
| `client/<os>` | Capture、encode、decode、render、windowing、对话框；不包含任何 protocol 相关内容 |
| `client/cli` | 从 flag 到 session：一个 binary 即可完成 host、connect 与打开 shell，无需 GUI toolkit。它 link 桌面 app 所用的同一套各 OS media 库 |

`core/` 必须始终可在离线、无 network 与 GPU 的条件下测试。`platform/` 可以使用 OS，但
必须在各平台提供完全一致的 API。若同一段代码出现在两个 client 中，则它应归属更低的
layer。

## 2. 一个 port，一个 transport

host 提供的全部功能都运行在**一个 UDP port**（默认 47777）之上，经由一个
`SessionTransport`，后者封装单个 `QuicEndpoint`：

```
                      UDP port 47777
                            |
                 ClassifyPacket（检查首字节）
                   /                    \
            QUIC packet            Deskhub datagram
                 |                        |
   +-------------+------------+       仅 beacon:
   |             |            |       以明文应答 LIST_SOURCES / PING；
 stream      datagram      (TLS)      其他裸 packet 一律丢弃
   |             |
 control      video
 input        audio       stream 承载经 framing 的 record（RecordStream）:
 clipboard                带 length prefix 的 message，最大 16 KiB。
 terminal                 每个 datagram 承载一个 video 或 audio
 file                     packet（≤ 1200 B）。
```

- **Stream**（可靠、有序）：control、input、clipboard、terminal、file。每条 connection
  使用一条由 client 打开的 bidirectional stream。某条 connection 上阻塞的 stream 不会
  影响其他 connection。进入的 stream 数据按每轮 service 64 KiB 的预算处理：消费数据的
  组件，主要是 terminal 的 VT emulation，会在各分片之间将控制权交还给 ACK、keepalive
  与 timeout 处理，因此 terminal 的大量输出不再导致 connection 因 idle timeout 被关闭。
- **Datagram**（不可靠、无序，但仍经 encrypt）：video 与 audio 的 packet。QUIC 不重传
  丢失的 packet；video 由 app 自身的 FEC/NACK 机制处理丢失，audio 则没有相应机制 ——
  见第 9 节。
- **裸 UDP** 仅用于 discovery：beacon 应答不使用 QUIC 的 scanner，而未经邀请的 probe
  只会得到空的 source 列表。不属于 discovery 类型的裸 packet，在到达任何 session 代码
  之前即被丢弃。

`QuicEndpoint` 完全隐藏 quiche（pimpl；`QuicEndpointNone.cpp` 提供 stub，但仅在 build
显式使用 `-DDESKHUB_QUIC=OFF` 时生效。缺少 quiche 会导致 configure 失败，因为 stub
binary 既无法 share 也无法 connect）。connection 以 peer 地址标识，不支持 connection
migration。按约定，quiche 的 connection 是 single-threaded 的，因此对 endpoint 的所有
操作都在 transport 的 send mutex 之下进行。transport 不会在阻塞的 socket 等待期间持有
该 mutex：先在未加锁状态执行 `WaitReadable`，随后加锁执行一次短暂的 `Poll`。若在等待
期间持有该 mutex，将阻塞所有发送方。

## 3. 准入：pairing

每台机器在首次运行时创建一个 ECDSA P-256 key（`HostIdentity`），其 SHA-256 SPKI hash
即为用户所见的 fingerprint。TLS 使用基于该 key 的自签 certificate。在 TLS 之上，应用层
handshake（`AuthNegotiation`）按 connection 决定准入。transport 负责执行该 handshake，
并丢弃来自 auth 尚未完成的 connection 的所有 message：

| client 提供的内容 | host 是否认识该机器 | 结果 |
| --- | --- | --- |
| 不提供 | 已 pair | **Signature**: client 使用自身 key 对包含 nonce 与 host fingerprint 的 transcript 签名，随即被接受。 |
| 不提供 | 未知 | **Approval**: 询问 host 前的用户（*Let this machine in?*）。 |
| 提供 passcode | host 设有 passcode | **Passcode**: 在加 salt 的 verifier 上执行 SPAKE2。码本身不经过网络，每条 connection 仅允许一次尝试，双方均需证明，且 MAC 绑定到 client 实际接收到的 host key，从而使 relay 攻击无效。填入的码始终会被校验，无论是否已 pair。 |
| 提供 passcode | host 未设 passcode | 无可比对的值 → 已 pair 走 Signature，否则走 Approval。 |
| 任意 | pairing 已关闭 | **Denied**（已 pair 的机器仍走 Signature）。 |

成功后 client 被写入 host 的 `paired_devices`；pairing 基于 key，而非地址。passcode 连
续错误三次将使 passcode 通道锁定 30 秒（`AuthThrottle`，与旧的 session lockout 共用
常量）。approval 通道无需 throttle，因为由人进行判断。

在 client 侧，`known_hosts`（`TrustStore`）固定 host 的 key。key **发生变化**时将以明确
的警告阻止连接；未知的 key 由 handshake 本身处理 —— 已证明 passcode 的 host 会被直接
记录，不再提示。

线上传输的是 public key 本身，而非单独的 fingerprint：host 对收到的内容自行计算 hash，
因此冒用他人身份需要使用冒名者并不持有的 key 进行签名。由于准入在每条 connection 上仅
处理一次，transport 之上的任何组件都不会再次询问：已证明身份的机器在后续 message 中不
携带 passcode，session 代码将整条 connection 视为已 authenticate。

## 4. Host 侧

```
HostEngine（每个 app 一个实例，持有 SessionTransport）
 ├─ net-loop thread: RunHostNetLoop
 │    recv → beacon 应答 | video 数据摄入 | Chan::Terminal → TerminalHost
 │    按 source 的 session Tick、clipboard flush、reconfig、统计
 ├─ capture/encode: 按 source，由 OS 的 capture 回调驱动（client 层）
 │    frame → encoder（按 source 的 mutex）→ Packetizer → FEC → SendTo（datagram）
 ├─ audio worker: capture 回调 → 无锁 frame ring → Opus encode →
 │    按 viewer 的 datagram（AudioBroadcaster）
 └─ TerminalHost（仅在 terminal 被共享时存在）
      ├─ 在 net-loop thread 上 HandleMessage: TERM_OPEN/DATA/RESIZE/CLOSE/EXIT/LIST → PTY
      └─ pump thread: PTY 输出 → host 侧 Screen mirror 与 TERM_DATA record、
           peer 丢失时分离、kicks
```

- 只要有内容被共享，engine 即处于运行状态。当没有 screen source 而仅勾选 terminal 时，
  engine 以无 source 的方式运行；只要 terminal 存在，循环即继续。
- 每个 screen source 对应一个 `SourcePipelineState`，拥有各自的 `ScreenHostSession`
  （viewer 表、negotiation、input 仲裁）、encoder、quality ladder 与诊断数据。一次
  encode 服务该 source 的全部 viewer。
- 反馈环：viewer 每秒发送一次 `Feedback`（loss 与 RTT），host 另外提供一个自身的信号，
  即 frame 到达发送环节时的时延，也就是 `enc_lat_ms` 报告的量。`BitrateController`
  （AIMD）与 `QualityLadder` 依据这三个信号调整 encoder 的 bitrate、分辨率与 fps。FEC
  自第一个 frame 起即启用，仅在长时间无丢失后才关闭，因为它所防范的丢失会在第一份报告
  之前出现；积压状态不会启用 FEC，因为 parity 只会加深队列。quiche 的 CUBIC congestion
  control 位于 datagram 通道之下，两者串联工作：quiche 限制离开本机的数据量，app 依据
  由此产生的丢失调整 encoder。
- Input：host 优先。当机器前的用户操作自己的 mouse 时，`LocalInputMonitor` 暂停 remote
  input；同一时刻只有一个 viewer 进行操作。
- Shell：每个 shell 对应一个 PTY（Windows 为 `ConPTY`，其他平台为 `forkpty`），最多 8
  个。连接中断时 shell 被分离，PTY 一直保留到 shell 进程退出或 shell 被关闭，不设时间限制。任何已准入的 client 均可列出被保留的 shell（`TermList`/`TermListAck`）并按 id reattach 其中之一。每次 open、close、detach 与 reattach 均连同地址、名称与 key 记入审计日志。
- 每个 shell 的输出自启动起也同时写入 host 侧的 `core/terminal` Screen。*Stop & attach*
  断开远端 client，并在 host 的 terminal 窗口中打开该 mirror，scrollback 保持完整。以此
  方式接管的 shell 归属于 host，不会过期，并在 host 的窗口关闭时结束。
- `TERM_CLOSE` 在 data 与 resize message 所受的 per-peer guard 之前被处理，因此任何已准入
  的 client 都可以按 id 结束任意一个 shell，而当时身处该 shell 的机器会收到 `TERM_EXIT`。
- 一个 picker，五个 client：`core/ui/ShellPicker` 把 `TermSessionList` 变成每个 client 都
  绘制的那些行 —— id 与尺寸、shell 属于谁，以及本 client 是否可以 reattach 或关闭它。只有
  已 detach 的 shell 才能 reattach，被 host 接管的 shell 两者皆不可。Apple 与 Android 的
  app 通过 `DHTermSessionInfo` 读取同样的行，因此没有任何 client 自行格式化 shell 行。

## 5. Client 侧

所有 client 界面都通过同一个组件 `HostLink`（`platform/client/HostLink`）连接到 host：
它建立 QUIC connection、检查 trust store、执行 auth handshake、维持 link，并在有此需求
的界面上于 link 中断时以 backoff 方式重新连接。没有任何 service 自行建立连接或
authenticate；service 按 wire 上的 `Chan` 打开一条 channel，获得独立的 inbox 队列，并在
自身的 thread 上处理该队列：

```
HostLink（每个打开的界面一个实例）
 ├─ link thread: dial → 检查 trust → auth → pump
 │   （按 Chan 将进入的 record 与 datagram 分发到各 channel 的队列；
 │    link pulse；在启用 recovery 的场景下以 backoff 重新连接）
 ├─ Chan::Control/Video/Audio ─> ScreenViewer
 │    ├─ net thread: HELLO/negotiation、video 摄入（Reassembler 与 FEC）、
 │    │   NACK、feedback、clipboard
 │    └─ decode thread: decoder 与 render 队列
 ├─ Chan::Terminal ─> TerminalViewer 的 service thread
 │    ├─ core/terminal 的 Screen 保存字符网格
 │    └─ UI 轮询 Snapshot()，并将按键送入 command 队列
 └─ Chan::File ─> FileTransferClient 的 service thread（FileUpload ring）
```

准入完成后，link 自行监测自身状态（`core/session/LinkPulse`）：每秒发送一个 session id
为 0 的 `Ping` datagram，host 的 beacon 在同一条 connection 上应答且无需 session，回传
的时间戳形成平滑后的 RTT，而未返回的 pong 的 id 构成丢包率。`ClassifyLinkQuality` 将两
者归纳为 Good / Fair / Poor，供设备列表以及接收 host 应答的面板使用 —— 桌面端为独立
窗口，Android 与 iOS 为 connect 页。session 窗口不再显示该指标，`HostLink` 通过
`onPulse` 与 `Pulse()` 对外提供。由于 ping 是 ack-eliciting 的，它同时充当 keepalive；
普通的 keepalive 定时器仅在 link 处于 `Deciding` 状态时仍有意义。过旧的 host 无法应答
session-0 的 ping，此时该指标保持为 Unknown，不产生其他影响。在恢复中的 link 上，该
pulse 同时用作 liveness 检查：连续五秒未收到 pong（且仅在首个 pong 已确认 host 会应答
之后计算），即将 connection 转入既有的重连流程。这五秒按 link 循环实际处于监测状态的
时间计算：`LinkPulse::Tick` 在 `HostLink::PumpReady` 每轮执行一次，某一轮中超出
`kLinkWatchStepUs` 的部分会从静默时间中扣除，因此本机卡顿不会被误判为 host 停止应答。

screen viewer 现在也采用该恢复机制，与 terminal 一致：link 中断或静默，或 session 连续
五秒未接收数据，会使窗口进入 `Reattaching` 状态（保留最后一帧画面，status 行切换为
reattach 提示），而不是直接结束。当 session 先发现问题时，`HostLink::RequestRedial` 触
发重连；link 重新获得准入后，viewer 以相同的 client id 重新执行 `HELLO`，host 重新绑定
该 viewer 的位置，stream 从新的 keyframe 恢复。若六十秒（`kViewerReattachGraceUs`）内
仍未恢复，窗口按常规流程连同原因关闭。

source 查询（`QuerySources`）以一次性、阻塞的形式使用同一条 link。UI 仍将各项请求（按
键、resize、接受 fingerprint）送入 command 队列。host key 发生变化时，link 保持在
`Deciding` 状态，直至用户接受或拒绝。terminal 窗口不解析 escape sequence：
`core/terminal` 将 byte stream 转换为字符网格，窗口仅负责绘制单元格并转发按键事件。目
前每个窗口仍各自持有一条 link；让指向同一 host 的所有窗口共享一条已准入的 link 是既定
的下一步，将在 `HostLink` 处以 registry 加 observer fan-out 的形式实现，而不是新增一次
handshake。

## 6. Discovery

beacon 以明文 UDP 应答 `LIST_SOURCES` 与 `PING`，使 scanner 扫描一个 subnet 时无需执行
254 次 TLS handshake。未获准入的机器得到的是空列表；真实的 source 列表仅在已准入的
connection 上提供。该应答同时通过 `SOURCE_LIST` 的 header flag 说明 host 的能力 ——
是否接受 input、是否共享 terminal —— 因此 client 在打开任何窗口之前即可得知手机只能被
观看。早于这些 flag 的 host 不会设置任何一项。最近设备、其在线状态（ping/pong probe）
以及 LAN scan 结果汇总为同一份设备列表，由 `core/ui/DeviceRows` 构建，并由五个 client
共同使用。

## 7. 磁盘上的数据

全部数据位于用户的 Deskhub 文件夹（`~/.deskhub`、`%USERPROFILE%\.deskhub`）：
`host_key.pem` 与 `host_cert.pem`（identity）、`known_hosts`（本机 trust 的 host）、
`paired_devices`（本 host 接受的机器）、`auth_salt`（verifier 使用的非机密 salt）、
`ui-settings.txt`、`recent-devices.txt`（地址与遮蔽后的 passcode）、Linux 上的
`portal-restore-token.txt`（桌面针对所选屏幕签发的 token），以及每次运行的 log。文件
I/O 位于 `platform/`；解析逻辑与数据结构位于 `core/`，并具备 unit test。

viewer 发送的文件保存在其他位置：由 host 选定的文件夹（`ui-settings.txt` 中的
`transfer_dir`，默认为用户主目录下的 `Deskhub`）。`FileStore` 将每个文件写为
`<name>.deskhub-part`，仅在整个文件到达且 CRC-32 匹配后改名，因此写入中途的文件不会以
真实名称出现；`UniqueFileName` 确保不覆盖任何文件。线上的文件名先由 `core/` 的
`SafeFileName` 处理，去除路径分隔符、控制字节、Windows 不接受的字符以及保留设备名，
然后 `platform/` 才访问 filesystem。

## 8. 测试

| Suite | 运行环境 | 覆盖内容 |
| --- | --- | --- |
| `make test` | 离线，不使用 socket | 整个 `core/`: wire、framing、FEC、session、VT emulator、settings、文案、确定性的 structured fuzzing |
| `make test-platform` | loopback socket | 真实的 QUIC handshake、端到端的 SPAKE2、经由网络的 terminal host 与 viewer、面向真实 shell 的 PTY、lockout、approval |
| `make test-integration` | loopback，capture/encode 为模拟实现 | 完整的 host↔client session: negotiation、经网络传输的视频、input、passcode 与 approval 的准入控制、对无效数据的容错，以及交叉负载下的时延 —— 文件传输、大量输出的 terminal 与按键操作与运行中的 stream 并行，各自按观测到的最大停顿设定阈值 |
| fuzz target | 每个 PR 上每个 target 30 秒，nightly 每个 15 分钟 | wire、H.264、reassembly、terminal 字节与 UI 文本的 parser，以及 host 与 viewer 两侧的 session state machine |
| `make test-perf` | release build，离线与 loopback | 对 hot path 进行实测: `core_perf` 覆盖纯 C++ 的路径，`platform_perf` 覆盖 loopback 上的真实 QUIC；两者均按每单位的 allocation 次数、4 倍输入下的开销，以及相对本机 baseline 的偏移进行判定 |

CI 另外强制执行 clang-format 与 clang-tidy（两者均固定版本）、SwiftLint `--strict`、
Android Lint、actionlint 与 shellcheck、三个 suite 在 ASan 与 TSan 下的运行、对
C++/Kotlin/Swift 的 CodeQL、对完整历史的 gitleaks 扫描，以及 `core/` 的 line ≥ 90 % 与
branch ≥ 80 % coverage。这三个 suite 还会被 cross-build 并在 arm64 Linux、Android
emulator 与 iOS Simulator 上运行。此外，一个 Windows job 每轮将 integration suite 额外
运行三次，用于定位一处间歇性的 memory corruption，该问题约每三次运行出现一次。发生
crash 的 frame 是该 corruption 的结果而非起因，因此 crash 必须留下 dump：测试 binary
为每个到达 handler 的异常写出完整的 minidump；由于 fastfail 不会到达任何 handler，各
Windows job 还会启用 Windows Error Reporting，并在其守护的 suite 运行之前，以一次有意
的 fail-fast 验证采集是否正常。nightly 将 load test 额外运行两轮：一轮启用 full page
heap，另一轮针对启用 Rust debug assertion 与 overflow check 构建的 quiche —— 这是唯一
能够观察 quiche 内部的手段，因为 ASan 不 instrument Rust，而 page heap 只保护 heap。
Linux 与 macOS 的 release job 同样运行 `core_perf` 与 `platform_perf`，并应用 allocation
与 scaling 两项判定（共享 runner 上不存在时间 baseline）。每个 pull request 还会收到一
份 perf-and-lag 报告，以一条自动更新的 comment 呈现，内容包括：两个 perf suite 在同一
runner 上与 base commit 的 A/B 结果（偏移仅作为警告，不导致失败）、来自 pull request
构建的负载下 integration 数据，以及 core 的 coverage 行。

## 9. 需要记录的设计决策

- **返回 false 的 capability probe 可能使整个控制环失效。** 当 MFT 未提供
  `CODECAPI_AVEncCommonMeanBitRate` 时，Media Foundation 的 encoder 会对 `SetBitrate`
  返回 `false`，而 `ApplyFeedback` 正确地将该拒绝理解为「未应用任何变更」。在报告
  `MeanBitRate: NOT SUPPORTED` 的 Intel Quick Sync MFT 上，结果是 host 从不改变
  bitrate：在该硬件上实测，30 秒持续 29-40 % 的丢包未产生任何 `Bitrate` 决策，quality
  ladder 也未发生变化。启动日志全程显示 `NOT SUPPORTED`，但未被理解为自适应机制已停止
  工作。同一文件中的 `SetFps` 与 `RequestKeyFrame` 早已具备回退到 `ReinitTransform()`
  的路径，唯独 `SetBitrate` 没有，现已按相同方式补充：`ConfigureTransform` 会从 `cfg`
  写入 `MF_MT_AVG_BITRATE`，因此重建一次即可应用新的速率。重建的代价是一个 IDR，因此仍
  优先尝试在线的 `codecapi` 路径。当按设备而异的 capability 控制着某一路控制输入时，
  回退必须是强制的：降级为更低性能是一种选择，静默降级为完全不生效则不是。

- **跟不上的发送方与无丢失的链路表现完全相同。** `BitrateController` 的所有输入 ——
  loss、RTT、接收速率 —— 均来自 viewer，因此控制环中没有任何组件能够判断落后的是发送方
  自身。在一台为两个 viewer 提供 host 的 Pixel 4 上实测：frame 离开 encoder 时已滞后
  15 秒，而 viewer 报告 0 % 丢包与 15 ms RTT，控制器将其视为余量并把 bitrate 重新提升至
  20 Mbps 上限。这是发送方内部的 bufferbloat：链路看起来越好，推送的数据量越大。现在
  host 在发送环节测量 frame 的时延，并将其与 viewer 的数据一同输入：超过 `kBacklogMs`
  时按 2 % 丢包处理，超过 `kSevereBacklogMs` 时按 5 % 丢包处理，两者都会按惯例封锁两秒
  内的回升。bitrate 仍是唯一的控制变量，因此 `QualityLadder` 随之下调，fps 上限相应调
  整。仅由对端提供数据的控制环，无法观察自身实际掌控的那半条 pipeline。

- **限制 fps 只在确实会丢弃 frame 的位置才有效。** ladder 的 fps 档位是一项请求，各平台
  必须在某个可以丢弃 frame 的位置予以落实。Windows 与 Linux 通过 `FrameGate` 在 capture
  阶段处理；Android 通过 `max-fps-to-encoder` 限制 MediaCodec 的输入；macOS 重新配置
  ScreenCaptureKit 的 frame interval。iOS 缺少相应位置：ReplayKit 按屏幕速率提供 frame，
  而 `VtEncoder::SetFps` 仅设置 `kVTCompressionPropertyKey_ExpectedFrameRate`，这是给
  rate control 的提示，不会丢弃任何内容。在此处调整档位只是重新配置 encoder，其需要处理
  的 frame 数量并未改变。现在 `OfferVtFrame` 在两个 Apple app 上运行同一个 `FrameGate`，
  并置于 idle-flush 缓存刷新之后，使静止画面仍有可重发的 frame。当某个参数在所有平台上
  都存在时，应先确认各平台如何处理它，再依赖 ladder。

- **send pacer 必须显著高于 encoder 自身的输出速率。** `Pacer::Gate` 在
  `SendEncodedFrame` 所处的 thread 上等待，而在 Android 上该 thread 即 MediaCodec 的
  drain 循环，也就是必须先调用 `releaseOutputBuffer`、encoder 才能交付下一帧的循环。
  因此 pacing 决定的不仅是线上速率，还包括 drain 速率，而 VirtualDisplay 仍按屏幕速率
  持续送入新的 frame。为平滑发送突发而将 `kPacingRateMultiple` 从 2 收紧至 1.2 的改动，
  已在 Pixel 4 上实测：每帧的突发中位数由 20 ms 升至 63 ms，encoder 的积压无限增长 ——
  `enc_lat_ms` 在 100 秒内超过 46 秒，viewer 落后 4.6 秒。取值为 2 时，同一次运行将
  `enc_lat_ms` 保持为 0。这部分余量不是可以回收的冗余，而是保证 encode pipeline 的排空
  速度快于填充速度的前提。处理发送突发应使用 socket buffer，或将 pacing 移出 drain
  thread，而不是调低该数值。

- **perf suite 依据开销判定，因此还需要一项依据结果的判定。** `core_perf` 测量每个
  packet 的 allocation 次数以及时间随输入的增长方式；在真实链路上单个丢包导致 22 % 的
  完整 frame 被丢弃期间，其全部 reassembler workload 仍然通过。该 suite 无法发现这一
  问题：丢弃有效 video 的开销*低于*对其 decode，因此错误的策略在该 suite 关注的每一项
  指标上得分更高。`LossGoodputTests` 是配套的判定，在代码所做工作少于应有水平时失败：
  它模拟带真实往返时延的尾部丢包链路，依据「所有 packet 均已到达的 frame 中实际抵达
  decoder 的比例」以及「相邻两个已交付 frame 之间的最大间隔」进行判定。两者均与硬件
  无关，因此在笔记本、CI runner 与手机上结果一致。当某项策略可以通过减少工作量而「成
  功」时，都应引入 goodput 判定。

- **单个 packet 丢失应只影响一帧，而不是影响到下一个 keyframe 为止的全部画面。**
  reassembler 此前在每次丢包时都置位 `waitingForIdr_`，因此单个缺失的 packet 会导致其后
  所有*完整*的 frame 被丢弃，直到新的 IDR 到达。在通过 Wi-Fi 作为 host 的手机上实测，这
  使 64 个真正不完整的 frame 变为 381 个被丢弃的 frame：6.4 MB 可 decode 的 video 被
  丢弃，画面冻结的中位数为 146 ms，单次最长 1.4 秒。现在仅丢弃不完整的 frame，其后的
  frame 直接进入 decoder，由 decoder 遮盖缺失的参考帧，同时 `InvalidateRef` 向 host 指
  明出错的 frame，并由 keyframe 请求完成修复。短暂的宏块瑕疵是为避免画面冻结而接受的
  代价。`waitingForIdr_` 仅保留用于其原本正确的一种情形：中途加入的 viewer 没有任何参考
  帧，必须等待第一个 IDR。

- **判定丢失前的等待时间必须长于一次 retransmit，否则 NACK 不起作用。** 此前一帧在被
  判定为丢失前仅等待两个帧间隔（60 fps 下为 33 ms），而同一链路上实测的 RTT 为
  24-49 ms。NACK 发出后，应答到达时该帧已被丢弃，表现为 `late_ms_avg=24`，且每秒有 87
  个 packet 属于已不存在的 frame。现在 `StallTimeoutUs` 取「按 pacing 计算的等待时间」
  与「1.5 倍往返时延」中的较大者，并仍受硬性 timeout 的上限约束，因此仅在确有需要的链路
  上才请求重传。

- **performance suite 依据 allocation 与开销的增长形态判定，而非毫秒数。** 三个 test
  suite 以 debug 方式构建，CI 还会在 ASan、TSan 与 coverage 下再次运行，此时以挂钟时间
  设定的预算衡量的是 sanitizer 而非代码本身。因此 `core_perf`（release preset，
  `make test-perf`）依据两项与硬件无关的指标判定失败：通过替换全局 `operator new` 计数
  的每 packet、每 frame 或每 KB 的 allocation 次数；以及某一行 `-scaling` 的耗时增长远
  快于输入。计时部分保留为与 `out/perf/baseline.txt` 的对比，该文件由 `make
  perf-baseline` 按机器生成，不纳入版本控制。这种划分使该 suite 能在笔记本、CI runner
  与手机上同样地判定出「reassembler 现在将每个分片复制两次」这类回退，同时仍为数值本身
  具有意义的路径输出每单位的 ns 与 MB/s。CI 在 Linux 与 macOS 的 release job 上执行这两
  项与硬件无关的判定；Windows 仅构建 binary，因为对于大于 16 字节的元素，MSVC 的 deque
  会为每个元素单独 allocate 一个 block，同样的代码在该平台的 allocation 计数不同。pull
  request 还会获得一份不受共享 runner 噪声影响的计时对比：base commit 与 pull request
  在同一 runner 上测量，容差 50 %，仅作为警告。`platform_perf` 将同样的判定扩展至
  loopback 上的真实 QUIC，此处挂钟时间反映的是 service 循环的节奏 —— 64 KiB 的 stream
  处理预算乘以 1 ms 的 poll tick —— 因此预算被缩小、处理不再线性扩展，或 poll 循环中新
  增一次 allocation，都会表现为明显的跳变，即使同样工作的 CPU 开销几乎不变。

- **`FileHost` 不在持有自身 lock 时发送数据。** QUIC 的 service 循环在
  `SessionTransport::sendMutex_` 之下执行 `QuicEndpoint::Poll`，而在该处关闭的
  connection 会直接回调至 `FileHost::OnPeerGone`，后者会获取 `FileHost::mutex_`。因此
  `sendMutex_ -> mutex_` 的顺序由 transport 固定。任何先获取 `mutex_` 再发送的路径 ——
  例如 `FileReceiver` 通过 `hooks.send` 发出 accept、ack 或 cancel —— 都会形成锁环，
  TSan 已将其识别为接收循环与在活动传输上切换 `SetAccepting(false)` 的 UI thread 之间的
  lock-order inversion。因此 receiver 产生的 record 在 `mutex_` 之下写入 `outbox_`，并
  在释放该锁之后才发送，两个阶段均持有 `outboxMutex_`，以保证 peer 收到的顺序与产生顺序
  一致。`OnPeerGone` 不发送任何数据：它本身运行于 `sendMutex_` 之下，因此丢弃已排入队列
  的内容。

- **command line client 是第四个前端，而非第二套实现。** 它在 `core/cli` 中解析 flag，
  随后驱动与桌面 app 完全相同的组件：`SharingHost` 用于 host，`ScreenViewer` 用于观看，
  `TerminalViewer` 用于打开 shell。它自身独有的只有显示窗口：Linux 上为 X11 与 EGL，
  Windows 上为桌面 app 自身的 `RunViewer`。这正是每个 client 的 `cpp/` 树被组织为静态库
  （`deskhub_linux_core`、`deskhub_win_core`、`deskhub_win_view`、`deskhub_mac_core`）
  而 GUI 代码位于其上的原因：这种划分使 CLI 能够 link media pipeline，而无需 link GTK
  或 wxWidgets。

- **`preflight` 仅在存在需要 capture 的屏幕时执行。** 所有 client 都用它检查 capture
  路径：Linux 上的 xdg portal、macOS 上的 Screen Recording 授权、Windows 上的 D3D11
  设备。仅包含 shell 的共享不需要这些条件，因此无条件检查曾导致无显示器主机上的
  `share --terminal` 报告 screen-capture permission 缺失。现在 source 列表为空时，
  `HostEngine::Start` 会跳过该检查。

- **只有 shell 而没有屏幕的 host 仍保持运行。** net 循环在没有任何 source 处于活动状态
  时结束 session，而仅共享 terminal 的场景按定义没有 source。`keepAlive` 依据调用方的
  意图（`ShareOptions::terminal`）确定，而不是依据在循环启动之后才挂接的 `TerminalHost`
  指针。

- **frame gate 按目标时刻计数，而不是从上一个保留的 frame 开始计数。** 当 compositor
  以 40 fps 向 30 fps 的目标提供画面时，大多数 33 ms 边界上并没有 frame，因此仅判断「与
  上一个保留帧的间隔是否足够」的 gate 会每隔一帧丢弃一帧，最终稳定在 20 fps：低于目标
  且不均匀，表现为 judder 而非速率更低的 stream。`FrameGate` 改为维护一个滚动的目标
  时刻：每次放行将其精确推进一个间隔，因此余量得以保留，输入 40 帧可输出 30 帧。低于
  目标速率的 capture 不会被抽稀，而已落后于真实时间的目标时刻会重新同步而非累积，因此
  一段空闲时间不会换来之后的突发。

- **Linux host 在独立 thread 上 encode，并向该 thread 传递缩小后的 frame，而非完整
  frame。** 在 PipeWire 的 `process` 回调中执行 encode 会将 capture 限制在
  `1000 / enc_ms` fps，并使每次 encode 耗时的波动转化为 client 侧的帧节奏抖动。现在
  encode 在独立 thread 上运行，通过 `FrameMailbox` 获取数据 —— 这是一个采用「保留最新」
  策略的单槽队列：encoder 落后时保留最新的 frame，旧的 frame 被计数而非排队。跨越队列
  传递的是已缩放到 encode 尺寸的 frame，约为原始数据量的七分之一。改为传递全分辨率的
  frame 所付出的代价远高于复制本身：capture 所在核心中留下 20 MB 处于 dirty 状态的
  cache line，随后 encode 所在核心必须将其取回，实测为 16 ms，而读取同等数量、但归其
  所有的内存仅需 3.4 ms。capture thread 无论如何都要处理每个源像素一次，因此这是执行该
  次遍历的正确位置。dma-buf 的 frame 仍在原位 encode：回调返回后 compositor 即复用其
  后备内存，因此它们无法存活于回调之外，而 VA-API 本身也在 GPU 上完成缩放。
- **Linux host 依据 frame 所处位置选择 encoder，而非依据已安装的软件。** dma-buf 的
  frame 交由 VA-API，它可以在生成该 frame 的 GPU 上以 zero-copy 方式导入；已映射到 CPU
  内存的 frame 在存在 NVIDIA driver 时交由 NVENC，因为在由 NVIDIA GPU 渲染的桌面上，
  compositor 会将 screencast 重新协商为共享内存，此时 encode 应由能够直接从系统内存
  读取像素的显卡承担。`HwEncoder` 在每次重建 encoder 时作出该判断，之后若收到另一类型
  的 frame 则返回 `false`，这即为需要重建的信号。
- **NVENC 之前的缩放由本项目实现，而非使用 swscale。** NVENC 接受 packed 的 32 位像素
  但不执行 resize，而 capture 得到的是全分辨率桌面。`libswscale` 在 3440x1440 →
  1280x534 上实测为 9.2 ms，约合 2 GB/s，比本机内存带宽低一个数量级，原因是 packed RGB
  的缩放不在其优化路径内。`core/` 中的 `RgbDownscale` 是针对该场景实现的面积平均算法：
  每个源像素一次 32 位 load，使用整数累加，同一 frame 实测 4.0 ms，并给出正确的抗锯齿
  结果，而非 swscale 采用的双线性单点采样。整帧 NVENC 的开销约为 5 ms，因此 60 fps 仍有
  余量。
- **性能数据只有来自 release 构建才有意义。** `make build-linux` 与 `make run-linux`
  配置的是 `x64-debug` preset，即 `-O0`，而 encode 路径现已是 `core/` 中的像素运算。同
  一帧在该配置下约需 19 ms，而 `make release-linux` 构建的版本约需 5 ms。基于 debug
  binary 得出的 judder 报告，实际衡量的是构建类型。

- **Apple 平台的 viewer 依据 PTS 在 control timebase 上进行视频节奏控制，且 pacer 不
  信任自身的结果。** 每帧一到达即显示，会使 Wi-Fi 的到达抖动表现为 judder，而各项
  latency 指标仍然良好：显示节奏与 latency 不是同一回事。`VideoPacer`（位于 core，具备
  离线测试）以与 e2e 指标相同的方式将 host 的 PTS 映射到本地显示时间 —— 采用
  `arrival − pts` 在滑动窗口内的最小值 —— 并加上约 33 ms 的提前量以吸收到达抖动，
  `VtDecoder` 据此驱动 `AVSampleBufferDisplayLayer` 的 control timebase，仅在偏差超过
  250 ms 时重新同步。超过 2 秒的 pts 跳变被视为新的 stream 而非抖动，因此映射会重新
  初始化，而不是在一个窗口内保持冻结。由于无法在此确认所有 OS 版本的渲染器都遵守外部
  timebase，decoder 会自行核查：连续多个经节奏控制的 frame 被已满的渲染队列丢弃时，它
  将切回立即显示模式并执行 flush，宁可放弃平滑处理，也不放弃画面。

- **音频为每个 datagram 一帧，丢失的 packet 不予重传。** 64 kbps 下 20 ms 的 Opus frame
  约为 160 字节，最大 209 字节，而一个 datagram 可容纳 1180 字节。因此音频通道没有
  packetizer、FEC、reassembler 与 NACK，而这几乎是 video 通道的全部组成。丢失在代价最低
  处处理：Opus 在下一帧中携带 in-band FEC，接收端让 decoder 遮盖 jitter buffer 报告的
  缺口。重传没有意义，因为延迟 200 ms 到达的帧既无法播放，又会延后其后的十帧。
  `make opus-smoke` 可在任何能够构建该库的机器上测得上述数据。
- **jitter buffer 中不含定时器。** `AudioJitterBuffer` 仅为状态机，目标延迟即为开始播放
  前需要缓存的帧数，60 ms 对应三帧。这使整个组件可以离线测试而无需等待实际时间，也使各
  失败情形变得明确：突发被限制而非排队，缓冲为空时重新缓冲而非断续播放，序号跳变被视为
  新的 stream 而非数千帧丢失。节奏控制位于 `AudioPlayer`，它每 20 ms 实际时间向一个 PCM
  ring 送入一帧，由 sink 的 render 回调读取。
- **capture 回调不执行 encode。** PipeWire 与 ScreenCaptureKit 在实时 thread 上提供
  音频，截止期为数毫秒，在此处超时会导致 host 自身音频播放的 xrun，而不仅是 Deskhub 的。
  Opus encode 耗时 0.3–1.5 ms 并存在尖峰，此前每个 viewer 的 `sendto` 还紧随其后运行在
  同一 thread 上。现在 `AudioBroadcaster::Offer` 仅将 20 ms 的 frame 复制到预先分配的
  无锁槽位 ring 中并记录 capture 时间；encode、诊断与按 viewer 的发送由 worker thread
  完成。worker 处理不及时的后果是一次被计数的丢弃（`framesRefused`），而不是 host 音频
  中的异常。
- **音频需要双方同时启用，旧版本 client 不会接收。** viewer 设置 `Hello.features` 的
  bit 0，host 在其 capability 中声明 `kHostSharesAudio`，host 仅向设置了该位的 viewer
  发送 packet。这正是 `kProtocolVersion` 保持为 2 的原因：5.0.x 的 viewer 发送
  `features = 0`，因此 5.1 的 host 不会向其发送无法解析的 message。

- **terminal 的 link 自行保活并自行重连。** terminal viewer 持有独立于 video session 的
  QUIC connection，因此 video 通道的 keepalive 都不会到达它。在提示符处无操作时该
  connection 没有流量，会因 QUIC 的 30 秒 idle timeout 被关闭，随后 viewer 在
  `Reattaching` 状态下停止 thread 而不重连，而 shell 仍被 host 保留完整的 2 分钟。现在
  `TerminalViewer` 按定时器发送 ack-eliciting 的 packet 并以 backoff 方式重连，复用
  `TerminalClient::Reattach()`（该函数早已在 core 中实现并测试，只是从未被调用），从而
  使同一个 shell 连同 scrollback 一起恢复。相关时间常量位于 core 的
  `deskhub::KeepaliveIntervalUs` 与 `ReconnectDelayUs`：keepalive 不超过 idle timeout
  的一半，以便承受一次 packet 丢失；重试恰好在 `kTerminalReattachGraceUs` 处停止：超过该时刻窗口报告连接丢失，但 shell 本体无时间限制地留在 host 上，可供后续显式 resume，而不是被释放。
- **record 要么完整写入 stream，要么不写入；落后的 client 通过重绘同步，而非逐字节
  补发。** 所有可靠数据 —— control、auth、terminal 输出 —— 都是共用一条 QUIC stream 的
  带 length prefix 的 record，因此线上出现半条 record 会永久破坏对端的 framing；
  `RecordStream` 没有重新同步的手段，peer 只能关闭 connection。`QuicEndpoint::SendStream`
  此前写入能容纳的部分并丢弃其余，该做法在诸如 `make test` 的命令产生的输出超过链路
  能力时失效：1 MiB 的 stream 窗口被占满，一条 `TermData` record 的尾部被丢弃，viewer
  的 framer 失败，shell 在打开一分钟后断开。现在它会拒绝 stream 无法容纳的 record，并在
  仍然发生部分写入时关闭 connection，因为已错位的 stream 无法原地修复。在其之上，
  `TerminalHost` 将未发送的输出保存在按 shell 划分的队列中，并在每个 tick 重试，因此
  短时间超过链路能力的输出突发（例如一次构建的输出）仍可完整到达 client。超过
  `kMaxPendingBytes` 时队列被丢弃而非继续增长：所有字节都已进入 host 侧的 `Screen`
  mirror，因此通过 `deskhub::term::RenderScreen` 使 client 同步，即对当前网格执行一次
  重绘，最快每 `kRepaintIntervalUs` 一次。用户来不及阅读的输出被跳过而非缓存，从而使持续
  输出的命令能够以自身速度运行，同时仍留下正确的最终画面。正在 reattach 的 client 同样
  接收该重绘，因为在出现中断之后，它在 byte stream 中的位置已无意义。
- **自动共享会等待桌面就绪，而不是只枚举一次。** Windows 将 autostart 注册为 `ONLOGON`
  的 scheduled task，它在 session 具备可枚举的显示器之前即触发，因此构造时的一次
  `ListDisplays()` 此前返回空，app 随即报告没有可共享的内容。
  `deskhub::ui::AutoShareGate`（位于 core，具备 unit test）承载重试规则 —— 每
  `kAutoShareProbeMs` 执行一次 probe，`kAutoShareGiveUpMs` 后停止 —— 各 client 以自身的
  定时器驱动它，因此该策略只存在一份。`NextAutoShareStep` 是同一规则的无状态形式，Swift
  client 通过 `dh_auto_share_step` 使用它。自动共享不会弹出模态对话框：登录时窗口可能位
  于 tray 中，此处的对话框既不可见又会无限期阻塞共享，因此拒绝的原因写入 Host 页的横幅
  与 log。桌面 client 还会在 OS 的 display 变更信号上刷新其选择列表，这使得之后接入显示
  器时列表仍然正确。
- **选择 quiche 而非 msquic 或 ngtcp2。** 这是唯一在 Android 与 iOS 上均有生产使用证据
  的 QUIC 库。它附带 BoringSSL，后者同时服务于 SPAKE2 与 host identity，因此无需第二个
  密码学库。
- **不使用 connection migration。** 候选库均缺乏可用的 client 侧支持。reconnect 与
  reattach 机制（类似 tmux，本就是移动端进入后台所必需）已覆盖该需求；被保留的 shell 也可被列出（`TermList`）并由新 client 按 id resume。
- **使用 ECDSA P-256 而非 Ed25519。** BoringSSL 的服务端不会通过 quiche 以 Ed25519 对
  TLS handshake 签名。不应改回。已保存的 Ed25519 identity 会在加载时被替换，否则它将使
  每次 handshake 以 `QUICHE_ERR_TLS_FAIL` 失败，且界面上没有任何说明。
- **passcode 的 verifier 是一次 SHA-256，而非开销较大的 KDF。** SPAKE2 已将攻击者限制为
  每条 connection 一次在线尝试，且不留下值得离线破解的 transcript，这正是 KDF 的计算
  强度所要达到的目的。
- **quiche 预先构建，不使用 FetchContent。** `scripts/build-quiche.sh` 在
  `third_party/quiche/` 下为每个 rust target 生成一个目录，另有共享的 `include/`，其中
  包含 quiche.h 与 boring-sys 附带的 BoringSSL 头文件。这些头文件被单独取出，因为
  Deskhub 为实现 host identity 直接调用 BoringSSL，并且需要单一的 include 路径与单一的
  TLS 库。`DeskhubQuiche.cmake` 将其转换为 `deskhub::quiche`；缺少该库会导致 configure
  失败。
- **Apple 平台 link `libplatform_bundled.a`。** Xcode 的 app 在 CMake 之外使用 platform
  的 archive，而在该场景下，对 quiche 的 PRIVATE link 不会出现在其 link 行中。因此通过
  一步 `libtool` 将 platform 与 quiche 合并为 `.pbxproj` 所 link 的单个 archive。
- **Windows toolchain 的既有问题已解决，应保持现状。** quiche 通过
  `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS` 使 Rust 目标文件链接静态 CRT，并通过
  `CFLAGS_x86_64_pc_windows_msvc` 中的 `/MT` 约束 BoringSSL 的目标文件（msvc 的默认值为
  DLL 运行时，而通过通用的 `RUSTFLAGS` 传入该 flag 会直接破坏 cargo 构建）。整棵树固定
  `MultiThreaded` 以保持一致，从而使发布的 exe 不依赖 VC++ Redistributable；wxWidgets
  在每次 configure 时重新固定 `wxBUILD_USE_STATIC_RUNTIME`，因为 `wx_option()` 会永久
  缓存该值。BoringSSL 必须在默认的 Visual Studio generator 下构建：在该 generator 下，
  cmake crate 仅通过 per-config 的 flag 传递 /MT，因此强制 `CMAKE_GENERATOR=Ninja` 会使
  BoringSSL 退回 /MD，最终 link 以 LNK2038 失败。若 MSBuild 因长路径触发 MSB6003，应启用
  Windows 的长路径支持。Git Bash 的 `/usr/bin/link.exe` 会遮蔽 MSVC 的 linker，应将
  `cl.exe` 所在目录置于前面；其路径改写会破坏以 `/` 开头的参数，需使用
  `MSYS2_ARG_CONV_EXCL`；NASM 的安装程序不修改 PATH。
- **在 Windows 主机上构建 Android 的 quiche 时不使用 cargo-ndk。** cargo-ndk 向
  boring-sys 传递不带扩展名的 `clang` 路径，而 CMake 在 Windows 上不接受该形式。因此
  `build-quiche.sh` 自行设置 `CC_*`、`CXX_*`、`AR_*`、cargo 的 linker 以及对应 ABI 的
  `--target=`，并直接调用 cargo。BoringSSL 在此仍需要 Ninja，因为 Visual Studio
  generator 无法面向 NDK；而 bindgen 会使用 Visual Studio 的 libclang，后者在自身
  binary 旁查找 `stddef.h`，因此 `BINDGEN_EXTRA_CLANG_ARGS` 以正斜杠指向 NDK 的 resource
  头文件，原因是 bindgen 按 shell 规则分割该变量并会去除反斜杠。
- **每个 cross-compile 的 app 都先构建自己的 quiche。** `build-android`、`build-ios`、
  `build-macos` 与 `build-linux` 均依赖对应 ABI 的 quiche target，正如 `debug` 与
  `release` 依赖 host 的 ABI 一样。quiche 按 ABI 构建，缺少时 CMake configure 会失败，
  因此跳过该步骤的构建表现得更像 toolchain 故障而非缺少库。此外，停留在上次成功构建
  状态的 app 使用的是其他机器已不再支持的 protocol。
- **iOS 的 quiche 固定 `IPHONEOS_DEPLOYMENT_TARGET=17.0`。** boring-sys 的 clang 采用
  SDK 的默认值，而 rustc 按自身的最低版本 link，两者不一致会在 link 阶段表现为未定义的
  `___chkstk_darwin`。
- **两个 clock，出于设计考虑。** `NowUs()` 为单调时钟（自启动以来的秒数），用于时间
  间隔；`NowUnixSeconds()` 是唯一可呈现为日期的时钟。混用两者不会产生明显错误：保存的
  单调时间戳会显示为 1970 年 1 月 1 日的某个时刻。
- **Windows 上的 PTY 子进程不接收任何标准句柄。** 当 host 自身的 stdout 被重定向时，
  Windows 会将该重定向越过 pseudo-console 属性向下传递，shell 随即与管道通信；只有在不
  传递任何句柄时，shell 才会使用已挂接的 ConPTY。
- **Windows 的 terminal 网格需要 `wxWANTS_CHARS`。** 缺少它时，frame 的对话框导航会在
  terminal 处理之前接收 Enter、Tab 与方向键。
- **macOS 的 TCC 将授权与代码签名绑定。** 本地构建的 app.app（ad-hoc，每次构建重新
  签名）与 Developer ID 的 dmg 共用同一条 `com.deskhub.macos` 记录：System Settings 显示
  权限已授予，而刚启动的副本却被拒绝，对 Accessibility 则是无提示的拒绝。
  `make reset-macos-permissions` 清除全部授权，使下次启动重新询问。
- **macOS 在 CI 中为桌面构建，在 release 时为签名构建，两者不同时进行。**
  `build-desktop` 在每次 push 时以 ad-hoc 签名编译 app，因此无法构建的 Cocoa 改动会在其
  自身的 pull request 上失败；`deploy` 通过 `release-macos` 处理同一个 app，即 fastlane
  路径（Developer ID、notarization、dmg），产出用户可以实际打开的版本。因此当
  `for_release` 被设置时，可复用的 workflow 会跳过其 macOS job，否则一个 tag 将额外占用
  一台 macOS runner，用于生成不会发布的 bundle。`build-mobile` 仅包含 iOS 与 Android，
  原因与划分方式相同。
- **所有 workflow 从同一个 action 获取 quiche 与 opus，且 cache key 即为完整的约定。**
  `.github/actions/third-party` 为 job 指定的任意 target 构建这两个库，因此原先十九份
  相同的「先 cache 再构建」代码块缩减为每个 job 一行。其 `cache-key` 输入是防止两个 job
  相互恢复对方库的唯一手段。两组不同的 target 属于不同情形，两个构建同一 triple 的
  runner 镜像同样属于不同情形：在 ubuntu-latest 上编译、在 ubuntu-22.04 上恢复的
  `libquiche.a`，会 link 到该 release 本应避免的 glibc 版本。任何改变构建产物的因素都
  应纳入该 key。
- **Windows 上所有 configuration 均使用同一种静态 release CRT。** cargo 以静态 release
  CRT 构建 quiche（msvc 的默认值；不应通过 `RUSTFLAGS` 强制指定，该设置会波及
  proc-macro 并导致 cargo 失败），整棵 CMake 树固定 `MultiThreaded` 以保持一致，这也是
  app 得以保持为不依赖 VC++ Redistributable 的单个 exe 的原因。Rust 不提供 debug CRT 的
  构建，因此 Debug 配置同样对齐：`_ITERATOR_DEBUG_LEVEL=0`、`/U_DEBUG`、移除 `/RTC1`，
  因为 release CRT 不含 `_CrtDbgReport`，也不支持 run-time check。任何不一致都会导致
  大量 LNK2038 错误。
- **passcode 是自助准入方式，approval 是备用方式。** 填入的码始终会被验证；没有码则由
  人进行判断。passcode 不会以任何攻击者可获取的形式经过网络。
- **VT emulator 由本项目实现。** 没有任何平台自带的 terminal 控件能够同时在五个 client
  上使用并具备合适的许可证；自行实现使 terminal 行为可以离线测试，并在各平台保持一致。
- **host 侧的 shell mirror 自首字节起即开始更新。** PTY 的输出是具破坏性的单消费者
  stream：已读取并发送给 viewer 的字节无法重放。因此 *Stop & attach* 打开的字符网格必须
  在字节经过时同步构建，而非在按下按钮时构建。远端 viewer 处于连接状态期间，mirror 自身
  对 terminal query 的响应会被丢弃：viewer 的屏幕已经作出响应，shell 不应收到两个响应。
- **仅使用一个 port。** beacon、屏幕与 terminal 共用一个 listener；connection 与 stream
  的多路复用由 QUIC 负责。此前的第二个 port 仅因 QUIC 之前的屏幕通路独占 socket 而存在。
- **一个 `HostLink` 取代此前的四套 handshake。** dial、trust 检查、auth 与 recovery 此前
  在 client 侧被实现了四次：source 查询、viewer、file sender，以及运行在独立
  `QuicEndpoint` 上的 terminal。这导致文件发送部分比 viewer 晚三个修复才获知 host key
  已变更。现在 `HostLink` 是 client 侧唯一执行 dial 或 authenticate 的代码；service 打
  开自己的 `Chan`，获得独立的 inbox 队列，并在自身 thread 上处理。terminal 的 backoff
  重连已移入 link，使所有需要 recovery 的界面都继承该行为，trust 规则也集中于一处：变更
  过的 key 使 link 保持在 `Deciding` 状态直至用户作出回应（仅 source 查询直接通过，
  `trustGate=false` 且不记录任何内容，因为其调用方没有可展示的对话框），并且只有 host
  以密码学方式证明过的 passcode 才会自动固定一个 key。
- **`HostLink` 通过 `Send` 发送，而非 `SendMessage`。** 在 Windows 上，platform 层背后
  的 OS 头文件将 `SendMessage` 定义为 `SendMessageA` 的宏，而在 `HostLink.cpp` 中这些
  头文件位于类声明之后、方法定义之前，导致 MSVC 要求为一个没有任何头文件声明过的
  `SendMessageA` 成员提供定义。Win32 的 API 名称（`SendMessage`、`PostMessage`、
  `CreateWindow`、`GetObject` 等）在任何 OS 头文件可达的 translation unit 中都不适合作
  为方法名；正确的处理方式是改名，而非 `#undef`。
- **portal 的 ScreenCast session 与一条 D-Bus 连接共存亡。** GLib 以弱引用缓存共享的
  session bus，因此对最后一个句柄执行 `g_object_unref` 会直接销毁该连接。随后
  `xdg-desktop-portal` 释放 session，compositor 销毁 PipeWire 节点，portal 刚刚提供的
  节点 id 不再指向任何对象，stream 进入 `paused` 并以 *no target node available* 失败。
  因此 `PortalScreenCast` 在 session 打开期间自行持有 `GDBusConnection`，而不是每次调用
  临时借用。桌面 app 长期掩盖了该问题，因为 GTK 在整个进程生命周期内持有 session bus 的
  引用；而 `deskhub-cli` 不 link GTK，因而没有该引用。
- **所有图标均由同一来源派生，且仅部分为圆角。** `make icons` 从唯一的母版
  `assets/icon_1024.png` 重新生成整套图标。macOS、iOS、Play Store 的商店展示以及
  Android 的 adaptive-icon 流程都会按各自的形状对图形进行遮罩，因此这些资源保持为满幅
  方形；Windows、Linux 以及 API 26 之前的 Android launcher 直接显示所提供的图形，因此
  其图标已内置圆角与透明部分，否则该 app 会在一组圆角图标中显示为实心方块。
  `scripts/make-icons.py` 刻意仅使用标准库，因为 bootstrap 不安装任何图像处理工具。
- **桌面 client 可同时持有多个 host，手机端仅持有一个。** Windows、Linux 与 macOS 上的
  connect 页不保存任何连接状态。每个应答的 host 会获得一个连接窗口 ——
  `client/windows/win32/MainFrame.cpp` 中的 `ConnectionFrame`、
  `client/linux/gtk/MainWindow.cpp` 中的 `ConnectionWindow`、
  `client/macos/app/swift/App.swift` 中名为 `connection` 的 `WindowGroup` —— 由其持有该
  host 的地址、passcode、capability、source 列表与 control 选项，从而使 connect 页可以
  继续连接下一个 host。主窗口仅保存已打开窗口的列表，用于在同一 host 被再次连接时将对应
  窗口置前、将每次 status probe 分发给地址匹配的窗口，以及在退出时关闭全部窗口。Android
  与 iOS 有意保持单连接模型：手机屏幕没有容纳第二个面板的空间，而其打开的 session 本身
  即为全屏。`ui::SameDeviceAddr` 是各处「同一个 host」的统一定义 —— 见下一条。
- **同一个 host 有两种地址写法，但只有一种比较方式。** `ScanAddressText` 在 port 为默认
  值时将其省略，因此 scan 结果行显示为 `192.168.1.60`，而用户输入并连接使用的地址为
  `192.168.1.60:47777`。将两者作为字符串比较会在无任何提示的情况下失败，而每一处这样做
  的位置都因此失去了某项功能：已连接的面板找不到匹配的设备行，因而不显示 ping；
  `PasscodeForDevice` 无法找到从 scan 列表中选中的 host 所保存的码。因此地址的相等判断
  必须经由 `ui::NormalizedDeviceAddr` 与 `ui::SameDeviceAddr`（`core/ui/Strings.h`），
  并以 `dh_same_device_addr` 提供给 Swift 与 Kotlin 的 client。不要使用 `==` 比较两个
  设备地址。

- **刚打开的 decoder 尚未持有参考帧。** `ScreenViewer` 在 surface 变化时重建 decoder，
  而 iOS 的 app 在离开屏幕时交回 surface，锁屏即会触发该情形。reassembler 对此并不知情：
  它继续投递原有的 P-frame，新建的 decoder 没有可供预测的数据，而 host 仅在收到请求时才
  发送 IDR，因此画面在该 session 的其余时间内保持黑屏。keyframe 请求此前在*旧* decoder
  被销毁时发出，而那恰是没有 surface 可供绘制的时刻：IDR 到达后被 decode 循环因缺少
  surface 而丢弃，`CancelKeyframeRequest` 同时清除了待处理的请求。现在 `EnsureDecoder`
  为其打开的每个 decoder 设置该标志，使 keyframe 在已具备绘制目标时才被请求。
  `MediaCodecDecoder` 在另一侧存在对应的问题：它在收到第一帧时即锁定 `sentCsd_`，即使该
  帧并不携带 parameter set，导致随后 keyframe 的 SPS/PPS 被当作普通数据排入队列，从未
  用于配置 codec；现在它会等待确实携带这些数据的帧。打开 decoder 的组件负责请求
  keyframe。

- **曾进入后台的 `AVSampleBufferDisplayLayer` 会无提示地丢弃 frame。** app 离开屏幕时
  iOS 会停止该 layer 的解码并设置 `requiresFlushToResumeDecoding`；在调用 `flush` 之前，
  每一次 `enqueueSampleBuffer` 都会被接受并丢弃。没有其他迹象表明这一点：`status` 不是
  `failed`，`isReadyForMoreMediaData` 仍为 true，渲染器也不报错，因此 viewer 在计数上
  表现正常，画面却为黑屏。`VtDecoder` 现在在 layer 上打开时检查该标志，并在每帧之前再次
  检查，执行 flush，并使该帧失败，以便 keyframe 请求随之发出。

- **QUIC service 循环发起的每个回调都可能删除其对应的 connection。** `Service()` 遍历
  connection id 的快照并逐个重新查找，因为 `cb_.onConnected`、`cb_.onStream` 与
  `cb_.onDatagram` 都会执行应用代码，而应用代码可能关闭某个 peer 并将其从
  `connections_` 中移除。`DrainStreams` 在其发起的每个回调之后重新检查 —— 名为
  `listStillIntact` 的 guard 正是为此而设 —— 但它仅从自身返回，因此 `Service()` 会继续
  执行 `DrainDatagrams(id, entry)`，此时 `entry` 已被移除并释放，而该函数的第一步操作即
  是将 `entry.conn` 传给 `quiche_conn_dgram_recv`。`Lookup(id) != &entry` 的检查位于两次
  drain 之后，即晚了一步。在 Windows CI 上，该问题表现为约每三次运行有一次以
  `0xc0000409` 或 `0xc0000374` 终止，长期未能定位的原因是 fastfail 不会到达
  `tests/integration/TestMain.cpp` 中的 `SetUnhandledExceptionFilter`，因此失败的运行
  只留下一个 exit code。此外，为定位该问题而设置的两个 job 均无法发现它：page heap 无法
  发现，因为被释放的内存块属于 quiche 自身，而 corruption 是已销毁的 connection 随后写入
  的内容；Rust-checks 构建同样无法发现，因为 quiche 内部并无错误。最终确定出错 frame 的
  是 Windows 的 ASan job。应在每次可能执行回调的调用之后重新校验 entry，而不是仅在代码块
  末尾校验一次。

- **liveness watchdog 只有在自身循环运行时才能衡量对端。** viewer 的五秒 pong 窗口按挂
  钟时间计算，因此本侧的任何停顿都会被判定为 host 停止应答。在 Windows 的 ASan CI job
  上，一次 32 MB 的上传与运行中的 stream 并行，使整个进程冻结 3.7 秒：标记为
  `t=07:46:58` 与 `t=07:47:00` 的日志行均在 07:47:01 输出，四个 QUIC endpoint 在该时刻
  各自报告了数秒的 poll 间隔，而 `HostLink` 判定一条完全正常的 link 已丢失。随后的重连
  将 client 切换到新的源 port，host 侧原有的 connection 因 30 秒 idle timeout 关闭，并
  中断了传输中的 batch（`transfer aborted ... link-lost`），
  `TestInputStaysLiveDuringABigTransfer` 因此耗尽其 120 秒的期限。现在
  `LinkPulse::Tick` 在 `PumpReady` 每轮执行一次，并扣除某一轮中超出 `kLinkWatchStepUs`
  的全部时间：只有在本侧确实处于监测状态时，静默时间才被计入。任何以本地时钟衡量远端
  状态的 watchdog，都必须扣除自身未进行观测的时间，否则它首先检测到的将是本机的状况。

- **存续时间超过其 connection 的传输必须被明确告知。** `FileSender` 仅在收到 ack、
  cancel 或 `LinkLost()` 时离开 `Sending` 状态，而 `FileUpload::Pump` 将被拒绝的发送
  视为 backpressure 而非失败。`ScreenViewer` 将 `LinkLost()` 接入了 `onStreamBroken`
  （该事件在 connection 仍然存续而 stream 被 reset 时触发）以及 session 的结束，但未接入
  `HostLink` 自身的 `onLinkLost`。因此传输中途的一次重连会使 `uploading()` 保持为 true，
  而对端已没有任何组件能够作出响应：host 已中止该 batch，新 connection 上的 receiver
  也从未收到该 offer。现在 viewer 在 `onLinkLost` 时以 `TransferReason::LinkLost` 判定
  上传失败。跨越重连继续传输需要在新的 connection 上重放 offer；在该功能实现之前，明确
  结束传输优于保留一个不再变化的进度条。

- **host 放开的 socket 仍被它启动的每个 shell 占着**：`Pty::Start` 使用 `forkpty`，
  子进程因此继承所有已打开的描述符，而 `ChildSetup` 直接 exec shell，一个也没关闭。
  会话的 UDP socket 就这样被带了过去。terminal host 停止时，`Pty::Impl::Shutdown`
  发出 `SIGHUP` 并以 `WNOHANG` 回收 —— 它不等待 —— 所以 shell 多久才退出，端口就被
  占用多久，即使 Deskhub 已经关闭了自己的描述符。在 ASan 下，platform 测试套件在
  `bind(127.0.0.1:47793)` 上以 `EADDRINUSE` 失败：上一个测试的 shell 还没退出。
  `UdpSocket::Open` 现在设置 `FD_CLOEXEC`，shell 一 exec 描述符就消失，端口只属于 host。
  任何会 fork 用户 shell 的进程中的长命描述符都需要这一点；仅在父进程关闭是不够的。
