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

/* CSS */
void CType_Xml::InitTypeConfigImp(STypeConfig* pType)
{
	//–¼‘O‚ÆŠg’£Žq
	_tcscpy( pType->m_szTypeName, _T("XML") );
	_tcscpy( pType->m_szTypeExts, _T("xml,xsl,xslt") );

	//Ý’è
	pType->m_cBlockComments[0].SetBlockCommentRule( L"<!--", L"-->" );
	pType->m_cBlockComments[1].SetBlockCommentRule( L"<![CDATA[", L"]]>" );
	pType->m_nStringType = STRING_LITERAL_HTML;
	pType->m_bStringLineOnly = true;
	pType->m_eDefaultOutline = OUTLINE_XML;
	pType->m_ColorInfoArr[COLORIDX_SSTRING].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_WSTRING].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_BRACKET_PAIR].m_bDisp = true;

	SetColorInfoBC(pType, COLORIDX_REGEX1, true,  RGB(255,  0,  0));
	SetColorInfoBC(pType, COLORIDX_REGEX2, true,  RGB(  0,  0,  0));
	SetColorInfoBC(pType, COLORIDX_REGEX3, false, RGB(128,  0,128));
	SetColorInfoBC(pType, COLORIDX_REGEX4, false, RGB(  0,128,255));
	SetColorInfoBC(pType, COLORIDX_REGEX5, true,  RGB(  0,  0,160));
	int keywordPos = 0;
	int idx = 0;
	pType->m_bUseRegexKeyword = true;
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/<!--.*?-->/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/<!--[^\\r\\n]*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/<!\\[CDATA\\[[^\\r\\n]?*\\]\\]>/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/<!\\[CDATA\\[[^\\r\\n]*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX1, L"/<\\/(?=[^!?a-z\\/])/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX1, L"/<(?=[^!?a-z\\/])/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX2, L"/<\\/|<|\\/>|>/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX3, L"/(?<=<)[a-z][a-z0-9_:\\-]*/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX3, L"/(?<=<\\/)[a-z][A-Za-z0-9_:\\-]*(?=>)/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX4, L"/\\b[a-z][a-z0-9_:\\-]+(?==')/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX4, L"/\\b[a-z][a-z0-9_:\\-]+(?==\")/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX5, L"/&(([a-z]{1,9})|(#((x[0-9a-f]{1,8})|([0-9]{1,12}))));/ki" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX1, L"/&/k" );
}



