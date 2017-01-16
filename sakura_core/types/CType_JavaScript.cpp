/*
	Copyright (C) 2008, kobake
	Copyright (C) 2014, Moca

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
#include "view/colors/EColorIndexType.h"

int g_nKeywordsIdx_JS = -1;
int g_nKeywordsIdx_JS2 = -1;

/* JavaScript */
void CType_JavaScript::InitTypeConfigImp(STypeConfig* pType)
{
	//名前と拡張子
	_tcscpy( pType->m_szTypeName, _T("JavaScript") );
	_tcscpy( pType->m_szTypeExts, _T("js, json") );

	//設定
	pType->m_cLineComment.CopyTo( 0, L"//", -1 );
	pType->m_cBlockComments[0].SetBlockCommentRule( L"/*", L"*/" );
	pType->m_bStringLineOnly = true;
	pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_JS;
	pType->m_nKeyWordSetIdx[1] = g_nKeywordsIdx_JS2;
	pType->m_ColorInfoArr[COLORIDX_DIGIT].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_SSTRING].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_WSTRING].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_BRACKET_PAIR].m_bDisp = true;
	SetColorInfoBC(pType, COLORIDX_REGEX1, false,  RGB(  0,  0,128));

	// ルールファイル
	pType->m_eDefaultOutline = OUTLINE_FILE;
	auto_strcpy( pType->m_szOutlineRuleFilename, _T("Keyword\\JavaScript.rule") );

	int keywordPos = 0;
	int idx = 0;
	pType->m_bUseRegexKeyword = true;
	// 正規表現リテラルは文脈依存なので、厳密には定義できないことに注意
	// 正規表現リテラル中に/*や、//があった場合などもおかしくなる
	// 「ret = a / b /i;」のような場合にも色が変わることに注意
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/\\/\\*(\\*[^\\/]|[^\\*]\\/|[^\\*\\/])*\\*\\//k" ); // コメント /* */ 行内にあると、正規表現キーワード色になってしまう対策
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX1, L"/(?<=[^a-zA-Z0-9_])\\/(\\\\.|[^\\\\\\/\\r\\n]){1,400}\\/[gimy]{0,4}/k" ); // 正規表現リテラル
}



