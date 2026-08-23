# NKMM_FIX_MOVE_LINE 実装レポート

対象フラグ: `NKMM_FIX_MOVE_LINE`(新規)

## 背景

`macro/MoveLineUp.qjs`・`macro/MoveLineDown.qjs`として、カーソル行と隣接行を
入れ替える(emacsのtranspose-lines、VCのLineTranspose相当)マクロを先に用意した。
これをマクロエンジンを経由しないネイティブコマンドとしても使えるようにした。
マクロ版はそのまま残しており、どちらも独立に使える。

## 追加したファイル

- `changelog/NKMM_FIX_MOVE_LINE.md`(このファイル)

## 修正した既存ファイル

- **`sakura_core/Funccode_x.hsrc`** — `F_MOVE_LINE_UP = 30251`/
  `F_MOVE_LINE_DOWN = 30252`を追加(`F_DUPLICATELINE`直後の空き番号)。
  このファイルはHeaderMakeが解釈するプレーンテキストで、C++の`#ifdef`を
  理解しないため、既存の`F_TAB_DUPLICATE`等と同様に無条件で追加している
  (実際に使われるかどうかは、この定数を参照する側のC++コードを
  `#ifdef NKMM_FIX_MOVE_LINE`で囲むことで制御する)。
- **`sakura_core/cmd/CViewCommander.h`** — `Command_MoveLineUp()`/
  `Command_MoveLineDown()`を宣言。
- **`sakura_core/cmd/CViewCommander.cpp`** — `HandleCommand`の
  switchに`F_MOVE_LINE_UP`/`F_MOVE_LINE_DOWN`の分岐を追加。
- **`sakura_core/cmd/CViewCommander_Edit_word_line.cpp`** —
  `Command_MoveLineUp()`/`Command_MoveLineDown()`本体。
- **`sakura_core/func/Funccode.cpp`** — `pnFuncList_Edit[]`(コマンド一覧)、
  `FuncID_To_HelpContextID()`(ヘルプトピックID。専用トピックが無いため
  `F_DUPLICATELINE`と同じ`HLP000043`を暫定的に流用)、`IsFuncEnable()`
  (`F_CUT_LINE`/`F_DELETE_LINE`と同じく「選択範囲が無ければtrue」)に追加。
- **`sakura_core/doc/CEditDoc.cpp`** — 読み取り専用時に禁止するコマンド一覧
  `EIsModificationForbidden[]`(二分探索用にコマンド番号の昇順ソートを
  維持する必要があるため`F_DUPLICATELINE`の直後に挿入)に追加。
- **`sakura_core/sakura_rc.rc`** / **`sakura_lang_en_US/sakura_lang_rc.rc`** —
  コマンド名の文字列リソースを追加(共通設定→キー割り当て、コマンド
  パレット等で表示される)。`sakura_rc.rc`はUTF-16LE(BOM付き)のため、
  Pythonでbytesとして読み込みUTF-16でデコード/エンコードして編集した
  (CLAUDE.md記載の手順)。
- **`sakura_core/my_config.h`** — `NKMM_FIX_MOVE_LINE`フラグを追加。

## 実装の詳細

### ネイティブ実装がマクロ版と異なる点

マクロ版(`Editor.GoLineTop`/`Up`/`Down`/`BeginSelect`/`InsText`の組み合わせ)は
レイアウト(折り返し)単位のカーソル移動をベースに改行単位の行を組み立てて
いたが、ネイティブ版は`CDocLineMgr::GetLine()`で論理行(`CDocLine`)を直接
参照する。折り返しの影響を一切受けないため、より単純かつ確実。

`CEditView::ReplaceData_CEditView2()`に、入れ替え対象の2行分の範囲を
`CLogicRange`(ロジック単位)で渡すことで、レイアウト再計算・再描画・
Undo/Redo登録(`CReplaceOpe`の自動記録)を任せている。これは
`CViewCommander_Edit_word_line.cpp`の他コマンド(`Command_DUPLICATELINE`等)が
使う`CLayoutPoint`+`InsertData_CEditView`/`DeleteData`の組み合わせより
経路が単純で、Undoの手当ても自前で行う必要がない。

### 最終行(改行無し、EOF行)を跨ぐ場合の扱い

ファイル末尾行が改行で終わっていない(一般的な「最終行に改行を付けない」
慣習のファイル)場合に、その行を巻き込んで入れ替えるときは、改行の
付け替えが必要になる:

- 上へ移動(`Command_MoveLineUp`): カーソル行が改行無しの最終行のとき、
  入れ替え後は上の行(prev)が新しい最終行になる。「新たに必要になる
  改行」は、prevが元々持っていた改行をそのまま転用する(新規に生成
  しない)。
- 下へ移動(`Command_MoveLineDown`): 下の行(next)が改行無しの最終行の
  とき、入れ替え後はカーソル行(cur)が新しい最終行になる。「新たに
  必要になる改行」は、curが元々持っていた改行をそのまま転用する。

どちらも、入れ替え後に最終行になる側は改行を完全に取り除く
(`GetLengthWithoutEOL()`で改行を除いた本文だけを使う)。

### カーソル位置

`ReplaceData_CEditView3`が内部で自動記録する操作前後のキャレット位置
(置換範囲の先頭に固定)とは異なる位置にカーソルを置きたい場合がある
(下へ移動したときは「下の行の先頭」に置きたいが、置換範囲の先頭は
「上の行の先頭」になる)。そのため、`ReplaceData_CEditView2()`の後で
明示的に`GetCaret().MoveCursor()`を呼び、さらに`CMoveCaretOpe`を追記して
Redo時も同じ位置に揃うようにしている
(`Command_DUPLICATELINE`等、このファイルの既存コマンドと同じ手当て)。

### 選択範囲・読み取り専用

`F_CUT_LINE`/`F_DELETE_LINE`と同様、選択範囲がある状態では使えない
(`IsFuncEnable`でグレーアウト。コマンド本体側でも同じ判定を防御的に
再チェックしている)。読み取り専用時に禁止されるコマンド一覧
(`EIsModificationForbidden`)にも登録しているため、書き込み禁止の
ファイルでは自動的に無効化される。

### デフォルトキー割り当て

`sakura_core/func/CKeyBind.cpp`のデフォルトキー割り当てテーブルに、
`Shift+Alt+↑` = 上へ移動、`Shift+Alt+↓` = 下へ移動を追加した
(ユーザー指定)。`VK_UP`/`VK_DOWN`行のShift+Alt+列がどちらも未使用
(`F_0`)だったため、既存の他コマンドとの衝突は無い。`NKMM_FIX_MOVE_LINE`
が無効な場合にF_MOVE_LINE_UP/DOWN(ディスパッチ側の`case`が存在しない
コマンド番号)を直接テーブルに書きたくなかったため、`_MOVE_LINE_UP`/
`_MOVE_LINE_DOWN`という同ファイル内の`_SQL_RUN`等と同じ流儀のマクロを
用意し、フラグ無効時は`F_0`に展開されるようにしてある。

### スコープ外にしたもの

- マクロ(`.qjs`/`.vbs`/`.pas`)から呼び出せるコマンドテーブル
  (`CSMacroMgr.cpp`)への登録は行っていない。マクロから使いたい場合は
  従来通り`macro/MoveLineUp.qjs`/`MoveLineDown.qjs`を使う想定。
- メインメニュー・ツールバーへの追加は行っていない。

## 動作確認について

`msbuild sakura.sln /t:sakura /p:Configuration=Debug /p:Platform=x64`で
フルビルド(リンクまで)が0エラー・0警告で通ることを確認した。

実機(`-PROF=`によるテスト専用プロファイル)でも、`WM_COMMAND`を直接
`PostMessage`する方法で以下を確認済み:

- 通常行同士の入れ替え(上/下とも)
- ファイル最終行(改行無し)を巻き込む入れ替え(上/下とも) —
  改行の付け替えが正しく行われ、行の結合や文字化けが発生しないこと
- 先頭行での上移動、最終行での下移動が何もせず(ErrorBeep相当)に
  安全に無視されること、クラッシュしないこと

いずれも`PostMessage`で`WM_COMMAND`を直接送る方法(funccodeを指定して
コマンドをディスパッチさせる)で確認した。`Shift+Alt+↑/↓`の実キー入力
自体(`CKeyBind`のテーブルからディスパッチまでの経路)は、この
サンドボックス環境で`SendInput`が`ERROR_INVALID_PARAMETER`で失敗し
実キー押下のシミュレーションができなかったため未確認。テーブルの
書き方自体は同じ行の他の列(`F_UP_BOX`等)と全く同じパターンのため
動作するはずだが、実機でShift+Alt+↑/↓を押して割り当てが効くか、
また共通設定→キー割り当て一覧に正しく表示されるかは、ユーザーによる
確認をお願いしたい。

Redo時のカーソル位置の再現(`CMoveCaretOpe`の追記)は実機での
Undo/Redo操作による確認は未実施。
