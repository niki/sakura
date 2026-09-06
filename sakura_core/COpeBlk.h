/*!	@file
	@brief 編集操作要素ブロック

	@author Norio Nakatani
	@date 1998/06/09 新規作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

class COpeBlk;

#ifndef _COPEBLK_H_
#define _COPEBLK_H_

#include "COpe.h"
#include <vector>



/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief 編集操作要素ブロック
	
	COpe を複数束ねるためのもの。Undo, Redoはこのブロック単位で行われる。
*/
class COpeBlk {
public:
	//コンストラクタ・デストラクタ
	COpeBlk();
	~COpeBlk();

	//インターフェース
	int GetNum() const{ return (int)m_ppCOpeArr.size(); }	//!< 操作の数を返す
	bool AppendOpe( COpe* pcOpe );							//!< 操作の追加
	COpe* GetOpe( int nIndex );								//!< 操作を返す
	void AddRef() { m_refCount++; }	//!< 参照カウンタ増加
	int Release() { return m_refCount > 0 ? --m_refCount : 0; }	//!< 参照カウンタ減少
	int GetRefCount() const { return m_refCount; }	//!< 参照カウンタ取得
	int SetRefCount(int val) {  return m_refCount = val > 0? val : 0; }	//!< 参照カウンタ設定

#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	//! このブロックが保持するテキストデータの概算バイト数(Undoバッファ上限判定用)。
	//! AppendOpe()のたびに加算するのでO(1)。 20260802
	int GetByteSize() const { return m_nByteSize; }
#endif // NKMM_
#ifdef NKMM_UNDO_COALESCE_TYPING
	//! 唯一の要素を取り出して(所有権を渡して)空にする。結合先ブロックへの
	//! 移動専用の簡易実装で、要素数が1でない場合はNULLを返す 20260906
	COpe* DetachSingleOpe();

	//! このブロックの末尾へ、後続の連続入力を結合してよいか
	//! (=空白・句読点を含まない1文字挿入で終わっているか) 20260906
	bool IsCoalesceOpen() const { return m_bCoalesceOpen; }
	void SetCoalesceOpen( bool b ) { m_bCoalesceOpen = b; }

	//! このブロックへ最後に結合した時刻(GetTickCount64())。一定時間(アイドル)
	//! 経過後は区切り文字が無くても結合を打ち切るための基準時刻 20260906
	ULONGLONG GetCoalesceTick() const { return m_nCoalesceTick; }
	void SetCoalesceTick( ULONGLONG t ) { m_nCoalesceTick = t; }
#endif // NKMM_

	//デバッグ
	void DUMP();									//!< 編集操作要素ブロックのダンプ

private:
	//メンバ変数
	std::vector<COpe*>	m_ppCOpeArr;	//!< 操作の配列
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	int	m_nByteSize = 0;				//!< AppendOpe()のたびに加算する概算バイト数 20260802
#endif // NKMM_
#ifdef NKMM_UNDO_COALESCE_TYPING
	bool		m_bCoalesceOpen = false;	//!< 後続の連続入力をこのブロックへ結合してよいか 20260906
	ULONGLONG	m_nCoalesceTick = 0;		//!< 最後に結合した時刻(GetTickCount64()) 20260906
#endif // NKMM_

	//参照カウンタ
	//　HandleCommand内から再帰的にHandleCommandが呼ばれる場合、
	//  内側のHandleCommand終了時にCOpeBlkが破棄されて後続の処理に影響が出るのを防ぐため、
	//　参照カウンタを用いて一番外側のHandleCommand終了時のみCOpeBlkを破棄する。
	//　COpeBlkをnewしたときにAddRef()するのが作法だが、しなくても使える。
	int m_refCount;
};



//////////////////////////////////////////////////////////////////////12
#endif /* _COPEBLK_H_ */



