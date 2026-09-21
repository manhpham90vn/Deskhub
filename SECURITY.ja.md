[English](SECURITY.md) · [Tiếng Việt](SECURITY.vi.md) · [中文](SECURITY.zh.md) · **日本語**

# Deskhub セキュリティポリシー

_最終更新: 2026 年 8 月 15 日_

本書は [`SECURITY.md`](SECURITY.md) の翻訳。食い違いがある場合は英語版が正文。

## ⚠️ 最初にお読みください

**Deskhub は session を encrypt する。session が運ぶ内容 —— video、キー入力、mouse、
clipboard、terminal のトラフィック —— はすべて QUIC/TLS 上を通り、受け入れの可否は
pairing handshake が決定する。未知のマシンは、host の passcode を知っていることを証明
する（コード自体は network を通過しない）か、host 側の利用者に承認される必要がある。**
encrypt されないのは discovery beacon のみであり、これは機密情報を含まない。encrypt
された connection の外から到達するそれ以外のデータはすべて破棄される。

いずれの host も **view-only** で共有でき（input は inject されず破棄される）、新規の
pairing を完全に無効化して pair 済みのマシンのみを受け入れることもできる。

encrypt されていることは、Internet に公開してよいことを意味しない。port は discovery の
探索に応答し続け、4 桁の passcode は短い秘密であり、最初の接続は未検証の信頼に基づく。
また flooding に対する防御機構はない。Deskhub は信頼できる network を前提に設計されて
いる。

したがって次の原則は引き続き適用される。

> **UDP 47777 を port-forward しないこと。共有中のマシンを Internet に直接公開しない
> こと。遠隔からのアクセスには VPN を用いること —— 本プロジェクトが検証に用いているのは
> [Tailscale](https://tailscale.com) である —— そして `100.x.y.z` のアドレスに connect
> すること。**

この原則に従う限り Deskhub は安全に使用できる。従わない場合、自機を Internet に公開する
ことになる。

## Threat model

### Deskhub が防ぐもの

| | |
|---|---|
| 開発者へのデータの流出 | 該当するデータは存在しない。サーバ、アカウント、telemetry、サードパーティ SDK のいずれもない。[`PRIVACY.ja.md`](PRIVACY.ja.md) を参照。 |
| トラフィックの盗聴 | すべての session は QUIC/TLS の内部で動作する。video の frame、キー入力、clipboard のテキスト、terminal のバイト列は 2 台の間で encrypt される。パケットキャプチャから得られるのは通信量と時刻であり、内容ではない。port に到達する未 encrypt の packet は、discovery の探索を除きすべて破棄される。 |
| リモートの viewer との操作の競合 | host を優先する。実際の mouse または keyboard を操作した時点で remote input は停止する。これは Windows、macOS、Linux の host に共通である。 |
| キーの押下状態の残留 | リモート側が押している状態のキーは、session の終了時、または viewer が切り替わった時点で自動的に解放される。 |
| 許可のない第三者の接続 | 受け入れは pairing handshake によって制御する。未知のマシンは SPAKE2 により host の passcode を証明する必要がある。コードはネットワークを通過せず、盗聴者はオフラインで解析できるデータを取得できず、connection ごとの試行は 1 回に限られる。passcode が未設定の場合は、host 側の利用者が *Let this machine in?* に回答するまで待機する。3 回失敗すると pairing は 30 秒間ロックされる。受け入れられたマシンは pair 済みとなり、暗号 key によって識別され、host の Devices ページに表示され、そこから取り消せる。discovery beacon は推測されたコードの正否を示さない。未知のマシンの探索には常に空の一覧が返るため、以前の探索によるコード推測の手段は存在しない。 |
| 以降の接続における中間者攻撃 | 各マシンは key を持つ。client は pair した各 host の key を保持し、key が変化した場合は利用者が明示的に受け入れるまで再接続を拒否する。passcode の証明は client が実際に受け取った host key に束縛されるため、relay された証明は検証を通らない。 |
| viewer 同士による mouse の競合 | 1 つの host を最大 5 viewer が閲覧できるが、input を操作できるのは 1 つのみである。先に参加した viewer が優先され、後から参加した viewer の input は、先行する viewer が 1 秒間無操作になるまで破棄される。6 番目の viewer は `Busy` として拒否される。 |
| 閲覧のみを許可したい viewer | view-only の共有はすべての host で利用でき、何らかの操作が inject される前に host 側で input の packet を破棄する。client の自主的な遵守には依存しない。Android と iOS の host は常に view-only である。 |
| 共有したまま放置されたスマートフォン | 最終的な防護は Deskhub ではなく OS が担う。Android は常駐通知を表示し、共有のたびに録画の同意を求める。iOS は broadcast のインジケータを表示し続ける。いずれも app を開かずに共有を停止できる。 |
| pair 済みのマシンによる自機へのファイル書き込み | ファイルを送信できるのは受け入れ済みのマシンのみであり、かつ受信側が file transfer を有効にしている場合に限られる。到達したデータは受信側が選択したフォルダの外に出られない。ネットワーク上のファイル名は、いずれかのファイルが開かれる前に、パスの最後の要素へ切り詰められ、区切り文字、制御バイト、filesystem が受け付けない文字、予約デバイス名が除去される。各ファイルは `.deskhub-part` の接尾辞付きで書き込まれ、全体が到着し CRC-32 が一致した時点でのみ改名される。既存の同名ファイルは上書きされず、番号が付加される。1 batch は 32 ファイル、1 ファイル 8 GiB、合計 32 GiB に制限される。同じ名称処理は、スマートフォンやタブレットでも写真ライブラリや Downloads フォルダに到達する前に実行される。 |
| 不正な packet | すべてのフィールドは読み取り前に境界検査を行う。parser は unit test を備え、CI では AddressSanitizer、UndefinedBehaviorSanitizer、ThreadSanitizer の下で実行され、libFuzzer により毎晩 fuzz される。target は 7 つで、wire format、H.264 の解析、packet の reassembly、terminal の byte stream、UI テキスト、および host 側と viewer 側の session state machine を対象とする。fuzzing で検出された crash は regression test として repo に保存し、新たな coverage は seed corpus に取り込む。 |

### Deskhub が防**がない**もの

以下は網羅的な一覧であり、現時点でいずれも解決されていない。

- **最初の接続は未検証の信頼に基づく。** pairing が防げるのは*それ以降*に現れる中間者
  である。key は固定され、変化は明確に拒否される。しかし最初の接触の時点で既に中間に
  位置している攻撃者は防げない。passcode を設定していない場合、client が到達した相手が
  そのまま pair され、passcode を設定しても、防護の程度は 4 桁の秘密に相当する水準に
  とどまる。この点が重要な場合は、fingerprint を別の経路で照合すること。
- **トラフィック解析は依然として可能である。** encrypt が隠すのは内容であって存在では
  ない。観察者は session が動作していること、video の通信量、入力の時刻を把握できる。
- **rate limiting はなく、DoS 耐性もない。** port に大量のデータを送り込めば session は
  中断する。passcode を設定していない host では、承認プロンプトを繰り返し表示させる
  こともできる。
- **discovery beacon はあらゆる送信元に応答する。** 任意の送信元アドレスからの
  `LIST_SOURCES` 探索や `PING` に応答が返る。未知のマシンへの応答は空の一覧であり、
  いかなる探索でも passcode を確認することはできないが、マシンはスキャンによって発見
  され、port は小規模な UDP リフレクタとして利用されうる。例外が 1 つある。現に
  encrypt された connection を保持している送信元アドレスには、未 encrypt の応答を返さ
  ない。マシンが身元を証明した後は、そのマシンからのデータはすべて encrypt された形で
  到達しなければならないため、偽造された平文の `SOURCE_LIST` や `PONG` で接続中の peer
  になりすますことはできない。
- **デバイス名は表示され、ログにも記録される。** viewer が送る *Your name* は転送中は
  encrypt されるが、host の画面に表示され、host の log に記録され、host の
  paired-devices の一覧に保存される。既定値はマシンの hostname であり、多くの場合
  利用者の実名である。ニックネームを使用し、この項目に機微な情報を入力しないこと。
  項目を空にしても名称の送信は止まらず、既定値が復元されるだけである。
- **viewer の枠は 5 秒間データがないと解放される。** 自分の viewer が切断された場合、
  その枠は再び開放され、次に到達した `Hello` が使用する。送信元を問わず、制約は
  passcode のみである。
- **共有は display 全体を公開する。** 単一のウィンドウではなく、そのモニタ上のすべての
  通知、ポップアップ、ウィンドウが対象となる。[`PRIVACY.ja.md` §3.4](PRIVACY.ja.md) を
  参照。
- **host となるスマートフォンやタブレットは端末全体を公開する。** Android と iOS も
  host になれるが、送出されるのは画面全体である。銀行の app、ワンタイムコード、
  メッセージ、共有中に入力するすべてのパスワードが含まれる。stream は他の session と
  同様に encrypt されるが、受け入れたすべての viewer がその全体を閲覧できる。モバイルの
  host は常に view-only であり、これは遠隔操作の危険を除くが、情報が公開される危険は
  低減しない。

## 安全に運用できる環境

✅ **安全**

- すべての端末を自分で管理している家庭内または個人の LAN。
- 自分の端末のみが参加している Tailscale の tailnet（または他の WireGuard/VPN
  トンネル）。VPN は encrypt の層を追加し、第三者が port に到達すること自体を防ぐ。
- *client* としてのみ用いるマシン（スマートフォン、タブレット、画面を共有しないノート
  PC）。client は着信する session を受け付けない。

❌ **安全ではない**

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

1. UDP 47777 をスキャンして当該マシンを発見する。pair していないマシンの探索には空の
   一覧が返るが、マシン自体は応答するため、存在は把握される。
2. 接続を試みる。passcode をネットワーク上から読み取ることはできない。コードが通過しな
   いためである。残る手段は、オンラインでの試行（connection ごとに 1 回、3 回失敗すると
   pairing が 30 秒ロックされる）か、passcode を設定していない host において、host 側の
   利用者が承認プロンプトで **Allow** を押すのを待つことである。
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
- [ ] マシンの受け入れ方法を決める。Settings で 4 桁の passcode を設定するか、空のまま
      にして承認プロンプトに自分で回答する。Devices ページを定期的に確認し、認識できない
      マシンは削除する。閲覧のみで足りる場合は *Viewers can control this machine* の
      チェックを外す。
- [ ] 使用していないときは Deskhub を終了する。background service ではないため、終了
      すれば受け入れ口も閉じる。
- [ ] Linux で `ufw` を使用している場合は、全面的に開放せず範囲を限定する。
      `sudo ufw allow 47777/udp` ではなく
      `sudo ufw allow from 100.64.0.0/10 to any port 47777 proto udp` とする。
- [ ] 他の network へ持ち出すノート PC で共有を動作させたままにしない。
- [ ] 離席時にはマシンをロックし、無人の session が引き継がれないようにする。
- [ ] `deskhub-cli` では passcode をコマンドに直接記述しない。`--passcode 0417` は
      `ps` や `/proc/*/cmdline` を通じてマシン上のすべてのプロセスから参照でき、shell
      の履歴にも残る。標準入力から読む `--passcode -`、自分のみが読めるファイルから読む
      `--passcode @FILE`、または環境変数 `DESKHUB_PASSCODE` を使用すること。

## ローカルに保存されるデータ

診断の log は、Windows、macOS、Linux において `~/.deskhub/`（Windows は
`%USERPROFILE%\.deskhub`）の下に平文で書き込まれる。内容は接続の統計と peer のアドレス
であり、画面の内容やキー入力は含まない。

デスクトップの app と `deskhub-cli` はこれらのファイルを共有する。同じフォルダには次も
保存される。`ui-settings.txt`（fps、bitrate、解像度の上限、port、view-only と pairing
のスイッチ、設定済みの host passcode、host に表示されるデバイス名）、
`recent-devices.txt`（直近 10 件の接続先アドレス、その時刻、それぞれに使用した
passcode）、`host_key.pem` と `host_cert.pem`（本マシンの秘密鍵と自己署名 certificate。
すなわち fingerprint の背後にある identity であり、この key ファイルを入手した者は本
マシンになりすませる）、`known_hosts`（本マシンが trust した host の key）、
`paired_devices`（この host が受け入れたマシンの key、名前、時刻）、`auth_salt`
（passcode verifier 用の秘密ではない salt）、および Linux では
`portal-restore-token.txt`（選択した画面に対してデスクトップが発行した token。自身の
デスクトップ session にのみ意味を持ち、送信されることはない）。モバイルの app は設定を
自身のサンドボックス内に保持し、iOS では app group のコンテナに置く。保存された
passcode は固定の XOR key で難読化され、そのままでは読み取れない状態になっている。
**これは encrypt ではない。** ソースとファイルを持つ者は数秒で復元できる。このフォルダ
は、自分の権限で動作するあらゆるプログラムから読み取り可能なものとして扱うこと。

他のマシンが送信したファイルはこのフォルダの外、受信側が選択したディレクトリに保存
される（別途選択していない場合は利用者のホームディレクトリ直下の `Deskhub`。
`transfer_dir` として保存される）。スマートフォンやタブレットでは端末の写真ライブラリ
または Documents / Downloads フォルダに置かれ、app をアンインストールしても残る。そこ
に届いたものはすべて、pair 済みのマシンが自分の端末に置いたファイルとして扱うこと。

これらがアップロードされることはない。フォルダはいつでも削除できる。

## 予定している緩和策

実施を予定している順に記載する。

1. **passcode と host key を、ファイルではなく OS の keychain に保存する。**
2. **discovery beacon が、要求していない探索に対して空の一覧を返すのではなく、応答
   しないようにする。**

本一覧の前回改訂以降に実施済みの事項: session 全体（video、input、clipboard、terminal
のいずれも）に対する encrypt された transport（QUIC/TLS）の導入と、discovery の探索を
除く未 encrypt データの破棄。passcode がネットワークを通過せず、収集もオフラインの
brute-force もできない SPAKE2 による pairing。host 側の承認プロンプト。取り消し可能な
pair 済みマシンの一覧。マシンごとの key と、client 側での key 変更の警告。passcode の
誤入力に対する 3 回 / 30 秒の lockout。

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

**上記に記載済みの内容は脆弱性とは扱わない。** 上に挙げた制約 —— 最初の接続における
未検証の信頼、トラフィック解析、探索に応答する beacon、DoS 耐性の欠如 —— はいずれも
既知であり記載済みである。これらを再度指摘する報告は新しい情報をもたらさない。*報告が
有用な*内容は、不正な packet から到達しうる memory corruption や crash、文書化された
threat model を回避する手法、データをマシン外へ流出させる経路、および緩和策が出荷された
後にその中に存在する欠陥である。

## サポート対象のバージョン

サポート対象は [Releases ページ](https://github.com/manhpham90vn/Deskhub/releases) の
最新 release のみである。修正は新しい release で提供し、旧バージョンへの backport は
行わない。

## 適用範囲

本ポリシーは、この repo の Deskhub の source、および Releases ページ、TestFlight、
Google Play で公開している binary を対象とする。Tailscale、利用者の OS、ルータ、その他
併用しているソフトウェアは対象としない。
