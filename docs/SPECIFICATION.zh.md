[English](SPECIFICATION.md) · [Tiếng Việt](SPECIFICATION.vi.md) · **中文** · [日本語](SPECIFICATION.ja.md)

# Deskhub —— 功能规格

本文档描述 Deskhub **做什么**，以使用者所见的形式呈现。这是一份产品规格，而非设计文档：
其中不含实现细节、protocol 说明和 build 步骤。这些内容分别位于
[`INSTALL.zh.md`](INSTALL.zh.md)、[`BUILD.zh.md`](BUILD.zh.md)、
[`SECURITY.zh.md`](../SECURITY.zh.md) 以及 source 树中。

本文件是 [`SPECIFICATION.md`](SPECIFICATION.md) 的译本；若两者有出入，以英文版为准。

- **状态：** 描述当前代码的行为。
- **读者：** 需要了解本产品应具备哪些行为的人员 —— tester、reviewer、贡献者，以及商店
  文案的撰写者。

---

## 1. 产品概述

Deskhub 使一台机器能够将自身屏幕呈现给同一 network 上的其他机器，并允许这些机器操作其
mouse 和 keyboard。它是单一应用：同一个 app 既可共享屏幕，也可观看其他机器的屏幕。桌面
机器还可共享一个 **terminal**，即 host 上的真实 shell，其他机器在各自的窗口中打开它
（第 4、5 节）。

不要求 installer，无需账号与登录，没有 background service，也没有云端组件。两台机器通过
IP 地址在彼此可达的 network 上相互发现。

## 2. 术语

| 术语 | 含义 |
| --- | --- |
| **Host** | 屏幕（或 terminal）被共享的机器。 |
| **Client** / **Viewer** | 正在观看某个 host，并可能对其进行操作的机器。 |
| **Source** | host 上可供共享的一项内容：一块 display，或 terminal。一个 host 可同时共享多项。 |
| **Session** | 一个 viewer 观看一个 source。每个 source 在各自的窗口中打开。 |
| **Key** | 机器在首次运行时创建的密码学身份，向用户呈现为 fingerprint（`SHA256:…`）。 |
| **Pairing** | host 长期接受某台机器。已 pair 的机器通过 key 被识别，无需 passcode 即可 connect，直至被 forget（第 9 节）。 |
| **Passcode** | host 可选要求的 4 位数字码，未知机器在 pair 前需提供。 |

一台机器可以同时担任 host 与 client。

## 3. 各平台的角色

| 平台 | 可作为 host | 可观看 | 声音 |
| --- | :--: | :--: | :--: |
| Windows | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Linux | ✅ | ✅ | ✅ |
| Android | ✅ view-only | ✅ | ⚠️ Android 10+ |
| iOS | ✅ view-only | ✅ | ⚠️ 仅 app 的音频 |

除第 12 节另有说明外，各平台的 client 功能集相同。手机与平板以 **view-only** 方式担任
host：它们推送自身屏幕，但不接受 remote input，因为没有任何移动 OS 允许普通 app 操作
设备。

app 在各平台上划分为相同的部分：**Host**、**Client** 与 **Settings**，另有 **Devices**
页列出与本机 pair 过的机器（第 9 节）。

三个桌面平台另提供一个 command line client。它提供相同的行为但没有页面界面：可担任
host、connect、打开 remote shell、查找机器，并读写与 app 完全相同的 settings、已 pair
机器列表和 trust 的 host key。它是本文档所述行为的另一种界面形式，而非另一套行为。

---

## 4. 作为 Host —— 共享本机屏幕

| ID | 功能 | 说明 |
| --- | --- | --- |
| H-1 | 选择 display | 共享前，用户勾选本机的哪些 display 对外提供。至少需选择一项。 |
| H-2 | 共享多块 display | 可同时共享多块 display；每块成为独立的 source，供 viewer 选择。 |
| H-3 | Source 上限 | 同时最多共享 **8** 块 display。若机器拥有更多，将提示用户仅前 8 块会被共享。 |
| H-4 | 开始与停止共享 | 一个操作开始共享，一个操作停止。一旦开始共享或正在开始，就会出现一条横幅显示状态（*Starting share…* / *Sharing*）及其细节；在此之前，按钮自身的文字（*Start sharing*）就是唯一的状态提示，因此不会绘制横幅。 |
| H-5 | 停止单块 display | 可单独停止某一块正在共享的 display，而不结束整场共享。 |
| H-6 | 连接信息 | 共享期间，app 列出本机的 network 地址以及 viewer 需使用的 port，便于告知他人或复制。桌面端的 *Share on network*（T-9）位于 host 界面该列表旁；列表仅显示所选 network 的地址，选择 *All networks* 则显示全部。共享中或正在开始共享时该选项被锁定，需停止共享后方可更改。 |
| H-7 | 实时 session 表 | 对每块共享中的 display，host 可见：display 名称、分辨率、viewer 数量、capture rate、send rate、占用带宽与 round-trip time。每个已连接的 viewer 在对应 display 下各占一行，若该 viewer 设置了名称（C-7）则以「名称 (ip:port)」标识，否则仅显示地址。 |
| H-8 | 断开某个 viewer | host 可从 session 表中断开任意单个 viewer。 |
| H-9 | Viewer 上限 | 同一时刻最多 **5** 个 viewer 观看同一 host。超出的请求以忙碌为由被拒绝。 |
| H-10 | 失败提示 | 若无法开始共享，将向用户显示原因，而非静默失败。terminal 因 port 被占用而无法启动的情况会被明确说明。 |
| H-11 | Terminal source（桌面） | source 列表中另有 **Terminal — a shell on this machine**。该项在每次列表显示时重新勾选，且不会被保存；仅共享屏幕、仅共享 terminal 或两者同时共享均为有效配置。三者共用 app 的同一个 UDP port（T-4）、同一 passcode 与同一 network 选择。 |
| H-12 | 表中的 shell session | terminal 共享期间，实时表中显示一行 *Terminal* 及其 port，每个已打开的 shell 在其下各占一行，标识方式与 viewer 相同（C-7），并带 *Disconnect*。*Terminal* 行上的 *Stop* 仅结束 terminal 共享。同时最多打开 **8** 个 shell。shell 的打开、关闭与 reattach 均连同 client 的地址、名称和 key 写入 session log（G-3）。 |
| H-13 | shell 常驻 host | 连接丢失的 shell —— network 中断，或 client 窗口关闭 —— 会连同内容与 scrollback 一起保留在 host 上，不设时间限制。同一 client 自行 reattach：在两分钟内以逐步延长的间隔重试，重试期间明确显示正在 reattach，并接回同一个 shell。任何已准入的 client 也可询问 host 正在保留哪些 shell，并按 id reattach 其中之一，而不必打开新 shell。shell 仅在其 shell 进程退出、host 停止共享 terminal，或从 session 表中关闭时结束；关闭 client 窗口永远不会结束 shell。 任何已准入的 client 也可以从该列表按 id 结束一个被保留的 shell；当时正在其中输入的机器会被告知该 shell 已结束。 |
| H-15 | 空闲的 shell 不视为断开 | 无流量的 terminal 连接由 client 保活，因此停留在提示符处的 shell 不会被误判为链路断开而关闭。 |
| H-16 | File transfer source（桌面） | source 列表中另有 **File transfer — files viewers send**，在每次列表显示时重新勾选，且不会被保存。与 terminal（H-11）相同，它共用 app 的同一个 UDP port（T-4）、passcode 与 network 选择。文件写入共享开始后实时表中 *File transfer* 行标明的文件夹（H-17、T-25）。单个 batch 最多 **32** 个文件、单文件 **8 GiB**、合计 **32 GiB**；超出部分或无法保存的文件名将连同原因被拒绝。每个文件先以最终名称加 `.deskhub-part` 后缀写入，仅在完整到达且 checksum 匹配后改名；损坏的文件被丢弃，并终止整个 batch。不会覆盖任何文件：文件夹中已存在的同名文件将追加编号。若该文件夹不可写，host 不接收任何文件并明确告知，而非静默失败。 |
| H-17 | 表中的传输 | file transfer 共享期间，实时表中显示一行 *File transfer* 标明文件夹，旁边配一个 **Open folder** 按钮，用于在系统文件管理器中打开该文件夹，每台正在发送的机器在其下各占一行，标识方式与 viewer 相同（C-7），并显示正在接收的文件、其在 batch 中的位置与完成比例，或 batch 终止的原因。*File transfer* 行上的 *Stop* 仅结束文件传输。batch 的提出、接受、拒绝与完成均连同该机器的地址、名称和 key 写入 session log（G-3）。 |
| H-14 | Stop & attach（桌面） | 每一行 shell，无论处于活动状态还是等待 reattach，都另有 **Stop & attach**：远端 client 被断开（其窗口报告 shell 已结束），同一个 shell 在 host 的 terminal 窗口中打开，内容与 scrollback 保持不变。此后该 shell 归属于 host：原 client 无法再 reattach，本来就没有时间限制（H-13），表中该行标记为 *attached on this machine*，关闭 host 上的窗口或点击该行的 *Stop* 即结束该 shell。此次接管同样写入 session log（G-3）。 |
| H-18 | 清晰展示的 passcode | 一旦开始共享或正在开始，host 页面就会单独展示配对 passcode —— 数字间距拉开、字号放大、等宽字体 —— 旁边配一个 **Copy passcode** 按钮，将纯数字代码放入剪贴板；在此之前没有可展示的代码，因此该区块保持隐藏。代码不再嵌在共享状态句中，该句仅保留说明。未设置 passcode 时，该区块显示 *None*，也不提供复制按钮。passcode 本身在各平台的 **Settings** 页面与其他安全设置一同编辑。 |

## 5. Connect —— 观看另一台机器

| ID | 功能 | 说明 |
| --- | --- | --- |
| C-1 | 按地址 connect | 用户在一个输入框中填写 host 的 IP 地址，在另一个输入框中填写 UDP port，后者预填默认值 `47777`。在地址框中粘贴 `192.168.1.10:47777` 同样有效，其中显式指定的 port 优先于 port 框。无效输入将给出解释性提示，而不会直接失败。 |
| C-2 | 输入 passcode | passcode 框可以留空。填入的码必须恰为 4 位数字，否则在发送任何数据之前 connect 即被拒绝。框中显示的值即为实际使用的值，不会在用户不知情的情况下填入任何内容。留空时由 host 决定：已 pair 的机器直接通过；未 pair 的机器需等待约一分钟，其间 host 前的用户会被询问是否接受（S-2）。填入的码被 host 拒绝时，将给出指明 passcode 的失败提示。从设备列表打开的对话框会显示该设备的 UDP port 与已保存的 passcode（D-7），两者均已预填且可编辑。 |
| C-3 | 取消 control | connect 之前，viewer 可取消勾选 *control the remote machine*，从而仅观看而不发送任何 input。完全不接受 input 的 host —— 手机或平板（P-4），或关闭了 input 的桌面 —— 在被询问其共享内容时会予以说明，桌面 client 则显示常驻提示，说明对此类 host 而言 control 与 terminal 均不起作用。 |
| C-4 | 选择 source | 若 host 共享了多块 display，将询问 viewer 观看哪一块。选择多块会打开多个窗口。若 host 仅共享一块 display，则直接打开。 |
| C-5 | 明确的失败原因 | 若 host 不可达、未在共享，或拒绝了 passcode，将明确告知 viewer 属于哪一种情况，并在提示中包含地址。 |
| C-6 | Session 结束通知 | session 结束时，无论由哪一侧结束，viewer 都会看到原因。 |
| C-7 | Viewer 名称 | connect 页上的 *Your name* 字段用于命名本设备。在用户首次设置名称之前，该字段按平台预填默认值：Windows 与 Linux 使用 hostname（无 hostname 时使用登录用户名），macOS 使用电脑名称，iOS 使用设备名称，Android 使用设备型号。该字段可编辑，connect 时字段中的内容即为保存并发送的内容。该值不会为空：清空后再 connect 将恢复上述平台默认值，该默认值重新填入字段，并被保存和发送，因此每次连接都附带一个名称。host 将该名称显示在本机地址旁，以便区分各个 viewer。名称保存在本设备上，最多 **64** 字节文本，控制字符会被移除。 |
| C-8 | 打开 shell | *Terminal — open a shell* 是 host 应答后出现的按钮（C-10），在所有 client 上均可用。shell 在独立窗口中打开，包含字符网格、scrollback、status 行，手机上另有一行辅助按键（Esc、Tab、可锁定的 Ctrl/Alt、方向键、^C）。该窗口同时说明 shell 无法打开的原因（passcode 错误、被拒绝、不可达）。同时观看屏幕并使用 shell 属于常规用法。所有 client 在打开任何窗口之前都会确认 host 共享了哪些内容：对于没有 terminal 的 host —— 手机、平板，或未共享 terminal 的桌面 —— 该按钮保持 disabled，不会打开 terminal 窗口。client 也可询问 host 正在保留哪些 shell，并按 id reattach 其中之一，而不必打开新 shell。 当 host 已经保留着 shell 时，按下该按钮会先打开**这些 shell 的列表** —— 每行标明其 id、尺寸以及打开它的机器 —— 只有在 reattach 其中一个或选择 *New shell* 之后才会打开 shell；若没有保留任何 shell，则直接打开一个新的。他人正在输入的 shell，或已被 host 接管的 shell（H-14），会列出但无法 reattach。client 有权结束的每一行还提供 *Close shell*，先询问，然后在 host 上结束该 shell，无论当时由谁持有。 |
| C-9 | 发送文件 | 所有 client 均可向正在接收文件的 host 发送文件。*File transfer — send files to it* 是 host 应答后出现的按钮（C-10），在所有 client 上均可用，点击后打开 **Send files** 界面。在 Android 与 iOS 上，文件通过系统相册选择器或系统文件浏览器选取，发送前会在 app 自身的 cache 中准备一份副本。同一时间只处理一个 batch：有 batch 正在进行时选择器被禁用，第二次提出以忙碌为由被拒绝。进度信息给出正在发送的文件、其在 batch 中的位置与完成比例，传输过程中可随时停止。结束后，batch 中的每个文件都会列出是否发送成功及其原因。所有 client 在打开任何窗口之前都会确认 host 共享了哪些内容：对于不接收文件的 host，该按钮保持 disabled，不会打开窗口。 |
| C-10 | 先 connect，后选择 | Connect 仅执行 authenticate：它与 host 建立连接，完成 pairing 或 passcode 校验（S-2），并查询 host 共享了哪些内容。已应答的 host 在各平台提供相同的内容：其地址、一个 **Disconnect**、V-7 所述的实时状态行，以及 *Remote desktop — view its screen*、*Terminal — open a shell*、*File transfer — send files to it* 各一个按钮，其中仅 host 实际共享的项目可用。打开 session 时复用刚刚完成的 pairing，因此 host 侧的用户不会被再次询问。这些内容的呈现位置因平台而异（C-11）。 |
| C-11 | 每个 host 一个窗口（桌面） | 在 Windows、Linux 与 macOS 上，已应答的 host 会打开独立的**连接窗口**，标题为该 host 的地址，其中包含 C-10 列出的全部内容。connect 页本身不改变状态：地址、port、passcode、名称等字段，Connect 按钮与设备列表均保持原样，因此在第一个 host 仍处于打开状态时即可连接下一个，一台机器可同时连接多个 host。对已有窗口的 host 再次 connect 只会将该窗口置前，而不会打开第二个。关闭某个连接窗口，或点击其中的 **Disconnect**，仅断开该 host，其余不受影响；退出 app 则全部关闭。由某个窗口打开的 session（V-1、C-8、C-9）是各自独立的窗口，其生命周期长于该连接窗口。在 Android 与 iOS 上同时只有一个连接，且保持在 connect 页上：Connect 成功之前，该页仅包含输入字段、Connect 与设备列表；host 应答后，这些内容被 C-10 所列内容取代，而 Disconnect，或对地址、port、passcode 的修改，都会使该页回到初始状态。 |

## 6. 查找机器

| ID | 功能 | 说明 |
| --- | --- | --- |
| D-1 | Network scan | client 在本地 network 上 scan 当前正在共享的机器并列出，scan 期间显示进度（「已检查 *m* 个地址中的 *n* 个」）。若 scan 未发现结果，将说明机器可能不出现的原因：它仅在共享时出现。 |
| D-2 | Scan 范围 | 单次 scan 最多覆盖本地 subnet 上的 **512** 个地址。若本机没有本地 network 地址，将告知用户无法执行 scan。 |
| D-3 | 自动重新 scan | scan 定期重复执行，也可通过 *Refresh now* 随时手动触发。 |
| D-4 | 点击即连接 | 点击已发现的设备即开始向其发起连接。 |
| D-5 | 最近设备 | 曾连接过的机器保留在 *Devices* 列表中，最多 **10** 台，在 *Where* 列标记为 *Recent*，并显示地址、status、ping 与上次连接时间。 |
| D-6 | 实时状态 | 每台最近设备显示 **Online**、**Offline** 或 **Checking…** 及 round-trip time，每 **30 秒**自动刷新一次，也可手动刷新。 |
| D-7 | 已保存的 passcode | 某台设备使用过的 passcode 会随该设备一并保存，并在从设备列表 connect 时预填至对话框，以可编辑字段的形式明确显示。不输入码直接 connect 不会清除已保存的码；输入新码则替换旧码。该码以遮蔽形式存储，这属于便利措施而非保护手段（见第 9 节）。机器完成 pair 后该码不再起作用，机器通过 key 被识别。 |
| D-8 | 移除设备 | 最近设备可从列表中移除。 |

## 7. 观看 session

| ID | 功能 | 说明 |
| --- | --- | --- |
| V-1 | 适应窗口 | 远端屏幕按窗口尺寸缩放并保持宽高比，窗口在打开时按 source 确定大小。桌面端上，当 stream 的形状在 session 中途确实发生变化时 —— 作为 host 的手机或平板旋转，或切换到形状不同的 display —— 窗口会按新形状重新适配；形状不变而仅 quality 变化时窗口不作调整。 |
| V-2 | 缩放与平移 | 画面最高可放大至 **5×**，并支持平移。缩放级别会显示，并可通过一个操作复位。 |
| V-3 | Session 状态 | 窗口显示一行实时 status：frame rate、带宽、round-trip time 与端到端 latency。 |
| V-4 | 带标题的窗口 | 每个 viewer 窗口的标题包含其显示的 source 与当前状态，因此多个 session 可以区分。 |
| V-5 | Disconnect | viewer 可随时结束 session。 |
| V-6 | 声音 | 在双方均支持的情况下（第 3 节），viewer 可听到被共享机器正在播放的声音，与画面的偏差约在一个 frame 以内。声音使用独立 channel：丢失一个 packet 仅损失零点几秒，且不影响画面；未播放任何内容的机器几乎不占用带宽。关闭了该功能的 viewer 不接收声音（T-23），关闭了该功能的 host 也不发送（T-22）。 |
| V-7 | 连接状态指示 | 该指示位于 host 应答的位置 —— 桌面端为连接窗口，Android 与 iOS 为 connect 页（C-11）—— 而不在 session 窗口中。它显示 host 的地址、一个 **Disconnect**（V-5），以及说明连接状态的实时行并附带 ping，在该 host 停止应答时立即转为红色。数据来源于为设备列表提供数据的同一个每秒一次的 probe，因此在打开 session 之前即已存在，并在多个 session 运行期间持续显示；桌面端上每个已打开的 host 各有一份。失去 host 的 session 窗口仍会显示正在 reattach（V-8）。 |
| V-8 | 自动重连 | 失去 host 的 session —— network 中断，stream 停止 —— 不会立即结束。窗口保留最后一帧画面，显示正在 reattach，并以 backoff 方式重连最多一分钟；host 再次应答后画面自动恢复。仅在该时间之后，或 host 主动结束、拒绝该 session 时，窗口才会连同原因关闭。shell 窗口自行重试两分钟后报告连接丢失；shell 本体无时间限制地留在 host 上，随时可接回（H-13）。 |

## 8. 操作远端机器

| ID | 功能 | 说明 |
| --- | --- | --- |
| I-1 | Mouse | 移动、左键、右键、中键、后退键、前进键以及滚轮均发送至 host。 |
| I-2 | Keyboard | 按下与释放事件均会发送，包括修饰键组合。 |
| I-3 | Pointer lock（桌面） | `F9` 将 mouse 锁定到远端屏幕，供需要原始位移的游戏及其他软件使用；`F9` 或 `Esc` 解除。当前状态显示在窗口标题中。 |
| I-4 | 失焦保护 | 失去焦点时会释放 pointer lock 以及所有仍处于按下状态的按键，因此不会有按键滞留在 host 上。 |
| I-5 | 触摸 trackpad（移动端） | 在手机与平板上，视频区域充当 trackpad：拖动移动指针，点按为左键，双击为右键，长按并拖动为拖拽，双指竖直拖动为滚动。 |
| I-6 | 指针与平移模式（移动端） | 一个开关在移动远端指针与平移放大后的画面之间切换。 |
| I-7 | 屏幕 keyboard（移动端） | 设备 keyboard 可按需显示或隐藏，并直接向远端机器输入。 |
| I-8 | 快捷键条（移动端） | 为触摸 keyboard 上不便输入的按键提供的按钮：`Esc`、`Tab`、`Enter`、四个方向键、`Del`、`Ctrl+C`、`Ctrl+V`。 |
| I-9 | host 优先 | 位于 host 机器前的用户所产生的 input 优先于任何远端 viewer。 |
| I-10 | 同一时刻仅一个 viewer 操作 | 同一时刻仅有一个 viewer 控制 mouse 与 keyboard。发生竞争时，先加入者优先；其他 viewer 的 input 将被忽略，直至当前操作者持续 **1 秒**无操作。 |
| I-11 | 强制 view-only | 当 host 关闭了 control，或 viewer 选择仅观看时，没有 input 能够到达 host，viewer 窗口也会显示其处于 view-only 状态。 |

## 9. 访问控制与安全

| ID | 功能 | 说明 |
| --- | --- | --- |
| S-1 | Encrypt | session 运行在已 encrypt 的 transport（QUIC/TLS）之上。session 承载的全部内容 —— video、control、input、clipboard 与 terminal 流量 —— 在两台机器之间均为 encrypt 状态。Discovery beacon 采用明文属于既定设计，且不携带任何机密；到达该 port 的其他未 encrypt packet 一律丢弃。完整说明见 [`SECURITY.zh.md`](../SECURITY.zh.md)。 |
| S-2 | 由 pairing 控制准入 | 机器首次 connect 时，由 host 决定是否接受。能够提供 host passcode 的机器，即以密码学方式证明了自己知道该码，而该码本身不经过 network。不提供码的机器，或 host 未设置码的情形，则转由 host 前的用户决定：*Let this machine in?*，提供 **Allow** 与 **Deny** 两项，一次回答适用于该机器正在打开的全部内容，包括屏幕与 shell。接受即意味着将两台机器 **pair**：此后该机器通过 key 被识别，无需 passcode 即可 connect，直至被 forget。但填入的码始终会被校验：即便是已 pair 的机器，提供错误的码同样会被拒绝。 |
| S-3 | passcode 可选，已 pair 列表 | passcode 为可选项，默认留空；留空时，未经 host 前用户批准，任何机器都无法接入。已 pair 的机器列在 **Devices** 页，包含名称、key、pair 时间与最后出现时间，并提供 *Forget* 与 *Forget every machine*、一个 *allow new pairings* 开关（关闭后仅接受已 pair 的机器），以及本机自身的 key 供核对。host 仅向已接受的机器透露其共享内容。 |
| S-4 | 连续失败后锁定 | passcode 连续错误 **3** 次将使 host 的 pairing 锁定 **30 秒**，并告知正在尝试的机器等待。此后每次连续锁定的时长为上一次的两倍，最长 **1 小时**；输入正确的 passcode 后恢复为 30 秒。已 pair 的机器不受影响。 |
| S-5 | control 开关 | host 可在关闭 *viewers can control this machine* 的状态下共享，此时无论 viewer 请求什么，所有 session 均为 view-only。 |
| S-6 | capture 的同意 | 在有此要求的平台上，使用操作系统自身的 permission 提示与屏幕选择对话框；未获用户授予时 Deskhub 无法 capture。 |
| S-7 | 仅在明确要求时共享 | 在用户开始共享之前不共享任何内容。关闭或停止即结束所有 session。 |
| S-8 | key 变更警告 | client 记录其 trust 过的每个 host 的 key。若该 key 发生变化 —— 这正是中间人攻击的典型特征 —— 将显示醒目的警告并给出新的 fingerprint，在用户明确接受之前拒绝连接。从未出现过的 key 由 pairing handshake 本身处理，不会提示。 |

## 10. Settings

Settings 按机器保存，跨重启保留，并自下一次开始共享时生效。手机与平板仅提供 network
port（T-4）—— 同时决定 network scan 检查的 port —— 以及 clipboard 同步（T-17）与保持
唤醒（T-19），另在共享界面提供 passcode（T-5）与共享所用的 network（T-9）。其余项目使用
内置默认值。

在桌面端，Settings 页面把这些项目分成三个区域：**Host** 放仅共享时使用的项目（T-1 – T-3、
T-5、T-6、T-11、T-22 以及 T-25 的文件夹），**Client** 放仅观看时使用的项目（T-23），
**General** 放两端都会使用的项目（T-4、T-13、T-15、T-17、T-19）。

| ID | Setting | 取值范围 | 默认值 |
| --- | --- | --- | --- |
| T-1 | Frame rate | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Quality | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Network port | 1 – 65535 | 47777 |
| T-5 | Passcode | 留空，或恰为 4 位数字 | 留空（见 S-2、S-3） |
| T-6 | viewer 可 control 本机 | 开 / 关 | 开 |
| T-9 | Share on network | All networks · 本机的某个地址 | All networks |
| T-11 | 打开 app 时开始共享 | 开 / 关 | 关 |
| T-13 | 登录时启动 Deskhub | 开 / 关 | 关 |
| T-15 | 在后台继续运行 | 开 / 关 | 关 |
| T-17 | 同步 clipboard 文本 | 开 / 关 | 关 |
| T-19 | session 期间保持本设备唤醒 | 开 / 关 | 开 |
| T-21 | 允许新机器 pair（Devices 页） | 开 / 关 | 开 |
| T-22 | 向 viewer 共享本设备的声音 | 开 / 关 | 开 |
| T-23 | 播放所观看设备的声音 | 开 / 关 | 开 |

| ID | 功能 | 说明 |
| --- | --- | --- |
| T-7 | 自动 quality | stream 的 quality 在配置的上下限内，依据可用的 network 容量自动调整；条件变化时无需用户操作。 |
| T-8 | 取值校验 | 超出范围或非数值的输入将被拒绝并保留原值，而不会被应用。 |
| T-10 | Network 回退 | 指定特定 network 时（T-9），host 仅可通过该地址访问。若开始共享时该地址已不存在，host 改为在所有 network 上共享，并在共享 status 中说明。已保存但当前不可用的地址仍会列出，并标记为 *not connected*。 |
| T-12 | 启动时自动共享 | 仅桌面。T-11 开启时，打开 app 将直接进入 Host 页，并使用已保存的 settings 开始共享，与用户手动按下 Share 等效。app 在登录时启动（T-13）时，桌面可能尚无任何 display；此时共享将等待，每半秒复查一次，最长 30 秒，一旦出现 display 即开始。等待期间 Host 页显示等待状态。若始终没有 display 出现，app 将共享其他已勾选的内容（terminal），或停止并在 Host 页给出原因。自动共享不会弹出对话框，因为登录时窗口可能位于 tray 中而不可见。各平台规则仍然适用：Linux 首次显示桌面的屏幕共享对话框，之后复用已保存的选择（P-3）；macOS 仍要求相应的 permission（P-2）。 |
| T-14 | 登录时启动 | 仅桌面。T-13 开启时：Linux 在 `~/.config/autostart` 写入一个 autostart 条目；Windows 注册名为 *Deskhub* 的 scheduled task，在登录时以提权方式启动 app，因而不会出现 UAC 提示；macOS 注册一个用户也可在 System Settings 中看到的 Login Item。关闭该选项时会移除相应条目。复选框始终显示操作系统报告的状态，而非上次保存的值。 |
| T-16 | 后台模式 | 仅桌面。T-15 开启时，将出现 tray 或菜单栏图标，包含 *Show/Hide window*、*Start/Stop sharing* 与 *Quit*；关闭窗口将隐藏 app 而非退出，共享在后台继续。窗口在启动时总会出现，仅在用户关闭时隐藏，因此 T-13、T-11 与 T-15 同时开启时，将在登录时开始共享并保持窗口显示，直至被关闭。Windows 上左键点击 tray 图标可显示或隐藏窗口。macOS 上窗口隐藏期间 Dock 图标消失。Linux 上 tray 需要 StatusNotifier host（KDE 标配；GNOME 需要 AppIndicator 扩展）；若不具备，关闭窗口仍会退出，以确保 app 不会变为无法访问的状态。在 Windows 与 Linux 上，共享进行期间关闭窗口一律隐藏至 tray，即使 T-15 处于关闭状态（前提是存在 tray），以免断开已连接的 viewer；macOS 上关闭窗口不会退出 app，因此共享在任何情况下都会继续。 |
| T-18 | Clipboard 同步 | T-17 开启时，session 中任一机器上复制的纯文本会在数秒内出现在其他机器上，双向均可；host 会将某个 viewer 的复制内容转发给其他 viewer。文本上限为 32 KiB，超长内容在完整字符处截断；图片、文件与格式不会传输。host 的开关决定整个 session 的行为：关闭时，host 既忽略也不发送 clipboard 数据。每台机器还需自身开关处于开启状态，才能读写本地 clipboard。Android 与 iOS 上操作系统另有限制：Android 设备仅在 Deskhub 处于前台时才能获取自身的复制内容，而接收到的文本任何时候都可写入；iOS 上的 viewer 在 Deskhub 读取新复制内容时可能出现系统粘贴提示；作为 host 的 iOS 设备不参与同步，因为其 broadcast 运行在无法访问 clipboard 的独立 process 中。 |
| T-20 | 保持唤醒 | T-19 开启时，机器在共享或观看期间不会进入睡眠，display 也不会关闭；session 一结束即解除该限制，且不修改任何睡眠设置。在 Windows、macOS 与 Linux 上，这对 host 与 viewer 同时覆盖 display 睡眠与系统睡眠（Linux 上需要 systemd-logind，以及遵循 freedesktop screensaver 接口的桌面环境，KDE 与 GNOME 均为标配）。在操作系统强制的情形下仍以系统为准：合上笔记本上盖、按下电源键，或 macOS 使用电池供电时，机器仍可能进入睡眠。在 Android 与 iOS 上，该开关在观看 stream 时保持屏幕常亮；而手机端的共享本身在屏幕熄灭时仍可继续（P-5），因此作为 host 时不会保持屏幕常亮。 |
| T-25 | 接收文件的存放位置 | 仅桌面。viewer 发送的文件写入本机选定的文件夹，默认为用户主目录下的 `Deskhub`。所选文件夹在共享期间标注于实时表的 *File transfer* 行（H-17），旁边配一个 **Open folder** 按钮——共享开始前不显示，因为此时它还没有任何作用。该文件夹若不存在则自动创建，所选路径与其他 settings 一并保存。不会向该文件夹之外写入任何内容：发送方提供的名称会被截取为路径的最后一段，并清除本地 filesystem 无法保存的字符。 |
| T-24 | 共享的声音内容 | T-22 开启时，host 共享其扬声器正在播放的内容，即该机器上所有应用产生的混合音频。Deskhub 不 capture microphone，也不提供双向音频。Android 是唯一涉及 permission 的平台：其 playback-capture API 位于系统标记为 *Microphone* 的 permission 之后，app 在开始共享时申请该 permission，且不作其他用途；若被拒绝，共享继续进行但没有声音。其他平台不申请 microphone permission。viewer 仅在自行开启相应选项（T-23）时才接收声音，因此开启 T-22 的 host 也不会向未开启接收的 viewer 发送数据；两个开关均自下一次 session 开始时生效。 |

## 11. 状态与故障排查

| ID | 功能 | 说明 |
| --- | --- | --- |
| G-1 | 实时 host 统计 | 按 display 与 viewer 给出 capture rate、send rate、带宽与 round-trip time。 |
| G-2 | 实时 client 统计 | 按 session 给出 frame rate、带宽、round-trip time 与端到端 latency。 |
| G-3 | Session log | 在 Windows、macOS 与 Linux 上，每次运行都会向用户的 Deskhub 文件夹写入一份 log 文件，便于附在 bug 报告中。Android 与 iOS 将诊断信息写入操作系统自身的 log 流，不留文件。 |
| G-4 | 版本与项目链接 | app 显示自身版本，并提供指向项目页面的链接。 |

## 12. 各平台特有行为

| ID | 平台 | 行为 |
| --- | --- | --- |
| P-1 | Windows | app 在启动时申请一次管理员权限，这是它能够向提权窗口输入的前提。开始共享时会自行添加一条 firewall 规则。 |
| P-2 | macOS | 显示 **Permissions** 面板，包含 *Screen Recording*（共享所需）与 *Accessibility*（接受 remote input 所需）的实时授权状态、各自的申请按钮，以及通往 System Settings 的快捷入口。未授予 Accessibility 时，部分按键会被 macOS 静默拦截。 |
| P-3 | Linux | Host 页与其他桌面平台一致，将本机的各块 display 与 terminal 列为勾选项，仅勾选的 display 会被共享。按下 Share 后，桌面仍会通过其自身的屏幕共享对话框确认 screen capture；在桌面支持的情况下（ScreenCast portal 版本 4 及以上），该确认会被保存，之后的共享静默复用，跨重启同样有效，因此对话框仅在首次出现。若桌面授予的内容与勾选的 display 不匹配，已保存的确认将被清除并再次显示对话框，以便用户授予正确的 display；若桌面拒绝或该确认已过期 —— 例如更换 compositor 或变更显示器之后 —— 对话框将再次出现，而取消对话框不会触发重试。若桌面授予的 display 无法与勾选列表对应，则共享桌面授予的全部内容，而非不共享任何内容。仅勾选 terminal 时完全跳过桌面对话框。此外，共享还要求系统允许 input injection。 |
| P-4 | Android / iOS | 作为 host 时为 **view-only**：设备推送屏幕内容，并丢弃所有 control packet，因为两个 OS 均不允许 app 在系统范围内 inject input。设备不共享 terminal，并在被查询时予以说明，因此不会有 client 对手机打开 terminal 窗口（C-8）；桌面 client 据此可提示勾选 control 不会生效（C-3）。只要 app 显示在屏幕上，设备即按 H-16 的 batch 规则接收文件，无需查找开关，并在屏幕共享期间持续接收，因此 viewer 可以一边观看屏幕一边发送文件。iOS 上，broadcast extension 在整个 broadcast 期间占用同一个 port 并由其提供两项功能。接收到的照片与视频将加入设备的相册：iOS 上需经系统仅添加模式的 Photos permission，在首个文件到达时申请，且不授予 Deskhub 任何读取权限，若被拒绝则文件改存至 Documents；Android 上通过系统媒体库存入 `Pictures/Deskhub` 与 `Movies/Deskhub`。其他文件存放在系统文件浏览器可见的位置（iOS 为 app 的 Documents 文件夹，Android 为 `Download/Deskhub`），并通过通知说明到达的内容。此处使用的 Android 媒体库需要 Android 10：在 Android 9 及更早版本上，到达的文件保留在设备上 app 自身的文件夹中，不会出现在相册或 Downloads 中。整块屏幕作为单一 source 共享，因此 display 选择、多 display 共享与单块停止（H-1、H-2、H-3、H-5）均不适用。旋转设备时 stream 随之旋转：viewer 看到的画面始终方向正确，其窗口也会按新形状重新适配（V-1）。session 界面以触摸操作为主：trackpad 手势、缩放控件、快捷键条、屏幕 keyboard、display 切换，以及位于角落、与 shell 和文件传输界面共用的关闭按钮。 |
| P-5 | Android | 共享需要系统的录屏同意对话框，该授权按每次共享给予且无法保存。共享声音另需 Android 标记为 *Microphone* 的 permission，因为 playback-capture API 位于其后；该 permission 在共享开始时申请，若被拒绝则共享无声的屏幕。共享期间显示一条常驻通知，app 退至后台或屏幕熄灭时 stream 仍继续。从系统通知中停止共享即结束 session。 |
| P-6 | iOS | 共享由 app 内的 **Start sharing** 按钮发起，该按钮打开系统的 broadcast 表单，因为 iOS 要求每次 broadcast 均经由该表单确认；broadcast 运行在独立 process 中，因此在 app 关闭后仍继续。共享界面报告已连接的 viewer 数量，并列出已设置名称的 viewer（C-7），同时报告 broadcast process 当前的内存占用，因为 iOS 会终止超出内存上限的 broadcast。该界面不提供 H-7 所述的按 viewer 的表格，也不支持单独断开某个 viewer（H-8）。终止 broadcast 的系统事件，例如来电，将结束 session。app 离开屏幕时仍处于打开状态的 session —— stream、shell 或文件传输 —— 会在 iOS 给予已离屏 app 的时间内保持，约半分钟，因此短暂切换到其他 app 不会中断。超过该时间后系统将挂起 app，session 会在 app 返回时自行 reattach（V-8）。 |

## 13. 明确不在范围内

Deskhub **不**提供，本规格也不涉及：

- Microphone capture、双向音频，或任何语音通道。声音仅单向传输，从被共享的机器流向
  观看者（V-6）。
- 远程打印。
- 纯文本以外的 clipboard 同步（图片、文件、富文本）。
- 任何账号、目录、在线状态或邀请机制。
- Relay、rendezvous 或 NAT-traversal 服务。经由互联网访问 host 属于用户自身的责任，
  例如通过 VPN 实现。
- Session 录制。
- 无人值守访问、wake-on-LAN，或远程电源控制。
- 多用户管理、角色划分，或审计记录。
