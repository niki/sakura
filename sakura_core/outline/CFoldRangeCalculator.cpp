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
#include "StdAfx.h"
#include "outline/CFoldRangeCalculator.h"
#include "outline/CFuncInfoArr.h"
#include "outline/CFuncInfo.h"

/*!	アウトライン解析結果(CFuncInfoArr)から折りたたみ範囲を算出する

	@date 2026.09.11 Yu-zuki. 新規作成 (コードフォールディング機能 Phase1)
*/
std::vector<SFoldRange> CFoldRangeCalculator::Calculate( CFuncInfoArr* pcFuncInfoArr, CLogicInt nDocLineCount )
{
	std::vector<SFoldRange> vecResult;
	if( NULL == pcFuncInfoArr ){
		return vecResult;
	}

	const int nNum = pcFuncInfoArr->GetNum();
	vecResult.reserve( nNum );

	// nDepth以上の深さを持つ開いたままの項目を保持するスタック。
	// 要素はvecResult内のインデックス。CFuncInfoArrは文書出現順(先行順)で
	// 積まれている前提(MakeFuncList_C等の実装が上から順にAppendDataしている)。
	std::vector<int> vecOpenIndex;

	for( int i = 0; i < nNum; ++i ){
		CFuncInfo* pcInfo = pcFuncInfoArr->GetAt( i );
		if( NULL == pcInfo ){
			continue;
		}
		// m_nFuncLineCRLFは1オリジン(CDlgFuncList.cppのGetLine()呼び出し箇所と同じ規約)。
		// CDocLineMgr::GetLine()は0オリジンのため、ここで変換しておく 20260911
		const CLogicInt nStartLine = pcInfo->m_nFuncLineCRLF - CLogicInt(1);
		const int nDepth = pcInfo->m_nDepth;

		// 同じか浅い深さの項目が現れたら、それより深い開いたままの項目を
		// すべて「直前の行」で閉じる。
		while( !vecOpenIndex.empty() ){
			SFoldRange& rOpen = vecResult[vecOpenIndex.back()];
			if( rOpen.nDepth < nDepth ){
				break;
			}
			rOpen.nEndLine = ( nStartLine > 0 ) ? (nStartLine - 1) : CLogicInt(0);
			vecOpenIndex.pop_back();
		}

		SFoldRange sRange;
		sRange.nStartLine = nStartLine;
		sRange.nEndLine = nStartLine;	// 後で閉じられるまでの仮値
		sRange.nDepth = nDepth;
		sRange.nFuncInfoIndex = i;
		vecResult.push_back( sRange );
		vecOpenIndex.push_back( (int)vecResult.size() - 1 );
	}

	// 最後まで開いたままの項目は文書末尾で閉じる
	const CLogicInt nLastLine = ( nDocLineCount > 0 ) ? (nDocLineCount - 1) : CLogicInt(0);
	for( size_t i = 0; i < vecOpenIndex.size(); ++i ){
		vecResult[vecOpenIndex[i]].nEndLine = nLastLine;
	}

	return vecResult;
}
