# 「折り返さない」時、テキスト幅が縮んでも水平スクロールバーが更新されない不具合の修正 20260806

対象フラグ: `NKMM_FIX_EDITVIEW_SCRBAR`(既存)配下、`NKMM_EDITVIEW_H_SCRBAR_REDRAW_TIMING`の
実装バグ修正。新規フラグは追加していない。

対象ファイル:

- `sakura_core/view/CEditView_Scroll.cpp`(`CEditView::AdjustScrollBars()`)
- `sakura_core/my_config.h`(既存ブロックへの追記)

---

## 背景

`NKMM_FIX_TEXTWIDTH_TOPK_CACHE`/`NKMM_FIX_TEXTWIDTH_MULTISET_CACHE`(テキスト最大幅
算出の高速化)の実機確認をユーザーに依頼したところ、「Backspaceで削除した場合に
スクロールバーの長さが変化しない、ほかは問題ない」という報告を受けた。

## 原因

`CEditView::AdjustScrollBars()`の水平スクロールバー更新部分
(`NKMM_EDITVIEW_H_SCRBAR_REDRAW_TIMING`、冗長な`SetScrollInfo`呼び出しを避ける
最適化)は、以下の条件をすべて満たすときだけ`SetScrollInfo`を呼ばずスキップする。

```cpp
if (!bEnable ||
    (nMaxLineKetas_ == m_pcEditDoc->m_cLayoutMgr.GetMaxLineKetas() &&
     si.nPage == (Int)GetTextArea().m_nViewColNum &&
     si.nPos == (Int)GetTextArea().GetViewLeftCol())
) {
	// 更新しない
}
```

`nMaxLineKetas_`は「前回更新時の折り返し桁数」(`CEditView.h`、`折り返し桁数の
設定値`)であり、**「折り返さない」時にスクロールバーの右端が実際に依存する
`GetRightEdgeForScrollBar()`→`GetMaxTextWidth()`(テキストの実測最大幅)とは
無関係な値**。折り返し桁数の設定自体は編集では変化しないため、この判定は
「折り返さない」モードでは事実上常に真になり得る。

- テキストを右方向へ伸ばす編集(通常のタイプ入力)は、多くの場合カーソルが
  表示域の右端を超えて水平方向に自動スクロールし、`GetViewLeftCol()`
  (`si.nPos`)が変化するため、たまたまスキップ条件を満たさず更新される。
- Backspaceでテキスト幅を縮める編集は、カーソルが表示域内で左へ戻るだけで
  自動スクロールが発生せず、`si.nPos`/`si.nPage`/`nMaxLineKetas_`のいずれも
  変化しないため、**スキップ条件を満たしてしまい`SetScrollInfo`が呼ばれず、
  スクロールバーの範囲が古い(広い)ままになる**。

これは`NKMM_FIX_TEXTWIDTH_TOPK_CACHE`/`MULTISET_CACHE`(今回のセッションで追加)
とは独立した、`NKMM_EDITVIEW_H_SCRBAR_REDRAW_TIMING`(既存)側の実装バグであり、
`CEditView_Scroll.cpp`は今回のテキスト幅キャッシュ作業では一度も変更していない。
テキスト幅算出自体は正しく更新されていたが、その変化をスクロールバーへ反映する
判定側が見落としていた。

## 対応

スキップ判定に、既に`::GetScrollInfo`で取得済みの現在の`si.nMax`と、今回の
`GetRightEdgeForScrollBar()`(が依存する`GetMaxTextWidth()`)の比較を追加した。
新しい変数は不要(`si`は直前の`::GetScrollInfo`呼び出しで最新のスクロールバー
状態を保持している)。

```cpp
if (!bEnable ||
    (nMaxLineKetas_ == m_pcEditDoc->m_cLayoutMgr.GetMaxLineKetas() &&
     si.nMax == (Int)GetRightEdgeForScrollBar() - 1 &&	// 追加
     si.nPage == (Int)GetTextArea().m_nViewColNum &&
     si.nPos == (Int)GetTextArea().GetViewLeftCol())
) {
	// 更新しない
}
```

これにより、テキスト幅が伸びても縮んでも(かつ折り返し桁数・表示域桁数・
左端桁が変化しない場合でも)、実際のスクロールバー範囲と計算結果が異なれば
必ず`SetScrollInfo`が呼ばれるようになる。

## 動作確認について

VS2022(`sakura.sln`、Debug/x64)でのビルドを実施。このリポジトリは本修正とは
無関係な既存の型変換エラー(`CStrictInteger`関連、`cmd/CViewCommander_Bookmark.cpp`
等、複数箇所)により全体ビルドが通らない既知の制約があるが、変更前後でビルドログの
エラー一覧(31件)が完全に一致することを確認し、`CEditView_Scroll.cpp`には
エラー・警告が一切ないことを確認済み。

### 追記: 実機確認済み 20260806

報告者が実機でBackspace削除時にスクロールバーが正しく縮むことを確認し、
問題なしとの報告を受けた。
