# NKMM_FIX_GLYPH_ATLAS_CACHE 設計

対象フラグ: `NKMM_FIX_GLYPH_ATLAS_CACHE`(新規)
対象ファイル(主なもの):

- `sakura_core/my_config.h`
- `sakura_core/view/CViewFont.h` / `.cpp`
- `sakura_core/view/CGlyphAtlasCache.h` / `.cpp`(新規)
- `sakura_core/view/CTextDrawer.cpp`
- `sakura_core/env/CommonSetting.h` / `CShareData.cpp` / `CShareData_IO.cpp`
- `sakura_core/prop/CPropComGeneral.cpp`
- `sakura_core/sakura_rc.rc` / `sakura_rc.h` / `sakura.hh`
- `sakura_core/doc/CEditDoc.cpp`

---

## 背景

現在のサクラエディタは、文字を1文字(サロゲートペアなら2コードユニット)描画するたびに GDI の `ExtTextOutW`(`CTextDrawer::DispText`、`sakura_core/view/CTextDrawer.cpp:52-161`)を呼んでおり、フォントのラスタライズ結果はキャッシュされていない。同じ文字・同じ色の組み合わせが画面内で何度も再描画される(スクロール・再描画のたびに毎回)ため、ここをキャッシュすれば描画負荷を下げられる。

調査の結果、`DispText`(唯一の文字グリフ描画関数)は**常に背景色込みで不透明描画(ETO_OPAQUE)**しており、選択範囲のハイライトも文字描画とは別経路(`DispTextSelected`、`CEditView_Paint.cpp:1354-1441`、EXOR反転・行単位)で処理されていることが判明した。つまり ClearType のサブピクセルレンダリング結果は「(フォント, 文字, 前景色, 背景色)」の組み合わせだけで完全に決まる。したがって、この組み合わせをキーにした**色付きビットマップそのものをキャッシュする方式**を採れば、ClearType の見た目を一切損なうことなく高速化できる(検討段階で候補に上がった「グレースケールAA(`GGO_GRAY8_BITMAP`)によるアルファ合成キャッシュへの妥協」は、本エディタの描画方式では不要と判明した)。

実際に同時使用される色の組み合わせは、`sakura_core/view/colors/EColorIndexType.h`の`enum EColorIndexType`で定義される約60種(テキスト、コメント、キーワード1-10、正規表現1-10、diff等)に収まるため、キャッシュのキー空間(フォントバリアント4種 × 色数十種 × 文字種)も現実的な大きさに収まる。

ユーザーの要望により、設定でON/OFFを切り替え可能にする(安全弁として)。

---

## 採用方式

**「(フォント, 文字, 前景色, 背景色)をキーにした完全色付きビットマップキャッシュ」**。複数グリフを1枚の大きなビットマップ(1024×1024、最大8ページ)に敷き詰める「シェルフパッキング」方式のアトラスとし、GDIオブジェクト数を1グリフ1ハンドルにせず抑える。ミス時は「アトラスページ上に実際に`ExtTextOutW`で描く」→「その結果をそのまま画面へ`BitBlt`」を1回で両立させる(見た目は非キャッシュ時と完全一致)。

キャッシュ非適用(素通し)にする条件:
- 設定OFF時
- `bTransparent==true`(背景画像/壁紙機能使用時。`CEditView::IsBkBitmap()`が真の場合に発生し、壁紙を透かす描画なのでタイルで上書きできない)
- 描画文字数が3以上(現状 `CFigure_Eol.cpp:285` の `"[EOF]"` のような低頻度の複数文字描画。1文字/サロゲートペア(2)のみキャッシュ対象)
- グリフのセルが `rcClip` の範囲からはみ出す(横スクロールで部分的に切れる)場合(キャッシュに「欠けたグリフ」が登録される事故を防止)

---

## 実装ステップ

### 1. 新規マクロ `NKMM_FIX_GLYPH_ATLAS_CACHE`
`sakura_core/my_config.h` の既存機能マクロ群の末尾に追加し、この機能一式をこのマクロでガードする(既存の `NKMM_FIX_COLOR_FONT` 等と同じ慣習)。

### 2. `CViewFont` のマクロガード解除(前提作業)
`sakura_core/view/CViewFont.h:58-62`・`private:` 内 `s_nGeneration`宣言(75-77行)、および `sakura_core/view/CViewFont.cpp` の対応する定義・インクリメント箇所の `#ifdef NKMM_FIX_COLOR_FONT` を外し、常時コンパイルにする(単なる `static ULONG GetFontGeneration()` アクセサなので副作用なし)。これにより新機能が `NKMM_FIX_COLOR_FONT` に依存せず世代番号でキャッシュ無効化できる。

### 3. 新規クラス `CGlyphAtlasCache`
- `sakura_core/view/CGlyphAtlasCache.h` / `.cpp`(`CColorFontRenderer.h/.cpp` と同じ `sakura_core/view/` 直下、`TSingleton<CGlyphAtlasCache>` を継承し同じ構成パターンを踏襲)。
- キー: `{HFONT hFont, wchar_t wch0, wchar_t wch1 /*サロゲート第2要素、無ければ0*/, COLORREF crFore, COLORREF crBack}` + ハッシュ関数。
- エントリ: `{nPageIndex, RECT rcCell, nCellWidthPx, nCellHeightPx}`。
- ページ: `{HBITMAP, HDC(CreateCompatibleDC+SelectObject済み), HBITMAP hbmpOld, シェルフ管理用のnShelfX/nShelfY/nShelfHeight}`。`CreateCompatibleBitmap`/`CreateCompatibleDC`を使用(生ピクセルアクセス不要なのでDIBSection化は不要)。
- 主要メソッド:
  - `bool DrawOrCache(HDC hdc, HFONT hFont, const wchar_t* pData, int nLength, COLORREF crFore, COLORREF crBack, int nDestX, int nDestY, int nCellWidthPx, int nCellHeightPx, const int* pDx)` — ヒット時は`BitBlt`のみ。ミス時は`AllocCell`でシェルフパッキングし、ページDC上に`SetTextColor`/`SetBkColor`/`SelectObject(hFont)`してから`ExtTextOutW_AnyBuild(..., ETO_CLIPPED|ETO_OPAQUE, ...)`で1回描画・登録し、その領域を画面へ`BitBlt`。ページ確保に失敗したら`false`を返し、呼び出し側に通常描画させる(既存ヒットは影響を受けない)。
  - `void SetEnabled(bool)` / `bool IsEnabled() const`
  - `void Clear()` — 全ページ解放(`DeleteObject`/`DeleteDC`)、マップクリア。デストラクタからも呼ぶ。
  - `void ClearIfStale()` — `CViewFont::GetFontGeneration()`の変化を見て自動的に`Clear()`(`CColorFontRenderer::ClearFontFaceCacheIfStale()`と同一パターン)。
- 上限: `MAX_PAGES = 8`(1024×1024×32bpp ≒ 4MB/ページ、最大32MB)。到達後は新規登録を諦めて素通しにフォールバックするだけで、既存キャッシュの読み取りやアプリの動作自体は継続する。LRU等の追い出しは実装しない(MVP)。

### 4. `CTextDrawer::DispText()` への統合
`sakura_core/view/CTextDrawer.cpp:142-155`、既存の `::ExtTextOutW_AnyBuild(...)` 呼び出し直前に分岐を追加する(`#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE`でガード)。

- `nDrawLength`が1〜2、`!bTransparent`、かつグリフのセル矩形(`nDrawX`〜`nDrawX+nCellWidth`)が`rcClip`に完全に収まる場合のみキャッシュ経路を試す。
- 現在のHDC状態(`CGraphics::PushTextForeColor/PushTextBackColor/PushMyFont`で呼び出し前に設定済み)を`::GetTextColor(hdc)`/`::GetBkColor(hdc)`/`(HFONT)::GetCurrentObject(hdc, OBJ_FONT)`で読み戻し、`CGlyphAtlasCache::getInstance()->DrawOrCache(...)`に渡す。
- `true`が返れば描画済みなので`ExtTextOutW_AnyBuild`はスキップ、`false`ならそのまま既存コードで直接描画(フォールバック)。
- `#include "CGlyphAtlasCache.h"`を追加。

### 5. 設定項目の追加
- **`sakura_core/env/CommonSetting.h`**: `CommonSetting_Window`構造体の`m_bUseCompatibleBMP`(116行)の直後に`BOOL m_bUseGlyphAtlasCache;`を追加(同じ「描画キャッシュ系」設定としてまとめる)。
- **`sakura_core/sakura_rc.rc`**: `IDD_PROP_GENERAL`ダイアログ(1546-1629行)内、既存のコメントアウト済み`IDC_CHECK_MEMDC`(1609-1610行、「画面キャッシュを使う」)の直後に新しい`GROUPBOX "描画"`とその中に`CONTROL "グリフキャッシュを使う(&G)" IDC_CHECK_GLYPHATLASCACHE`を追加。「履歴」グループボックス(1611行〜、y=152開始)と重ならない`y=131〜149`の帯に配置する。
- **`sakura_core/sakura_rc.h`**: `IDC_CHECK_GLYPHATLASCACHE`を未使用の次のID(既存の`IDC_CHECK_USETYPECOLOR`(1728)・`IDC_GROUP_COLORLIST`(1729)の続き、1730)で追加し、`_APS_NEXT_CONTROL_VALUE`を更新。
- **`sakura_core/sakura.hh`**: `HIDC_CHECK_MEMDC`(11752)近辺に`HIDC_CHECK_GLYPHATLASCACHE`をヘルプID追加(実装時に未使用番号を再確認)。
- **`sakura_core/prop/CPropComGeneral.cpp`**: `p_helpids[]`配列、`CPropGeneral::SetData()`(370行付近、`CheckDlgButton`)、`CPropGeneral::GetData()`(450行付近、`IsDlgButtonChecked`)にそれぞれ追加(既存の`m_bMenuIcon`/`CPropComWin.cpp`パターンを踏襲)。
- **`sakura_core/env/CShareData_IO.cpp`**: `cProfile.IOProfileData(pszSecName, LTEXT("bUseGlyphAtlasCache"), common.m_sWindow.m_bUseGlyphAtlasCache);`を追加(ini読み書き)。
- **`sakura_core/env/CShareData.cpp`**: デフォルト値を設定(305行付近、`sWindow.m_bUseCompatibleBMP = TRUE;`の直後)。初回リリースは安全側でデフォルト**OFF**にする。

### 6. 設定変更時のキャッシュ無効化
- **フォント再生成時(主経路)**: `CEditDoc::OnChangeSetting()`(`CEditDoc.cpp:751`付近)は設定Apply時に必ず`m_pcViewFont->UpdateFont(...)`を呼び`CViewFont::s_nGeneration`をインクリメントする。`CGlyphAtlasCache::DrawOrCache()`内の`ClearIfStale()`が次回描画時にこれを検知して自動的に全ページ破棄する。追加のフック不要。
- **ON/OFF切り替えの即時反映**: `CEditDoc::OnChangeSetting()`内、上記`UpdateFont`呼び出しの直後に`CGlyphAtlasCache::getInstance()->SetEnabled(GetDllShareData().m_Common.m_sWindow.m_bUseGlyphAtlasCache != 0);`を追加。`SetEnabled(false)`内部で即座に`Clear()`し、以後`DrawOrCache`は常に`false`を返す。

---

## リスクと確認済み事項

- **印刷・印刷プレビュー**: `CPrintPreview.cpp`は`CTextDrawer::DispText`を経由しないため無影響。
- **選択範囲ハイライト**: `DispTextSelected`(`CEditView_Paint.cpp:1354-1441`)はEXOR反転による別経路であり、キャッシュ由来のBitBlt結果に対しても正しく機能する。
- **カラー絵文字(`NKMM_FIX_COLOR_FONT`)**: `CFigureStrategy.cpp`のカラーグリフキュー登録(`TryQueueColorGlyph`)は文字コード・フォントから都度判定するため、`DispText`がキャッシュ経由でも直接描画でも独立して動作する。絵文字混在行は目視確認する。
- **HFONTハンドルの再利用**: キーにHFONTを直接使うが、フォント再作成のたびに`s_nGeneration`が上がりキャッシュ全体が破棄されるため、ハンドル再利用による誤ヒットのリスクは`CColorFontRenderer`が既に許容している既存パターンと同一で新規リスクではない。
- **マルチウィンドウ**: `CGlyphAtlasCache`はプロセス単位のシングルトンだが、`CViewFont`(≒HFONT)はウィンドウ単位のため、同じフォント設定でも別ウィンドウ間ではキャッシュは共有されない(性能上のロスのみ、正しさには影響しない)。同一ウィンドウ内の分割ビューは共有される。
- **DPI変更**: 本プロジェクトにPer-Monitor DPI対応コード自体が存在しないため、既存の描画パイプラインと同等の挙動(新規リスク無し)。

---

## 検証方法

- `sakura/sakura.sln`をVisual Studioで`Debug|x64`ビルド。`.rc`変更を反映するためリソースの再コンパイルを確認。
- 共通設定(全般タブ)で新設「描画」グループの「グリフキャッシュを使う」チェックボックスが表示・トグルでき、既存の「履歴」グループと重ならないこと。
- ON/OFF双方で、通常編集・スクロール・選択範囲反転・キャレット行ハイライト・複数色のシンタックスハイライト・横スクロール時の左右端の部分クリップ・背景画像(壁紙)使用時・絵文字混在行の見た目がピクセルレベルで一致すること(スクリーンショット比較推奨)。
- フォント/配色設定変更やキャッシュ設定のOFF→ON/ON→OFF切り替えをApply/OK直後に、古いキャッシュの色・フォントが残らないこと。
- タスクマネージャ等でGDIオブジェクト数を監視し、長時間スクロールしてもページ数(最大8)で頭打ちになり無制限に増え続けないこと。
- 印刷プレビューの見た目が変わっていないことの回帰確認。
