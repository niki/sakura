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

class COpeBuf;

#ifndef _COPEBUF_H_
#define _COPEBUF_H_


#include <vector>
#include "_main/global.h"
class COpeBlk;/// 2002/2/10 aroka




/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief アンドゥ・リドゥバッファ
*/
class COpeBuf {
public:
	//コンストラクタ・デストラクタ
	COpeBuf();
	~COpeBuf();

	//状態
	bool IsEnableUndo() const;					//!< Undo可能な状態か
	bool IsEnableRedo() const;					//!< Redo可能な状態か
	int GetCurrentPointer( void ) const { return m_nCurrentPointer; }	/* 現在位置を返す */	// 2007.12.09 ryoji
	int GetNextSeq() const { return m_nCurrentPointer + 1; }
	int GetNoModifiedSeq() const { return m_nNoModifiedIndex; }

	//操作
	void ClearAll();							//!< 全要素のクリア
	bool AppendOpeBlk( COpeBlk* pcOpeBlk );		//!< 操作ブロックの追加
	void SetNoModified();						//!< 現在位置で無変更な状態になったことを通知

	//使用
	COpeBlk* DoUndo( bool* pbModified );		//!< 現在のUndo対象の操作ブロックを返す
	COpeBlk* DoRedo( bool* pbModified );		//!< 現在のRedo対象の操作ブロックを返す

	//デバッグ
	void DUMP();								//!< 編集操作要素ブロックのダンプ

private:
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	//! 共通設定の上限(KB)を超えていたら、古い(Undo方向の)ブロックから破棄して収める。
	//! Redo対象(m_nCurrentPointer以降)は直後に必要になり得るため対象外。 20260802
	void _ShrinkToBudget();
#endif // NKMM_

	std::vector<COpeBlk*>	m_vCOpeBlkArr;		//!< 操作ブロックの配列
	int						m_nCurrentPointer;	//!< 現在位置
	int						m_nNoModifiedIndex;	//!< 無変更な状態になった位置
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	int						m_nTotalByteSize;	//!< m_vCOpeBlkArr全体の概算バイト数(逐次更新) 20260802
#endif // NKMM_
};



///////////////////////////////////////////////////////////////////////
#endif /* _COPEBUF_H_ */



