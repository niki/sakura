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
#include "types/CType.h"
#include "types/CTypeInit.h"

int g_nKeywordsIdx_BAT = -1;

/* MS-DOSバッチファイル */
void CType_Dos::InitTypeConfigImp(STypeConfig* pType)
{
	//名前と拡張子
	auto_strcpy( pType->m_szTypeName, _T("MS-DOSバッチファイル") );
	auto_strcpy( pType->m_szTypeExts, _T("bat") );

	//設定
	pType->m_cLineComment.CopyTo( 0, L"REM ", -1 );	/* 行コメントデリミタ */
	pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_BAT;

	pType->m_eDefaultOutline = OUTLINE_FILE;		/* アウトライン解析方法 */
	auto_strcpy_s( pType->m_szOutlineRuleFilename, _countof2(pType->m_szOutlineRuleFilename), _T("Keyword\\bat.rl") );

	pType->m_KeyHelpArr[0].m_bUse = true;
	auto_strcpy( pType->m_KeyHelpArr[0].m_szAbout, _T(";バッチファイルのキーワードヘルプ定義") );
	auto_strcpy_s( pType->m_KeyHelpArr[0].m_szPath, _countof2(pType->m_KeyHelpArr[0].m_szPath), _T("Keyword\\bat_win2k.khp") );
	pType->m_bUseKeyWordHelp = true;		// 辞書選択機能の使用可否
	pType->m_nKeyHelpNum = 1;				// 登録辞書数
}



#ifdef BUILD_OPT_IMPKEYWORD
const wchar_t* g_ppszKeywordsBAT[] = {
#include "generated/bat_keywords.inc"
};
int g_nKeywordsBAT = _countof(g_ppszKeywordsBAT);
#endif
