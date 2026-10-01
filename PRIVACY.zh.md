[English](PRIVACY.md) · [Tiếng Việt](PRIVACY.vi.md) · **中文** · [日本語](PRIVACY.ja.md)

# Deskhub 隐私政策

_生效日期：2026 年 10 月 1 日 —— 版本 2.12_

> 本文件译自 [`PRIVACY.md`](PRIVACY.md)。如有出入，以英文版为准。

## 1. 引言

本隐私政策说明 **Deskhub**（以下称「本 app」或「我们」）在你使用 Deskhub 的移动应用
（iOS、Android）以及 Windows、macOS 和 Linux 上的 Deskhub 桌面应用（合称「本软件」）时
如何处理信息。

Deskhub 是一款 remote desktop 应用：它将你某台电脑的屏幕 stream 到你的另一台设备，并
允许你通过 mouse、keyboard 与触摸 input 操作该电脑。

本软件由一名个人开发者开发并发布：

- **开发者：** Manh Pham
- **联系方式：** manhpv151090@gmail.com
- **项目页面：** https://github.com/manhpham90vn/Deskhub

## 2. 概要

**开发者不会通过 Deskhub 接收或存储你的 session 内容与使用数据。** app 仍会在你的
设备上处理和保存部分信息，详情见下文。Deskhub 没有用户账号、开发者运营的服务器、
analytics、crash reporting、广告，也未嵌入收集数据的第三方 SDK。

## 3. 本软件处理的信息

为实现功能，本软件需要处理部分数据，且**完全在你自己的设备上及设备之间**进行处理。其中
任何一项都不会被传送给开发者或任何第三方。

| 数据 | 用途 | 传输与存放位置 | 保留期限 |
|---|---|---|---|
| 被共享电脑的屏幕内容（video frame） | 在你的另一台设备上显示该屏幕 | 在你的两台设备之间直接发送，传输中 encrypt（QUIC/TLS） | 不存储；仅在 session 期间存在于内存中 |
| 被共享电脑正在播放的声音（仅当该电脑共享声音且 viewer 提出请求时） | 使观看者能够听到该电脑的声音 | 在你的两台设备之间直接发送，传输中 encrypt（QUIC/TLS），以压缩音频形式传输 | 不存储；仅在 session 期间存在于内存中 |
| Mouse、keyboard 与触摸 input | 从你的另一台设备操作被共享的电脑 | 由观看设备直接发送至被共享的电脑，传输中 encrypt（QUIC/TLS） | 不存储，也从不写入日志；inject 之后即丢弃 |
| 本设备的 key，即首次运行时创建的一把私钥 | 在两个方向上证明本设备的身份：共享时向连接进来的设备证明，connect 时向所连接的 host 证明；对用户呈现为一个 fingerprint（`SHA256:…`） | 写入 app 自身文件夹中的 `host_key.pem`。private key 从不发送给其他设备：共享时，向连接进来的设备出示的是由该 key 构建的 certificate；connect 时仅发送 public key 和签名。certificate 不会被保留 —— TLS 库只能从文件加载它，因此它会被短暂写入同一文件夹中的 `transport_cert.<random>.pem`，仅你可读，并在加载后删除；因 crash 而遗留的文件会在本设备下次开始共享时删除。它只包含公开的 certificate —— private key 从 `host_key.pem` 读取。在 iOS 上，该文件夹被排除在 iCloud 与电脑备份之外；在 Android 上，app 的备份已关闭；在 Windows、macOS 与 Linux 上，复制你主目录的备份工具会一并复制该 key 文件。*Copy public key* 会将 public key 放入你的 clipboard，并以本设备名称作为 label，供你交给 host 的所有者。早期版本的 `host_cert.pem` 与 `client_key*.pem` 文件不再被读取 | 保留至你删除该文件为止；不会被自动替换。删除后本设备将获得新的身份：此前允许旧身份的 host 需要重新允许它，此前信任它的设备会将其视为一个新的 host |
| 受信任的 host（固定的 key fingerprint、名称、本设备首次与最后一次连上它的时间、它最后一次应答的地址与 port） | 识别本设备已信任的 host，无论它出现在哪个地址，并在曾属于受信任 host 的地址上出现另一把 key 应答时发出警告 | 写入同一文件夹中的 `known_hosts`；不会被传输 | 保留至你移除 host 或删除文件；地址与最后一次见到的时间在每次连接时更新 |
| 允许 connect 到本 host 的 client public key，各带一个 label | 只允许能证明持有对应 private key 的 client 进入 | 写入同一文件夹中的 `authorized_keys`，每行一个 key 及其 label；不记录任何时间。不会被传输。key 会在你粘贴它、你批准该设备的 connection request，或该设备扫描本 host 的 QR code 时加入 —— 后两种情况下 label 为该设备发送的名称 | 保留至你移除这些 key 或删除文件；没有该文件时任何人都无法 connect |
| Connection request —— 每台尚未获允许却尝试 connect 到本 host 的设备的名称、public key、地址与时间 | 让本 host 的所有者看到谁在请求，并以 *Approve* 或 *Deny* 作出决定 | 请求方设备通过 encrypt 连接发送其名称与 public key，并用对应的 private key 签名；只有在该签名验证通过之后，本 host 才将其连同所见地址与时间写入同一文件夹中的 `access_requests`。不会传输到这两台设备之外；仅显示在本 host 自己的屏幕上 | 最多 16 条。每条在 10 分钟后删除，或在你按下 *Approve*（将该 key 移入 `authorized_keys`）或 *Deny* 时立即删除 |
| QR pairing token —— 本 host 在展示其 QR code 期间签发的一次性随机码 | 让扫描该码的那台设备无需任何进一步步骤即可接入 | 写入同一文件夹中的 `pairing_tokens`，并附每个 token 的过期时间。token 随你展示的 QR code 与链接传播 —— 任何能看到该屏幕的人都能读到它 —— 并由扫描它的设备通过 encrypt 连接发送一次。QR code 中还包含本设备的 network 地址与 port、其 key fingerprint 及其设备名 | 在隐藏该码、停止共享、token 被使用时删除，或 5 分钟后删除 |
| 你扫描 QR code 期间的摄像头画面（仅 Android 与 iOS） | 从 host 的屏幕上读取其 QR code | 仅在设备上处理，用于查找并解码该码；不保存、不传输，也不向任何人显示 | 从不保存；每一帧检查完毕即丢弃 |
| 你输入的 IPv4 地址（以及你给出的 port）—— Deskhub 不解析 host 名称 | 连接到另一台机器 | 仅保留在你输入它的设备上 | 本地保留至你修改为止 |
| 最近连接的 10 个 host —— 地址、最近一次连接的时间以及 host 自报的名称 | 填充 *Recent devices* 列表 | 写入你设备上 app 自身文件夹中的 `recent-hosts.txt`，每个 host 一行：Windows 为 `%USERPROFILE%\.deskhub`，macOS 与 Linux 为 `~/.deskhub`，iOS 与 Android 为 app 沙箱；从不传输。早期版本的 `recent-devices.txt` 文件会被删除，而非转换 | 保留至你连接了 10 个更新的 host，或删除该文件为止 |
| 你的共享设置（frame rate、bitrate、分辨率上限、port、network 地址、viewer 操作权限、本设备是否操作它所观看的机器，以及 clipboard sync、共享声音、播放所观看机器的声音、keep awake、随 OS 启动、自动共享与后台模式等开关） | 在下次打开 app 时恢复你的 settings | 写入同一文件夹中的 `ui-settings.txt`；在 iOS 上位于 app 与 broadcast extension 共享的 app group 容器中 | 保留至你修改或删除该文件为止 |
| *Start Deskhub when you log in* 创建的启动项（仅桌面端） | 在你登录时启动 Deskhub | 由操作系统自身的机制创建，位于 app 文件夹之外：Windows 上为名为 *Deskhub* 的 scheduled task（在登录时以最高权限运行），Linux 上为 `~/.config/autostart/deskhub.desktop`，macOS 上为一个登录项。它只包含 app 的路径；不会传输任何内容 | 在你关闭该设置时移除 |
| 你在 Linux 桌面的屏幕共享对话框中选定 display 后，桌面签发的屏幕 permission token（仅 Linux） | 使后续共享复用该选择，从而对话框仅在首次出现 | 写入同一文件夹中的 `portal-restore-token.txt`；该 token 仅对本机上你自己的桌面 session 有意义，不会被传输 | 每次共享后被替换；在你选择 *Choose screens again* 或删除该文件时移除 |
| Clipboard 文本（仅当 clipboard sync 开关开启且存在运行中的 session 时） | 使在一台设备上复制的文本可在其他设备上粘贴 | 在你的设备之间直接发送，传输中 encrypt（QUIC/TLS），每次复制上限 32 KiB；仅限纯文本，不包含图片或文件 | Deskhub 不存储；仅存在于各设备自身的系统 clipboard 中 |
| 当前是否有 broadcast 在运行、已连接的 viewer 数量及其设备名、broadcast extension 自身的内存占用（MB），以及最近一次启动错误的文本（仅 iOS） | 使 app 的共享界面能够显示 broadcast extension 的状态。iOS 将其作为独立 process 运行，并在内存占用过高时终止它 | 写入同一 app group 容器中的 `broadcast-status.txt` | 在 broadcast 结束时删除 |
| Settings → General → *Device name* 中的设备名。留空时使用本电脑或设备自身的名称（Windows 与 Linux 为 hostname，macOS 为电脑名称，iOS 为设备名称，Android 为机型） | 为本设备命名：共享时显示给 viewer，显示给 connect 到本设备的已允许 client，在你所连接的 host 上显示于该设备地址旁，显示在 host 为本设备记录的 connection request 中，并用作你所复制 public key 的 label | 保存在同一文件夹的 `ui-settings.txt` 中，并在 connect 时发送给 host。该数据在传输中 encrypt，但会显示在 host 的屏幕上并写入其日志，因此除非你自行设置名称，默认名称将被发送。尚未允许本设备的 host 会在其 connection request 列表中显示该名称，并保留最多 10 分钟。本设备共享时，还会将该名称发送给每个已用允许的 key 完成认证的 client —— 认证之前绝不发送 —— 该 client 会将其保存在自己的最近列表中，该名称也会写入本设备展示的 QR code 中。使用该 QR code 或链接连接的设备，会把该名称作为 server name 放入 TLS handshake 的第一个 packet 中，而该 packet 未经 encrypt —— 此时任何监视 network 的人都能读到它。它还会嵌入你复制的 public key 中，并且是 host 在批准本设备或通过 QR code 准入本设备时保存到其 `authorized_keys` 中的 label | 保留至你修改或删除该文件为止。清空该字段是回退到默认值，而非移除名称 |
| 你选择发送给已连接电脑的文件（仅在你亲自选定文件并按下 Send 时） | 将文件从你的一台设备传送到另一台 | 在你的两台设备之间直接发送，传输中 encrypt（QUIC/TLS）；在手机或平板上，发送前会在 app 自身的 cache 中准备一份副本以供读取 | 文件的存放位置取决于接收方。电脑会将其写入为此选定的文件夹，未另行选择时为该用户主目录下的 `Deskhub`，并保留至该用户删除为止。手机与平板没有对应的文件夹：照片和视频会加入该设备的相册（Android 上为 `Pictures/Deskhub` 与 `Movies/Deskhub`），其他文件放置在系统文件浏览器可见的位置，即 iOS 上 app 的 Documents 文件夹与 Android 上的 `Download/Deskhub`，并保留至你删除为止。在 iOS 上，相册不接受的照片改存至 Documents。经由媒体库的存放方式需要 Android 10：在 Android 9 及更早版本上，到达的文件保留在设备上 Deskhub 自身的文件夹中，不会出现在相册或 Downloads 中。发送端手机或平板上的临时副本会在发送窗口关闭时删除 |
| 每个被提出文件的名称、大小与 checksum，以及发送设备的名称、地址和 key fingerprint | 使接收电脑能够显示正在到达的内容、拒收无法存储的内容，并使其所有者了解发送来源 | 在你的两台设备之间发送，传输中 encrypt；接收电脑将该提出、其判定与结果写入自身的 session log | 保留在该电脑的 log 文件中，直至你删除 |
| 电脑用于存放接收文件的文件夹 | 在下次打开 app 时恢复该选择 | 写入 app 自身文件夹中的 `ui-settings.txt`；不会被传输 | 保留至你修改或删除该文件为止 |
| 连接统计（bitrate、丢包、latency） | 调整 stream 的 quality，并显示在状态栏 | 仅在你的两台设备之间交换。在 Windows、macOS 与 Linux 上，它们还会以定期的 `[DIAG]` 摘要写入下文所述的本地诊断日志 | session 结束时丢弃，日志中保存的部分除外 |
| 诊断日志（Windows、macOS 与 Linux）—— 连接统计、peer 的地址与 port、peer 发送的设备名、key fingerprint、每个发送或接收的文件的名称、大小、结果及其存放的文件夹，以及错误信息。从不包含屏幕内容、按下的按键、指针移动、clipboard 文本或 terminal 输出 | 让你或你选择发送给的人找出问题所在 | 以纯文本写入 app 自身文件夹中，每次运行一个文件 `deskhub-<date>-<time>-<pid>.log`，仅你可读（在 Windows 上通过该文件继承的文件夹权限实现）；从不传输。在 Android 与 iOS 上，同样的内容只写入系统日志（logcat、Xcode console），不写入文件 | 较旧的日志会被自动删除，只保留最新的十个；你可以随时删除该文件夹或这些文件 |

### 3.1 设计上的 peer-to-peer 架构

所有通信均**直接在你自己的两台设备之间**进行，通过：

- 你的本地 network（Wi-Fi 或 LAN），或
- 由**你**运营或订阅的 VPN（例如 Tailscale），前提是你选择以此进行跨 Internet 的访问。

我们不运营 relay 服务器、signaling 服务器或任何后端。本软件在技术上不具备将数据发送给
开发者的手段。

### 3.2 本软件**不**处理的数据

除上文所述的设备名外，Deskhub 不要求你提供姓名、电子邮件地址、电话号码、通讯录、
位置或广告标识符。app 不使用 microphone。只有在你于手机或平板上点按 *Scan QR code*
之后扫描 host 的 QR code 期间，app 才会使用摄像头：画面在设备上解码以查找该码，
既不保存也不发送到任何地方。只有在你选择照片和文件发送、
其他设备向你发送这些内容，或它们出现在你选择 Share 的屏幕上时，app 才会接触它们。
上文说明了收到的文件保存在哪里、保留多久。

### 3.3 共享手机或平板的屏幕

Android 与 iOS 设备既可共享自身屏幕，也可观看其他机器。该 stream 为 **view-only**：
进入的 mouse 与 keyboard packet 会被丢弃，因为两个操作系统都不允许普通 app 操作设备。
被 capture 的是**整块屏幕**，包括共享期间出现的全部内容：通知、其他 app、银行 app，以及
你输入的密码。在 Android 上，系统会为每次共享弹出录屏同意对话框，并在共享期间显示常驻
通知；在 iOS 上，系统的 broadcast 指示持续可见。两者均为操作系统自身的机制，且都可用于
随时停止共享。与桌面端相同，video 直接发送至你的另一台设备，不会被存储，也不会发送给
我们。

### 3.4 屏幕共享与远程操作的范围

共享 stream 的是**所选的整块 display**：该显示器上出现的全部内容对已连接的 viewer 可
见，包括通知、弹窗，以及你在共享期间打开的任何窗口。（共享单个应用窗口的功能已于
2026-07-27 移除；本软件目前仅共享整块 display。）当你允许远程操作时，viewer 的 input
会如同其本人坐在 PC 前一样被 inject，并可作用于**共享 display 上可见的任何应用**，不再
限于单个窗口。在任何 host 上，你都可以在 Settings 中完全关闭远程操作，此时共享变为
view-only，到达的 input 将被丢弃而非 inject。在允许操作期间，两项安全机制始终生效：
若 PC 前的用户操作了真实的 mouse 或 keyboard，remote input 将暂停（host 优先）；远端
按住的任何按键，都会在连接结束或 viewer 切换时自动释放。最多五个 viewer 可同时观看同一
台 PC，但任一时刻只有其中一个操作 mouse 与 keyboard。

## 4. App 申请的 permission

| 平台 | Permission | 用途 |
|---|---|---|
| iOS | Local Network | iOS 要求具备该权限，才能与同一 network 上的 PC 收发数据。仅用于 stream 的 session。 |
| iOS | 录屏（broadcast） | 仅在你通过系统 broadcast 选择器开始共享本设备屏幕时使用。iOS 每次都会询问，并在整个过程中显示录制指示。 |
| Android | `INTERNET`、网络状态 | 建立到 PC 的 UDP 连接所必需。仅用于 stream 的 session。 |
| Android | 屏幕 capture 同意（`MediaProjection`） | 仅在你开始共享本设备屏幕时使用。Android 每次都会询问，且无法保存该回答。 |
| Android | `FOREGROUND_SERVICE`、`FOREGROUND_SERVICE_MEDIA_PROJECTION` | 在 app 进入后台或屏幕熄灭时维持共享。这是 Android 对屏幕 capture 的要求。 |
| Android | `RECORD_AUDIO` | Android 将 playback-capture API 置于该 permission 之后，而 playback，即设备自身正在播放的内容，是 Deskhub 唯一 capture 的对象。该 permission 在共享开始时申请；若被拒绝，共享将继续进行但没有声音。Deskhub 不会打开 microphone。 |
| Android | `POST_NOTIFICATIONS` | 显示 Android 在屏幕共享期间要求的常驻通知，并在其他设备发送文件时说明到达的内容，以及在有设备请求连接本机时提醒你（一条 *Connection request* 通知，附该设备的名称与地址）。不发送其他通知。 |
| iOS | 相册，仅添加 | 在他人发送的照片或视频首次到达本设备时申请，用于将其加入 Photos app。Deskhub 只能添加条目，不会读取、修改或删除相册中已有的内容。若被拒绝，文件改存至 app 的 Documents 文件夹。 |
| iOS | 通知 | 在其他设备发送文件时说明到达的内容，以及在有设备请求连接本机时提醒你（一条 *Connection request* 通知，附该设备的名称与地址）。不发送其他通知。 |
| macOS | 通知 | 在首次有设备请求连接本机时申请，用于显示附该设备名称与地址的 *Connection request* 通知。不通知其他任何内容。 |
| Android | `CAMERA` | 仅在你于 Client 页点按 *Scan QR code* 时申请，用于从 host 的屏幕上读取其 QR code。画面在设备上解码，从不保存或发送。若被拒绝，你可以改为将 host 的链接粘贴到地址栏中。 |
| iOS | 摄像头 | 仅在你于 Client 页点按 *Scan QR code* 时申请，用途与限制同 Android。若被拒绝，你可以改为粘贴 host 的链接。 |
| macOS | 屏幕录制（Screen Recording） | 本 Mac 共享时用于 capture 屏幕（及其播放的声音）。在 System Settings → Privacy & Security 中授予一次即可。 |
| macOS | 辅助功能（Accessibility） | 本 Mac 在允许操作的情况下共享时，用于 inject viewer 的 mouse 与 keyboard input，并察觉 Mac 前的用户何时操作了它（host 优先）。 |
| macOS | Local Network | Deskhub 首次访问你 network 上的另一台设备时由 macOS 询问。仅用于 session。 |
| Windows | 管理员权限（UAC） | app 以提权方式运行，以便向提权窗口 inject input。每次启动时 UAC 都会询问，唯一的例外是 *Start Deskhub when you log in* 通过其 scheduled task 启动它。 |
| Windows | Windows Firewall | 共享时，提权运行的 app 会为自己添加或刷新一条名为 *Deskhub (host)* 的入站 UDP 规则，覆盖所有 network profile。它不修改其他任何规则；你为 Deskhub 创建的 block 规则会保持不变，并使它继续无法被访问。 |
| Linux | 屏幕共享同意（桌面 portal） | 桌面自身的对话框会在首次共享时询问要共享哪些屏幕；该回答通过第 3 节所述的 token 被记住。 |
| Linux | 虚拟 input 设备（`/dev/uinput`） | 用于 inject viewer 的 input。`.deb` 与 `.rpm` 会安装一条 udev 规则，将其开放给 `input` 组以及在座席上登录的用户。 |
| Linux | 读取 `/dev/input/event*` | 仅用于 host 优先，即察觉机器前的用户正在输入或移动 mouse。需由你自行将用户加入 `input` 组；安装包不会这样做，缺少该权限时 Deskhub 仍可工作，但永远不会暂停 remote input。Deskhub 只读取是否发生了 input，且不记录其中任何内容。 |

在桌面端，共享声音不需要单独的 permission：本软件 capture 的是电脑自身正在播放的内容，
而非 microphone。Android 是例外，且仅在名称层面如此：其 playback-capture API 位于
`RECORD_AUDIO` 之后，因此需要共享声音的 Android 设备必须持有系统标记为 *Microphone*
的 permission。Deskhub 不将该 permission 用于其他用途，不录制 microphone，不提供双向
音频，也不在其他任何平台申请 microphone permission。

App 不申请上表所列之外的任何 permission。若将来的版本需要新的 permission，该 permission
将在相应场景中申请，本政策也会随之更新。

## 5. Analytics、广告与第三方

- **Analytics 与 telemetry：** 无。
- **Crash reporting：** 本软件未内置。诊断日志仅存在于你自己的机器上，位于 app 的
  控制台输出中；在 Windows、macOS 与 Linux 上，还位于 `~/.deskhub/`（Windows 上为
  `%USERPROFILE%\.deskhub`）下的纯文本文件中，仅你可读，较旧的日志会被自动删除。其内容见
  第 3 节。这些日志不会被上传；只有在你自行复制并发送时才会离开你的设备，而且你可以随时
  删除该文件夹。
- **广告：** 无。
- **第三方 SDK：** 没有收集数据的 SDK。除自身源码（在项目页面公开）与操作系统框架外，
  本软件还使用了若干开源库构建，它们完全在你的设备上运行，不会自行向任何地方发送数据：
  quiche 与 BoringSSL（encrypt 与传输）、Opus（声音）、FFmpeg（Linux 上的 video），以及
  Android 上的 AndroidX、CameraX 与 ZXing（界面、摄像头与 QR 解码）。它们连同各自的许可证
  列于 [`THIRD_PARTY_NOTICES.zh.md`](THIRD_PARTY_NOTICES.zh.md)。
- **App 分发：** iOS app 通过 Apple 的 TestFlight 分发，Android app 通过 Google Play 分发。
  Apple 与 Google 可能依据各自的隐私政策收集安装、使用与 crash 数据；该收集行为不在我们的
  掌控范围内。通过 TestFlight 与 Google Play Console，它们可能向开发者展示来自单台设备的
  crash 报告（需设备所有者同意共享）—— 附设备型号、操作系统版本与 app 版本 —— 在
  TestFlight 上还包括测试者自愿发送的反馈，以及聚合统计。其中不包含你的 session 内容。
- **Tailscale 及其他 VPN：** 若你选择通过 VPN 连接，相关数据将依据该服务商的隐私政策
  处理。Deskhub 既不要求也不捆绑任何 VPN。

## 6. 安全

- Stream 的流量保留在你自己的 network 或你自己的 VPN 隧道内。使用 Tailscale 等 VPN
  时，设备之间的数据由该 VPN（WireGuard）进行端到端 encrypt。
- Deskhub 会 encrypt session 的流量：video、control、input、clipboard 与 terminal
  数据均在你的设备之间通过 QUIC/TLS 传输。client 必须使用 host 在 `authorized_keys` 中列出的 key
  对该连接的 transcript 签名，并在发送任何内容之前检查已固定的 host key。只有当 host 的
  所有者批准该设备的 connection request、向它展示 host 的 QR code，或粘贴它的 public key
  时，key 才会被列入。不会保存或传输
  任何 passcode。Deskhub 从不 scan 你的 network，也不应答任何未 encrypt 的 discovery 请求。
  设备名在传输中 encrypt，但会显示在 host 上、显示在 connection request 中，并嵌入你复制的
  public key 中，因此不应在其中填入敏感信息。
  请勿将 Deskhub 直接暴露到 Internet。完整的 threat model，包括保护范围、不受保护的
  范围以及漏洞报告方式，见
  [`SECURITY.zh.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.zh.md)。
- 早期版本遗留的数据不会被转换：旧的 `paired_devices` 列表、旧的启用标记以及旧的
  `auth_salt` 文件会被删除，旧 `ui-settings.txt` 中的 passcode 行会在首次加载该文件时被
  移除。无法读取的 `authorized_keys` 或 `known_hosts` 文件在其无法读取
  期间拒绝访问，并在下一次更改时重新写入。
- 由于我们不持有关于你的任何数据，不存在可能被攻破的开发者侧数据库。

## 7. 数据保留与删除

我们不保留任何数据，因此也没有需要我们删除的内容。所有 session 数据在 session 结束时
消失。app 中保存的地址可通过清空相应字段或卸载 app 移除。最近设备列表、已保存的
settings、key、允许的 client、受信任的 host、等待中的 connection request 以及仍有效的 QR
token 可通过删除 app 的文件夹移除（Windows 上为
`%USERPROFILE%\.deskhub`，macOS 与 Linux 上为 `~/.deskhub`），app 会在下次启动时重新
创建空的文件夹；诊断日志也位于同一文件夹中，会随之删除。在 iOS 与 Android 上，卸载 app
即可移除这些数据。*Start Deskhub when you log in* 创建的启动项位于该文件夹之外：请在删除
该文件夹或卸载之前关闭该设置，或手动移除该启动项。

其他设备发送给你的文件属于你，而不属于 app。送达之后，这些文件位于该电脑选定的文件夹
中，或位于手机与平板的相册、Documents 或 Downloads 中。卸载 Deskhub 不会影响这些文件，
需在相应位置自行删除。

## 8. 你的权利（GDPR、CCPA 及类似法规）

欧盟《通用数据保护条例》（GDPR）与《加州消费者隐私法》（CCPA）等法规赋予你对个人数据的
权利：访问、更正、删除、可携带、反对以及不受歧视。

由于 Deskhub 既不收集也不持有个人数据，不存在可据以行使上述权利的数据。若你认为我们
持有关于你的数据，请通过下方联系方式与我们联系，我们将在 30 天内答复。

我们不进行 CCPA 所定义的个人信息「出售」或「分享」。

## 9. 儿童隐私

本软件不面向儿童，并且如上所述，不从任何人收集数据，包括 13 岁以下（COPPA）与 16 岁
以下（GDPR）的儿童。

## 10. 跨境数据传输

Deskhub 不向开发者发送数据。如果通过 VPN 连接，session 数据会在你的设备之间经由
该 network 传输，详情见第 3.1 节。

## 11. 本政策的变更

若本软件的数据处理方式发生变化（例如未来版本增加可选的 crash reporting），本政策将在
该变更**发布之前**更新，并附新的生效日期与下方的变更记录。当前版本始终发布于：
https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md

| 版本 | 日期 | 变更内容 |
|---|---|---|
| 2.12 | 2026-10-01 | **更正与更严格的本地数据处理。** Windows、macOS 与 Linux 上的诊断日志文件现在仅你可读，较旧的日志会被自动删除，只保留最新的十个；Windows app 不再把 viewer 按下的按键写入日志，无论是在 remote control 中还是在 terminal 中；本政策现已列出日志包含的内容，其中不包括 viewer 按下的按键。连接统计会记录在这些日志中，而本政策的早期版本未说明这一点。connection request 只有在请求设备用其 key 签名之后才会被记录，因此它出示的 key 已得到证明。由本设备 key 构建的 TLS certificate —— 仅公开部分 —— 会被短暂写入仅你可读的文件并立即删除，因 crash 而遗留的文件会在本设备下次开始共享时删除；早期版本称其从不保存。在 iOS 上，保存本设备 private key 的文件夹现已被排除在 iCloud 与电脑备份之外。旧的 `auth_salt` 文件现在会被删除，旧 `ui-settings.txt` 中遗留的 passcode 行会在加载该文件时立即移除。本版本还更正了早期的陈述：`known_hosts` 会记录首次与最后一次连上每个 host 的时间；设置文件还保存本设备是否操作其所观看的机器以及是否播放其声音；现已列出 *Start Deskhub when you log in* 创建的启动项；iOS 的 broadcast 状态文件保存已连接 viewer 的名称；使用 QR code 或链接连接的设备会在 TLS handshake 的第一个 packet 中以未 encrypt 的形式发送 host 的名称；你输入的地址是 IPv4 地址，从不解析 host 名称；permission 表现已涵盖 macOS、Windows 与 Linux；本软件包含开源库，但没有收集数据的 SDK；iOS app 通过 TestFlight 分发，TestFlight 与 Google Play Console 一样，可向开发者展示单台设备的 crash 报告。 |
| 2.11 | 2026-09-30 | **每台设备一把 key、connection request 与 QR pairing。** 每台设备现在只有一把 key（`host_key.pem`），共享与 connect 时均以它为身份；单独的 client key（`client_key*.pem`）与保存的 certificate（`host_cert.pem`）不再存在 —— certificate 在内存中构建、从不保存，遗留文件被忽略而非转换。受信任的 host 按 key fingerprint 记忆，并附各自最后一次应答的地址，不再按地址记忆。app 文件夹中新增两个文件，均不会传输到相关两台设备之外：`access_requests` 保存每台尚未获允许却请求 connect 的设备的名称、public key、地址与时间（最多 16 条，每条在 10 分钟后或在 *Approve* / *Deny* 时删除）；`pairing_tokens` 保存 host 在共享期间可展示的 QR code 背后的一次性随机 token（在隐藏该码、被使用或 5 分钟后过期时删除）。QR code 本身包含 host 的地址、port、key fingerprint、设备名与 token，任何看到屏幕的人都能读取。设备名现在还会显示在它留下的 connection request 中，并在 host 批准它或通过 QR code 准入它时成为其 key 的 label。在 Android 与 iOS 上，摄像头仅在你扫描 QR code 期间使用，并在那一刻申请 permission；画面在设备上解码，从不保存或发送。新的 connection request 还会在本设备上触发一条系统通知，说明请求设备的名称与地址；该通知由本设备自己的通知中心显示，不会发往其他任何地方。 |
| 2.10 | 2026-09-29 | host 现在会将其设备名发送给每个已用允许的 key 完成认证的 client —— 认证之前不发送任何内容 —— client 会将该名称保存在其最近列表中。最近列表改用新文件 `recent-hosts.txt`（地址、最近一次连接的时间、host 名称；最多 10 个）。旧的 `recent-devices.txt` 会被删除而非转换。 |
| 2.9 | 2026-09-29 | **passcode 已被移除，访问方式与 SSH 相同。** 任何地方都不再保存或传输 passcode。LAN discovery 已被移除：Deskhub 从不 scan 你的 network，host 也不应答任何明文 discovery 请求。host 将其允许的 client public key 保存在 `authorized_keys` 中，各带一个 label；client 将其信任的 host 保存在 `known_hosts` 中，包括固定的 host key fingerprint、地址、名称以及所使用的 client key。在 Settings 中设置的唯一设备名会发送给你所连接的 host，并嵌入你复制的 public key 中。不为允许的 client 记录任何时间。早期版本的数据文件 —— passcode、旧的 `paired_devices` 列表、旧的启用标记 —— 会被删除而非转换。 |
| 2.8 | 2026-09-28 | Host 可将获准访问的 public key 保存到 `authorized_keys`，并在本地保留启用标记。已保存的 host 配置增加别名和选用的 client identity。旧版仅含 fingerprint 的列表只在新列表启用前使用。 |
| 2.7 | 2026-09-28 | CLI 可创建和导入额外的命名 client 签名 key，并为连接选择其中一个。每个命名 private key 单独保存在本地文件中；列出 identity 时只提供 public key 信息。 |
| 2.6 | 2026-09-28 | client 接入现在需要已授权的签名 key 和已固定的 host key。Recent devices 与 UI settings 不再保存 passcode；读取旧文件并安全重写时会删除旧字段。若重写失败，保留旧文件供下次重试。 |
| 2.5 | 2026-09-07 | 本次为更正而非行为变更，Deskhub 的行为与此前一致。本政策的早期版本称，到达 Android 手机或平板的文件会通过系统媒体库存入 `Pictures/Deskhub`、`Movies/Deskhub` 或 `Download/Deskhub`。该说明适用于 Android 10 及以上版本。Deskhub 所使用的媒体库路径需要 Android 10，因此在 Android 9 及更早版本上，到达的文件保留在设备上 app 自身的文件夹中，不会出现在相册或 Downloads 中。两种情况下，文件都只到达相关的两台设备。 |
| 2.4 | 2026-08-28 | **手机与平板现在既可接收文件也可发送文件**，其文件存放位置为新增内容。在 iOS 上，照片与视频会加入你的相册，系统会在首次时申请仅添加模式的 Photos permission；Deskhub 只能添加条目，不会读取或修改已有内容。其他文件放入 app 的 Documents 文件夹，可由 Files app 访问。在 Android 上，照片存入 `Pictures/Deskhub`，视频存入 `Movies/Deskhub`，其余文件存入 `Download/Deskhub`，均通过系统媒体库完成。两个平台都会以通知说明到达的内容。上述数据均不会到达我们。本版本还更正了此前政策中两处不准确的表述：Android 一直需要系统标记为 *Microphone* 的 permission（`RECORD_AUDIO`）来 capture 设备自身播放的内容，即版本 2.1 所述的声音共享，而 Deskhub 并不录制 microphone；此外，桌面 app 从未失去版本 2.3 中所述的 *File transfer* 选项，失去的只是其背后被保存的设置。 |
| 2.3 | 2026-08-24 | **接收文件不再是一项被保存的设置。** 存储的 *Take files viewers send* 设置已从 `ui-settings.txt` 中移除：手机与平板在 app 显示于屏幕上时即接收文件，而电脑将 *File transfer* 作为共享内容之一提供，每次默认勾选且不予保存，因此电脑仍仅在共享期间接收文件。到达文件的处理方式未变：仍需发送方已 pair 且已被接受，仍存放于该机器为接收文件指定的位置，仍不会覆盖已有文件，并仍会连同发送设备的名称、地址与 key fingerprint 记入本地日志。屏幕共享仍是通过专用按钮触发的主动操作，正在共享屏幕的电脑在此期间同时继续接收文件。 |
| 2.2 | 2026-08-21 | Deskhub 支持在你自己的设备之间**发送文件**。正在共享屏幕的电脑可同时提供文件接收，任何与其连接的设备都可选择并发送文件。在 Android 与 iOS 上，文件来自系统相册选择器或系统文件浏览器，发送期间会在 app 自身的 cache 中准备一份副本，发送完成后删除。文件在你的两台设备之间直接传输，使用与画面相同的 encrypt transport，不会发送给我们，也不经过我们的任何服务器。接收电脑将其写入自身选定的文件夹，未另行选择时为该用户主目录下的 `Deskhub`，并与其他设置一同保存在 `ui-settings.txt` 中；不会覆盖已有文件，并将每次提出、判定与结果，连同发送设备的名称、地址与 key fingerprint 写入本地 session log。除非共享方勾选，否则文件接收处于关闭状态；手机与平板仅发送文件，不接收。 |
| 2.1 | 2026-08-19 | 屏幕共享可同时共享该电脑的**声音**。被 capture 的是电脑自身扬声器正在播放的混合音频，而非 microphone；Deskhub 没有双向音频，也不申请 microphone permission。音频经压缩后，通过与画面相同的 encrypt transport 直接发送给观看者，且不会被存储。只有当共享方开启 *Share this device's sound* **且** viewer 开启 *Play the sound of the device you are watching* 时，音频才会传输；关闭其中任一开关即停止传输。两个开关与其他设置一同保存在 `ui-settings.txt` 中，且默认开启。 |
| 2.0 | 2026-08-15 | Session 运行在 encrypt 的 transport（QUIC/TLS）之上，涵盖 video、input、clipboard 与 terminal 流量，机器的接入通过 pairing 完成。新增保存在你自己设备上的数据均位于 app 的文件夹内，且不会发送给我们：作为本机身份的密钥对（`host_key.pem`、`host_cert.pem`）、你 trust 过的 host 的 key（`known_hosts`）、被本 host 接受的机器（`paired_devices`，包含 key fingerprint、各机器发送的名称与时间戳），以及一个非机密的 salt（`auth_salt`）。passcode 变为可选，且不会被传输：pairing handshake 在不发送该码的前提下完成验证。 |
| 1.9 | 2026-08-14 | 在 Linux 上，桌面屏幕共享对话框中所做的屏幕选择将被保存：桌面签发的 permission token 写入 `portal-restore-token.txt`，使后续共享跳过该对话框。该 token 仅对本机上你自己的桌面 session 有效，不会被传输，每次共享后被替换，并在你选择 *Choose screens again* 或删除该文件时移除。 |
| 1.8 | 2026-08-14 | 新增 *keep awake* 开关（默认开启）：在你共享或观看期间，app 请求操作系统不要使机器进入睡眠、不要关闭 display，并在 session 结束时撤回该请求。仅保存开关状态，存于同一本地设置文件中；相关信息不会被传输，也不会修改任何系统睡眠设置。 |
| 1.7 | 2026-08-13 | Clipboard 同步在 Android 与 iOS 上同样可用，开关、32 KiB 上限与仅限纯文本的规则均与桌面端一致。操作系统另有限制：Android 设备仅在 Deskhub 处于前台时才能读取自身 clipboard，而接收到的文本随时可写入；iOS 上的 viewer 在 Deskhub 读取新复制内容时可能出现系统粘贴提示；作为 host 的 iOS 设备不参与 clipboard 同步，因为其 broadcast 运行在独立 process 中。手机与平板也获得了桌面端「选择用于共享的 network 地址」的设置，保存在同一本地设置文件中，不会发送至任何位置。除该设置外，设备上没有新增存储的内容。 |
| 1.6 | 2026-08-13 | 桌面 app 新增可选的 clipboard 同步：开关开启后，你在 session 期间复制的纯文本会在设备之间发送（当时与其余流量一样未 encrypt，上限 32 KiB），并写入另一台机器的 clipboard；Deskhub 不存储该内容。新增在本地保存的设置包括：用于共享的 network 地址、随 OS 启动、启动时自动共享、后台与 tray 模式，以及 clipboard 开关本身。开启随 OS 启动会创建对应平台的启动项（Linux 上为 autostart 文件，Windows 上为名为 *Deskhub* 的 scheduled task，macOS 上为 Login Item）；关闭则移除该项。 |
| 1.5 | 2026-08-13 | 每个 client 可通过 connect 页的 *Your name* 字段设置设备名。该名称保存在你设备上已有的 `ui-settings.txt` 设置文件中，并在 connect 时发送给 host（当时与其余流量一样未 encrypt），以便 host 在其 session 列表、status 行与日志中标识该 viewer。host 仅在你连接期间将该名称保留在内存中，不予存储。该字段预填为你的电脑或设备自身的名称，因此除非你改为自选名称，默认值将被发送。 |
| 1.4 | 2026-08-12 | broadcast extension 与 app 共享的 iOS 状态文件新增记录 extension 自身的内存占用（MB），以便共享界面显示该数值。该数值仅描述 Deskhub 的 broadcast process，保留在你设备上的 app group 容器中，并在 broadcast 结束时与状态文件的其余内容一同删除。 |
| 1.3 | 2026-08-12 | Android 与 iOS 设备可以 view-only 方式共享自身屏幕，因此手机或平板的屏幕可以 stream 到你的另一台设备。该变更新增了各 OS 所要求的屏幕 capture permission（在 Android 上还包括一个前台服务及其通知），并在 iOS 上新增 app 与其 broadcast extension 共享的 app group 容器，用于保存你的 passcode 与 port，以及 extension 在其中写入的一个短期状态文件，供 app 显示 broadcast 是否正在运行。video 仍仅在你自己的设备之间传输，且不会被存储。 |
| 1.2 | 2026-08-07 | passcode 在所有 host 上成为必填项，并在首次启动时生成而非留空，所有 client 均可输入。共享设置除 Windows 外也在 macOS 与 Linux 上保存，最近设备列表在所有平台上保存，均位于 app 自身的本地文件夹中。没有新的数据离开你的设备。 |
| 1.1 | 2026-08-05 | Windows app 开始在两次启动之间保存数据：最近 10 个连接地址、你的共享设置，以及所使用的 passcode。全部内容保留在你自己机器上的 `%USERPROFILE%\.deskhub` 中，不会被传输到任何位置。本版本同时记录了 view-only 共享与 5 个 viewer 的上限。 |
| 1.0 | 2026-07-24 | 首次发布。 |

## 12. 联系方式

关于本政策或 Deskhub 隐私方面的任何问题：

- **邮件：** manhpv151090@gmail.com
- **Issues：** https://github.com/manhpham90vn/Deskhub/issues
