# NKMM_UNDO_RESTORE_SELECTION 実装レポート

対象フラグ: `NKMM_UNDO_RESTORE_SELECTION`(既存)

## 背景

従来は、範囲選択した状態で削除・置換を行い、その後Undoでテキストを
元に戻しても、キャレットのみが復元され選択状態には戻らなかった。
VS Code等では「範囲選択→削除」をUndoすると、戻ったテキストが改めて
選択状態になる。この挙動をsakura本体にも入れたのが本フラグ。

実装は`3f678076d`(「マルチカーソル処理実装 Ver.3 undo選択領域の復元」、
2026-08-31)。コミット表題はマルチカーソル寄りだが、実際の内容は
このUndo選択復元機能そのものと、`NKMM_MULTI_CURSOR`側のUndo/Redo復元
機構(`nCursorSlot`)を同時に作った回であるため両方が1コミットに
含まれている。マルチカーソルの一般的なアーキテクチャ自体は
`changelog/NKMM_MULTI_CURSOR.md`を参照。本ドキュメントは選択復元
(`bHadSelection`)に絞って説明する。

## 追加したファイル

- `changelog/NKMM_UNDO_RESTORE_SELECTION.md`(このファイル)

## 修正した既存ファイル

`3f678076d`時点(その後`25ba202b6`でUndo/Redo側の共通処理が
`RestoreMultiCursorAfterUndoRedo()`へ関数化されているが、本フラグに
関わるロジック自体は変わっていない):

- **`sakura_core/COpe.h`** — `CReplaceOpe`に`bool bHadSelection = false`を
  追加。「この置換の直前が選択状態だったか」を1個のOpeに焼き込む。
  同時に`COpe`基底に`NKMM_MULTI_CURSOR`用の`int nCursorSlot = -1`を
  追加(マルチカーソル一括編集でどのカーソルの編集かを識別する識別子。
  選択復元自体はこのフラグ無しでも成立するが、マルチカーソル環境下での
  選択復元(後述)はこれに依存する)。
- **`sakura_core/view/CEditView.h`** / **`CEditView_Command_New.cpp`** —
  `ReplaceData_CEditView`/`ReplaceData_CEditView2`/`ReplaceData_CEditView3`
  の末尾に省略可能引数`bool bHadSelection = false`を追加し、
  `ReplaceData_CEditView3`内で生成する`CReplaceOpe`の
  `bHadSelection`にそのまま渡す。`CEditView::DeleteData()`の
  「選択範囲を削除する」分岐から呼ぶ際にのみ`true`を渡している
  (選択なしの素のBackSpace/Deleteでは渡さないため既定の`false`のまま)。
- **`sakura_core/cmd/CViewCommander_Edit.cpp`** —
  `Command_UNDO()`のOPE_REPLACE処理に、`bHadSelection`と設定
  `m_bUndoRestoreSelection`が両方真のときだけ選択状態
  (`CLayoutRange(ptCaretPos_Before, ptRestoredTo)`)を復元する分岐を追加。
  `bFastMode`(大量Opeの一括Undoで詳細な後処理を省略する経路)では
  対象外。マルチカーソル用の一括復元処理
  (`RestoreMultiCursorAfterUndoRedo`、現在は`25ba202b6`で関数化)にも、
  各カーソルスロットの`CReplaceOpe::bHadSelection`を見て選択を復元する
  分岐が同様に入っている。
- **`sakura_core/env/CommonSetting.h`** — `CommonSetting_Edit`に
  `bool m_bUndoRestoreSelection`を追加(ダイアログ項目無し)。
- **`sakura_core/env/CShareData.cpp`** — 既定値`true`で初期化
  (VS Code既定に合わせる)。
- **`sakura_core/env/CShareData_IO.cpp`** — ini `[Common]`セクションに
  `bUndoRestoreSelection`として読み書き。
- **`sakura_core/my_config.h`** — `NKMM_UNDO_RESTORE_SELECTION`フラグ定義
  (このコミットで新規追加)。

## 実装の詳細

### 選択前の状態をどう記憶するか

`CReplaceOpe`(削除・置換のUndo/Redo単位)に`bHadSelection`という
真偽値1個だけを追加している。選択の向き(アンカーがどちら側だったか)
までは持たず、「削除・置換の直前が選択状態だったか」だけを記録する。
Undo時は`m_ptCaretPos_PHY_Before`(削除範囲の先頭)〜
`m_ptCaretPos_PHY_To`(削除範囲の末尾)をそのまま選択範囲として使うため、
選択の向きの情報自体は不要という判断。

値のセットは`CEditView::DeleteData()`が「選択範囲を削除する」分岐で
`ReplaceData_CEditView(..., true /* bHadSelection */)`を呼ぶことで
行われる。素の1文字BackSpace/Delete(選択なし)はこの分岐を通らないため
`bHadSelection`は既定の`false`のままとなり、対象外になる。

対応は`CReplaceOpe`(`OPE_REPLACE`)のみ。矩形選択の削除は別経路
(`DeleteData2`)を通るため対象外(矩形選択モード自体の復元が別途必要に
なるためスコープ外、と`my_config.h`のコメントに明記されている)。

### 単一カーソルでの復元

`Command_UNDO()`のOPE_REPLACE分岐で、`pcReplaceOpe->bHadSelection`と
ini設定`m_bUndoRestoreSelection`が両方真であれば、
`m_pCommanderView->GetSelectionInfo().m_sSelectBgn`を
`ptCaretPos_Before`(削除前の選択開始位置)にセットし、
`m_sSelect`を`CLayoutRange(ptCaretPos_Before, ptRestoredTo)`
(`ptRestoredTo`は`m_ptCaretPos_PHY_To`をレイアウト単位に変換したもの)
にセットする。`bFastMode`では`ptCaretPos_Before`等がこのループ内で
計算されない分岐のため、このブロック自体が対象外になる。

Redo側は選択復元を行わない。削除後には選択すべき対象のテキストが
存在しないため、単一カーソル時代からRedoは一貫して選択を出さない
仕様になっている。

### マルチカーソルでの復元

`NKMM_MULTI_CURSOR`が有効な場合、1回のUndo/Redoブロックに複数カーソル
分の`CReplaceOpe`が積まれる。それぞれの`nCursorSlot`(0=プライマリ、
1以上=extraカーソル)を手がかりに、スロットごとに
「`bHadSelection`が立っている`OPE_REPLACE`」を探し
(`SSlotRestore::pcSelOpe`)、見つかればそのOpeの
`m_ptCaretPos_PHY_Before`/`_To`からプライマリ・各extraそれぞれの
選択(アンカー位置・選択先)を独立に復元する。見つからなければ
そのカーソルはキャレットのみ(選択なし)に戻す。

この復元処理は当初`Command_UNDO`/`Command_REDO`にほぼ同一のコードが
2箇所複製されていたが、後続の`25ba202b6`(「fix マルチカーソル・
リファクタリング」)で`RestoreMultiCursorAfterUndoRedo(pcOpeBlk,
nOpeBlkNum, bIsUndo)`という1つの共通関数に統合されている
(`bIsUndo`で「復元元が`_Before`か`_After`か」「選択復元を行うか
(Undoのみ)」の2点だけを切り替える)。本フラグ自体のロジックは
この関数化で変わっていない。

extraカーソル分の選択は、プライマリの新しい選択アンカー基準の相対値
(`nAnchorRelLine`/`nAnchorRelColumn`)として作り直され、既存の
`m_vExtraCursors`は丸ごと置き換えられる。このブロックに現れなかった
extra(非アクティブだった、または元々存在しなかったもの)は
復元しようがなく消える。

### 設定によるON/OFF

ini `[Common]`セクションの`bUndoRestoreSelection`(既定`true`)で
オン/オフできる。共通設定ダイアログには対応する項目は無く、
ini直接編集専用の設定として実装されている
(`NKMM_MULTI_CURSOR`の`bMultiCursorMergeOverlapping`と同じ運用)。

## 動作確認について

`git log`上、`NKMM_UNDO_RESTORE_SELECTION`関連のコードは実装コミット
(`3f678076d`)以降、`25ba202b6`(関数化リファクタ)、`1a490c48e`
(`NKMM_UNDO_COALESCE_TYPING`追加時にini初期化コードの前後に新規
`#ifdef`ブロックが挿入されただけ)、`dfb62427a`・`aa3e68b85`
(Undo履歴パネル関連、行番号がずれているだけで内容の変更は無し)で
触れられているが、いずれも本フラグの判定ロジック自体は変更していない
ことをdiffで確認した。

このエージェントによる調査は`git show`でのコードリーディングのみで、
実機ビルド・GUI動作確認は行っていない。以下は本ドキュメント作成時点で
コード上確認できていない、または実機確認が望まれる点:

- 単一カーソル・マルチカーソルいずれについても、実際にsakura.exeを
  起動して「選択→削除→Undo」で選択が復元されることを確認したという
  記録は、今回読んだ範囲のコミットログ・コメントには見当たらなかった
  (`NKMM_MULTI_CURSOR`本体の一般的な動作確認については
  `changelog/NKMM_MULTI_CURSOR.md`側を参照)。
- `bFastMode`経路(大量Opeの一括Undo/Redo)は仕様上この機能の対象外だが、
  「大量」の具体的な閾値・発生条件は今回のコード読解の範囲では特定して
  いない。
- 置換(検索/置換ダイアログ経由のReplace等)で選択状態から実行した
  ケースが`bHadSelection=true`を経由するかどうかは、`DeleteData()`
  以外の`ReplaceData_CEditView`系呼び出し元を全て洗い出せておらず未確認。
