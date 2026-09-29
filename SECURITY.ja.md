[English](SECURITY.md) · [Tiếng Việt](SECURITY.vi.md) · [中文](SECURITY.zh.md) · **日本語**

# Deskhub セキュリティポリシー

_最終更新: 2026 年 9 月 29 日_

本書は [`SECURITY.md`](SECURITY.md) の翻訳。食い違いがある場合は英語版が正文。

## ⚠️ 最初にお読みください

**Deskhub は信頼できる network または VPN 上で使うこと。UDP 47777 を
port-forward したり、共有中のマシンを Internet に直接公開したりしないこと。**

Session の video、キー入力、mouse、clipboard、terminal の通信には QUIC/TLS を使う。
アクセスは SSH と同じ仕組みで行われる。client が受け入れられるのは、その public key が
host の `authorized_keys` に記載され、対応する private key を保持していることを証明した
場合に限られる。client は何かを送る前に、host の key を初回接続時に固定した key と照合
し、変わっていれば即座に拒否する。network 越しに何かを承認することはなく —— passcode
も承認プロンプトも存在しない —— host は平文の packet に一切応答しない。encrypt 済みの
connection の外から届くものはすべて破棄される。

いずれの host も **view-only** で共有できる（input は inject されず破棄される）。

Encrypt だけでは network 上のリスクはなくならない。host への初回の Connect では、
fingerprint を照合しない限り、提示された key がそのまま信頼される。また app に
flooding への防御機構もない。

遠隔からアクセスするときは VPN を使う。本プロジェクトは
[Tailscale](https://tailscale.com) で検証しており、host の `100.x.y.z` アドレスへ
Connect できる。画面や terminal を共有する前に、以下の制限も確認してほしい。

## Threat model

### Deskhub が防ぐもの

| | |
|---|---|
| 開発者へのデータの流出 | 該当するデータは存在しない。サーバ、アカウント、telemetry、サードパーティ SDK のいずれもない。[`PRIVACY.ja.md`](PRIVACY.ja.md) を参照。 |
| トラフィックの盗聴 | すべての session は QUIC/TLS の内部で動作する。video の frame、キー入力、clipboard のテキスト、terminal のバイト列は 2 台の間で encrypt される。パケットキャプチャから得られるのは通信量と時刻であり、内容ではない。port に到達する未 encrypt の packet は破棄され、client が authenticate するまで host は application レベルで何も —— 共有内容さえも —— 送信しない。 |
| リモートの viewer との操作の競合 | host を優先する。実際の mouse または keyboard を操作した時点で remote input は停止する。これは Windows、macOS、Linux の host に共通である。 |
| キーの押下状態の残留 | リモート側が押している状態のキーは、session の終了時、または viewer が切り替わった時点で自動的に解放される。 |
| 許可のない第三者の接続 | 受け入れられるのは、public key が host の `authorized_keys` に含まれる client のみであり、その client は対応する private key で当該 connection 自体の transcript に署名しなければならない。推測すべき秘密はなく、network 越しに何かが承認されることもない。`authorized_keys` ファイルが存在しなければ誰も受け入れられない。host が保持する authenticate 待ちの接続は最大 8 で、それぞれ 10 秒後に切断される。1 つの key とアドレスから 1 分以内に 3 回不正な署名があると、その組み合わせは 10 秒間ブロックされる。host の Devices ページで key を削除すると、そのデバイスの実行中の session も直ちに閉じられる。受け入れは、それを得た connection が続く間だけ有効である。 |
| 中間者攻撃 | 各 host は key を持つ。client は初回接続時に、利用者が fingerprint を照合した後でそれを固定し、以降の接続では何かを送る前に毎回照合する。変化した key は即座に拒否され、プロンプトから受け入れる手段はない。その host を *Trusted hosts* から削除し、改めて信頼する必要がある。client の署名は、TLS session から export した session 識別子と client が受け取った host key の fingerprint を対象とするため、別の host に relay された署名や、別の connection で replay された署名は検証を通らない。 |
| viewer 同士による mouse の競合 | 1 つの host を最大 5 viewer が閲覧できるが、input を操作できるのは 1 つのみである。先に参加した viewer が優先され、後から参加した viewer の input は、先行する viewer が 1 秒間無操作になるまで破棄される。6 番目の viewer は `Busy` として拒否される。 |
| 閲覧のみを許可したい viewer | view-only の共有はすべての host で利用でき、何らかの操作が inject される前に host 側で input の packet を破棄する。client の自主的な遵守には依存しない。Android と iOS の host は常に view-only である。 |
| 共有したまま放置されたスマートフォン | 最終的な防護は Deskhub ではなく OS が担う。Android は常駐通知を表示し、共有のたびに録画の同意を求める。iOS は broadcast のインジケータを表示し続ける。いずれも app を開かずに共有を停止できる。 |
| 許可済み client による自機へのファイル書き込み | ファイルを送信できるのは受け入れ済みのマシンのみであり、かつ受信側が file transfer を有効にしている場合に限られる。到達したデータは受信側が選択したフォルダの外に出られない。ネットワーク上のファイル名は、いずれかのファイルが開かれる前に、パスの最後の要素へ切り詰められ、区切り文字、制御バイト、filesystem が受け付けない文字、予約デバイス名が除去される。各ファイルは `.deskhub-part` の接尾辞付きで書き込まれ、全体が到着し CRC-32 が一致した時点でのみ改名される。既存の同名ファイルは上書きされず、番号が付加される。1 batch は 32 ファイル、1 ファイル 8 GiB、合計 32 GiB に制限される。同じ名称処理は、スマートフォンやタブレットでも写真ライブラリや Downloads フォルダに到達する前に実行される。 |
| 不正な packet | すべてのフィールドは読み取り前に境界検査を行う。parser は unit test を備え、CI では AddressSanitizer、UndefinedBehaviorSanitizer、ThreadSanitizer の下で実行され、libFuzzer により毎晩 fuzz される。target は 7 つで、wire format、H.264 の解析、packet の reassembly、terminal の byte stream、UI テキスト、および host 側と viewer 側の session state machine を対象とする。fuzzing で検出された crash は regression test として repo に保存し、新たな coverage は seed corpus に取り込む。 |

### Deskhub が防**がない**もの

以下は網羅的な一覧であり、現時点でいずれも解決されていない。

- **保持された shell は、それを開いたマシンではなく、すべての許可済み client に属する。**
  host に残された shell は、それを開いた接続よりも長く、時間制限なく生き続ける。許可さ
  れたマシンはいずれも、host が保持している shell を一覧し、detach されたものを
  reattach し、どれでも閉じることができる。各 shell の id、サイズ、デバイス名はその一覧
  に含まれる。したがって、許可した 2 台目の client —— あるいは Devices ページで key を
  削除していない client —— は、以前の shell が何をしていたかを読み戻し、その中で作業を
  続けられる。信頼しなくなった key は削除し、使い終えた shell は残さず閉じること。
- **最初の接続は未検証の信頼に基づく。** host key の固定が防げるのは*それ以降*に現れる
  中間者である。変化は即座に拒否される。しかし最初の接触の時点で既に中間に位置している
  攻撃者は、ダイアログの求めに従い、*New host* ダイアログが示す fingerprint を host の
  Devices ページ上のものと照合しない限り防げない。`deskhub-cli` は
  `--accept-new-host-key` を指定しない限り未知の host を拒否する。また
  `host add … --host-key-stdin` で key を事前に固定することもできる。
- **トラフィック解析は依然として可能である。** encrypt が隠すのは内容であって存在では
  ない。観察者は session が動作していること、video の通信量、入力の時刻を把握できる。
- **rate limiting はなく、DoS 耐性もない。** port に大量のデータを送り込めば session は
  中断する。authenticate 待ちの接続数と不正な署名に対する制限は推測を防ぐものであり、
  flooding を防ぐものではない。
- **QUIC handshake には依然として応答する。** host は平文の packet には一切応答しなく
  なったが、port への QUIC/TLS handshake は client が何かを証明する前に完了する。その
  ため、アドレスを知っている第三者は、何かが待ち受けていることを把握でき、host の
  certificate を見ることもできる。
- **デバイス名は表示され、ログにも記録される。** client が送るデバイス名は転送中は
  encrypt されるが、host の画面に表示され、host の log に記録される。さらに本マシンが
  コピーするすべての public key のラベルとなるため、その key を許可した各 host の
  `authorized_keys` にも残る。host も、許可済みの key で authenticate を終えたすべての
  client に自身のデバイス名を送信し —— それより前に送ることはない —— その client は
  最近の一覧に名前を保持する。既定値はマシンの hostname であり、多くの場合利用者の実名
  である。Settings → General → *Device name* でニックネームを設定し、この項目に機微な
  情報を入力しないこと。空にしても名称の送信は止まらず、既定値が復元されるだけである。
- **viewer の枠は 5 秒間データがないと解放される。** 自分の viewer が切断された場合、
  その枠は再び開放され、次に到達した `Hello` が使用する。送信元は許可済みの key で
  admission を通過したマシンであればよい。
- **共有は display 全体を公開する。** 単一のウィンドウではなく、そのモニタ上のすべての
  通知、ポップアップ、ウィンドウが対象となる。[`PRIVACY.ja.md` §3.4](PRIVACY.ja.md) を
  参照。
- **host となるスマートフォンやタブレットは端末全体を公開する。** Android と iOS も
  host になれるが、送出されるのは画面全体である。銀行の app、ワンタイムコード、
  メッセージ、共有中に入力するすべてのパスワードが含まれる。stream は他の session と
  同様に encrypt されるが、受け入れたすべての viewer がその全体を閲覧できる。モバイルの
  host は常に view-only であり、これは遠隔操作の危険を除くが、情報が公開される危険は
  低減しない。

## Deskhub を使う環境

✅ **推奨する環境**

- すべての端末を自分で管理している家庭内または個人の LAN。
- 自分の端末のみが参加している Tailscale の tailnet（または他の WireGuard/VPN
  トンネル）。VPN は encrypt の層を追加し、第三者が port に到達すること自体を防ぐ。
- *client* としてのみ用いるマシン（スマートフォン、タブレット、画面を共有しないノート
  PC）。client は着信する session を受け付けない。

❌ **避けるべき環境**

- ルータで UDP 47777 を port-forward すること、または共有中のマシンを DMZ に置くこと。
- カフェ、ホテル、空港、学内、コワーキングスペース、カンファレンスの Wi-Fi で画面を
  共有すること。
- 他の端末を管理していないオフィスや共同住宅の LAN で共有すること。
- ゲスト端末、自身で設定していない IoT 機器、管理下にない他者のマシンが存在する
  network。
- クラウド VM の公開インターフェースや公開トンネルサービスを通じて port を公開すること。

既定では socket はすべてのインターフェース（`INADDR_ANY`）にバインドするため、そのマシン
が接続しているあらゆる network から到達できる。意識していない network も含まれる。
**Share on network** の設定はこの範囲を限定する。マシンのアドレスを 1 つ選択すると host
はそのインターフェースのみにバインドし、他の network のマシンは port に到達できない。
注意点が 2 つある。第一に、共有を開始する時点で選択したアドレスが存在しない場合
（ケーブルの抜去、DHCP による新しいアドレスの割り当て）、Deskhub はすべての
インターフェースにフォールバックし、その旨を共有 status に表示する。この設定に依存する
場合はその表示を確認すること。第二に、単一のインターフェースにバインドすると、同一
マシン上の loopback（`127.0.0.1`）経由の viewer も遮断される。Windows では、app は起動
時点から昇格して動作し（昇格したウィンドウへ input を inject するために一度だけ権限を
要求する）、共有時に firewall のルールを自動的に開く。このルールは app 全体とすべての
profile を対象とするため、バインドを限定しても firewall は限定されない。この利便性も、
上記の原則が重要である理由の一つである。

## 同一 network 上の攻撃者にできること

画面を共有中で Deskhub が動作しているマシンと同じ LAN に第三者がいる場合、その相手は
次のことが可能である。

1. 各アドレスの UDP 47777 に QUIC handshake を試みて当該マシンを発見する。平文の
   packet には応答が返らないが、handshake 自体には応答するため、狙いを定めたスキャンに
   対しては存在が把握される。
2. 接続を試みる。そのためには、host の所有者が public 側を `authorized_keys` に追加した
   private key が必要である。推測すべき passcode も、誰かにクリックさせるプロンプトも
   存在しない。そのような key がなければ、せいぜい client が host に*初めて*接続する際に
   中間に入り込むことを試みる程度であり、これは fingerprint の照合によって検出される。
3. 接続せずにトラフィックを観察する。ただし得られるのは通信量と時刻のみである。video を
   含む session の内容は encrypt されており、キャプチャから画面やキー入力を再構成する
   ことはできない。
4. port に大量のデータを送り込み session を中断させる。マシンに到達できる攻撃者に対する
   rate limiting の機構は存在しない。

host を優先する挙動は、マシンの*前にいる間*の不正な操作を抑える。離席している間は機能
せず、防護が必要なのはまさにその時間帯である。

## 堅牢化のためのチェックリスト

Deskhub を現状のまま使い続ける場合、次の項目を実施することを推奨する。

- [ ] 両方のマシンで Tailscale を動作させ、`100.x.y.z` のアドレスのみで connect する。
- [ ] ルータに UDP 47777 の port-forward および UPnP マッピングが**存在しない**ことを
      確認する。
- [ ] host では必要な client key のみを許可し、各 client からの初回接続時に host key の
      fingerprint を照合する。Devices ページを定期的に確認して不要な key を削除する。
      閲覧のみで足りる場合は *Viewers can control this machine* のチェックを外す。
- [ ] 使用していないときは Deskhub を終了する。background service ではないため、終了
      すれば受け入れ口も閉じる。
- [ ] Linux で `ufw` を使用している場合は、全面的に開放せず範囲を限定する。
      `sudo ufw allow 47777/udp` ではなく
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` とする。
- [ ] 他の network へ持ち出すノート PC で共有を動作させたままにしない。
- [ ] 離席時にはマシンをロックし、無人の session が引き継がれないようにする。
- [ ] `deskhub-cli` の `key public --name NAME` で client public key、`host-key public` で
      本 host の key を表示する。それぞれを信頼できる経路で渡し、host では
      `access add --stdin`、client では `host add … --host-key-stdin` を使う。

## ローカルに保存されるデータ

診断の log は、Windows、macOS、Linux において `~/.deskhub/`（Windows は
`%USERPROFILE%\.deskhub`）の下に平文で書き込まれる。内容は接続の統計と peer のアドレス
であり、画面の内容やキー入力は含まない。

デスクトップの app と `deskhub-cli` はこれらのファイルを共有する。`DESKHUB_CONFIG_DIR`
または CLI の `--config-dir` で、両者を別のフォルダに向けることができる。同じフォルダには
次も保存される。`ui-settings.txt`（fps、bitrate、解像度の上限、port、view-only の
スイッチ、デバイス名、bind アドレス、その他のトグル）、`recent-hosts.txt`（直近 10 件
の接続先 host —— アドレス、その時刻、各 host が名乗った名前）、`client_key.pem` と `client_key.<name>.pem`（本マシンの
client 秘密鍵。入手した者は、その key が許可されているあらゆる場所に sign in できる。
Windows では DPAPI で保護される）、`host_key.pem` と `host_cert.pem`（本マシンの host
秘密鍵と自己署名 certificate。すなわち fingerprint の背後にある identity であり、この
key ファイルを入手した者は host として本マシンになりすませる）、`authorized_keys`（この
host に受け入れられる client public key とそれぞれのラベル）、`known_hosts`（本マシンが
信頼する host。アドレス、固定した fingerprint、名前、使用する client key）、および
Linux では `portal-restore-token.txt`（選択した画面に対してデスクトップが発行した
token。自身のデスクトップ session にのみ意味を持ち、送信されることはない）。
passcode はどこにも保存されない。POSIX システムではフォルダは `0700`、各ファイルは
`0600` で作成され、atomic に書き込まれる。Windows では利用者本人、SYSTEM、
Administrators のみに制限される。モバイルの app は同じファイルを自身のサンドボックス内
に保持し、iOS では app group のコンテナ内の `.deskhub` フォルダ、Android では app の内部ストレージに置く。この
フォルダは、自分の権限で動作するあらゆるプログラムから読み取り可能なものとして扱うこと。

読み取れない `authorized_keys` や `known_hosts` の内容を推測で補うことはない。読み取れ
ない間、host は誰も受け入れず、client はすべての host を拒否し、次の変更時にファイルが
新たに書き込まれる。旧バージョンのデータ —— passcode、以前の pair 済みマシン一覧 —— は
変換されず、残ったファイルは削除される。

他のマシンが送信したファイルはこのフォルダの外、受信側が選択したディレクトリに保存
される（別途選択していない場合は利用者のホームディレクトリ直下の `Deskhub`。
`transfer_dir` として保存される）。スマートフォンやタブレットでは端末の写真ライブラリ
または Documents / Downloads フォルダに置かれ、app をアンインストールしても残る。そこ
に届いたものはすべて、許可済み client が自分の端末に置いたファイルとして扱うこと。

これらがアップロードされることはない。フォルダはいつでも削除できる。

## 予定している緩和策

実施を予定している順に記載する。

1. **host key と client key を、まだ保護されていないプラットフォームにおいて、ファイル
   ではなく OS の keychain に保存する。**

本一覧の前回改訂以降に実施済みの事項: SSH 方式のアクセス —— client は host の
`authorized_keys` に記載された public key によってのみ受け入れられ、connection ごとに
新たに署名する。初回接続時の host key の固定と、変化した場合の拒否。passcode、承認
プロンプト、pairing のスイッチの削除。LAN discovery の削除により、host は平文の packet
に一切応答しなくなった。authenticate 待ちの接続数と不正な署名に対する制限。

本一覧は方針の表明であり、スケジュールではない。Deskhub は 1 名が余暇に保守している。
計画ではなく現状に基づいて判断していただきたい。

## 脆弱性の報告

セキュリティに関する問題は**非公開で**報告していただきたい。公開の GitHub issue として
は投稿しないこと。

- **メール:** manhpv151090@gmail.com —— 件名に `[Deskhub security]` を記載すること。
- **または:** GitHub で[非公開の security
  advisory](https://github.com/manhpham90vn/Deskhub/security/advisories/new) を作成する。

実行環境（OS、タイトルバーまたは [`VERSION`](VERSION) の Deskhub のバージョン）、実施
した操作、観測された結果を記載していただきたい。proof of concept があると有用である。

**対応の目安:** 7 日以内に受領の連絡、30 日以内に評価を行う。開発者 1 名が余暇で運営
しているプロジェクトのため、期間については了承いただきたい。いずれの場合も明確な回答を
返す。修正が出荷された場合、希望されない限り release notes に謝辞を記載する。

bug bounty はなく、報酬の支払いも行わない。

初回接続時の信頼、トラフィック解析、見える QUIC handshake、DoS 耐性の欠如は、上記に
記載済みの制限である。その影響について新しい証拠があれば報告してほしい。不正な
packet による memory corruption や crash、データが予期せず端末外へ出る問題、
公開済みの緩和策の欠陥なども報告対象となる。

## サポート対象のバージョン

サポート対象は [Releases ページ](https://github.com/manhpham90vn/Deskhub/releases) の
最新 release のみである。修正は新しい release で提供し、旧バージョンへの backport は
行わない。

## 適用範囲

本ポリシーは、この repo の Deskhub の source、および Releases ページ、TestFlight、
Google Play で公開している binary を対象とする。Tailscale、利用者の OS、ルータ、その他
併用しているソフトウェアは対象としない。
