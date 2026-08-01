/*!	@file
	@brief GDIによるグリフキャッシュ(グリフアトラス)

	(フォント,文字,前景色,背景色)をキーに、ExtTextOutの描画結果を
	ビットマップとしてキャッシュし、以後はBitBltで再利用する。

	DispTextは常にETO_OPAQUEで背景も同時描画するため、ClearTypeの
	サブピクセルレンダリング結果は(フォント,文字,fg,bg)の組だけで
	完全に決まる。そのためこの方式はClearTypeの見た目を一切損なわない。

	詳細はchangelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md、
	changelog/NKMM_FIX_GLYPH_ATLAS_CACHE_IMPL.md参照。
*/
#ifndef SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_
#define SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include <unordered_map>
#include <vector>
#include "util/design_template.h"

//! グリフアトラスキャッシュのキー: (フォント,文字,前景色,背景色,セル幅,セル高さ)の組で一意
//! セル幅・高さも含めるのは、同じ(フォント,文字,fg,bg)でも呼び出し元のDx配列(字送り)
//! 計算の都合で異なる幅を要求してくることがあるため。幅を含めないと、キャッシュヒット時に
//! 保存済みサイズでBitBltしてしまい、今回要求されたセルより小さいと右端に前フレームの
//! 内容が塗り残る(「ゴミ」)。フォントサイズを大きくすると誤差が拡大されて可視化しやすい。
//! 20260802 バグ修正でセル幅・高さをキーに追加
struct SGlyphAtlasKey {
	HFONT     hFont;
	wchar_t   wch0;      //!< 1文字目(サロゲート上位、または単一文字)
	wchar_t   wch1;      //!< サロゲート下位。単一コード単位のときは 0
	COLORREF  crFore;
	COLORREF  crBack;
	int       nCellWidthPx;
	int       nCellHeightPx;

	bool operator==(const SGlyphAtlasKey& rhs) const
	{
		return hFont == rhs.hFont && wch0 == rhs.wch0 && wch1 == rhs.wch1
			&& crFore == rhs.crFore && crBack == rhs.crBack
			&& nCellWidthPx == rhs.nCellWidthPx && nCellHeightPx == rhs.nCellHeightPx;
	}
};

//! SGlyphAtlasKey用のハッシュ関数(std::unordered_map用)
struct SGlyphAtlasKeyHash {
	size_t operator()(const SGlyphAtlasKey& k) const
	{
		size_t h = std::hash<void*>()((void*)k.hFont);
		h = h * 131 + (size_t)k.wch0;
		h = h * 131 + (size_t)k.wch1;
		h = h * 131 + (size_t)k.crFore;
		h = h * 131 + (size_t)k.crBack;
		h = h * 131 + (size_t)k.nCellWidthPx;
		h = h * 131 + (size_t)k.nCellHeightPx;
		return h;
	}
};

//! アトラスページ内の1グリフ分の格納情報
struct SGlyphAtlasEntry {
	int  nPageIndex = 0;
	RECT rcCell = {0, 0, 0, 0};	//!< ページ内座標(左上原点)
	int  nCellWidthPx = 0;
	int  nCellHeightPx = 0;
};

//! グリフアトラスの1ページ(複数グリフをシェルフパッキングで敷き詰めた1枚のビットマップ)
struct SGlyphAtlasPage {
	HBITMAP hBitmap = nullptr;
	HDC     hdcPage = nullptr;		//!< CreateCompatibleDC + SelectObject(hBitmap) 済み
	HBITMAP hbmpOld = nullptr;
	int     nShelfX = 0;			//!< 現シェルフ内の次の書き込みX
	int     nShelfY = 0;			//!< 現シェルフの開始Y
	int     nShelfHeight = 0;		//!< 現シェルフの高さ(最大セル高さ)
};

/*!	GDIによるグリフキャッシュ(グリフアトラス)のシングルトン。

	CTextDrawer::DispTextから、1文字(またはサロゲートペア)描画のたびに
	DrawOrCache()を呼び出す。ヒット時はBitBltのみで済むため、同じ
	(フォント,文字,fg,bg)の組み合わせが繰り返し描画されるスクロール・
	再描画時の負荷を下げる。

	CViewFont::GetFontGeneration()の値が変化したら(=フォントが作り直されたら)
	キャッシュ全体を破棄する(ClearIfStale)。
*/
class CGlyphAtlasCache : public TSingleton<CGlyphAtlasCache> {
	friend class TSingleton<CGlyphAtlasCache>;
	CGlyphAtlasCache();
	~CGlyphAtlasCache();

public:
	//! 設定でON/OFFする。OFFにすると即座に全ページを解放する。
	void SetEnabled(bool bEnabled);
	bool IsEnabled() const { return m_bEnabled; }

	/*!	グリフ1個(または1サロゲートペア)を描画する。

		@param hdc            [in] 描画先DC
		@param hFont          [in] 現在選択中のフォント(GetCurrentObject(hdc,OBJ_FONT)相当)
		@param pData          [in] 描画対象文字列の先頭
		@param nLength        [in] pDataの長さ(1または2のみ受け付ける)
		@param crFore         [in] 前景色(GetTextColor(hdc)相当)
		@param crBack         [in] 背景色(GetBkColor(hdc)相当)
		@param nDestX         [in] 描画先X(ExtTextOutと同じ原点)
		@param nDestY         [in] 描画先Y
		@param nCellWidthPx   [in] このグリフの描画幅(呼び出し側のDx配列合計)
		@param nCellHeightPx  [in] 行の高さ(CTextMetrics::GetHankakuDy())
		@param pDx            [in] ミス時の実描画に使うDx配列(nLength要素)

		@retval true  キャッシュ経由で処理した(呼び出し側はExtTextOutを呼ばなくてよい)
		@retval false 何もしなかった(呼び出し側が通常どおりExtTextOutすること)
	*/
	bool DrawOrCache(
		HDC hdc,
		HFONT hFont,
		const wchar_t* pData, int nLength,
		COLORREF crFore, COLORREF crBack,
		int nDestX, int nDestY,
		int nCellWidthPx, int nCellHeightPx,
		const int* pDx
	);

	//! 全ページを解放し、キャッシュを空にする。
	void Clear();

private:
	void ClearIfStale();
	bool AllocCell(int w, int h, SGlyphAtlasEntry* pOut);
	SGlyphAtlasPage* CreatePage();

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
	//! デバッグ用: 指定ページの実ピクセルをGetDIBits()で取得し、そのままbmpファイルへ書き出す
	//! (System.Drawing/GDI+等を介さない、HDCの内容そのもの)。Clear()から自動的に呼ばれる。
	void DumpPageToFile(const SGlyphAtlasPage& page, const wchar_t* pszPath) const;
#endif // NKMM_

	bool  m_bEnabled;
	ULONG m_nFontGeneration;
	bool  m_bPageAllocFailed;	//!< 一度でもページ確保に失敗したら以後は試みない
	std::vector<SGlyphAtlasPage> m_vPages;
	std::unordered_map<SGlyphAtlasKey, SGlyphAtlasEntry, SGlyphAtlasKeyHash> m_mapEntries;

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
	int m_nDumpCounter = 0;	//!< Clear()の呼び出しごとにファイル名が重複しないようにする通し番号
#endif // NKMM_

	static const int PAGE_SIZE = 1024;
	static const int MAX_PAGES = 8;
};

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE

#endif /* SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_ */
