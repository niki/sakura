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

int g_nKeywordsIdx_RTF;

/* リッチテキスト */
//JUl. 10, 2001 JEPRO WinHelp作るのにいるケンね
//Jul. 10, 2001 JEPRO 追加
void CType_Rich::InitTypeConfigImp(STypeConfig* pType)
{
	//名前と拡張子
	_tcscpy( pType->m_szTypeName, _T("リッチテキスト") );
	_tcscpy( pType->m_szTypeExts, _T("rtf") );

	//設定
	pType->m_eDefaultOutline = OUTLINE_TEXT;					/* アウトライン解析方法 */
	pType->m_nKeyWordSetIdx[0]  = g_nKeywordsIdx_RTF;
	pType->m_ColorInfoArr[COLORIDX_DIGIT].m_bDisp = true;		/* 半角数値を色分け表示 */
	pType->m_ColorInfoArr[COLORIDX_SSTRING].m_bDisp = false;	//シングルクォーテーション文字列を色分け表示しない
	pType->m_ColorInfoArr[COLORIDX_WSTRING].m_bDisp = false;	//ダブルクォーテーション文字列を色分け表示しない
	pType->m_ColorInfoArr[COLORIDX_URL].m_bDisp = false;		//URLにアンダーラインを引かない
}



#ifdef BUILD_OPT_IMPKEYWORD
//Jul. 10, 2001 JEPRO 追加
const wchar_t* g_ppszKeywordsRTF[] = {
#include "generated/rtf_keywords.inc"
};
int g_nKeywordsRTF = _countof(g_ppszKeywordsRTF);
#endif
