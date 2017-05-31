#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CColor_Numeric.h"
#include "parse/CWordParse.h"
#include "util/string_ex2.h"
#include "doc/layout/CLayout.h"
#include "types/CTypeSupport.h"
#ifdef SC_MOD_NUMERIC_COLOR
#define REGEX_MODE (2)  // 0:std::regex, 1:boost::regex, 2:re2
#if REGEX_MODE == 0
  #include <regex>
  using namespace std;
#elif REGEX_MODE == 1
  #pragma comment(lib, "libboost_regex-vc141-mt-1_64.lib")
  #include <boost/regex.hpp>
  using namespace boost;
#elif REGEX_MODE == 2
  #pragma comment(lib, "re2.lib")
  #include <re2/re2.h>
#endif
#endif  // SC_

static int IsNumber( const CStringRef& cStr, int offset );/* 数値ならその長さを返す */	//@@@ 2001.02.17 by MIK

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         半角数値                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColor_Numeric::BeginColor(const CStringRef& cStr, int nPos)
{
	if(!cStr.IsValid())return false;

	int	nnn;

	if( _IsPosKeywordHead(cStr,nPos)
		&& (nnn = IsNumber(cStr, nPos)) > 0 )		/* 半角数字を表示する */
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
#ifdef SC_MOD_NUMERIC_COLOR
	register const wchar_t *p = cStr.GetPtr() + offset;
	register const wchar_t *q = cStr.GetPtr() + cStr.GetLength();

#if REGEX_MODE == 2
	using _regex = re2::RE2;
	using _cmatch = re2::StringPiece;
	#define _regex_search re2::RE2::PartialMatch
	#define REGSTR(x) x
	#define REGEX(x) REGSTR(x)
#else
	using _regex = wregex;
	using _cmatch = wcmatch;
	#define _regex_search regex_search
	#define REGSTR(x) L##x
	#define REGEX(x) _regex(REGSTR(x))
#endif

	int i = 0;
	_cmatch match;

	static const _regex re1_enter(REGSTR("e"));
	static const _regex re1[] = {
			REGEX("^[0-9]+\\.[0-9]*([eE][-+][0-9]+)([fF]?)"),  // 1e-2
			REGEX("^(\\.[0-9]+)([eE][-+][0-9]+)([fF]?)"),      // .12e+2
	};
	static const _regex re2_enter(REGSTR("\\."));
	static const _regex re2[] = {
			REGEX("^([0-9]+\\.[0-9]*)([fF]?)"),                // 1.0f 1.f 1.
			REGEX("^(\\.[0-9]+)([fF]?)"),                      // .1f .1
	};
	static const _regex re3[] = {
			REGEX("^0x[0-9a-fA-F]+"),                          // 0x123
			REGEX("^[0-9]+([uUlL]{0,2})"),                     // 123
	};

#if REGEX_MODE == 2
//------------------------------------------------------------------
// re2
//------------------------------------------------------------------
	std::string str(to_achar(p));

	if (_regex_search(str, re1_enter, &match)) {
		for (auto && re : re1) {
			if (_regex_search(str, re, &match)) {
				i = std::max<int>(match.data() - str.data(), i);
			}
		}
		if (i > 0) return i;
	}
	
	if (_regex_search(str, re2_enter, &match)) {
		for (auto && re : re2) {
			if (_regex_search(str, re, &match)) {
				i = std::max<int>(match.data() - str.data(), i);
			}
		}
		if (i > 0) return i;
	}
	
	for (auto && re : re3) {
		if (_regex_search(str, re, &match)) {
			i = std::max<int>(match.data() - str.data(), i);
		}
	}
#else
//------------------------------------------------------------------
// Boost
//------------------------------------------------------------------
	if (_regex_search(p, q, re1_enter)) {
		for (auto && re : re1) {
			if (_regex_search(p, q, match, re)) {
				i = std::max<int>(match.length(0), i);
			}
		}
		if (i > 0) return i;
	}
	
	if (_regex_search(p, q, re2_enter)) {
		for (auto && re : re2) {
			if (_regex_search(p, q, match, re)) {
				i = std::max<int>(match.length(0), i);
			}
		}
		if (i > 0) return i;
	}
	
	for (auto && re : re3) {
		if (_regex_search(p, q, match, re)) {
			i = std::max<int>(match.length(0), i);
		}
	}
#endif

	return i;
#else
	register const wchar_t* p;
	register const wchar_t* q;
	register int i = 0;
	register int d = 0;
	register int f = 0;

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
#endif  // SC_
}
//@@@ 2001.11.07 End by MIK
