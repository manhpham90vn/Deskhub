[English](PRIVACY.md) · [Tiếng Việt](PRIVACY.vi.md) · [中文](PRIVACY.zh.md) · **日本語**

# Deskhub プライバシーポリシー

_発効日: 2026 年 9 月 30 日 — バージョン 2.11_

> 本書は [`PRIVACY.md`](PRIVACY.md) の翻訳。内容に差がある場合は英語版を優先する。

## 1. はじめに

本プライバシーポリシーは、**Deskhub**（以下「本 app」または「当方」）が、Deskhub の
モバイルアプリケーション（iOS、Android）および Windows・macOS・Linux 向けの Deskhub
デスクトップアプリケーション（あわせて「本ソフトウェア」）の利用時に、情報をどのように
取り扱うかを説明するものである。

Deskhub は remote desktop のアプリケーションであり、利用者のコンピュータの画面を別の
端末へ stream し、その端末から mouse、keyboard、タッチ input によって当該コンピュータを
操作できるようにする。

本ソフトウェアは個人の開発者が開発・公開している。

- **開発者:** Manh Pham
- **連絡先:** manhpv151090@gmail.com
- **プロジェクトページ:** https://github.com/manhpham90vn/Deskhub

## 2. 要旨

**開発者が Deskhub を通じて利用者の session 内容や利用状況のデータを受け取り、
保存することはない。** app は利用者の端末上で一部の情報を処理・保存する。
詳しくは以下に記載する。Deskhub にユーザーアカウント、開発者が運営するサーバ、
analytics、crash reporting、広告、組み込みの第三者 SDK はない。

## 3. 本ソフトウェアが処理する情報

動作のために、本ソフトウェアは一部のデータを処理する。処理は**すべて利用者自身の端末上
および端末間でのみ**行われる。そのいずれも、開発者および第三者へ送信されることはない。

| データ | 目的 | 送信先・保存先 | 保持期間 |
|---|---|---|---|
| 共有されているコンピュータの画面内容（video の frame） | その画面を利用者の別の端末に表示するため | 2 台の端末間で直接送信し、転送中は encrypt（QUIC/TLS） | 保存しない。session の間のみメモリ上に存在する |
| 共有されているコンピュータが再生している音声（当該コンピュータが音声を共有し、viewer が要求した場合のみ） | 閲覧者がそのコンピュータの音声を聞けるようにするため | 2 台の端末間で直接送信し、転送中は encrypt（QUIC/TLS）。圧縮された音声として送る | 保存しない。session の間のみメモリ上に存在する |
| Mouse、keyboard、タッチの input | 別の端末から、共有されているコンピュータを操作するため | 閲覧側の端末から共有されているコンピュータへ直接送信し、転送中は encrypt（QUIC/TLS） | 保存しない。inject 後に破棄する |
| 本端末の key（初回起動時に生成される 1 つの秘密鍵） | 本端末の identity を双方向に証明するため。共有時には接続してくる端末に対して、connect 時には接続先の host に対して。利用者には 1 つの fingerprint（`SHA256:…`）として表示される | app 自身のフォルダの `host_key.pem` に保存する。private key は端末の外に出ない。共有時には、この key からメモリ上で構築した certificate —— 保存はしない —— を接続してくる端末に提示し、connect 時には public key と署名のみを送信する。*Copy public key* は、本端末の名前をラベルとした public key を clipboard に置き、利用者が host の所有者に渡せるようにする。旧バージョンの `host_cert.pem` と `client_key*.pem` のファイルは読み取られなくなった | 利用者がファイルを削除するまで保持する。自動的に置き換えられることはない。削除すると本端末は新しい identity となり、旧 identity を許可していた host は改めて許可する必要があり、旧 identity を信頼していた端末には新しい host として表示される |
| 信頼済み host（固定した key の fingerprint、名前、最後に応答したアドレスと port） | 本端末が信頼した host を、どのアドレスに現れても識別し、信頼済み host のものだったアドレスで別の key が応答した際に警告するため | 同じフォルダの `known_hosts` に保存する。送信しない | host を削除するかファイルを削除するまで保持する。アドレスは接続ごとに更新される |
| 本 host への connect を許可した client public key と各ラベル | 対応する private key を保持していることを証明した client だけを受け入れるため | 同じフォルダの `authorized_keys` に、1 行に 1 つの key とそのラベルとして保存する。時刻は記録しない。送信しない。key が追加されるのは、利用者が貼り付けたとき、その端末の接続要求を承認したとき、またはその端末が本 host の QR code をスキャンしたときであり、後の 2 つの場合、ラベルは端末が送った名前になる | key を削除するかファイルを削除するまで保持する。ファイルがなければ誰も connect できない |
| 接続要求 —— まだ許可されていない状態で本 host に接続を試みた各端末の名前、public key、アドレス、時刻 | 本 host の所有者が、誰が求めているかを確認し、*Approve* または *Deny* で判断できるようにするため | 要求する端末は encrypt された connection 上で名前と public key を送り、本 host はそれを、確認したアドレスと時刻とともに、同じフォルダの `access_requests` に書き込む。2 台の端末の外には送信されず、本 host 自身の画面にのみ表示される | 最大 16 件。それぞれ 10 分後、または *Approve*（key を `authorized_keys` に移す）か *Deny* を押した時点で削除される |
| QR pairing token —— 本 host が QR code を表示している間に発行するランダムな 1 回限りの code | code をスキャンした 1 台の端末を、追加の手順なしに受け入れるため | 同じフォルダの `pairing_tokens` に、各 token の期限とともに保存する。token は利用者が見せる QR code とリンクの中を移動し —— その画面を見られる者は誰でも読み取れる —— スキャンした端末から encrypt された connection 上で一度だけ送られる。QR code には本端末の network アドレスと port、key の fingerprint、デバイス名も含まれる | code を隠したとき、共有を停止したとき、token が使用されたとき、または 5 分後に削除される |
| QR code をスキャンしている間のカメラの frame（Android と iOS のみ） | host の QR code をその画面から読み取るため | code を見つけて decode するために端末上でのみ処理する。保存も送信もされず、誰にも表示されない | 保存しない。各 frame は検査後に破棄される |
| 利用者が入力するアドレス（IP または hostname） | 相手のマシンへ接続するため | 入力した端末内にのみ保持する | 利用者が変更するまでローカルに保持する |
| 直近 10 件の接続先 host —— アドレス、最後に接続した時刻、host が自ら名乗った名前 | *Recent devices* の一覧を構成するため | 利用者の端末上、app 自身のフォルダの `recent-hosts.txt` に 1 行 1 host で保存する。Windows は `%USERPROFILE%\.deskhub`、macOS と Linux は `~/.deskhub`、iOS と Android は app のサンドボックス。送信されることはない。旧バージョンの `recent-devices.txt` は変換されず、削除される | より新しい host へ 10 件接続するか、ファイルを削除するまで保持する |
| 共有に関する設定（frame rate、bitrate、解像度の上限、port、network アドレス、viewer による操作の可否、clipboard sync・音声・keep awake・OS 起動時の開始・自動共有・バックグラウンドモードの各トグル） | 次回 app を開いた際に settings を復元するため | 同じフォルダの `ui-settings.txt` に保存する。iOS では app と broadcast extension が共有する app group のコンテナに置く | 利用者が変更するか、ファイルを削除するまで保持する |
| Linux のデスクトップが、その画面共有ダイアログで display を選択した後に発行する画面 permission の token（Linux のみ） | 以後の共有でその選択を再利用し、ダイアログが初回のみ表示されるようにするため | 同じフォルダの `portal-restore-token.txt` に保存する。この token は本マシン上の利用者自身のデスクトップ session にのみ意味を持ち、送信されない | 共有のたびに置き換わる。*Choose screens again* を選択するか、ファイルを削除すると消去される |
| Clipboard のテキスト（clipboard sync のトグルが on で、session が動作している場合のみ） | ある端末でコピーしたテキストを他の端末で貼り付けられるようにするため | 端末間で直接送信し、転送中は encrypt（QUIC/TLS）。1 回のコピーにつき 32 KiB を上限とする。プレーンテキストのみで、画像やファイルは含まない | Deskhub は保存しない。各端末の通常のシステム clipboard 内にのみ存在する |
| broadcast が動作中かどうか、接続中の viewer の数、broadcast extension 自身のメモリ使用量（MB）、直近の起動エラーの文言（iOS のみ） | app の共有画面が broadcast extension の状態を表示できるようにするため。iOS はこれを独立した process として動作させ、メモリ使用量が上限を超えた場合に終了させる | 同じ app group のコンテナの `broadcast-status.txt` に保存する | broadcast の終了時に削除する |
| Settings → General → *Device name* のデバイス名。空の場合は、当該コンピュータまたは端末自身の名称が使われる（Windows と Linux は hostname、macOS はコンピュータ名、iOS はデバイス名、Android は機種名） | 本端末に名前を付けるため。共有時には viewer に表示され、本端末に接続する許可済み client にも表示され、接続先の host では本端末のアドレスの隣に表示され、host が本端末について記録する接続要求にも表示され、コピーする public key のラベルとして使われる | 同じフォルダの `ui-settings.txt` に保存し、connect 時に host へ送信する。転送中は encrypt されるが、host の画面に表示され、その log にも記録される。したがって自分で選んだ名称を設定しない限り、既定値が送信される。本端末をまだ許可していない host は、接続要求の一覧にこの名称を表示し、最長 10 分間保持する。本端末が共有しているときは、許可された key で認証を終えた各 client にも名称を送信し —— 認証前に送ることはない —— その client は自身の最近の一覧に名称を保持し、本端末が表示する QR code にも名称が書き込まれる。コピーする public key にも埋め込まれ、host が本端末を承認または QR code で受け入れた際に `authorized_keys` に保存するラベルにもなる | 利用者が変更するか、ファイルを削除するまで保持する。欄を空にすることは既定値への切り替えであり、名称の削除ではない |
| 接続中のコンピュータへ送信すると選択したファイル（利用者がファイルを選択し Send を押した場合のみ） | ある端末から別の端末へファイルを移すため | 2 台の端末間で直接送信し、転送中は encrypt（QUIC/TLS）。スマートフォンやタブレットでは、送信中に読み取れるよう、事前に app 自身の cache へコピーを用意する | ファイルの保存先は受信側による。コンピュータはそのために選択したフォルダ（別途選択しない場合は当該ユーザーのホームディレクトリ直下の `Deskhub`）へ書き込み、そのユーザーが削除するまで保持する。スマートフォンとタブレットには該当するフォルダがない。写真と動画は端末の写真ライブラリに追加され（Android では `Pictures/Deskhub` と `Movies/Deskhub`）、それ以外のファイルはシステムのファイルブラウザから参照できる場所、すなわち iOS では app の Documents フォルダ、Android では `Download/Deskhub` に置かれ、削除するまで残る。iOS ではライブラリが受け付けない写真は Documents に保存される。メディアストア経由の保存には Android 10 が必要であり、Android 9 以前では、届いたファイルは端末上の Deskhub 自身のフォルダに留まり、ギャラリーにも Downloads にも現れない。送信側の端末上の一時コピーは、送信ウィンドウを閉じた時点で削除する |
| 提示された各ファイルの名称、サイズ、checksum、および送信側端末の名称、アドレス、key fingerprint | 受信側のコンピュータが到着中の内容を表示し、保存できない内容を拒否できるようにするため。またその所有者が送信元を把握できるようにするため | 2 台の端末間で送信し、転送中は encrypt する。受信側のコンピュータは、提示、その判断、結果を自身の session log に記録する | 利用者が削除するまで、当該コンピュータの log ファイルに保持される |
| コンピュータが受信ファイルを保存するフォルダ | 次回 app を開いた際にその選択を復元するため | app 自身のフォルダの `ui-settings.txt` に保存する。送信しない | 利用者が変更するか、ファイルを削除するまで保持する |
| 接続の統計（bitrate、packet ロス、latency） | stream の quality を調整し、status バーに表示するため | 2 台の端末間でのみ交換する | 保存しない。session の終了時に破棄する |

### 3.1 設計としての peer-to-peer

すべての通信は**利用者自身の 2 台の端末間で直接**行われる。経路は次のいずれかである。

- 利用者のローカル network（Wi-Fi または LAN）
- **利用者自身**が運用または契約している VPN（たとえば Tailscale）。Internet 越しの
  アクセスに用いる場合。

当方は relay サーバ、signaling サーバ、その他のバックエンドを一切運用していない。本
ソフトウェアには、開発者へデータを送信する技術的手段が存在しない。

### 3.2 当方が処理**しない**データ

上記のデバイス名を除き、Deskhub は氏名、メールアドレス、電話番号、連絡先、位置情報、
広告 ID の入力を求めない。microphone も使用しない。カメラを使用するのは、スマートフォンや
タブレットで *Scan QR code* をタップして host の QR code をスキャンしている間に限られる。
frame は code を見つけるために端末上で decode され、保存も送信もされない。写真やファイルに
アクセスするのは、利用者が送信対象として選んだ場合、別の端末から受信した場合、
または共有する画面に表示された場合に限る。保存先と保持期間は上記に記載する。

### 3.3 スマートフォンやタブレットの画面共有

Android と iOS の端末は、他のマシンを閲覧できるほか、自身の画面を共有することもできる。
その stream は **view-only** である。受信した mouse と keyboard の packet は破棄する。
いずれの OS も通常の app に端末の操作を許可していないためである。capture の対象は
**画面全体**であり、共有中に表示されるものはすべて含まれる。通知、他の app、銀行の
app、入力したパスワードなどである。Android では共有のたびにシステム自身の録画同意
ダイアログが表示され、動作中は常駐通知が出る。iOS ではシステムの broadcast インジケータ
が表示され続ける。いずれも OS 自身の仕組みであり、どちらからでも共有を停止できる。
デスクトップと同様、video は利用者の別の端末へ直接届き、保存されることも当方へ送信される
こともない。

### 3.4 画面共有と遠隔操作の範囲

共有は**選択した display 全体**を stream する。当該モニタに表示される内容はすべて、
接続している viewer から見える。通知、ポップアップ、共有中に開いたあらゆるウィンドウが
含まれる。（アプリケーションのウィンドウを 1 つだけ共有する機能は 2026-07-27 に削除
した。本ソフトウェアは現在、display 全体のみを共有する。）遠隔操作を許可した場合、
viewer の input は、その利用者が PC の前にいるのと同様に inject され、**共有している
display 上に表示されているあらゆるアプリケーション**に作用する。単一のウィンドウには
限定されない。いずれの host でも Settings から遠隔操作を完全に無効化でき、その場合の
共有は view-only となり、到達した input は inject されずに破棄される。操作を許可して
いる間、2 つの安全機構が常に働く。PC の前の利用者が実際の mouse または keyboard を操作
した場合、remote input は停止する（host 優先）。また、リモート側が押している状態のキー
は、接続の終了時または viewer の切り替え時に自動的に解放される。1 台の PC を同時に閲覧
できるのは最大 5 viewer であるが、ある時点で mouse と keyboard を操作できるのはそのうち
1 つのみである。

## 4. App が要求する permission

| プラットフォーム | Permission | 用途 |
|---|---|---|
| iOS | Local Network | 同じ network 上の PC とデータを送受信するために iOS が要求する。stream の session にのみ使用する。 |
| iOS | 画面収録（broadcast） | システムの broadcast ピッカーから本端末の画面共有を開始する場合にのみ使用する。iOS は毎回確認を行い、実行中は録画インジケータを表示する。 |
| Android | `INTERNET`、ネットワーク状態 | PC への UDP 接続を確立するために必要である。stream の session にのみ使用する。 |
| Android | 画面 capture の同意（`MediaProjection`） | 本端末の画面共有を開始する場合にのみ使用する。Android は毎回確認を行い、その回答を保存することはできない。 |
| Android | `FOREGROUND_SERVICE`、`FOREGROUND_SERVICE_MEDIA_PROJECTION` | app がバックグラウンドに移行した場合や画面が消灯した場合にも共有を継続するために必要である。画面 capture に対する Android の要件である。 |
| Android | `RECORD_AUDIO` | Android は playback-capture API をこの permission の背後に配置しており、その playback、すなわち端末自身が再生している音声が、Deskhub が capture する唯一の対象である。共有の開始時に要求し、拒否された場合は音声なしで共有を継続する。Deskhub が microphone を開くことはない。 |
| Android | `POST_NOTIFICATIONS` | 画面共有中に Android が要求する常駐通知を表示し、他の端末からファイルが届いた際にその内容を通知する。それ以外の通知は送信しない。 |
| iOS | 写真ライブラリ（追加のみ） | 他者から送られた写真または動画が本端末に初めて届いた際に要求し、Photos app に追加するために使用する。Deskhub は項目の追加のみが可能であり、ライブラリ内の既存の内容を読み取ることも、変更することも、削除することもない。拒否された場合、ファイルは app の Documents フォルダに保存される。 |
| iOS | 通知 | 他の端末からファイルが届いた際にその内容を通知する。それ以外の通知は送信しない。 |
| Android | `CAMERA` | Client ページで *Scan QR code* をタップしたときにのみ要求し、host の QR code をその画面から読み取るために使用する。frame は端末上で decode され、保存も送信もされない。拒否した場合は、host のリンクをアドレス欄に貼り付けることで代替できる。 |
| iOS | カメラ | Client ページで *Scan QR code* をタップしたときにのみ要求し、Android と同じ目的・同じ制限で使用する。拒否した場合は、host のリンクを貼り付けることで代替できる。 |

デスクトップでは、音声の共有に専用の permission を必要としない。capture の対象は
コンピュータ自身が再生している音声であり、microphone ではないためである。Android は
例外であるが、それは名称上のことに留まる。playback-capture API が `RECORD_AUDIO` の
背後にあるため、音声を共有する Android 端末はシステムが *Microphone* と表示する
permission を保持する必要がある。Deskhub はこの permission を他の用途に使用せず、
microphone を録音せず、双方向の音声も持たず、他のいかなるプラットフォームでも
microphone の permission を要求しない。

App はこれ以外の permission を要求しない。将来のバージョンが新たな permission を必要と
する場合、その permission は該当する場面で要求し、本ポリシーも併せて更新する。

## 5. Analytics、広告、第三者

- **Analytics および telemetry:** なし。
- **Crash reporting:** なし。診断の log（`[DIAG]`）は利用者自身のマシン上にのみ存在
  する。app のコンソール出力、および Windows・macOS・Linux では `~/.deskhub/`
  （Windows は `%USERPROFILE%\.deskhub`）配下のプレーンテキストのファイルである。これ
  らがアップロードされることはなく、利用者自身が複製して送信しない限り端末外へ出ること
  はない。当該フォルダはいつでも削除できる。
- **広告:** なし。
- **第三者の SDK:** なし。本ソフトウェアは自身のソースコード（プロジェクトページで公開）
  と OS のフレームワークのみから構築されている。
- **アプリストア:** 本 app は Apple App Store と Google Play で配布している。Apple と
  Google は各社のプライバシーポリシーに基づきインストールおよび利用の統計を収集する
  場合がある。当該収集は当方の管理外であり、当方が受け取るのは、これらのプラット
  フォームがすべての開発者に提供する集計済みの匿名統計のみである。
- **Tailscale その他の VPN:** VPN 経由で接続する場合、当該データはその提供者の
  プライバシーポリシーに基づいて扱われる。Deskhub はいかなる VPN も要求せず、同梱も
  しない。

## 6. セキュリティ

- Stream のトラフィックは利用者自身の network または VPN トンネル内に留まる。
  Tailscale などの VPN を使用する場合、端末間のデータは当該 VPN（WireGuard）によって
  end-to-end で encrypt される。
- Deskhub は session のトラフィックを encrypt する。video、control、input、clipboard、
  terminal のデータはいずれも端末間で QUIC/TLS 上を通る。client は host が
  `authorized_keys` に記載した key で connection の transcript に署名する必要があり、
  何かを送る前に固定済みの host key を確認する。key が記載されるのは、host の所有者が
  その端末の接続要求を承認したとき、host の QR code を見せたとき、または public key を
  貼り付けたときに限られる。passcode は保存も送信もされない。
  Deskhub は利用者の network を scan せず、encrypt されていない discovery 要求にも応答
  しない。デバイス名は転送中 encrypt されるが host に表示され、接続要求にも表示され、
  コピーする public key にも埋め込まれるため、この項目に機微な情報を入力しないこと。Deskhub を Internet に直接公開しないこと。完全な threat model、保護
  される範囲、保護されない範囲、脆弱性の報告方法は
  [`SECURITY.ja.md`](https://github.com/manhpham90vn/Deskhub/blob/main/SECURITY.ja.md)
  に記載している。
- 旧バージョンが残したデータ —— passcode、以前の `paired_devices` の一覧、以前の
  有効化の印 —— は変換されず、残ったファイルは削除される。読み取れない
  `authorized_keys` や `known_hosts` のファイルは、読み取れない間はアクセスを拒否し、
  次の変更時に新たに書き込まれる。
- 当方は利用者に関するデータを保持していないため、侵害されうる開発者側のデータベースも
  存在しない。

## 7. データの保持と削除

当方はデータを保持しないため、当方が削除すべきものも存在しない。session のデータは
session の終了時に消える。app に保存されたアドレスは、該当する欄を空にするか app を
アンインストールすることで削除できる。最近使用した端末の一覧、保存済みの settings、
key、許可済み client、信頼済み host、待機中の接続要求、有効な QR token は、app のフォルダ
（Windows は `%USERPROFILE%\.deskhub`、macOS と Linux は `~/.deskhub`）を削除することで
消去できる。app は次回起動時に空のフォルダを
作り直す。iOS と Android では、app をアンインストールすればこれらは削除される。

他の端末から送られたファイルは app ではなく利用者に帰属する。到着後は、当該コンピュータ
が選択したフォルダ、またはスマートフォンやタブレットの写真ライブラリ、Documents、
Downloads に保存される。Deskhub をアンインストールしてもこれらのファイルは影響を受けず、
削除は各保存場所で行う必要がある。

## 8. 利用者の権利（GDPR、CCPA および類似の法令）

EU 一般データ保護規則（GDPR）やカリフォルニア州消費者プライバシー法（CCPA）などの法令
は、個人データに関する権利、すなわちアクセス、訂正、削除、可搬性、異議申立て、差別を
受けないことを定めている。

Deskhub は個人データを収集も保持もしないため、これらの権利を行使する対象となるデータが
存在しない。当方が利用者のデータを保持していると考える場合は、下記の連絡先まで連絡され
たい。30 日以内に回答する。

当方は CCPA が定義する個人情報の「販売」および「共有」を行わない。

## 9. 子どものプライバシー

本ソフトウェアは子どもを対象としておらず、上記のとおり、13 歳未満（COPPA）および 16 歳
未満（GDPR）の子どもを含め、いかなる利用者からもデータを収集しない。

## 10. 国際的なデータ移転

Deskhub は開発者にデータを送信しない。VPN 経由で接続する場合、session データは
3.1 節で説明した network を通って利用者の端末間を移動する。

## 11. 本ポリシーの変更

本ソフトウェアのデータの取り扱いが変更される場合（たとえば将来のバージョンが任意の
crash reporting を追加する場合）、本ポリシーはその変更が**出荷される前に**更新し、
新しい発効日と下記の変更履歴を付す。最新版は常に次の場所で公開する。
https://github.com/manhpham90vn/Deskhub/blob/main/PRIVACY.md

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 2.11 | 2026-09-30 | **端末ごとに 1 つの key、接続要求、QR pairing。** 各端末は、共有時と connect 時の双方で identity となる単一の key（`host_key.pem`）を持つようになった。別個の client key（`client_key*.pem`）と保存していた certificate（`host_cert.pem`）は廃止され、certificate はメモリ上で構築されて保存されず、残ったファイルは変換されずに無視される。信頼済み host は、アドレスではなく key の fingerprint と最後に応答したアドレスによって記憶される。app のフォルダに 2 つのファイルが追加され、いずれも関係する 2 台の端末の外には送信されない。`access_requests` は、まだ許可されていない状態で接続を求めた各端末の名前、public key、アドレス、時刻を保持する（最大 16 件、それぞれ 10 分後または *Approve* / *Deny* で削除）。`pairing_tokens` は、host が共有中に表示できる QR code の背後にあるランダムな 1 回限りの token を保持する（code を隠したとき、使用されたとき、または 5 分で期限切れになったときに削除）。QR code 自体は host のアドレス、port、key の fingerprint、デバイス名、token を含み、画面を見た者なら誰でも読み取れる。端末の名前は、その端末が残す接続要求にも表示され、host が承認または QR code で受け入れた際に key のラベルになる。Android と iOS では、QR code をスキャンしている間のみカメラを使用し、その時点で要求する permission の背後に置く。frame は端末上で decode され、保存も送信もされない。 |
| 2.10 | 2026-09-29 | host は、許可された key で認証を終えた各 client に自身のデバイス名を送信するようになった —— 認証前には何も送信しない —— client はその名前を最近の一覧に保持する。最近の一覧は新しいファイル `recent-hosts.txt`（アドレス、最後に接続した時刻、host の名前。最大 10 件）に移った。以前の `recent-devices.txt` は変換されず、削除される。 |
| 2.9 | 2026-09-29 | **passcode を廃止し、アクセスは SSH と同じ仕組みになった。** passcode はもはやどこにも保存も送信もされない。LAN discovery を削除した。Deskhub が利用者の network を scan することはなく、host は平文の discovery 要求に応答しない。host は許可する client public key を、それぞれのラベルとともに `authorized_keys` に保持する。client は信頼する host を `known_hosts` に保持する。固定した host key の fingerprint、アドレス、名前、使用する client key である。Settings で設定する 1 つのデバイス名が、接続先の host に送信され、コピーする public key に埋め込まれる。許可済み client について時刻は保持しない。旧バージョンのデータファイル —— passcode、以前の `paired_devices` の一覧、以前の有効化の印 —— は変換されず、削除される。 |
| 2.8 | 2026-09-28 | Host は許可した public key を `authorized_keys` に保存し、有効化の印をローカルに保持できる。保存済み host profile に別名と選択した client identity を追加した。旧方式の fingerprint のみの一覧は新しい一覧の有効化前に限り使用する。 |
| 2.7 | 2026-09-28 | CLI で追加の名前付き client 署名 key を作成・import し、接続に使用する key を選択できる。名前付き private key は個別のローカルファイルに保存され、identity の一覧には public key の情報のみを表示する。 |
| 2.6 | 2026-09-28 | client の接続には許可済みの署名 key と固定済みの host key が必要になった。Recent devices と UI settings は passcode を保存せず、旧ファイルの読み込みと安全な再書き込み時に古い項目を削除する。書き換えに失敗した場合は旧ファイルを残して後で再試行する。 |
| 2.5 | 2026-09-07 | 本項は訂正であり、挙動の変更ではない。Deskhub の動作は従来と同一である。本ポリシーの旧版では、Android のスマートフォンやタブレットに届いたファイルはシステムのメディアストア経由で `Pictures/Deskhub`、`Movies/Deskhub`、`Download/Deskhub` に保存されると記載していた。これは Android 10 以降について正しい。Deskhub が使用するメディアストアの経路には Android 10 が必要であるため、Android 9 以前では、届いたファイルは端末上の app 自身のフォルダに留まり、ギャラリーにも Downloads にも表示されない。いずれの場合も、ファイルが届くのは関係する 2 台の端末のみである。 |
| 2.4 | 2026-08-28 | **スマートフォンとタブレットが、ファイルの送信に加えて受信にも対応した。** その保存先が新たな内容である。iOS では写真と動画が写真ライブラリに追加され、初回にシステムの追加専用の Photos permission を要求する。Deskhub は追加のみが可能で、既存の内容の読み取りや変更は行わない。それ以外のファイルは app の Documents フォルダに置かれ、Files app から参照できる。Android では、写真は `Pictures/Deskhub`、動画は `Movies/Deskhub`、その他のファイルは `Download/Deskhub` へ、いずれもシステムのメディアストア経由で保存される。両プラットフォームとも、届いた内容を通知で知らせる。これらのデータが当方に届くことはない。本バージョンでは、旧版の記載のうち 2 点を訂正する。Android は端末自身が再生している音声を capture するために、システムが *Microphone* と表示する permission（`RECORD_AUDIO`）を従来から必要としていた。これがバージョン 2.1 に記載した音声の共有にあたる。Deskhub が microphone を録音することは現在もない。また、デスクトップの app はバージョン 2.3 に記載した *File transfer* の選択肢自体を失ったことはなく、失われたのはその背後にある保存設定のみである。 |
| 2.3 | 2026-08-24 | **ファイルの受信は保存される設定ではなくなった。** 保存されていた *Take files viewers send* の設定は `ui-settings.txt` から削除した。スマートフォンとタブレットは app が画面に表示されている間はファイルを受信し、コンピュータは共有する対象の一つとして *File transfer* を提示する。これは毎回既定で選択され、保存されないため、コンピュータは共有中にのみファイルを受信する。届いたファイルの扱いに変更はない。送信側が pair 済みかつ受け入れ済みであることを要し、当該マシンが受信ファイル用に定めた場所に保存され、既存のファイルを上書きせず、送信端末の名称、アドレス、key fingerprint とともにローカルに記録される。画面の共有は引き続き専用のボタンによる明示的な操作であり、画面を共有中のコンピュータはその間もファイルの受信を継続する。 |
| 2.2 | 2026-08-21 | Deskhub が利用者自身の端末間での**ファイル送信**に対応した。画面を共有中のコンピュータはファイルの受信も提示でき、接続しているどの端末からもファイルを選択して送信できる。Android と iOS では、ファイルはシステムの写真ピッカーまたはファイルブラウザから選択し、送信中は app 自身の cache にコピーを用意したうえで、送信後に削除する。ファイルは映像と同じ encrypt された transport で 2 台の端末間を直接流れ、当方に送信されることも、当方のサーバを経由することもない。受信側のコンピュータは自身が選択したフォルダ（別途選択しない場合は当該ユーザーのホームディレクトリ直下の `Deskhub`。他の設定とともに `ui-settings.txt` に保存される）へ書き込み、既存のファイルを上書きせず、各提示、その判断、結果を、送信端末の名称・アドレス・key fingerprint とともにローカルの session log に記録する。ファイルの受信は共有側が有効にしない限り無効であり、スマートフォンとタブレットは送信のみを行い受信はしない。 |
| 2.1 | 2026-08-19 | 画面の共有に加えて、そのコンピュータの**音声**も共有できるようになった。capture の対象はコンピュータ自身のスピーカーが再生しているミックスであり、microphone ではない。Deskhub に双方向の音声はなく、microphone の permission も要求しない。音声は圧縮され、映像と同じ encrypt された transport で閲覧者へ直接送信され、保存されることはない。音声が流れるのは、共有側が *Share this device's sound* を有効にし、**かつ** viewer が *Play the sound of the device you are watching* を有効にしている場合に限られる。いずれかを無効にすれば送信は止まる。両方のトグルは他の設定とともに `ui-settings.txt` に保存され、既定では有効である。 |
| 2.0 | 2026-08-15 | Session が encrypt された transport（QUIC/TLS）上で動作するようになり、video、input、clipboard、terminal のトラフィックが対象となった。マシンの受け入れは pairing により行う。利用者自身の端末に新たに保存されるデータはすべて app のフォルダ内にあり、当方へ送信されることはない。本マシンの identity である鍵ペア（`host_key.pem`、`host_cert.pem`）、trust した host の key（`known_hosts`）、本 host が受け入れたマシン（`paired_devices`。key fingerprint、各マシンが送信した名称、時刻を含む）、および機密ではない salt（`auth_salt`）である。passcode は任意となり、送信されることはない。pairing handshake が送信することなくその正当性を証明する。 |
| 1.9 | 2026-08-14 | Linux において、デスクトップの画面共有ダイアログで行った画面の選択を保存するようにした。デスクトップが発行する permission の token を `portal-restore-token.txt` に保存し、以後の共有ではダイアログを省略する。この token は本マシン上の利用者自身のデスクトップ session でのみ有効であり、送信されず、共有のたびに置き換わり、*Choose screens again* を選択するか、ファイルを削除した場合に消去される。 |
| 1.8 | 2026-08-14 | *keep awake* のトグルを追加した（既定で有効）。共有中および閲覧中、app は OS に対してマシンをスリープさせないこと、display を消灯しないことを要求し、session の終了時にその要求を解除する。保存されるのは有効・無効の選択のみで、同一のローカル設定ファイルに保持される。これに関する情報が送信されることはなく、システムのスリープ設定も変更しない。 |
| 1.7 | 2026-08-13 | Clipboard の同期を Android と iOS でも利用できるようにした。トグル、32 KiB の上限、プレーンテキストのみという規則はデスクトップと同一である。OS による制約がある。Android 端末は Deskhub が前面にある間のみ自身の clipboard を読み取れるが、受信したテキストは常に適用できる。iOS の viewer では、Deskhub が新しいコピー内容を読み取る際にシステムのペースト確認が表示される場合がある。host として動作している iOS 端末は clipboard の同期に参加しない。broadcast が独立した process で動作しているためである。スマートフォンとタブレットにも、デスクトップにある「共有に使用する network アドレスの選択」が加わった。これは同一のローカル設定ファイルに保存され、どこにも送信されない。この選択以外に、端末上へ新たに保存されるものはない。 |
| 1.6 | 2026-08-13 | デスクトップの app に任意の clipboard 同期を追加した。トグルを有効にすると、session 中にコピーしたプレーンテキストが端末間で送信され（当時は他のトラフィックと同様に未 encrypt、上限 32 KiB）、相手のマシンの clipboard に書き込まれる。Deskhub はその内容を保存しない。新たにローカルへ保存される設定は、共有に使用する network アドレス、OS 起動時の開始、起動時の自動共有、バックグラウンドおよび tray モード、そして clipboard のトグルである。OS 起動時の開始を有効にすると、各プラットフォームの起動エントリ（Linux は autostart ファイル、Windows は *Deskhub* という名称の scheduled task、macOS は Login Item）が作成され、無効にすると削除される。 |
| 1.5 | 2026-08-13 | 各 client が connect ページの *Your name* 欄でデバイス名を設定できるようにした。名称は利用者の端末上の既存の設定ファイル `ui-settings.txt` に保存され、connect 時に host へ送信される（当時は他のトラフィックと同様に未 encrypt）。host はこれを session の一覧、status 行、log において当該 viewer の識別に用いる。host は接続中のみ名称をメモリ上に保持し、保存はしない。この欄にはコンピュータまたは端末自身の名称があらかじめ入っているため、自分で選んだ名称に置き換えない限り既定値が送信される。 |
| 1.4 | 2026-08-12 | broadcast extension が app と共有する iOS の状態ファイルに、extension 自身のメモリ使用量（MB）を記録するようにし、共有画面で表示できるようにした。この値は Deskhub の broadcast process のみを示し、利用者の端末上の app group コンテナ内に留まり、broadcast の終了時に状態ファイルの他の内容とともに削除される。 |
| 1.3 | 2026-08-12 | Android と iOS の端末が自身の画面を view-only で共有できるようにし、スマートフォンやタブレットの画面を別の端末へ stream できるようにした。これに伴い、各 OS が要求する画面 capture の permission（Android ではフォアグラウンドサービスとその通知を含む）を追加し、iOS では passcode と port を保持するために app と broadcast extension が共有する app group コンテナ、および broadcast の動作状態を app が表示するために extension が書き込む短期の状態ファイルを追加した。video は引き続き利用者自身の端末間でのみ流れ、保存されることはない。 |
| 1.2 | 2026-08-07 | passcode をすべての host で必須とし、空欄のままではなく初回起動時に生成するようにした。すべての client から入力できる。共有の設定を Windows に加えて macOS と Linux でも保存するようにし、最近使用した端末の一覧はすべてのプラットフォームで、それぞれ app 自身のローカルフォルダに保存するようにした。新たに端末外へ出るデータはない。 |
| 1.1 | 2026-08-05 | Windows の app が起動をまたいでデータを保存するようにした。直近 10 件の接続先アドレス、共有の設定、およびそれらに使用した passcode である。いずれも利用者自身のマシンの `%USERPROFILE%\.deskhub` に留まり、どこにも送信されない。本バージョンでは view-only の共有と viewer 5 名の上限も文書化した。 |
| 1.0 | 2026-07-24 | 初版。 |

## 12. 連絡先

本ポリシーまたは Deskhub のプライバシーに関する問い合わせ:

- **メール:** manhpv151090@gmail.com
- **Issues:** https://github.com/manhpham90vn/Deskhub/issues
