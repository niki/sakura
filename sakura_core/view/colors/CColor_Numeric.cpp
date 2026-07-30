#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CColor_Numeric.h"
#include "parse/CWordParse.h"
#include "util/string_ex2.h"
#include "doc/layout/CLayout.h"
#include "types/CTypeSupport.h"
#ifdef NKMM_FIX_NUMERIC_COLOR
#define REGEX_MODE (3)  // 0:std::regex
                        // 1:boost::regex
                        // 2:BREGEXP
                        // 3:PCRE2 (要NKMM_FIX_REGEXP_FALLBACK。bregonig.dllの
                        //   有無に関わらず常時使用可能。20260720追加)
#if REGEX_MODE == 0
	#include <regex>
	using namespace std;
#elif REGEX_MODE == 1
	#pragma comment(lib, "libboost_regex.lib")
	#include <boost/regex.hpp>
	using namespace boost;
#elif REGEX_MODE == 2
	#include "window/CEditWnd.h"
#elif REGEX_MODE == 3
	#ifdef NKMM_FIX_REGEXP_FALLBACK
		#include "extmodule/CRegexFallback.h"
	#else
		#error REGEX_MODE == 3 (PCRE2) を使うには NKMM_FIX_REGEXP_FALLBACK の定義が必要です
	#endif
#endif
#endif // NKMM_

static int IsNumber( const CStringRef& cStr, int offset );/* 数値ならその長さを返す */	//@@@ 2001.02.17 by MIK
#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
/* タイプ別の数値リテラル専用判定。共通部分はIsNumber()に委譲する */
static int IsNumberCpp( const CStringRef& cStr, int offset );
static int IsNumberJava( const CStringRef& cStr, int offset );
static int IsNumberCsharp( const CStringRef& cStr, int offset );
static int IsNumberJavaScript( const CStringRef& cStr, int offset );
static int IsNumberPhp( const CStringRef& cStr, int offset );
static int IsNumberPython( const CStringRef& cStr, int offset );
static int IsNumberRuby( const CStringRef& cStr, int offset );
static int IsNumberPerl( const CStringRef& cStr, int offset );
static int IsNumberVb( const CStringRef& cStr, int offset );
static int IsNumberPascal( const CStringRef& cStr, int offset );
static int IsNumberCss( const CStringRef& cStr, int offset );
static int IsNumberAsm( const CStringRef& cStr, int offset );
#endif // NKMM_

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         半角数値                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColor_Numeric::BeginColor(const CStringRef& cStr, int nPos)
{
	if(!cStr.IsValid())return false;

	int	nnn;

#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
	int (*pfnIsNumber)(const CStringRef&, int) = IsNumber;
	switch( m_eLang )
	{
	case ENUMLANG_CPP:        pfnIsNumber = IsNumberCpp;        break;
	case ENUMLANG_JAVA:       pfnIsNumber = IsNumberJava;       break;
	case ENUMLANG_CSHARP:     pfnIsNumber = IsNumberCsharp;     break;
	case ENUMLANG_JAVASCRIPT: pfnIsNumber = IsNumberJavaScript; break;
	case ENUMLANG_PHP:        pfnIsNumber = IsNumberPhp;        break;
	case ENUMLANG_PYTHON:     pfnIsNumber = IsNumberPython;     break;
	case ENUMLANG_RUBY:       pfnIsNumber = IsNumberRuby;       break;
	case ENUMLANG_PERL:       pfnIsNumber = IsNumberPerl;       break;
	case ENUMLANG_VB:         pfnIsNumber = IsNumberVb;         break;
	case ENUMLANG_PASCAL:     pfnIsNumber = IsNumberPascal;     break;
	case ENUMLANG_CSS:        pfnIsNumber = IsNumberCss;        break;
	case ENUMLANG_ASM:        pfnIsNumber = IsNumberAsm;        break;
	default: break;	/* ENUMLANG_GENERIC: 専用実装なし。IsNumber()のまま(リッチテキスト等) */
	}
	if( _IsPosKeywordHead(cStr,nPos)
		&& (nnn = pfnIsNumber(cStr, nPos)) > 0 )		/* 半角数字を表示する */
#else
	if( _IsPosKeywordHead(cStr,nPos)
		&& (nnn = IsNumber(cStr, nPos)) > 0 )		/* 半角数字を表示する */
#endif // NKMM_
	{
		/* キーワード文字列の終端をセットする */
		this->m_nCOMMENTEND = nPos + nnn;
		return true;	/* 半角数値である */ // 2002/03/13 novice
	}
	return false;
}


bool CColor_Numeric::EndColor(const CStringRef& cStr, int nPos)
{
	if( nPos == this->m_nCOMMENTEND ){
		return true;
	}
	return false;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         実装補助                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

//@@@ 2001.11.07 Start by MIK
/*
 * 数値なら長さを返す。
 * 10進数の整数または小数。16進数(正数)。
 * 文字列   数値(色分け)
 * ---------------------
 * 123      123
 * 0123     0123
 * 0xfedc   0xfedc
 * -123     -123
 * &H9a     &H9a     (ただしソース中の#ifを有効にしたとき)
 * -0x89a   0x89a
 * 0.5      0.5
 * 0.56.1   0.56 , 1 (ただしソース中の#ifを有効にしたら"0.56.1"になる)
 * .5       5        (ただしソース中の#ifを有効にしたら".5"になる)
 * -.5      5        (ただしソース中の#ifを有効にしたら"-.5"になる)
 * 123.     123
 * 0x567.8  0x567 , 8
 */
/*
 * 半角数値
 *   1, 1.2, 1.2.3, .1, 0xabc, 1L, 1F, 1.2f, 0x1L, 0x2F, -.1, -1, 1e2, 1.2e+3, 1.2e-3, -1e0
 *   10進数, 16進数, LF接尾語, 浮動小数点数, 負符号
 *   IPアドレスのドット連結(本当は数値じゃないんだよね)
 */
static int IsNumber(const CStringRef& cStr,/*const wchar_t *buf,*/ int offset/*, int length*/)
{
#ifdef NKMM_FIX_NUMERIC_COLOR
	const wchar_t *p2 = cStr.GetPtr() + offset;
	const wchar_t *q2 = cStr.GetPtr() + cStr.GetLength();

#if REGEX_MODE == 0  // std::regex
	using _regex = wregex;                                     // 照合パターン
	using _match = wcmatch;                                    // 文字列のパターン マッチング
	#define _REG_IS_AVAILABLE() (1)                            // 正規表現が有効か
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))     // パターンマッチングを行うか
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            regex_search(p, q, match, pt)  // 正規表現がマッチする部分が存在するか
	#define _REG_STARTP(p)      (0)                            // 文字列の先頭位置
	#define _REG_ENDP(match)    match.length(0)                // 文字列の終端位置
	#define _REG_INIT(p)        
	#define _REG_FREE(p)        
	#define PREFIX                                             // 正規表現文字列に付加するプリフィックス
	#define SUFIX                                              // 正規表現文字列に付加するサフィックス
	#define REGSTR(x)           L##x                           // 文字列型
	#define REGEX(x)            _regex(REGSTR(x))              // 正規表現ライブラリが扱う型
#elif REGEX_MODE == 1  // boost::regex
	using _regex = wregex;
	using _match = wcmatch;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            regex_search(p, q, match, pt)
	#define _REG_STARTP(p)      (0)
	#define _REG_ENDP(match)    match.length(0)
	#define _REG_INIT(p)        
	#define _REG_FREE(p)        
	#define PREFIX              
	#define SUFIX               
	#define REGSTR(x)           L##x
	#define REGEX(x)            _regex(REGSTR(x))
#elif REGEX_MODE == 2  // BREGEXP
	using _regex = std::wstring;
	using _match = BREGEXP_W*;
	#define _REG_IS_AVAILABLE() CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.IsAvailable()
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.BMatch(pt.c_str(), p, q, &(match), msg)
	#define _REG_STARTP(p)      p
	#define _REG_ENDP(match)    match->endp[0]
	#define _REG_INIT(p)        p = nullptr
	#define _REG_FREE(p)        if (p) { CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.BRegfree(p); }
	#define PREFIX              "/"
	#define SUFIX               "/k"
	#define REGSTR(x)           L"" PREFIX ##x SUFIX
	#define REGEX(x)            _regex(REGSTR(x))
#elif REGEX_MODE == 3  // PCRE2 (bregonig.dllの有無に関わらず常時利用可能。20260720追加)
	using _regex = std::wstring;
	using _match = BREGEXP_W*;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            RegexFallback::BMatch(pt.c_str(), p, q, &(match), msg)
	#define _REG_STARTP(p)      p
	#define _REG_ENDP(match)    match->endp[0]
	#define _REG_INIT(p)        p = nullptr
	#define _REG_FREE(p)        if (p) { RegexFallback::BRegfree(p); }
	#define PREFIX              "/"
	#define SUFIX               "/k"
	#define REGSTR(x)           L"" PREFIX ##x SUFIX
	#define REGEX(x)            _regex(REGSTR(x))
#else  // std::regex
	static_assert(0);
#endif

	static const struct {
		wchar_t enter; // 最低条件
		bool    term;  // 検索グループの終端
		_regex  exp;   // 式
	} sPattern[] = {
		{L'e', false, REGEX("^[0-9]+\\.[0-9]*([eE][-+][0-9]+)([fF]?)")},  // 1e-2
		{L'e', true,  REGEX("^(\\.[0-9]+)([eE][-+][0-9]+)([fF]?)")},      // .12e+2
		
		{L'.', false, REGEX("^([0-9]+\\.[0-9]*)([fF]?)")},                // 1.0f 1.f 1.
		{L'.', true,  REGEX("^(\\.[0-9]+)([fF]?)")},                      // .1f .1
		
		{0,    false, REGEX("^0x[0-9a-fA-F]+")},                          // 0x123
		{0,    true,  REGEX("^[0-9]+([uUlL]{0,2})")},                     // 123
	};

	if (_REG_IS_AVAILABLE()) {
		int pos = 0;
		wchar_t szMsg[80] = {}; //!< エラーメッセージ
		_match match;
		_REG_INIT(match);

		for (auto && re : sPattern) {
			if (_REG_ENTRY(p2, re.enter)) {
				if (_REG_SEARCH(re.exp, p2, q2, match, szMsg)) {
					pos = std::max<int>(_REG_ENDP(match) - _REG_STARTP(p2), pos);
				}
				if (re.term) {
					if (pos > 0) break;
				}
			}
		}

		_REG_FREE(match);
		return pos;
	} else {
		// 正規表現ライブラリが読み込まれていない
		// xxx そのまま通常の方法で判定する
		return 0;
	}
#else

	const wchar_t* p;
	const wchar_t* q;
	int i = 0;
	int d = 0;
	int f = 0;

	p = cStr.GetPtr() + offset;
	q = cStr.GetPtr() + cStr.GetLength();

	if( *p == L'0' )  /* 10進数,Cの16進数 */
	{
		p++; i++;
		if( ( p < q ) && ( *p == L'x' ) )  /* Cの16進数 */
		{
			p++; i++;
			while( p < q )
			{
				if( ( *p >= L'0' && *p <= L'9' )
				 || ( *p >= L'A' && *p <= L'F' )
				 || ( *p >= L'a' && *p <= L'f' ) )
				{
					p++; i++;
				}
				else
				{
					break;
				}
			}
			/* "0x" なら "0" だけが数値 */
			if( i == 2 ) return 1;
			
			/* 接尾語 */
			if( p < q )
			{
				if( *p == L'L' || *p == L'l' || *p == L'F' || *p == L'f' )
				{
					p++; i++;
				}
			}
			return i;
		}
		else if( *p >= L'0' && *p <= L'9' )
		{
			p++; i++;
			while( p < q )
			{
				if( *p < L'0' || *p > L'9' )
				{
					if( *p == L'.' )
					{
						if( f == 1 ) break;  /* 指数部に入っている */
						d++;
						if( d > 1 )
						{
							if( *(p - 1) == L'.' ) break;  /* "." が連続なら中断 */
						}
					}
					else if( *p == L'E' || *p == L'e' )
					{
						if( f == 1 ) break;  /* 指数部に入っている */
						if( p + 2 < q )
						{
							if( ( *(p + 1) == L'+' || *(p + 1) == L'-' )
							 && ( *(p + 2) >= L'0' && *(p + 2) <= L'9' ) )
							{
								p++; i++;
								p++; i++;
								f = 1;
							}
							else if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
							{
								p++; i++;
								f = 1;
							}
							else
							{
								break;
							}
						}
						else if( p + 1 < q )
						{
							if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
							{
								p++; i++;
								f = 1;
							}
							else
							{
								break;
							}
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
				p++; i++;
			}
			if( *(p - 1)  == L'.' ) return i - 1;  /* 最後が "." なら含めない */
			/* 接尾語 */
			if( p < q )
			{
				if( (( d == 0 ) && ( *p == L'L' || *p == L'l' ))
				 || *p == L'F' || *p == L'f' )
				{
					p++; i++;
				}
			}
			return i;
		}
		else if( *p == L'.' )
		{
			while( p < q )
			{
				if( *p < L'0' || *p > L'9' )
				{
					if( *p == L'.' )
					{
						if( f == 1 ) break;  /* 指数部に入っている */
						d++;
						if( d > 1 )
						{
							if( *(p - 1) == L'.' ) break;  /* "." が連続なら中断 */
						}
					}
					else if( *p == L'E' || *p == L'e' )
					{
						if( f == 1 ) break;  /* 指数部に入っている */
						if( p + 2 < q )
						{
							if( ( *(p + 1) == L'+' || *(p + 1) == L'-' )
							 && ( *(p + 2) >= L'0' && *(p + 2) <= L'9' ) )
							{
								p++; i++;
								p++; i++;
								f = 1;
							}
							else if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
							{
								p++; i++;
								f = 1;
							}
							else
							{
								break;
							}
						}
						else if( p + 1 < q )
						{
							if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
							{
								p++; i++;
								f = 1;
							}
							else
							{
								break;
							}
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
				p++; i++;
			}
			if( *(p - 1)  == L'.' ) return i - 1;  /* 最後が "." なら含めない */
			/* 接尾語 */
			if( p < q )
			{
				if( *p == L'F' || *p == L'f' )
				{
					p++; i++;
				}
			}
			return i;
		}
		else if( *p == L'E' || *p == L'e' )
		{
			p++; i++;
			while( p < q )
			{
				if( *p < L'0' || *p > L'9' )
				{
					if( ( *p == L'+' || *p == L'-' ) && ( *(p - 1) == L'E' || *(p - 1) == L'e' ) )
					{
						if( p + 1 < q )
						{
							if( *(p + 1) < L'0' || *(p + 1) > L'9' )
							{
								/* "0E+", "0E-" */
								break;
							}
						}
						else
						{
							/* "0E-", "0E+" */
							break;
						}
					}
					else
					{
						break;
					}
				}
				p++; i++;
			}
			if( i == 2 ) return 1;  /* "0E", 0e" なら "0" が数値 */
			/* 接尾語 */
			if( p < q )
			{
				if( (( d == 0 ) && ( *p == L'L' || *p == L'l' ))
				 || *p == L'F' || *p == L'f' )
				{
					p++; i++;
				}
			}
			return i;
		}
		else
		{
			/* "0" だけが数値 */
			/*if( *p == L'.' ) return i - 1;*/  /* 最後が "." なら含めない */
			if( p < q )
			{
				if( (( d == 0 ) && ( *p == L'L' || *p == L'l' ))
				 || *p == L'F' || *p == L'f' )
				{
					p++; i++;
				}
			}
			return i;
		}
	}

	else if( *p >= L'1' && *p <= L'9' )  /* 10進数 */
	{
		p++; i++;
		while( p < q )
		{
			if( *p < L'0' || *p > L'9' )
			{
				if( *p == L'.' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					d++;
					if( d > 1 )
					{
						if( *(p - 1) == L'.' ) break;  /* "." が連続なら中断 */
					}
				}
				else if( *p == L'E' || *p == L'e' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					if( p + 2 < q )
					{
						if( ( *(p + 1) == L'+' || *(p + 1) == L'-' )
						 && ( *(p + 2) >= L'0' && *(p + 2) <= L'9' ) )
						{
							p++; i++;
							p++; i++;
							f = 1;
						}
						else if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else if( p + 1 < q )
					{
						if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			p++; i++;
		}
		if( *(p - 1) == L'.' ) return i - 1;  /* 最後が "." なら含めない */
		/* 接尾語 */
		if( p < q )
		{
			if( (( d == 0 ) && ( *p == L'L' || *p == L'l' ))
			 || *p == L'F' || *p == L'f' )
			{
				p++; i++;
			}
		}
		return i;
	}

	else if( *p == L'-' )  /* マイナス */
	{
		p++; i++;
		while( p < q )
		{
			if( *p < L'0' || *p > L'9' )
			{
				if( *p == L'.' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					d++;
					if( d > 1 )
					{
						if( *(p - 1) == L'.' ) break;  /* "." が連続なら中断 */
					}
				}
				else if( *p == L'E' || *p == L'e' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					if( p + 2 < q )
					{
						if( ( *(p + 1) == L'+' || *(p + 1) == L'-' )
						 && ( *(p + 2) >= L'0' && *(p + 2) <= L'9' ) )
						{
							p++; i++;
							p++; i++;
							f = 1;
						}
						else if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else if( p + 1 < q )
					{
						if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			p++; i++;
		}
		/* "-", "-." だけなら数値でない */
		//@@@ 2001.11.09 start MIK
		//if( i <= 2 ) return 0;
		//if( *(p - 1)  == L'.' ) return i - 1;  /* 最後が "." なら含めない */
		if( i == 1 ) return 0;
		if( *(p - 1) == L'.' )
		{
			i--;
			if( i == 1 ) return 0;
			return i;
		}  //@@@ 2001.11.09 end MIK
		/* 接尾語 */
		if( p < q )
		{
			if( (( d == 0 ) && ( *p == L'L' || *p == L'l' ))
			 || *p == L'F' || *p == L'f' )
			{
				p++; i++;
			}
		}
		return i;
	}

	else if( *p == L'.' )  /* 小数点 */
	{
		d++;
		p++; i++;
		while( p < q )
		{
			if( *p < L'0' || *p > L'9' )
			{
				if( *p == L'.' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					d++;
					if( d > 1 )
					{
						if( *(p - 1) == L'.' ) break;  /* "." が連続なら中断 */
					}
				}
				else if( *p == L'E' || *p == L'e' )
				{
					if( f == 1 ) break;  /* 指数部に入っている */
					if( p + 2 < q )
					{
						if( ( *(p + 1) == L'+' || *(p + 1) == L'-' )
						 && ( *(p + 2) >= L'0' && *(p + 2) <= L'9' ) )
						{
							p++; i++;
							p++; i++;
							f = 1;
						}
						else if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else if( p + 1 < q )
					{
						if( *(p + 1) >= L'0' && *(p + 1) <= L'9' )
						{
							p++; i++;
							f = 1;
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			p++; i++;
		}
		/* "." だけなら数値でない */
		if( i == 1 ) return 0;
		if( *(p - 1)  == L'.' ) return i - 1;  /* 最後が "." なら含めない */
		/* 接尾語 */
		if( p < q )
		{
			if( *p == L'F' || *p == L'f' )
			{
				p++; i++;
			}
		}
		return i;
	}

#if 0
	else if( *p == L'&' )  /* VBの16進数 */
	{
		p++; i++;
		if( ( p < q ) && ( *p == L'H' ) )
		{
			p++; i++;
			while( p < q )
			{
				if( ( *p >= L'0' && *p <= L'9' )
				 || ( *p >= L'A' && *p <= L'F' )
				 || ( *p >= L'a' && *p <= L'f' ) )
				{
					p++; i++;
				}
				else
				{
					break;
				}
			}
			/* "&H" だけなら数値でない */
			if( i == 2 ) i = 0;
			return i;
		}

		/* "&" だけなら数値でない */
		return 0;
	}
#endif

	/* 数値ではない */
	return 0;
#endif // NKMM_
}
//@@@ 2001.11.07 End by MIK

#ifdef NKMM_FIX_NUMERIC_LANG_LITERAL
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//              タイプ別の数値リテラル専用判定 20260730          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//
// IsNumber()は10進/16進/浮動小数点の基本形しか扱わない、全タイプ共通の
// 汎用実装。ここでは各言語固有の記法(2進数、桁区切り記号、サフィックス等)を
// タイプ別に補う。共通部分はIsNumber()の結果に加算する形で委譲し、
// IsNumber()自体の実装(REGEX_MODE込み)には手を入れない。
// (CColor_Numeric::BeginColor()が現在のタイプに応じてIsNumberXxx()を呼び分ける。
//  未対応のタイプは従来通りIsNumber()のみが使われる)
//
// 対応済み: C/C++, Java, C#, JavaScript, PHP, Python, Ruby, Perl,
//           Visual Basic, Pascal, CSS, アセンブラ(MASM系)
// 対応見送り: リッチテキスト(RTF)。制御ワードの数値パラメータは常に
//   「符号+10進整数」のみで、桁区切り・進数接頭辞・サフィックスの概念が
//   一切ないため、IsNumber()だけで過不足なく判定できる(専用実装は不要)
// 対象外(既知の未対応、必要になったら別途拡張):
//   - 桁区切り記号が小数点をまたぐ場合(例 1'234.5, 1_234.5)。整数部が
//     桁区切りで伸ばされた直後に小数点が来ると、そこで走査が止まり
//     小数部が色付けされない。整数部/16進数字部内で完結する桁区切りは動く
//   - マイナス符号付き16進数/2進数(例 -0x89a, -0b101, VBの-&HFF)。
//     IsNumber()の'-'分岐が元々0x/0b/&等を認識しないため、"-0"だけが
//     誤って数値扱いされる問題は全言語共通で温存される
//   - C++のi64サフィックス(MS独自拡張、非推奨のため対象外)
//   - サフィックスを持たない言語(JavaScript, PHP, Perl, Pascal)でも、
//     IsNumber()自身は常にL/l/F/fサフィックスを1文字消費してしまう
//     (例JSの"1F")。該当言語には元々存在しない記法であり、そのような
//     コードが実質存在しないため実害はない想定。IsNumber()自体を
//     無改造で使い回す設計上のトレードオフ
//   - VB.NET固有のS/I/L/D/F/Rおよびアンダースコア付きサフィックス(US/UI/UL)。
//     VB6/VBA/VB.NETの方言差が大きいため、共通の記号サフィックス
//     (%&@!#)のみに絞った
//   - アセンブラの浮動小数点定数、NASM/GAS等MASM以外の方言

//! 基数nBaseの数字なら true (nBase: 2, 8, 10, 16 のいずれか)
static bool IsDigitOfBase(wchar_t c, int nBase)
{
	if( nBase == 16 ) return (c>=L'0'&&c<=L'9') || (c>=L'a'&&c<=L'f') || (c>=L'A'&&c<=L'F');
	if( nBase == 8  ) return (c>=L'0'&&c<=L'7');
	if( nBase == 2  ) return (c==L'0'||c==L'1');
	return (c>=L'0'&&c<=L'9');
}

//! 桁区切り記号chSepを考慮しつつ、基数nBaseの数字が続く限り読み進めた長さを返す
//  (区切り記号の直後が数字でない場合は桁区切りとみなさず、そこで止める)
static int SkipDigitsWithSeparator(const wchar_t* p, const wchar_t* q, int nBase, wchar_t chSep)
{
	int n = 0;
	while( p + n < q )
	{
		if( IsDigitOfBase(p[n], nBase) ){ n++; continue; }
		if( p[n] == chSep && p + n + 1 < q && IsDigitOfBase(p[n + 1], nBase) ){ n += 2; continue; }
		break;
	}
	return n;
}

//! 整数サフィックス(u/U, l/L の任意の組み合わせ)を最大nMaxLen文字まで読み進めた長さを返す
static int SkipIntSuffix(const wchar_t* p, const wchar_t* q, int nMaxLen)
{
	int n = 0;
	while( p + n < q && n < nMaxLen )
	{
		wchar_t c = p[n];
		if( c==L'u' || c==L'U' || c==L'l' || c==L'L' ){ n++; continue; }
		break;
	}
	return n;
}

//! chSet中のいずれか1文字に一致すれば1、しなければ0を返す(単独サフィックス用)
static int SkipOneCharOf(const wchar_t* p, const wchar_t* q, const wchar_t* chSet)
{
	if( p < q && wcschr(chSet, *p) ) return 1;
	return 0;
}

//! [p,p+n)の範囲に'.'または指数記号(e/E)が含まれるか(浮動小数点形かどうかの判定用)
static bool ContainsFloatMarker(const wchar_t* p, int n)
{
	for( int i = 0; i < n; i++ )
	{
		if( p[i]==L'.' || p[i]==L'e' || p[i]==L'E' ) return true;
	}
	return false;
}

//! chFirst+chLower/chUpper+基数nBaseの数字列(例 0b1010, 0o777, &HFF, $FF)なら
//  長さを返す。数値でなければ0。chFirstは接頭辞の1文字目(C系は'0'、VBは'&'、
//  Pascalは進数ごとに'$'/'&'/'%'など)
static int TryPrefixedLiteral(const wchar_t* p, const wchar_t* q, wchar_t chFirst, wchar_t chLower, wchar_t chUpper, int nBase, wchar_t chSep)
{
	if( q - p < 3 ) return 0;	// 接頭辞2文字 + 最低1桁は必要
	if( p[0] != chFirst || (p[1] != chLower && p[1] != chUpper) ) return 0;
	int nDigits = SkipDigitsWithSeparator(p + 2, q, nBase, chSep);
	if( nDigits == 0 ) return 0;	// 接頭辞だけでは数値でない
	return 2 + nDigits;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                            C/C++                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b.../0B... (C++14)
//   - 桁区切り記号 ' (C++14。例 36'000'000, 0x12'34'56'78)
//   - 整数サフィックス u/U, l/L, ll/LL の任意の組み合わせ(最大3文字)
//     (IsNumber()はd(小数点の有無)==0のときL/lを1文字だけ、F/fを常に1文字だけ
//      しか消費しないため、123ULLやuU系、1.5Lのようなlong doubleサフィックスは
//      取りこぼす)
static int IsNumberCpp(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	// 2進数リテラルはIsNumber()では扱えないため専用に判定する
	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'\'');
	if( nBin > 0 )
	{
		return nBin + SkipIntSuffix(p + nBin, q, 3);
	}

	// それ以外(10進/16進/浮動小数点)の基本形はIsNumber()に委譲する
	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	// 桁区切り記号での延長。0x/0Xで始まる場合は16進数字、それ以外は10進数字とみなす
	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'\'');

	// 整数/long doubleサフィックスでの延長(IsNumber()が取りこぼす分を補う)
	n += SkipIntSuffix(p + n, q, 3);

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                             Java                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b.../0B... (Java 7+)
//   - 桁区切り記号 _ (Java 7+。例 1_000_000)
//   - long整数サフィックス L/l は2進数リテラルの後ろにも付けられる(0b1010L)
//   - double型サフィックス D/d (IsNumber()はF/fしか知らない)
// JavaにはC++のu/U、複数文字サフィックスに相当するものはないため対象外
static int IsNumberJava(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 )
	{
		return nBin + SkipOneCharOf(p + nBin, q, L"Ll");
	}

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	// L/lはIsNumber()が(小数点なしのときのみ)既に消費済み。ここではD/dだけ補う
	n += SkipOneCharOf(p + n, q, L"Dd");

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                              C#                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b.../0B... (C# 7.0+)
//   - 桁区切り記号 _ (C# 7.0+。例 1_000_000)
//   - 整数サフィックス u/U, l/L の組み合わせ(最大2文字。C++と違いllの重複はない)
//   - 浮動小数点サフィックス d/D(double), m/M(decimal)。f/FはIsNumber()側で消費済み
static int IsNumberCsharp(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 )
	{
		return nBin + SkipIntSuffix(p + nBin, q, 2);
	}

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	if( ContainsFloatMarker(p, n) )
	{
		n += SkipOneCharOf(p + n, q, L"DdMm");
	}
	else
	{
		n += SkipIntSuffix(p + n, q, 2);
	}

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          JavaScript                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b.../0B..., 8進数リテラル 0o.../0O... (ES2015+)
//     (IsNumber()は0xしか知らないため両方とも専用に判定する)
//   - 桁区切り記号 _ (ES2021+。例 1_000_000)
//   - BigIntサフィックス n (ES2020+。例 123n, 0x1Fn, 0b101n)
// JavaScriptにu/l/f/d/m等のサフィックスは存在しない
static int IsNumberJavaScript(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 ) return nBin + SkipOneCharOf(p + nBin, q, L"n");

	int nOct = TryPrefixedLiteral(p, q, L'0', L'o', L'O', 8, L'_');
	if( nOct > 0 ) return nOct + SkipOneCharOf(p + nOct, q, L"n");

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	n += SkipOneCharOf(p + n, q, L"n");

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                             PHP                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b/0B (PHP 5.4+)、8進数リテラル 0o/0O (PHP 8.1+。
//     従来の0777形式はIsNumber()がそのまま処理する)
//   - 桁区切り記号 _ (PHP 7.4+)
// PHPに数値サフィックスは存在しない
static int IsNumberPhp(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 ) return nBin;

	int nOct = TryPrefixedLiteral(p, q, L'0', L'o', L'O', 8, L'_');
	if( nOct > 0 ) return nOct;

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                            Python                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b/0B、8進数リテラル 0o/0O (Python 3では暗黙の0777形式が
//     廃止され明示的な接頭辞が必須。IsNumber()自体は0777もそのまま処理する)
//   - 桁区切り記号 _ (Python 3.6+)
//   - 複素数サフィックス j/J (例 3.14j, 10j)
static int IsNumberPython(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 ) return nBin;

	int nOct = TryPrefixedLiteral(p, q, L'0', L'o', L'O', 8, L'_');
	if( nOct > 0 ) return nOct;

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	n += SkipOneCharOf(p + n, q, L"jJ");

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                             Ruby                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b/0B、8進数リテラル 0o/0O (従来の0777形式もIsNumber()が処理)
//   - 桁区切り記号 _
//   - 有理数サフィックス r、虚数サフィックス i (常にr→iの順。例 1r, 1i, 1ri)
static int SkipRubyNumericSuffix(const wchar_t* p, const wchar_t* q)
{
	int n = SkipOneCharOf(p, q, L"r");
	n += SkipOneCharOf(p + n, q, L"i");
	return n;
}

static int IsNumberRuby(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 ) return nBin + SkipRubyNumericSuffix(p + nBin, q);

	int nOct = TryPrefixedLiteral(p, q, L'0', L'o', L'O', 8, L'_');
	if( nOct > 0 ) return nOct + SkipRubyNumericSuffix(p + nOct, q);

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	n += SkipRubyNumericSuffix(p + n, q);

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                             Perl                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 2進数リテラル 0b/0B、8進数リテラル 0o/0O (Perl 5.34+。従来の0777形式も
//     IsNumber()がそのまま処理する)
//   - 桁区切り記号 _
// Perlに数値サフィックスは存在しない
static int IsNumberPerl(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nBin = TryPrefixedLiteral(p, q, L'0', L'b', L'B', 2, L'_');
	if( nBin > 0 ) return nBin;

	int nOct = TryPrefixedLiteral(p, q, L'0', L'o', L'O', 8, L'_');
	if( nOct > 0 ) return nOct;

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	bool bHex = (q - p >= 2) && p[0] == L'0' && (p[1] == L'x' || p[1] == L'X');
	n += SkipDigitsWithSeparator(p + n, q, bHex ? 16 : 10, L'_');

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                        Visual Basic                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 16進数 &HFF、8進数 &O17 (VB6/VBA/VB.NET共通)、2進数 &B1010 (VB.NET 15+)
//     (IsNumber()は0x接頭辞方式しか知らず、&接頭辞方式は判定できない)
//   - 型宣言文字(整数系): %(Integer) &(Long) @(Decimal) !(Single) #(Double)
//     (VB6/VBA/VB.NET共通の記号サフィックス。単独1文字のみ)
// VBの"_"は行継続文字であり桁区切り記号ではないため、桁区切りには対応しない
// (SkipDigitsWithSeparator呼び出し時はL'\0'を渡し、区切り記号なしにしている)。
// また、VB.NETのS/I/L/D/F/Rおよびアンダースコア付きサフィックス(US/UI/UL)は
// バージョン依存が大きいため今回は対象外
static int IsNumberVb(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int nHex = TryPrefixedLiteral(p, q, L'&', L'h', L'H', 16, L'\0');
	if( nHex > 0 ) return nHex;

	int nOct = TryPrefixedLiteral(p, q, L'&', L'o', L'O', 8, L'\0');
	if( nOct > 0 ) return nOct;

	int nBin = TryPrefixedLiteral(p, q, L'&', L'b', L'B', 2, L'\0');
	if( nBin > 0 ) return nBin;

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	n += SkipOneCharOf(p + n, q, L"%&@!#");

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                            Pascal                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法(Delphi/FreePascal系):
//   - 16進数 $FF (IsNumber()は0x接頭辞方式しか知らないため専用に判定)
//   - 8進数 &17、2進数 %1010 (FreePascal拡張)
//   - 桁区切り記号 _ (FreePascal 3.2+の拡張。Turbo Pascal/Delphi古い版にはない)
// Pascalに数値サフィックスは存在しない
static int IsNumberPascal(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	if( p < q && *p == L'$' )
	{
		int nDigits = SkipDigitsWithSeparator(p + 1, q, 16, L'_');
		if( nDigits > 0 ) return 1 + nDigits;
	}
	if( p < q && *p == L'&' )
	{
		int nDigits = SkipDigitsWithSeparator(p + 1, q, 8, L'_');
		if( nDigits > 0 ) return 1 + nDigits;
	}
	if( p < q && *p == L'%' )
	{
		int nDigits = SkipDigitsWithSeparator(p + 1, q, 2, L'_');
		if( nDigits > 0 ) return 1 + nDigits;
	}

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	n += SkipDigitsWithSeparator(p + n, q, 10, L'_');

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                             CSS                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// 補う記法:
//   - 数値直後の単位(dimension token。例 10px, 1.5em, 100%)をまとめて
//     色付けする。単位名を網羅的に列挙する代わりに「英字の並び、または
//     単独の%」を単位とみなす(CSSでは新しい単位が追加され続けるため)
// CSSに2進/16進/8進リテラルや桁区切り記号は存在しない。#RRGGBB形式の
// 色指定はCSS文法上「数値」ではない別トークン(hash-token)であり対象外
static int SkipCssUnit(const wchar_t* p, const wchar_t* q)
{
	if( p < q && *p == L'%' ) return 1;
	int n = 0;
	while( p + n < q && ((p[n]>=L'a'&&p[n]<=L'z') || (p[n]>=L'A'&&p[n]<=L'Z')) ) n++;
	return n;
}

static int IsNumberCss(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	n += SkipCssUnit(p + n, q);

	return n;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          アセンブラ                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// アウトライン解析がproc/endpキーワードを使っている(CType_Asm.cpp)ことから
// MASM系の方言を想定する。MASMの進数は「接頭辞」ではなく「接尾辞」方式で、
// IsNumber()の判定手順(先頭文字で分岐)とは根本的に形が異なるため、
// 10進数の委譲以外はほぼ独自に判定する。
// 補う記法:
//   - 16進数 (例 0FFh, 1Ah)。先頭は10進数字(0-9)である必要がある
//     (そうしないと識別子と区別できないというMASMの規則)。続けて
//     0-9A-Fa-fが並び、末尾に必ずh/Hが要る
//   - 8進数 (例 17o, 17q)。0-7の並びの末尾に必ずo/O/q/Qが要る
//   - 2進数 (例 1010b, 1010y)。0-1の並びの末尾に必ずb/B/y/Yが要る
//   - 10進数の明示的なd/Dサフィックス(省略可能)
// 浮動小数点(REAL4/REAL8等)やNASM/GAS等の他方言の記法は対象外
static int TrySuffixedLiteral(const wchar_t* p, const wchar_t* q, int nBase, const wchar_t* pszSuffixSet, bool bFirstMustBeDecimal)
{
	if( p >= q || !IsDigitOfBase(*p, nBase) ) return 0;
	if( bFirstMustBeDecimal && !(*p >= L'0' && *p <= L'9') ) return 0;
	int n = 1;
	while( p + n < q && IsDigitOfBase(p[n], nBase) ) n++;
	if( p + n >= q || !wcschr(pszSuffixSet, p[n]) ) return 0;	// 接尾辞が無ければ数値と認めない
	return n + 1;
}

static int IsNumberAsm(const CStringRef& cStr, int offset)
{
	const wchar_t* p = cStr.GetPtr() + offset;
	const wchar_t* q = cStr.GetPtr() + cStr.GetLength();

	// 'B'は2進数サフィックスであると同時に有効な16進数字でもあるため、
	// 16進数を先にチェックしないと"0BEEFh"のようなコードで誤判定する
	int nHex = TrySuffixedLiteral(p, q, 16, L"hH", true);
	if( nHex > 0 ) return nHex;

	int nOct = TrySuffixedLiteral(p, q, 8, L"oOqQ", false);
	if( nOct > 0 ) return nOct;

	int nBin = TrySuffixedLiteral(p, q, 2, L"bByY", false);
	if( nBin > 0 ) return nBin;

	int n = IsNumber(cStr, offset);
	if( n == 0 ) return 0;

	n += SkipOneCharOf(p + n, q, L"Dd");	// 明示的な10進数サフィックス(省略可)

	return n;
}
#endif // NKMM_FIX_NUMERIC_LANG_LITERAL
