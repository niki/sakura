/*
	Copyright (C) 2026, CAT-ech Co.

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

#include "StdAfx.h"
#include "CGlyphAtlasCache.h"

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include "CViewFont.h"
#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
#include "util/file.h"	// GetExedir
#endif // NKMM_

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

//! CViewFont::GetFontGeneration()が変化していたら(=フォントが作り直されていたら)全破棄する
void CGlyphAtlasCache::ClearIfStale()
{
	ULONG cur = CViewFont::GetFontGeneration();
	if( cur == m_nFontGeneration ) return;
	m_nFontGeneration = cur;
	Clear();
}

void CGlyphAtlasCache::Clear()
{
#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
	if( !m_vPages.empty() ){
		wchar_t szExeDir[_MAX_PATH];
		GetExedir(szExeDir);	// sakura.exe のあるディレクトリ(util/file.h)
		wchar_t szDumpDir[MAX_PATH];
		swprintf_s(szDumpDir, L"%s\\sakura_glyph_atlas_dump", szExeDir);
		::CreateDirectoryW(szDumpDir, NULL);	// 既に存在していればエラーになるが無視してよい

		for( size_t i = 0; i < m_vPages.size(); ++i ){
			wchar_t szPath[MAX_PATH];
			swprintf_s(szPath, L"%s\\clear%03d_page%d.bmp", szDumpDir, m_nDumpCounter, (int)i);
			DumpPageToFile(m_vPages[i], szPath);
		}
		++m_nDumpCounter;
	}
#endif // NKMM_

	for( auto& page : m_vPages ){
		::SelectObject(page.hdcPage, page.hbmpOld);
		::DeleteObject(page.hBitmap);
		::DeleteDC(page.hdcPage);
	}
	m_vPages.clear();
	m_mapEntries.clear();
	m_vPendingBlits.clear();	// ページを解放するので、積み残しのBitBlt指示(ページ番号参照)も破棄する
	m_bPageAllocFailed = false;
}

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
//! GetDIBits()でHBITMAPの生ピクセルを取得し、BITMAPFILEHEADER/BITMAPINFOHEADERを
//! 自前で組み立てて実物のbmpファイルとして書き出す(GDI+等の再エンコードを介さない)。
void CGlyphAtlasCache::DumpPageToFile(const SGlyphAtlasPage& page, const wchar_t* pszPath) const
{
	BITMAPINFOHEADER bi = {};
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = PAGE_SIZE;
	bi.biHeight = PAGE_SIZE;	// 正の値 = ボトムアップDIB(標準的なBMPの並び)
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;

	const DWORD dwStride = PAGE_SIZE * 4;
	const DWORD dwBufSize = dwStride * PAGE_SIZE;
	std::vector<BYTE> vPixels(dwBufSize);

	int nScanLines = ::GetDIBits(page.hdcPage, page.hBitmap, 0, PAGE_SIZE, vPixels.data(),
		(BITMAPINFO*)&bi, DIB_RGB_COLORS);
	if( nScanLines == 0 ) return;

	BITMAPFILEHEADER bf = {};
	bf.bfType = 0x4D42;	// 'BM'
	bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bf.bfSize = bf.bfOffBits + dwBufSize;

	HANDLE hFile = ::CreateFileW(pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE ) return;

	DWORD dwWritten;
	::WriteFile(hFile, &bf, sizeof(bf), &dwWritten, NULL);
	::WriteFile(hFile, &bi, sizeof(bi), &dwWritten, NULL);
	::WriteFile(hFile, vPixels.data(), dwBufSize, &dwWritten, NULL);
	::CloseHandle(hFile);
}
#endif // NKMM_

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

//! シェルフパッキングでセルを確保する。既存ページのシェルフに入らなければ新しい棚、
//! それも入らなければ新規ページを試みる。
bool CGlyphAtlasCache::AllocCell(int w, int h, SGlyphAtlasEntry* pOut)
{
	if( w <= 0 || h <= 0 || w > PAGE_SIZE || h > PAGE_SIZE ) return false;

	for( int i = 0; i < (int)m_vPages.size(); ++i ){
		SGlyphAtlasPage& page = m_vPages[i];
		if( page.nShelfX + w <= PAGE_SIZE && h <= page.nShelfHeight ){
			pOut->nPageIndex = i;
			pOut->rcCell.left   = page.nShelfX;
			pOut->rcCell.top    = page.nShelfY;
			pOut->rcCell.right  = page.nShelfX + w;
			pOut->rcCell.bottom = page.nShelfY + page.nShelfHeight;
			page.nShelfX += w;
			return true;
		}
		int nNextShelfY = page.nShelfY + page.nShelfHeight;
		if( page.nShelfHeight > 0 && nNextShelfY + h <= PAGE_SIZE ){
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
	SGlyphAtlasPage* pNew = CreatePage();
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

bool CGlyphAtlasCache::DrawOrCache(
	HDC hdc,
	HFONT hFont,
	const wchar_t* pData, int nLength,
	COLORREF crFore, COLORREF crBack,
	int nDestX, int nDestY,
	int nCellWidthPx, int nCellHeightPx,
	const int* pDx)
{
	if( !m_bEnabled ) return false;
	if( nLength < 1 || nLength > 2 ) return false;
	ClearIfStale();

	SGlyphAtlasKey key{ hFont, pData[0], (nLength == 2 ? pData[1] : L'\0'), crFore, crBack, nCellWidthPx, nCellHeightPx };
	auto it = m_mapEntries.find(key);
	if( it != m_mapEntries.end() ){
		const SGlyphAtlasEntry& e = it->second;
		m_vPendingBlits.push_back(SGlyphAtlasBlit{ e.nPageIndex, e.rcCell, nDestX, nDestY });
#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
		++m_nHitCount;
#endif // NKMM_
		return true;
	}

	// 今回ミスした文字自身が半角ASCII印字可能文字なら、この(フォント,fg,bg)の組み合わせで
	// 以後発生するはずだったASCII文字のミスをまとめて先に焼いておく。nCellWidthPxは
	// 「半角ASCII文字自身のミス」のときしか渡ってこないので、そのままASCII全体の
	// セル幅として使ってよい(全角文字のミスではこの分岐へ入らない)
	if( nLength == 1 && pData[0] >= 0x20 && pData[0] <= 0x7E ){
		WarmUpAscii(hFont, crFore, crBack, nCellWidthPx, nCellHeightPx);
		it = m_mapEntries.find(key);
		if( it != m_mapEntries.end() ){
			const SGlyphAtlasEntry& e = it->second;
			m_vPendingBlits.push_back(SGlyphAtlasBlit{ e.nPageIndex, e.rcCell, nDestX, nDestY });
#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
			++m_nMissCount;	// ウォームアップ経由でも、今回の文字にとっては「ミスして焼いた」ことに変わりないので数える
#endif // NKMM_
			return true;
		}
		// ページ上限等でウォームアップ中にこの文字自体は焼けなかった。
		// 以下の単発ミス処理へフォールバックする
	}

	SGlyphAtlasEntry newEntry;
	if( !AllocCell(nCellWidthPx, nCellHeightPx, &newEntry) ){
		// ページ上限到達 or 確保失敗。今回は呼び出し側に直接描画させる(既存ヒットは継続利用可能)
		return false;
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

	m_vPendingBlits.push_back(SGlyphAtlasBlit{ newEntry.nPageIndex, rcCellDest, nDestX, nDestY });
#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
	++m_nMissCount;
#endif // NKMM_
	return true;
}

//! 半角ASCII印字可能文字(0x20〜0x7E)を(hFont,crFore,crBack)の組でまとめて焼く。
//! 1文字ずつSelectObject/SetTextColor/SetBkColorをやり直すコストを、同じページに
//! 収まる範囲でひとまとめにする(ページ境界をまたいだら選択し直す)。
//! 既にキャッシュ済みの文字はスキップするので、途中(ウォームアップ未完了)の
//! 状態で複数回呼ばれても安全。
void CGlyphAtlasCache::WarmUpAscii(HFONT hFont, COLORREF crFore, COLORREF crBack, int nCellWidthPx, int nCellHeightPx)
{
	int dx[1] = { nCellWidthPx };
	SGlyphAtlasPage* pCurPage = nullptr;
	HFONT hOldFont = nullptr;

	for( wchar_t ch = 0x20; ch <= 0x7E; ++ch ){
		SGlyphAtlasKey key{ hFont, ch, L'\0', crFore, crBack, nCellWidthPx, nCellHeightPx };
		if( m_mapEntries.find(key) != m_mapEntries.end() ) continue;

		SGlyphAtlasEntry newEntry;
		if( !AllocCell(nCellWidthPx, nCellHeightPx, &newEntry) ) break;	// ページ上限等。ここで打ち切る
		SGlyphAtlasPage& page = m_vPages[newEntry.nPageIndex];

		if( &page != pCurPage ){
			// ページが変わった(または初回)ので、フォント・色の選択をやり直す
			if( pCurPage ){
				::SelectObject(pCurPage->hdcPage, hOldFont);
			}
			pCurPage = &page;
			::SetTextColor(page.hdcPage, crFore);
			::SetBkColor(page.hdcPage, crBack);
			hOldFont = (HFONT)::SelectObject(page.hdcPage, hFont);
		}

		RECT rcCellDest = newEntry.rcCell;
		::ExtTextOutW_AnyBuild(page.hdcPage, rcCellDest.left, rcCellDest.top,
			ETO_CLIPPED | ETO_OPAQUE, &rcCellDest, &ch, 1, dx);

		newEntry.nCellWidthPx  = nCellWidthPx;
		newEntry.nCellHeightPx = nCellHeightPx;
		m_mapEntries.emplace(key, newEntry);
#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
		++m_nWarmedCount;
#endif // NKMM_
	}

	if( pCurPage ){
		::SelectObject(pCurPage->hdcPage, hOldFont);
	}
}

#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
CGlyphAtlasCache::SStats CGlyphAtlasCache::GetStats() const
{
	SStats st;
	st.nPageCount        = (int)m_vPages.size();
	st.nEntryCount       = m_mapEntries.size();
	st.nPendingBlitCount = m_vPendingBlits.size();
	st.nHitCount         = m_nHitCount;
	st.nMissCount        = m_nMissCount;
	st.nWarmedCount      = m_nWarmedCount;
	return st;
}
#endif // NKMM_

//! DrawOrCache()が積んだ分をまとめてBitBltする。ページDCとhdcの行き来を
//! グリフ単位で繰り返さないよう、ミスの焼き込み(ページ側)と転送(hdc側)を
//! フェーズ分離するのが目的(詳細はヘッダのコメント、changelog参照)。
void CGlyphAtlasCache::FlushQueue(HDC hdc)
{
	for( const SGlyphAtlasBlit& blit : m_vPendingBlits ){
		const SGlyphAtlasPage& page = m_vPages[blit.nPageIndex];
		::BitBlt(hdc, blit.nDestX, blit.nDestY,
			blit.rcSrc.right - blit.rcSrc.left, blit.rcSrc.bottom - blit.rcSrc.top,
			page.hdcPage, blit.rcSrc.left, blit.rcSrc.top, SRCCOPY);
	}
	m_vPendingBlits.clear();
}

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE
