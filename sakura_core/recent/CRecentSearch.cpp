﻿/*
	Copyright (C) 2008, kobake

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
#include "CRecentSearch.h"
#include "config/maxdata.h"
#include "env/DLLSHAREDATA.h"
#include <string.h>





// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           生成                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

CRecentSearch::CRecentSearch()
{
	Create(
		GetShareData()->m_sSearchKeywords.m_aSearchKeys.dataPtr(),
		GetShareData()->m_sSearchKeywords.m_aSearchKeys.dataPtr()->GetBufferCount(),
		&GetShareData()->m_sSearchKeywords.m_aSearchKeys._GetSizeRef(),
		NULL,
#ifdef NKMM_FIX_MAXDATA
		RegKey(NKMM_REGKEY).get(_T("RecentSearchKeyMax"), NKMM_MAX_SEARCHKEY),
#else
		MAX_SEARCHKEY,
#endif // NKMM_
		NULL
	);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                      オーバーライド                         //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*
	アイテムの比較要素を取得する。

	@note	取得後のポインタはユーザ管理の構造体にキャストして参照してください。
*/
const TCHAR* CRecentSearch::GetItemText( int nIndex ) const
{
	return to_tchar(*GetItem(nIndex));
}

bool CRecentSearch::DataToReceiveType( LPCWSTR* dst, const CSearchString* src ) const
{
	*dst = *src;
	return true;
}

bool CRecentSearch::TextToDataType( CSearchString* dst, LPCTSTR pszText ) const
{
	if( false == ValidateReceiveType(to_wchar(pszText)) ){
		return false;
	}
	CopyItem(dst, to_wchar(pszText));
	return true;
}

int CRecentSearch::CompareItem( const CSearchString* p1, LPCWSTR p2 ) const
{
	return wcscmp(*p1,p2);
}

void CRecentSearch::CopyItem( CSearchString* dst, LPCWSTR src ) const
{
	wcscpy(*dst,src);
}

bool CRecentSearch::ValidateReceiveType( LPCWSTR p ) const
{
	if( GetTextMaxLength() <= wcslen(p) ){
		return false;
	}
	return true;
}

size_t CRecentSearch::GetTextMaxLength() const
{
	return m_nTextMaxLength;
}
