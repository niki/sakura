# NKMM_FIX_GLYPH_ATLAS_CACHE 実装詳細(ステップ1〜6)

このファイルは `changelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md`(設計概要)の「実装ステップ」節を、実装時にそのまま参照できるようコード例付きで詳細化したもの。設計判断の根拠・背景・リスク一覧はそちらを参照。

---

## 1. 新規マクロ `NKMM_FIX_GLYPH_ATLAS_CACHE`

`sakura_core/my_config.h` 末尾(`#endif /* MY_CONFIG_H */`の直前)に追加。**実装済み**。

```cpp
#define NKMM_FIX_GLYPH_ATLAS_CACHE
```

---

## 2. `CViewFont` のマクロガード解除(前提作業)

`sakura_core/view/CViewFont.h:58-62`・`75-77`、および `sakura_core/view/CViewFont.cpp` の対応箇所にある `#ifdef NKMM_FIX_COLOR_FONT` / `#endif // NKMM_` を削除し、常時コンパイルにする。

### `CViewFont.h` 変更前
```cpp
#ifdef NKMM_FIX_COLOR_FONT
	//! フォントが作り直されるたびに増加する世代番号。
	//! HFONTから派生させたリソース(IDWriteFontFace等)のキャッシュ無効化に使う。
	static ULONG GetFontGeneration(){ return s_nGeneration; }
#endif // NKMM_

private:
	void CreateFont(const LOGFONT *plf);
	void DeleteFont();

	HFONT	m_hFont_HAN;
	HFONT	m_hFont_HAN_BOLD;
	HFONT	m_hFont_HAN_UL;
	HFONT	m_hFont_HAN_BOLD_UL;

	LOGFONT	m_LogFont;

#ifdef NKMM_FIX_COLOR_FONT
	static ULONG s_nGeneration;
#endif // NKMM_
```

### 変更後(ガードのみ削除、中身はそのまま)
```cpp
	//! フォントが作り直されるたびに増加する世代番号。
	//! HFONTから派生させたリソース(IDWriteFontFace、グリフアトラス等)のキャッシュ無効化に使う。
	static ULONG GetFontGeneration(){ return s_nGeneration; }

private:
	void CreateFont(const LOGFONT *plf);
	void DeleteFont();

	HFONT	m_hFont_HAN;
	HFONT	m_hFont_HAN_BOLD;
	HFONT	m_hFont_HAN_UL;
	HFONT	m_hFont_HAN_BOLD_UL;

	LOGFONT	m_LogFont;

	static ULONG s_nGeneration;
```

`CViewFont.cpp`側も同様に、`s_nGeneration`の定義(28-30行)と`CreateFont`/`DeleteFont`内の`++s_nGeneration;`(38-40行)を囲む`#ifdef NKMM_FIX_COLOR_FONT`/`#endif`を削除する。

既存の`CColorFontRenderer`側の`#ifdef NKMM_FIX_COLOR_FONT`ガードはそのまま残してよい(呼び出し側の変更は不要)。

---

## 3. 新規クラス `CGlyphAtlasCache`

### ファイル: `sakura_core/view/CGlyphAtlasCache.h`

```cpp
#ifndef SAKURA_CGLYPHATLASCACHE_H_
#define SAKURA_CGLYPHATLASCACHE_H_

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include "util/design_pattern.h" // TSingleton
#include <unordered_map>
#include <vector>

//! グリフアトラスキャッシュのキー: (フォント,文字,前景色,背景色)の組で一意
struct SGlyphAtlasKey {
	HFONT     hFont;
	wchar_t   wch0;      // 1文字目(サロゲート上位 or 単一文字)
	wchar_t   wch1;      // サロゲート下位。単一コード単位のときは 0
	COLORREF  crFore;
	COLORREF  crBack;

	bool operator==(const SGlyphAtlasKey& rhs) const {
		return hFont == rhs.hFont && wch0 == rhs.wch0 && wch1 == rhs.wch1
			&& crFore == rhs.crFore && crBack == rhs.crBack;
	}
};

struct SGlyphAtlasKeyHash {
	size_t operator()(const SGlyphAtlasKey& k) const {
		size_t h = std::hash<void*>()((void*)k.hFont);
		h = h * 131 + k.wch0;
		h = h * 131 + k.wch1;
		h = h * 131 + k.crFore;
		h = h * 131 + k.crBack;
		return h;
	}
};

struct SGlyphAtlasEntry {
	int  nPageIndex;
	RECT rcCell;         // ページ内座標(左上原点)
	int  nCellWidthPx;
	int  nCellHeightPx;
};

struct SGlyphAtlasPage {
	HBITMAP hBitmap   = nullptr;
	HDC     hdcPage   = nullptr;  // CreateCompatibleDC + SelectObject(hBitmap) 済み
	HBITMAP hbmpOld   = nullptr;
	int     nShelfX      = 0;     // 現シェルフ内の次の書き込みX
	int     nShelfY      = 0;     // 現シェルフの開始Y
	int     nShelfHeight = 0;     // 現シェルフの高さ(最大セル高さ)
};

//! (フォント,文字,前景色,背景色)をキーに、ExtTextOutの描画結果をビットマップとして
//! キャッシュし、以後はBitBltで再利用するグリフアトラス。
//! ClearTypeの見た目はDispTextが常にETO_OPAQUEで背景も同時描画するため
//! (フォント,文字,fg,bg)の組だけで完全に決まり、キャッシュしても劣化しない。
class CGlyphAtlasCache : public TSingleton<CGlyphAtlasCache> {
	friend class TSingleton<CGlyphAtlasCache>;
	CGlyphAtlasCache();
	~CGlyphAtlasCache();

public:
	void SetEnabled(bool bEnabled);
	bool IsEnabled() const { return m_bEnabled; }

	//! グリフ1個(または1サロゲートペア)を描画する。
	//! @retval true  キャッシュ経由で処理した(呼び出し側はExtTextOutを呼ばなくてよい)
	//! @retval false 何もしなかった(呼び出し側が通常どおりExtTextOutすること)
	bool DrawOrCache(
		HDC hdc,
		HFONT hFont,
		const wchar_t* pData, int nLength,      // 1 or 2 のみ受け付ける
		COLORREF crFore, COLORREF crBack,
		int nDestX, int nDestY,                  // ExtTextOutと同じ描画原点
		int nCellWidthPx, int nCellHeightPx,     // pDxArrayから求めた実セル幅・GetHankakuDy()
		const int* pDx                            // ミス時、実描画に使うDx配列(1〜2要素)
	);

	void Clear();

private:
	void ClearIfStale();
	bool AllocCell(int w, int h, SGlyphAtlasEntry* pOut);
	SGlyphAtlasPage* CreatePage();

	bool  m_bEnabled;
	ULONG m_nFontGeneration;
	bool  m_bPageAllocFailed;
	std::vector<SGlyphAtlasPage> m_vPages;
	std::unordered_map<SGlyphAtlasKey, SGlyphAtlasEntry, SGlyphAtlasKeyHash> m_mapEntries;

	static const int PAGE_SIZE = 1024;
	static const int MAX_PAGES = 8;
};

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE

#endif /* SAKURA_CGLYPHATLASCACHE_H_ */
```

### ファイル: `sakura_core/view/CGlyphAtlasCache.cpp`

```cpp
#include "StdAfx.h"
#include "CGlyphAtlasCache.h"

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include "CViewFont.h"
#include "CommonSetting.h" // ExtTextOutW_AnyBuild 等

CGlyphAtlasCache::CGlyphAtlasCache()
	: m_bEnabled(false)
	, m_nFontGeneration(CViewFont::GetFontGeneration())
	, m_bPageAllocFailed(false)
{
}

CGlyphAtlasCache::~CGlyphAtlasCache()
{
	Clear();
}

void CGlyphAtlasCache::SetEnabled(bool bEnabled)
{
	m_bEnabled = bEnabled;
	if( !bEnabled ){
		Clear();
	}
}

void CGlyphAtlasCache::ClearIfStale()
{
	ULONG cur = CViewFont::GetFontGeneration();
	if( cur == m_nFontGeneration ) return;
	m_nFontGeneration = cur;
	Clear();
}

void CGlyphAtlasCache::Clear()
{
	for( auto& page : m_vPages ){
		::SelectObject(page.hdcPage, page.hbmpOld);
		::DeleteObject(page.hBitmap);
		::DeleteDC(page.hdcPage);
	}
	m_vPages.clear();
	m_mapEntries.clear();
	m_bPageAllocFailed = false;
}

SGlyphAtlasPage* CGlyphAtlasCache::CreatePage()
{
	if( m_bPageAllocFailed ) return nullptr;
	if( (int)m_vPages.size() >= MAX_PAGES ) return nullptr;

	HDC hdcScreen = ::GetDC(NULL);
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, PAGE_SIZE, PAGE_SIZE);
	HDC hdcPage = ::CreateCompatibleDC(hdcScreen);
	::ReleaseDC(NULL, hdcScreen);

	if( !hBitmap || !hdcPage ){
		if( hBitmap ) ::DeleteObject(hBitmap);
		if( hdcPage ) ::DeleteDC(hdcPage);
		m_bPageAllocFailed = true;
		return nullptr;
	}

	SGlyphAtlasPage page;
	page.hBitmap = hBitmap;
	page.hdcPage = hdcPage;
	page.hbmpOld = (HBITMAP)::SelectObject(hdcPage, hBitmap);
	::SetBkMode(hdcPage, OPAQUE);

	m_vPages.push_back(page);
	return &m_vPages.back();
}

bool CGlyphAtlasCache::AllocCell(int w, int h, SGlyphAtlasEntry* pOut)
{
	// 既存ページのシェルフに入るか試す
	for( int i = 0; i < (int)m_vPages.size(); ++i ){
		SGlyphAtlasPage& page = m_vPages[i];
		if( page.nShelfX + w <= PAGE_SIZE && h <= page.nShelfHeight ){
			pOut->nPageIndex = i;
			pOut->rcCell = { page.nShelfX, page.nShelfY, page.nShelfX + w, page.nShelfY + page.nShelfHeight };
			page.nShelfX += w;
			return true;
		}
		if( w <= PAGE_SIZE && page.nShelfY + t_max(page.nShelfHeight, h) <= PAGE_SIZE ){
			// 新しい棚を開始
			page.nShelfY += page.nShelfHeight;
			page.nShelfHeight = h;
			page.nShelfX = 0;
			pOut->nPageIndex = i;
			pOut->rcCell = { 0, page.nShelfY, w, page.nShelfY + h };
			page.nShelfX = w;
			return true;
		}
	}
	// 新規ページ
	SGlyphAtlasPage* pNew = CreatePage();
	if( !pNew ) return false;
	int idx = (int)m_vPages.size() - 1;
	pNew->nShelfHeight = h;
	pNew->nShelfX = w;
	pNew->nShelfY = 0;
	pOut->nPageIndex = idx;
	pOut->rcCell = { 0, 0, w, h };
	return true;
}

bool CGlyphAtlasCache::DrawOrCache(
	HDC hdc, HFONT hFont, const wchar_t* pData, int nLength,
	COLORREF crFore, COLORREF crBack,
	int nDestX, int nDestY, int nCellWidthPx, int nCellHeightPx, const int* pDx)
{
	if( !m_bEnabled ) return false;
	ClearIfStale();

	SGlyphAtlasKey key{ hFont, pData[0], (nLength == 2 ? pData[1] : L'\0'), crFore, crBack };
	auto it = m_mapEntries.find(key);
	if( it != m_mapEntries.end() ){
		const SGlyphAtlasEntry& e = it->second;
		SGlyphAtlasPage& page = m_vPages[e.nPageIndex];
		::BitBlt(hdc, nDestX, nDestY, e.nCellWidthPx, e.nCellHeightPx,
			page.hdcPage, e.rcCell.left, e.rcCell.top, SRCCOPY);
		return true;
	}

	SGlyphAtlasEntry newEntry;
	if( !AllocCell(nCellWidthPx, nCellHeightPx, &newEntry) ){
		return false; // ページ上限 or 確保失敗。今回は直接描画にフォールバック
	}
	SGlyphAtlasPage& page = m_vPages[newEntry.nPageIndex];

	::SetTextColor(page.hdcPage, crFore);
	::SetBkColor(page.hdcPage, crBack);
	HFONT hOldFont = (HFONT)::SelectObject(page.hdcPage, hFont);
	RECT rcCellDest = newEntry.rcCell;
	::ExtTextOutW_AnyBuild(page.hdcPage, rcCellDest.left, rcCellDest.top,
		ETO_CLIPPED | ETO_OPAQUE, &rcCellDest, pData, nLength, pDx);
	::SelectObject(page.hdcPage, hOldFont);

	newEntry.nCellWidthPx  = nCellWidthPx;
	newEntry.nCellHeightPx = nCellHeightPx;
	m_mapEntries.emplace(key, newEntry);

	::BitBlt(hdc, nDestX, nDestY, nCellWidthPx, nCellHeightPx,
		page.hdcPage, rcCellDest.left, rcCellDest.top, SRCCOPY);
	return true;
}

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE
```

> 実装時の注意: `AllocCell`のシェルフ管理は簡略版。「幅が入らないが新しい棚は開始できる」ケースの座標計算は実装時に単体テスト(小さいPAGE_SIZEでの手計算)で再確認すること。既存ページの走査は`m_vPages.size()`が最大8と小さいため線形探索で問題ない。

---

## 4. `CTextDrawer::DispText()` への統合

`sakura_core/view/CTextDrawer.cpp:142-155` の既存 `::ExtTextOutW_AnyBuild(...)` 呼び出し直前に分岐を追加する。

### 変更前(142-155行)
```cpp
		//描画
		::ExtTextOutW_AnyBuild(
			hdc,
			nDrawX,					//X
#ifdef NKMM_LINE_MARGIN_TOP
			m_pEditView->GetLineMargin() +
#endif // NKMM_
			y + marginy,			//Y
			ExtTextOutOption() & ~(bTransparent? ETO_OPAQUE: 0),
			&rcClip,
			pDrawData,				//文字列
			nDrawLength,			//文字列長
			pDrawDxArray			//文字間隔の入った配列
		);
```

### 変更後
```cpp
		//描画
		int nDrawY =
#ifdef NKMM_LINE_MARGIN_TOP
			m_pEditView->GetLineMargin() +
#endif // NKMM_
			y + marginy;

		bool bCached = false;
#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE
		if( !bTransparent && nDrawLength >= 1 && nDrawLength <= 2 ){
			int nCellWidth = 0;
			for( int i = 0; i < nDrawLength; ++i ) nCellWidth += pDrawDxArray[i];
			bool bFullyVisible =
				nDrawX >= rcClip.left && (nDrawX + nCellWidth) <= rcClip.right;
			if( bFullyVisible ){
				CGlyphAtlasCache* pAtlas = CGlyphAtlasCache::getInstance();
				if( pAtlas->IsEnabled() ){
					HFONT hFont = (HFONT)::GetCurrentObject(hdc, OBJ_FONT);
					COLORREF crFore = ::GetTextColor(hdc);
					COLORREF crBack = ::GetBkColor(hdc);
					bCached = pAtlas->DrawOrCache(
						hdc, hFont, pDrawData, nDrawLength, crFore, crBack,
						nDrawX, nDrawY, nCellWidth, pMetrics->GetHankakuDy(), pDrawDxArray
					);
				}
			}
		}
#endif // NKMM_FIX_GLYPH_ATLAS_CACHE

		if( !bCached ){
			::ExtTextOutW_AnyBuild(
				hdc,
				nDrawX,
				nDrawY,
				ExtTextOutOption() & ~(bTransparent? ETO_OPAQUE: 0),
				&rcClip,
				pDrawData,
				nDrawLength,
				pDrawDxArray
			);
		}
```

`CTextDrawer.cpp`先頭のinclude群(`#include "CViewFont.h"`付近)に`#include "CGlyphAtlasCache.h"`を追加する。

---

## 5. 設定項目の追加

### `sakura_core/env/CommonSetting.h`
`CommonSetting_Window`構造体、`m_bUseCompatibleBMP`(116行)の直後:
```cpp
	BOOL			m_bUseCompatibleBMP;		//!< 再作画用互換ビットマップを使う 2007.09.09 Moca
	BOOL			m_bUseGlyphAtlasCache;		//!< グリフキャッシュ(グリフアトラス)を使う 20260801
```

### `sakura_core/sakura_rc.rc`
`IDD_PROP_GENERAL`(1546-1629行)、既存のコメントアウト済み`IDC_CHECK_MEMDC`(1609-1610行)の直後、「履歴」グループボックス(1611行、`y=152`開始)より上の`y=131〜149`帯に追加:
```rc
    GROUPBOX        "描画",IDC_STATIC,158,131,130,18,WS_GROUP
    CONTROL         "グリフキャッシュを使う(&G)",IDC_CHECK_GLYPHATLASCACHE,"Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP,163,142,120,10
```
実装時に同一ダイアログ内で`&G`のニーモニックが重複していないか確認すること。

### `sakura_core/sakura_rc.h`
既存の`IDC_CHECK_USETYPECOLOR`(1728)・`IDC_GROUP_COLORLIST`(1729)に続けて追加し、`_APS_NEXT_CONTROL_VALUE`を更新:
```c
#define IDC_CHECK_GLYPHATLASCACHE       1730
...
#define _APS_NEXT_CONTROL_VALUE         1731
```

### `sakura_core/sakura.hh`
`HIDC_CHECK_MEMDC`(11752)近辺に追加(実装時に未使用番号を再確認):
```c
#define HIDC_CHECK_GLYPHATLASCACHE   11753  //グリフキャッシュを使う
```

### `sakura_core/prop/CPropComGeneral.cpp`
`p_helpids[]`配列に追加:
```cpp
	IDC_CHECK_GLYPHATLASCACHE,		HIDC_CHECK_GLYPHATLASCACHE,		//グリフキャッシュを使う
```
`CPropGeneral::SetData()`(370行付近、旧`IDC_CHECK_MEMDC`コメントアウト行の直後):
```cpp
	::CheckDlgButton( hwndDlg, IDC_CHECK_GLYPHATLASCACHE, m_Common.m_sWindow.m_bUseGlyphAtlasCache );
```
`CPropGeneral::GetData()`(450行付近、同様の位置):
```cpp
	m_Common.m_sWindow.m_bUseGlyphAtlasCache = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_GLYPHATLASCACHE );
```

### `sakura_core/env/CShareData_IO.cpp`
旧`bUseCompotibleBMP`のコメントアウト行(2002行)の直後:
```cpp
	cProfile.IOProfileData( pszSecName, LTEXT("bUseGlyphAtlasCache")	, common.m_sWindow.m_bUseGlyphAtlasCache );
```

### `sakura_core/env/CShareData.cpp`
`sWindow.m_bUseCompatibleBMP = TRUE;`(305行)の直後、**デフォルトTRUE**(安全確認済みのため 20260805):
```cpp
	sWindow.m_bUseGlyphAtlasCache = TRUE;		// 20260801 グリフキャッシュ(グリフアトラス)を使う // 20260805 安全確認済みのためデフォルトTRUEに変更
```

---

## 6. 設定変更時のキャッシュ無効化

`sakura_core/doc/CEditDoc.cpp`、`OnChangeSetting()`(751行付近)の`m_pcViewFont->UpdateFont(...)`呼び出しの直後に追加:
```cpp
	m_pcViewFont->UpdateFont( ... ); // 既存

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE
	CGlyphAtlasCache::getInstance()->SetEnabled(
		GetDllShareData().m_Common.m_sWindow.m_bUseGlyphAtlasCache != 0 );
#endif // NKMM_
```

- フォント再生成による自動無効化(`ClearIfStale`)は`UpdateFont`が呼ばれるたびに`s_nGeneration`が増えることに相乗りするため、追加のフックは不要。
- `SetEnabled(false)`は内部で即座に`Clear()`するため、OFF直後にGDIメモリが解放される。
