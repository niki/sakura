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

#ifdef NKMM_UNDO_COALESCE_TYPING
//! Undoの結合単位を区切る文字(空白・句読点等)かどうか。完全なUnicode網羅は
//! 狙わず、ASCIIの記号類と主な日本語の句読点・括弧類のみを対象とする。
//! 識別子で使われる'_'は区切りに含めない 20260906
bool IsUndoCoalesceBreakChar( wchar_t c )
{
	if( iswspace(c) ){
		return true;
	}
	switch( c ){
	case L'.': case L',': case L'!': case L'?': case L';': case L':':
	case L'\'': case L'"': case L'(': case L')': case L'[': case L']':
	case L'{': case L'}': case L'<': case L'>': case L'/': case L'\\':
	case L'|': case L'`': case L'~': case L'^': case L'*': case L'+':
	case L'=': case L'@': case L'#': case L'$': case L'%': case L'&':
	case L'-':
	case 0x3001: /* 、 */		case 0x3002: /* 。 */
	case 0x30FB: /* ・ */
	case 0x300C: case 0x300D:	/* 「」 */
	case 0x300E: case 0x300F:	/* 『』 */
	case 0x3010: case 0x3011:	/* 【】 */
	case 0x3008: case 0x3009:	/* 〈〉 */
	case 0x300A: case 0x300B:	/* 《》 */
	case 0xFF01: /* ！ */		case 0xFF1F: /* ？ */
	case 0xFF08: /* （ */		case 0xFF09: /* ） */
	case 0xFF0C: /* ， */		case 0xFF0E: /* ． */
	case 0xFF1A: /* ： */		case 0xFF1B: /* ； */
		return true;
	default:
		return false;
	}
}

namespace {
	//! 区切り文字が無くても、これ以上キー入力の間隔が空いたら結合を打ち切る
	//! アイドル時間(ミリ秒)。一気に連続入力した「hello」はまとめてUndoされるが、
	//! 「hello」の後1〜2秒待ってから続けて入力した分は別のまとまりになる 20260906
	const ULONGLONG UNDO_COALESCE_IDLE_MS = 1500;

	//! pcBlkが「1個のCInsertOpeのみで構成され、かつ挿入した側で結合分類済み
	//! (eCoalesceKind != COALESCE_UNKNOWN)のブロック」かどうかを調べる。
	//!
	//! COpe自体にはInsertData_CEditView経由の挿入内容が残らない(m_cOpeLineDataは
	//! 使われない)ため、挿入した実際の文字を知っている呼び出し側(Command_WCHAR等)が
	//! 挿入直後にeCoalesceKindへ分類結果を書き込んでおく必要がある。分類されていない
	//! 挿入(貼り付け等)はCOALESCE_UNKNOWNのままなので、ここで自動的に対象外になる 20260906
	bool IsClassifiedSingleInsertBlk( COpeBlk* pcBlk, bool* pbHasBreakChar )
	{
		if( pcBlk->GetNum() != 1 ){
			return false;
		}
		COpe* pcOpe = pcBlk->GetOpe( 0 );
		if( pcOpe->GetCode() != OPE_INSERT ){
			return false;
		}
		if( pcOpe->eCoalesceKind == COpe::COALESCE_UNKNOWN ){
			return false;
		}
		*pbHasBreakChar = (pcOpe->eCoalesceKind == COpe::COALESCE_BREAK);
		return true;
	}
}
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

#ifdef NKMM_UNDO_COALESCE_TYPING
/*!	直前にpushされたブロックへ結合できるなら結合する 20260906

	結合条件:
	  - 現在位置がバッファの末尾(Redo対象が無い)であること
	  - 直前のブロックがIsCoalesceOpen()(=空白・句読点を含まない1文字挿入で
	    終わっている)であること
	  - 直前のブロックへ最後に結合してからUNDO_COALESCE_IDLE_MS以内であること
	    (区切り文字が無くても、一定時間キー入力が空いたら新しいまとまりにする)
	  - 直前のブロック最後の操作の「操作後」キャレット位置と、新ブロック最初の
	    操作の「操作前」キャレット位置が一致すること(間にカーソル移動・
	    マウスクリック・選択操作等の別操作が挟まっていない)
	  - 新ブロック自体も「挿入した側で分類済みの1個のCInsertOpeのみ」で、
	    区切り文字として分類されていないこと(改行はCommand_WCHAR側で
	    iswspace()により区切り文字として分類されるため、この時点で対象外に
	    なり必ず区切りになる。貼り付け等の未分類の挿入も対象外)

	結合の可否に関わらず、新ブロック(結合されなかった場合はこの後
	AppendOpeBlk()される想定)のIsCoalesceOpen()/結合時刻は、このブロック
	単体が上記の「区切り文字を含まない1文字挿入」であるかどうかに応じて
	設定する。これにより、区切り文字が挟まった直後や改行の直後は結合が止まり、
	次の入力から新しいまとまりが始まる
*/
bool COpeBuf::TryMergeIntoLastOpeBlk( COpeBlk* pcOpeBlk )
{
	bool bHasBreakChar = true;
	bool bPureInsert = IsClassifiedSingleInsertBlk( pcOpeBlk, &bHasBreakChar );
	bool bWordChunk = bPureInsert && !bHasBreakChar;
	ULONGLONG nNow = GetTickCount64();

	if( bWordChunk && IsEnableUndo() && !IsEnableRedo() ){
		COpeBlk* pcTail = m_vCOpeBlkArr[m_nCurrentPointer - 1];
		if( pcTail->IsCoalesceOpen() && (nNow - pcTail->GetCoalesceTick()) <= UNDO_COALESCE_IDLE_MS ){
			COpe* pcLastOpeOfTail = pcTail->GetOpe( pcTail->GetNum() - 1 );
			COpe* pcFirstOpeOfNew = pcOpeBlk->GetOpe( 0 );
			if( pcLastOpeOfTail->m_ptCaretPos_PHY_After == pcFirstOpeOfNew->m_ptCaretPos_PHY_Before ){
				COpe* pcMovedOpe = pcOpeBlk->DetachSingleOpe();
				pcTail->AppendOpe( pcMovedOpe );
				// pcTailはこの後もIsCoalesceOpen()==trueのまま(単語が続く)。
				// 結合時刻を更新し、次の文字への猶予をここから再度計る
				pcTail->SetCoalesceTick( nNow );
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
				m_nTotalByteSize += pcMovedOpe->GetDataByteSize();
				_ShrinkToBudget();
#endif // NKMM_
				delete pcOpeBlk;
				return true;
			}
		}
	}

	// 結合しない場合、この後AppendOpeBlk()される新ブロック自身の
	// 「後続への結合可否」と結合時刻を、このブロック単体の内容に応じて設定しておく
	pcOpeBlk->SetCoalesceOpen( bWordChunk );
	if( bWordChunk ){
		pcOpeBlk->SetCoalesceTick( nNow );
	}
	return false;
}
#endif // NKMM_

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



