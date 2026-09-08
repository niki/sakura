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

#ifndef SAKURA_COPE_H_
#define SAKURA_COPE_H_



//! アンドゥバッファ用 操作コード
enum EOpeCode {
	OPE_UNKNOWN		= 0, //!< 不明(未使用)
	OPE_INSERT		= 1, //!< 挿入
	OPE_DELETE		= 2, //!< 削除
	OPE_REPLACE		= 3, //!< 置換
	OPE_MOVECARET	= 4, //!< キャレット移動
};

#ifdef NKMM_UNDO_COALESCE_TYPING
//! Undoの結合単位を区切る文字(空白・句読点等)かどうか。完全なUnicode網羅は
//! 狙わず、ASCIIの記号類と主な日本語の句読点・括弧類のみを対象とする。
//! 識別子で使われる'_'は区切りに含めない 20260906
bool IsUndoCoalesceBreakChar( wchar_t c );
#endif // NKMM_

class CLineData {
public:
	CNativeW cmemLine;
	int nSeq;
	void swap(CLineData& o){
		std::swap(cmemLine, o.cmemLine);
		std::swap(nSeq, o.nSeq);
	}
};

namespace std {
template <>
	inline void swap(CLineData& n1, CLineData& n2)
	{
		n1.swap(n2);
	}
}

typedef std::vector<CLineData> COpeLineData;

#ifdef NKMM_UNDO_HISTORY_PANEL
//! 履歴パネルのツールチップ用プレビュー1個あたりの目安の最大文字数。あまり長いと
//! ツールチップが巨大になるため、先頭部分だけ見せれば十分という考え方 20260908
const int HISTORY_PREVIEW_MAXLEN = 120;

//! pData(nDataLen文字)を、改行を"⏎"に置き換えた1行のプレビュー文字列としてcmemDstへ
//! 追記する。HISTORY_PREVIEW_MAXLENを超えた時点で打ち切る(ちょうどでなくてよい) 20260908
void AppendOpeHistoryPreview( CNativeW& cmemDst, const wchar_t* pData, int nDataLen );
//! 複数行データ(COpeLineData)版。各行のcmemLineは通常末尾に改行文字自体を含んでいる
//! (このコードベースの行データの慣習)ため、単純に先頭から流し込むだけで行区切りに
//! "⏎"が入る。行間に別途区切りを追加する必要はない 20260908
void AppendOpeHistoryPreview( CNativeW& cmemDst, const COpeLineData& cLineData );
#endif // NKMM_

#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
//! COpeLineData(1つの操作が保持する行データの配列)の概算バイト数を合計する 20260802
inline int CalcOpeLineDataByteSize(const COpeLineData& lineData)
{
	int total = 0;
	for( const CLineData& ld : lineData ){
		// CNativeW::capacity()はconst指定が無いためconst参照からは呼べない。
		// 同じ計算をする_GetMemory()(const版)経由で取得する
		total += (int)(ld.cmemLine._GetMemory()->capacity());
	}
	return total;
}
#endif // NKMM_

#ifdef NKMM_FIX_STATUSBAR_WORDNUM_CACHE
//! COpeLineData(1つの操作が保持する行データの配列)の合計文字数を求める(改行文字を除く、
//! サロゲートペアは1文字)。GetDocumentWordNum()のO(1)キャッシュ更新に使う。 20260806
int CalcOpeLineDataCharCount(const COpeLineData& lineData);
#endif // NKMM_

/*!
	編集操作要素
	
	Undoのためにに操作手順を記録するために用いる。
	1オブジェクトが１つの操作を表す。
*/
//2007.10.17 kobake 解放漏れを防ぐため、データをポインタではなくインスタンス実体で持つように変更
class COpe {
public:
	COpe(EOpeCode eCode);		/* COpeクラス構築 */
	virtual ~COpe();	/* COpeクラス消滅 */

	virtual void DUMP( void );	/* 編集操作要素のダンプ */

	EOpeCode	GetCode() const{ return m_nOpe; }

#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	//! このCOpeが保持するテキストデータの概算バイト数(Undoバッファ上限判定用)。
	//! 既定は0(CMoveCaretOpe等、テキストを保持しない操作)。 20260802
	virtual int GetDataByteSize() const { return 0; }
#endif // NKMM_

private:
	EOpeCode	m_nOpe;						//!< 操作種別

public:
	CLogicPoint	m_ptCaretPos_PHY_Before;	//!< キャレット位置。文字単位。			[共通]
	CLogicPoint	m_ptCaretPos_PHY_After;		//!< キャレット位置。文字単位。			[共通]
#ifdef NKMM_MULTI_CURSOR
	//! マルチカーソルの一括編集(ApplyToAllCursors)で、このOpeがどのカーソルの編集による
	//! ものかを識別する。-1=マルチカーソル無関係(通常の単一カーソル編集)、0=プライマリ
	//! 自身、1以上=そのextraの編集時点でのm_vExtraCursors内index+1。Command_UNDO/REDO側で、
	//! ブロック内の「最後に処理したOpe」(降順/昇順走査、必ずしも特定のカーソルとは限らない)
	//! に頼らず、各カーソルの直前(Undo)/直後(Redo)の状態(位置・選択)へ確実に戻すために
	//! 使う。1つの一括編集ブロックに複数カーソル分のOpeが混在するため、単なるプライマリか
	//! どうかのbool 1個では各extraを個別に復元できず、識別子として持つ 20260831
	int			nCursorSlot = -1;
#endif // NKMM_
#ifdef NKMM_UNDO_COALESCE_TYPING
	//! Undoの結合可否の分類。COpeにはm_cOpeLineData等の実データを持たない
	//! 挿入経路(InsertData_CEditView等)があり、後からブロックの中身を調べても
	//! 何を挿入したか分からないため、挿入した側(Command_WCHAR等、実際に入力
	//! された文字を知っている場所)がこの場で直接分類して記録しておく。
	//! COALESCE_UNKNOWN(既定)は「この経路は分類対象外」を意味し、結合しない 20260906
	enum ECoalesceKind{ COALESCE_UNKNOWN = 0, COALESCE_WORD = 1, COALESCE_BREAK = 2 };
	ECoalesceKind	eCoalesceKind = COALESCE_UNKNOWN;
#endif // NKMM_
#ifdef NKMM_UNDO_HISTORY_PANEL
	//! 履歴パネルのツールチップ用、この操作で実際に挿入された文字列のプレビュー
	//! (先頭部分のみ、複数行は"⏎"で1行化。既定は空)。OPE_INSERT/OPE_REPLACEの
	//! 挿入側で使う。上のm_cOpeLineData/m_pcmemDataIns等は「今ドキュメントに実データが
	//! 無い側」(=Undo方向で消した結果)だけを保持し、要らなくなったら空にする設計
	//! (DoUndo/DoRedoのping-pong)のため、それを流用するとまだ一度もUndoされていない
	//! (=ドキュメントに実データがある)ブロックのプレビューが常に空になってしまう。
	//! そのため挿入した側(InsertData_CEditView/ReplaceData_CEditView3)がCOpe生成時に
	//! 一度だけ独立してセットする専用領域とする 20260908
	CNativeW	cmemHistoryPreviewIns;
#endif // NKMM_
};

//!削除
class CDeleteOpe : public COpe{
public:
	CDeleteOpe() : COpe(OPE_DELETE)
	{
		m_ptCaretPos_PHY_To.Set(CLogicInt(0),CLogicInt(0));
	}
	virtual void DUMP( void );	/* 編集操作要素のダンプ */
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	virtual int GetDataByteSize() const override { return CalcOpeLineDataByteSize(m_cOpeLineData); } // 20260802
#endif // NKMM_
public:
	CLogicPoint	m_ptCaretPos_PHY_To;		//!< 操作前のキャレット位置。文字単位。	[DELETE]
	COpeLineData	m_cOpeLineData;			//!< 操作に関連するデータ				[DELETE/INSERT]
	int				m_nOrgSeq;
};

//!挿入
class CInsertOpe : public COpe{
public:
	CInsertOpe() : COpe(OPE_INSERT) { }
	virtual void DUMP( void );	/* 編集操作要素のダンプ */
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	virtual int GetDataByteSize() const override { return CalcOpeLineDataByteSize(m_cOpeLineData); } // 20260802
#endif // NKMM_
public:
	COpeLineData	m_cOpeLineData;			//!< 操作に関連するデータ				[DELETE/INSERT]
	int				m_nOrgSeq;
};

//!置換
class CReplaceOpe : public COpe{
public:
	CReplaceOpe() : COpe(OPE_REPLACE)
	{
		m_ptCaretPos_PHY_To.Set(CLogicInt(0),CLogicInt(0));
	}
#ifdef NKMM_FIX_UNDO_BUFFER_LIMIT
	virtual int GetDataByteSize() const override {
		return CalcOpeLineDataByteSize(m_pcmemDataIns) + CalcOpeLineDataByteSize(m_pcmemDataDel);
	} // 20260802
#endif // NKMM_
public:
	CLogicPoint	m_ptCaretPos_PHY_To;		//!< 操作前のキャレット位置。文字単位。	[DELETE]
	COpeLineData	m_pcmemDataIns;			//!< 操作に関連するデータ				[INSERT]
	COpeLineData	m_pcmemDataDel;			//!< 操作に関連するデータ				[DELETE]
	int				m_nOrgInsSeq;
	int				m_nOrgDelSeq;
#ifdef NKMM_UNDO_RESTORE_SELECTION
	bool			bHadSelection = false;	//!< この置換の直前が選択状態だったか(Undoでの選択復元用) 20260831
#endif // NKMM_
};

//!キャレット移動
class CMoveCaretOpe : public COpe{
public:
	CMoveCaretOpe() : COpe(OPE_MOVECARET) { }
	CMoveCaretOpe(const CLogicPoint& ptBefore, const CLogicPoint& ptAfter)
	: COpe(OPE_MOVECARET)
	{
		m_ptCaretPos_PHY_Before = ptBefore;
		m_ptCaretPos_PHY_After = ptAfter;
	}
	CMoveCaretOpe(const CLogicPoint& ptCaretPos)
	: COpe(OPE_MOVECARET)
	{
		m_ptCaretPos_PHY_Before = ptCaretPos;
		m_ptCaretPos_PHY_After = ptCaretPos;
	}
};









///////////////////////////////////////////////////////////////////////
#endif /* SAKURA_COPE_H_ */



