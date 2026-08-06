# 「折り返さない」時のテキスト最大幅算出の高速化 20260806

対象フラグ: `NKMM_FIX_TEXTWIDTH_TOPK_CACHE`（新規）
対象ファイル:

- `sakura_core/doc/layout/CLayoutMgr.h`（`STextWidthCandidate`, `m_vTextWidthTopK`）
- `sakura_core/doc/layout/CLayoutMgr_New.cpp`（`CLayoutMgr::CalculateTextWidth`）
- `sakura_core/doc/layout/CLayoutMgr_DoLayout.cpp`（`CLayoutMgr::CalculateTextWidth_Range`,
  `CLayoutMgr::_UpdateTextWidthTopKForEdit`）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

`NKMM_FIX_STATUSBAR_WORDNUM_CACHE`の作業の延長で、「Undoバッファを使ってもっと
速くできる処理はないか」という話の中で挙がった箇所。`CalculateTextWidth`系は
Undoバッファの差分方式(挿入文字数-削除文字数の加算)がそのままでは使えない
(最大幅は非加法的な集約値のため)ことを確認した上で、代わりの高速化案として
「次点候補リスト」方式を採用した。

## 原因

`CLayoutMgr::CalculateTextWidth()`(`CLayoutMgr_New.cpp:287`)は、「折り返さない」
設定時にスクロールバーの水平方向の範囲を決めるため、文書全体の最大幅行を
`m_nTextWidth`/`m_nTextWidthMaxLine`として記憶している。各`CLayout`ノードは
自身の幅を`SetLayoutWidth()`で常に最新に保っている(`CLayoutMgr.cpp:422`、
レイアウト構築のたびに更新される)ため、幅の再計算自体は不要だが、**「今の
最大幅行がどれか」という1件だけの記憶**しか持っていない。

`CalculateTextWidth_Range()`(`CLayoutMgr_DoLayout.cpp:581`、編集のたびに呼ばれる)
は、現在の最大幅行(`m_nTextWidthMaxLine`)自体が今回の編集で変更・削除された
場合、次に幅が大きい行が分からないため、`CalculateTextWidth(FALSE, -1, -1)`で
**全レイアウト行を走査**して最大幅行を探し直していた。

```cpp
if(( pctwArg->nDelLines < CLayoutInt(0) && ... && m_nTextWidthMaxLine == pctwArg->ptLayout.y )||
   ( pctwArg->nDelLines >= CLayoutInt(0) && ... ))
{
	// 全ラインを走査する
	nCalTextWidthLinesFrom = -1;
	nCalTextWidthLinesTo   = -1;
}
```

折り返しなし設定で、文書中最も長い行を繰り返し編集するような使い方(巨大な
1行のログ・ミニファイされたコード等)では、編集のたびにこの全行スキャンが
走ることになる。

## 対応

最大幅行1件だけでなく、幅の降順で上位`TEXTWIDTH_TOPK_MAX`(8)件を候補リスト
`m_vTextWidthTopK`として保持するようにした。

- `CalculateTextWidth()`が全行走査するとき(`bOnlyExpansion==FALSE`)、その
  走査と同時に候補リストも構築し直す(挿入ソート、O(n log K)、Kは固定8)。
- `CalculateTextWidth_Range()`は、既存の「最大幅行が編集範囲に含まれるか」の
  判定(スカラー値`m_nTextWidthMaxLine`に対して行っている)と**全く同じ基準**を
  候補リストの各エントリにも適用する`_UpdateTextWidthTopKForEdit()`を新設し、
  分岐の前に必ず呼ぶ。編集範囲に含まれる候補は無効化(リストから除外)、
  編集範囲より後ろの候補は行番号をシフトする(挿入・削除による行番号のズレに
  追従)。

```cpp
void CLayoutMgr::_UpdateTextWidthTopKForEdit( const CalTextWidthArg* pctwArg, CLayoutInt nInsLineNum )
{
	for( const auto& cand : m_vTextWidthTopK ){
		bool bInvalidated = /* 既存のm_nTextWidthMaxLine無効化条件と同じ基準 */;
		if( bInvalidated ) continue;	// 除外
		if( Int(nInsLineNum) && cand.nLayoutY >= pctwArg->ptLayout.y )
			shifted.nLayoutY += nInsLineNum;	// シフト
		vNew.push_back(shifted);
	}
	m_vTextWidthTopK.swap(vNew);
}
```

最大幅行が編集で無効になった際は、まずこの(追従済みの)候補リストに次点が
残っていないか確認し、残っていればそれを新しい最大幅としてそのまま採用して
全行スキャンをスキップする。候補が尽きたとき(候補リストの全件が同じ編集で
無効化された場合。かなり稀)だけ、従来通り全行スキャンにフォールバックし、
その結果で候補リストを作り直す。

```cpp
if( !m_vTextWidthTopK.empty() ){
	m_nTextWidth = m_vTextWidthTopK.front().nWidth;
	m_nTextWidthMaxLine = m_vTextWidthTopK.front().nLayoutY;
	bCalTextWidth = FALSE;	// 全行スキャン不要
}else{
	// 従来通り全行スキャン
}
```

## 既知の制約

候補リストの無効化条件は、既存の`m_nTextWidthMaxLine`無効化条件をそのまま
候補ごとに一般化したものであり、この既存条件自体が持つ既知の盲点(行数が
変わらない1行内編集で、その行が候補だった場合に幅の縮小を検知できず、
古い(実際より大きい)幅を保持し続けることがある)は今回の変更でも解消して
いない。これは`m_nTextWidthMaxLine`自体にも元々あった挙動であり、今回の
変更で悪化させたものではない(全行スキャンへのフォールバックが常に効くため、
表示が誤った幅のまま固定されることはない)。

`CalculateTextWidth()`には「アプリケーションの最大幅に達したら走査を打ち切る」
早期終了がある(`bCalLineLen==FALSE`時)。全行走査の途中でこれが働くと候補
リストが8件未満で打ち切られることがあるが、これは「候補が少し早く尽きて
全行スキャンにフォールバックする頻度がわずかに上がる」だけで、正しさには
影響しない。

## 動作確認について

VS2022(`sakura.sln`、Debug/x64)でのビルドを実施。このリポジトリは本修正とは
無関係な既存の型変換エラー(`CStrictInteger`関連、`cmd/CViewCommander_Bookmark.cpp`
等、複数箇所)により全体ビルドが通らない既知の制約があるが、変更前後でビルド
ログのエラー一覧(31件)が完全に一致することを確認し、`CLayoutMgr.h`/
`CLayoutMgr_New.cpp`/`CLayoutMgr_DoLayout.cpp`にはエラー・警告が一切ないことを
確認済み。実機での速度改善確認、および大量の挿入・削除を繰り返した際の候補
リストの整合性(行番号シフトの正しさ)の実機確認は未実施。

### 追記 20260806

`my_config.h`では現在`NKMM_FIX_TEXTWIDTH_MULTISET_CACHE`が有効なため、実機での
最長幅キャッシュの動作確認(スクロールバーの伸縮、Undo/Redo等)は実際には
MULTISET_CACHE側のロジックで行われている(そちらの実機確認については
`NKMM_FIX_TEXTWIDTH_MULTISET_CACHE.md`を参照)。本ファイル(TOPK方式)の
`#else`側ロジック自体は、この実機確認の対象になっていない。
