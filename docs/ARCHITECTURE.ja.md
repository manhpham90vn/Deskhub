[English](ARCHITECTURE.md) · [Tiếng Việt](ARCHITECTURE.vi.md) · [中文](ARCHITECTURE.zh.md) · **日本語**

# Deskhub — Architecture

この文書は Deskhub のコードを変更する人向けに、layer、process、thread、wire
protocol と、その設計判断を説明する。製品の挙動は
[`SPECIFICATION.ja.md`](SPECIFICATION.ja.md)、セキュリティ上の境界は
[`SECURITY.ja.md`](../SECURITY.ja.md) を参照。

本書は [`ARCHITECTURE.md`](ARCHITECTURE.md) の翻訳。食い違いがある場合は英語版が正文。

- **状態:** 現在のコードを記述している。
- **読者:** 実装に携わるコントリビュータ。

---

## 1. Layer

構成の原則は一つ。共通のロジックを一度だけ書き、すべての client から使う。

```
core/       純粋な C++20。OS ヘッダとサードパーティコードを含まず、オフラインで unit test
platform/   OS 向けの薄い abstraction。ヘッダごとに同一の API を提供する（core に依存）
client/     OS ごとの app: windows、linux、macos、ios、android（platform と core に依存）
            加えて client/cli、デスクトップ 3 種向けの command line client
```

| Layer | 内容 |
| --- | --- |
| `core/protocol` | Wire format（`Wire.h`）、stream の record framing（`RecordStream.h`）、QUIC とそれ以外を判別する packet classifier |
| `core/transport` | video 向けの Packetizer/Reassembler、FEC、retransmit キャッシュ、send pacer、audio の jitter buffer（`AudioJitterBuffer`） |
| `core/session` | session state machine を役割ごとに分割: `session/host`（viewer ごとの session、viewer 表、viewer feedback、source pipeline の状態、share の流れ、`SourceListResponder`、file receiver）、`session/client`（screen client、file sender、terminal client、connect の流れ、開いている viewer）、およびそれらの隣に置いた共有部品（transfer の型、terminal session 表、clipboard sync、link pulse と link recovery） |
| `core/control` | Bitrate controller、quality ladder、stream のサイズ決定、clock offset、fps の gate（`FrameGate`）、link の統計（`LinkStats`）、表示の pacer（`VideoPacer`） |
| `core/media` | すべての capture、encoder、decoder が従う media の契約（`VideoContract`、`CaptureContract`、`AudioContract`）、H.264 の Annex B と SPS の解析、rate の計画、RGB の downscaler、最新優先の `FrameMailbox`、PCM ring、表示の fit と share の型 |
| `core/input` | 両端の input: client の input キューと sender、host の receiver と applier、key と pointer の mapping、scancode 表、hotkey、pointer lock、trackpad の cursor |
| `core/terminal` | すべての client が共有する VT emulator: `VtParser`、`Screen`、`KeyEncoder`、`Palette`、および repaint、snapshot、scroll の anchor |
| `core/transfer` | ファイル転送の基本部品: `Crc32` と `SafeFileName` |
| `core/diag` | host と viewer の status 行の背後にある診断テキスト（`ShareDiag`、`ScreenClientDiag`、`WindowStat`） |
| `core/net` | fingerprint を key とする trust store（client 側）、authorized keys（host 側）、待機中の接続要求（`AccessRequests`）、`deskhub://pair/` の招待レコード（`PairingInvite`）、OpenSSH public key のテキスト、すべての呼び出し元が使う唯一の `Base64`、IPv4 の解析、bind アドレスの選択 |
| `core/auth` | 署名対象の auth transcript（`Transcript`）、key とアドレスごとの失敗の limiter（`AuthFailureLimiter`）、1 回限りの QR token（`PairingTokens`） |
| `core/qr` | `QrCode` —— すべての client と CLI が pairing code を描くのに用いる QR encoder |
| `core/ui` | 利用者に表示されるすべての文字列、settings の解析と layout、配色 theme、host 表の行、shell picker、transfer の表示、autostart と自動共有の規則、最近のデバイスと host profile（`HostProfiles`）。5 つの client が同一の内容を表示するためのもの |
| `core/cli` | command line の文法とその JSON writer。入力は平文、出力は検証済みの command |
| `platform/net` | `UdpSocket`（OS ごとの実装）、`QuicEndpoint`（quiche を pimpl の背後に配置）、`SessionTransport`、`NetInfo`（本マシン自身のアドレス） |
| `platform/auth` | `AuthNegotiation` —— 双方が用いる唯一の key 署名 handshake。host 側の結果を持つ（3 節） |
| `platform/client` | `HostLink`（dial、trust、auth、承認待ち、channel。すべての画面が共有）、`ScreenViewer` とその loop、`TerminalViewer` と `TerminalFeed`、`FileTransferClient` と `FileUpload`、`SourceQuery`（ブロッキング版と非同期版）、`HostProfiles`（fingerprint を key とする信頼済み host。名前と最後のアドレスを伴う） |
| `platform/host` | `HostEngine`、`HostNetLoop`、`SharingHost`、`ShareController`、`ShareDriver`、`TerminalHost`、`FileHost`、`ViewerBroadcast`、`PairingInvite`（token を発行し、この host が表示する招待を構築する） |
| `platform/audio` | audio の capture（PipeWire、WASAPI）と sink（PipeWire、WASAPI、Core Audio、AAudio）、host 側の `AudioBroadcaster` と viewer 側の `AudioPlayer` |
| `platform/media` | Opus codec、ディスプレイの列挙、Linux の xdg portal ScreenCast session、両 Apple app が使う VideoToolbox の encoder、decoder、source pipeline |
| `platform/input` | 「host 優先」のためのローカル input の監視（`LocalInputMonitor`）と OS ごとのネイティブ key code の mapping |
| `platform/diag` | `LOGI`/`LOGW`/`LOGE` と実行ごとの log ファイル |
| `platform/system` | Clock、random、PTY（ConPTY / forkpty）、machine key（`HostIdentity`）とそれによる署名（`AuthProof`）、`authorized_keys` と `known_hosts` のファイル、待機中の要求（`AccessRequestsFile`）と有効な QR token（`PairingTokenFile`）、最近の一覧（`RecentDevicesFile`）、settings の保存、config フォルダとそのファイルロック、受信ファイルの保存（`FileStore`）、デバイス名、autostart、keep-awake、console、環境変数、メモリ使用量 |
| `platform/ffi` | Swift と Kotlin の app が呼び出す C の surface: `ClientFfi`（文字列、theme の色、key の mapping、アドレスの解析、source の問い合わせ、自動共有の step）、`SettingsFfi`（settings、デバイス名）、`DevicesFfi`（最近のデバイス、許可済み client、接続要求、本マシンの fingerprint と public key）、`HostProfileFfi`（信頼済み host）、`PairingFfi`（招待、QR モジュール、無効化）、および share、screen、terminal、send の各 surface |
| `client/<os>` | Capture、encode、decode、render、windowing、ダイアログ。protocol に関する要素は含まない |
| `client/cli` | flag から session まで: GUI toolkit なしで host、connect、shell の起動を行う binary 1 つ。デスクトップ app と同じ OS ごとの media ライブラリを link する |

`core/` は network と GPU なしでオフラインにテストできる状態を維持する。`platform/` は
OS を利用してよいが、すべてのプラットフォームで同一の API を提供しなければならない。
同じコードが 2 つの client に現れる場合、それはより下位の layer に属する。

## 2. 一つの port、一つの transport

host が提供するすべての機能は、単一の `QuicEndpoint` を包む `SessionTransport` を介して
**一つの UDP port**（既定 47777）上で動作する。

```
                      UDP port 47777
                            |
                 ClassifyPacket（先頭バイトを判定）
                   /                    \
            QUIC packet             それ以外
                 |                        |
   +-------------+------------+        破棄: 平文では何にも
   |             |            |        応答しない
 stream      datagram      (TLS)
   |             |
 control      video
 input        audio       stream は framing された record（RecordStream）を運ぶ。
 clipboard                length prefix 付きの message で、最大 16 KiB。
 terminal                 datagram は video または audio の packet を
 file                     1 つ運ぶ（≤ 1200 B）。
```

- **Stream**（信頼性あり、順序保証あり）: control、input、clipboard、terminal、file。
  connection ごとに bidirectional stream を 2 本使用し、どちらも client が開く。stream 0
  （`kQuicControlStream`）は control、auth、input、clipboard、terminal の record を
  interactive の urgency で運び、stream 8（`kQuicFileStream`）はファイル転送を bulk の
  urgency で運ぶ。したがって upload がキー入力より先に並ぶことはない。ある
  connection で滞留した stream が別の connection を妨げることはない。受信した stream の
  データは service 1 巡あたり 64 KiB の予算で処理する。データを消費する側、主に
  terminal の VT emulation は、分割処理の合間に制御を ACK、keepalive、timeout 処理へ
  戻すため、terminal の大量出力によって connection が idle timeout で閉じられることは
  なくなった。
- **Datagram**（信頼性なし、順序保証なし、ただし encrypt 済み）: video と audio の
  packet。QUIC は失われた packet を再送しない。video については app 自身の FEC/NACK の
  機構が損失を扱い、audio については Opus の in-band FEC と concealment が扱う —— 9 節を
  参照。
- **生の UDP** には一切応答しない。discovery は存在しない。QUIC でない受信 packet は
  session のコードに届く前に破棄され、`SourceListResponder` は authenticate 済みの
  connection に対してのみ `LIST_SOURCES` と session-0 の `PING` に応答する。平文の UDP
  を使う唯一の経路は送信側の fallback である。そのアドレスへの QUIC connection が確立
  していないとき、`SessionTransport::SendTo` は message を `QuicEndpoint::SendRaw` に
  渡す。host がこの経路を通ることはない。authenticate していない peer には何も送らない
  ためである。

`QuicEndpoint` は quiche を完全に隠蔽する（pimpl。`QuicEndpointNone.cpp` が stub を
提供するが、これは build が明示的に `-DDESKHUB_QUIC=OFF` を指定した場合に限られる。
quiche がない場合は configure が失敗する。stub の binary では share も connect も
できないためである）。connection は peer のアドレスで識別し、connection migration は
行わない。規約上、quiche の connection は single-threaded であるため、endpoint への
操作はすべて transport の send mutex の下で行う。transport は、ブロックする socket の
待機をまたいでこの mutex を保持しない。まずロックせずに `WaitReadable` を実行し、その
後にロックして短い `Poll` を行う。client の auth 待ちである `RunClientAuth` も例外では
ない。待機中に mutex を保持すると、すべての送信側が停止する。

## 3. 受け入れの判定: SSH と同様の key

各マシンは初回起動時に ECDSA P-256 の key を 1 つ生成し（`HostIdentity`、
`host_key.pem`）、自動的に置き換えることはない。この 1 つの key が両方の役割における
マシンそのものである。host は TLS を通じてこれを提示し、client はこれで sign in する。
DER SubjectPublicKeyInfo の SHA-256 ハッシュが、Devices ページ、QR code、接続要求の一覧、
`authorized_keys`、`known_hosts` のいずれでも利用者に表示される唯一の fingerprint である。
TLS には X.509 certificate が必要なため、port を開くたびに `HostIdentity` がこの key を
中心に自己署名 certificate を**メモリ上で**構築する。quiche は certificate をファイル
からしか読み込まないため、endpoint はそれを `transport_cert.<random>.pem` として一瞬
だけ書き出し（中身は公開 certificate のみで、`0700` の config フォルダ内に `0600` で
作成。private key は quiche が `host_key.pem` から読む）、quiche にパスを渡した直後に
削除する。hosting の開始時に、1 分より古い `transport_cert.*` ファイル（書きかけの
`.tmp-` ファイルを含む）—— crash だけが残すもの —— は一掃される。fingerprint は certificate ではなく SPKI をハッシュしたものなので、起動ごとに新しい
certificate になっても誰かが固定した内容は何も変わらず、旧バージョンが保存していた
`host_cert.pem` は読み取られることも必要とされることもない。host は `authorized_keys`
（`AuthorizedKeys`。`ecdsa-sha2-nistp256 AAAA… label` の行を最大 128 行 —— 手で貼り
付けた key のために Ed25519 の行も引き続き解析する）に記載された public key のみを
受け入れる。label は表示名であり、権限を意味することはない。

TLS の上位では、アプリケーション層の handshake（`AuthNegotiation`、auth version 8）が
connection ごとに受け入れの可否を決定する。transport がこれを実行し、auth が完了して
いない connection に対して host はアプリケーション層のデータを一切送信しない。

1. QUIC/TLS が完了する。client は**何かを送信する前に**、host の key に対する trust を
   確定する（`HostLink::SettleTrust`。後述）。
2. client は `AuthStart` を送信する。
   `00 | u16 keyLen | key | u8 nameLen | name | u8 tokenLen | token | 08` —— 自身の public key、デバイス名、QR code 経由で来た場合は
   32 バイトの pairing token（`tokenLen` は 0 または 32）、最後に auth version。
3. `HostAuth::Begin` は `AuthChallenge` で応答する。これは許可済み、未知、token 付きの
   いずれの key に対しても `Signature` であり、`authorized_keys` を読み取れない場合に
   限り `ConfigError` となる。未知の key については、その署名が検証されるまで何も書き
   込まない（手順 4）。limiter が既にブロックしている送信元アドレスから token 付きの
   `AuthStart` が来た場合は、challenge を返す前に直ちに閉じる。
4. client は transcript —— ドメインのラベル、auth version、役割、この QUIC/TLS
   connection から export した session の値、自身の public key、host の TLS fingerprint
   （`core/auth/Transcript`）—— に署名し、host はそれを当該 key で検証する。検証に失敗
   した署名は `BadSignature` で終わる。検証に成功すると、`AuthResult` は次の順で決まる。
   - key が `authorized_keys` にある → `Accepted`。
   - `pairing_tokens` の有効なエントリに一致する token がある → token を消費し、key を
     client の名前をラベルとして `authorized_keys` に追記し、connection は `Accepted`。
   - それ以外 → 接続要求 —— 時刻、送信元アドレス、client の名前をラベルとした public
     key —— を `access_requests` に書き込み、結果は `AwaitingApproval`（要求を記録
     できない場合は `NotPaired`）。

   送られたがどれにも一致しなかった token は、署名が検証された今になって初めて、送信元
   アドレスへの失敗 1 回として課され（1 分に 3 回で 10 秒間ブロック）、以降は token を
   伴わない要求として扱われる。`AwaitingApproval` は client 側の判定ではなく、実際の
   wire 上の結果（code 9）である。

`NotPaired`、`AwaitingApproval`、`BadSignature`、または `ConfigError` の challenge の後、
host は応答から約 2 秒後に自ら connection を閉じる（`kRefusalLingerUs`、QUIC
application error 8）。これは応答が届くのに十分な時間である。client も自分の側を閉じる。
host はクリックを待つ未 authenticate の接続を保持しない。

署名は 1 本の connection に束縛されるため、再接続の際には改めて署名する。0-RTT や
session resumption は存在しない。authenticate していない connection はすべて、どの key
を提示したか、`AuthStart` を送ったかどうかに関係なく、QUIC が受け入れた時点から 10 秒の
猶予を与えられ、host が同時に保持するのは最大 8 本である —— `core/auth/AuthDeadlines`
が期限を保持し、`QuicCallbacks::admitConnection` は 9 本目の connection を受け入れる前に
断り、`onAbandoned` は announce される前に終わった connection の枠を解放する。1 つの key と
送信元 IP から 1 分以内に 3 回失敗すると、その組み合わせは 10 秒間ブロックされる
（`AuthFailureLimiter`）。`AwaitingApproval` と `ConfigError` は失敗に数えない。
`access_requests` は最大 16 件の要求を、key ごとに 1 件、送信元アドレスごとに 1 件まで
—— 新しい要求は key が同じ行と IP アドレスが同じ行（port は無視）をすべて置き換えるので、
再要求はアドレスと時刻を更新する —— 16 件が埋まっていれば最も古いものを破棄して、それぞれ
10 分間保持する。*Approve* は key をデバイスの名前とともに
`authorized_keys` へ移し、*Deny* は行を削除して client には何も伝えない。

受け入れは 1 本の QUIC connection に属するものであり、アドレスに属するものではない。その
connection が閉じた時点で受け入れは取り消されるため、同じアドレスと port からの次の
connection は改めて証明を行う必要がある。handshake を開始済みの connection で 2 回目の
`AuthStart` を送ると、その connection は閉じられる。確立済みの身元を証明されていない身元に
差し替えることはできず、拒否された key を同じ connection 上で再試行することもできない。
Devices ページで client key を削除する（または `access remove`）と、その時点でその key が
開いている connection も閉じられる。

client 側では `known_hosts`（`TrustStore`）が host の **fingerprint** を key とし、各
エントリは host の名前、最後に応答したアドレス、初回・最終確認時刻を保持する
（`HostProfiles`）。`HostLink::SettleTrust` は TLS が確立した時点で、相手が提示した key の
fingerprint に対して一度実行される。

- QR 招待から dial した場合: fingerprint は招待内のものと一致しなければならない。
  一致は、応答したマシンが code を作ったマシンの private key を保持していることを意味
  するため、host は黙って固定され、token が `AuthStart` で送られる。不一致は、その
  アドレスで別の何かが応答していることを意味する。link は招待不一致の文言
  （`kInviteHostMismatch`。source の問い合わせでは `SourceQueryFailure::InviteMismatch`
  として報告される）で失敗し、token は client から出ない。app の外から開かれた
  `deskhub://` リンク（`deskhub://pair` のみに一致する Android の intent、または iOS の
  `openURL`）は黙って固定されない。リンク内の host key がまだ信頼されていない場合、app は
  まず fingerprint を示した *New host* の確認を表示し、*Trust and connect* の後にのみ
  dial する。既に信頼済みの host にはそのまま dial する。app 内でスキャンした code は
  従来どおり直接固定される。
- 既に `known_hosts` にある場合: エントリの最後のアドレスを更新し（`TouchTrustedHost`）、
  link は先へ進む —— host にどのアドレスで到達しても構わない。アドレスを key とする
  ものはもう存在しないためである。
- それ以外の場合: link は fingerprint を添えて *not trusted yet* として失敗する。app は
  *New host* ダイアログを表示し、*Trust and connect* の後に `acceptNewHostKey` を付けて
  再接続する。CLI は `--accept-new-host-key` を指定した場合に限り同じ動作をする。
  `SettleTrust` 自体は警告を計算しない。ダイアログを描く前に app が
  `PreviousOwnerWarningFor`（`FindByEndpoint` を利用）に、そのアドレスが以前別の信頼済み
  host として応答していたかを問い合わせ、そうであればその host の名前と fingerprint を
  プロンプトに加える。
  *changed key* という判定は存在しない。古いアドレスの新しい key は新しい host である。

応答が `AwaitingApproval` の場合、`HostLink` は同名の状態で待機し、
`AwaitingApprovalLine` を表示し、recovery 中の link が既に用いている backoff で、最長
`kDefaultApprovalWaitUs`（120 秒）または呼び出し元がキャンセルするまで再接続する。各
再接続は完全な connection と新しい `AuthStart` であるため、所有者の *Approve* の後の
最初の 1 回が受け入れられる。期限を過ぎると link は
`AuthResultCode::AwaitingApproval` で失敗し、その文言は利用者に *Approve* を求めて再度
接続するよう伝える。

ネットワーク上を流れるのは public key そのものであり、fingerprint 単体ではない。host は
受け取った内容を自身でハッシュするため、他者の identity を名乗るには、なりすます側が
保持していない key で署名する必要がある。また、受け入れの判定は connection ごとに 1 度
だけ行われるため、transport より上位の構成要素が再度確認することはない。session の
コードは connection 全体を authenticate 済みとして扱う。

## 4. Host 側

```
HostEngine（app ごとに 1 インスタンス、SessionTransport を保持）
 ├─ net-loop thread: RunHostNetLoop
 │    recv → source 一覧/pong の応答（受け入れ済みのみ） | video データの取り込み
 │         | Chan::Terminal → TerminalHost | Chan::File → FileHost
 │    source ごとの session Tick、clipboard flush、reconfig、統計
 ├─ capture/encode: source ごと。OS の capture コールバックが駆動する（client 層）
 │    frame → encoder（source ごとの mutex）→ Packetizer → FEC → SendTo（datagram）
 ├─ audio worker: capture コールバック → lock-free な frame ring → Opus encode →
 │    viewer ごとの datagram（AudioBroadcaster）
 ├─ TerminalHost（terminal が共有されている場合にのみ存在）
 │    ├─ net-loop thread 上で HandleMessage: TERM_OPEN/DATA/RESIZE/CLOSE/LIST → PTY
 │    └─ pump thread: PTY 出力 → host 側の Screen mirror と TERM_DATA record、
 │         peer 喪失時の切り離し、kicks
 └─ FileHost（ファイルを受け付けている場合にのみ存在）
      └─ net-loop thread 上で HandleMessage: FileReceiver → FileStore。応答はキューに
           入れ、自身のロックを解放した後に stream 8 で送る
```

- 何らかの内容が共有されている限り、engine は動作する。screen source がない場合でも、
  terminal またはファイル共有が有効であれば engine は source を持たない状態で動作する。
  どちらかが要求されているか、その `TerminalHost` または `FileHost` がまだ動作している
  限り、ループも継続する。
- 各 screen source は `SourcePipelineState` に対応し、それぞれ独自の
  `ScreenHostSession`（viewer 表、negotiation、input の調停）、encoder、quality ladder、
  診断情報を持つ。1 回の encode がその source のすべての viewer に供給される。
- フィードバックの経路: viewer は `Feedback`（loss、RTT、受信レート）を毎秒送信し、
  host は自身の信号として、frame が送信段に到達した時点での経過時間を加える。これは
  `enc_lat_ms` が報告する量と同一である。`BitrateController`（AIMD）はそのうち 2 つ ——
  loss と frame の経過時間 —— に基づいて動作し、`QualityLadder` はそれが選んだ bitrate
  に従って解像度と fps を下げる。RTT と受信レートは表示に使うだけである。FEC は最初の frame から有効
  であり、損失のない状態が長く続いた場合にのみ無効化する。FEC が対処する損失は最初の
  レポートより前に現れるためである。滞留状態では FEC を有効化しない。parity は待ち行列
  をさらに深くするだけだからだ。quiche の CUBIC congestion control は datagram の経路の
  下位にあり、両者は直列に機能する。quiche がマシンから出るデータ量を制限し、app は
  その結果生じた損失に応じて encoder を調整する。
- Input: host を優先する。マシンの前にいる利用者が自身の mouse を操作している間、
  `LocalInputMonitor` は remote input を停止する。同時に操作できる viewer は 1 つで
  ある。
- Shell: shell ごとに PTY を 1 つ割り当てる（Windows は `ConPTY`、他は `forkpty`）。
  上限は 8 である。接続が切れた場合は shell を切り離し、shell プロセスが終了するか
  shell が閉じられるまで PTY を保持する。時間制限はない。許可された client はいずれも、
  保持されている shell を一覧（`TermList`/`TermListAck`）し、切り離された shell を id で
  reattach し、いずれの shell も終了させることができる。`TERM_CLOSE` は、data と resize
  の message が従う peer ごとの guard より前に処理され、その shell に入っていたマシンには
  `TERM_EXIT` が送られる。open、close、detach、reattach はいずれもアドレス、名前、key
  とともに監査ログに記録する。
- picker は 1 つ、client は 5 つ: `core/ui/ShellPicker` は `TermSessionList` を、すべての
  client が描画する行 —— id とサイズ、その shell が誰のものか、この client が reattach
  または close してよいか —— に変換する。reattach できるのは detach された shell だけで
  あり、host が引き取った shell はそのどちらでもない。Apple と Android の app は
  `DHTermSessionInfo` を通じて同じ行を読むため、shell の行を独自に整形する client はない。
- 各 shell の出力は、開始時点から host 側の `core/terminal` Screen にも書き込まれる。
  *Stop & attach* はリモートの client を切断し、その mirror を scrollback を保ったまま
  host の terminal ウィンドウで開く。この方法で引き継いだ shell は host に帰属し、
  期限切れせず、host のウィンドウを閉じた時点で終了する。

## 5. Client 側

すべての client 画面は、同一の構成要素である `HostLink`（`platform/client/HostLink`）
を通じて host に接続する。`HostLink` は QUIC connection を確立し、trust store を確認し、
auth handshake を実行し、link を維持し、必要とする画面については link 切断時に backoff
を伴って再接続する。自前で接続や authenticate を行う service は存在しない。service は
wire 上の `Chan` ごとに channel を開き、専用の inbox キューを受け取り、自身の thread で
それを処理する。

```
HostLink（開いている画面ごとに 1 インスタンス）
 ├─ link thread: dial → trust の確認 → auth → pump
 │   （受信した record と datagram を Chan ごとに各 channel のキューへ振り分ける。
 │    link pulse。recovery が有効な場合は backoff を伴う再接続）
 ├─ Chan::Control/Video/Audio ─> ScreenViewer
 │    ├─ net thread: HELLO/negotiation、video の取り込み（Reassembler と FEC）、
 │    │   NACK、feedback、clipboard
 │    └─ decode thread: decoder と render キュー
 ├─ Chan::Terminal ─> TerminalViewer の service thread
 │    ├─ core/terminal の Screen が文字グリッドを保持する
 │    └─ UI が Snapshot() をポーリングし、キー入力を command キューへ送る
 └─ Chan::File ─> FileTransferClient の service thread（FileUpload の ring）
```

受け入れが完了すると、link は自身の状態を監視する（`core/session/LinkPulse`）。session
id が 0 の `Ping` を、datagram ではなく control stream 上の record として毎秒送信し、host の `SourceListResponder` が、authenticate
済みの同一 connection 上で session を必要とせずに応答する。ping は ack-eliciting である
ため keepalive も兼ねており、通常の keepalive タイマーは link が受け入れられる前にのみ
意味を持つ。復旧中の link では、この pulse が
liveness の確認も兼ねる。5 秒間 pong を受信しない場合（ただし最初の pong によって host
が応答することが確認された後に限る）、connection は既存の再接続の経路に入る。この 5 秒
は、link のループが実際に監視していた時間で計算する。`LinkPulse::Tick` は
`HostLink::PumpReady` の 1 巡ごとに実行され、1 巡が `kLinkWatchStepUs` を超えた分は
無音時間から差し引かれる。したがって自機の停止が、host の応答停止と取り違えられること
はない。

screen viewer も terminal と同様にこの復旧機構を利用する。link の切断や無通信、あるいは
session が 5 秒間データを受信しない状態になると、ウィンドウは終了せず `Reattaching`
状態に移行する（最後のフレームを保持し、status 行を reattach 中の表示に切り替える）。
session が先に問題を検出した場合は `HostLink::RequestRedial` が再接続を要求し、link が
再び受け入れられると viewer は同一の client id で `HELLO` を再実行する。host は viewer
の枠を再割り当てし、streaming は新しい keyframe から再開する。60 秒
（`kViewerReattachGraceUs`）を過ぎても復旧しない場合、ウィンドウは通常どおり理由を
付して終了する。

source の問い合わせ（`QuerySources`）は、同じ link を一度限りのブロッキング形式で使用
する。UI は各種の要求（キー入力、resize）を command キューへ送る。未知の host key は
fingerprint を添えて link を失敗させ、UI がそれを *New host* ダイアログに表示する。その
アドレスが以前別の信頼済み host として応答していた場合、以前の所有者の警告は UI が自ら
調べて添える。この key を許可していない host は link を `AwaitingApproval` で待機させ、
利用者がキャンセルできる間、UI はその status 行を poll する。terminal のウィンドウは
escape sequence を解析しない。`core/terminal` が byte stream をセルのグリッドに変換し、ウィンドウはセルの描画とキーイベントの転送のみを
行う。現時点では各ウィンドウが個別に link を保持している。同一の host に向けたすべての
ウィンドウで受け入れ済みの link を共有することは想定済みの次の段階であり、`HostLink`
において registry と observer の fan-out として実装する。handshake を追加するもので
はない。

## 6. host の見つけ方

discovery は存在しない。network を scan するものはなく、host は平文の packet に一切
応答しない。client が接続する先は、利用者が入力したアドレス、最近の host、信頼済み
host（`HostProfiles`）、または QR 招待内のアドレスである。`SourceListResponder` は
受け入れ済みの connection 上でのみ `LIST_SOURCES` に応答する。この応答は
`SOURCE_LIST` のヘッダフラグによって host の能力 —— input を受け取るか、terminal を
共有するか、音声を共有するか、ファイルを受け付けるか（`kHostAcceptsInput`、
`kHostSharesTerminal`、`kHostSharesAudio`、`kHostAcceptsFiles`）—— も示すため、client はウィンドウを開く前に、
スマートフォンは閲覧のみであることを把握できる。source のレコードの後に、payload は host
のデバイス名（長さ 1 バイトと最大 64 バイトの UTF-8。空でもよい）を載せるため、この名前が
届くのは authenticate 済みの client だけである。client は `ParseSourceListHostName` で
これを解析し、制御バイトはすべて空白に置き換えられる。

最近の一覧は `platform/system/RecentDevicesFile`（`recent-hosts.txt`）にあり、解析処理は
`core/ui/RecentDevices` が担う。内容はアドレス、最終接続時刻、host の名前で、最大 10 件。
FFI の `dh_list_sources` は host が応答したときにのみそこへ記録するため、各 app が自分で
一覧を更新することはなくなり、`dh_recent_touch` は削除された。旧 `recent-devices.txt` は
変換されずに削除される。

QR code は唯一の out-of-band の経路であり、out-of-band のままである。host がそれを送信
することはなく、所有者が見せ、誰かが画面から読み取るかリンクを貼り付ける。app の外から
スマートフォンに届いたリンク —— `deskhub://pair` URL のタップ —— は、その host が既に
信頼済みでない限り、app が dial する前に *New host* ダイアログで確認される（3 節）。
`deskhubp::BuildPairingInvite(port, bindIp, hostName)` はランダムな 32 バイトの token を
発行し（`pairing_tokens` に 5 分の期限で保持。同時に有効なのは最大 4 つで、パネルを隠す
か共有を停止すると `RevokePairingTokens` がすべて無効化する）、`core/net/PairingInvite`
を整形する。テキストは `deskhub://pair/` に続けて、バイナリレコードの base64url ——
version バイト、endpoint 数 `n`、host のアドレス最大 4 つ分の `n × (IPv4, port)`、
32 バイトの fingerprint、32 バイトの token、長さ接頭辞付きの最大 32 バイトの host 名 ——
である。レコードは 180 文字に制限されており、誤り訂正レベル M で version 10 以下の QR
code に収まる。これはスマートフォンが腕の長さの距離からノート PC の画面を読み取れる
大きさである。`core/qr/QrCode`（`EncodeQr`、および CLI の `share --qr` 向けの
`RenderQrText`）が唯一の encoder であり、すべての client はそれが返すモジュールグリッド
を描く。Android と両 Apple app は `dh_qr_encode` を通じて描く。client 側の `ParsePairingInvite` が
`HostLink` に endpoint、要求すべき fingerprint、送るべき token を与え、
`dh_pairing_invite_address` は app にアドレス欄へ表示する最初の `ip:port` を与える。
スキャンだけがプラットフォームごとの部分 —— Android は CameraX + ZXing、iOS は
AVFoundation —— であり、どちらも decode したテキスト以外は何も返さない。

## 7. ディスク上のデータ

すべてのデータは利用者の Deskhub フォルダ（`~/.deskhub`、`%USERPROFILE%\.deskhub`、
iOS では App Group のコンテナ内の `.deskhub` フォルダ、Android では内部ストレージ。`DESKHUB_CONFIG_DIR` または
CLI の `--config-dir` で変更できる）に置かれる。`host_key.pem`（唯一の machine key。
TLS certificate は起動ごとに構築され、ディスク上には quiche が読み込む一瞬だけ
公開 certificate のみを含む `transport_cert.<random>.pem` として存在し、crash で残った
場合は次に hosting を開始したときに一掃される。`host_cert.pem` はもう書き込まれず、残っていても無視される）、`authorized_keys`（この host が受け入れる client key）、
`known_hosts`（fingerprint を key とする信頼済み host。名前と最後のアドレスを伴う）、
`access_requests`（Approve または Deny を待つ接続要求 —— 時刻、アドレス、デバイス名を
ラベルとした public key を 1 行ずつ。最大 16 件、それぞれ 10 分後に破棄）、`pairing_tokens`（現在有効な QR token と
その期限）、`ui-settings.txt`（デバイス名を含む）、`recent-hosts.txt`（アドレス、最終
接続時刻、host の名前）、Linux では `portal-restore-token.txt`（選択した画面に対して
デスクトップが発行した token）、および実行ごとの log（最新の 10 件だけを残す。`kKeptSessionLogs`）である。
passcode はどこにも保存されない —— 古い `ui-settings.txt` にあった `passcode=` 行は
読み込み時に取り除かれ、廃止された `paired_devices`、その有効化マーカー、`auth_salt` は
削除される。`client_key*.pem` も存在しない。旧バージョンが保持していたファイルは無視
され、移行されない。POSIX ではディレクトリは `0700`、ファイルは log も含めて `0600`
（log は `O_NOFOLLOW` で開く）で、設定ファイルは atomic に書き込まれる。Windows では
フォルダの ACL が利用者本人、SYSTEM、Administrators のみを許可し、その中のファイルは
log も含めて独自の mode を持たず、その ACL を継承する。iOS ではこのフォルダは iCloud と Finder のバックアップ
から除外され、app と broadcast extension がこれを共有しており、それによって extension の要求が
app の一覧に届き、app の *Approve* が extension に届く。ファイル I/O は `platform/` に
置き、解析処理とデータ構造は `core/` に置いて unit test を備える。

viewer が送信したファイルは別の場所に保存される。host が選択したフォルダ
（`ui-settings.txt` の `transfer_dir`。既定は利用者のホームディレクトリ直下の
`Deskhub`）である。`FileStore` は各ファイルを `<name>.deskhub-part` として書き込み、
ファイル全体が到着し CRC-32 が一致した時点でのみ改名する。したがって書き込み途中の
ファイルが正式な名前で現れることはなく、`UniqueFileName` がいかなるファイルも上書き
されないことを保証する。ネットワーク上のファイル名は、`platform/` が filesystem を
操作する前に `core/` の `SafeFileName` によって処理され、パス区切り、制御バイト、
Windows が受け付けない文字、予約デバイス名が除去される。

## 8. テスト

| Suite | 実行環境 | 対象範囲 |
| --- | --- | --- |
| `make test` | オフライン、socket なし | `core/` の全体: wire（`AuthStart` の token フィールドを含む）、framing、FEC、session、VT emulator、settings、文字列、決定的な structured fuzzing、および pairing の各部品 —— `Base64`、`PairingInvite` の往復と制限、`PairingTokens` の発行・消費・失効、`AccessRequests` の容量と失効、既知の encoding に対する `QrCode` |
| `make test-platform` | loopback socket | 実際の QUIC handshake、end-to-end の key 署名による認証、fingerprint を key とする host の固定、`AccessRequestsFile`（要求の記録、承認、拒否）と `PairingTokenFile`（token の発行、1 回限りの引き換え、無効化）、実際の `HostAuth` を通じた承認と token による受け入れ、ネットワーク越しの terminal host と viewer、実 shell に対する PTY、不正な署名による lockout |
| `make test-integration` | loopback、capture/encode は模擬実装 | host↔client の session 一式: negotiation、ネットワーク越しの video、input、許可済み key による受け入れ、他の golden message と並ぶ `AUTH_START_TOKEN` の wire vector、不正データへの耐性、および交差負荷下の遅延 —— 動作中の stream と並行してファイル転送、大量出力の terminal、キー入力を実行し、それぞれ観測された最大の停止時間で判定する |
| fuzz target | PR ごとに各 target 30 秒、nightly は各 15 分 | 9 つの target: wire、H.264（Annex B と SPS）、reassembly、terminal のバイト列、UI テキストの parser、host 側と viewer 側の session state machine、key と trust ファイルの parser（`FuzzKeys`: public key のテキスト、`authorized_keys`、`known_hosts`、接続要求、pairing token、招待）、QR encoder（`FuzzQr`） |
| `make test-perf` | release build、オフラインと loopback | hot path を実測する: `core_perf` は純 C++ の経路、`platform_perf` は loopback 上の実際の QUIC を対象とする。いずれも単位あたりの allocation 回数、入力 4 倍時のコスト、当該マシンで記録した baseline からの乖離によって判定する |

CI はさらに clang-format と clang-tidy（いずれもバージョン固定）、SwiftLint
`--strict`、Android Lint、actionlint と shellcheck、3 つの suite の ASan と TSan での
実行、C++/Kotlin/Swift への CodeQL、履歴全体への gitleaks、`core/` の line ≥ 90 % と
branch ≥ 80 % の coverage を強制する。これら 3 つの suite は arm64 Linux、Android
emulator、iOS Simulator 向けに cross-build され実行もされる。さらに Windows の job が
1 ラウンドにつき integration suite を 3 回追加で実行する。これは断続的に発生する
memory corruption を特定するためであり、当該事象はおよそ 3 回に 1 回しか現れない。
crash が発生した frame はこの corruption の結果であって原因ではないため、crash は必ず
dump を残す必要がある。テスト binary は handler に到達したすべての例外について完全な
minidump を出力し、fastfail はいずれの handler にも到達しないため、各 Windows job は
Windows Error Reporting も有効化し、対象の suite を実行する前に意図的な fail-fast に
よって収集が機能することを確認する。nightly では load test をさらに 2 巡実行する。
1 巡は full page heap の下で、もう 1 巡は Rust の debug assertion と overflow check を
有効にして build した quiche に対して行う。これは quiche の内部を観察できる唯一の手段
である。ASan は Rust を instrument せず、page heap が保護するのは heap のみだからだ。
Linux と macOS の release job も `core_perf` と `platform_perf` を、allocation と
scaling の 2 つの判定とともに実行する（共有 runner には時間の baseline が存在しない）。
また各 pull request には perf-and-lag のレポートが、自動更新される 1 つの comment として
付与される。内容は、両方の perf suite を同一 runner 上で base commit と A/B した結果
（乖離は警告であり失敗とはしない）、pull request の build から得た負荷下の integration
の数値、および core の coverage 行である。

## 9. 記録しておくべき設計判断

- **ホストがアプリケーションデータを送るには認証が必要**: `SessionTransport` は、
  その接続の鍵認証が完了するまで record と datagram の送信を拒否する。認証の
  challenge と結果は内部の認証送信経路を使う。信頼ストアは単一プロセス内で
  読み取り・変更・書き込みを直列化し、ファイルのアトミックな置換で保存する。

- **破損したクライアント許可リストはアクセスを許可しない**: `authorized_keys` の
  読み取り失敗、過大なサイズ、不正な形式、重複した鍵はいずれも設定全体の失敗と
  して扱う。ホストは接続中の認証状態を定期的に再確認するため、別プロセスが
  ファイルを置き換えた場合も、プロセス内の generation 更新なしに接続を取り消せる。

- **信頼はアドレスではなく鍵に従う**: `known_hosts` はホストの fingerprint を key と
  し、その隣のアドレスは最後に応答したものにすぎない。以前の規則 —— ピンは一つの
  `ip:port` に属し、そこで鍵が変わればボタンのない完全な遮断 —— は、DHCP のリース変更
  のたびに攻撃のように見せ、利用者にホストを反射的に削除して再信頼する習慣を教えて
  しまった。それこそ遮断が防ごうとしていた習慣である。新しいアドレスで到達した信頼済み
  ホストは今では単に接続し、既知のアドレスにある*別の*鍵はそのまま、このクライアントが
  一度も会っていないマシンとして扱われる。以前そこで応答していたホストの名前を示す警告
  付きの *New host* ダイアログである（`PreviousOwnerWarningFor`）。この警告は遮断が
  担っていた唯一の信号 —— 「このアドレスにいるものは以前のものではない」—— を保ちつつ、
  古いホストを *Trusted hosts* に手を付けずに残す。したがって新しいホストを信頼する
  ことは変更をクリックで通過することにはならず、fingerprint を示した初対面にしかならない。

- **新しいクライアント許可リストは完全な public key を保持する**:
  `authorized_keys` は長さを制限した OpenSSH public key 行を受け入れ、不正な行や
  重複鍵を拒否する。これが唯一の許可リストであり、ファイルがなければ誰も受け入れない。
  `known_hosts` は fingerprint とともに各信頼済みホストの別名と最後のアドレスを
  保存する。設定の書き込みにはプロセス間のファイルロックと原子的な置換を使う。
  Service は `SetConfigDir` または `DESKHUB_CONFIG_DIR` で、ログ用とは別の設定
  ディレクトリを指定できる。

- **Fingerprint は SPKI に基づく**: Deskhub が表示・保存する `SHA256:…` はすべて
  public key の DER SubjectPublicKeyInfo を SHA-256 でハッシュした値である。
  OpenSSH public key の行に含まれる SSH blob の一般的な SSH fingerprint とは
  直接比較できない。テキスト鍵を取り込む際は SPKI に変換してから Deskhub の
  fingerprint を計算する。

- **署名対象は曖昧さのない認証 transcript**：`core/auth/Transcript` は Deskhub の
  ドメイン、認証バージョン、署名者の役割、QUIC/TLS exporter の 32 バイト値、
  クライアント公開鍵全体、TLS ホスト鍵の指紋を長さ付きフィールドとして符号化する。
  quiche 0.29.3 への小さなパッチで C API から TLS exporter を使えるようにした。
  両端は接続ごとに同じ値を導出し、導出できない場合は認証に失敗する。ホストは
  接続ごとに署名済み応答を一度だけ受け付け、別のセッションでの再利用を防ぐ。
  認証を終えていない接続はすべて QUIC が受け入れてから十秒で閉じ、同時に保持するのは
  最大八件とする —— `AuthStart` を送らない接続も、署名しない接続と同じく数に入り、九件目
  はそもそも受け入れない。
  同じ鍵と送信元 IP から一分以内に署名検証が三回失敗すると、その組を十秒間拒否する。
  メモリ上の表は最大 64 組を保持し、認証成功時に失敗回数を消去する。

- **認証はプロトコルバージョン 3 の中で独立したバージョンを持つ**：`AuthStart` は公開鍵の
  前に互換性のための 0 バイトを残し、認証バージョンを最後に置く —— 今は 8 であり、
  バージョン 7 の pairing token のフィールドを保ったまま、承認の判断を署名の後ろへ、
  そして result の中へ移した:
  `00 | u16 keyLen | key | u8 nameLen | name | u8 tokenLen | token | 08`。ホストは末尾の
  バイトが自分のバージョンでない開始メッセージを拒否し、`VersionMismatch` を送って接続を
  閉じる。クライアントは別バージョンの challenge や result を受け取ると接続を閉じる ——
  認証バージョン 7 と 8 のビルドが中途半端に動くのではなくバージョン不一致を報告するのは
  このためである。ホストの challenge は `Signature`、または鍵の一覧を読めない場合の
  `ConfigError` である。`Denied` と `AwaitingApproval` は `AuthMode` に残っているが、
  バージョン 8 のホストがそれらを送ることはない。`AuthResultCode::AwaitingApproval`（9）は
  `Accepted` や `NotPaired` と同じく `AuthResult` で運ばれる。ホストの transport はそれを
  `onRefused` でも報告し、クライアントは時間切れになった待機をそれで失敗させる。
  challenge、response、result はバージョン付きのデータだけを運ぶ。

- **初回接続時は信頼、変更時は警告**：未知のホスト鍵は SSH と同様に一度だけ利用者に
  示し、利用者が受け入れた場合にのみ固定する（CLI では `--accept-new-host-key`）。同じ
  アドレスで以前応答していたものと異なる鍵は、クリックで通過する*変更*ではない ——
  新しい鍵を受け入れるボタンも `HostKeyChanged` という結果ももう存在しない —— この
  クライアントが一度も信頼したことのないホストであり、以前の所有者の名前を添えた通常の
  *New host* ダイアログで出会う。古いホストのピンは残るため、そのクリックで何かが
  上書きされることはない。利用者はただ、fingerprint を目の前にして、もう 1 台のマシンを
  信頼しただけである。

- **デバイス名は一つ**：Settings → General → *Device name*（空の場合は OS の名前）は、
  マシンが持つ唯一の名前である。ホストは viewer に表示して受け入れたクライアントに送信し、クライアントは接続時に送信し、
  マシンがコピーする公開鍵のラベルにもなり、ホストがそのマシンの要求を承認するか QR
  token で受け入れた際に `authorized_keys` に書き込むラベルでもある。Client ページから
  独自の名前欄を削除したのは、ホストが目にする名前と、その `authorized_keys` にある
  ラベルを一致させるためである。

- **旧データは移行しない**：passcode、以前の `paired_devices` の一覧とその有効化マーカーは
  変換しない —— いずれもクライアントが鍵を保持していることを何ら証明しない —— 残った
  ファイルは削除する。7.0.x の `client_key.pem`、`client_key.<name>.pem`、`host_cert.pem`
  は単に無視する。machine key は既に `host_key.pem` だったため fingerprint は変わらず、
  クライアントの旧 Ed25519 identity は新しい identity に引き継がれない —— ホストの所有者が
  Approve または QR で machine key を一度許可する。読み取れない `authorized_keys` や
  `known_hosts` の内容を推測で補う
  ことはない。読み取れない間、ホストは全員を拒否し、クライアントはすべてのホストを拒否し、
  次の変更時にファイルを新たに書き込む。

- **デスクトップ画面に何を表示するかは各アプリのコードではなく `core/ui` のデータで決まる**：
  配色（`Theme.h`。各色にライトとダークの値を持つ）、ホストのライブ表の列とサイズ
  （`HostRows.h`）、Settings ページの枠・セクション・並び順（`SettingsLayout.h`）は一度だけ
  定義する。Windows は直接読み、Linux はホスト表の列を直接読んで GTK 自身のテーマで
  描き、macOS は `dh_theme_color`・`dh_host_columns`・`dh_settings_layout` 経由で、
  iOS と Android は `dh_theme_color` 経由で読む。アプリが決めるのは、
  ある `SettingField` をどのコントロールで描くかだけである。以前は各アプリが同じ色の値と
  ページ順を個別に持っていたため、次第にずれていった。Windows で赤いボタンが macOS では
  灰色になり、二つのデスクトップにある設定が三つ目では欠けていた。レイアウトのテストは保存
  される各設定がちょうど一度だけ現れることを確かめるので、アプリが設定を黙って落とすことは
  もうできない。

- **false を返す capability probe が制御ループ全体を無効化しうる。** MFT が
  `CODECAPI_AVEncCommonMeanBitRate` を提供しない場合、Media Foundation の encoder は
  `SetBitrate` に `false` を返し、`ApplyFeedback` はこの拒否を「何も適用されていない」
  として正しく扱う。`MeanBitRate: NOT SUPPORTED` を報告する Intel Quick Sync の MFT
  では、その結果として host は bitrate をまったく変更しなかった。当該ハードウェアでの
  実測では、29-40 % の損失が 30 秒継続しても `Bitrate` の判断は 1 件も発生せず、
  quality ladder も変化しなかった。起動ログには一貫して `NOT SUPPORTED` が出力されて
  いたが、適応機構が停止しているとは解釈されなかった。同じファイルの `SetFps` と
  `RequestKeyFrame` には既に `ReinitTransform()` へのフォールバックがあり、これを欠いて
  いたのは `SetBitrate` のみであった。現在は同様のフォールバックを追加している。
  `ConfigureTransform` が `cfg` から `MF_MT_AVG_BITRATE` を書き込むため、再構築により
  新しいレートが適用される。再構築には IDR の分のコストが伴うため、オンラインの
  `codecapi` 経路を先に試行する点は変更していない。デバイスごとの capability が制御
  入力を左右する場合、フォールバックは必須とすること。性能が低下する動作を選ぶことは
  選択肢であるが、通知なく機能が完全に無効になることは選択肢ではない。

- **追随できない送信側は、損失のないリンクとまったく同じ挙動を示す。**
  `BitrateController` に与えられていたものはすべて viewer の `Feedback` から得られ、
  そのうち実際に使うのは loss だけである（RTT と受信レートは表示用）。そのため制御ループ
  のどの構成要素も、遅れているのが送信側自身であることを判断できなかった。
  2 つの viewer に対して host を務める Pixel 4 での実測では、frame が encoder を出る
  時点で 15 秒遅延していた一方、viewer は損失 0 %、RTT 15 ms を報告しており、制御側は
  これを余裕と解釈して bitrate を上限の 20 Mbps まで戻した。これは送信側内部の
  bufferbloat であり、リンクが良好に見えるほど送出量が増える。現在は host が送信段で
  frame の経過時間を測定し、viewer の数値と併せて入力する。`kBacklogMs` を超えた場合は
  損失 2 % に相当する扱いで、`kSevereBacklogMs` を超えた場合は損失 5 % に相当する扱いで
  抑制し、いずれも通例どおり 2 秒間は再上昇を禁止する。制御変数は引き続き bitrate のみ
  であるため、`QualityLadder` がそれに追随して段を下げ、fps の上限も連動する。対向から
  のみ情報を得る制御ループは、自身が実際に管理している側のパイプラインを観測できない。

- **fps の制限は、実際に frame を破棄できる箇所でのみ効果を持つ。** ladder の fps の段
  は要求であり、各プラットフォームは frame を破棄できる箇所でこれを実現しなければ
  ならない。Windows と Linux は `FrameGate` により capture の段階で処理し、Android は
  `max-fps-to-encoder` で MediaCodec の入力を制限し、macOS は ScreenCaptureKit の frame
  interval を再設定する。iOS には該当する箇所がなかった。ReplayKit は画面のレートで
  frame を供給し、`VtEncoder::SetFps` は `kVTCompressionPropertyKey_ExpectedFrameRate`
  を設定するのみで、これは rate control への指示であって frame を破棄しない。ここで段を
  変更しても encoder を再設定するだけで、処理すべき frame の数は変わらない。現在は
  `OfferVtFrame` が両方の Apple app で同一の `FrameGate` を実行する。idle-flush の
  キャッシュを更新した後に実行するため、静止画面でも再送できる frame が残る。あるパラ
  メータがすべてのプラットフォームに存在する場合、ladder に依拠する前に各プラット
  フォームでの扱いを確認すること。

- **send pacer は encoder 自身の出力レートより十分に高く保つ必要がある。**
  `Pacer::Gate` は `SendEncodedFrame` が動作する thread 上で待機し、Android ではそれが
  MediaCodec の drain ループ、すなわち encoder が次の frame を引き渡すために
  `releaseOutputBuffer` を呼び出さねばならないループである。したがって pacing は
  ネットワーク上のレートだけでなく drain のレートも決定するが、その間も VirtualDisplay
  は画面のレートで新しい frame を送り込み続ける。送信のバーストを平準化する目的で
  `kPacingRateMultiple` を 2 から 1.2 に狭めた変更は Pixel 4 で実測している。frame
  あたりのバーストは中央値で 20 ms から 63 ms に増加し、encoder の滞留は際限なく増大
  した。`enc_lat_ms` は 100 秒で 46 秒を超え、viewer は 4.6 秒遅れた。値が 2 の場合、
  同じ実行で `enc_lat_ms` は 0 のままであった。この余裕は削減可能な冗長ではなく、
  encode のパイプラインが供給より速く排出されることを保証する条件である。送信のバースト
  には socket buffer で対処するか、pacing を drain thread の外へ移すこと。この数値を
  下げて対処してはならない。

- **perf suite はコストで判定するため、結果を見る判定を別に設ける必要がある。**
  `core_perf` は packet あたりの allocation 回数と、時間が入力に対してどのように増加
  するかを測定する。実際のリンク上で 1 つの packet 損失が無傷の frame の 22 % を破棄
  させていた期間も、その reassembler の workload はすべて合格していた。この suite では
  検出できない。有効な video を破棄するコストは decode するコストより*低い*ため、誤った
  方針のほうが suite の見るすべての指標で良好な値を示す。`LossGoodputTests` はこれを
  補う判定であり、コードが本来行うべき処理量を下回った場合に失敗する。実際の往復時間を
  伴う末尾損失のリンクを模擬し、packet がすべて到着した frame のうち実際に decoder へ
  到達した割合と、配信された frame 間の最大間隔によって判定する。いずれもハードウェアに
  依存しないため、ノート PC、CI runner、スマートフォンのいずれでも同じ結果となる。
  方針が処理量を減らすことで「成功」しうる場合には、goodput による判定を設けること。

- **packet が 1 つ失われた場合に失うのは frame 1 枚であり、次の keyframe までの映像
  全体ではない。** reassembler は以前、損失のたびに `waitingForIdr_` を設定していたため、
  packet が 1 つ欠けるだけで、新しい IDR が到着するまで後続の*完全な* frame をすべて
  破棄していた。Wi-Fi 経由でスマートフォンを host とした実測では、これにより実際に
  不完全であった 64 枚が 381 枚の破棄となった。decode 可能な video 6.4 MB が破棄され、
  映像の停止時間は中央値 146 ms、最大で 1 回あたり 1.4 秒に達した。現在は不完全な frame
  のみを破棄し、それ以降の frame は decoder へ直接渡す。decoder が欠落した参照を補完
  する間に、`InvalidateRef` が問題のある frame を host へ通知し、keyframe の要求により
  修復される。短時間のマクロブロックの乱れは、映像の停止を避けるために許容するコストで
  ある。`waitingForIdr_` は本来正しかった唯一の状況、すなわち途中から参加した viewer が
  参照を一切持たず最初の IDR を待つ必要がある場合のために残している。

- **損失と判定するまでの待機時間は retransmit より長くする必要がある。さもなければ
  NACK は機能しない。** 以前は frame が損失と判定されるまでの待機時間が frame 間隔
  2 つ分（60 fps で 33 ms）しかなく、同一リンクで実測された RTT は 24-49 ms であった。
  NACK は送出されるものの、応答が到着した時点で当該 frame は既に破棄されており、
  `late_ms_avg=24` および毎秒 87 個の packet が存在しない frame に属する状態として現れて
  いた。現在の `StallTimeoutUs` は、pacing に基づく待機時間と、往復時間の 1.5 倍に NACK
  の保留時間（`kNackHoldUs`、2 ms）を加えた値のうち大きいほうを採用し、ハードな timeout による上限は維持する。これにより、再送を要求する
  のは実際にそれを必要とするリンクに限られる。

- **performance suite は allocation とコストの増加形態で判定し、ミリ秒では判定しない。**
  3 つの test suite は debug で build され、CI ではさらに ASan、TSan、coverage の下で
  実行されるため、実時間による予算は sanitizer を測ることになりコードを測らない。
  そのため `core_perf`（release preset、`make test-perf`）はハードウェアに依存しない
  2 つの指標で判定する。グローバルな `operator new` を差し替えて計数する packet、frame、
  KB あたりの allocation 回数と、`-scaling` の行における所要時間が入力よりはるかに速く
  増加することである。計測時間の側は `out/perf/baseline.txt` との比較として残す。この
  baseline は `make perf-baseline` がマシンごとに生成し、バージョン管理には含めない。
  この分離により、「reassembler が各断片を 2 回コピーするようになった」といった退行を、
  ノート PC、CI runner、スマートフォンのいずれでも同様に検出でき、同時に数値自体が意味
  を持つ経路については単位あたりの ns と MB/s を出力し続けられる。CI はこの 2 つの
  ハードウェア非依存の判定を Linux と macOS の release job で実行する。Windows は
  binary の build のみを行う。MSVC の deque は 16 バイトを超える要素について要素ごとに
  block を allocate するため、同じコードでも allocation の回数が異なるからである。
  pull request にはさらに、共有 runner のノイズに左右されない計測比較が付く。base
  commit と pull request を同一 runner で測定し、許容幅 50 %、警告のみとする。
  `platform_perf` は同じ判定を loopback 上の実際の QUIC に拡張する。ここでの実時間は
  service ループの周期、すなわち 64 KiB の stream 処理予算と 1 ms の poll tick の積を
  反映するため、予算の縮小、処理の線形性の喪失、poll ループでの allocation の追加は、
  同じ処理の CPU コストがほとんど変わらない場合でも明確な変化として現れる。

- **`QuicEndpoint::Poll` はブロックする読み取りで終わらない。** 待つのは最初のパケット
  だけである。backlog が残っている間は 1 ms、これは flush のバーストと stream の処理を
  律する tick であり、それ以外は呼び出し側が指定した時間だけ待つ。以降のパケットは
  `WaitReadable(0)` が到着を示した場合にのみ読み取る。receive timeout が切れるまで読み
  続ける方式では、すべての `Poll` が最後のパケットの後に、`sendMutex_` を保持したまま、
  timeout をもう一度支払っていた。Linux は 1 ms の timeout を守るが、Windows では同じ
  待機に数十ミリ秒かかり、`platform_tests` は Linux の 100 s に対して 700 s を要した。
  したがって待ち時間 0 の `Poll` は、空いている link ではまったく待たない。ループで
  poll する呼び出し側は、先に待つ（`WaitReadable`、lock なし）か、`platform_perf` の
  ように待ち時間を渡す。その drain-scaling の判定は、どちらも 64 KiB の処理予算を
  超える 2 つの規模を比較する。小さい規模は tick を一度も使わずに終わり、大きい規模を
  超線形に見せてしまうからである。

- **`FileHost` は自身の lock を保持したまま送信しない。** QUIC の service ループは
  `SessionTransport::sendMutex_` の下で `QuicEndpoint::Poll` を実行し、そこで閉じられた
  connection は `FileHost::OnPeerGone` を直接呼び出す。この関数は `FileHost::mutex_` を
  取得する。したがって `sendMutex_ -> mutex_` の順序は transport によって固定されて
  いる。先に `mutex_` を取得してから送信する経路、たとえば `FileReceiver` が
  `hooks.send` を通じて accept、ack、cancel を送出する箇所は、この順序と循環を形成し、
  TSan は受信ループと、動作中の転送に対して `SetAccepting(false)` を切り替える UI
  thread との間の lock-order inversion として検出した。そのため receiver が生成する
  record は `mutex_` の下で `outbox_` に格納し、mutex を解放した後に送信する。両方の
  段階で `outboxMutex_` を保持し、peer が受け取る順序が生成順と一致するようにしている。
  `OnPeerGone` は送信を行わない。既に `sendMutex_` の下で動作しているため、格納した
  内容は破棄する。

- **command line client は 4 つ目のフロントエンドであり、2 つ目の実装ではない。** flag
  を `core/cli` で解析し、その後はデスクトップ app が駆動するのと同一の構成要素を駆動
  する。host には `SharingHost`、閲覧には `ScreenViewer`、shell の起動には
  `TerminalViewer` を用いる。固有の要素は表示ウィンドウのみであり、Linux では X11 と
  EGL、Windows ではデスクトップ app 自身の `RunViewer` を使用する。各 client の `cpp/`
  ツリーが静的ライブラリ（`deskhub_linux_core`、`deskhub_win_core`、
  `deskhub_win_view`、`deskhub_mac_core`）であり、GUI のコードがその上位に置かれている
  のはこのためである。この分割により、CLI は GTK や wxWidgets を link せずに media
  pipeline を link できる。

- **`preflight` は capture すべき画面がある場合にのみ実行する。** すべての client が
  これを用いて capture の経路を確認する。Linux では xdg portal、macOS では Screen
  Recording の許可、Windows では D3D11 デバイスである。shell のみを含む共有にはこれらは
  不要であり、無条件に確認していたために、ディスプレイのない環境での
  `share --terminal` が screen-capture の permission がないという報告を返していた。現在
  は source 一覧が空の場合、`HostEngine::Start` がこの確認を省略する。

- **shell のみで画面を持たない host も動作を継続する。** net ループは動作中の source が
  なくなった時点で session を終了するが、terminal のみ、またはファイルのみの共有には
  定義上 source が存在しない。`keepAlive` は呼び出し側の意図（`ShareOptions::terminal`、
  `ShareOptions::files`）と、`TerminalHost` または `FileHost` がまだ動作しているかどうか
  から決定する。ループの開始後にのみ設定される tenant のポインタだけから決定することは
  ない。

- **frame gate は目標時刻に向けて計数し、直前に通した frame からは計数しない。** 30 fps
  を目標とする状況で compositor が 40 fps を供給する場合、33 ms の境界の多くに frame が
  存在しない。したがって「直前に通した frame から十分に離れているか」のみを判定する
  gate は 1 枚おきに破棄し、最終的に 20 fps に収束する。これは目標を下回り、かつ不均一
  であり、レートの低い stream ではなく judder となる。`FrameGate` は代わりに進行する
  目標時刻を保持する。通過のたびに目標時刻を正確に 1 間隔分進めるため余りが保たれ、
  入力 40 枚に対して出力 30 枚となる。目標より遅い capture が間引かれることはなく、
  実時間より遅れた目標時刻は蓄積せずに再同期するため、静穏な時間が後のバーストにつな
  がることもない。

- **Linux の host は専用の thread で encode し、その thread には縮小後の frame を渡す。**
  PipeWire の `process` コールバック内で encode すると capture が `1000 / enc_ms` fps に
  制限され、encode 時間の変動がそのまま client 側の frame 間隔の揺らぎとなる。現在
  encode は専用の thread で動作し、`FrameMailbox` を通じてデータを受け取る。これは最新
  のものを保持する単一スロットのキューであり、encoder が遅れた場合は最新の frame を
  保持し、古い frame はキューに積まずに計数する。キューを渡るのは encode サイズへ縮小
  済みの frame であり、データ量はおよそ 7 分の 1 である。フル解像度の frame をコピーして
  渡す場合のコストはコピー自体をはるかに上回る。capture を行うコアに dirty な状態で
  20 MB のキャッシュラインが残り、encode を行うコアがそれを取得する必要が生じるため、
  実測で 16 ms を要した。自身が保有する同量のメモリを読み出す場合は 3.4 ms である。
  capture thread はいずれにせよすべてのソースピクセルを 1 回処理する必要があるため、
  その 1 回の走査を行う場所として適切である。dma-buf の frame は引き続きその場で encode
  する。コールバックが戻った時点で compositor が背後のメモリを再利用するため、frame は
  コールバックより長く存続できず、また VA-API が GPU 上で縮小を行うためである。
- **Linux の host は frame の所在によって encoder を選択し、導入されているソフトウェア
  によっては選択しない。** dma-buf の frame は VA-API へ渡す。VA-API はその frame を
  生成した GPU 上で zero-copy により import できる。CPU メモリへ map 済みの frame は、
  NVIDIA の driver がある場合 NVENC へ渡す。NVIDIA の GPU が描画するデスクトップでは
  compositor が screencast を共有メモリへ再交渉するため、その場合の encode は、システム
  メモリから直接ピクセルを取得できるカードが担うのが適切である。`HwEncoder` は encoder
  の再構築ごとにこの判断を行い、後から他方の種類の frame を受け取った場合は `false` を
  返す。これが再構築の契機となる。
- **NVENC の前段の縮小処理は本プロジェクトの実装であり、swscale は使用しない。** NVENC
  は packed の 32 ビットピクセルを受け取るが resize は行わず、capture されるのはフル
  解像度のデスクトップである。`libswscale` は 3440x1440 → 1280x534 で 9.2 ms を計測した。
  これはおよそ 2 GB/s であり、本機のメモリ帯域より 1 桁低い。packed RGB のスケーリングが
  最適化された経路から外れるためである。`core/` の `RgbDownscale` はこの形状のために
  実装した面積平均であり、ソースピクセル 1 つにつき 32 ビットのロード 1 回、整数による
  累積を行い、同じ frame を 4.0 ms で処理する。また swscale のバイリニア 1 タップでは
  なく、正しいアンチエイリアス結果が得られる。frame 全体の NVENC のコストはおよそ 5 ms
  であるため、60 fps には余裕がある。
- **性能値は release build から得たものにのみ意味がある。** `make build-linux` と
  `make run-linux` は `x64-debug` preset を configure する。これは `-O0` であり、encode
  の経路は現在 `core/` 内のピクセル演算である。同じ frame の処理時間は、この構成では
  約 19 ms、`make release-linux` では約 5 ms である。debug の binary に対して測定した
  judder の報告は、実際には build の種類を測定している。

- **Apple の viewer は PTS を control timebase 上で用いて映像の表示間隔を制御し、pacer
  は自身の結果を前提としない。** frame を到着した時点で表示すると、Wi-Fi の到着ゆらぎが
  judder として現れる一方、latency の各指標は良好なままとなる。表示の間隔と latency は
  別の事象である。`VideoPacer`（core に属し、オフラインでテスト可能）は host の PTS を
  ローカルの表示時刻へ、e2e の指標と同じ方法で対応付ける。すなわち `arrival − pts` の
  移動窓における最小値を用い、到着ゆらぎを吸収するための約 33 ms の先行を加える。
  `VtDecoder` はこれに基づき `AVSampleBufferDisplayLayer` の control timebase を駆動し、
  乖離が 250 ms を超えた場合にのみ再同期する。2 秒を超える pts の跳躍はゆらぎではなく
  新しい stream と解釈するため、対応付けは一定期間停止するのではなく初期化し直す。
  レンダラが外部の timebase に従うことをすべての OS バージョンについて確認する手段が
  ないため、decoder は自身で検証を行う。表示間隔を制御した frame が満杯のレンダラ
  キューによって連続して破棄された場合、即時表示へ切り替えて flush する。映像ではなく
  平滑化のほうを手放す判断である。

- **音声は datagram 1 つにつき frame 1 つとし、失われた packet は再送しない。** 64 kbps
  における 20 ms の Opus frame はおよそ 160 バイト、最大でも 209 バイトであり、datagram
  には 1180 バイトの余地がある。したがって音声の経路には packetizer、FEC、reassembler、
  NACK のいずれもない。これらは video の経路のほぼすべてに相当する。損失はコストの最も
  低い箇所で処理する。Opus は次の frame に in-band FEC を含むため、jitter buffer が
  1 つの frame の欠落を報告すると、受信側は次の packet を `decode_fec=1` で decode して
  それを復元し、FEC で復元できない場合にのみ decoder に欠落を補完（PLC）させる。再送には意味がない。200 ms 遅れて到着した frame
  は再生できないうえ、後続の 10 枚を遅延させるからである。`make opus-smoke` により、
  このライブラリを build できる任意のマシンで上記の数値を測定できる。
- **jitter buffer にタイマーは含まれない。** `AudioJitterBuffer` は状態のみで構成され、
  目標遅延は再生開始前に蓄積する frame 数にほかならない。60 ms は 3 枚に相当する。
  これにより全体を待機なしでオフラインにテストでき、失敗の形態も明確になる。バーストは
  キューに積まず上限で抑え、バッファが空の場合は途切れた再生ではなく再バッファを行い、
  シーケンス番号の跳躍は数千枚の損失ではなく新しい stream として解釈する。表示間隔の
  制御は `AudioPlayer` にあり、実時間で 20 ms ごとに 1 frame を PCM の ring へ送り、
  sink の render コールバックがこれを取り出す。
- **capture コールバックは encode を行わない。** PipeWire と ScreenCaptureKit は数
  ミリ秒の締切を持つリアルタイム thread で音声を供給し、そこで締切を超過すると Deskhub
  だけでなく host 自身の再生に xrun が生じる。Opus の encode は 0.3–1.5 ms を要し、
  スパイクも発生する。以前は viewer ごとの `sendto` が同じ thread でその後に実行されて
  いた。現在の `AudioBroadcaster::Offer` は 20 ms の frame を事前に確保した lock-free の
  スロット ring へコピーし、capture 時刻を記録するのみである。encode、診断、viewer ごと
  の送信は worker thread が担う。worker が処理しきれない場合の影響は計数される破棄
  （`framesRefused`）であり、host の音声の乱れではない。
- **音声は双方が有効にした場合にのみ流れる。** viewer が `Hello.features` の bit 0 を
  設定し、host は capability に `kHostSharesAudio` を提示し、host は当該ビットが設定されて
  いる viewer にのみ packet を送信する。
- **プロトコルバージョン 3 は自分自身としか通信しない。** admission によって無意味になった
  passcode のバイトを `Hello`・`LIST_SOURCES`・`TERM_OPEN` から取り除き、H.264 のみの配信
  では一度も使われなかった codec ネゴシエーションを `Hello`/`HELLO_ACK` から取り除いた際に、
  `kProtocolVersion` は 3 に上がった。現在はすべての parser が現行のレイアウト全体を要求
  する。旧来の短い形式も reserved の詰め物もない。`ClassifyPacket` は現行バージョンのみを
  自分のものとみなすため、旧バージョンの peer は中途半端に解釈されることなく破棄される。
  wire を変更する際はバージョンを上げ、互換用の分岐は決して追加しない。

- **terminal の link は自身で維持し、自身で再接続する。** terminal viewer は video の
  session とは別に独自の QUIC connection を保持するため、video 側の keepalive はいずれ
  も届かない。プロンプトで操作がない状態ではトラフィックが発生せず、QUIC の 30 秒の
  idle timeout により切断され、viewer は `Reattaching` 状態で thread を停止したまま再接続
  を行わなかった。その間、shell は host 側で 2 分間保持されたままであった。現在は
  keepalive と再接続が `HostLink` にあり、recovery を求めるすべての画面が利用する。
  link はタイマーにより ack-eliciting な packet を送信し、backoff を伴って再接続する。
  `TerminalViewer` は `kTerminalReattachGraceUs` を猶予としてこれを利用し、
  `TerminalClient::Reattach()`（core に実装され test もされていたが、呼び出されて
  いなかった）を再利用するため、同一の shell が scrollback とともに復帰する。タイミング
  の定数は core の `deskhub::KeepaliveIntervalUs` と `ReconnectDelayUs` が保持する。
  keepalive は idle timeout あたり 3 回送り（最短でも 1 秒間隔、最長でも timeout の半分
  の間隔）、packet が 1 つ失われても維持できるようにしている。再試行は `kTerminalReattachGraceUs` で正確に
  停止する。それを過ぎるとウィンドウは喪失を報告するが、shell 自体は時間制限なく
  host 上に残り、破棄される代わりに後の明示的な resume を待つ。
- **record は stream へ完全な形で書き込むか、まったく書き込まないかのいずれかであり、
  遅れた client は再描画によって同期させる。** 信頼性を要するデータ、すなわち control、
  auth、terminal の出力は、いずれも 1 本の QUIC stream を共有する length prefix 付きの
  record である。したがって record が途中まで送られると、対向側の framing は恒久的に
  ずれる。`RecordStream` に再同期の手段はなく、peer は connection を閉じる。
  `QuicEndpoint::SendStream` は以前、収まる分だけ書き込み残りを破棄していた。この方式は
  `make test` のようなコマンドの出力がリンクの能力を上回るまでは機能していた。1 MiB の
  stream window が埋まり、`TermData` record の末尾が破棄され、viewer の framer が失敗し、
  shell は開いてから 1 分後に切断された。現在は各 record を完全な形で stream ごとの
  outbox（最大 4 MiB、file stream では 256 KiB）に入れ、window が開くにつれて排出し、
  outbox に収まらない record は拒否する。それでも quiche が書き込みエラーを報告した場合、
  `BreakStream` がその stream だけを reset し、`onStreamBroken` で報告する。connection
  ともう一方の stream は維持される。ずれた stream をその場で修復することはできないが、
  それ以外を道連れにする必要はないためである。その上位では `TerminalHost` が未送信の出力を shell
  ごとのキューに保持し、tick ごとに再試行するため、一時的にリンクの能力を上回る出力の
  バースト（たとえば build の出力）も client に完全な形で届く。`kMaxPendingBytes` を
  超えた場合、キューは増大させずに破棄する。すべてのバイトは既に host 側の `Screen`
  mirror に届いているため、`deskhub::term::RenderScreen` により client を同期させる。
  これは現在のグリッドを 1 回描画し直すもので、最短でも `kRepaintIntervalUs` に 1 回で
  ある。利用者が読み取る余地のなかった出力はバッファせずに省略するため、出力し続ける
  コマンドは自身の速度で動作し、なおかつ正しい最終画面が残る。reattach 中の client も
  同じ再描画を受け取る。中断の後では、byte stream における位置に意味がないためである。
- **自動共有はデスクトップの準備を待つ。1 度だけ列挙して終えることはしない。** Windows
  は autostart を `ONLOGON` の scheduled task として登録するが、これは session が列挙
  可能なディスプレイを持つ前に起動する。そのため構築時の `ListDisplays()` は以前は空を
  返し、app は共有できる対象がないと報告していた。`deskhub::ui::AutoShareGate`（core に
  属し、unit test を備える）が再試行の規則を保持する。`kAutoShareProbeMs` ごとに probe
  し、`kAutoShareGiveUpMs` で終了する。各 client はこれを自身のタイマーで駆動するため、
  規則は 1 箇所にのみ存在する。`NextAutoShareStep` は同じ規則の状態を持たない形であり、
  Swift の client は `dh_auto_share_step` を通じてこれを利用する。自動共有はモーダルを
  開かない。ログイン時にはウィンドウが tray に隠れていることがあり、その状態のダイアログ
  は表示されないまま共有を無期限に妨げるためである。拒否の理由は Host ページのバナーと
  log に出力する。デスクトップの client は OS の display 変更シグナルでも選択一覧を更新
  する。これにより、後からディスプレイを接続した場合も一覧が正しく保たれる。
- **msquic や ngtcp2 ではなく quiche を採用する。** Android と iOS の双方で本番環境での
  実績がある唯一の QUIC ライブラリである。BoringSSL を同梱しており、これが machine key、
  そのメモリ上の certificate、transcript の署名にも利用できるため、暗号ライブラリを 2 つ
  抱える必要がない。
- **connection migration は使用しない。** 候補となるライブラリのいずれにも、利用可能な
  client 側の対応がなかった。reconnect と reattach の機構（tmux と同様の方式であり、
  モバイルのバックグラウンド動作のために元より必要であった）がこの要件を満たしている。保持されている shell は一覧（`TermList`）して新しい client から id で resume することもできる。
- **Ed25519 ではなく ECDSA P-256 を使用する。** BoringSSL のサーバ側は、quiche を通じて
  Ed25519 で TLS handshake に署名せず、今は 1 つの key が TLS と client の署名の両方を
  担わなければならない。保存済みの key が P-256 でない場合、ファイルに触れずに起動を
  失敗させる。`host_key.pem` が存在しない場合に限り新しい identity を作成するため、既存の
  fingerprint は勝手に変わらない。
- **quiche は事前に build し、FetchContent は使用しない。**
  `scripts/build-quiche.sh` が `third_party/quiche/` の下に rust target ごとの
  ディレクトリと、共有の `include/` を生成する。後者には quiche.h と、boring-sys が
  同梱する BoringSSL のヘッダが含まれる。これらを取り出しているのは、Deskhub が host
  identity のために BoringSSL を直接呼び出しており、include パスと TLS ライブラリを
  それぞれ 1 つに保つ必要があるためである。`DeskhubQuiche.cmake` がこれを
  `deskhub::quiche` として提供し、ライブラリがない場合は `platform/CMakeLists.txt` が
  configure を失敗させる（build が `-DDESKHUB_QUIC=OFF` で明示的に外した場合を除く）。
  opus も `DeskhubOpus.cmake` と `-DDESKHUB_AUDIO=OFF` で同じ扱いを受ける。
- **Apple では `libplatform_bundled.a` を link する。** Xcode の app は platform の
  archive を CMake の外から利用するが、その場合 quiche への PRIVATE な link はその link
  行に現れない。そのため `libtool` の工程で platform と quiche と opus を、`.pbxproj` が
  link する 1 つの archive に統合している。
- **Windows の toolchain における既知の問題は解消済みであり、この状態を維持する。**
  quiche は Rust のオブジェクトについて
  `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS` により静的 CRT を使用し、BoringSSL の
  オブジェクトについては `CFLAGS_x86_64_pc_windows_msvc` の `/MT` で指定する（msvc の
  既定は DLL ランタイムであり、このフラグを包括的な `RUSTFLAGS` で渡すと cargo の build
  が破綻する）。ツリー全体も `MultiThreaded` を固定して整合させており、これにより
  出荷する exe は VC++ Redistributable を必要としない。wxWidgets は configure のたびに
  `wxBUILD_USE_STATIC_RUNTIME` を再設定する。`wx_option()` がこの値を恒久的に
  キャッシュするためである。BoringSSL は既定の Visual Studio generator で build しなけ
  ればならない。この generator では cmake crate が /MT を per-config のフラグでのみ
  伝達するため、`CMAKE_GENERATOR=Ninja` を強制すると BoringSSL が /MD に戻り、最終の
  link が LNK2038 で失敗する。MSBuild が長いパスにより MSB6003 を発生させる場合は、
  Windows の長いパスのサポートを有効にすること。Git Bash の `/usr/bin/link.exe` は
  MSVC の linker を隠すため、`cl.exe` のあるディレクトリを先に配置すること。Git Bash の
  パス変換は `/` で始まる引数を壊すため `MSYS2_ARG_CONV_EXCL` を使用すること。NASM の
  インストーラは PATH を変更しない。
- **Windows ホストで Android 向けの quiche を build する際は cargo-ndk を使用しない。**
  cargo-ndk は boring-sys に拡張子のない `clang` のパスを渡すが、CMake は Windows 上で
  これを受け付けない。そのため `build-quiche.sh` は `CC_*`、`CXX_*`、`AR_*`、cargo の
  linker、および対象 ABI の `--target=` を自身で設定し、cargo を直接呼び出す。
  BoringSSL はこの場合も Ninja を必要とする。Visual Studio generator は NDK を対象と
  できないためである。また bindgen は Visual Studio の libclang を使用し、自身の binary
  の隣に `stddef.h` を探すため、`BINDGEN_EXTRA_CLANG_ARGS` でスラッシュ区切りにより
  NDK の resource ヘッダを指定する。bindgen はこの変数をシェルの規則で分割し、バック
  スラッシュを除去するためである。
- **cross-compile する app はいずれも自身の quiche を先に build する。**
  `build-android`、`build-ios`、`build-macos`、`build-linux` は、`debug` と `release`
  が host の ABI に依存するのと同様に、それぞれの ABI 向けの quiche target に依存する。
  quiche は ABI ごとに build され、存在しない場合は CMake の configure が失敗するため、
  この工程を省略した build はライブラリの不足ではなく toolchain の不具合のように見える。
  また、前回成功した build のまま残された app は、他のマシンが既に対応していない
  protocol を使用することになる。
- **iOS の quiche は `IPHONEOS_DEPLOYMENT_TARGET=17.0` を固定する。** boring-sys の
  clang は SDK の既定値に従い、rustc は自身の最低バージョンで link するため、この不一致
  は link 時に未定義の `___chkstk_darwin` として現れる。
- **clock は 2 つ用意し、それぞれ用途を分ける。** `NowUs()` は単調時計（起動からの秒数）
  であり、時間間隔に用いる。`NowUnixSeconds()` は日付として表示できる唯一の時計である。
  両者を混同しても明確なエラーは生じない。保存した単調時刻は 1970 年 1 月 1 日のある
  時刻として表示される。
- **Windows の PTY の子プロセスには標準ハンドルを渡さない。** host 自身の stdout が
  リダイレクトされている場合、Windows はそのリダイレクトを pseudo-console の属性を越え
  て引き継ぎ、shell はパイプと通信する。ハンドルを一切渡さない場合にのみ、shell は接続
  されている ConPTY を使用する。
- **Windows の terminal グリッドには `wxWANTS_CHARS` が必要である。** これがない場合、
  frame のダイアログナビゲーションが Enter、Tab、方向キーを terminal より先に受け取る。
- **macOS の TCC は許可とコード署名を結び付けている。** ローカルで build した app.app
  （ad-hoc であり、build のたびに署名し直す）と Developer ID の dmg は同一の
  `com.deskhub.macos` のエントリを共有する。System Settings では許可済みと表示される
  一方、起動したコピーは拒否され、Accessibility については通知もない。
  `make reset-macos-permissions` はすべての許可を消去し、次回の起動で再度確認させる。
  そのため Debug build は `com.deskhub.macos.debug`（"Deskhub Dev"）として動き、dmg と
  エントリを共有するのはローカルで build した Release のコピーだけになる。
- **Debug build はインストール済みの release に決して触れない**：app と CLI のすべての Debug
  build（Debug 構成で CMake が設定する `DESKHUB_DEV_BUILD`）はデータを `~/.deskhub` では
  なく `~/.deskhub-dev` に保存する。これがないと、古い release をインストールしたマシンで
  新しい build を動かしたとき、新しい build がその release の許可済み client 一覧を削除し
  （新しい build は廃止したファイルを削除する）、macOS では共有する bundle id のせいで
  実行中の app まで終了させていた。
- **iOS は Deskhub フォルダを App Group のコンテナ内の一段下に置く**：データフォルダは
  Deskhub が所有し `0700` に保てるものでなければならないが、コンテナのルートは iOS のもので、
  iOS はそのモード変更を拒否する。ルートを直接指していたときは、key、`known_hosts`、
  `authorized_keys` の読み書きがすべて "cannot be read" または "could not be saved" で
  失敗した。app と broadcast extension はどちらもコンテナを `SetAppDataDirInside` に渡し、
  これがデスクトップと同じ `.deskhub`（または `.deskhub-dev`）という名前を付け足すので、
  両者は引き続き同じフォルダを共有する。
- **macOS は CI ではデスクトップ build、release では署名 build であり、同時に両方を行わ
  ない。** `build-desktop` は push のたびに app を ad-hoc 署名でコンパイルするため、
  build できなくなった Cocoa の変更はその pull request で失敗する。`deploy` は
  `release-macos`、すなわち Developer ID、notarization、dmg からなる fastlane の経路を
  通じて同じ app を扱い、利用者が実際に開ける成果物を生成する。したがって再利用可能な
  workflow は `for_release` が設定されている場合に macOS の job を省略する。そうしなけ
  れば、tag ごとに 2 台目の macOS runner を消費して、配布しない bundle を生成すること
  になる。`build-mobile` は iOS と Android のみを扱い、`for_release` を受け取らない。
  `deploy` はこれを呼び出さず、その `release-ios` と `release-android` の job が自ら
  fastlane で build する。
- **すべての workflow は quiche と opus を同一の action から取得し、cache key が取り決め
  のすべてを表す。** `.github/actions/third-party` は job が指定した任意の target に
  ついて両ライブラリを build する。これにより、同一の「キャッシュしてから build する」
  ブロックを各 job が複製する代わりに、それを必要とする 25 の job がそれぞれ 1 行で済む。その `cache-key` 入力は、
  2 つの job が互いのライブラリを復元することを防ぐ唯一の手段である。target の組み合わせ
  が異なれば別のものであり、同じ triple を build する 2 つの runner イメージも別のもので
  ある。ubuntu-latest でコンパイルし ubuntu-22.04 で復元した `libquiche.a` は、この
  release が回避しようとしている glibc に link することになる。build の成果物を変える
  要素はすべてその key に含める。
- **Windows ではすべての configuration で同一の静的 release CRT を使用する。** cargo は
  静的 release CRT で quiche を build する。これは前項のとおり target ごとの
  `CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS` と `/MT` で強制するものであり、包括的
  な `RUSTFLAGS` で強制してはならない。proc-macro に波及して cargo が失敗する。CMake ツリー全体も
  `MultiThreaded` を固定して整合させており、これが app を VC++ Redistributable 不要の
  単一の exe に保つ要因でもある。Rust には debug CRT の build がないため、Debug 構成も
  同様に揃える。`_ITERATOR_DEBUG_LEVEL=0`、`/U_DEBUG`、`/RTC1` の除去である。release の
  CRT には `_CrtDbgReport` がなく、run-time check にも対応していないためである。不一致
  があれば多数の LNK2038 で終わる。
- **passcode と LAN scan は削除したままである。** 4 桁のコードは開いた port 上の短い
  秘密であり、平文の discovery への応答は network 上の誰に対しても host の存在を知らせて
  しまう。それ以降、どちらも復活していない —— QR code は画面から読み取るものであり、要求は
  TLS handshake の完了と署名の検証の後にのみ書き込まれる。

- **承認は authenticate 済みの経路に乗り、秘密ではなく identity を示す。** 2026-09-28 に
  承認プロンプトへ向けられた反対は、誤った人物が誤ったマシンのためにクリックしうると
  いうものだった —— passcode のプロンプトは誰でも入力しえたコードを表示していた。接続
  要求は入力されたものを何も示さない。デバイスの名前、実際に保持している key の
  fingerprint（要求は client がその key でこの connection の transcript に署名した後に
  のみ書き込まれるため、他のマシンの名義で要求を出すことは誰にもできない）、送信元
  アドレスで
  あり、*Approve* は行の位置ではなくその fingerprint に対して作用する。平文で流れるもの
  はなく、推測できるものもない。所有者が誤りうるのは想定していないマシンを承認すること
  だけであり、それを確認できるようにするために行が存在する。host は待機中も connection
  を開いたままにしない —— `AwaitingApproval` を返して 2 秒後に connection を閉じ、要求は
  ファイルのエントリであり、client が再接続する —— ため、要求の flood は 16 個の socket
  ではなく 16 行を費やすだけである。各行は送信側に自前の key による署名 1 回を課すが、
  新しい key は安価なので、flood を抑えるのは key ではなく送信元アドレスである。
  `AccessRequests::Add` は IP アドレス（port は無視）ごとに 1 行を保ち、そのアドレスからの
  新しい要求は、どの key を持っていてもその行を置き換える。QUIC は `AuthStart` を読む前に
  peer のアドレスを検証しているので、送信者は自分が受信できないアドレスを名乗れない。
  正規の要求を押し出すには 16 のアドレスが要り、残るコストはアドレスを共有するデバイス
  （同じ NAT）にかかり、それらには自分たちの最新の要求しか残らない。アドレスごとの
  レート制限は加えていない。1 つのアドレスはもともと 1 行しか持てないので、レート制限を
  加えても表が失いうるものは変わらない。

- **QR code は host の fingerprint と 1 回限りの token を運び、`AuthStart` の前に固定
  する。** token は 5 分間盗む価値のある秘密であるため、client は、code に印字された
  fingerprint の private key を保持していることを TLS handshake で既に証明したマシンに
  対してのみそれを使う。code 内のアドレスにいる中間者はその key を提示できないため、
  client は招待不一致の失敗で停止し、token はネットワークを渡らない。host 側では token
  を定数時間で比較し、初回使用で消費し、5 分で失効させ、それを表示したパネルとともに
  消し、誤った推測は不正な署名を数えるのと同じ limiter で送信元アドレスに課す —— 1 分に
  3 回、その後 10 秒間ブロック —— ため、2^256 の可能性が高速に試されることはない。

- **要求と token はファイルに置き、別の process が作用できるようにする。** iOS では
  broadcast extension が `AuthStart` を受け取る一方で、app が QR code と要求の一覧を描く。
  CLI では `share` が動作している間に、別の terminal で `access approve` が入力される。
  `access_requests` と `pairing_tokens` は共有の設定フォルダに、`authorized_keys` と同じ
  lock と atomic な置換の下で置かれ、`AccessRequestsGeneration` が poll する側に安価な
  変更カウンタを与え、*Approve* は一方のファイルから他方への移動にすぎず、次の
  `AuthStart` がそれを読み戻す。

- **マシンごとに 1 つの key、certificate はメモリ上。** マシンごとに 2 つの key があると、
  fingerprint が 2 つ、*My keys* ページ、import と passphrase のコード、key と食い違い
  うる保存済み certificate、そしてどこでどの client key を使うかを記憶しなければならない
  `known_hosts` を意味した。`host_key.pem` の 1 つの ECDSA P-256 key が host 側の TLS と
  client 側の transcript 署名を担い、TLS が要求する X.509 は起動ごとにその周りに構築
  され、保持されることはない —— ディスクに触れるのは quiche が読み込む一瞬だけであり、
  そのファイルが含むのは公開 certificate のみで、private key は `host_key.pem` から出る
  ことがない。
  fingerprint は常に certificate ではなく SPKI の SHA-256
  だったため、アップグレードした host はすべての client が固定していた fingerprint を
  保った。client の identity は変わった —— Ed25519 から machine key へ —— ため、すべての
  client は貼り付けではなく Approve かスキャンで、改めて一度許可される。

- **QR encoder は本プロジェクトで実装している。** `core/` はサードパーティのヘッダを
  許さず、プラットフォームごとの QR ライブラリでは 1 つの code の描画が 5 通り、CLI 用に
  6 通り目が必要になる。`core/qr/QrCode` は誤り訂正レベル M のバイトモード encoder で、
  既知の encoding に対してオフラインでテストされ、すべての client はそれが返すモジュール
  グリッドの四角を塗るだけである。decode は逆の事情 —— カメラと高速な検出器が必要 ——
  であるため、2 つのスマートフォンはプラットフォーム自身のもの（Android は CameraX +
  ZXing、iOS は AVFoundation）を使い、文字列を返す。

- **Base64 は 1 箇所に置く。** OpenSSH の key 行と招待レコードの両方が必要とし、2 つの
  コピーは既に食い違い始めていた。`core/net/Base64` が標準と URL-safe の両アルファベット
  に対する唯一の encoder と decoder であり、独自のテストを備える。
- **VT emulator は本プロジェクトで実装している。** 5 つの client すべてで利用でき、かつ
  適切なライセンスを備えたプラットフォーム標準の terminal ウィジェットは存在しない。
  自前で実装することで、terminal の挙動をオフラインでテスト可能にし、各プラットフォーム
  で同一にできる。
- **host 側の shell mirror は最初のバイトから更新する。** PTY の出力は破壊的な単一
  消費者の stream であり、読み取って viewer に送ったバイトは後から再生できない。
  したがって *Stop & attach* が開くグリッドは、バイトが通過する時点で構築しなければ
  ならず、ボタンが押された時点では構築できない。リモートの viewer が接続している間、
  mirror 自身の terminal query への応答は破棄する。viewer の画面が既に応答しており、
  shell が 2 つの応答を受け取ってはならないためである。
- **port は 1 つのみ使用する。** 画面、terminal、file transfer は 1 つの listener を共有し、
  connection と stream の多重化は QUIC が担う。かつて 2 つ目の port が存在したのは、
  QUIC 導入前の画面の経路が socket を占有していたことによる。
- **1 つの `HostLink` が従来の 4 つの handshake を置き換える。** dial、trust の確認、
  auth、recovery は、client 側でかつて 4 回実装されていた。source の問い合わせ、viewer、
  file sender、および独自の `QuicEndpoint` 上で動作する terminal である。このため、
  ファイル送信の部分は host key の変更を viewer より 3 つの修正ぶん遅れて認識していた。
  現在、client 側で dial または authenticate を行うコードは `HostLink` のみである。
  service は自身の `Chan` を開き、専用の inbox キューを受け取り、自身の thread で処理
  する。terminal の backoff を伴う再接続は link に移され、recovery を必要とするすべての
  画面がこれを継承する —— 承認待ちも同じ再接続を再利用する —— trust の規則も 1 箇所に
  集約されている。未知の key は利用者がそれを信頼する（`acceptNewHostKey`）まで link を
  失敗させ、招待はそれが名指しする key だけを固定し、既知の key はどのアドレスでも
  認識される。
- **`HostLink` は `SendMessage` ではなく `Send` で送信する。** Windows では platform 層
  の背後にある OS ヘッダが `SendMessage` を `SendMessageA` のマクロとして定義しており、
  `HostLink.cpp` ではそれがクラス宣言の後、メソッド定義の前に位置していた。その結果
  MSVC は、いずれのヘッダも宣言していない `SendMessageA` というメンバの定義を要求した。
  Win32 の API 名（`SendMessage`、`PostMessage`、`CreateWindow`、`GetObject` など）は、
  OS ヘッダが到達しうるいかなる translation unit においてもメソッド名として適切では
  ない。対処は改名であり、`#undef` ではない。
- **portal の ScreenCast session は D-Bus 接続と寿命を共にする。** GLib は共有の
  session bus を弱参照でキャッシュするため、最後のハンドルに `g_object_unref` を行うと
  接続そのものが破棄される。その後 `xdg-desktop-portal` は session を解放し、
  compositor は PipeWire のノードを破棄し、portal が直前に提供したノード id はどこも
  指さなくなる。stream は `paused` に至り、*no target node available* で失敗する。
  したがって `PortalScreenCast` は呼び出しごとに借用するのではなく、session が開いて
  いる間は自身で `GDBusConnection` を保持する。デスクトップの app がこの問題を長く
  覆い隠していたのは、GTK がプロセスの寿命の間 session bus への参照を保持するためで
  ある。`deskhub-cli` は GTK を link しないため、その参照を持たなかった。
- **キャプチャする画面ごとに自分の PipeWire リモートを開く**：portal のセッションが
  `OpenPipeWireRemote` から渡す fd は一つで、それを複製しても二本目の接続にはならない —
  `dup` は同じソケットを指す別のディスクリプタにすぎない。複製に対して
  `pw_context_connect_fd` を二回呼ぶと、独立した proxy-id マップを持つ `pw_core` が二つ
  できて一本のバイトストリームを読み書きする。id はデーモンが持つ単一のクライアント id
  空間で衝突し、先に epoll が起きたスレッドが相手宛のメッセージを — バッファのメモリを運ぶ
  `SCM_RIGHTS` ディスクリプタも含めて — 飲み込んでしまう。競争に負けたストリームはリンクの
  割り当て段階で *Buffer allocation failed* となって死ぬ。モニタが一台なら必ず動き、二台だと
  運任せだったのはこれが理由である。そこで `ScreenCapture::Start` は
  `PortalScreenCast::OpenRemoteFd()` を呼んで自分専用のリモートを取る。portal は start 済み
  のセッションに対する `OpenPipeWireRemote` の再呼び出しを許している。セッションが最初の fd を
  持ち続けるのは、portal がリモートを渡すことの確認と `isOpen()` の裏付けのためだけである。
- **すべてのアイコンは 1 つの原本から生成し、角を丸めるのは一部のみである。**
  `make icons` は唯一のマスターである `assets/icon_1024.png` からセット全体を再生成
  する。macOS、iOS、Play Store の掲載、Android の adaptive-icon のパイプラインは、
  いずれも図形を各自の形状でマスクするため、これらのアセットは全面の正方形のままと
  する。Windows、Linux、API 26 より前の Android のランチャーは与えられた図形をその
  まま描画するため、これらのアイコンには角の丸めと透明部分をあらかじめ含める。そうしな
  ければ、この app は丸いアイコンが並ぶ中で角の立った四角として表示される。
  `scripts/make-icons.py` が標準ライブラリのみを使用しているのは意図的であり、bootstrap
  が画像処理ツールを導入しないためである。Linux のウィンドウアイコンはインストール済みの
  hicolor テーマから取るが、build フォルダから実行するビルドにはそれがないため、すべての
  Linux ビルドは `deskhub-256.png` をバイナリの隣にコピーし、テーマに `deskhub` アイコンが
  ない場合 `gtk/main.cpp` はそのファイルにフォールバックする。
- **デスクトップの client は複数の host を同時に保持し、スマートフォンは 1 つのみ保持
  する。** Windows、Linux、macOS の connect ページは接続状態を保持しない。応答した host
  ごとに接続ウィンドウが割り当てられる。`client/windows/win32/MainFrame.cpp` の
  `ConnectionFrame`、`client/linux/gtk/MainWindow.cpp` の `ConnectionWindow`、
  `client/macos/app/swift/App.swift` の `connection` という `WindowGroup` であり、
  それぞれがその host のアドレス、capability、source 一覧、control の選択を
  保持する。これにより connect ページは次の host に接続できる状態を保つ。メイン
  ウィンドウは開いているウィンドウの一覧のみを保持し、同じ host に再度接続した際に該当
  ウィンドウを前面に出すため、status の probe をアドレスの一致するウィンドウへ渡すため、
  および終了時にすべてを閉じるために使用する。Android と iOS は意図的に単一接続を維持
  する。スマートフォンの画面には 2 つ目のパネルを置く余地がなく、開く session はいずれ
  にせよ全画面であるためだ。各所での「同じ host」の定義は `ui::SameDeviceAddr` である
  —— 次の項目を参照。
- **同じ host に対して 2 通りのアドレス表記が存在するが、比較方法は 1 つである。**
  アドレスは既定の port を付けても付けなくても表記できるため、`192.168.1.60` と
  `192.168.1.60:47777` は同じ host を指す。この 2 つを文字列として比較すると通知なく
  失敗する。接続済みのパネルは該当する最近の行を見つけられず、1 つの host に対して接続
  ウィンドウが 2 つ開いていた。したがってアドレスの等価判定は
  `ui::NormalizedDeviceAddr` と `ui::SameDeviceAddr`（`core/ui/Strings.h`）を経由させる。
  デバイスのアドレス同士を `==` で比較してはならない。

- **開いたばかりの decoder は参照 frame を保持していない。** `ScreenViewer` は surface
  が変化するたびに decoder を再構築し、iOS の app は画面から離れる際に surface を返す。
  端末をロックするだけでこの状況になる。reassembler はこれを認識しないため、従来どおり
  P-frame を配信し続けるが、新しい decoder には予測の基となるデータがなく、host は要求
  された場合にのみ IDR を送るため、映像は session の残りの間表示されなかった。keyframe
  の要求は以前、*旧*い decoder が破棄される時点で送出されていたが、それは描画対象の
  surface が存在しない時点でもある。IDR は到着したものの decode ループが surface の
  不在により破棄し、`CancelKeyframeRequest` が保留中の要求も消去していた。現在
  `EnsureDecoder` は開いたすべての decoder についてこのフラグを設定するため、描画先が
  ある状態で keyframe が要求される。`MediaCodecDecoder` には対応する問題が反対側に
  あった。最初に渡された frame が parameter set を含まない場合でもその時点で `sentCsd_`
  を確定させていたため、後続の keyframe の SPS/PPS が通常のデータとしてキューに入り、
  codec を設定することがなかった。現在は実際にそれらを含む frame を待つ。decoder を
  開いた側が keyframe を要求する。

- **バックグラウンドに入った `AVSampleBufferDisplayLayer` は frame を通知なく破棄する。**
  app が画面から離れると iOS は layer の decode を停止し、
  `requiresFlushToResumeDecoding` を設定する。`flush` を呼ぶまで、
  `enqueueSampleBuffer` はすべて受理されたうえで破棄される。これを示す手掛かりは他に
  ない。`status` は `failed` ではなく、`isReadyForMoreMediaData` は true のままで、
  レンダラもエラーを報告しないため、viewer は計数上は正常に動作しながら黒画面を表示して
  いた。`VtDecoder` は現在、layer 上で開く際にこのフラグを確認し、各 frame の前にも再度
  確認し、flush を行い、その frame を失敗させる。keyframe の要求が併せて送出されるよう
  にするためである。

- **QUIC の service ループが行うすべてのコールバックは、対象の connection を削除しうる。**
  `Service()` は connection id のスナップショットを走査し、id ごとに再度検索する。
  `cb_.onConnected`、`cb_.onStream`、`cb_.onDatagram` はいずれもアプリケーションの
  コードを実行し、それが peer を閉じて `connections_` から削除しうるためである。
  `DrainStreams` は自身が行う各コールバックの後に再確認する。`listStillIntact` という
  名の guard はそのために存在する。しかしそれは自身から return するのみであるため、
  `Service()` はそのまま `DrainDatagrams(id, entry)` に進み、この時点で `entry` は既に
  削除・解放されている。そして当該関数が最初に行うのは `entry.conn` を
  `quiche_conn_dgram_recv` に渡すことである。`Lookup(id) != &entry` の確認は 2 回の
  drain の後に置かれており、1 段階遅い。Windows の CI では、これはおよそ 3 回に 1 回の
  実行が `0xc0000409` または `0xc0000374` で終了する形で現れた。長期間特定できなかった
  のは、fastfail が `tests/integration/TestMain.cpp` の
  `SetUnhandledExceptionFilter` に到達せず、失敗した実行が exit code しか残さなかった
  ためである。加えて、この問題を特定するために用意した 2 つの job はいずれも検出でき
  なかった。page heap では検出できない。解放されたブロックは quiche 自身のものであり、
  corruption は破棄済みの connection がその後に書き込んだ内容だからである。Rust-checks
  の build でも検出できない。quiche の内部に誤りはないからである。最終的に該当の frame
  を特定したのは Windows の ASan job であった。コールバックを実行しうる呼び出しのたびに
  entry を再検証すること。ブロックの末尾で 1 度だけ検証してはならない。

- **liveness の watchdog は、自身のループが動作している間しか対向を測定できない。**
  viewer の 5 秒の pong ウィンドウは実時間で計測していたため、こちら側のいかなる停止も
  host の応答停止と判定されていた。Windows の ASan CI job では、動作中の stream と並行
  した 32 MB のアップロードがプロセス全体を 3.7 秒停止させた。`t=07:46:58` と
  `t=07:47:00` のログ行がいずれも 07:47:01 に出力され、4 つの QUIC endpoint がその時点で
  それぞれ数秒の poll 間隔を報告し、`HostLink` は正常な link を失われたと判定した。その
  後の再接続により client は新しい送信元 port に移り、host 側の従来の connection は 30
  秒の idle timeout で閉じられ、送信中の batch も中断された
  （`transfer aborted ... link-lost`）。その結果
  `TestInputStaysLiveDuringABigTransfer` は 120 秒の期限を消費した。現在 `LinkPulse::Tick` は `PumpReady` の 1 巡ごとに実行され、1 巡が
  `kLinkWatchStepUs` を超えた分をすべて差し引く。無音時間は、こちらが実際に監視できて
  いた間のみ計上する。ローカルの時計で遠隔の相手を計測する watchdog は、自身が観測して
  いなかった時間を差し引かなければならない。そうしなければ、最初に検出するのは自機の
  状態である。

- **connection より長く存続する転送には、その旨を通知しなければならない。**
  `FileSender` が `Sending` 状態を離れるのは ack、cancel、`LinkLost()` を受け取った場合
  のみであり、`FileUpload::Pump` は拒否された送信を失敗ではなく backpressure として
  扱う。`onStreamBroken` と session の終了には接続されていたが、自身の `HostLink` が
  接続を失ったことには接続されていなかった upload は、転送の途中での再接続の後に
  `Sending` のまま残り、対向側には応答できる構成要素が存在しなかった。host は既に batch
  を中止しており、新しい connection の receiver はその offer を受け取っていない。そのため
  `FileTransferClient` は転送の途中で再接続せず、link が `Ready` を離れた時点で
  `TransferReason::LinkLost` として upload を失敗させる。再接続をまたいで転送を継続する
  には、新しい connection で offer を再送する必要がある。その機能が実装されるまでは、
  転送を明示的に終了するほうが、変化しない進捗バーを残すより適切である。

- **host が手放した socket を、それが生んだ shell がなお抛えている**：`Pty::Start` は
  `forkpty` を使うため、子プロセスは開いている記述子をすべて引き継ぎ、`ChildSetup` は
  一つも閉じずに shell を exec する。セッションの UDP socket も一緒に付いていく。
  terminal host を止めるとき、`Pty::Impl::Shutdown` は `SIGHUP` を送って `WNOHANG` で
  回収する —— 待たない —— ので、shell が死ぬまでの間はポートを押さえたままだ。Deskhub
  自身の記述子はもう閉じていてもである。ASan 下で platform スイートが
  `bind(127.0.0.1:47793)` で `EADDRINUSE` となって失敗した。前のテストの shell がまだ
  終了していなかったのだ。`UdpSocket::Open` は `FD_CLOEXEC` を立てるようになり、shell が
  exec した瞬間に記述子は消え、ポートは host だけのものになる。ユーザの shell を
  fork するプロセスの長命な記述子はすべてこれを必要とする。親で閉じるだけでは足りない。
  ただし close-on-exec には隙間が一つ残っていた。`forkpty` と `exec` の間にある
  子プロセスは依然として複製を保持しており、service ループが poll ごとに 1 ミリ秒を
  費やさなくなって次のテストが早く始まるようになると、同じ `bind` が ASan の実行の
  たびに失敗した。そのため `Shutdown` は hangup の後、最大 200 ms の間子プロセスを
  回収し、それを過ぎれば kill する。これにより、閉じた端末が zombie を残すことも
  なくなる。master はこの待機の後ではなく前に閉じる。macOS では、終了中の shell は
  誰にも読まれていない出力が排出されるまで端末の close でブロックするため、master が
  開いたままでは kill された子プロセスでさえ終了を完了できず、最後の `waitpid` は
  戻らず、macOS のすべてのテスト job が timeout まで走り続けた。
- **Windows app はクリップボードを `wxClipboard` ではなく Win32 経由で扱う**：`capture::InitRuntime` は
  Windows.Graphics.Capture とエンコーダースレッドが暗黙の MTA に依存するため UI スレッドをマルチスレッド
  COM apartment に入れる。その結果 wxWidgets の `OleInitialize` は起動時に失敗し、OLE 経由のクリップボード
  呼び出しはすべて `CO_E_NOTINITIALIZED`（"Failed to put data on the clipboard … CoInitialize has not been
  called"）を返す。`OpenClipboard`/`SetClipboardData` は apartment を必要としないため、Copy ボタンと
  クリップボード同期はこれらを使う。UI スレッドを STA に切り替えると、`CoInitialize` を一度も呼ばない
  ワーカースレッドがすべて apartment を失う。
