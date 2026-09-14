# NKMM_UNDO_COALESCE_TYPING 実装レポート

対象フラグ: `NKMM_UNDO_COALESCE_TYPING`(新規)

## 背景

従来のsakuraエディタは1文字入力するたびに1回のUndo単位が積まれるため、
「hello」と入力してからCtrl+Zを押すと「o」→「l」→…と1文字ずつしか戻せない。
VS Code等のエディタは、区切り文字(空白・句読点等)や一定時間の入力停止まで
連続した入力を1つのUndo単位としてまとめる挙動になっており、これに合わせる
オプションを追加した。既定値はOFF(従来通り1文字単位)で、共通設定「編集」
タブから有効化できる。

## 追加したファイル

- `changelog/NKMM_UNDO_COALESCE_TYPING.md`(このファイル)

## 修正した既存ファイル

- **`sakura_core/COpe.h`** — `IsUndoCoalesceBreakChar()`の宣言、および
  `COpe`に結合可否の分類を持たせる`ECoalesceKind`(`COALESCE_UNKNOWN`/
  `COALESCE_WORD`/`COALESCE_BREAK`)と`eCoalesceKind`メンバを追加。
- **`sakura_core/COpeBlk.h`/`.cpp`** — ブロック単位で「後続の入力を結合して
  よいか」を表す`IsCoalesceOpen()`/`SetCoalesceOpen()`、最後に結合した時刻を
  持つ`GetCoalesceTick()`/`SetCoalesceTick()`、結合処理で使う
  `DetachSingleOpe()`(唯一の要素を取り出して空にする)を追加。
- **`sakura_core/COpeBuf.cpp`/`.h`** — `IsUndoCoalesceBreakChar()`本体
  (区切り文字の判定)、アイドル時間の定数`UNDO_COALESCE_IDLE_MS`、
  結合本体の`TryMergeIntoLastOpeBlk()`を追加。
- **`sakura_core/cmd/CViewCommander_Edit.cpp`** — `Command_WCHAR()`で、
  今挿入した1文字が区切り文字かどうかをその場で判定し、直前に追加した
  `CInsertOpe`の`eCoalesceKind`へ書き込む。
- **`sakura_core/cmd/CViewCommander_Clipboard.cpp`** — `Command_INSTEXT()`
  (IME変換確定・クリップボード貼り付け等の文字列一括挿入経路)でも同様に、
  挿入文字列全体を走査して区切り文字を含むかどうかを分類する。
- **`sakura_core/view/CEditView.cpp`** — `SetUndoBuffer()`で、設定が有効な
  場合に`TryMergeIntoLastOpeBlk()`を呼び、結合できなければ従来通り
  `AppendOpeBlk()`する分岐を追加。
- **`sakura_core/env/CommonSetting.h`** — `CommonSetting_Edit`に
  `bool m_bUndoCoalesceTyping`を追加。
- **`sakura_core/env/CShareData.cpp`** — 既定値`false`を設定。
- **`sakura_core/env/CShareData_IO.cpp`** — ini項目`bUndoCoalesceTyping`の
  読み書きを追加。
- **`sakura_core/prop/CPropComEdit.cpp`** — 共通設定「編集」タブの
  チェックボックス(`IDC_CHECK_bUndoCoalesceTyping`)との`SetData`/`GetData`
  を追加。
- **`sakura_core/sakura_rc.h`** — `IDC_CHECK_bUndoCoalesceTyping = 1752`を
  `NKMM_UNDO_COALESCE_TYPING`ガード付きで追加。
- **`sakura_core/sakura_rc.rc`** — 上記チェックボックスのダイアログリソース
  追加(バイナリ差分。UTF-16LE(BOM付き)のためCLAUDE.md記載の手順で編集)。
  英語版`sakura_lang_en_US/sakura_lang_rc.rc`は未対応(コメントに明記、
  ビルド・実行は可能だがUIが出ない)。
- **`sakura_core/my_config.h`** — `NKMM_UNDO_COALESCE_TYPING`フラグ定義と、
  仕組み全体を説明する長いコメントを追加。

## 実装の詳細

### 結合可否の分類はCOpeではなく「挿入した側」が行う

`InsertData_CEditView`経由の挿入操作(`COpe`/`CInsertOpe`)は、実際に何を
挿入したかという文字列データ自体を保持しない(`m_cOpeLineData`が空のまま)。
そのため、後からブロックの中身を調べても「区切り文字を挿入したのか、
通常の文字を挿入したのか」を判別できない。この設計上の制約に対応するため、
実際に入力された文字・文字列を知っている呼び出し側で直接分類する方式に
なっている:

- `Command_WCHAR()`(`CViewCommander_Edit.cpp`) — 通常のキー入力1文字。
- `Command_INSTEXT()`(`CViewCommander_Clipboard.cpp`) — IME変換確定
  (`WM_IME_COMPOSITION`経由の`F_INSTEXT_W`)およびクリップボード貼り付け
  (Ctrl+V)などの文字列一括挿入。

挿入直後にブロック末尾の`CInsertOpe`(`OPE_INSERT`)を取り出し、
`eCoalesceKind`へ`COALESCE_WORD`(区切り文字を含まない)または
`COALESCE_BREAK`(区切り文字を含む)を書き込む。分類していない挿入経路は
`COALESCE_UNKNOWN`のままとなり、結合対象外として自動的に除外される。

### 結合を打ち切る4条件

`COpeBuf::TryMergeIntoLastOpeBlk()`が、新しいブロックを直前のブロックへ
結合してよいかを判定する。結合が止まり次の入力から新しいまとまりが
始まるのは以下のいずれか:

1. **区切り文字の入力** — `IsUndoCoalesceBreakChar()`(`COpeBuf.cpp`)が
   ASCIIの記号類(`. , ! ? ; : ' " ( ) [ ] { } < > / \ | ` ~ ^ * + = @ # $ % & -`)
   および日本語の主な句読点・括弧類(`、`。`・`「」『』【】〈〉《》`全角
   `！？（），．：；`)と`iswspace()`をtrueと判定する文字。識別子で使われる
   `_`は区切りに含めない。
2. **アイドル時間** — 直前の結合から`UNDO_COALESCE_IDLE_MS = 1500`
   (ミリ秒、コード上のリテラルで1.5秒)を超えて`GetTickCount64()`の差が
   開いた場合。区切り文字が無くても新しいまとまりになる。
3. **改行(Enter)** — 改行文字は`iswspace()`によって`IsUndoCoalesceBreakChar()`
   がtrueを返すため、他の区切り文字と同じ経路で必ず区切りになる
   (改行専用の分岐は無い)。
4. **カーソル移動等の別操作の介在** — 直前のブロック最後の操作の「操作後」
   キャレット位置(`m_ptCaretPos_PHY_After`)と、新ブロック最初の操作の
   「操作前」キャレット位置(`m_ptCaretPos_PHY_Before`)が一致しない場合。
   カーソル移動・マウスクリック・選択操作等が間に挟まると自動的に不一致に
   なり結合されない。

このほか、結合対象になるのは「1個の`CInsertOpe`のみで構成され、かつ挿入側で
分類済みのブロック」に限られる(`IsClassifiedSingleInsertBlk()`)。
`NKMM_MULTI_CURSOR`の一括編集や選択範囲への上書き入力など複数操作が混在する
ブロックは対象外(結合せず、結合の起点にもしない)。

### 結合処理そのもの

条件を満たすと、新ブロックから`DetachSingleOpe()`で唯一の`COpe`を取り出し、
直前のブロック(`m_vCOpeBlkArr[m_nCurrentPointer - 1]`)へ`AppendOpe()`で
追記し、新ブロック自体は`delete`する。Redo方向の履歴が残っている場合
(`IsEnableRedo()`)は結合しない。呼び出し元の`CEditView::SetUndoBuffer()`は
`TryMergeIntoLastOpeBlk()`が`false`を返したときのみ、従来通り
`AppendOpeBlk()`する。

### 設定項目

共通設定「編集」タブの「元に戻す」グループに、チェックボックス
(`IDC_CHECK_bUndoCoalesceTyping`)としてUIを追加した。ini項目名は
`bUndoCoalesceTyping`、既定値`false`(従来通りの1文字単位のUndoを維持)。

## 動作確認について

コミットメッセージ・diff上には、ビルド確認やGUI実機確認についての記述は
残っていない。本レポートはコミット`1a490c48e`の差分と現在のコードを静的に
読んだ内容のみに基づいており、実機での動作確認(実際にキー入力してUndo単位が
まとまるか、1.5秒のアイドル判定が意図通り働くか、共通設定ダイアログの
チェックボックスが正しく表示・保存されるか等)は本レポート作成時点では
未実施。

なお、ユーザーの記憶(セッション履歴)によれば、この機能の実装過程で
「`CInsertOpe`自体が挿入テキストを保持しない」という制約に起因する実バグが
見つかり、後からブロックの中身を調べて分類する方式から、挿入した呼び出し側
(`Command_WCHAR`等)でその場に分類結果をタグ付けする現在の方式に修正された
とされている。本レポートで読んだ現在のコード(`COpe::eCoalesceKind`を
`Command_WCHAR()`/`Command_INSTEXT()`が挿入直後に設定する構成)は、この
記憶の記述と一致している。また同記憶によれば、GUI自動化による実機確認で
末端まで動作することが確認済みとされているが、その具体的な確認ログは
本レポート作成時点のリポジトリ上には見当たらず、コミット差分・コードからは
検証できない。
