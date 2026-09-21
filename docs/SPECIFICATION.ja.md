[English](SPECIFICATION.md) · [Tiếng Việt](SPECIFICATION.vi.md) · [中文](SPECIFICATION.zh.md) · **日本語**

# Deskhub — 機能仕様

本書は Deskhub が**何をするか**を、利用者から見える形で記述したものである。製品仕様で
あり設計書ではない。実装の詳細、protocol の説明、build 手順は含まない。それらは
[`INSTALL.ja.md`](INSTALL.ja.md)、[`BUILD.ja.md`](BUILD.ja.md)、
[`SECURITY.ja.md`](../SECURITY.ja.md)、および source ツリーに記載している。

本書は [`SPECIFICATION.md`](SPECIFICATION.md) の翻訳。食い違いがある場合は英語版が正文。

- **状態:** 現在のコードの挙動を記述している。
- **読者:** 本製品が何をすべきかを把握する必要がある関係者 —— tester、reviewer、
  コントリビュータ、ストア掲載文の作成者。

---

## 1. 製品概要

Deskhub は、あるマシンの画面を同じ network 上の他のマシンに表示し、それらのマシンから
mouse と keyboard を操作できるようにする。アプリケーションは 1 つであり、同じ app が
画面の共有と他マシンの閲覧の両方を担う。デスクトップのマシンは **terminal**、すなわち
host 上の実際の shell も共有でき、他のマシンはそれを自身のウィンドウで開く（4 節と
5 節）。

installer は必須ではなく、アカウント、サインイン、background service、クラウド構成要素
のいずれも存在しない。2 台は、双方が到達できる network 上で IP アドレスにより相互に
発見する。

## 2. 用語

| 用語 | 意味 |
| --- | --- |
| **Host** | 画面（または terminal）を共有される側のマシン。 |
| **Client** / **Viewer** | host を閲覧し、場合によっては操作するマシン。 |
| **Source** | host 上で共有できる対象。display または terminal を指す。host は複数を同時に共有できる。 |
| **Session** | 1 つの viewer が 1 つの source を閲覧する単位。source ごとに独立したウィンドウで開く。 |
| **Key** | マシンが初回起動時に生成する暗号的な identity。利用者には fingerprint（`SHA256:…`）として表示される。 |
| **Pairing** | host がそのマシンを継続的に受け入れること。pair 済みのマシンは key で識別され、forget されるまで passcode なしで connect できる（9 節）。 |
| **Passcode** | 未知のマシンが pair する前に host が要求できる、任意の 4 桁のコード。 |

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
**Settings**、および本マシンと pair したマシンを一覧する **Devices** ページである
（9 節）。

デスクトップ 3 種には command line client もある。ページ形式の画面を持たないまま同じ
挙動を提供する。host として動作し、connect し、remote shell を開き、マシンを探索し、
app と同一の settings、pair 済みマシン一覧、trust 済み host key を読み書きする。本書に
記載した挙動の別形態のインターフェースであり、異なる挙動ではない。

---

## 4. Host —— 本マシンの画面を共有する

| ID | 機能 | 説明 |
| --- | --- | --- |
| H-1 | display の選択 | 共有前に、本マシンのどの display を提供するかを利用者が選択する。最低 1 つの選択が必要である。 |
| H-2 | 複数 display の共有 | 複数の display を同時に共有できる。各 display は独立した source となり、viewer が選択する。 |
| H-3 | source の上限 | 同時に共有できる display は最大 **8** 枚である。マシンがそれ以上の display を持つ場合、最初の 8 枚のみが共有される旨を利用者に通知する。 |
| H-4 | 共有の開始と停止 | 1 つの操作で開始し、1 つの操作で停止する。現在の状態は常に表示される（*Not sharing* / *Starting share…* / *Sharing*）。 |
| H-5 | display 単位の停止 | 共有中の display を 1 枚のみ停止でき、共有全体は終了しない。 |
| H-6 | 接続情報 | 共有中、app は本マシンの network アドレスと viewer が使用する port を一覧表示する。読み上げや複製に用いる。デスクトップでは *Share on network*（T-9）が host 画面のこの一覧の隣に配置され、一覧には選択した network のアドレスのみが表示される。*All networks* を選ぶとすべて表示される。共有中および開始処理中はこの選択が固定され、変更するには共有を停止する必要がある。 |
| H-7 | ライブ session 表 | 共有中の display ごとに、host は display 名、解像度、viewer 数、capture rate、send rate、使用帯域、round-trip time を確認できる。接続中の viewer は各 display の下に 1 行ずつ表示され、名前が設定されている場合は「名前 (ip:port)」で（C-7）、設定されていない場合はアドレスのみで識別される。 |
| H-8 | viewer の切断 | host は session 表から任意の viewer を個別に切断できる。 |
| H-9 | viewer の上限 | 1 つの host を同時に閲覧できるのは最大 **5** viewer である。それを超える要求は busy として拒否される。 |
| H-10 | 失敗時の報告 | 共有を開始できない場合、無言で失敗せず理由を表示する。port が使用中で terminal を起動できない場合も、その旨を明示する。 |
| H-11 | Terminal source（デスクトップ） | source 一覧には **Terminal — a shell on this machine** も含まれる。この項目は一覧が表示されるたびに選択し直され、保存されない。画面のみ、terminal のみ、両方のいずれの構成も有効である。いずれも app の同一の UDP port（T-4）、passcode、network 選択を共有する。 |
| H-12 | 表内の shell session | terminal の共有中、ライブ表には port を伴う *Terminal* の行が表示され、開いている shell はその下に 1 行ずつ、viewer と同じ方式（C-7）で識別されて並び、*Disconnect* が付く。*Terminal* 行の *Stop* は terminal の共有のみを終了する。同時に開ける shell は最大 **8** である。shell の open、close、reattach は、client のアドレス・名前・key とともに session log（G-3）に記録される。 |
| H-13 | shell は host に残り続ける | 接続を失った shell —— network の切断、または client ウィンドウの終了 —— は、内容と scrollback を保ったまま host 上に保持され、時間制限はない。元の client は自動的に reattach する。2 分間、間隔を延ばしながら再試行し、その間は reattach 中である旨を表示し、同じ shell を復帰させる。許可された client はいずれも、host が保持している shell を問い合わせて id で reattach し、新規に開く代わりとすることができる。shell が終了するのは、その shell プロセスが終了したとき、host が terminal の共有を停止したとき、または session 表から閉じられたときのみであり、client のウィンドウを閉じても shell が終了することはない。 許可された client は、その一覧から保持中の shell を id で終了させることもできる。その shell に入力していたマシンには、shell が終了した旨が伝えられる。 |
| H-15 | 無操作の shell は切断ではない | トラフィックのない terminal 接続は client が維持するため、プロンプトで停止している shell がリンク断と誤認されて閉じられることはない。 |
| H-16 | File transfer source（デスクトップ） | source 一覧には **File transfer — files viewers send** も含まれる。一覧が表示されるたびに選択し直され、保存されない。terminal（H-11）と同様に、app の同一の UDP port（T-4）、passcode、network 選択を共有する。ファイルは選択欄の下および共有 status に示されるフォルダへ保存される（T-25）。1 batch あたり最大 **32** ファイル、1 ファイル **8 GiB**、合計 **32 GiB** である。これを超えるもの、および保存できない名前のものは理由とともに拒否される。各ファイルは最終的な名前に `.deskhub-part` を付けた形で書き込まれ、全体が到着し checksum が一致した時点で改名される。破損したファイルは破棄され、その batch は中断される。既存ファイルの上書きは行わず、同名が存在する場合は番号を付加する。フォルダに書き込めない場合、host はファイルを受け取らず、無言で失敗せずその旨を通知する。 |
| H-17 | 表内の転送 | file transfer の共有中、ライブ表にはフォルダ名を示す *File transfer* の行が表示され、送信中のマシンはその下に 1 行ずつ、viewer と同じ方式（C-7）で識別されて並び、受信中のファイル、batch 内の位置、完了率、または batch が中断した理由が表示される。*File transfer* 行の *Stop* はファイル転送のみを終了する。batch の提示、受理、拒否、完了は、当該マシンのアドレス・名前・key とともに session log（G-3）に記録される。 |
| H-14 | Stop & attach（デスクトップ） | shell の各行は、動作中のものも reattach 待ちのものも **Stop & attach** を備える。リモートの client は切断され（その画面には shell の終了が表示される）、同一の shell が host 側の terminal ウィンドウで、内容と scrollback を保ったまま開く。以後その shell は host に帰属する。元の client は reattach できず、時間制限はそもそも適用されず（H-13）、表の当該行は *attached on this machine* と表示され、host のウィンドウを閉じるか当該行の *Stop* を押すと shell が終了する。この引き継ぎも session log（G-3）に記録される。 |
| H-18 | はっきり見える passcode | host 画面はペアリング用 passcode を単独で表示する —— 数字は間隔をあけ、大きな等幅フォントで —— その横の **Copy passcode** ボタンがコードそのものをクリップボードに入れる。コードは共有状態の文に埋め込まれなくなり、その文は説明のみを保つ。passcode 未設定のときは *None* と表示され、コピーボタンは出ない。passcode 自体は、どのプラットフォームでも **Settings** ページで他のセキュリティ設定と並んで編集する。 |

## 5. Connect —— 他のマシンを閲覧する

| ID | 機能 | 説明 |
| --- | --- | --- |
| C-1 | アドレスによる connect | 利用者は一方の入力欄に host の IP アドレスを、もう一方に UDP port を入力する。後者には既定値 `47777` が入っている。アドレス欄に `192.168.1.10:47777` を貼り付けた場合も有効であり、明示された port が port 欄より優先される。不正な入力に対しては、失敗ではなく説明を伴うヒントが表示される。 |
| C-2 | passcode の入力 | passcode 欄は空のままでもよい。入力する場合は正確に 4 桁である必要があり、そうでなければ何も送信する前に connect が拒否される。欄に表示されている値がそのまま使用され、利用者の関知しない値が補われることはない。空の場合は host が判断する。pair 済みのマシンはそのまま受け入れられ、pair していないマシンは、host 側の利用者に受け入れの可否が確認される約 1 分間待機する（S-2）。入力したコードが host に拒否された場合は、passcode を明示したメッセージとともに失敗する。デバイス一覧から開くダイアログには、そのデバイスの UDP port と保存済みの passcode（D-7）が、いずれも編集可能な状態で入力済みで表示される。 |
| C-3 | control の解除 | connect 前に、viewer は *control the remote machine* の選択を解除し、input を送らずに閲覧のみを行える。input を一切受け取らない host —— スマートフォンやタブレット（P-4）、または input を無効にして共有しているデスクトップ —— は、共有内容を問われた際にその旨を返し、デスクトップの client はそうした host に対して control と terminal が機能しない旨の常設の注記を表示する。 |
| C-4 | source の選択 | host が複数の display を共有している場合、どれを閲覧するかを viewer に確認する。複数を選択すると複数のウィンドウが開く。display が 1 枚のみの場合は直ちに開く。 |
| C-5 | 明確な失敗理由 | host に到達できない、共有していない、passcode を拒否された、のいずれであるかを viewer に明示し、メッセージにアドレスを含める。 |
| C-6 | session 終了の通知 | session が終了した際は、どちら側からの終了であっても viewer に理由が表示される。 |
| C-7 | viewer の名前 | connect ページの *Your name* 欄が本デバイスの名前を設定する。利用者が最初に名前を設定するまでは、プラットフォームごとの既定値が入力されている。Windows と Linux は hostname（hostname がない場合はログインユーザー名）、macOS はコンピュータ名、iOS はデバイス名、Android は機種名である。この欄は編集可能で、connect した時点の内容が保存され送信される。値が未設定になることはない。空にして connect した場合は上記の既定値が復元され、それが欄に再入力されたうえで保存・送信されるため、接続には常に名前が伴う。host はこの名前を本マシンのアドレスの隣に表示し、viewer を区別できるようにする。名前は本デバイスに保存され、テキストは最大 **64** バイト、制御文字は除去される。古いバージョンの host はこの名前を表示しない。 |
| C-8 | shell を開く | *Terminal — open a shell* は host が応答した後に表示されるボタンであり（C-10）、すべての client に存在する。shell は独立したウィンドウで開き、文字グリッド、scrollback、status 行を備える。スマートフォンではさらに補助キーの列（Esc、Tab、ロック可能な Ctrl/Alt、方向キー、^C）が加わる。shell を開けなかった理由（passcode の誤り、拒否、到達不能）も同じウィンドウに表示される。画面の閲覧と shell の利用を同時に行うことは通常の使い方である。すべての client は、何かを開く前に host が何を共有しているかを確認する。terminal を持たない host —— スマートフォン、タブレット、terminal を共有していないデスクトップ —— に対してはボタンが disabled のままとなり、terminal のウィンドウは開かれない。client は、host が保持している shell を問い合わせて id で reattach し、新規に開く代わりとすることもできる。 host が既に shell を保持している場合、このボタンを押すとまず**保持されている shell の一覧**が開く。各行はその id、サイズ、開いたマシンを示す。shell が開くのは、いずれかを reattach するか *New shell* を選んだ後である。何も保持されていなければ、新しい shell が直ちに開く。他者が入力中の shell、および host が引き取った shell（H-14）は一覧に出るが reattach はできない。client が終了させてよい各行には *Close shell* もあり、確認を求めたうえで、誰が保持していてもその shell を host 上で終了させる。 |
| C-9 | ファイルの送信 | すべての client は、ファイルを受け取っている host にファイルを送信できる。*File transfer — send files to it* は host が応答した後に表示されるボタンであり（C-10）、すべての client に存在し、**Send files** の画面を開く。Android と iOS ではシステムの写真ピッカーまたはファイルブラウザから選択し、送信前に app 自身の cache にコピーを用意する。batch は同時に 1 つのみである。実行中はピッカーが無効化され、2 つ目の提示は busy として拒否される。進捗表示には送信中のファイル、batch 内の位置、完了率が示され、転送はいつでも停止できる。終了後、batch 内の各ファイルについて送信の成否と理由が一覧表示される。すべての client は、何かを開く前に host が何を共有しているかを確認する。ファイルを受け取らない host に対してはボタンが disabled のままとなり、ウィンドウは開かれない。 |
| C-10 | connect してから選択する | Connect が行うのは authenticate のみである。host に接続し、pairing または passcode の検証を完了し（S-2）、host が何を共有しているかを問い合わせる。応答した host が提示する内容はどのプラットフォームでも同一である。アドレス、**Disconnect**、V-7 のライブ状態行、および *Remote desktop — view its screen*、*Terminal — open a shell*、*File transfer — send files to it* の各ボタンであり、host が実際に共有している機能のみが有効になる。session を開く際には完了済みの pairing を再利用するため、host 側の利用者に再度確認が行われることはない。これらが表示される位置はプラットフォームごとに異なる（C-11）。 |
| C-11 | host ごとに 1 ウィンドウ（デスクトップ） | Windows、Linux、macOS では、応答した host ごとに独立した**接続ウィンドウ**が開く。タイトルは当該 host のアドレスで、C-10 に挙げた内容をすべて含む。connect ページ自体の状態は変化しない。アドレス、port、passcode、名前の各欄、Connect、デバイス一覧はそのまま残るため、最初の host を開いたまま次の host に接続でき、1 台が複数の host に同時に接続できる。既にウィンドウがある host に再度 connect した場合は、2 つ目を開かず当該ウィンドウを前面に出す。接続ウィンドウを閉じるか、その **Disconnect** を押すと、その host のみが切断され、他は影響を受けない。app を終了するとすべて閉じる。ウィンドウから開いた session（V-1、C-8、C-9）はそれぞれ独立したウィンドウであり、接続ウィンドウより長く存続する。Android と iOS では接続は同時に 1 つのみで、connect ページ上に留まる。Connect が成功するまで、そのページは各入力欄、Connect、デバイス一覧のみで構成される。host が応答すると、それらは C-10 の内容に置き換わり、Disconnect、またはアドレス・port・passcode の変更によって初期状態に戻る。 |

## 6. マシンの探索

| ID | 機能 | 説明 |
| --- | --- | --- |
| D-1 | network scan | client はローカル network を scan して現在共有中のマシンを一覧表示し、scan 中は進捗を表示する（「*m* 件中 *n* 件のアドレスを確認」）。結果が得られない場合は、マシンが表示されない理由 —— 共有中でなければ現れないこと —— を通知する。 |
| D-2 | scan の範囲 | 1 回の scan はローカル subnet の最大 **512** アドレスを対象とする。ローカル network アドレスを持たないマシンでは、scan を実行できない旨を通知する。 |
| D-3 | 自動での再 scan | scan は定期的に繰り返され、*Refresh now* で任意に再実行できる。 |
| D-4 | クリックで接続 | 検出されたデバイスをクリックすると、そのデバイスへの接続を開始する。 |
| D-5 | 最近のデバイス | 接続したことのあるマシンは *Devices* 一覧に最大 **10** 台保持され、*Where* 列に *Recent* と表示され、アドレス、status、ping、最終接続時刻を伴う。 |
| D-6 | ライブな状態表示 | 最近のデバイスはそれぞれ **Online**、**Offline**、**Checking…** を round-trip time とともに表示し、**30 秒**ごとに自動更新されるほか、手動でも更新できる。 |
| D-7 | 保存された passcode | あるデバイスに使用した passcode はそのデバイスとともに保存され、デバイス一覧から connect する際にダイアログへ入力済みで表示される。編集可能な欄に明示的に表示され、暗黙に用いられることはない。コードを入力せずに connect しても保存済みの値は消えず、新しいコードを入力した場合は置き換わる。保存は伏せ字化した形で行われるが、これは利便性のためであって保護手段ではない（9 節）。マシンが pair された後はコードは用いられず、key によって識別される。 |
| D-8 | デバイスの削除 | 最近のデバイスは一覧から削除できる。 |

## 7. session の閲覧

| ID | 機能 | 説明 |
| --- | --- | --- |
| V-1 | ウィンドウへの適合 | リモートの画面はアスペクト比を保ったままウィンドウに合わせて拡縮され、ウィンドウは開いた時点で source の大きさに合わせられる。デスクトップでは、session の途中で stream の形状が実際に変化した場合 —— host であるスマートフォンやタブレットの回転、形状の異なる display への切り替え —— ウィンドウは新しい形状に合わせ直す。形状が変わらず quality のみが変化した場合、ウィンドウは変更しない。 |
| V-2 | ズームとパン | 表示は **5×** までズームでき、パンも可能である。ズーム倍率は表示され、1 操作でリセットできる。 |
| V-3 | session の状態 | ウィンドウにライブの status 行を表示する。frame rate、帯域、round-trip time、end-to-end latency である。 |
| V-4 | タイトル付きウィンドウ | viewer の各ウィンドウには、表示中の source と現在の状態がタイトルとして付くため、複数の session を区別できる。 |
| V-5 | Disconnect | viewer はいつでも session を終了できる。 |
| V-6 | 音声 | 双方が対応している場合（3 節）、viewer は共有されているマシンが再生している音声を、映像とおよそ 1 frame 以内のずれで聴取できる。音声は専用の channel を使用する。packet を 1 つ失った場合の損失は数十ミリ秒にとどまり、映像には影響しない。何も再生していないマシンの帯域消費はごくわずかである。音声を無効にした viewer には届かず（T-23）、無効にした host は送信しない（T-22）。 |
| V-7 | 接続状態の表示 | この表示は host が応答した場所にある。デスクトップでは接続ウィンドウ、Android と iOS では connect ページ（C-11）であり、session のウィンドウではない。host のアドレス、**Disconnect**（V-5）、および接続中であることを示すライブ行を ping とともに表示し、当該 host が応答しなくなった時点で赤色に変わる。値はデバイス一覧に用いるのと同じ毎秒 1 回の probe に由来するため、session を開く前から存在し、複数の session が動作している間も表示され続ける。デスクトップでは開いている host ごとに個別に保持される。host を失った session のウィンドウも、reattach 中である旨を表示する（V-8）。 |
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
| S-1 | Encrypt | session は encrypt された transport（QUIC/TLS）の上で動作する。session が運ぶすべての内容 —— video、control、input、clipboard、terminal のトラフィック —— は 2 台の間で encrypt される。Discovery beacon が平文であるのは設計上の意図であり、機密情報は含まない。当該 port に到達するそれ以外の未 encrypt な packet はすべて破棄される。詳細は [`SECURITY.ja.md`](../SECURITY.ja.md) を参照。 |
| S-2 | pairing による受け入れ制御 | マシンが初めて connect した際、受け入れの可否は host が判断する。host の passcode を提示できるマシンは、そのコードを知っていることを暗号的に証明したことになり、コード自体は network を通過しない。コードを提示しないマシン、および host がコードを設定していない場合は、host 側の利用者に判断が委ねられる。*Let this machine in?* に対して **Allow** と **Deny** が提示され、1 回の回答がそのマシンが開こうとしている対象（画面と shell の双方）すべてに適用される。受け入れは 2 台を **pair** することを意味し、以後そのマシンは key によって識別され、forget されるまで passcode なしで connect できる。ただし入力されたコードは常に検証される。pair 済みのマシンであっても、誤ったコードを提示した場合は拒否される。 |
| S-3 | passcode は任意、pair 済み一覧 | passcode は任意であり既定では空である。空の場合、host 側の利用者が承認しない限り、いかなるマシンも受け入れられない。pair 済みのマシンは **Devices** ページに、名前、key、pair した時刻、最後に確認された時刻とともに一覧表示され、*Forget* と *Forget every machine*、オフにすると pair 済みのマシンのみを受け入れる *allow new pairings* スイッチ、および照合用に本マシン自身の key が提供される。host は共有内容を、受け入れたマシンにのみ開示する。 |
| S-4 | 連続失敗時のロックアウト | passcode を **3** 回誤ると、host の pairing は **30 秒間**ロックされ、試行中のマシンには待機するよう通知される。pair 済みのマシンは影響を受けない。 |
| S-5 | control のスイッチ | host は *viewers can control this machine* を無効にしたまま共有でき、この場合 viewer の要求内容にかかわらず、すべての session が view-only となる。 |
| S-6 | capture への同意 | それを要求するプラットフォームでは、OS 自身の permission プロンプトと画面選択ダイアログを使用する。利用者が許可しない限り Deskhub は capture できない。 |
| S-7 | 明示的な共有のみ | 利用者が共有を開始するまで、いかなる内容も共有されない。app を閉じるか共有を停止すると、すべての session が終了する。 |
| S-8 | key 変更時の警告 | client は trust した各 host の key を保持する。その key が変化した場合 —— これは中間者攻撃に特徴的な兆候である —— 新しい fingerprint を示す明確な警告が表示され、利用者が明示的に受け入れるまで接続は拒否される。一度も確認されたことのない key は pairing handshake 自体が処理し、確認は求められない。 |

## 10. Settings

Settings はマシンごとに保持され、再起動をまたいで保存され、次回の共有開始時から有効に
なる。スマートフォンとタブレットが提供するのは network port（T-4。network scan が確認
する port も兼ねる）、clipboard 同期（T-17）、keep awake（T-19）、および共有画面上の
passcode（T-5）と共有に用いる network（T-9）である。それ以外の項目は組み込みの既定値を
使用する。

| ID | Setting | 範囲 | 既定値 |
| --- | --- | --- | --- |
| T-1 | Frame rate | 1 – 240 fps | 60 |
| T-2 | Bitrate | 1 – 1000 Mbps | 20 |
| T-3 | Quality | 720p · 1080p · 1440p · Native | 1080p |
| T-4 | Network port | 1 – 65535 | 47777 |
| T-5 | Passcode | 空、または正確に 4 桁 | 空（S-2、S-3 を参照） |
| T-6 | viewer による本マシンの control を許可する | on / off | on |
| T-9 | Share on network | All networks · 本マシンのいずれかのアドレス | All networks |
| T-11 | app の起動時に共有を開始する | on / off | off |
| T-13 | ログイン時に Deskhub を起動する | on / off | off |
| T-15 | バックグラウンドで動作を継続する | on / off | off |
| T-17 | clipboard のテキストを同期する | on / off | off |
| T-19 | session 中は本デバイスをスリープさせない | on / off | on |
| T-21 | 新しいマシンの pair を許可する（Devices ページ） | on / off | on |
| T-22 | 本デバイスの音声を viewer に共有する | on / off | on |
| T-23 | 閲覧中のデバイスの音声を再生する | on / off | on |

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
| T-25 | 受信ファイルの保存先 | デスクトップのみ。viewer が送信したファイルは、本マシンが選択したフォルダに書き込まれる。既定は利用者のホームディレクトリ直下の `Deskhub` である。選択したフォルダは、共有前はファイル転送の選択欄の隣に、共有中は共有 status に表示され、存在しない場合は作成され、他の settings とともに保存される。そのフォルダの外部には何も書き込まれない。送信側が付けた名前はパスの最後の要素のみに切り詰められ、ローカルの filesystem が保存できない文字は除去される。 |
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
| P-4 | Android / iOS | host としての動作は **view-only** である。端末は画面を送出し、control の packet はすべて破棄する。いずれの OS も app によるシステム全体への input inject を許可していないためである。端末は terminal を共有せず、問い合わせに対してその旨を返すため、スマートフォンに対して terminal のウィンドウを開く client は存在しない（C-8）。デスクトップの client はこれに基づき、control の選択が無効である旨を表示できる（C-3）。app が画面に表示されている間、端末は H-16 の batch 規則に従ってファイルを受け取る。切り替えるスイッチはなく、画面の共有中も受け取りを継続するため、viewer は画面を閲覧しながらファイルを送信できる。iOS では broadcast extension が broadcast の間 1 つの port を保持し、両方の機能をそこで提供する。受信した写真と動画は端末の写真ライブラリに追加される。iOS ではシステムの追加専用の Photos permission を経由し、最初のファイル到着時に要求され、Deskhub に読み取り権限は付与されない。拒否された場合はファイルが Documents に保存される。Android ではシステムのメディアストア経由で `Pictures/Deskhub` と `Movies/Deskhub` に保存される。その他のファイルは、システムのファイルブラウザから参照できる場所（iOS は app の Documents フォルダ、Android は `Download/Deskhub`）に保存され、到着した内容を通知で知らせる。ここで用いる Android のメディアストアには Android 10 が必要であり、Android 9 以前では到着したファイルは端末上の app 自身のフォルダに留まり、ギャラリーにも Downloads にも現れない。画面全体が単一の source として共有されるため、display の選択、複数 display の共有、display 単位の停止（H-1、H-2、H-3、H-5）は適用されない。端末を回転させると stream も追随し、viewer に表示される内容は常に正しい向きとなり、そのウィンドウも新しい形状に合わせ直す（V-1）。session の UI はタッチ操作を前提としており、trackpad のジェスチャ、ズーム操作、ホットキーバー、画面 keyboard、display の切り替え、および shell とファイル転送の画面と共通の、隅に配置された閉じるボタンを備える。 |
| P-5 | Android | 共有にはシステムの画面収録の同意ダイアログが必要であり、これは共有ごとに付与され、保存できない。音声の共有には、Android が *Microphone* と表示する permission も必要である。playback-capture API がその背後にあるためで、共有開始時に要求される。拒否された場合は音声なしで画面を共有する。共有中は常駐通知が表示され、app がバックグラウンドに移行しても、画面が消灯しても stream は継続する。システム通知から共有を停止すると session が終了する。 |
| P-6 | iOS | 共有は app 内の **Start sharing** ボタンから開始する。このボタンはシステムの broadcast シートを開く。iOS がすべての broadcast をそのシートで確認することを要求しているためである。broadcast は独立した process で動作するため、app を閉じた後も継続する。共有画面は接続中の viewer 数を報告し、名前を設定している viewer についてはその名前も表示する（C-7）。あわせて broadcast process の現在のメモリ使用量を報告する。iOS はメモリ上限を超えた broadcast を終了させるためである。この画面には H-7 のような viewer 単位の表はなく、viewer を個別に切断することもできない（H-8）。broadcast を終了させるシステムイベント、たとえば着信は、session を終了させる。app が画面から離れる時点で開いていた session —— stream、shell、ファイル転送 —— は、iOS が画面を離れた app に与える時間、およそ 30 秒の間維持されるため、短時間他の app に切り替えても切断されない。それを超えるとシステムが app を suspend し、session は app が復帰した時点で自動的に reattach する（V-8）。 |

## 13. 明確に範囲外とする事項

Deskhub が**提供しない**もの、および本仕様が扱わないもの。

- Microphone の capture、双方向の音声、音声チャネル。音声は共有されているマシンから
  閲覧者への一方向にのみ流れる（V-6）。
- リモート印刷。
- プレーンテキスト以外の clipboard 同期（画像、ファイル、リッチテキスト）。
- アカウント、ディレクトリ、プレゼンス、招待の仕組み。
- Relay、rendezvous、NAT-traversal のサービス。インターネット越しに host へ到達する
  手段の用意は利用者の責任であり、たとえば VPN を用いる。
- Session の録画。
- 無人アクセス、wake-on-LAN、リモートの電源制御。
- マルチユーザー管理、ロール、監査証跡。
