[English](SPECIFICATION.md) · [Tiếng Việt](SPECIFICATION.vi.md) · [中文](SPECIFICATION.zh.md) · **日本語**

# Deskhub — 機能仕様

この仕様書では、Deskhub で何ができ、操作に app がどう応えるかを説明する。
インストール、build、セキュリティについては [`INSTALL.ja.md`](INSTALL.ja.md)、
[`BUILD.ja.md`](BUILD.ja.md)、[`SECURITY.ja.md`](../SECURITY.ja.md) を参照。
実装の詳細は [`ARCHITECTURE.ja.md`](ARCHITECTURE.ja.md) と source ツリーにある。

本書は [`SPECIFICATION.md`](SPECIFICATION.md) の翻訳。食い違いがある場合は英語版が正文。

- **状態:** 現在のコードの挙動を記述している。
- **読者:** tester、reviewer、コントリビュータ、ストア掲載文の作成者。

---

## 1. 製品概要

Deskhub では、双方から到達できる network 上の別の端末へ画面を Share できる。host が
許可すれば、viewer はそのマシンの mouse と keyboard も操作できる。同じ app で画面の
共有と別のマシンへの Connect の両方を行う。デスクトップの host は **terminal** も
提供でき、接続した端末は host 上の shell を専用のウィンドウで開く（4 節と 5 節）。

installer は用意されているが必須ではない。アカウント、サインイン、background
service、クラウド構成要素も不要。client は、双方から到達できる network 上で IP
アドレスを使って host に Connect する。

## 2. 用語

| 用語 | 意味 |
| --- | --- |
| **Host** | 画面（または terminal）を共有される側のマシン。 |
| **Client** / **Viewer** | host を閲覧し、場合によっては操作するマシン。 |
| **Source** | host 上で共有できる対象。display または terminal を指す。host は複数を同時に共有できる。 |
| **Session** | 1 つの viewer が 1 つの source を閲覧する単位。source ごとに独立したウィンドウで開く。 |
| **Machine key** | マシンが初回起動時に生成し、自動的に置き換えることのない唯一の key。両方の役割におけるマシンの identity であり、host は他のマシンから connect される際にこれで自身を証明し、client は host に connect する際にこれで sign in する。利用者には 1 つの fingerprint（`SHA256:…`）として表示され、その public 側を host の所有者が許可済み client に追加する。 |
| **Allowed clients** | connect を許可された public key の、host 側の一覧（`authorized_keys`）。各 key はラベルを伴う。一覧にある key のみが受け入れられる（9 節）。 |
| **Trusted host** | 本マシンが key によって信頼すると決めた host。初回接続時に固定した、または QR code から取得した fingerprint、名前、および最後に応答したアドレスから成る。信頼は key に従うため、信頼済み host はアドレスが変わっても信頼を保つ。 |
| **Connection request** | まだ許可していないデバイスが connect を試みた際に host が記録するもの。デバイスの名前、key の fingerprint、アドレス、時刻から成る。host の所有者はこれを **Approve** または **Deny** できる（H-18）。 |
| **QR code** / **invite** | 共有中の host が表示できる code —— `deskhub://pair/…` リンクでもある —— で、host のアドレス、fingerprint、名前、1 回限りの token を含む。これをスキャンまたは貼り付けたデバイスは host を信頼し、承認の手順なしに直ちに受け入れられる（C-12）。 |
| **Device name** | 本マシンが用いる唯一の名前。host するときは viewer に表示されるとともに connect してきた許可済み client に送信され、connect するときは host に送信され、コピーした public key のラベルにも用いられる（T-26）。 |

1 台のマシンが host と client を兼ねることもできる。

## 3. プラットフォームごとの役割

| プラットフォーム | host 可否 | 閲覧可否 | 音声 |
| --- | :--: | :--: | :--: |
| Windows | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Linux | ✅ | ✅ | ✅ |
| Android | ✅ view-only | ✅ | ⚠️ Android 10+ |
| iOS | ✅ view-only | ✅ | ⚠️ app の音声のみ |

12 節に別段の記載がない限り、client の機能はどのプラットフォームでも同一である。
スマートフォンとタブレットは **view-only** で host する。画面は送出するが remote input
は受け取らない。通常の app に端末を操作させるモバイル OS が存在しないためである。

app はどのプラットフォームでも同じ区分で構成される。**Host**、**Client**、
**Settings**、および誰がどこへ connect できるかを決める key を保持する **Devices**
ページである（9 節）。

デスクトップ 3 種には、共有、Connect、remote shell の起動、許可済み client・接続要求・
信頼済み host の管理に使える command line client もある。settings、machine key、許可済み
client、信頼済み host は app と共通。CLI でリモート画面を見られるのは Windows と Linux で、macOS では app を
使う。

---

## 4. Host —— 本マシンの画面を共有する

| ID | 機能 | 説明 |
| --- | --- | --- |
| H-1 | display の選択 | 共有前に、本マシンのどの display を提供するかを利用者が選択する。最低 1 つの選択が必要である。 |
| H-2 | 複数 display の共有 | 複数の display を同時に共有できる。各 display は独立した source となり、viewer が選択する。 |
| H-3 | source の上限 | 同時に共有できる display は最大 **8** 枚である。マシンがそれ以上の display を持つ場合、最初の 8 枚のみが共有される旨を利用者に通知する。 |
| H-4 | 共有の開始と停止 | 1 つの操作で開始し、1 つの操作で停止する。共有を開始した、または開始しつつあるときは、バナーが状態（*Starting share…* / *Sharing*）とその詳細を表示する。それ以前はボタン自体のラベル（*Start sharing*）だけが状態を示し、バナーは表示されない。 |
| H-5 | display 単位の停止 | 共有中の display を 1 枚のみ停止でき、共有全体は終了しない。 |
| H-6 | 接続情報 | 共有中、app は本マシンの network アドレスと viewer が使用する port を一覧表示する。読み上げや複製に用いる。その一覧の隣に **Show QR code** がある。QR code —— および複製ボタン付きの、それが encode する `deskhub://pair/…` リンク —— を描き、スキャンまたは貼り付けた瞬間に 1 台のデバイスを受け入れる（C-12）。code は表示するたびに新しく作られ、表示中はボタンが *Hide QR code* と表示され、隠すか共有を停止すると code は無効化されて使えなくなる。デスクトップでは *Share on network*（T-9）が host 画面のこの一覧の隣に配置され、一覧には選択した network のアドレスのみが表示される。*All networks* を選ぶとすべて表示される。共有中および開始処理中はこの選択が固定され、変更するには共有を停止する必要がある。 |
| H-7 | ライブ session 表 | 共有中の display ごとに、host は display 名、解像度、viewer 数、capture rate、send rate、使用帯域、round-trip time を確認できる。接続中の viewer は各 display の下に 1 行ずつ表示され、デバイス名とアドレスによる「名前 (ip:port)」で（C-7）、名前が送信されなかった場合はアドレスのみで識別される。 |
| H-8 | viewer の切断 | host は session 表から任意の viewer を個別に切断できる。 |
| H-9 | viewer の上限 | 1 つの host を同時に閲覧できるのは最大 **5** viewer である。それを超える要求は busy として拒否される。 |
| H-10 | 失敗時の報告 | 共有を開始できない場合、無言で失敗せず理由を表示する。port が使用中で terminal を起動できない場合も、その旨を明示する。 |
| H-11 | Terminal source（デスクトップ） | source 一覧には **Terminal — a shell on this machine** も含まれる。この項目は一覧が表示されるたびに選択し直され、保存されない。画面のみ、terminal のみ、両方のいずれの構成も有効である。いずれも app の同一の UDP port（T-4）、許可済み client（S-2）、network 選択を共有する。 |
| H-12 | 表内の shell session | terminal の共有中、ライブ表には port を伴う *Terminal* の行が表示され、開いている shell はその下に 1 行ずつ、viewer と同じ方式（C-7）で識別されて並び、*Disconnect* が付く。*Terminal* 行の *Stop* は terminal の共有のみを終了する。同時に開ける shell は最大 **8** である。shell の open、close、reattach は、client のアドレス・名前・key とともに session log（G-3）に記録される。 |
| H-13 | shell は host に残り続ける | 接続を失った shell —— network の切断、または client ウィンドウの終了 —— は、内容と scrollback を保ったまま host 上に保持され、時間制限はない。元の client は自動的に reattach する。2 分間、間隔を延ばしながら再試行し、その間は reattach 中である旨を表示し、同じ shell を復帰させる。許可された client はいずれも、host が保持している shell を問い合わせて id で reattach し、新規に開く代わりとすることができる。shell が終了するのは、その shell プロセスが終了したとき、host が terminal の共有を停止したとき、または session 表から閉じられたときのみであり、client のウィンドウを閉じても shell が終了することはない。 許可された client は、その一覧から保持中の shell を id で終了させることもできる。その shell に入力していたマシンには、shell が終了した旨が伝えられる。 |
| H-15 | 無操作の shell は切断ではない | トラフィックのない terminal 接続は client が維持するため、プロンプトで停止している shell がリンク断と誤認されて閉じられることはない。 |
| H-16 | File transfer source（デスクトップ） | source 一覧には **File transfer — files viewers send** も含まれる。一覧が表示されるたびに選択し直され、保存されない。terminal（H-11）と同様に、app の同一の UDP port（T-4）、許可済み client（S-2）、network 選択を共有する。ファイルは、共有開始後にライブ表の *File transfer* 行に示されるフォルダへ保存される（H-17、T-25）。1 batch あたり最大 **32** ファイル、1 ファイル **8 GiB**、合計 **32 GiB** である。これを超えるもの、および保存できない名前のものは理由とともに拒否される。各ファイルは最終的な名前に `.deskhub-part` を付けた形で書き込まれ、全体が到着し checksum が一致した時点で改名される。破損したファイルは破棄され、その batch は中断される。既存ファイルの上書きは行わず、同名が存在する場合は番号を付加する。フォルダに書き込めない場合、host はファイルを受け取らず、無言で失敗せずその旨を通知する。 |
| H-17 | 表内の転送 | file transfer の共有中、ライブ表にはフォルダ名を示す *File transfer* の行が表示され、その横の **Open folder** ボタンでそのフォルダをシステムのファイルマネージャーで開ける。送信中のマシンはその下に 1 行ずつ、viewer と同じ方式（C-7）で識別されて並び、受信中のファイル、batch 内の位置、完了率、または batch が中断した理由が表示される。*File transfer* 行の *Stop* はファイル転送のみを終了する。batch の提示、受理、拒否、完了は、当該マシンのアドレス・名前・key とともに session log（G-3）に記録される。 |
| H-14 | Stop & attach（デスクトップ） | shell の各行は、動作中のものも reattach 待ちのものも **Stop & attach** を備える。リモートの client は切断され（その画面には shell の終了が表示される）、同一の shell が host 側の terminal ウィンドウで、内容と scrollback を保ったまま開く。以後その shell は host に帰属する。元の client は reattach できず、時間制限はそもそも適用されず（H-13）、表の当該行は *attached on this machine* と表示され、host のウィンドウを閉じるか当該行の *Stop* を押すと shell が終了する。この引き継ぎも session log（G-3）に記録される。 |
| H-18 | 接続要求 | 許可済み client に含まれない状態で connect してきたデバイスは追い返されない。host は**接続要求**を記録し、Host ページはそれをアドレス一覧とライブ session 表の間に、すべてのプラットフォームで一覧表示する。各行はデバイスの名前（または *(unnamed)*）、key の fingerprint の先頭部分、アドレス、および **Approve** と **Deny** を示す。*Approve* はデバイスの key を、デバイスの名前をラベルとして許可済み client に追加するため、次の試行 —— デバイスは自動的に再試行を続ける（C-5）—— が受け入れられる。*Deny* は要求を破棄し、それ以外には何も起こらないため、デバイスには時間内に誰も承認しなかったとだけ伝わる。要求は **10 分**で自然に失効し、host は最大 **16** 件を保持し、同じデバイスが再度要求した場合は行を追加せず自身の行を更新する。待機中のものがなければ、その節はその旨を 1 行で示す。CLI は `access requests` で一覧し、`access approve` / `access deny --fingerprint SHA256:…` で応答する。実行中の `share` も新しい要求が届くたびに表示する。 |

## 5. Connect —— 他のマシンを閲覧する

| ID | 機能 | 説明 |
| --- | --- | --- |
| C-1 | アドレスによる connect | 利用者は一方の入力欄に host の IP アドレスを、もう一方に UDP port を入力する。後者には既定値 `47777` が入っている。アドレス欄に `192.168.1.10:47777` を貼り付けた場合も有効であり、明示された port が port 欄より優先される。同じ欄は、host の QR code から得た `deskhub://pair/…` リンクの貼り付けも受け付ける（C-12）。この場合 Connect はリンク内のアドレスに接続し、欄には応答したアドレスが入る。不正な入力 —— アドレスでも Deskhub のリンクでもない文字列を含む —— に対しては、失敗ではなく説明を伴うヒントが表示される。 |
| C-2 | host への初回接続 | 本マシンが一度も信頼したことのない host に connect すると、**New host** ダイアログが開き、host の key の fingerprint を示して、host の Devices ページ上のもの（S-3）と照合するよう利用者に求める。*Cancel* は何も保存せず、どこにも connect しない。**Trust and connect** はその host を信頼済み host（S-8）に保存して connect する。そのアドレスが以前は信頼済みの別の host のものだった場合、ダイアログはその host の名前と fingerprint を挙げてその旨を告げる。既知のアドレスにある別の key は別のマシンだからである。既に信頼済みの host には、どのアドレスで到達しても何も確認せずに connect する。QR code（C-12）を通じて到達した host はダイアログなしに信頼される。code が既に fingerprint を含んでおり、応答したマシンがその key を保持していることを証明した場合にのみ接続が進む。 |
| C-3 | control の解除 | connect 前に、viewer は *control the remote machine* の選択を解除し、input を送らずに閲覧のみを行える。input を一切受け取らない host —— スマートフォンやタブレット（P-4）、または input を無効にして共有しているデスクトップ —— は、共有内容を問われた際にその旨を返し、デスクトップの client はそうした host に対して control と terminal が機能しない旨の常設の注記を表示する。 |
| C-4 | source の選択 | host が複数の display を共有している場合、どれを閲覧するかを viewer に確認する。複数を選択すると複数のウィンドウが開く。display が 1 枚のみの場合は直ちに開く。 |
| C-5 | 明確な失敗理由 | 接続が失敗した理由 —— host に到達できない、共有していない、まだ信頼されていない、本デバイスを拒否した、所有者が時間内に本デバイスを承認しなかった（S-2）—— を viewer に正確に明示し、メッセージにアドレスを含める。本デバイスを許可していない host は直ちに失敗させない。client は *Waiting for the owner of ‹host› to approve this device…* を **Cancel** ボタン付きで表示し、最長 **2 分間**、数秒ごとに自動的に再接続を続ける。所有者が *Approve*（H-18）を押した瞬間に次の試行が受け入れられ、ページは何事もなかったように先へ進む。その 2 分が過ぎた後にのみ失敗し、所有者が時間内に承認しなかったこと、および所有者に Host ページの *Connection requests* で *Approve* を押してもらってから再度 connect するよう伝える —— 要求は host に 10 分間残るため、遅れた承認には 2 回目の Connect だけで足りる。失敗の表示は他の 2 つの方法も挙げる。host の QR code をスキャンする（C-12）、または *Devices → Copy public key* から本デバイスの public key をコピーして host の所有者に *Clients allowed to connect* へ貼り付けてもらう。CLI は同じ操作を行う `deskhub-cli access approve --fingerprint SHA256:…`、`deskhub-cli key public`、`deskhub-cli access add --stdin` のコマンドを添える。 |
| C-6 | session 終了の通知 | session が終了した際は、どちら側からの終了であっても viewer に理由が表示される。 |
| C-7 | viewer の名前 | 接続には常に本マシンのデバイス名（T-26）が伴う。connect ページには独自の名前欄はない。host はこの名前を本マシンのアドレスの隣に表示し、viewer を区別できるようにする。 |
| C-8 | shell を開く | *Terminal — open a shell* は host が応答した後に表示されるボタンであり（C-10）、すべての client に存在する。shell は独立したウィンドウで開き、文字グリッド、scrollback、status 行を備える。スマートフォンではさらに補助キーの列（Esc、Tab、ロック可能な Ctrl/Alt、方向キー、^C）が加わる。shell を開けなかった理由（本デバイスが時間内に承認されなかった、拒否、到達不能）も同じウィンドウに表示される。画面の閲覧と shell の利用を同時に行うことは通常の使い方である。すべての client は、何かを開く前に host が何を共有しているかを確認する。terminal を持たない host —— スマートフォン、タブレット、terminal を共有していないデスクトップ —— に対してはボタンが disabled のままとなり、terminal のウィンドウは開かれない。client は、host が保持している shell を問い合わせて id で reattach し、新規に開く代わりとすることもできる。 host が既に shell を保持している場合、このボタンを押すとまず**保持されている shell の一覧**が開く。各行はその id、サイズ、開いたマシンを示す。shell が開くのは、いずれかを reattach するか *New shell* を選んだ後である。何も保持されていなければ、新しい shell が直ちに開く。他者が入力中の shell、および host が引き取った shell（H-14）は一覧に出るが reattach はできない。client が終了させてよい各行には *Close shell* もあり、確認を求めたうえで、誰が保持していてもその shell を host 上で終了させる。 |
| C-9 | ファイルの送信 | すべての client は、ファイルを受け取っている host にファイルを送信できる。*File transfer — send files to it* は host が応答した後に表示されるボタンであり（C-10）、すべての client に存在し、**Send files** の画面を開く。Android と iOS ではシステムの写真ピッカーまたはファイルブラウザから選択し、送信前に app 自身の cache にコピーを用意する。batch は同時に 1 つのみである。実行中はピッカーが無効化され、2 つ目の提示は busy として拒否される。進捗表示には送信中のファイル、batch 内の位置、完了率が示され、転送はいつでも停止できる。終了後、batch 内の各ファイルについて送信の成否と理由が一覧表示される。すべての client は、何かを開く前に host が何を共有しているかを確認する。ファイルを受け取らない host に対してはボタンが disabled のままとなり、ウィンドウは開かれない。 |
| C-10 | connect してから選択する | Connect が行うのは authenticate のみである。host に接続し、その key を確認し（S-8）、本マシン自身の key を証明し（S-2）、host が何を共有しているかを問い合わせる。応答した host が提示する内容はどのプラットフォームでも同一である。アドレス、**Disconnect**、V-7 のライブ状態行、および *Remote desktop — view its screen*、*Terminal — open a shell*、*File transfer — send files to it* の各ボタンであり、host が実際に共有している機能のみが有効になる。その後に開く各 session も同じ key で再度 sign in し、host 側で何かが確認されることはない。これらが表示される位置はプラットフォームごとに異なる（C-11）。 |
| C-11 | host ごとに 1 ウィンドウ（デスクトップ） | Windows、Linux、macOS では、応答した host ごとに独立した**接続ウィンドウ**が開く。タイトルは当該 host のアドレスで、C-10 に挙げた内容をすべて含む。connect ページ自体の状態は変化しない。アドレスと port の各欄、Connect、最近の一覧はそのまま残るため、最初の host を開いたまま次の host に接続でき、1 台が複数の host に同時に接続できる。既にウィンドウがある host に再度 connect した場合は、2 つ目を開かず当該ウィンドウを前面に出す。接続ウィンドウを閉じるか、その **Disconnect** を押すと、その host のみが切断され、他は影響を受けない。app を終了するとすべて閉じる。ウィンドウから開いた session（V-1、C-8、C-9）はそれぞれ独立したウィンドウであり、接続ウィンドウより長く存続する。Android と iOS では接続は同時に 1 つのみで、connect ページ上に留まる。Connect が成功するまで、そのページは各入力欄、Connect、最近の一覧のみで構成される。host が応答すると、それらは C-10 の内容に置き換わり、Disconnect、またはアドレス・port の変更によって初期状態に戻る。 |
| C-12 | QR code による connect | 最も速い初回接続であり、fingerprint の照合も承認も不要である。スマートフォンやタブレットでは Client ページに **Scan QR code** ボタンがあり、カメラを開く。初回は OS がカメラの permission を求める。Deskhub がこれを要求するのはここだけで、他の目的には使わない。拒否した場合、ページはリンクを貼り付ける方法を示す。他の app（system のカメラ、メッセージ）から `deskhub://pair/…` リンクを開いても同じ場所に着く。デスクトップを含むすべての client は、アドレス欄にリンクを貼り付けることもできる（C-1）。code は host の最大 4 つのアドレスと port、host の key の fingerprint、名前、およびランダムな 1 回限りの token を含む。client はアドレスを順に試し、他の何かを送る前に、応答したマシンが code に記された key を保持していることを確認する。保持していなければ、接続は *The machine that answered is not the one that made this QR code* として停止し、token は送られない。保持していれば、host は *New host* ダイアログ（C-2）なしに信頼され、token は本デバイスの key と名前とともに送られ、host はその key をデバイスの名前をラベルとして許可済み client に追加して受け入れる —— Client ページは C-10 の内容へ直ちに進む。各 code は一度だけ、最長 **5 分間**有効であり、host で隠すか共有を停止すると無効化される（H-6）。host が既に許可しているデバイスは、host の現在のアドレスを取得するためだけに code をスキャンすることもできる。通常どおり受け入れられ、token は未使用のまま残る。カメラの frame は端末上で decode され、保存も送信もされない。 |

## 6. マシンの探索

| ID | 機能 | 説明 |
| --- | --- | --- |
| D-1 | network scan なし | Deskhub は network を scan せず、host は discovery packet に応答しない。client は、利用者が入力したアドレス（C-1）、最近のアドレス（D-5）、信頼済み host（S-8）、または host の所有者が見せた QR code 内のアドレス（C-12）によって host に到達する。code は目視または貼り付けで運ばれ、network を通ることはないため、discovery ではない。 |
| D-4 | クリックで接続 | 最近のアドレスを選択するか、信頼済み host の *Connect* を押すと、その host への接続を開始する。 |
| D-5 | 最近のデバイス | 接続したことのある host は Client ページの最近の一覧に最大 **10** 件保持され、それぞれ host の名前（そのデバイス名。T-26。host は本マシンが authenticate した後にのみ送信する。送信されなかった場合はアドレス）、アドレス、最終接続時刻を表示する。host が一覧に加わるのは応答した後に限られる。旧バージョンの最近の一覧は引き継がれない。 |
| D-8 | デバイスの削除 | 最近のアドレスは一覧から削除できる。 |

## 7. session の閲覧

| ID | 機能 | 説明 |
| --- | --- | --- |
| V-1 | ウィンドウへの適合 | リモートの画面はアスペクト比を保ったままウィンドウに合わせて拡縮され、ウィンドウは開いた時点で source の大きさに合わせられる。デスクトップでは、session の途中で stream の形状が実際に変化した場合 —— host であるスマートフォンやタブレットの回転、形状の異なる display への切り替え —— ウィンドウは新しい形状に合わせ直す。形状が変わらず quality のみが変化した場合、ウィンドウは変更しない。 |
| V-2 | ズームとパン | 表示は **5×** までズームでき、パンも可能である。ズーム倍率は表示され、1 操作でリセットできる。 |
| V-3 | session の状態 | ウィンドウにライブの status 行を表示する。frame rate、帯域、round-trip time、end-to-end latency である。 |
| V-4 | タイトル付きウィンドウ | viewer の各ウィンドウには、表示中の source と現在の状態がタイトルとして付くため、複数の session を区別できる。 |
| V-5 | Disconnect | viewer はいつでも session を終了できる。 |
| V-6 | 音声 | 双方が対応している場合（3 節）、viewer は共有されているマシンが再生している音声を、映像とおよそ 1 frame 以内のずれで聴取できる。音声は専用の channel を使用する。packet を 1 つ失った場合の損失は数十ミリ秒にとどまり、映像には影響しない。何も再生していないマシンの帯域消費はごくわずかである。音声を無効にした viewer には届かず（T-23）、無効にした host は送信しない（T-22）。 |
| V-7 | 接続状態の表示 | この表示は host が応答した場所にある。デスクトップでは接続ウィンドウ、Android と iOS では connect ページ（C-11）であり、session のウィンドウではない。host のアドレス、**Disconnect**（V-5）、および接続中であることを示すライブ行を ping とともに表示し、当該 host が応答しなくなった時点で赤色に変わる。値は authenticate 済みの接続上で毎秒 1 回行う ping に由来するため、session を開く前から存在し、複数の session が動作している間も表示され続ける。デスクトップでは開いている host ごとに個別に保持される。host を失った session のウィンドウも、reattach 中である旨を表示する（V-8）。 |
| V-8 | 自動再接続 | host を失った session —— network の切断、stream の停止 —— は直ちには終了しない。ウィンドウは最後のフレームを保持し、reattach 中である旨を表示し、backoff を伴って最大 1 分間再接続を試みる。host が再び応答すれば映像は自動的に再開する。その時間を過ぎた場合、または host が意図的に session を終了もしくは拒否した場合にのみ、理由とともにウィンドウを閉じる。shell のウィンドウは 2 分間自動的に再試行した後に接続の喪失を報告するが、shell 自体は時間制限なく host 上に残り、いつでも復帰できる（H-13）。 |

## 8. リモートのマシンの操作

| ID | 機能 | 説明 |
| --- | --- | --- |
| I-1 | Mouse | 移動、左・右・中・戻る・進むの各ボタン、およびスクロールホイールが host に送信される。 |
| I-2 | Keyboard | 押下と解放の各イベントが、修飾キーの組み合わせを含めて送信される。 |
| I-3 | Pointer lock（デスクトップ） | `F9` により mouse をリモート画面に固定する。生の移動量を前提とするゲームなどで使用する。`F9` または `Esc` で解除する。現在の状態はウィンドウのタイトルに表示される。 |
| I-4 | フォーカス喪失時の安全機構 | フォーカスを失うと pointer lock および押下状態のキーがすべて解放されるため、host にキーが残留することはない。 |
| I-5 | タッチ trackpad（モバイル） | スマートフォンとタブレットでは、映像領域が trackpad として機能する。ドラッグでポインタを移動、タップで左クリック、ダブルタップで右クリック、長押しドラッグでドラッグ、2 本指の縦方向ドラッグでスクロールを行う。 |
| I-6 | ポインタとパンの切り替え（モバイル） | リモートのポインタの移動と、ズームした表示のパンをトグルで切り替える。 |
| I-7 | 画面 keyboard（モバイル） | 端末の keyboard を必要に応じて表示・非表示にでき、リモートのマシンに直接入力する。 |
| I-8 | ホットキーバー（モバイル） | タッチ keyboard での入力が難しいキー用のボタンを提供する。`Esc`、`Tab`、`Enter`、方向キー 4 種、`Del`、`Ctrl+C`、`Ctrl+V`。 |
| I-9 | host の優先 | host のマシンの前にいる利用者による input が、すべてのリモート viewer に優先する。 |
| I-10 | 同時に操作できるのは 1 viewer | mouse と keyboard を操作できる viewer は同時に 1 つのみである。競合した場合は先に参加した viewer が優先され、他の viewer の input は、操作中の viewer が **1 秒**無操作になるまで無視される。 |
| I-11 | view-only の強制 | host が control を無効にしている場合、または viewer が閲覧のみを選択した場合、input は host に到達せず、viewer のウィンドウは view-only である旨を表示する。 |

## 9. アクセス制御と安全性

| ID | 機能 | 説明 |
| --- | --- | --- |
| S-1 | Encrypt | session は encrypt された transport（QUIC/TLS）の上で動作する。session が運ぶすべての内容 —— video、control、input、clipboard、terminal のトラフィック —— は 2 台の間で encrypt される。host は平文では何にも応答しない。当該 port に到達する未 encrypt な packet はすべて破棄され、client が authenticate するまでは何も —— 共有内容さえも —— 明かさない。詳細は [`SECURITY.ja.md`](../SECURITY.ja.md) を参照。 |
| S-2 | 許可済み client による受け入れ制御 | アクセスは SSH と同じ仕組みで行われる。client は自身の machine key の private 側を保持していることを証明し、host はその public key が許可済み client に含まれている場合にのみ受け入れる。key がその一覧に載る方法は 3 つあり、いずれも host の所有者による意図的な行為である。デバイスの接続要求に対して **Approve** を押す（H-18）。デバイスが、所有者が共有中に見せる QR code（C-12）をスキャンまたは貼り付ける —— その 1 回限りの token がクリックの代わりとなる。または所有者がデバイスの public key（デバイス側で *Copy public key* によりコピーしたもの）を Devices ページに貼り付けるか、CLI（`access add --stdin`）で追加する。passcode も、未知のマシンを受け入れるスイッチも存在しない。有効な token か Approve なしに受け入れられる者はいない。token は 1 回限りで 5 分間有効、code を隠すか共有を停止すると無効化され、誤った token は不正な署名と同様に数えられる（S-4）。許可済み client が 1 つもなければ、誰も受け入れられない。key のラベルは表示名にすぎず、権限を意味することはない。 |
| S-3 | Devices ページ | **Devices** ページは 2 つの領域から成る。*When this machine is the host* には、**This machine's host key** —— 接続する側が照合するための `SHA256:` fingerprint と *Copy* ボタン、および他の host の所有者が本マシンを許可するために貼り付ける行をコピーする **Copy public key**。同じ key が、client としての本マシンの sign in にも用いられる —— と、**Clients allowed to connect to this machine** —— 各 client をラベルと fingerprint で示し *Remove* を備え、新しい public key を貼り付ける *Allow* と *Remove every client* を伴う —— がある。client を削除すると、そのデバイスの実行中の session は直ちに切断される。*When this machine is the client* には、**Trusted hosts** —— 各 host を名前、最後に応答したアドレス、fingerprint で示し、*Connect* と *Remove* を備える —— がある。管理すべき key の一覧は存在しない。1 つの machine key が両方の役割を担う（S-9）。 |
| S-4 | 連続失敗時のロックアウト | 1 つの client key と 1 つのアドレスから 1 分以内に **3** 回不正な署名があると、そのアドレスからのその key は **10 秒間**ブロックされる。誤った QR token は、その送信元アドレスに対して同じ制限に数えられるため、token を推測することはできない。host が同時に保持する authenticate 待ちの接続は最大 **8** であり、**10 秒**以内に authenticate しなかった接続は切断される。保持する接続要求も最大 **16** で、それぞれ **10 分間**である（H-18）。他の client は影響を受けない。 |
| S-5 | control のスイッチ | host は *viewers can control this machine* を無効にしたまま共有でき、この場合 viewer の要求内容にかかわらず、すべての session が view-only となる。 |
| S-6 | capture への同意 | それを要求するプラットフォームでは、OS 自身の permission プロンプトと画面選択ダイアログを使用する。利用者が許可しない限り Deskhub は capture できない。 |
| S-7 | 明示的な共有のみ | 利用者が共有を開始するまで、いかなる内容も共有されない。app を閉じるか共有を停止すると、すべての session が終了する。 |
| S-8 | 信頼済み host は key に従う | client は信頼する各 host の key を —— 初回接続時に利用者が fingerprint を確認した後（C-2）、または host の QR code から（C-12）—— 固定し、何かを送信する前にそれを照合する。信頼はアドレスではなく key に属する。新しいアドレスに移った信頼済み host はそこで認識され、ダイアログなしに connect し、*Trusted hosts* はその新しいアドレスを最後のアドレスとして記憶する。既知のアドレスにある別の key は、単に本マシンがまだ会ったことのない host である。他の初回接続と同様に *New host* ダイアログが表示され、そのアドレスが以前は別の信頼済み host のものだったという警告が添えられる。その host は利用者が削除するまで一覧に残る。CLI は未知の host を拒否してその fingerprint を表示し、`--accept-new-host-key` を付けて再実行した場合にのみ保存する。host を事前に固定することもでき（`host add ALIAS --address IP[:PORT] --host-key-stdin`）、`trust forget` は fingerprint、alias、または最後のアドレスを受け取る。 |
| S-9 | Machine key | すべてのマシンは、初回起動時に生成され自動的に置き換えられることのない key をちょうど 1 つ持つ。これが両方の役割における identity であり、生成、import、選択するものは何もない。Devices ページの *Copy public key*（CLI: `key public`）は、本マシンのデバイス名（T-27）をラベルとした 1 行 `ecdsa-sha2-nistp256 AAAA… <device name>` をコピーする。この行は host の所有者が本マシンを許可するために貼り付けるものであり、host が本マシンの接続要求を承認するか QR code で受け入れた際に保存するものでもある。private key がマシンの外に出ることはない。key ファイルを削除するとマシンは新しい identity を得る。それを許可していた host は改めて許可する必要があり、それを信頼していた client には *New host* ダイアログが再び表示される。 |
| S-10 | アクセス設定は引き継がれない | 旧バージョンのアクセス設定は変換されない。7.0.x からの移行では、client が sign in に用いる key が machine key になったため、すべての client を改めて受け入れる必要がある —— 承認または QR code、あるいは key の貼り付けによって。残された `client_key*.pem` と `host_cert.pem` は無視され、読み取りも移行もされない。旧バージョンは本バージョンに connect できず、バージョンが一致しない旨が通知される。7.0.x と 7.1 は互いに通信できないため、すべてのデバイスを更新すること。 |

## 10. Settings

Settings はマシンごとに保持され、再起動をまたいで保存され、次回の共有開始時から有効に
なる。スマートフォンとタブレットが提供するのはデバイス名（T-26）、network port（T-4）、
clipboard 同期（T-17）、keep awake（T-19）、および共有画面上の共有に用いる network（T-9）
である。それ以外の項目は組み込みの既定値を使用する。

デスクトップでは Settings ページがこれらを三つの枠に分ける。**Host** は共有にだけ使う項目
（T-1 – T-3、T-6、T-11、T-22、および T-25 のフォルダー）、**Client** は視聴にだけ使う
項目（T-23）、**General** は両側で使う項目（T-26、T-4、T-13、T-15、T-17、T-19）である。

| ID | Setting | 範囲 | 既定値 |
| --- | --- | --- | --- |
| T-1 | Frame rate | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Quality | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Network port | 1 – 65535 | 47777 |
| T-6 | viewer による本マシンの control を許可する | on / off | on |
| T-9 | Share on network | All networks · 本マシンのいずれかのアドレス | All networks |
| T-11 | app の起動時に共有を開始する | on / off | off |
| T-13 | ログイン時に Deskhub を起動する | on / off | off |
| T-15 | バックグラウンドで動作を継続する | on / off | off |
| T-17 | clipboard のテキストを同期する | on / off | off |
| T-19 | session 中は本デバイスをスリープさせない | on / off | on |
| T-22 | 本デバイスの音声を viewer に共有する | on / off | on |
| T-23 | 閲覧中のデバイスの音声を再生する | on / off | on |
| T-26 | デバイス名 | 最大 64 バイトのテキスト | 空 —— OS が本マシンに付けている名前 |

| ID | 機能 | 説明 |
| --- | --- | --- |
| T-7 | 自動 quality | stream の quality は、設定された上下限の範囲内で、利用可能な network の容量に応じて自動的に調整される。状況が変化しても利用者の操作は不要である。 |
| T-8 | 入力値の検証 | 範囲外の値や数値でない入力は拒否され、直前の値が保持される。適用は行われない。 |
| T-10 | network のフォールバック | 特定の network を指定している場合（T-9）、host にはそのアドレス経由でのみ到達できる。共有開始時にそのアドレスが存在しない場合、host はすべての network で共有し、その旨を共有 status に表示する。保存済みだが現在利用できないアドレスも、*not connected* と付記して一覧に残る。 |
| T-12 | 起動時の自動共有 | デスクトップのみ。T-11 が on の場合、app を開くと直ちに Host ページへ移り、保存済みの settings で共有を開始する。利用者が Share を押した場合と同一である。ログイン時に起動された場合（T-13）、デスクトップにまだ display が存在しないことがある。この場合、共有は待機し、0.5 秒ごとに最大 30 秒まで再確認し、display が現れた時点で開始する。待機中、Host ページには待機状態が表示される。display が現れなかった場合、app は他に選択されている対象（terminal）を共有するか、理由を Host ページに表示したまま停止する。自動共有はダイアログを開かない。ログイン時にはウィンドウが tray に隠れており、利用者の目に入らない可能性があるためである。各プラットフォームの規則は引き続き適用される。Linux は初回にデスクトップの画面共有ダイアログを表示し、以後は保存済みの選択を使用する（P-3）。macOS は引き続き permission を必要とする（P-2）。 |
| T-14 | ログイン時の起動 | デスクトップのみ。T-13 が on の場合、Linux は `~/.config/autostart` に autostart エントリを作成し、Windows は *Deskhub* という名称の scheduled task を登録してログオン時に昇格状態で起動するため UAC のプロンプトは表示されず、macOS は利用者が System Settings でも確認できる Login Item を登録する。off にすると該当のエントリは削除される。チェックボックスは常に OS が報告する状態を表示し、最後に保存された値のみを表示するわけではない。 |
| T-16 | バックグラウンドモード | デスクトップのみ。T-15 が on の場合、tray またはメニューバーにアイコンが表示され、*Show/Hide window*、*Start/Stop sharing*、*Quit* を備える。ウィンドウを閉じても終了せず app が隠れ、共有はバックグラウンドで継続する。ウィンドウは起動時に必ず表示され、利用者が閉じたときのみ隠れるため、T-13、T-11、T-15 を併用するとログイン時に共有を開始し、ウィンドウは閉じられるまで表示され続ける。Windows では tray アイコンの左クリックでウィンドウの表示と非表示を切り替えられる。macOS ではウィンドウが隠れている間、Dock のアイコンが消える。Linux では tray に StatusNotifier host が必要である（KDE では標準、GNOME では AppIndicator 拡張が必要）。存在しない場合、ウィンドウを閉じると終了する。app が操作不能な状態になることを避けるためである。Windows と Linux では、共有中にウィンドウを閉じると、T-15 が off であっても（tray が利用可能であれば）必ず tray に隠れる。接続中の viewer を切断しないためである。macOS ではウィンドウを閉じても app は終了しないため、いずれの場合も共有は継続する。 |
| T-18 | clipboard の同期 | T-17 が on の場合、session 内のいずれかのマシンでコピーされたプレーンテキストが、数秒以内に他のマシンに反映される。双方向であり、host は viewer のコピー内容を他の viewer にも中継する。テキストの上限は 32 KiB で、超過分は文字境界で切り捨てられる。画像、ファイル、書式は転送されない。host のトグルが session 全体を左右し、off の場合 host は clipboard のデータを無視し、送信もしない。各マシンは、自身のローカル clipboard を読み書きするために自身のトグルも on にしておく必要がある。Android と iOS では OS による制約がある。Android 端末は Deskhub が前面にある間のみ自身のコピー内容を取得でき、受信したテキストは常に適用できる。iOS の viewer では、Deskhub が新しいコピー内容を読み取る際にシステムのペースト確認が表示される場合がある。host として動作している iOS 端末は同期に参加しない。broadcast が clipboard にアクセスできない別 process で動作しているためである。 |
| T-20 | スリープ防止 | T-19 が on の場合、共有中および閲覧中はマシンがスリープせず、display も消灯しない。session が終了した時点でこの抑止は解除され、スリープ関連の設定は変更されない。Windows、macOS、Linux では、host と viewer の双方について display のスリープとシステムのスリープの両方が対象となる（Linux では systemd-logind と、freedesktop の screensaver インターフェースに従うデスクトップ環境が必要であり、KDE と GNOME では標準である）。OS が優先される場面では OS の動作に従う。ノート PC の蓋を閉じる、電源ボタンを押す、macOS がバッテリー駆動である場合などは、依然としてスリープに入りうる。Android と iOS では、このトグルは stream の閲覧中に画面を点灯したままにする。スマートフォンからの共有は画面が消灯しても継続するため（P-5）、host 側では画面を保持しない。 |
| T-25 | 受信ファイルの保存先 | デスクトップのみ。viewer が送信したファイルは、本マシンが選択したフォルダに書き込まれる。既定は利用者のホームディレクトリ直下の `Deskhub` である。選択したフォルダは、共有中はライブ表の *File transfer* 行に示され（H-17）、その横に **Open folder** ボタンが並ぶ —— 共有前はまだ意味を持たないため表示されない。フォルダは存在しない場合は作成され、選択は他の settings とともに保存される。そのフォルダの外部には何も書き込まれない。送信側が付けた名前はパスの最後の要素のみに切り詰められ、ローカルの filesystem が保存できない文字は除去される。 |
| T-27 | デバイス名 | Settings → General → *Device name* は、あらゆる場面で本マシンの名前となる。host するときは viewer に表示され（H-7）、受け入れた client の最近の一覧に表示され（D-5）、connect するときは host に表示され（C-7）、コピーする public key のラベル、および host が本デバイスを承認するか QR code で受け入れた際に保存する key のラベルにもなる（S-9）。空の場合は、OS が本マシンに付けている名前を用いる。Windows と Linux は hostname、macOS はコンピュータ名、iOS はデバイス名、Android は機種名である。制御文字は除去される。 |
| T-24 | 共有される音声の内容 | T-22 が on の場合、host は自身のスピーカーが再生している内容、すなわちそのマシン上のすべてのアプリケーションが生成するミックスを共有する。Deskhub は microphone を capture せず、双方向の音声も持たない。permission が関係するのは Android のみである。playback-capture API がシステム上 *Microphone* と表示される permission の背後にあるため、app は共有開始時にこれを要求し、他の用途には使用しない。拒否された場合、共有は音声なしで継続する。他のプラットフォームでは microphone の permission を要求しない。viewer は自身が該当の設定を有効にしている場合にのみ音声を受け取るため（T-23）、T-22 が on の host であっても、受信しない viewer には送信しない。両方のトグルは次回の session 開始時から有効になる。 |

## 11. 状態表示と障害切り分け

| ID | 機能 | 説明 |
| --- | --- | --- |
| G-1 | ライブな host 統計 | display 単位および viewer 単位の capture rate、send rate、帯域、round-trip time。 |
| G-2 | ライブな client 統計 | session 単位の frame rate、帯域、round-trip time、end-to-end latency。 |
| G-3 | Session log | Windows、macOS、Linux では、実行ごとに利用者の Deskhub フォルダへ log ファイルを書き出す。バグ報告への添付を想定している。Android と iOS は診断情報を OS 自身の log ストリームに出力し、ファイルは残さない。 |
| G-4 | バージョンとプロジェクトリンク | app は自身のバージョンを表示し、プロジェクトページへのリンクを提供する。 |

## 12. プラットフォーム固有の挙動

| ID | プラットフォーム | 挙動 |
| --- | --- | --- |
| P-1 | Windows | app は起動時に一度だけ管理者権限を要求する。これが昇格したウィンドウへ入力できる前提となる。共有開始時には自身の firewall ルールを追加する。 |
| P-2 | macOS | **Permissions** パネルを表示し、*Screen Recording*（共有に必要）と *Accessibility*（remote input の受け取りに必要）の現在の許可状態、それぞれの要求ボタン、System Settings へのショートカットを提供する。Accessibility が付与されていない場合、一部のキー入力は macOS によって通知なく遮断される。 |
| P-3 | Linux | Host ページは他のデスクトップと同様に、本マシンの各 display と terminal をチェックボックスとして並べ、選択された display のみが共有される。Share を押した後も、デスクトップは自身の画面共有ダイアログで screen capture を確認する。デスクトップが対応している場合（ScreenCast portal バージョン 4 以降）、この確認は保存され、以後の共有は再起動をまたいでも通知なく再利用するため、ダイアログは初回のみ表示される。デスクトップが許可した内容が選択した display と一致しない場合、保存済みの確認は破棄され、正しい display を許可できるようダイアログが再度表示される。デスクトップが拒否した場合、または compositor の更新やモニタの変更により確認が失効した場合は、ダイアログが再び表示される。ダイアログをキャンセルしても再試行は行われない。デスクトップが許可した display を選択一覧に対応づけられない場合は、何も共有しないのではなく、デスクトップが許可した内容をすべて共有する。terminal のみを選択した場合、デスクトップのダイアログは完全に省略される。なお共有には、システムが input injection を許可していることも必要である。 |
| P-4 | Android / iOS | host としての動作は **view-only** である。端末は画面を送出し、control の packet はすべて破棄する。いずれの OS も app によるシステム全体への input inject を許可していないためである。端末は terminal を共有せず、問い合わせに対してその旨を返すため、スマートフォンに対して terminal のウィンドウを開く client は存在しない（C-8）。デスクトップの client はこれに基づき、control の選択が無効である旨を表示できる（C-3）。app が画面に表示されている間、端末は H-16 の batch 規則に従ってファイルを受け取る。切り替えるスイッチはなく、画面の共有中も受け取りを継続するため、viewer は画面を閲覧しながらファイルを送信できる。iOS では broadcast extension が broadcast の間 1 つの port を保持し、両方の機能をそこで提供する。受信した写真と動画は端末の写真ライブラリに追加される。iOS ではシステムの追加専用の Photos permission を経由し、最初のファイル到着時に要求され、Deskhub に読み取り権限は付与されない。拒否された場合はファイルが Documents に保存される。Android ではシステムのメディアストア経由で `Pictures/Deskhub` と `Movies/Deskhub` に保存される。その他のファイルは、システムのファイルブラウザから参照できる場所（iOS は app の Documents フォルダ、Android は `Download/Deskhub`）に保存され、到着した内容を通知で知らせる。ここで用いる Android のメディアストアには Android 10 が必要であり、Android 9 以前では到着したファイルは端末上の app 自身のフォルダに留まり、ギャラリーにも Downloads にも現れない。画面全体が単一の source として共有されるため、display の選択、複数 display の共有、display 単位の停止（H-1、H-2、H-3、H-5）は適用されない。host として動作中、スマートフォンにもデスクトップと同じ **Show QR code** ボタン（H-6）と、*Approve* / *Deny* を備えた **Connection requests** の一覧（H-18）が表示される。端末を回転させると stream も追随し、viewer に表示される内容は常に正しい向きとなり、そのウィンドウも新しい形状に合わせ直す（V-1）。session の UI はタッチ操作を前提としており、trackpad のジェスチャ、ズーム操作、ホットキーバー、画面 keyboard、display の切り替え、および shell とファイル転送の画面と共通の、隅に配置された閉じるボタンを備える。 |
| P-5 | Android | 共有にはシステムの画面収録の同意ダイアログが必要であり、これは共有ごとに付与され、保存できない。音声の共有には、Android が *Microphone* と表示する permission も必要である。playback-capture API がその背後にあるためで、共有開始時に要求される。拒否された場合は音声なしで画面を共有する。共有中は常駐通知が表示され、app がバックグラウンドに移行しても、画面が消灯しても stream は継続する。システム通知から共有を停止すると session が終了する。`CAMERA` permission は利用者が *Scan QR code*（C-12）をタップしたときにのみ要求され、他の目的には使われない。frame は端末上で decode される。 |
| P-6 | iOS | 共有は app 内の **Start sharing** ボタンから開始する。このボタンはシステムの broadcast シートを開く。iOS がすべての broadcast をそのシートで確認することを要求しているためである。broadcast は独立した process で動作するため、app を閉じた後も継続する。共有画面は接続中の viewer 数を報告し、名前を設定している viewer についてはその名前も表示する（C-7）。あわせて broadcast process の現在のメモリ使用量を報告する。iOS はメモリ上限を超えた broadcast を終了させるためである。この画面には H-7 のような viewer 単位の表はなく、viewer を個別に切断することもできない（H-8）。QR code（H-6）と接続要求（H-18）は表示する。broadcast process が各要求を 2 つの process が共有するフォルダに書き込み、app がそれを一覧し、app での *Approve* を broadcast process がデバイスの次の試行時に読み取る。broadcast を終了させるシステムイベント、たとえば着信は、session を終了させる。app が画面から離れる時点で開いていた session —— stream、shell、ファイル転送 —— は、iOS が画面を離れた app に与える時間、およそ 30 秒の間維持されるため、短時間他の app に切り替えても切断されない。それを超えるとシステムが app を suspend し、session は app が復帰した時点で自動的に reattach する（V-8）。カメラの permission は利用者が *Scan QR code*（C-12）をタップしたときにのみ要求され、他の目的には使われない。 |

## 13. 明確に範囲外とする事項

Deskhub が**提供しない**もの、および本仕様が扱わないもの。

- Microphone の capture、双方向の音声、音声チャネル。音声は共有されているマシンから
  閲覧者への一方向にのみ流れる（V-6）。
- リモート印刷。
- プレーンテキスト以外の clipboard 同期（画像、ファイル、リッチテキスト）。
- アカウント、ディレクトリ、プレゼンスの仕組み、およびサービスを経由する招待。host が
  表示する QR code（C-12）は画面から読み取るか手で貼り付けるものであり、利用者に代わって
  マシン間でそれを運ぶものは存在しない。
- Discovery、relay、rendezvous、NAT-traversal のサービス。インターネット越しに host へ
  到達する手段の用意は利用者の責任であり、たとえば VPN を用いる。
- Session の録画。
- 無人アクセス、wake-on-LAN、リモートの電源制御。
- マルチユーザー管理、ロール、監査証跡。
