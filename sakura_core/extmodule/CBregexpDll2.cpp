/*
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
#include "CBregexpDll2.h"
#ifdef NKMM_FIX_REGEXP_FALLBACK
#include "CRegexFallback.h"
#endif

CBregexpDll2::CBregexpDll2()
{
}

CBregexpDll2::~CBregexpDll2()
{
}

#ifdef NKMM_FIX_REGEXP_FALLBACK
/*!
	20260720 bregonig.dllが見つからない場合にPCRE2へフォールバックする機能を追加。
	DLLロードと初期処理。bregonig.dll(bregonig32/64.dll含む)が見つからない場合は、
	PCRE2ベースのフォールバックエンジンへ切り替えて成功扱いにする。
	DLLは見つかったがエクスポート不整合の場合(DLL_INITFAILURE)は、DLLが壊れている
	ことを示す実エラーなので、フォールバックせずそのまま返す。
*/
EDllResult CBregexpDll2::InitDllWithFallback(LPCTSTR pszSpecifiedDllName)
{
	EDllResult result = InitDll(pszSpecifiedDllName);
	if (result == DLL_LOADFAILURE) {
		m_bFallback = true;
		return DLL_SUCCESS;
	}
	return result;
}

// 20260720 以下8関数は、フォールバック中はRegexFallback名前空間(PCRE2ベース)へ委譲する。
// (元は CBregexpDll2.h にインライン定義されていたが、フォールバック分岐のため.cppへ移動)
int CBregexpDll2::BMatch(const wchar_t* str, const wchar_t* target,const wchar_t* targetendp,BREGEXP_W** rxp,wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BMatch(str,target,targetendp,rxp,msg);
	return m_BMatch(str,target,targetendp,rxp,msg);
}
int CBregexpDll2::BSubst(const wchar_t* str, const wchar_t* target,const wchar_t* targetendp,BREGEXP_W** rxp,wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BSubst(str,target,targetendp,rxp,msg);
	return m_BSubst(str,target,targetendp,rxp,msg);
}
int CBregexpDll2::BTrans(const wchar_t* str, wchar_t* target,wchar_t* targetendp,BREGEXP_W** rxp,wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BTrans(str,target,targetendp,rxp,msg);
	return m_BTrans(str,target,targetendp,rxp,msg);
}
int CBregexpDll2::BSplit(const wchar_t* str, wchar_t* target,wchar_t* targetendp,int limit,BREGEXP_W** rxp,wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BSplit(str,target,targetendp,limit,rxp,msg);
	return m_BSplit(str,target,targetendp,limit,rxp,msg);
}
void CBregexpDll2::BRegfree(BREGEXP_W* rx)
{
	if (m_bFallback) { RegexFallback::BRegfree(rx); return; }
	m_BRegfree(rx);
}
const wchar_t* CBregexpDll2::BRegexpVersion(void)
{
	if (m_bFallback) return RegexFallback::BRegexpVersion();
	return m_BRegexpVersion();
}
int CBregexpDll2::BMatchEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BMatchEx(str,targetbeg,target,targetendp,rxp,msg);
	return m_BMatchEx(str,targetbeg,target,targetendp,rxp,msg);
}
int CBregexpDll2::BSubstEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	if (m_bFallback) return RegexFallback::BSubstEx(str,targetbeg,target,targetendp,rxp,msg);
	return m_BSubstEx(str,targetbeg,target,targetendp,rxp,msg);
}
#endif // NKMM_FIX_REGEXP_FALLBACK

/*!
	@date 2001.07.05 genta 引数追加。ただし、ここでは使わない。
	@date 2007.06.25 genta 複数のDLL名に対応
	@date 2007.09.13 genta サーチルールを変更
		@li 指定有りの場合はそれのみを返す
		@li 指定無し(NULLまたは空文字列)の場合はBREGONIG, BREGEXPの順で試みる
*/
LPCTSTR CBregexpDll2::GetDllNameImp( int index )
{
#ifdef NKMM_FIX_BREGONIG_NAME_SEARCH
	TCHAR szPath[_MAX_PATH + 1];
	GetExedir(szPath);
	std::wstring fname = szPath;
#ifdef _WIN64
	fname += _T("\\bregonig64.dll");
	if (fexist(fname.c_str())) {
		return _T("bregonig64.dll");
	}
#else
	fname += _T("\\bregonig32.dll");
	if (fexist(fname.c_str())) {
		return _T("bregonig32.dll");
	}
#endif // _WIN64
#endif // NKMM_
	return _T("bregonig.dll");
}


/*!
	DLLの初期化

	関数のアドレスを取得してメンバに保管する．

	@retval true 成功
	@retval false アドレス取得に失敗
*/
bool CBregexpDll2::InitDllImp()
{
	//DLL内関数名リスト
	const ImportTable table[] = {
		{ &m_BMatch,			"BMatchW" },
		{ &m_BSubst,			"BSubstW" },
		{ &m_BTrans,			"BTransW" },
		{ &m_BSplit,			"BSplitW" },
		{ &m_BRegfree,			"BRegfreeW" },
		{ &m_BRegexpVersion,	"BRegexpVersionW" },
		{ &m_BMatchEx,			"BMatchExW" },
		{ &m_BSubstEx,			"BSubstExW" },
		{ NULL, 0 }
	};
	
	if( ! RegisterEntries( table )){
		return false;
	}
	
	return true;
}
