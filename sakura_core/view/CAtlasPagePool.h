/*!	@file
	@brief GDI互換ビットマップページのシェルフパッキングプール

	CGlyphAtlasCache(通常テキストのグリフキャッシュ)が内部に持っていた
	「固定サイズの正方形ページを何枚か持ち、シェルフパッキングでセルの
	置き場所を配る」というページ管理ロジックを抽出した共通部品。セルの
	中身をどう描くかには一切関与しない(呼び出し側がGetPageDC()で取得した
	HDCへ自分で描く)。

	抽出時点ではCColorFontRenderer(カラーグリフ=絵文字のオーバーレイ描画)
	もキーの構造やセルの焼き方(GDIのExtTextOutではなくDirect2D+BitBlt)こそ
	違え同種のページ管理を持つ想定で「共通化できる部品」として設計したが、
	CColorFontRendererは現状これを一切参照していない — グリフ単位のビットマップ
	キャッシュ自体を持たず、毎フレームDirect2Dへ描き直す方式のまま
	(詳細はdocs/color_font_emoji_design.html参照)。実際に使っているのは
	CGlyphAtlasCacheのみ 20260831。
*/
#ifndef SAKURA_CATLASPAGEPOOL_7C9E9B3F_5B7B_4C9E_8B9B_3F7C9E9B3F5B_H_
#define SAKURA_CATLASPAGEPOOL_7C9E9B3F_5B7B_4C9E_8B9B_3F7C9E9B3F5B_H_

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include <Windows.h>
#include <vector>

//! AllocCell()の結果(ページ番号+ページ内座標)
struct SAtlasCellRect {
	int  nPageIndex = 0;
	RECT rcCell = {0, 0, 0, 0};	//!< ページ内座標(左上原点)
};

class CAtlasPagePool {
public:
	//! @param nPageSize 1ページの一辺のピクセル数(正方形)
	//! @param nMaxPages 確保するページ数の上限
	CAtlasPagePool(int nPageSize, int nMaxPages);
	~CAtlasPagePool();

	/*!	シェルフパッキングでセルを確保する。既存ページのシェルフに入らなければ
		新しい棚、それも入らなければ新規ページを確保して先頭棚に配置する。

		@retval true  確保できた。pOutに書き込んだ
		@retval false ページ上限到達・確保失敗、またはセルがページより大きい
	*/
	bool AllocCell(int w, int h, SAtlasCellRect* pOut);

	//! 全ページを解放し、プールを空にする(再度AllocCell()すれば作り直される)
	void Clear();

	int     GetPageCount() const { return (int)m_vPages.size(); }
	HDC     GetPageDC(int nPageIndex) const { return m_vPages[nPageIndex].hdcPage; }
	HBITMAP GetPageBitmap(int nPageIndex) const { return m_vPages[nPageIndex].hBitmap; }

private:
	struct SPage {
		HBITMAP hBitmap = nullptr;
		HDC     hdcPage = nullptr;
		HBITMAP hbmpOld = nullptr;
		int     nShelfX = 0;
		int     nShelfY = 0;
		int     nShelfHeight = 0;
	};
	SPage* CreatePage();

	const int m_nPageSize;
	const int m_nMaxPages;
	bool m_bPageAllocFailed;	//!< 一度でもページ確保に失敗したら以後は試みない
	std::vector<SPage> m_vPages;
};

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE

#endif /* SAKURA_CATLASPAGEPOOL_7C9E9B3F_5B7B_4C9E_8B9B_3F7C9E9B3F5B_H_ */
/*[EOF]*/
