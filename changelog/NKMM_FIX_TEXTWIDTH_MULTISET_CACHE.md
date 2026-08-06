# テキスト最大幅算出のstd::multiset化（全行スキャンの完全排除） 20260806

対象フラグ: `NKMM_FIX_TEXTWIDTH_MULTISET_CACHE`（新規、`NKMM_FIX_TEXTWIDTH_TOPK_CACHE`の上位版）
対象ファイル:

- `sakura_core/doc/layout/CLayoutMgr.h`（`m_multisetTextWidth`,
  `_TextWidthMultisetErase`, `_TextWidthMultisetInsert`）
- `sakura_core/doc/layout/CLayoutMgr.cpp`（`CreateLayout`, `_Empty`,
  `DeleteLayoutAsLogical`）
- `sakura_core/doc/layout/CLayoutMgr_New.cpp`（`CalculateTextWidth`）
- `sakura_core/doc/layout/CLayoutMgr_DoLayout.cpp`（`CalculateTextWidth_Range`）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

`NKMM_FIX_TEXTWIDTH_TOPK_CACHE`(次点候補8件方式)の実装後、「std::multisetなどを
用いて実装する」という要望を受け、より完全な解決策として実装した。

## 前段: TOPK方式の限界

`NKMM_FIX_TEXTWIDTH_TOPK_CACHE`は「幅の上位8件」を候補として保持し、最大幅行が
編集で無効化されても次点があれば全行スキャンを回避する方式だった。実用上は
効果があるが、以下の弱点があった。

- 候補8件が同時編集で全滅すると全行スキャンにフォールバックする(稀だが原理上
  起こりうる)。
- 候補の識別に「レイアウト行番号(表示行の何行目か)」を使っていたため、挿入・
  削除のたびに全候補の行番号をシフトする処理が必要で、この手のオフバイワン
  リスクを抱えていた(本セッションの冒頭で修正した`m_pLayoutPrevRefer`
  ダングリングポインタと同種のバグを埋め込みかねない箇所)。

## 対応

全レイアウト行の(幅, `CLayout*`)を`std::multiset<std::pair<CLayoutInt,CLayout*>>`
として常時保持する方式に置き換えた。`CLayout*`をキーの一部に使うことで、行番号
シフトの処理が一切不要になる(ポインタは行の位置が変わっても不変)。

各`CLayout`ノードは元々`SetLayoutWidth()`で常に最新の幅を保持しており
(`CLayoutMgr.cpp:422`、レイアウト構築のたびに更新される)、この値が変わる/
ノードが生成・削除される箇所は以下の4つだけであることを確認した。

1. `CLayoutMgr::CreateLayout()` — 新規ノード生成時に幅を設定する唯一の箇所
   (`SLayoutWork::_CreateLayout`経由で、全ノード生成がここを通る)
2. `CLayoutMgr::CalculateTextWidth()`の`bCalLineLen`分岐 — 既存ノードの幅を
   再計算する唯一の箇所(`SetLayoutWidth`の呼び出し箇所は全コード中これと
   `CreateLayout()`の2箇所のみであることをgrepで確認済み)
3. `CLayoutMgr::DeleteLayoutAsLogical()` — 個別行削除(編集のたびの差分削除)
4. `CLayoutMgr::_Empty()` — 全行削除(全体再構築の前処理、破棄時)

この4箇所それぞれに、消去(`_TextWidthMultisetErase`)・追加
(`_TextWidthMultisetInsert`)のフックを追加した。幅が変わる箇所(2)は「変更前の
幅で消してから、変更後の幅で追加し直す」、生成(1)は「追加のみ」、削除(3)(4)は
「消去のみ」。幅0(折り返しあり時、または未設定)のノードは集合に入れない。

```cpp
void CLayoutMgr::_TextWidthMultisetErase( CLayout* pLayout )
{
	CLayoutInt nWidth = pLayout->GetLayoutWidth();
	if( nWidth <= CLayoutInt(0) ) return;
	auto it = m_multisetTextWidth.find(std::make_pair(nWidth, pLayout));
	if( it != m_multisetTextWidth.end() )
		m_multisetTextWidth.erase(it);
}
void CLayoutMgr::_TextWidthMultisetInsert( CLayout* pLayout )
{
	CLayoutInt nWidth = pLayout->GetLayoutWidth();
	if( nWidth <= CLayoutInt(0) ) return;
	m_multisetTextWidth.insert(std::make_pair(nWidth, pLayout));
}
```

この4箇所さえ正しければ集合は編集内容に関わらず常に正確であり続けるため、
`CalculateTextWidth_Range()`から「今回の編集が最大幅行に影響したか」を判定する
分岐ロジック(`TOPK_CACHE`版で使っていた行番号ベースの無効化・シフト判定を含む)
が丸ごと不要になった。関数の中身は集合の現在の最大値をそのまま読むだけになる。

```cpp
if( m_pcEditDoc->m_nTextWrapMethodCur == WRAP_NO_TEXT_WRAP ){
	m_nTextWidth = m_multisetTextWidth.empty() ? CLayoutInt(0) : m_multisetTextWidth.rbegin()->first;
}
```

## 既存コードとの関係

- `CalculateTextWidth()`自体(外部トリガー、ファイル読み込み・設定変更等で
  `bCalLineLen=TRUE`かつ全行を指定して呼ばれるケース)の中身(各行の幅の再計算
  そのもの)は変更していない。これは編集のたびではなく低頻度にしか発生せず、
  かつ全行の実測が本質的に必要なため、今回の最適化の対象外(そもそもの狙いは
  「編集のたびの全行"最大値探し"」の排除であって、低頻度の全行再計算自体は
  排除しようがない)。この関数内の`bCalLineLen`分岐だけ、上記フック(2)を
  追加している。
- `m_nTextWidthMaxLine`(最大幅の行番号)は本方式では更新しない。外部から
  参照されているのは`GetMaxTextWidth()`(`m_nTextWidth`のみを返す)だけで、
  `m_nTextWidthMaxLine`自体はCLayoutMgr内部の従来ロジック専用の実装詳細
  (「編集が最大幅行に触れたか」の判定にのみ使われていた)であり、本方式では
  そのロジック自体を使わないため、更新しなくても外部への影響はない。
- `NKMM_FIX_TEXTWIDTH_TOPK_CACHE`は削除せず、`#else`側にそのまま残した
  (`NKMM_FIX_TEXTWIDTH_MULTISET_CACHE`を無効化すれば、TOPK方式に戻る)。

## 動作確認について

VS2022(`sakura.sln`、Debug/x64)でのビルドを実施。このリポジトリは本修正とは
無関係な既存の型変換エラー(`CStrictInteger`関連、`cmd/CViewCommander_Bookmark.cpp`
等、複数箇所)により全体ビルドが通らない既知の制約があるが、変更前後でビルド
ログのエラー一覧(31件)が完全に一致することを確認し、`CLayoutMgr.h`/
`CLayoutMgr.cpp`/`CLayoutMgr_New.cpp`/`CLayoutMgr_DoLayout.cpp`にはエラー・警告が
一切ないことを確認済み。

### 追記: 実機確認済み 20260806

報告者が実機で、最長行を含む範囲の削除・Undo/Redo等を確認。当初「Backspaceで
削除した場合にスクロールバーの長さが変化しない」という問題が見つかったが、
これは本ロジックの不具合ではなく別ファイル(`CEditView_Scroll.cpp`の
`AdjustScrollBars()`)側の不具合と判明し、`NKMM_FIX_EDITVIEW_SCRBAR_HWIDTH_SKIP.md`
で修正した。その修正後、Backspaceでのスクロールバー縮小を含めて問題なしとの
報告を受けた。空文書化・フォント変更・タブ幅変更・折り返し設定切り替え等の
個別の実機確認は、①②の確認作業の中で包括的に行われ、問題は報告されていない
(各項目を単体で切り分けての確認ではない)。
