# NKMM_FIX_UNDO_BUFFER_LIMIT

対象フラグ: `NKMM_FIX_UNDO_BUFFER_LIMIT`(20260802)

## 背景

`COpeBuf`(Undo/Redoバッファ)は元々件数・データ量とも無制限で、`CDeleteOpe`/
`CInsertOpe`/`CReplaceOpe`が保持するテキストのコピー(`COpeLineData`、実体は
`CNativeW`)がセッション中ずっと積み上がり続ける。超長い行を削除しても、その
コピーがUndo履歴に残り続ける限りメモリは解放されない
(`NKMM_FIX_SHRINK_LINE_BUFFER`は「現在表示されている行」のバッファしか
縮められないため、この分は対象外)。

## 対応

件数ではなくデータ量(バイト数)で上限を管理する。共通設定「編集」タブにKB単位の
入力欄を追加(0=無制限、既定0=従来通りの挙動を維持)。

- `COpe::GetDataByteSize()`(仮想関数、既定0)を`CDeleteOpe`/`CInsertOpe`/
  `CReplaceOpe`でオーバーライドし、保持する`CNativeW`の実バッファ容量
  (`_GetMemory()->capacity()`)を合計する。`COpeBlk::AppendOpe()`のたびに加算して
  ブロック単位のバイト数をO(1)でキャッシュし、`COpeBuf`側でも全ブロックの合計を
  O(1)で追跡する(履歴全体を毎回舐めない)。
- `COpeBuf::AppendOpeBlk()`の末尾で上限判定(`_ShrinkToBudget()`)を行い、超過して
  いたら一番古い(Undo方向の)ブロックから破棄する。Redo対象
  (`m_nCurrentPointer`以降)は直後に必要になり得るため破棄しない。
- 「保存済みに一致する」基準点(`m_nNoModifiedIndex`、行ごとの変更行表示に使う)が
  破棄対象に含まれていた場合は-1(追跡不能)にする。この場合、行ごとの「変更行」
  表示が実態より多め(安全側)になるだけで、ファイル全体の変更フラグ
  (`CDocEditor::IsModified()`)は別の独立したフラグのため影響を受けない。

対象ファイル:
- `sakura_core/env/CommonSetting.h`: `CommonSetting_Edit::m_nUndoBufMaxKB`
- `sakura_core/env/CShareData.cpp,CShareData_IO.cpp`: 既定値・INI永続化
- `sakura_core/COpe.h`: `COpe::GetDataByteSize()`
- `sakura_core/COpeBlk.h,cpp`: `COpeBlk::GetByteSize()`
- `sakura_core/COpeBuf.h,cpp`: `COpeBuf::_ShrinkToBudget()`
- `sakura_core/prop/CPropComEdit.cpp`、`sakura_rc.h,rc`: 共通設定UI
  (`IDD_PROP_EDIT`「編集」タブに追加。EN_US言語版rcは未対応 20260802。
  未対応でもビルド・実行は可能で、この設定のUIが出ないだけ)

## 実装メモ

- アップダウンコントロール(`IDC_SPIN_UNDOBUFMAXKB`)を追加。Win32のアップダウン
  コントロールは刻み幅をネイティブに持たず、矢印クリックのたびに来る
  `UDN_DELTAPOS`通知をアプリ側で解釈する仕組みのため、他の項目(1刻み)と違い
  ここでは4KBずつ増減させている。
