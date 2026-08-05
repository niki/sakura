/*!	@file
	@brief 編集操作要素

	@author Norio Nakatani
	@date 1998/06/09 新規作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"
#include "COpe.h"
#include "mem/CMemory.h"// 2002/2/10 aroka
#ifdef NKMM_FIX_STATUSBAR_WORDNUM_CACHE
#include "charset/charcode.h"
#include "env/CShareData.h"
#endif // NKMM_


// COpeクラス構築
COpe::COpe(EOpeCode eCode)
{
	assert( eCode != OPE_UNKNOWN );
	m_nOpe = eCode;					// 操作種別

	m_ptCaretPos_PHY_Before.Set(CLogicInt(-1),CLogicInt(-1));	//カーソル位置
	m_ptCaretPos_PHY_After.Set(CLogicInt(-1),CLogicInt(-1));	//カーソル位置

}




/* COpeクラス消滅 */
COpe::~COpe()
{
}

/* 編集操作要素のダンプ */
void COpe::DUMP( void )
{
	DEBUG_TRACE( _T("\t\tm_nOpe                  = [%d]\n"), m_nOpe               );
	DEBUG_TRACE( _T("\t\tm_ptCaretPos_PHY_Before = [%d,%d]\n"), m_ptCaretPos_PHY_Before.x, m_ptCaretPos_PHY_Before.y   );
	DEBUG_TRACE( _T("\t\tm_ptCaretPos_PHY_After  = [%d,%d]\n"), m_ptCaretPos_PHY_After.x, m_ptCaretPos_PHY_After.y   );
	return;
}

/* 編集操作要素のダンプ */
void CDeleteOpe::DUMP( void )
{
	COpe::DUMP();
	DEBUG_TRACE( _T("\t\tm_ptCaretPos_PHY_To     = [%d,%d]\n"), m_ptCaretPos_PHY_To.x, m_ptCaretPos_PHY_To.y );
	DEBUG_TRACE( _T("\t\tm_cOpeLineData.size         = [%d]\n"), m_cOpeLineData.size() );
	for( size_t i = 0; i < m_cOpeLineData.size(); i++ ){
		DEBUG_TRACE( _T("\t\tm_cOpeLineData[%d].nSeq         = [%d]\n"), m_cOpeLineData[i].nSeq );
		DEBUG_TRACE( _T("\t\tm_cOpeLineData[%d].cmemLine     = [%ls]\n"), m_cOpeLineData[i].cmemLine.GetStringPtr() );		
	}
	return;
}

/* 編集操作要素のダンプ */
void CInsertOpe::DUMP( void )
{
	COpe::DUMP();
	DEBUG_TRACE( _T("\t\tm_cOpeLineData.size         = [%d]\n"), m_cOpeLineData.size() );
	for( size_t i = 0; i < m_cOpeLineData.size(); i++ ){
		DEBUG_TRACE( _T("\t\tm_cOpeLineData[%d].nSeq         = [%d]\n"), m_cOpeLineData[i].nSeq );
		DEBUG_TRACE( _T("\t\tm_cOpeLineData[%d].cmemLine     = [%ls]\n"), m_cOpeLineData[i].cmemLine.GetStringPtr() );
	}
	return;
}

#ifdef NKMM_FIX_STATUSBAR_WORDNUM_CACHE
// COpeLineDataの合計文字数(改行文字を除く、サロゲートペアは1文字)を求める 20260806
int CalcOpeLineDataCharCount(const COpeLineData& lineData)
{
	const bool bExtEol = GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol;
	int total = 0;
	for( const CLineData& ld : lineData ){
		const wchar_t* p = ld.cmemLine.GetStringPtr();
		const int nLen = ld.cmemLine.GetStringLength();
		// 末尾の改行文字を後方から判定して除く(CRLFの2文字にも対応)
		int nEolLen = 0;
		while( nEolLen < nLen && WCODE::IsLineDelimiter(p[nLen - 1 - nEolLen], bExtEol) ){
			++nEolLen;
		}
		total += CNativeW::GetCharCountInRange(p, nLen, 0, nLen - nEolLen);
	}
	return total;
}
#endif // NKMM_


