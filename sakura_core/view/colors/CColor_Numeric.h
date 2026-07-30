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
#ifndef SAKURA_CCOLOR_NUMERIC_19741AD7_75D8_455C_9E04_7A8901616E8B_H_
#define SAKURA_CCOLOR_NUMERIC_19741AD7_75D8_455C_9E04_7A8901616E8B_H_

#include "view/colors/CColorStrategy.h"

#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
//! タイプ別の数値リテラル専用判定を切り替えるための識別子
//  (対応タイプが増えたらここに追加していく。未対応タイプはENUMLANG_GENERICのまま)
enum ENumericLang{
	ENUMLANG_GENERIC = 0,	//!< 専用実装なし。IsNumber()のみ
	ENUMLANG_CPP,
	ENUMLANG_JAVA,
	ENUMLANG_CSHARP,
	ENUMLANG_JAVASCRIPT,
	ENUMLANG_PHP,
	ENUMLANG_PYTHON,
	ENUMLANG_RUBY,
	ENUMLANG_PERL,
	ENUMLANG_VB,
	ENUMLANG_PASCAL,
	ENUMLANG_CSS,
	ENUMLANG_ASM,
	// リッチテキスト(RTF)は専用実装なし。制御ワードの数値パラメータは
	// 常に「符号+10進整数」のみで、桁区切り・進数接頭辞・サフィックスの
	// 概念が一切ないため、IsNumber()だけで過不足なく判定できる
};
#endif // NKMM_

class CColor_Numeric : public CColorStrategy{
public:
	CColor_Numeric() : m_nCOMMENTEND(0)
#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
		, m_eLang(ENUMLANG_GENERIC)
#endif // NKMM_
	{ }
#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
	//! タイプ切り替え時に、現在のタイプに対応する専用実装があるかをキャッシュしておく
	//  (BeginColor()は高頻度で呼ばれるため、その都度タイプ名を比較しない)
	virtual void Update(void)
	{
		CColorStrategy::Update();
		const TCHAR* name = m_pTypeData->m_szTypeName;
		if(      _tcscmp( name, _T("C/C++") )      == 0 ) m_eLang = ENUMLANG_CPP;
		else if( _tcscmp( name, _T("Java") )       == 0 ) m_eLang = ENUMLANG_JAVA;
		else if( _tcscmp( name, _T("C#") )         == 0 ) m_eLang = ENUMLANG_CSHARP;
		else if( _tcscmp( name, _T("JavaScript") ) == 0 ) m_eLang = ENUMLANG_JAVASCRIPT;
		else if( _tcscmp( name, _T("PHP") )        == 0 ) m_eLang = ENUMLANG_PHP;
		else if( _tcscmp( name, _T("Python") )     == 0 ) m_eLang = ENUMLANG_PYTHON;
		else if( _tcscmp( name, _T("Ruby") )       == 0 ) m_eLang = ENUMLANG_RUBY;
		else if( _tcscmp( name, _T("Perl") )       == 0 ) m_eLang = ENUMLANG_PERL;
		else if( _tcscmp( name, _T("Visual Basic") ) == 0 ) m_eLang = ENUMLANG_VB;
		else if( _tcscmp( name, _T("Pascal") )     == 0 ) m_eLang = ENUMLANG_PASCAL;
		else if( _tcscmp( name, _T("CSS") )        == 0 ) m_eLang = ENUMLANG_CSS;
		else if( _tcscmp( name, _T("アセンブラ") ) == 0 ) m_eLang = ENUMLANG_ASM;
		else                                               m_eLang = ENUMLANG_GENERIC;
	}
#endif // NKMM_
	virtual EColorIndexType GetStrategyColor() const{ return COLORIDX_DIGIT; }
	virtual void InitStrategyStatus(){ m_nCOMMENTEND = 0; }
	virtual bool BeginColor(const CStringRef& cStr, int nPos);
	virtual bool EndColor(const CStringRef& cStr, int nPos);
	virtual bool Disp() const { return m_pTypeData->m_ColorInfoArr[COLORIDX_DIGIT].m_bDisp; }
private:
	int m_nCOMMENTEND;
#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
	ENumericLang m_eLang;	//!< 現在のタイプに対応する専用IsNumberXxx()の種別
#endif // NKMM_
};

#endif /* SAKURA_CCOLOR_NUMERIC_19741AD7_75D8_455C_9E04_7A8901616E8B_H_ */
/*[EOF]*/
