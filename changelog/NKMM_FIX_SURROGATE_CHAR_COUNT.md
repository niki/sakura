# NKMM_FIX_SURROGATE_CHAR_COUNT 修正レポート

対象フラグ: `NKMM_FIX_SURROGATE_CHAR_COUNT`（新規）
対象ファイル(主なもの):

- `sakura_core/mem/CNativeW.h` / `.cpp`（`GetCharCountInRange()`追加）
- `sakura_core/view/CEditView.cpp`（`GetDocumentWordNum()`）
- `sakura_core/view/CViewSelect.cpp`（選択範囲の「文字数でカウント」）
- `sakura_core/view/CCaret.cpp`（「文字単位」表示モードのキャレット桁位置）
- `sakura_core/my_config.h`

---

## 背景

桁数・文字数カウントの一部が、UTF-16のコード単位(`wchar_t`)数をそのまま「文字数」として扱っており、サロゲートペア文字(絵文字等)を1文字ではなく2文字とカウントしてしまっていた。

一方、ルーラー基準の桁位置表示（`bCaretHabaMode == true`のとき）は既存の`CMemoryIterator`/`CNativeW::GetKetaOfChar`経由でサロゲートペアを正しく1文字(全角2桁)として扱っており、影響を受けない。

## 原因

以下の3箇所が、`CNativeW::GetSizeOfChar()`（サロゲートペアなら2、それ以外は1を返す既存ヘルパー）を経由せず、行の生の長さやインデックスの差分をそのまま文字数として扱っていた。

1. `CEditView::GetDocumentWordNum()` — ドキュメント全体の文字数(ステータスバー表示)。各行の`GetLengthWithoutEOL()`(コード単位長)を単純合計。
2. `CViewSelect::PrintSelectionInfoMsg()`内の「文字数でカウント」ブロック — `LineColumnToIndex()`の差分や`GetLengthWithoutEOL()`をそのまま加算。
3. `CCaret::ShowCaretPosInfo()`の「文字単位」列表示（`m_bDispColByChar`有効時）— `GetCaretLogicPos().GetX()`(生のコード単位インデックス)をそのまま桁として表示。

## 対応

`CNativeW`に論理文字数を数える新規ヘルパーを追加し、上記3箇所をこれに置き換えた。

```cpp
//! 指定範囲[nIdxFrom, nIdxTo)の論理文字数を返す(サロゲートペアは1文字として数える)
int CNativeW::GetCharCountInRange( const wchar_t* pData, int nDataLen, int nIdxFrom, int nIdxTo );
```

内部では`CNativeW::GetSizeOfChar()`を使い、`CMemoryIterator`と同じ要領でインデックスを1論理文字ずつ進めながら数える。

## 既知の制限

`CCaret.cpp`の「行番号をCRLF単位で表示」（`m_bLineNumIsCRLF == true`）と「文字単位」表示を同時に使う組み合わせ（`ShowCaretPosInfo()`内、`bCaretHabaMode == false`かつ`pTypes->m_bLineNumIsCRLF == true`の分岐）は今回のフラグでは未対応。

この分岐で使う`GetCaretLogicPos().GetX()`は折り返し前の物理(CRLF)行全体を基準にしたコード単位インデックスだが、`ShowCaretPosInfo()`内で保持している`pLine`/`nLineLen`は現在表示中の折り返しセグメント(レイアウト行)のデータのみで、複数行に折り返されている場合は物理行全体のデータを参照できない。そのため、物理行全体のバッファを取得する手段を別途用意しないと正しく修正できず、今回はスコープ外とした。

## 動作確認について

サロゲートペア(絵文字)を含むテキストで、ステータスバーの文字数表示・選択範囲の文字数表示・「文字単位」列表示が、絵文字1個につき+1で数えられることを確認する想定。実機での確認は未実施。
