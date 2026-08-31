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
	, m_pagePool(PAGE_SIZE, MAX_PAGES)
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
	if( m_pagePool.GetPageCount() > 0 ){
		wchar_t szExeDir[_MAX_PATH];
		GetExedir(szExeDir);	// sakura.exe のあるディレクトリ(util/file.h)
		wchar_t szDumpDir[MAX_PATH];
		swprintf_s(szDumpDir, L"%s\\sakura_glyph_atlas_dump", szExeDir);
		::CreateDirectoryW(szDumpDir, NULL);	// 既に存在していればエラーになるが無視してよい

		for( int i = 0; i < m_pagePool.GetPageCount(); ++i ){
			wchar_t szPath[MAX_PATH];
			swprintf_s(szPath, L"%s\\clear%03d_page%d.bmp", szDumpDir, m_nDumpCounter, i);
			DumpPageToFile(i, szPath);
		}
		++m_nDumpCounter;
	}
#endif // NKMM_

	m_pagePool.Clear();
	m_mapEntries.clear();
	m_vPendingBlits.clear();	// ページを解放するので、積み残しのBitBlt指示(ページ番号参照)も破棄する
}

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
//! GetDIBits()でHBITMAPの生ピクセルを取得し、BITMAPFILEHEADER/BITMAPINFOHEADERを
//! 自前で組み立てて実物のbmpファイルとして書き出す(GDI+等の再エンコードを介さない)。
void CGlyphAtlasCache::DumpPageToFile(int nPageIndex, const wchar_t* pszPath) const
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

	int nScanLines = ::GetDIBits(m_pagePool.GetPageDC(nPageIndex), m_pagePool.GetPageBitmap(nPageIndex), 0, PAGE_SIZE, vPixels.data(),
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

//! シェルフパッキングでのページ確保自体はCAtlasPagePoolに委譲し、結果を
//! SGlyphAtlasEntryへ詰め替えるだけ。CAtlasPagePoolは元々CColorFontRenderer側の
//! カラーグリフとも共用する想定で抽出したが、CColorFontRendererは現状これを
//! 参照しておらず(グリフ単位のキャッシュを持たず毎フレームDirect2Dで描き直す)、
//! 実際にCAtlasPagePoolを使っているのはこのクラスのみ 20260831。
bool CGlyphAtlasCache::AllocCell(int w, int h, SGlyphAtlasEntry* pOut)
{
	SAtlasCellRect rc;
	if( !m_pagePool.AllocCell(w, h, &rc) ) return false;
	pOut->nPageIndex = rc.nPageIndex;
	pOut->rcCell = rc.rcCell;
	return true;
}

bool CGlyphAtlasCache::DrawOrCache(
	HDC hdc,
	HFONT hFont,
	const wchar_t* pData, int nLength,
	COLORREF crFore, COLORREF crBack,
	int nDestX, int nDestY,
	int nGlyphYOffset,
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
		WarmUpAscii(hFont, crFore, crBack, nGlyphYOffset, nCellWidthPx, nCellHeightPx);
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
	HDC hdcPage = m_pagePool.GetPageDC(newEntry.nPageIndex);

	::SetTextColor(hdcPage, crFore);
	::SetBkColor(hdcPage, crBack);
	HFONT hOldFont = (HFONT)::SelectObject(hdcPage, hFont);
	RECT rcCellDest = newEntry.rcCell;
	// 20260809 ETO_OPAQUEによる不透明フィルはセル全体(rcCellDest)を対象にしつつ、
	// 実際にグリフを描く位置はセルの上端からnGlyphYOffsetだけ下にずらす。
	// (呼び出し側がnDestYを行の折り返しクリップ矩形の上端(マージン抜き)で
	// 渡し、グリフ自体は行間マージン分だけ下にずらして描きたいケースに対応する。
	// ここでずらさずnDestY側だけをマージン込みにすると、マージン分だけ
	// BitBlt先が本来のクリップ矩形からずれ、行の上端が塗り残されたまま
	// 次の行のマージン部分にはみ出して描画されてしまう)
	::ExtTextOutW_AnyBuild(hdcPage, rcCellDest.left, rcCellDest.top + nGlyphYOffset,
		ETO_CLIPPED | ETO_OPAQUE, &rcCellDest, pData, nLength, pDx);
	::SelectObject(hdcPage, hOldFont);

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
void CGlyphAtlasCache::WarmUpAscii(HFONT hFont, COLORREF crFore, COLORREF crBack, int nGlyphYOffset, int nCellWidthPx, int nCellHeightPx)
{
	int dx[1] = { nCellWidthPx };
	// ページはCAtlasPagePool内部にあり生ポインタを持てないため、ページ番号(int)で
	// 「フォント・色の選択をやり直すべきか」を判定する(-1は「まだ何も選択していない」)。
	int nCurPageIndex = -1;
	HDC hdcCurPage = nullptr;
	HFONT hOldFont = nullptr;

	for( wchar_t ch = 0x20; ch <= 0x7E; ++ch ){
		SGlyphAtlasKey key{ hFont, ch, L'\0', crFore, crBack, nCellWidthPx, nCellHeightPx };
		if( m_mapEntries.find(key) != m_mapEntries.end() ) continue;

		SGlyphAtlasEntry newEntry;
		if( !AllocCell(nCellWidthPx, nCellHeightPx, &newEntry) ) break;	// ページ上限等。ここで打ち切る

		if( newEntry.nPageIndex != nCurPageIndex ){
			// ページが変わった(または初回)ので、フォント・色の選択をやり直す
			if( nCurPageIndex >= 0 ){
				::SelectObject(hdcCurPage, hOldFont);
			}
			nCurPageIndex = newEntry.nPageIndex;
			hdcCurPage = m_pagePool.GetPageDC(nCurPageIndex);
			::SetTextColor(hdcCurPage, crFore);
			::SetBkColor(hdcCurPage, crBack);
			hOldFont = (HFONT)::SelectObject(hdcCurPage, hFont);
		}

		RECT rcCellDest = newEntry.rcCell;
		::ExtTextOutW_AnyBuild(hdcCurPage, rcCellDest.left, rcCellDest.top + nGlyphYOffset,
			ETO_CLIPPED | ETO_OPAQUE, &rcCellDest, &ch, 1, dx);

		newEntry.nCellWidthPx  = nCellWidthPx;
		newEntry.nCellHeightPx = nCellHeightPx;
		m_mapEntries.emplace(key, newEntry);
#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
		++m_nWarmedCount;
#endif // NKMM_
	}

	if( nCurPageIndex >= 0 ){
		::SelectObject(hdcCurPage, hOldFont);
	}
}

#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
CGlyphAtlasCache::SStats CGlyphAtlasCache::GetStats() const
{
	SStats st;
	st.nPageCount        = m_pagePool.GetPageCount();
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
void CGlyphAtlasCache::FlushQueue(HDC hdc, size_t markBegin)
{
	// markBeginより前は他の(外側の)描画パスがまだ積んでいる途中の分なので触らない。
	// Clear()/ClearIfStale()でページごと破棄された場合はmarkBeginが現在のサイズを
	// 超えていることがあるので、その場合は何もしない。
	if( markBegin >= m_vPendingBlits.size() ) return;

	for( size_t i = markBegin; i < m_vPendingBlits.size(); ++i ){
		const SGlyphAtlasBlit& blit = m_vPendingBlits[i];
		::BitBlt(hdc, blit.nDestX, blit.nDestY,
			blit.rcSrc.right - blit.rcSrc.left, blit.rcSrc.bottom - blit.rcSrc.top,
			m_pagePool.GetPageDC(blit.nPageIndex), blit.rcSrc.left, blit.rcSrc.top, SRCCOPY);
	}
	// 自分(このパス)が積んだ分だけを取り除く。外側のパス分(markBeginより前)は残す。
	m_vPendingBlits.resize(markBegin);
}

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE
