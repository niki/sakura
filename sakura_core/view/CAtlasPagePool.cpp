/*!	@file
	@brief GDI互換ビットマップページのシェルフパッキングプール
*/
#include "StdAfx.h"
#include "CAtlasPagePool.h"

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

CAtlasPagePool::CAtlasPagePool(int nPageSize, int nMaxPages)
	: m_nPageSize(nPageSize)
	, m_nMaxPages(nMaxPages)
	, m_bPageAllocFailed(false)
{
	// m_vPagesはm_nMaxPages個までしか増えないため事前にreserveしておく。
	// これが無いと、呼び出し側(WarmUpAscii等)がページ確保を繰り返す間に
	// push_backでvectorが再確保され、その時点で保持していたポインタ/参照が
	// ダングリングになる不具合があった(実機で解像度変更直後の文字化け・
	// 黒塗り潰しとして再現・修正確認済み。詳細はCGlyphAtlasCache.cppの
	// 旧コメント参照)。呼び出し側はページを跨ぐ処理ではポインタではなく
	// ページ番号(int)を保持し、都度GetPageDC()で引き直すこと。
	m_vPages.reserve(m_nMaxPages);
}

CAtlasPagePool::~CAtlasPagePool()
{
	Clear();
}

void CAtlasPagePool::Clear()
{
	for( auto& page : m_vPages ){
		::SelectObject(page.hdcPage, page.hbmpOld);
		::DeleteObject(page.hBitmap);
		::DeleteDC(page.hdcPage);
	}
	m_vPages.clear();
	m_bPageAllocFailed = false;
}

CAtlasPagePool::SPage* CAtlasPagePool::CreatePage()
{
	if( m_bPageAllocFailed ) return nullptr;
	if( (int)m_vPages.size() >= m_nMaxPages ) return nullptr;

	HDC hdcScreen = ::GetDC(NULL);
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, m_nPageSize, m_nPageSize);
	HDC hdcPage = ::CreateCompatibleDC(hdcScreen);
	::ReleaseDC(NULL, hdcScreen);

	if( !hBitmap || !hdcPage ){
		if( hBitmap ) ::DeleteObject(hBitmap);
		if( hdcPage ) ::DeleteDC(hdcPage);
		m_bPageAllocFailed = true;
		return nullptr;
	}

	SPage page;
	page.hBitmap = hBitmap;
	page.hdcPage = hdcPage;
	page.hbmpOld = (HBITMAP)::SelectObject(hdcPage, hBitmap);
	::SetBkMode(hdcPage, OPAQUE);

	m_vPages.push_back(page);
	return &m_vPages.back();
}

bool CAtlasPagePool::AllocCell(int w, int h, SAtlasCellRect* pOut)
{
	if( w <= 0 || h <= 0 || w > m_nPageSize || h > m_nPageSize ) return false;

	for( int i = 0; i < (int)m_vPages.size(); ++i ){
		SPage& page = m_vPages[i];
		if( page.nShelfX + w <= m_nPageSize && h <= page.nShelfHeight ){
			pOut->nPageIndex = i;
			pOut->rcCell.left   = page.nShelfX;
			pOut->rcCell.top    = page.nShelfY;
			pOut->rcCell.right  = page.nShelfX + w;
			pOut->rcCell.bottom = page.nShelfY + page.nShelfHeight;
			page.nShelfX += w;
			return true;
		}
		int nNextShelfY = page.nShelfY + page.nShelfHeight;
		if( page.nShelfHeight > 0 && nNextShelfY + h <= m_nPageSize ){
			// 新しい棚を開始
			page.nShelfY = nNextShelfY;
			page.nShelfHeight = h;
			page.nShelfX = w;
			pOut->nPageIndex = i;
			pOut->rcCell.left   = 0;
			pOut->rcCell.top    = page.nShelfY;
			pOut->rcCell.right  = w;
			pOut->rcCell.bottom = page.nShelfY + h;
			return true;
		}
	}

	// どのページにも入らない: 新規ページを確保して先頭棚に配置
	SPage* pNew = CreatePage();
	if( !pNew ) return false;
	pNew->nShelfY = 0;
	pNew->nShelfHeight = h;
	pNew->nShelfX = w;
	pOut->nPageIndex = (int)m_vPages.size() - 1;
	pOut->rcCell.left   = 0;
	pOut->rcCell.top    = 0;
	pOut->rcCell.right  = w;
	pOut->rcCell.bottom = h;
	return true;
}

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE
/*[EOF]*/
