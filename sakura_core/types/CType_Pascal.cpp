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
#include "view/colors/EColorIndexType.h"

int g_nKeywordsIdx_PASCAL = -1;

/* Pascal */
//Mar. 10, 2001 JEPRO	半角数値を色分け表示
void CType_Pascal::InitTypeConfigImp(STypeConfig* pType)
{
	//名前と拡張子
	auto_strcpy( pType->m_szTypeName, _T("Pascal") );
	auto_strcpy( pType->m_szTypeExts, _T("dpr,pas") );

	//設定
	pType->m_cLineComment.CopyTo( 0, L"//", -1 );					/* 行コメントデリミタ */		//Nov. 5, 2000 JEPRO 追加
	pType->m_cBlockComments[0].SetBlockCommentRule( L"{", L"}" );	/* ブロックコメントデリミタ */	//Nov. 5, 2000 JEPRO 追加
	pType->m_cBlockComments[1].SetBlockCommentRule( L"(*", L"*)" );	/* ブロックコメントデリミタ2 */	//@@@ 2001.03.10 by MIK
	pType->m_nStringType = 1;										/* 文字列区切り記号エスケープ方法  0=[\"][\'] 1=[""][''] */	//Nov. 5, 2000 JEPRO 追加
	pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_PASCAL;
	pType->m_ColorInfoArr[COLORIDX_DIGIT].m_bDisp = true;			//@@@ 2001.11.11 upd MIK
	pType->m_bStringLineOnly = true; // 文字列は行内のみ
}



#ifdef BUILD_OPT_IMPKEYWORD
// 2014.11.23 PASCALが__stdcallに置換されるためPASCALに変更
const wchar_t* g_ppszKeywordsPASCAL[] = {
#include "generated/pascal_keywords.inc"
};
int g_nKeywordsPASCAL = _countof(g_ppszKeywordsPASCAL);
#endif
