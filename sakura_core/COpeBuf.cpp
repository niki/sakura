/*!	@file
	@brief アンドゥ・リドゥバッファ

	@author Norio Nakatani
	@date 1998/06/09 新規作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/
#include "StdAfx.h"
#include "COpeBuf.h"
#include "COpeBlk.h"// 2002/2/10 aroka
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
#include "env/CShareData.h" // GetDllShareData() 20260802
#endif // NKMM_


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//               コンストラクタ・デストラクタ                  //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* COpeBufクラス構築 */
COpeBuf::COpeBuf()
{
	m_nCurrentPointer = 0;	/* 現在位置 */
	m_nNoModifiedIndex = 0;	/* 無変更な状態になった位置 */
#ifdef NKMM_FIX_UNDOREDO
	m_vCOpeBlkArr.reserve(1000);
#endif // NKMM_
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	m_nTotalByteSize = 0; // 20260802
#endif // NKMM_
}

/* COpeBufクラス消滅 */
COpeBuf::~COpeBuf()
{
	/* 操作ブロックの配列を削除する */
	int size = (int)m_vCOpeBlkArr.size();
	for( int i = 0; i < size; ++i ){
		SAFE_DELETE(m_vCOpeBlkArr[i]);
	}
	m_vCOpeBlkArr.clear();
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           状態                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* Undo可能な状態か */
bool COpeBuf::IsEnableUndo() const
{
	return 0 < m_vCOpeBlkArr.size() && 0 < m_nCurrentPointer;
}

/* Redo可能な状態か */
bool COpeBuf::IsEnableRedo() const
{
	return 0 < m_vCOpeBlkArr.size() && m_nCurrentPointer < (int)m_vCOpeBlkArr.size();
}



// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           操作                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* 操作の追加 */
bool COpeBuf::AppendOpeBlk( COpeBlk* pcOpeBlk )
{
	/* 現在位置より後ろ（アンドゥ対象）がある場合は、消去 */
	int size = (int)m_vCOpeBlkArr.size();
	if( m_nCurrentPointer < size ){
		for( int i = m_nCurrentPointer; i < size; ++i ){
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
			m_nTotalByteSize -= m_vCOpeBlkArr[i]->GetByteSize(); // 20260802
#endif // NKMM_
			SAFE_DELETE(m_vCOpeBlkArr[i]);
		}
		m_vCOpeBlkArr.resize(m_nCurrentPointer);
	}
	/* 配列のメモリサイズを調整 */
	m_vCOpeBlkArr.push_back(pcOpeBlk);
	m_nCurrentPointer++;
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	m_nTotalByteSize += pcOpeBlk->GetByteSize(); // 20260802
	_ShrinkToBudget();
#endif // NKMM_
	return true;
}

#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
/*!	共通設定の上限(KB)を超えていたら、古い(Undo方向の)ブロックから破棄して収める。 20260802

	Redo対象(m_nCurrentPointer以降)は直後に必要になり得るため破棄対象にしない。
	破棄したブロックより手前で「保存済みに一致する」基準点(m_nNoModifiedIndex)が
	含まれていた場合、その基準点自体を失うため-1(追跡不能)にする。この場合以降は
	行ごとの「変更行」表示が実態より多め(安全側)になるだけで、ファイル全体の
	変更フラグ(CDocEditor::IsModified())には影響しない。
*/
void COpeBuf::_ShrinkToBudget()
{
	int nMaxKB = GetDllShareData().m_Common.m_sEdit.m_nUndoBufMaxKB;
	if( nMaxKB <= 0 ) return; // 0=無制限

	long long nMaxBytes = (long long)nMaxKB * 1024;
	if( nMaxBytes > INT_MAX ) nMaxBytes = INT_MAX;

	int nEvict = 0;
	long long nSize = m_nTotalByteSize;
	while( nSize > nMaxBytes && nEvict < m_nCurrentPointer ){
		nSize -= m_vCOpeBlkArr[nEvict]->GetByteSize();
		++nEvict;
	}
	if( nEvict == 0 ) return;

	for( int i = 0; i < nEvict; ++i ){
		SAFE_DELETE(m_vCOpeBlkArr[i]);
	}
	m_vCOpeBlkArr.erase(m_vCOpeBlkArr.begin(), m_vCOpeBlkArr.begin() + nEvict);
	m_nTotalByteSize = (int)nSize;
	m_nCurrentPointer -= nEvict;
	if( m_nNoModifiedIndex > nEvict ){
		m_nNoModifiedIndex -= nEvict;
	}else if( m_nNoModifiedIndex >= 0 ){
		m_nNoModifiedIndex = -1; // 基準点自体を破棄した。以後は追跡不能
	}
}
#endif // NKMM_

/* 全要素のクリア */
void COpeBuf::ClearAll()
{
	/* 操作ブロックの配列を削除する */
	int size = (int)m_vCOpeBlkArr.size();
	for( int i = 0; i < size; ++i ){
		SAFE_DELETE(m_vCOpeBlkArr[i]);
	}
	m_vCOpeBlkArr.clear();
	m_nCurrentPointer = 0;	/* 現在位置 */
	m_nNoModifiedIndex = 0;	/* 無変更な状態になった位置 */
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	m_nTotalByteSize = 0; // 20260802
#endif // NKMM_
}

/* 現在位置で無変更な状態になったことを通知 */
void COpeBuf::SetNoModified()
{
	m_nNoModifiedIndex = m_nCurrentPointer;	/* 無変更な状態になった位置 */
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           使用                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* 現在のUndo対象の操作ブロックを返す */
COpeBlk* COpeBuf::DoUndo( bool* pbModified )
{
	/* Undo可能な状態か */
	if( !IsEnableUndo() ){
		return NULL;
	}
	m_nCurrentPointer--;
	if( m_nCurrentPointer == m_nNoModifiedIndex ){		/* 無変更な状態になった位置 */
		*pbModified = false;
	}else{
		*pbModified = true;
	}
	return m_vCOpeBlkArr[m_nCurrentPointer];
}

/* 現在のRedo対象の操作ブロックを返す */
COpeBlk* COpeBuf::DoRedo( bool* pbModified )
{
	COpeBlk*	pcOpeBlk;
	/* Redo可能な状態か */
	if( !IsEnableRedo() ){
		return NULL;
	}
	pcOpeBlk = m_vCOpeBlkArr[m_nCurrentPointer];
	m_nCurrentPointer++;
	if( m_nCurrentPointer == m_nNoModifiedIndex ){		/* 無変更な状態になった位置 */
		*pbModified = false;
	}else{
		*pbModified = true;
	}
	return pcOpeBlk;
}



// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         デバッグ                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* アンドゥ・リドゥバッファのダンプ */
void COpeBuf::DUMP()
{
#ifdef _DEBUG
	int i;
	MYTRACE( _T("COpeBuf.m_nCurrentPointer=[%d]----\n"), m_nCurrentPointer );
	int size = (int)m_vCOpeBlkArr.size();
	for( i = 0; i < size; ++i ){
		MYTRACE( _T("COpeBuf.m_vCOpeBlkArr[%d]----\n"), i );
		m_vCOpeBlkArr[i]->DUMP();
	}
	MYTRACE( _T("COpeBuf.m_nCurrentPointer=[%d]----\n"), m_nCurrentPointer );
#endif
}



