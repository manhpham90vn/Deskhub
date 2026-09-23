[English](SECURITY.md) · [Tiếng Việt](SECURITY.vi.md) · **中文** · [日本語](SECURITY.ja.md)

# Deskhub 安全策略

_最后更新：2026 年 8 月 15 日_

本文件是 [`SECURITY.md`](SECURITY.md) 的译本；若两者有出入，以英文版为准。

## ⚠️ 请先阅读

**Deskhub 会 encrypt 自己的 session。session 承载的全部内容 —— video、按键、mouse、
clipboard 与 terminal 流量 —— 均运行在 QUIC/TLS 之上，接入与否由一次 pairing handshake
决定：未知机器必须证明自己知道 host 的 passcode（该码本身不经过 network），或由 host 前
的用户批准。** 唯一未 encrypt 的部分是 discovery beacon，它不携带任何机密；其余从
encrypt 连接之外到达的数据一律丢弃。

每个 host 还可以 **view-only** 方式共享（input 被丢弃而非 inject），也可以完全关闭新的
pairing，仅接受已 pair 的机器。

Encrypt 不等同于可以暴露在 Internet 上：该 port 仍会回应 discovery 探测，4 位 passcode
仍是一个较短的秘密，首次连接仍以未经验证的信任为前提，且系统没有抗 flooding 的机制。
Deskhub 面向可信的 network 设计。

因此下列原则依然成立：

> **不要对 UDP 47777 做 port-forward。不要将正在共享的机器直接暴露到 Internet。远程
> 访问请使用 VPN —— 本项目以 [Tailscale](https://tailscale.com) 为验证对象 —— 并连接其
> `100.x.y.z` 地址。**

遵循该原则时，Deskhub 可以安全使用；不遵循则相当于把本机开放给 Internet。

## Threat model

### Deskhub 能够防护的内容

| | |
|---|---|
| 数据流向开发者 | 不存在此类数据。没有服务器、账号、telemetry 或第三方 SDK。见 [`PRIVACY.zh.md`](PRIVACY.zh.md)。 |
| 流量被读取 | 每个 session 都运行在 QUIC/TLS 之内：video frame、按键、clipboard 文本与 terminal 字节在两台机器之间均为 encrypt 状态。抓包只能得到流量规模与时间信息，得不到内容。到达该 port 的未 encrypt packet 一律丢弃，discovery 探测除外。 |
| 远端 viewer 争夺机器的控制权 | host 优先：一旦操作真实的 mouse 或 keyboard，remote input 即被暂停。该行为在 Windows、macOS 与 Linux 的 host 上一致。 |
| 按键滞留 | 远端仍处于按下状态的按键，会在 session 结束或 viewer 切换时自动释放。 |
| 陌生机器未经允许接入 | 接入由 pairing handshake 控制。未知机器必须通过 SPAKE2 证明 host 的 passcode —— 该码不经过网络，窃听者无法获得可离线破解的数据，每条 connection 仅允许一次尝试 —— 或者在未设置 passcode 时，等待 host 前的用户回答 *Let this machine in?*。连续三次尝试失败将使 pairing 锁定 30 秒，此后每次连续锁定的时长翻倍，最长一小时。接入之后机器处于已 pair 状态：通过密码学 key 识别，列在 host 的 Devices 页上，并可在该页撤销；Forget 该机器也会关闭它当前打开的所有 connection。接入资格仅在赢得它的那条 connection 存续期间有效。discovery beacon 不再确认所猜测的码：陌生机器的探测始终得到空列表，因此此前的探测式猜码手段已不存在。 |
| 后续连接中的中间人攻击 | 每台机器都有一个 key。client 保存已 pair 的每个 host 的 key，key 变化时拒绝重新连接，直到用户明确接受。passcode 的证明绑定到 client 实际收到的 host key，因此经 relay 的证明无法通过验证。 |
| 多个 viewer 争夺 mouse | 最多 5 个 viewer 观看同一 host，但只有一个 viewer 控制 input：先加入者优先，后加入者的 input 会被丢弃，直到前者持续一秒无操作。第 6 个 viewer 以 `Busy` 状态被拒绝。 |
| 仅允许观看的 viewer | view-only 共享在所有 host 上可用，它在 host 侧、在任何操作被 inject 之前即丢弃 input packet，并不依赖 client 自觉遵守。Android 与 iOS 的 host 始终为 view-only。 |
| 手机在共享状态下被遗忘 | 最终的防护由操作系统提供，而非 Deskhub：Android 显示常驻通知，并在每次共享时重新征求录屏同意；iOS 保持 broadcast 指示可见。两者都可在不打开 app 的情况下停止共享。 |
| 已 pair 的机器向本机写入文件 | 只有已接受的机器才能发送文件，且仅在接收方启用 file transfer 时。到达的数据无法离开接收方选定的文件夹：线上的文件名会被截取为路径的最后一段，并在任何文件被打开之前清除分隔符、控制字节、filesystem 不接受的字符以及保留设备名。每个文件先以 `.deskhub-part` 后缀写入，仅在完整到达且 CRC-32 匹配后改名。已存在的同名文件会追加编号而不被覆盖。单个 batch 限制为 32 个文件、单文件 8 GiB、合计 32 GiB。同样的名称处理在手机与平板上也会执行，早于数据进入相册或 Downloads 文件夹。 |
| 畸形 packet | 每个字段在读取前都进行边界检查。parser 具备 unit test，在 CI 中于 AddressSanitizer、UndefinedBehaviorSanitizer 与 ThreadSanitizer 下运行，并由 libFuzzer 每晚 fuzz，共七个 target，覆盖 wire format、H.264 解析、packet reassembly、terminal byte stream、UI 文案，以及 host 与 viewer 两侧的 session state machine。通过 fuzzing 发现的 crash 会作为 regression test 保留在 repo 中，新的覆盖也会并入 seed corpus。 |

### Deskhub **不能**防护的内容

以下为完整清单，其中没有任何一项在当前已得到解决：

- **被保留的 shell 属于这次 pairing，而不属于打开它的那台机器。** 留在 host 上的
  shell 比打开它的连接活得更久，且不设时间限制；每一台已准入的机器都可以列出 host
  正在保留的 shell、reattach 其中已 detach 的一个，并关闭其中任意一个。每个 shell 的
  id、尺寸与设备名都包含在该列表中。因此你 pair 的第二台机器 —— 或者你尚未在 Devices
  页面吊销其 key 的机器 —— 可以读回先前 shell 正在做的事并在其中继续操作。请吊销你
  不再信任的设备，并在用完后关闭 shell，而不是任其留存。
- **首次连接以未经验证的信任为前提。** pairing 能够阻止*此后*出现的中间人：key 已被
  固定，任何变化都会被明确拒绝。但它无法阻止在首次接触时就已处于中间位置的攻击者：
  未设置 passcode 时，client 连到哪台机器，哪台机器就会被 pair；而 passcode 所能提升的
  保护程度，仅相当于一个 4 位秘密。若这一点对你重要，请通过其他渠道核对 fingerprint。
- **流量分析仍然可行。** Encrypt 隐藏的是内容而非存在：观察者可以得知有 session 正在
  运行、video 的流量规模，以及你输入的时间。
- **没有 rate limiting，也不具备抗 DoS 能力。** 向该 port 灌入大量数据会中断 session；
  对于未设置 passcode 的 host，还可使批准提示被反复触发。
- **discovery beacon 仍会回应任何来源。** 来自任意源地址的 `LIST_SOURCES` 探测或
  `PING` 都会得到回应。陌生机器得到的是空列表，任何探测也无法确认 passcode，但该机器
  仍可通过扫描被发现，该 port 仍可被用作小型 UDP 反射器。有一个例外：当前持有 encrypt
  连接的源地址不会收到未 encrypt 的回应。机器证明身份之后，其发出的所有数据都必须以
  encrypt 形式到达，因此伪造的明文 `SOURCE_LIST` 或 `PONG` 无法冒充已连接的 peer。
- **设备名会被显示并记入日志。** viewer 发送的 *Your name* 在传输中已 encrypt，但仍会
  显示在 host 的屏幕上、写入 host 的日志，并保存在 host 的 paired-devices 列表中。其
  默认值为机器的 hostname，通常即为使用者的真实姓名。建议使用昵称，不要在该字段中填入
  敏感信息。清空该字段不会阻止名称被发送，只会恢复默认值。
- **viewer 名额在沉默 5 秒后自动释放。** 若你的 viewer 掉线，该名额将重新开放，下一个
  到达的 `Hello` 即可占用，只要发送方已通过 admission（pairing、passcode 或 approval）。
- **共享会暴露整块 display。** 不是单个窗口，而是该显示器上的每一条通知、每一个弹窗与
  每一个窗口。见 [`PRIVACY.zh.md` §3.4](PRIVACY.zh.md)。
- **手机或平板作为 host 时会暴露整台设备。** Android 与 iOS 同样可以作为 host，其推送
  的是整块屏幕：银行 app、一次性验证码、消息，以及共享期间输入的所有密码。该 stream
  与其他 session 一样经过 encrypt，但所有被接受的 viewer 都能看到全部内容。移动端 host
  始终为 view-only，这消除了被远程操作的风险，但并不降低信息暴露的风险。

## 适合运行的环境

✅ **安全**

- 你掌控全部设备的家庭或个人 LAN。
- 仅有你自己的设备加入的 Tailscale tailnet（或其他 WireGuard/VPN 隧道）。VPN 增加一层
  encrypt，并阻止陌生机器接触该 port。
- 仅作为 *client* 的机器（手机、平板、不共享屏幕的笔记本）。client 不接受入站 session。

❌ **不安全**

- 在路由器上为 UDP 47777 配置 port-forward，或将共享中的机器置于 DMZ。
- 在咖啡馆、酒店、机场、校园、联合办公或会议的 Wi-Fi 上共享屏幕。
- 在办公室或合租住所的 LAN 上共享，而你无法掌控其他设备。
- 任何存在访客设备、非你配置的 IoT 设备，或你不负责管理的他人机器的 network。
- 通过云 VM 的公网接口或公开隧道服务暴露该 port。

默认情况下 socket 绑定到所有接口（`INADDR_ANY`），因此在本机接入的每一个 network 上都
可被访问，包括你已不再关注的 network。**Share on network** 设置可收窄此范围：选定本机
的某个地址后，host 只绑定该接口，其他 network 上的机器无法接触该 port。有两点需要注意。
其一，若开始共享时所选地址已不存在（网线拔出、DHCP 分配了新地址），Deskhub 会回退到所有
接口并在共享 status 中说明；如依赖该设置，请留意该提示。其二，绑定单一接口同时也会阻止
同一机器上经 loopback（`127.0.0.1`）的 viewer。在 Windows 上，app 自启动起即以提权方式
运行（它申请一次权限，以便向提权窗口 inject input），并在共享时自动打开 firewall 规则。
该规则覆盖整个 app 与所有 profile，因此收窄绑定并不会收窄 firewall；这一便利之处也正是
上述原则重要的原因。

## 同一 network 中攻击者的能力

若有人与正在共享屏幕、且 Deskhub 正在运行的机器处于同一 LAN，他们可以：

1. 通过扫描 UDP 47777 发现该机器。未 pair 机器的探测得到的是空列表，但机器仍会回应，
   因此其存在仍会暴露。
2. 尝试接入。他们无法从线上读取 passcode，因为该码不经过网络。可用的方式是在线尝试
   （每条 connection 一次，三次错误将使 pairing 锁定 30 秒，此后每次锁定翻倍，最长一小时，
   因此试遍全部 10,000 个码需要数月），或在未设置 passcode 的
   host 上，等待 host 前的用户在批准提示上点击 **Allow**。
3. 在不接入的情况下观察流量，但只能得到规模与时间信息。session 的内容（包括 video）
   均已 encrypt，抓包无法重建屏幕或按键。
4. 向该 port 灌入大量数据以中断 session。对于能够接触到该机器的攻击者，没有任何 rate
   limiting 机制。

host 优先的机制可在你*坐在*机器前时限制异常操作，但在你离开时不起作用，而那正是需要
防护的时段。

## 加固清单

若继续按当前形态使用 Deskhub，建议执行以下各项：

- [ ] 在两台机器上运行 Tailscale，并仅通过 `100.x.y.z` 地址连接。
- [ ] 确认路由器上**没有**针对 UDP 47777 的 port-forward 或 UPnP 映射。
- [ ] 确定机器的接入方式：在 Settings 中设置 4 位 passcode，或留空并自行回答批准提示。
      定期检查 Devices 页，移除不再识别的机器。仅需他人观看时，取消勾选 *Viewers can
      control this machine*。
- [ ] 不使用时退出 Deskhub。它不是 background service，关闭即关闭了接入点。
- [ ] 在 Linux 上使用 `ufw` 时，请收窄规则而非全面放行：
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp`，而不是
      `sudo ufw allow 47777/udp`。
- [ ] 不要在会被带往其他 network 的笔记本上保持共享运行。
- [ ] 离开时锁定机器，避免无人看管的 session 被接管。
- [ ] 使用 `deskhub-cli` 时，不要将 passcode 写入命令本身。`--passcode 0417` 可被机器
      上的每个进程通过 `ps` 与 `/proc/*/cmdline` 看到，并会进入 shell 历史。请使用
      `--passcode -` 从标准输入读取、`--passcode @FILE` 从仅自己可读的文件读取，或设置
      环境变量 `DESKHUB_PASSCODE`。

## 本地保存的数据

诊断日志以纯文本写入 `~/.deskhub/`（Windows 上为 `%USERPROFILE%\.deskhub`），适用于
Windows、macOS 与 Linux。日志包含连接统计与 peer 地址，不包含屏幕内容或按键。

桌面 app 与 `deskhub-cli` 共用这些文件。该文件夹中还包括：`ui-settings.txt`（fps、
bitrate、分辨率上限、port、view-only 与 pairing 开关、已设置的 host passcode，以及向
host 显示的设备名）、`recent-devices.txt`（最近 10 个连接地址、时间，以及各自使用的
passcode）、`host_key.pem` 与 `host_cert.pem`（本机的私钥与自签 certificate，即其
fingerprint 背后的 identity；取得该 key 文件者可冒充本机）、`known_hosts`（本机 trust
过的 host 的 key）、`paired_devices`（本 host 接受的机器的 key、名称与时间戳）、
`auth_salt`（passcode verifier 使用的非机密 salt），以及 Linux 上的
`portal-restore-token.txt`（桌面针对所选屏幕签发的 token，仅对你的桌面 session 有意义，
不会被传输）。移动端 app 将设置保存在自身沙箱中，iOS 上为 app group 容器。保存的
passcode 使用固定的 XOR key 进行混淆，使其不会直接可读。**这不是 encrypt**：掌握源码
与该文件的人可在数秒内还原。请将该文件夹视为以你的身份运行的任何程序均可读取的内容。

其他机器发送的文件保存在该文件夹之外，位于接收方选定的目录（未另行选择时为用户主目录下
的 `Deskhub`，以 `transfer_dir` 保存）。在手机或平板上，这些文件位于设备的相册或
Documents / Downloads 文件夹，卸载 app 后仍会保留。请将送达该处的任何内容视为由已 pair
的机器放入你设备的文件。

上述内容不会被上传；你可以随时删除该文件夹。

## 计划中的缓解措施

按计划实施顺序列出：

1. **将 passcode 与 host key 保存在操作系统的 keychain 中**，而非文件中。
2. **使 discovery beacon 不再回应**未经请求的探测，而不是返回空列表。

自本清单上次修订以来已完成的事项：为整个 session 提供 encrypt transport（QUIC/TLS），
涵盖 video、input、clipboard 与 terminal，并丢弃未 encrypt 的数据（discovery 探测
除外）；采用 SPAKE2 pairing，使 passcode 不经过网络，无法被收集或离线 brute-force；
host 侧的批准提示；带撤销功能的已 pair 机器列表；机器 key 以及 client 侧的 key 变更
警告；以及 passcode 错误三次即锁定的机制，锁定时长从 30 秒起逐次翻倍，最长一小时。

本清单说明的是实施意向，而非时间表。Deskhub 由一人在业余时间维护。请以当前状态为准，
而非以计划为准。

## 报告漏洞

请**以非公开方式**报告安全问题，不要提交为公开的 GitHub issue。

- **邮件：** manhpv151090@gmail.com —— 请在标题中注明 `[Deskhub security]`。
- **或者：** 在 GitHub 上创建[非公开的 security
  advisory](https://github.com/manhpham90vn/Deskhub/security/advisories/new)。

请注明运行环境（OS、标题栏或 [`VERSION`](VERSION) 中的 Deskhub 版本）、执行的操作，以及
观察到的结果。提供 proof of concept 会很有帮助。

**处理流程：** 7 天内确认收到，30 天内给出评估。本项目由一名开发者在业余时间维护，请对
时间安排给予理解；无论结论如何都会给出明确答复。若修复发布，除非你不希望，你将被记入
release notes 的致谢。

本项目没有 bug bounty，也不支付任何报酬。

**上文已说明的内容不属于漏洞。** 上述列出的局限 —— 首次连接的未验证信任、流量分析、
beacon 回应探测、不具备抗 DoS 能力 —— 均为已知并已记录；重述其中任一项的报告不会提供
新的信息。*值得*报告的内容包括：可由畸形 packet 触发的 memory corruption 或 crash、
突破已记录 threat model 的方法、任何将数据泄出本机的途径，或某项缓解措施发布后其中存在
的缺陷。

## 支持的版本

仅支持 [Releases 页面](https://github.com/manhpham90vn/Deskhub/releases)上的最新
release。修复随新的 release 发布，不向旧版本 backport。

## 适用范围

本策略适用于本 repo 中的 Deskhub source，以及在 Releases 页面、TestFlight 与 Google
Play 上发布的 binary。不适用于 Tailscale、你的操作系统、路由器，或你同时使用的其他
软件。
