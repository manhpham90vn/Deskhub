[English](SECURITY.md) · [Tiếng Việt](SECURITY.vi.md) · **中文** · [日本語](SECURITY.ja.md)

# Deskhub 安全策略

_最后更新：2026 年 10 月 1 日_

本文件是 [`SECURITY.md`](SECURITY.md) 的译本；若两者有出入，以英文版为准。

## ⚠️ 请先阅读

**请在可信的 network 或 VPN 中使用 Deskhub。不要对 UDP 47777 做 port-forward，
也不要把正在共享的机器直接暴露在 Internet 上。**

Session 通过 QUIC/TLS 传输，包括 video、按键、mouse、clipboard 和 terminal 流量。
访问方式与 SSH 相同：client 只有在其 public key 列于 host 的 `authorized_keys` 中时才能
接入，并且必须证明自己持有对应的 private key。一把 key 只能通过 host 所有者的主动操作进入
该列表 —— 在该设备的 connection request 上按下 **Approve**、在共享时向该设备展示
**QR code**，或粘贴该设备的 public key。没有 passcode，也没有任何放未知机器进来的开关。
每台机器只有一把 key；client 会 pin 住 host 的 key —— 首次连接时在核对 fingerprint 之后，
或直接取自 QR code —— 并在发送任何内容之前进行检查；host 更换地址后信任依然保留，而在
已知地址上应答的另一把 key 会被视为从未见过的机器，并给出警告。host 不应答任何明文
packet：encrypt 连接之外的任何内容都会被丢弃。

每个 host 还可以 **view-only** 方式共享（input 被丢弃而非 inject）。

Encrypt 无法消除所有 network 风险：除非你核对 fingerprint，否则首次 Connect 某个 host
时会信任其出示的任何 key，app 也无法抵御 flooding。

远程访问请使用 VPN。本项目已使用 [Tailscale](https://tailscale.com) 测试；可以
Connect 到 host 的 `100.x.y.z` 地址。

请将 host 置于可信的 network 中，并在共享屏幕或 terminal 前阅读下文的限制。

## Threat model

### Deskhub 能够防护的内容

| | |
|---|---|
| 数据流向开发者 | 不存在此类数据。没有服务器、账号、telemetry，也没有收集数据的第三方 SDK。见 [`PRIVACY.zh.md`](PRIVACY.zh.md)。 |
| 流量被读取 | 每个 session 都运行在 QUIC/TLS 之内：video frame、按键、clipboard 文本与 terminal 字节在两台机器之间均为 encrypt 状态。抓包只能得到流量规模与时间信息，得不到内容。到达该 port 的未 encrypt packet 一律丢弃，且在 client 完成 authenticate 之前，host 不发送任何应用层内容 —— 包括其共享内容。 |
| 远端 viewer 争夺机器的控制权 | host 优先：一旦操作真实的 mouse 或 keyboard，remote input 即被暂停。Windows 与 macOS 的 host 开箱即用。Linux 的 host 仅在运行 Deskhub 的用户能够读取 `/dev/input/event*` 时才这样做 —— 实际上即属于 `input` 组，而安装包不会授予这一权限；缺少该权限时，日志会提示一次，remote input 永远不会被暂停。 |
| 按键滞留 | 远端仍处于按下状态的按键，会在 session 结束或 viewer 切换时自动释放。 |
| 陌生机器未经允许接入 | 只有 public key 位于 host 的 `authorized_keys` 中的 client 才能接入，且它必须用对应的 private key 对这条 connection 本身的 transcript 签名。一把 key 只能通过三种方式进入该列表，每一种都掌握在所有者手中：所有者在该设备的 connection request 上按下 **Approve** —— 该行显示设备的名称、它已证明持有的 key 的 fingerprint 及其地址；该设备扫描所有者在共享时展示的 QR code —— 其中随机的 32 字节 token 只能使用一次、有效 5 分钟，并在隐藏该码或停止共享时失效；或所有者粘贴该设备的 public key。陌生机器连接进来时不会被放行：只有在它用自己出示的 key 签名之后，host 才会记录一条所有者可以置之不理的 request，并在应答约两秒后由 host 自行关闭该 connection —— 对它拒绝的每一条 connection 都是如此。没有 passcode，`authorized_keys` 文件不存在即意味着任何人都无法接入，client 也没有任何办法绕过所有者。host 同时最多保留 8 个等待 authenticate 的 connection —— 每一个尚未完成 authenticate 的 connection 都计入，无论它出示的是什么 key，第九个不会被接受 —— 并在接受每个 connection 10 秒后，若其仍未完成 authenticate，即将其断开；最多保留 16 条 request，每个来源地址最多一条 —— 来自已有 request 的地址的新 request 会取代原有那条 —— 每条 10 分钟；同一 key 与地址在一分钟内出现 3 次无效签名，该组合将被封锁 10 秒，来自已签名设备的错误 QR token 也按其来源地址计入同一限制。等待批准不算作失败。在 host 的 Devices 页上移除某个 key，也会立即关闭该设备正在运行的 session。接入资格仅在赢得它的那条 connection 存续期间有效。 |
| 中间人攻击 | 每台机器只有一把 key。client 会 pin 住 host 的 key —— 首次连接时在用户核对 fingerprint 之后，或直接取自携带该 key 的 QR code —— 并在此后每次连接发送任何内容之前进行检查。信任跟随 key，因此换了地址的 host 仍是那个 host；而在 client 已知的地址上出现的*另一把* key 不会被当作受信任的 host，而是作为 **New host** 显示，并附带警告，指出以前在该地址应答的是哪台机器 —— 它不能被当作"变更"接受，只能在 fingerprint 展示在眼前的情况下重新信任。扫描 QR code 的 client 只有在应答的机器已通过 TLS handshake 证明其持有码中印出的 key 之后才会发送 token；该地址上的其他任何东西都得不到任何内容。client 的签名覆盖从 TLS session 导出的 session 标识以及 client 所见的 host key fingerprint，因此被 relay 到其他 host 或在其他 connection 上重放的签名无法通过验证。 |
| 多个 viewer 争夺 mouse | 最多 5 个 viewer 观看同一 host，但只有一个 viewer 控制 input：先加入者优先，后加入者的 input 会被丢弃，直到前者持续一秒无操作。第 6 个 viewer 以 `Busy` 状态被拒绝。 |
| 仅允许观看的 viewer | view-only 共享在所有 host 上可用，它在 host 侧、在任何操作被 inject 之前即丢弃 input packet，并不依赖 client 自觉遵守。Android 与 iOS 的 host 始终为 view-only。 |
| 手机在共享状态下被遗忘 | 最终的防护由操作系统提供，而非 Deskhub：Android 显示常驻通知，并在每次共享时重新征求录屏同意；iOS 保持 broadcast 指示可见。两者都可在不打开 app 的情况下停止共享。 |
| 允许的 client 向本机写入文件 | 只有已接受的机器才能发送文件，且仅在接收方启用 file transfer 时。到达的数据无法离开接收方选定的文件夹：线上的文件名会被截取为路径的最后一段，并在任何文件被打开之前清除分隔符、控制字节、filesystem 不接受的字符以及保留设备名。每个文件先以 `.deskhub-part` 后缀写入，仅在完整到达且 CRC-32 匹配后改名。已存在的同名文件会追加编号而不被覆盖。单个 batch 限制为 32 个文件、单文件 8 GiB、合计 32 GiB。同样的名称处理在手机与平板上也会执行，早于数据进入相册或 Downloads 文件夹。 |
| 畸形 packet | 每个字段在读取前都进行边界检查。parser 具备 unit test，在 CI 中于 AddressSanitizer、UndefinedBehaviorSanitizer 与 ThreadSanitizer 下运行，并由 libFuzzer 进行 fuzz —— 每个 pull request 对每个 target 运行 30 秒，每晚对每个 target 运行 15 分钟。共有九个 target，覆盖 wire format、H.264 解析（Annex B 与 SPS）、packet reassembly、host 与 viewer 两侧的 session state machine、UI 文案、terminal byte stream、保存的 key 与信任文件以及 pairing link，还有 QR code 编码器。通过 fuzzing 发现的 crash 会作为 regression test 保留在 repo 中，新的覆盖也会并入 seed corpus。 |

### Deskhub **不能**防护的内容

以下为完整清单，其中没有任何一项在当前已得到解决：

- **被保留的 shell 属于每一个允许的 client，而不属于打开它的那台机器。** 留在 host 上的
  shell 比打开它的连接活得更久，且不设时间限制；每一台已准入的机器都可以列出 host
  正在保留的 shell、reattach 其中已 detach 的一个，并关闭其中任意一个。每个 shell 的
  id、尺寸与设备名都包含在该列表中。因此你允许的第二个 client —— 或者你尚未在 Devices
  页面移除其 key 的机器 —— 可以读回先前 shell 正在做的事并在其中继续操作。请移除你
  不再信任的 key，并在用完后关闭 shell，而不是任其留存。
- **首次连接以未经验证的信任为前提。** 固定 host key 能够阻止*此后*出现的中间人：不同的
  key 就是不同的 host，对话框也会如此说明。但它无法阻止在首次接触时就已处于中间位置的
  攻击者，除非你按照 *New host* 对话框的要求，将其显示的 fingerprint 与 host 的 Devices
  页上显示的核对。扫描 host 的 QR code 会替你完成这一核对，因为码中携带了 fingerprint。
  `deskhub-cli` 会拒绝未知的 host，除非指定 `--accept-new-host-key`；你也可以用
  `host add … --host-key-stdin` 预先固定该 key。
- **批准了错误的 request 是所有者的失误，Deskhub 无法察觉。** 一条 connection request
  显示的是设备自己选定的名称、它所持有 key 的 fingerprint，以及它的来源地址。该设备已
  证明自己持有那把 key —— 只有在签名有效之后才会记录 request —— 但名称不能证明任何事，
  而且生成 key 没有任何成本。在按下 *Approve* 之前，请将 fingerprint 与该设备自己的
  Devices 页核对，并确认地址与你预期该设备所在的位置一致。network 上的任何人都可以留下
  一条 request；只有你能把它变成访问权限。
- **request 可能被挤掉，但无法从单一地址挤掉。** host 每个来源地址最多保留一条
  request，来自已有 request 的地址的新 request 会取代原有那条；总共最多 16 条，并丢弃
  最旧的一条以腾出位置。因此，一台机器不断换用新 key，只会覆盖它自己的那一行；要把一条
  真实的 request 挤出列表，需要 16 个不同的地址。与他人共用地址的设备 —— 例如位于同一个
  NAT 之后 —— 仍可能被对方的 request 取代。若你等待的 request 不见了，请让该设备重新连接。
- **QR code 在五分钟内对任何能看到屏幕的人而言都是一个秘密。** 它让一台设备无需在 host
  上点击任何东西即可接入。拍下它的人 —— 从你身后、从截图、从共享的屏幕 —— 都可以在它
  过期、被使用或被隐藏之前冒用你的身份。只向你打算放行的人展示它，并在对方连接后立即
  隐藏；被它准入的设备随后会出现在 *Devices allowed to connect to this machine* 下，若不是你预期的那台，
  可在那里移除。
- **`deskhub://` link 的可信程度取决于发送它的人。** host 在其 QR code 旁显示的 link
  可以在手机或平板上从任何网页、消息或 app 中打开（在 Android 上，只有 `deskhub://pair`
  link 会到达该 app）。以这种方式打开时，Deskhub 不会默默信任它：除非它所指的 host 已受
  信任，否则它会显示与首次连接相同的 *New host* 确认对话框，其中带有 link 所指定的
  fingerprint，只有在你接受之后才会连接。但该 fingerprint 来自 link 本身。若你接受了
  别人制作的 link，却没有将 fingerprint 与 host 的 Devices 页核对，你信任的可能是对方的
  机器：它会得到本应发给真正 host 的 pairing token，向你显示它想显示的任何屏幕，并收到
  你在该 session 中输入和发送的内容。用 app 内的扫描器扫描 QR code 时则不会询问而直接
  信任该码，因为你是把相机对准 host 自己的屏幕。
- **在 Linux 上，安装包扩大了可以 inject input 的范围。** remote control 需要
  `/dev/uinput`，因此 `.deb` 与 `.rpm` 会安装一条 udev 规则，将其开放给 `input` 组以及在
  座席上登录的用户（`MODE="0660", GROUP="input", TAG+="uaccess"`）。于是以该用户身份
  运行的任何程序都可以创建虚拟 keyboard 或 mouse，而不仅是 Deskhub。此外，host 优先所
  需的 `input` 组还能读取机器上的每一个 keyboard —— 以该组成员身份运行的任何程序都可以
  记录按键。只有在你接受这一点时，才将用户加入该组。
- **流量分析仍然可行。** Encrypt 隐藏的是内容而非存在：观察者可以得知有 session 正在
  运行、video 的流量规模，以及你输入的时间。
- **没有 rate limiting，也不具备抗 DoS 能力。** 向该 port 灌入大量数据会中断 session。
  对等待中的 authenticate、等待中的 request 与无效签名的限制只能阻止猜测，无法阻止
  flooding。
- **QUIC handshake 仍会应答。** host 不再回应任何明文 packet，但针对该 port 的 QUIC/TLS
  handshake 会在 client 证明任何内容之前完成，因此知道该地址的陌生人仍可得知有程序在
  监听，并看到 host 的 certificate。
- **设备名会被显示并记入日志。** client 发送的设备名在传输中已 encrypt，但会显示在 host
  的屏幕上、写入 host 的日志，出现在本机留下的每一条 connection request 中，并且是本机所
  复制的 public key 的 label，也是 host 在批准本机或通过 QR code 准入本机时所保存 key 的
  label —— 因此它会进入每个允许该 key 的 host 的 `authorized_keys`。host 也会将自己的设备名发送给每个已使用
  允许的 key 完成 authenticate 的 client —— 在此之前绝不发送 —— 该 client 会将其保存在
  最近列表中。其默认值为机器的 hostname，通常即为使用者
  的真实姓名。请在 Settings → General → *Device name* 中设置昵称，不要在其中填入敏感
  信息。清空该字段不会阻止名称被发送，只会恢复默认值。
- **viewer 名额在沉默 5 秒后自动释放。** 若你的 viewer 掉线，该名额将重新开放，下一个
  到达的 `Hello` 即可占用，只要发送方已使用允许的 key 通过 admission。
- **共享会暴露整块 display。** 不是单个窗口，而是该显示器上的每一条通知、每一个弹窗与
  每一个窗口。见 [`PRIVACY.zh.md` §3.4](PRIVACY.zh.md)。
- **手机或平板作为 host 时会暴露整台设备。** Android 与 iOS 同样可以作为 host，其推送
  的是整块屏幕：银行 app、一次性验证码、消息，以及共享期间输入的所有密码。该 stream
  与其他 session 一样经过 encrypt，但所有被接受的 viewer 都能看到全部内容。移动端 host
  始终为 view-only，这消除了被远程操作的风险，但并不降低信息暴露的风险。

## 密码学构件

各部分的构成如下，供需要核查设计的人参考：

- **设备 key。** 每台设备一把 NIST P-256 曲线上的 ECDSA key，首次运行时由 BoringSSL
  生成，保存在 `host_key.pem` 中。同一把 key 既在设备作为 host 时标识它，也在它作为
  client 时为它签名。其 fingerprint 是该 key 以 DER 编码的 SubjectPublicKeyInfo 的
  SHA-256，写作 `SHA256:` 后接不带填充的 base64。
- **Certificate。** host 为该 key 出示一张自签名的 X.509 v3 certificate：subject 与
  issuer 均为 `CN=deskhub`，随机的 63 位序列号，自 port 打开之时起有效 20 年，以
  ECDSA-SHA256 签名。它每次都重新生成，不含任何名称或地址。
- **传输。** 经由 Cloudflare 的 quiche 实现的 QUIC，其 TLS 1.3 来自 BoringSSL，ALPN 为
  `deskhub`。双方都不检查 certificate 链 —— 没有 certificate authority，因此关闭了
  peer 验证 —— client 转而将 certificate 中 public key 的 SHA-256 与它已 pin 住的
  fingerprint 比对，并在发送任何内容之前完成。
- **Client authenticate。** 两端用 TLS exporter 以 label `EXPORTER-Deskhub-Auth-v5`
  导出一个 32 字节的 session 标识。client 对一份 transcript 签名，其内容为固定的域字符串
  （`Deskhub/auth/signature`）、协议版本、它的角色、该 session 标识、它自己的 public key
  以及 host 的 key fingerprint；host 依据 `authorized_keys` 验证。Deskhub 自己的 key 以
  ECDSA P-256 与 SHA-256 签名。host 也接受粘贴的 `ssh-ed25519` public key，并验证用它
  生成的 Ed25519 签名；Deskhub 自身从不创建这种 key，因为 TLS 要求设备 key 为 P-256。
- **Pairing token。** 32 字节，取自操作系统的随机数生成器（Windows 上为
  `BCryptGenRandom`，Apple 平台上为 `arc4random_buf`，Linux 与 Android 上为
  `getrandom`，失败时回退到 `/dev/urandom`），以常数时间比较，只能使用一次，有效 5 分钟。
  host 同时最多保留 4 个有效 token，并丢弃最接近过期的那个，为新 token 腾出位置。

## 适合运行的环境

✅ **建议使用的环境**

- 你掌控全部设备的家庭或个人 LAN。
- 仅有你自己的设备加入的 Tailscale tailnet（或其他 WireGuard/VPN 隧道）。VPN 增加一层
  encrypt，并阻止陌生机器接触该 port。
- 仅作为 *client* 的机器（手机、平板、不共享屏幕的笔记本）。client 不接受入站 session。

❌ **应避免的环境**

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
运行，以便向提权窗口 inject input：每次启动时 UAC 都会询问，唯一的例外是
*Start Deskhub when you log in* 通过其 scheduled task 启动它 —— 该 task 在登录时以提权
方式运行它而不询问。它还会在共享时自动打开 firewall，添加一条名为 *Deskhub (host)* 的
入站 UDP 规则 —— 该规则覆盖整个 app 与所有 profile，因此收窄绑定并不会收窄 firewall；这一
便利之处也正是上述原则重要的原因。Deskhub 只替换它自己的这条规则。你自己为该 app 创建的
block 规则会保持不变；由于 Windows 中 block 规则优先于 allow 规则，host 在你移除该规则之前
将一直无法被访问。

## 同一 network 中攻击者的能力

若有人与正在共享屏幕、且 Deskhub 正在运行的机器处于同一 LAN，他们可以：

1. 逐个地址对 UDP 47777 尝试 QUIC handshake 以找到该机器。明文 packet 不会得到任何
   应答，但 handshake 本身会，因此该机器在有针对性的扫描下仍会暴露。
2. 尝试接入 —— 这需要一个 private key，且其 public 部分已在 `authorized_keys` 中，而只有
   所有者的 *Approve*、一个有效的 QR token 或一次粘贴才能把它放进去。陌生人*能*做的是留下
   一条 connection request，以他们自选的名称和刚生成的 key 出现在所有者的 Host 页上；它 10 分钟后过期，
   每个地址同一时间只保留一条 request —— 因此一台机器换用新 key 只会覆盖它自己的那一行 ——
   只有 *Approve* 才能把它变成访问权限。他们可以尝试猜测 QR token —— 每次猜测都要用自己的 key 签名 —— 但错误的 token 会像
   无效签名一样计入其地址 —— 一分钟内 3 次即被封锁 10 秒 —— 而 token 是 32 个随机字节，
   有效 5 分钟且只能使用一次。除此之外，他们最多只能尝试在某个 client *首次*连接 host 时
   处于中间位置，而 fingerprint 核对 —— 或 QR code 中携带的 fingerprint —— 可以发现这一点。
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
- [ ] 在 host 上仅允许你需要的 client key，并在每个 client 首次连接时核对 host key 的
      fingerprint。定期检查 Devices 页并移除不再认可的 key。仅需观看时，取消勾选 *Viewers can control this machine*。
- [ ] 只批准你预期中的 connection request，并在批准前核对该行中的 fingerprint 与地址。
      其余的请 Deny 或置之不理 —— 它们会自行过期。
- [ ] 你展示 QR code 的那台设备一旦连接，就立即隐藏该码；绝不要在正在共享或演示的屏幕上
      展示它。
- [ ] 不使用时退出 Deskhub。它不是 background service，退出即关闭了接入点。但
      *Launch & background* 下的三项设置会改变这一点：*Start Deskhub when you log in*
      会在每次登录时启动它（在 Windows 上通过一个以提权方式运行、不弹出 UAC 的
      scheduled task），*Start sharing when Deskhub opens* 随即开始共享，而
      *Keep running in the background (tray icon) when the window is closed* 使关闭窗口
      不再退出程序。三者结合，会让 host 从你登录那一刻起就在监听，屏幕上却没有任何窗口。
      除非你确实需要这样，否则请关闭它们；出现 tray 图标时，请从 tray 图标退出。
- [ ] 在 Linux 上，为了 host 优先而把自己加入 `input` 组之前请三思：这也会让你运行的
      每个程序都能读取每一个 keyboard。
- [ ] 在 Linux 上使用 `ufw` 时，请收窄规则而非全面放行：
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp`，而不是
      `sudo ufw allow 47777/udp`。
- [ ] 不要在会被带往其他 network 的笔记本上保持共享运行。
- [ ] 离开时锁定机器，避免无人看管的 session 被接管。
- [ ] 使用 `deskhub-cli` 时，以 `key public` 显示本机的 public key，以 `host-key public`
      显示本机作为 host 的 key —— 两者是同一把 key。通过可信渠道传递它，然后在 host 上使用
      `access add --stdin`，在 client 上使用 `host add … --host-key-stdin`。要在 terminal
      中批准一条 request，请查看 `access requests`，并仅对你预期的 fingerprint 以
      `access approve --fingerprint SHA256:…` 作答。

## 本地保存的数据

桌面 app 以纯文本将诊断日志写入 `~/.deskhub/`（Windows 上为 `%USERPROFILE%\.deskhub`），
每次运行一个文件，命名为 `deskhub-<date>-<time>-<pid>.log`；在 macOS 与 Linux 上，
`deskhub-latest.log` symlink 指向最新的一个。在 macOS 与 Linux 上，每个日志创建时仅你可读
（`0600`），且绝不通过 symlink 写入；在 Windows 上，它继承所在文件夹的受限权限。较旧的
日志会被删除，只保留最新的十个。日志包含连接统计（`[DIAG]` 行：bitrate、丢包、延迟、
frame 耗时）、peer 的地址与 port、peer 发送的设备名、key fingerprint、每个发送或接收的
文件的名称、大小、结果及其存放的文件夹，以及错误信息。日志不包含屏幕内容、viewer 按下的
按键 —— 无论是在 remote control session 中还是在 terminal 中 —— 或指针的移动、clipboard
文本或 terminal 字节。Android 与 iOS 只把同样的内容写入系统日志
（logcat、Xcode console），从不写入文件。

桌面 app 与 `deskhub-cli` 共用该文件夹中的其余内容；`DESKHUB_CONFIG_DIR` 或 CLI 的
`--config-dir` 可让两者将这些文件改放到另一个文件夹：`ui-settings.txt`（fps、bitrate、
分辨率上限、port、view-only 开关、设备名、bind 地址以及其他开关）、`recent-hosts.txt`
（最近连接的 10 个 host —— 地址、时间以及每个 host 自报的名称）、`host_key.pem`（本机唯一的
私钥 —— 它在两种角色下都是 fingerprint 背后的 identity；取得它的人既可冒充本机作为 host，
*也*可在允许本机的任何地方登录）、`authorized_keys`（允许接入本 host 的 client public key，
各带其 label）、`known_hosts`（本机信任的 host —— 固定的 fingerprint、首次与最后一次见到它
的时间、它最后一次应答的地址与 port，以及它的名称）、`access_requests`（等待你 *Approve*
的设备 —— 名称、public key、地址与时间；最多 16 条，每条 10 分钟后丢弃）、`pairing_tokens`
（当前有效的 QR token 及各自的过期时间 —— 在过期之前，该文件的副本与屏幕上的码同样有效），
以及 Linux 上的 `portal-restore-token.txt`（桌面针对所选屏幕签发的 token，仅对你的桌面
session 有意义，不会被传输）。在它们旁边，你可能还会看到 `authorized_keys`、`known_hosts`、
`access_requests` 与 `pairing_tokens` 各自的 `.lock` 文件 —— 它让 app 与 CLI 轮流写入 ——
以及短暂存在的 `.tmp-…` 文件，即原子写入的中间文件。

不保存任何 certificate。TLS certificate 在每次打开 port 时由 `host_key.pem` 于内存中构建，
但 TLS 库只能从文件加载 certificate，因此它会被短暂写入同一文件夹中的
`transport_cert.<random>.pem`，以 `0600` 创建，并在加载后立即删除；任何超过一分钟的此类
文件 —— 只有 crash 才会留下 —— 都会在本机下次开始 hosting 时被清除。它只包含公开的
certificate：TLS 库从 `host_key.pem` 读取 private key。

任何地方都不保存 passcode。在 POSIX 系统上，该文件夹以 `0700` 创建，其中每个文件为
`0600`，并以原子方式写入；在 Windows 上该文件夹仅限你的用户、SYSTEM 与 Administrators
访问，其中的文件继承这一限制。若
主目录无法使用，POSIX 版本会回退到 `$TMPDIR/.deskhub`，再回退到当前目录下的 `.deskhub`。
Debug 构建使用 `.deskhub-dev` 而非 `.deskhub`，因此绝不会触及 release 构建的 key。移动端
app 将相同的文件保存在自身沙箱中 —— iOS 上为 app group 容器内的 `.deskhub` 文件夹，由 app
与其 broadcast extension 共用，并被排除在 iCloud 与电脑备份之外；Android 上为 app 的内部
存储，且整个 app 关闭了备份。桌面上的文件夹没有这种排除：复制你主目录的备份工具也会一并
复制 `host_key.pem`。请将该文件夹视为以你的身份运行的任何程序均可读取的内容。

无法读取的 `authorized_keys` 或 `known_hosts` 文件不会被猜测：在其无法读取期间，host
不允许任何人接入，client 拒绝所有 host，下一次更改会重新写入该文件。旧版本的数据不会被
转换：旧的已 pair 机器列表、旧的激活标记以及旧的 `auth_salt` 文件会被删除，旧
`ui-settings.txt` 中的 `passcode=` 行会在首次加载该文件时被移除。7.0.x 写入的
`client_key*.pem` 与 `host_cert.pem` 会被忽略，绝不读取；你可以随意删除它们。

其他机器发送的文件保存在该文件夹之外，位于接收方选定的目录（未另行选择时为用户主目录下
的 `Deskhub`，以 `transfer_dir` 保存）。在手机或平板上，这些文件位于设备的相册或
Documents / Downloads 文件夹，卸载 app 后仍会保留。请将送达该处的任何内容视为由允许的
client 放入你设备的文件。

上述内容不会被上传；你可以随时删除该文件夹。

## 构建与发布加固

- **编译器与链接器。** C 与 C++ 代码的 GCC 与 Clang 构建使用 `-fstack-protector-strong`，
  优化构建还使用 `-D_FORTIFY_SOURCE=3` —— Android 除外，它保留 NDK 自身的级别 2；只有
  Linux binary 以完整 RELRO（`-z relro -z now`）链接。MSVC 构建使用 `/sdl`。这些选项位于
  `cmake/DeskhubHardening.cmake`。
- **macOS。** app 以 hardened runtime 签名，但运行在 **App Sandbox 之外**
  （`com.apple.security.app-sandbox` 为 `false`）：沙箱中的 app 无法向其他 app 发送
  keyboard 与 mouse 事件，而这正是 remote control。因此它与任何未沙箱化的 app 一样，可以
  访问你的用户账户能触及的一切。
- **Windows。** app 在每次启动时都请求管理员权限（见上文），因此它的缺陷就是一个提权进程的
  缺陷。
- **持续集成。** 每个 pull request 都会在 AddressSanitizer、UndefinedBehaviorSanitizer 与
  ThreadSanitizer 下运行三套测试，并运行 clang-tidy、上文所述的 fuzzer、针对 C++、Kotlin
  与 Swift 代码的 CodeQL，以及用 gitleaks 扫描整个 Git 历史中提交的机密；CodeQL 与
  gitleaks 还会在每次 push 到 `main` 时以及每周运行。引入存在已知高危漏洞的依赖的 pull
  request 无法通过其 dependency review。第三方 GitHub Actions 固定到 commit hash，workflow
  下载的 linter 与扫描工具会与固定的 SHA-256 校验和比对。

## 计划中的缓解措施

按计划实施顺序列出：

1. **将机器 key 保存在操作系统的 keychain 中**，而非文件中。

自本清单上次修订以来已完成的事项：SSH 式的访问控制 —— 仅接受 public key 列于 host 的
`authorized_keys` 中的 client，每条 connection 重新签名；移除 passcode 与 pairing 开关；
移除 LAN discovery，使 host 完全不应答明文 packet；对等待中的 authenticate 与无效签名的
限制；随后在 8.0 中：每台机器一把 key，不再保存 certificate；信任跟随 host 的 key 而非
地址（key 变化时的硬性拒绝变为一个指出先前所有者的 *New host* 对话框）；所有者通过已
authenticate 的通道按 fingerprint 批准的 connection request；以及 QR pairing —— 其一次性
token 只会被 client 发送给码中 fingerprint 所指的那台机器；随后在 9.0 中：仅在请求方签名
之后才记录 connection request，且每个来源地址最多一条；host 自行关闭被拒绝的 connection；为每一个尚未 authenticate
的 connection 设置期限；从 app 外部打开的 link 在被信任之前先显示 *New host* 确认；设备 key
不进入 iOS 备份；Windows 日志中不再有来自 remote control 或 terminal 的按键码；日志文件
仅所有者可读并会被清理到只剩最新的十个；firewall 规则不再删除你创建的 block 规则；
以及加固的编译器与链接器选项。

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

上文已记录首次连接的信任限制、流量分析、可见的 QUIC handshake 和缺乏 DoS 防护等情况。
如果你掌握了有关其影响的新证据，请报告。也请报告其他安全问题，例如畸形 packet
引发的 memory corruption 或 crash、数据意外离开设备，或已发布缓解措施中的缺陷。

## 支持的版本

仅支持 [Releases 页面](https://github.com/manhpham90vn/Deskhub/releases)上的最新
release。修复随新的 release 发布，不向旧版本 backport。

## 适用范围

本策略适用于本 repo 中的 Deskhub source，以及在 Releases 页面、TestFlight 与 Google
Play 上发布的 binary。不适用于 Tailscale、你的操作系统、路由器，或你同时使用的其他
软件。
