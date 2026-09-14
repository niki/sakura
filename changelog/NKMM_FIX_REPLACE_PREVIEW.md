# NKMM_FIX_REPLACE_PREVIEW 実装レポート

対象フラグ: `NKMM_FIX_REPLACE_PREVIEW`(新規)

## 背景

置換ダイアログ(`CDlgReplace`)で「すべて置換」を実行する前に、実際に
何が・どう変わるのかを確認できないため、意図しない置換に気付きにくい。
検索文字列・置換後文字列・各オプション(大文字/小文字区別、単語単位、
正規表現)を入力するたびに、実際に置換を実行せずその場でサンプルを
見られるようにした。

なお、`sakura_core/my_config.h`のフラグ定義コメントには「結果一覧リスト
形式」「`dlg/CDlgReplacePreview.h/.cpp`(新規)」「`Command_REPLACE_PREVIEW()`」
「`IDC_BUTTON_PREVIEW`」といった、複数の一致箇所を一覧表示する別ダイアログ
を想定した記述があるが、実際にコミット(`da538dd99`)された内容はこれとは
異なる。`CDlgReplacePreview`というファイル・クラスは存在せず、
`Command_REPLACE_PREVIEW`という関数も無い。実装されているのは、既存の
置換ダイアログ内に「置換前」「置換後」それぞれ1行分のサンプル欄を追加し、
カーソルに最も近い一致を1件だけライブ表示する方式である(詳細は後述)。
このレポートは、コメントの記述ではなく実際の差分の内容に基づいて書いて
いる。

## 追加したファイル

- `changelog/NKMM_FIX_REPLACE_PREVIEW.md`(このファイル)

コード側の新規ファイルは無い(既存の`CDlgReplace.cpp`/`.h`と
`CViewCommander_Search.cpp`/`.h`への追記のみ)。

## 修正した既存ファイル

- **`sakura_core/cmd/CViewCommander.h`** — `ComputeReplaceSample()`を宣言。
- **`sakura_core/cmd/CViewCommander_Search.cpp`** — `ComputeReplaceSample()`
  本体を追加。
- **`sakura_core/dlg/CDlgReplace.h`** — サンプル欄用のメンバ
  (`m_crSampleText`/`m_crSampleBack`/`m_hbrSampleBack`/`m_hFontSample`/
  `m_strSampleAfter`/`m_nSampleAfterHighlightPos`/`m_nSampleAfterHighlightLen`)
  と、`OnCbnEditChange()`/`DispatchEvent()`/`OnDrawItem()`/
  `UpdateSamplePreview()`/`SetSampleAfterText()`を追加。
- **`sakura_core/dlg/CDlgReplace.cpp`** — 上記メンバ関数の実装本体。
  `OnInitDialog`/`OnDestroy`/`OnBnClicked`にサンプル更新のフック処理を追加。
- **`sakura_core/sakura_rc.h`** — 新規コントロールID
  `IDC_STATIC_REPLACESAMPLE_BEFORE`(1906)/`IDC_STATIC_REPLACESAMPLE_AFTER`
  (1907)を追加。
- **`sakura_core/sakura_rc.rc`** — 置換ダイアログ(`IDD_REPLACE`)に
  「置換前:」ラベル+`IDC_STATIC_REPLACESAMPLE_BEFORE`(LTEXT)、
  「置換後:」ラベル+`IDC_STATIC_REPLACESAMPLE_AFTER`(LTEXT、
  `SS_OWNERDRAW`)の2行を追加。バイナリ差分(UTF-16LE)のため、
  CLAUDE.md記載の手順(bytesとして読み込みUTF-16でデコード/エンコード)で
  編集されている。
- **`sakura_core/my_config.h`** — `NKMM_FIX_REPLACE_PREVIEW`フラグを追加。
  前述の通り、コメント本文は実装(単一サンプルのインラインプレビュー)と
  一致しない設計メモが残っている。

## 実装の詳細

### サンプル計算: `ComputeReplaceSample()`

`CViewCommander::ComputeReplaceSample()`は、文書を一切変更しない読み取り
専用の関数で、指定した検索文字列・置換後文字列・オプションに対して
「最初に見つかる一致箇所」を1件だけ探す。探索はカーソル行
(`nStartLine`)から末尾まで、見つからなければ先頭から`nStartLine`まで
折り返して行う(通常の「次を検索」と同じ考え方で、カーソルに近い一致を
優先する)。

一致箇所が見つかった行について、置換前の行全体(`outBefore`)と置換後の
行全体(`outAfter`)、および一致箇所の位置・長さ(`outMatchPos`/
`outMatchLenBefore`/`outMatchLenAfter`)を返す。正規表現の場合は、一致した
部分文字列だけに対して`CBregexp::Replace()`をもう一度かけることで
`$1`/`$&`等の後方参照を解決した実際の文字列を得ている(行全体を置換して
末尾の未変更部分を自前で切り詰める方式は境界計算を誤りやすいため、この
方式にしたとコード中のコメントに記載がある)。正規表現以外は
`CSearchAgent::CreateWordList`+`SearchStringWord`(単語単位)、または
`CSearchStringPattern`+`CSearchAgent::SearchString`(通常の文字列検索)を
使う。検索対象は`CDocLine::GetLengthWithEOL()`で改行込みの行(実際の
単発置換`Command_REPLACE`と同じ範囲)。

`DoGrepReplaceFile`(Grep置換)や`Command_REPLACE_ALL`の内部ロジックを直接
呼び出す・共有する実装ではなく、`ComputeReplaceSample()`は独立に書かれた
専用コードである。全行を回して複数件の一致を集める処理でもなく、最初に
見つかった1件で探索を打ち切って返る(`return true`)。

### UI: ダイアログ内サンプル欄(一覧リストではない)

追加されたのは`IDD_REPLACE`ダイアログ内の2行の静的テキスト欄のみで、
別ウィンドウの一覧リスト(`ListBox`/`ListView`)ではない。

- `IDC_STATIC_REPLACESAMPLE_BEFORE` — 置換前欄。通常の`LTEXT`。一致した
  語句そのもの(文脈なし)を表示する。`WM_CTLCOLORSTATIC`を
  `DispatchEvent()`で横取りし、エディタの通常テキストの配色
  (`CTypeSupport(pcEditView, COLORIDX_TEXT)`で取得した文字色・背景色)で
  描画する。
- `IDC_STATIC_REPLACESAMPLE_AFTER` — 置換後欄。`SS_OWNERDRAW`指定で、
  `OnDrawItem()`が自前描画する。周辺の文脈込みで表示し、置換された語句の
  部分だけを青字(`RGB(0,0,255)`)で強調表示する。

いずれもフォントは`CTypeSupport::GetTypeFont()`で取得したエディタの
設定フォント(通常テキスト)を`WM_SETFONT`で設定している。

### いつ更新されるか

`UpdateSamplePreview()`が以下のタイミングで呼ばれ、サンプル欄を更新する。

- `OnInitDialog`(ダイアログを開いた直後の初期表示)
- `OnCbnEditChange`(`IDC_COMBO_TEXT`/`IDC_COMBO_TEXT2`の入力欄編集中、
  1文字入力するたびに更新されるライブプレビュー)
- `IDC_CHK_LOHICASE`/`IDC_CHK_WORD`/`IDC_CHK_REGULAREXP`のチェック切替時
  (`IDC_CHK_LOHICASE`/`IDC_CHK_WORD`は元々コメントアウトされていた
  `OnBnClicked`のcaseを、このコミットで有効化して流用している)
- `IDC_RADIO_TARGET`系(置換対象の切替)・`IDC_CHK_PASTE`(クリップボード
  貼り付けチェック)変更時

`UpdateSamplePreview()`内では、`CDlgFind`のライブ入力プレビューと同じ
手法で`CEditView::m_strCurSearchKey`/`m_sCurSearchOption`を更新し、文書
中の一致箇所をスクロールバー上にもマークしている(検索文字列が空なら
マーク解除)。検索履歴(`CSearchKeywordManager`)への登録や入力欄自体の
書き換えは行わない副作用フリーな実装。

### 長い行・改行を挟む一致の扱い

- `MakeContextWindow()`(無名namespace内のヘルパ)が、一致箇所の前後
  5文字だけを切り出し、省略した側に`...`を付ける。これにより長い行でも
  置換後欄が一致箇所を中心に収まる。
- `VisualizeEol()`が`\r\n`/`\r`/`\n`を`↵`記号に変換して表示する
  (正規表現が改行自体にマッチする場合や、置換後文字列が改行を挿入する
  場合でも見えるようにするため)。強調表示位置(`nHighlightPos`/
  `nHighlightLen`)もこの変換後の位置に追従して補正される。

### 対応範囲・非対応の明示(スコープ外)

`UpdateSamplePreview()`内で明示的にガードされている。

- 置換対象ラジオが「置換」(`IDC_RADIO_REPLACE`)以外(挿入/追加/行削除等)
  の場合、または「クリップボードから貼り付け」(`IDC_CHK_PASTE`)が
  チェックされている場合は、サンプル欄に
  「（この設定はサンプル表示に対応していません）」と表示し、実際の計算は
  行わない。`my_config.h`のコメントにも「位置依存・外部データ依存で
  行単位の事前計算が破綻しやすいため」と理由が記されている。
- つまりこのプレビューが対応するのは「置換」(現在のファイル内での
  すべて置換に使う対象設定)のみで、単発の1件置換(`Command_REPLACE`)や
  Grep置換(ファイル横断、`DoGrepReplaceFile`)にはこのプレビューUI自体が
  組み込まれていない(`CDlgReplace`とは別のダイアログのため対象外)。
- 「この内容で置換」のような、プレビューから直接「すべて置換」を実行する
  専用ボタン(`IDC_BUTTON_PREVIEW`)は存在しない。既存の
  `IDC_BUTTON_REPALCEALL`(すべて置換)ボタンとそのハンドラ
  (`Command_REPLACE_ALL`)がこのコミットで変更された形跡はなく、実行経路は
  従来のまま。

## 動作確認について

このコミット自体にビルド結果や実機確認についての記述は残っていない
(コミットメッセージは`feat 置換ダイアログに結果プレビューを追加`の1行
のみで、本文なし)。現時点のコードを`grep`した限り、`NKMM_FIX_REPLACE_PREVIEW`
は上記6ファイル(`CViewCommander.h`/`CViewCommander_Search.cpp`/
`CDlgReplace.h`/`CDlgReplace.cpp`/`sakura_rc.h`/`my_config.h`)以外では
参照されておらず、`my_config.h`のコメントが述べる別ダイアログ
(`CDlgReplacePreview`)や専用コマンド(`Command_REPLACE_PREVIEW`)は
リポジトリ内に一度も存在しない。ビルド・実機での動作確認は本レポート
作成時点では行っていない。
