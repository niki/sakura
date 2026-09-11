/*
	Copyright (C) 2026, Yu-zuki.

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
#ifndef SAKURA_CFOLDRANGECALCULATOR_H_
#define SAKURA_CFOLDRANGECALCULATOR_H_

#include <vector>
#include "basis/SakuraBasis.h"

class CFuncInfoArr;

//! 折りたたみ可能範囲(関数/構造体等のブロック単位) 20260911
struct SFoldRange{
	CLogicInt	nStartLine;		//!< 開始行(ヘッダ行。0オリジン、CRLF単位)
	CLogicInt	nEndLine;		//!< 終了行(0オリジン、CRLF単位。開始行と同じ場合は単一行なので折りたたみ対象外)
	int			nDepth;			//!< アウトライン解析での深さ
	int			nFuncInfoIndex;	//!< 元になったCFuncInfoArr上のインデックス(GetAt()に渡せる) 20260911
};

//! CFuncInfoArr(アウトライン解析結果)から折りたたみ範囲を算出する
//  CFuncInfoには開始行しか記録されていないため、次に現れる「同じ深さ以下」の
//  項目の直前までを終了行とみなす(末尾まで無ければ文書末尾)。
//  そのため戻り値の各要素は最低1行以上の本体を持つとは限らず、単一行の
//  項目(nStartLine==nEndLine)は呼び出し側で折りたたみ対象外として扱うこと。
class CFoldRangeCalculator{
public:
	static std::vector<SFoldRange> Calculate( CFuncInfoArr* pcFuncInfoArr, CLogicInt nDocLineCount );
};

#endif /* SAKURA_CFOLDRANGECALCULATOR_H_ */
/*[EOF]*/
