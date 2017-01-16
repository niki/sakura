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

int g_nKeywordsIdx_RUBY = -1;
int g_nKeywordsIdx_RUBY2 = -1;
int g_nKeywordsIdx_RUBY3 = -1;
int g_nKeywordsIdx_RUBY4 = -1;

/* Ruby */
void CType_Ruby::InitTypeConfigImp(STypeConfig* pType)
{
	//–¼‘O‚ÆŠg’£Žq
	_tcscpy( pType->m_szTypeName, _T("Ruby") );
	_tcscpy( pType->m_szTypeExts, _T("rb") );

	//Ý’è
	pType->m_nTabSpace = CKetaXInt(2);
	pType->m_cLineComment.CopyTo( 0, L"#", -1 );
	pType->m_cBlockComments[0].SetBlockCommentRule( L"=begin", L"=end" );
	pType->m_bStringLineOnly = true;
	pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_RUBY;
	pType->m_nKeyWordSetIdx[1] = g_nKeywordsIdx_RUBY2;
	pType->m_nKeyWordSetIdx[2] = g_nKeywordsIdx_RUBY3;
	pType->m_nKeyWordSetIdx[3] = g_nKeywordsIdx_RUBY4;
	pType->m_ColorInfoArr[COLORIDX_DIGIT].m_bDisp = true;
	pType->m_ColorInfoArr[COLORIDX_SSTRING].m_bDisp = false;
	pType->m_ColorInfoArr[COLORIDX_WSTRING].m_bDisp = false;
	SetColorInfoBC(pType, COLORIDX_KEYWORD1, true,  RGB(255,  0,  0));
	SetColorInfoBC(pType, COLORIDX_KEYWORD2, false, RGB(128,  0,128));
	SetColorInfoBC(pType, COLORIDX_KEYWORD3, true,  RGB(255,  0,255));
	SetColorInfoBC(pType, COLORIDX_KEYWORD4, true,  RGB( 64,  0,  0));
	SetColorInfoBC(pType, COLORIDX_KEYWORD5, false, RGB( 96, 96, 96));
	SetColorInfoBC(pType, COLORIDX_REGEX1, true, RGB(128,  0,  0));
	SetColorInfoBC(pType, COLORIDX_REGEX2, true, RGB(255,  0,  0));
	SetColorInfoBC(pType, COLORIDX_REGEX3, true, RGB( 64,  0,128));
	SetColorInfoBC(pType, COLORIDX_REGEX4, true, RGB(  0,128,255));

	pType->m_eDefaultOutline = OUTLINE_FILE;
	auto_strcpy( pType->m_szOutlineRuleFilename, _T("Keyword\\Ruby.rule") );

	int keywordPos = 0;
	int idx = 0;
	pType->m_bUseRegexKeyword = true;
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD3, L"/defined\\?/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/\\/[^\\n]*\\//k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD2, L"/\\$[\\&\\`\\'\\+\\~\\!\\@\\/\\\\\\,\\.\\<\\>\\*\\:\\\"]/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD2, L"/\\$-[0adFiIKlpv]/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX2, L"/\\$#\\$*\\w([\\w\\d']|::)*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX2, L"/[\\$\\@\\%]\\$*\\w([\\w\\d']|::)*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX2, L"/\\&\\$*\\w([\\w\\d']|::)*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_TEXT, L"/\\\\#/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/=begin/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_COMMENT, L"/#.*/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_TEXT, L"/[\\$\\\\][\"'`]/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_TEXT, L"/[\\w\\d]'/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_SSTRING, L"/'[^\\r\\n]*'/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_WSTRING, L"/\"[^\\r\\n]*\"/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/\\./k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD3, L"/[\\[\\]\\|\\,\\.]/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD5, L"/[=\\+\\-\\/\\*\\<\\>]/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_REGEX5, L"/\\d+/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(alive|blockdev|chardev|const_defined|directory|empty|eof|eql|equal|exclude_end|executable)\\?/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(executable_real|file|frozen|grpowned|include|instance_of|integer|is_a|key|kind_of|member)\\?/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(method_defined|more|nonzero|owned|pipe|readable|readable_real|respond_to|setgid|setuid)\\?/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(size|socket|sticky|stop|symlink|tainted|tty|value|writable|writable_real)\\?/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(capitalize|chomp|chop|collect|downcase|flatten|gsub)\\!/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/(slice|squeeze|strip|succ|swapcase|tr_s|uniq|upcase)\\!/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD2, L"/File\\:\\:(LOCK_EX|LOCK_NB|LOCK_SH|LOCK_UN|SEEK_SET|SEEK_CUR|SEEK_END|Separator)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD2, L"/Math\\:\\:(E|PI)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/Class\\.(nesting|new)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/File\\.(lstat|mtime|open|readlink|rename|size|split|stat|symlink|truncate|umask|unlink|utime)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/File\\.(atime|basename|chmod|chown|ctime|delete|dirname|expand_path|foreach|ftype|join|link)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/IO\\.(new|pipe|popen|readlines|select)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/Thread\\.(abort_on_exception|critical|current|exit|fork|kill|main|new|pass|start|stop)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/Time\\.(at|gm|local|mktime|new|now|times)/k" );
	RegexAdd( pType, keywordPos, idx++, COLORIDX_KEYWORD4, L"/Dir\\.(chdir|chroot|delete|entries|foreach|getwd|glob|mkdir|open|pwd|rmdir|unlink)/k" );
}



