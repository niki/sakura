/*!	@file
@brief CViewCommanderクラスのコマンド(編集系 基本形)関数群

	2012/12/16	CViewCommander.cpp,CViewCommander_New.cppから分離
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2002, genta
	Copyright (C) 2003, MIK, genta, かろと, zenryaku, Moca, ryoji, naoh, KEITA, じゅうじ
	Copyright (C) 2005, genta, D.S.Koba, ryoji
	Copyright (C) 2007, ryoji, kobake
	Copyright (C) 2008, ryoji, nasukoji
	Copyright (C) 2009, ryoji
	Copyright (C) 2010, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "CViewCommander.h"
#include "CViewCommander_inline.h"

#include "view/CRuler.h"
#include "uiparts/CWaitCursor.h"
#include "plugin/CJackManager.h"
#include "plugin/CSmartIndentIfObj.h"
#include "debug/CRunningTimer.h"
#ifdef NKMM_MULTI_CURSOR
#include <algorithm>
#endif // NKMM_


/* wchar_t1個分の文字を入力 */
void CViewCommander::Command_WCHAR( wchar_t wcChar, bool bConvertEOL )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){	/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	CLogicInt		nPos;
	CLogicInt		nCharChars;

	GetDocument()->m_cDocEditor.SetModified(true,true);	//	Jan. 22, 2002 genta

	if( m_pCommanderView->m_bHideMouse && 0 <= m_pCommanderView->m_nMousePouse ){
		m_pCommanderView->m_nMousePouse = -1;
		::SetCursor( NULL );
	}

	/* 現在位置にデータを挿入 */
	CNativeW cmemDataW2;
	cmemDataW2 = wcChar;
	if( WCODE::IsLineDelimiter(wcChar, GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) ){ 
		/* 現在、Enterなどで挿入する改行コードの種類を取得 */
		if( bConvertEOL ){
			CEol cWork = GetDocument()->m_cDocEditor.GetNewLineCode();
			cmemDataW2.SetString( cWork.GetValue2(), cWork.GetLen() );
		}

		/* テキストが選択されているか */
		if( m_pCommanderView->GetSelectionInfo().IsTextSelected() ){
			m_pCommanderView->DeleteData( true );
		}
		if( m_pCommanderView->m_pTypeData->m_bAutoIndent ){	/* オートインデント */
			const CLayout* pCLayout;
			const wchar_t*	pLine;
			CLogicInt		nLineLen;
			pLine = GetDocument()->m_cLayoutMgr.GetLineStr( GetCaret().GetCaretLayoutPos().GetY2(), &nLineLen, &pCLayout );
			if( NULL != pCLayout ){
				const CDocLine* pcDocLine;
				pcDocLine = GetDocument()->m_cDocLineMgr.GetLine( pCLayout->GetLogicLineNo() );
				pLine = pcDocLine->GetDocLineStrWithEOL( &nLineLen );
				if( NULL != pLine ){
					/*
					  カーソル位置変換
					  レイアウト位置(行頭からの表示桁位置、折り返しあり行位置)
					  →
					  物理位置(行頭からのバイト数、折り返し無し行位置)
					*/
					CLogicPoint ptXY;
					GetDocument()->m_cLayoutMgr.LayoutToLogic(
						GetCaret().GetCaretLayoutPos(),
						&ptXY
					);

					/* 指定された桁に対応する行のデータ内の位置を調べる */
					for( nPos = CLogicInt(0); nPos < nLineLen && nPos < ptXY.GetX2(); ){
						// 2005-09-02 D.S.Koba GetSizeOfChar
						nCharChars = CNativeW::GetSizeOfChar( pLine, nLineLen, nPos );

						/* その他のインデント文字 */
						if( 0 < nCharChars
						 && pLine[nPos] != L'\0'	// その他のインデント文字に L'\0' は含まれない	// 2009.02.04 ryoji L'\0'がインデントされてしまう問題修正
						 && m_pCommanderView->m_pTypeData->m_szIndentChars[0] != L'\0'
						){
							wchar_t szCurrent[10];
							wmemcpy( szCurrent, &pLine[nPos], nCharChars );
							szCurrent[nCharChars] = L'\0';
							/* その他のインデント対象文字 */
							if( NULL != wcsstr(
								m_pCommanderView->m_pTypeData->m_szIndentChars,
								szCurrent
							) ){
								goto end_of_for;
							}
						}
						
						{
							bool bZenSpace=m_pCommanderView->m_pTypeData->m_bAutoIndent_ZENSPACE;
							if(nCharChars==1 && WCODE::IsIndentChar(pLine[nPos],bZenSpace))
							{
								//下へ進む
							}
							else break;
						}

end_of_for:;
						nPos += nCharChars;
					}

					//インデント取得
					//CNativeW cmemIndent;
					//cmemIndent.SetString( pLine, nPos );

					//インデント付加
					cmemDataW2.AppendString(pLine, nPos);
				}
			}
		}
	}
	else{
		/* テキストが選択されているか */
		if( m_pCommanderView->GetSelectionInfo().IsTextSelected() ){
			/* 矩形範囲選択中か */
			if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
				Command_INDENT( wcChar );
				return;
			}else{
				m_pCommanderView->DeleteData( true );
			}
		}
		else{
			if( ! m_pCommanderView->IsInsMode() /* Oct. 2, 2005 genta */ ){
				DelCharForOverwrite(&wcChar, 1);	// 上書き用の一文字削除	// 2009.04.11 ryoji
			}
		}
	}

	//本文に挿入する
	CLayoutPoint ptLayoutNew;
	m_pCommanderView->InsertData_CEditView(
		GetCaret().GetCaretLayoutPos(),
		cmemDataW2.GetStringPtr(),
		cmemDataW2.GetStringLength(),
		&ptLayoutNew,
		true
	);

	/* 挿入データの最後へカーソルを移動 */
	GetCaret().MoveCursor( ptLayoutNew, true );
	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

	/* スマートインデント */
	ESmartIndentType nSIndentType = m_pCommanderView->m_pTypeData->m_eSmartIndent;
	switch( nSIndentType ){	/* スマートインデント種別 */
	case SMARTINDENT_NONE:
		break;
	case SMARTINDENT_CPP:
		/* C/C++スマートインデント処理 */
		m_pCommanderView->SmartIndent_CPP( wcChar );
		break;
	default:
		//プラグインから検索する
		{
			CPlug::Array plugs;
			CJackManager::getInstance()->GetUsablePlug( PP_SMARTINDENT, nSIndentType, &plugs );

			if( plugs.size() > 0 ){
				assert_warning( 1 == plugs.size() );
				//インタフェースオブジェクト準備
				CWSHIfObj::List params;
				CSmartIndentIfObj* objIndent = new CSmartIndentIfObj( wcChar );	//スマートインデントオブジェクト
				objIndent->AddRef();
				params.push_back( objIndent );

				//キー入力をアンドゥバッファに反映
				m_pCommanderView->SetUndoBuffer();

				//キー入力とは別の操作ブロックにする（ただしプラグイン内の操作はまとめる）
				if( GetOpeBlk() == NULL ){
					SetOpeBlk(new COpeBlk);
				}
				GetOpeBlk()->AddRef();	// ※ReleaseはHandleCommandの最後で行う

				//プラグイン呼び出し
				( *plugs.begin() )->Invoke( m_pCommanderView, params );
				objIndent->Release();
			}
		}
		break;
	}

	/* 2005.10.11 ryoji 改行時に末尾の空白を削除 */
	if( WCODE::IsLineDelimiter(wcChar, GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) && m_pCommanderView->m_pTypeData->m_bRTrimPrevLine ){	/* 改行時に末尾の空白を削除 */
		/* 前の行にある末尾の空白を削除する */
		m_pCommanderView->RTrimPrevLine();
	}

	m_pCommanderView->PostprocessCommand_hokan();	//	Jan. 10, 2005 genta 関数化
}



/*!
	@brief 2バイト文字入力
	
	WM_IME_CHARで送られてきた文字を処理する．
	ただし，挿入モードではWM_IME_CHARではなくWM_IME_COMPOSITIONで文字列を
	取得するのでここには来ない．

	@param wChar [in] SJIS漢字コード．上位が1バイト目，下位が2バイト目．
	
	@date 2002.10.06 genta 引数の上下バイトの意味を逆転．
		WM_IME_CHARのwParamに合わせた．
*/
void CViewCommander::Command_IME_CHAR( WORD wChar )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){	/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	//	Oct. 6 ,2002 genta 上下逆転
	if( 0 == (wChar & 0xff00) ){
		Command_WCHAR( wChar & 0xff );
		return;
	}
	GetDocument()->m_cDocEditor.SetModified(true,true);	//	Jan. 22, 2002 genta

 	if( m_pCommanderView->m_bHideMouse && 0 <= m_pCommanderView->m_nMousePouse ){
		m_pCommanderView->m_nMousePouse = -1;
		::SetCursor( NULL );
	}

	// Oct. 6 ,2002 genta バッファに格納する
	// Aug. 15, 2007 kobake WCHARバッファに変換する
#ifdef _UNICODE
	wchar_t szWord[2]={wChar,0};
#else
	ACHAR szAnsiWord[3]={(wChar >> 8) & 0xff, wChar & 0xff, 0};
	const wchar_t* pUniData = to_wchar(szAnsiWord);
	wchar_t szWord[2]={pUniData[0],0};
#endif
	CLogicInt nWord=CLogicInt(1);

	/* テキストが選択されているか */
	if( m_pCommanderView->GetSelectionInfo().IsTextSelected() ){
		/* 矩形範囲選択中か */
		if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
			Command_INDENT( szWord, nWord );	//	Oct. 6 ,2002 genta 
			return;
		}else{
			m_pCommanderView->DeleteData( true );
		}
	}
	else{
		if( ! m_pCommanderView->IsInsMode() /* Oct. 2, 2005 genta */ ){
			DelCharForOverwrite(szWord, nWord);	// 上書き用の一文字削除	// 2009.04.11 ryoji
		}
	}

	//	Oct. 6 ,2002 genta 
	CLayoutPoint ptLayoutNew;
	m_pCommanderView->InsertData_CEditView( GetCaret().GetCaretLayoutPos(), szWord, nWord, &ptLayoutNew, true );

	/* 挿入データの最後へカーソルを移動 */
	GetCaret().MoveCursor( ptLayoutNew, true );
	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

	m_pCommanderView->PostprocessCommand_hokan();	//	Jan. 10, 2005 genta 関数化
}



#ifdef NKMM_MULTI_CURSOR
//! Command_UNDO/Command_REDO共通: マルチカーソルの一括編集(ApplyToAllCursors)がpcOpeBlkに
//! 複数カーソル分のOpeを積んでいた場合、単純に「ブロック内の最後に処理したOpe」を最終状態と
//! するロジックは必ずしもプライマリのOpeとは限らない結果になる。ApplyToAllCursors側でOpeごとに
//! 記録したnCursorSlot(0=プライマリ、1以上=extra)を手がかりに、プライマリ・各extraそれぞれ
//! 自身の状態へ明示的に戻す。Undo/Redoでほぼ同一だった処理(SSlotRestore収集→プライマリ復元→
//! extra再構築→Redraw)を1関数へ統合したもの。bIsUndoで変わる点は3つだけ:
//!  - 復元元: Undoは各スロットで最初に見つかったOpeの_Before、Redoは最後に見つかったOpeの_After
//!  - 選択復元: UndoのみNKMM_UNDO_RESTORE_SELECTION+bHadSelectionのOPE_REPLACEから復元する
//!    (Redoは削除後に選択すべき対象が残らないため、単一カーソル時も一貫して選択を出さない仕様)
//!  - m_sSelectBgn: Undoは選択の有無に関わらず常にSet()する(元の単一カーソル版Undoの挙動を
//!    踏襲)。Redoは元々Set()を一切呼んでいなかった(DisableSelectAreaはm_sSelectBgnを触らない
//!    ため、次に実際の選択操作が始まるまで古い値が残るだけで実害はないが、統合にあたり挙動を
//!    変えないためそのまま踏襲する)
//! bFastMode(大量Opeの一括Undo/Redo)ではptCaretPos_Before等がループ内で計算されない分岐のため
//! 呼び出し側で除外し、pcOpeBlk!=NULLも呼び出し側で確認してから呼ぶこと 20260831
void CViewCommander::RestoreMultiCursorAfterUndoRedo( COpeBlk* pcOpeBlk, int nOpeBlkNum, bool bIsUndo )
{
	struct SSlotRestore{
		int nSlot;
		int nPosOpeIdx = -1;			//!< 位置(Undo:_Before/Redo:_After)の復元に使うOpe
#ifdef NKMM_UNDO_RESTORE_SELECTION
		CReplaceOpe* pcSelOpe = NULL;	//!< 選択があった場合のOPE_REPLACE(Undoのみ使用)
#endif // NKMM_
	};
	std::vector<SSlotRestore> vSlots;
	for( int k = 0; k < nOpeBlkNum; ++k ){
		COpe* pcCandidate = pcOpeBlk->GetOpe(k);
		if( pcCandidate->nCursorSlot < 0 ) continue;
		SSlotRestore* pSlot = NULL;
		for( auto& s : vSlots ){ if( s.nSlot == pcCandidate->nCursorSlot ){ pSlot = &s; break; } }
		if( !pSlot ){ vSlots.push_back( SSlotRestore{ pcCandidate->nCursorSlot } ); pSlot = &vSlots.back(); }
		if( bIsUndo ){
			if( pSlot->nPosOpeIdx < 0 ) pSlot->nPosOpeIdx = k;	// 降順走査、最初(最小index)を残す
		}else{
			pSlot->nPosOpeIdx = k;	// 昇順走査、最後(最大index)を残す
		}
#ifdef NKMM_UNDO_RESTORE_SELECTION
		if( bIsUndo && pcCandidate->GetCode() == OPE_REPLACE ){
			CReplaceOpe* pcCandidateReplace = static_cast<CReplaceOpe*>(pcCandidate);
			if( pcCandidateReplace->bHadSelection ) pSlot->pcSelOpe = pcCandidateReplace;
		}
#endif // NKMM_
	}

	SSlotRestore* pPrimarySlot = NULL;
	for( auto& s : vSlots ){ if( s.nSlot == 0 ){ pPrimarySlot = &s; break; } }

	// プライマリ自身の分がこの一括編集に含まれていなければ(通常起こらないが、プライマリの
	// 選択範囲が空でIsEmptyArea等により編集自体が無かった場合等)、他のextra分だけ復元しても
	// 相対化の基準点が無いため何もしない
	if( !pPrimarySlot ) return;

	COpe* pcPrimaryPosOpe = pcOpeBlk->GetOpe( pPrimarySlot->nPosOpeIdx );
	CLayoutPoint ptPrimaryPos;
	GetDocument()->m_cLayoutMgr.LogicToLayout(
		bIsUndo ? pcPrimaryPosOpe->m_ptCaretPos_PHY_Before : pcPrimaryPosOpe->m_ptCaretPos_PHY_After,
		&ptPrimaryPos );

	CLayoutPoint ptPrimaryAnchor = ptPrimaryPos;	// 選択が無ければアンカー=キャレット位置とみなす(extraの相対化の基準用)
	bool bPrimaryHasSelection = false;
#ifdef NKMM_UNDO_RESTORE_SELECTION
	CLayoutPoint ptPrimarySelTo;
	if( bIsUndo && pPrimarySlot->pcSelOpe && GetDllShareData().m_Common.m_sEdit.m_bUndoRestoreSelection ){
		GetDocument()->m_cLayoutMgr.LogicToLayout( pPrimarySlot->pcSelOpe->m_ptCaretPos_PHY_Before, &ptPrimaryAnchor );
		GetDocument()->m_cLayoutMgr.LogicToLayout( pPrimarySlot->pcSelOpe->m_ptCaretPos_PHY_To, &ptPrimarySelTo );
		bPrimaryHasSelection = true;
	}
#endif // NKMM_
	if( bIsUndo ){
		m_pCommanderView->GetSelectionInfo().m_sSelectBgn.Set( ptPrimaryAnchor );
	}
	if( bPrimaryHasSelection ){
#ifdef NKMM_UNDO_RESTORE_SELECTION
		m_pCommanderView->GetSelectionInfo().m_sSelect = CLayoutRange( ptPrimaryAnchor, ptPrimarySelTo );
#endif // NKMM_
	}else{
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( false );
	}
	// 選択状態を確定させてからMoveCursorを呼ぶ(MoveCursor内部でShowCaretPosInfo()を呼び
	// ステータスバーの選択文字数表示を更新するため、選択確定より先に呼ぶと古い選択状態のまま
	// 表示してしまう)
	GetCaret().MoveCursor( ptPrimaryPos, true );
	GetCaret().m_nCaretPosX_Prev = ptPrimaryPos.GetX2();

	// 残りのextra分を、プライマリの新しい位置・選択アンカー基準の相対値として作り直す。既存の
	// m_vExtraCursorsは編集直後(=Undo/Redo対象の古い状態)の値しか持っていないため丸ごと置き換える。
	// このブロックに現れなかった(非アクティブだった、または元々存在しなかった)extraは
	// 復元しようがなく消える
	std::vector<CEditView::SExtraCursor> vNewExtras;
	for( auto& s : vSlots ){
		if( s.nSlot == 0 ) continue;
		COpe* pcPosOpe = pcOpeBlk->GetOpe( s.nPosOpeIdx );
		CLayoutPoint ptExtraPos;
		GetDocument()->m_cLayoutMgr.LogicToLayout(
			bIsUndo ? pcPosOpe->m_ptCaretPos_PHY_Before : pcPosOpe->m_ptCaretPos_PHY_After,
			&ptExtraPos );
		CLayoutPoint ptExtraCaret = ptExtraPos;

		CEditView::SExtraCursor ne;
		ne.bHasSelection = false;
#ifdef NKMM_UNDO_RESTORE_SELECTION
		if( bIsUndo && s.pcSelOpe && GetDllShareData().m_Common.m_sEdit.m_bUndoRestoreSelection ){
			CLayoutPoint ptExtraAnchor, ptExtraSelTo;
			GetDocument()->m_cLayoutMgr.LogicToLayout( s.pcSelOpe->m_ptCaretPos_PHY_Before, &ptExtraAnchor );
			GetDocument()->m_cLayoutMgr.LogicToLayout( s.pcSelOpe->m_ptCaretPos_PHY_To, &ptExtraSelTo );
			ne.bHasSelection = true;
			ne.nAnchorRelLine = ToInt(ptExtraAnchor.GetY2()) - ToInt(ptPrimaryAnchor.GetY2());
			ne.nAnchorRelColumn = ToInt(ptExtraAnchor.GetX2()) - ToInt(ptPrimaryAnchor.GetX2());
			ptExtraCaret = ptExtraSelTo;
		}
#endif // NKMM_
		ne.nRelLine = ToInt(ptExtraCaret.GetY2()) - ToInt(ptPrimaryPos.GetY2());
		ne.nRelColumn = ToInt(ptExtraCaret.GetX2()) - ToInt(ptPrimaryPos.GetX2());
		ne.nDesiredRelColumn = ne.nRelColumn;
		vNewExtras.push_back( ne );
	}
	m_pCommanderView->m_vExtraCursors = std::move( vNewExtras );

	// 呼び出し元にあるNKMM_FIX_UNDOREDOの「プライマリの行が変わらなければ全画面再描画を
	// 省略する」最適化は、単一カーソル時代の前提(影響範囲は常にキャレット行の近辺)が
	// マルチカーソルでは崩れる — ここで復元したプライマリ・各extraの選択は、プライマリの
	// 行と無関係な遠い行の見た目にも影響するため、その最適化に関係なくここで確実に再描画する
	m_pCommanderView->Redraw();
}
#endif // NKMM_



//	from CViewCommander_New.cpp
/* Undo 元に戻す */
void CViewCommander::Command_UNDO( void )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){	/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	{
		COpeBlk* opeBlk = m_pCommanderView->m_cCommander.GetOpeBlk();
		if( opeBlk ){
			int nCount = opeBlk->GetRefCount();
			opeBlk->SetRefCount(1); // 強制的にリセットするため1を指定
			m_pCommanderView->SetUndoBuffer();
			if( m_pCommanderView->m_cCommander.GetOpeBlk() == NULL && 0 < nCount ){
				m_pCommanderView->m_cCommander.SetOpeBlk(new COpeBlk());
				m_pCommanderView->m_cCommander.GetOpeBlk()->SetRefCount( nCount );
			}
		}
	}

	if( !GetDocument()->m_cDocEditor.IsEnableUndo() ){	/* Undo(元に戻す)可能な状態か？ */
		return;
	}

	MY_RUNNINGTIMER( cRunningTimer, "CViewCommander::Command_UNDO()" );

	COpe*		pcOpe = NULL;

	COpeBlk*	pcOpeBlk;
	int			nOpeBlkNum;
	int			i;
	bool		bIsModified;
//	int			nNewLine;	/* 挿入された部分の次の位置の行 */
//	int			nNewPos;	/* 挿入された部分の次の位置のデータ位置 */

	CLayoutPoint ptCaretPos_Before;

	CLayoutPoint ptCaretPos_After;

#ifdef NKMM_FIX_UNDOREDO
	// 表示域の一番左の桁
	CLayoutInt nViewLeftCol = m_pCommanderView->GetTextArea().GetViewLeftCol();
	
	// 処理前の行位置
	CLogicPoint ptCaretLogic_Start = GetCaret().GetCaretLogicPos();  // 現在行
	CLayoutPoint ptCaretLayout_Start = GetCaret().GetCaretLayoutPos();
	
	// 変更前の行位置
	CLogicPoint ptCaretLogic_Next = {ptCaretLogic_Start.x, ptCaretLogic_Start.y + 1};  // 次の行
	CLayoutPoint ptCaretLayout_Next;
	
	GetDocument()->m_cLayoutMgr.LogicToLayout(
		ptCaretLogic_Next,
		&ptCaretLayout_Next,
		ptCaretLayout_Start.y
	);
#endif // NKMM_

	/* 各種モードの取り消し */
	Command_CANCEL_MODE();

	m_pCommanderView->m_bDoing_UndoRedo = true;	/* アンドゥ・リドゥの実行中か */

	/* 現在のUndo対象の操作ブロックを返す */
	if( NULL != ( pcOpeBlk = GetDocument()->m_cDocEditor.m_cOpeBuf.DoUndo( &bIsModified ) ) ){
		nOpeBlkNum = pcOpeBlk->GetNum();
		bool bDraw = (nOpeBlkNum < 5) && m_pCommanderView->GetDrawSwitch();
		bool bDrawAll = false;
		const bool bDrawSwitchOld = m_pCommanderView->SetDrawSwitch(bDraw);	// hor

		CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd(), 1000 < nOpeBlkNum );
		HWND hwndProgress = NULL;
		int nProgressPos = 0;
		if( cWaitCursor.IsEnable() ){
			hwndProgress = m_pCommanderView->StartProgress();
		}

		const bool bFastMode = (100 < nOpeBlkNum);
		for( i = nOpeBlkNum - 1; i >= 0; i-- ){
			pcOpe = pcOpeBlk->GetOpe( i );
			if( bFastMode ){
				GetCaret().MoveCursorFastMode( pcOpe->m_ptCaretPos_PHY_After );
			}else{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					pcOpe->m_ptCaretPos_PHY_After,
					&ptCaretPos_After
				);
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					pcOpe->m_ptCaretPos_PHY_Before,
					&ptCaretPos_Before
				);

				/* カーソルを移動 */
				GetCaret().MoveCursor( ptCaretPos_After, false );
			}

			switch( pcOpe->GetCode() ){
			case OPE_INSERT:
				{
					CInsertOpe* pcInsertOpe = static_cast<CInsertOpe*>(pcOpe);

					/* 選択範囲の変更 */
					CLogicRange cSelectLogic;
					cSelectLogic.SetFrom(pcOpe->m_ptCaretPos_PHY_Before);
					cSelectLogic.SetTo(pcOpe->m_ptCaretPos_PHY_After);
					if( bFastMode ){
					}else{
						m_pCommanderView->GetSelectionInfo().m_sSelectBgn.SetFrom(ptCaretPos_Before);
						m_pCommanderView->GetSelectionInfo().m_sSelectBgn.SetTo(m_pCommanderView->GetSelectionInfo().m_sSelectBgn.GetFrom());
						m_pCommanderView->GetSelectionInfo().m_sSelect.SetFrom(ptCaretPos_Before);
						m_pCommanderView->GetSelectionInfo().m_sSelect.SetTo(ptCaretPos_After);
					}

					/* データ置換 削除&挿入にも使える */
					bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
						m_pCommanderView->GetSelectionInfo().m_sSelect,				// 削除範囲
						&pcInsertOpe->m_cOpeLineData,	// 削除されたデータのコピー(NULL可能)
						NULL,
						bDraw,						// 再描画するか否か
						NULL,
						pcInsertOpe->m_nOrgSeq,
						NULL,
						bFastMode,
						&cSelectLogic
					);

					/* 選択範囲の変更 */
					m_pCommanderView->GetSelectionInfo().m_sSelectBgn.Clear(-1); //範囲選択(原点)
					m_pCommanderView->GetSelectionInfo().m_sSelect.Clear(-1);
				}
				break;
			case OPE_DELETE:
				{
					CDeleteOpe* pcDeleteOpe = static_cast<CDeleteOpe*>(pcOpe);

					//2007.10.17 kobake メモリリークしてました。修正。
					if( 0 < pcDeleteOpe->m_cOpeLineData.size() ){
						/* データ置換 削除&挿入にも使える */
						CLayoutRange sRange;
						sRange.Set(ptCaretPos_Before);
						CLogicRange cSelectLogic;
						cSelectLogic.Set(pcOpe->m_ptCaretPos_PHY_Before);
						bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
							sRange,
							NULL,										/* 削除されたデータのコピー(NULL可能) */
							&pcDeleteOpe->m_cOpeLineData,
							bDraw,										/*再描画するか否か*/
							NULL,
							0,
							&pcDeleteOpe->m_nOrgSeq,
							bFastMode,
							&cSelectLogic
						);
					}
					pcDeleteOpe->m_cOpeLineData.clear();
				}
				break;
			case OPE_REPLACE:
				{
					CReplaceOpe* pcReplaceOpe = static_cast<CReplaceOpe*>(pcOpe);

					CLayoutRange sRange;
					sRange.SetFrom(ptCaretPos_Before);
					sRange.SetTo(ptCaretPos_After);
					CLogicRange cSelectLogic;
					cSelectLogic.SetFrom(pcOpe->m_ptCaretPos_PHY_Before);
					cSelectLogic.SetTo(pcOpe->m_ptCaretPos_PHY_After);

					/* データ置換 削除&挿入にも使える */
					bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
						sRange,				// 削除範囲
						&pcReplaceOpe->m_pcmemDataIns,	// 削除されたデータのコピー(NULL可能)
						&pcReplaceOpe->m_pcmemDataDel,	// 挿入するデータ
						bDraw,						// 再描画するか否か
						NULL,
						pcReplaceOpe->m_nOrgInsSeq,
						&pcReplaceOpe->m_nOrgDelSeq,
						bFastMode,
						&cSelectLogic
					);
					pcReplaceOpe->m_pcmemDataDel.clear();

#ifdef NKMM_UNDO_RESTORE_SELECTION
					// 削除・置換される前が選択状態だった場合、Undoで復元したテキストを
					// 改めて選択状態にする(VS Code等と同じ挙動)。設定でオフにできる。
					// bFastMode(大量Opeの一括Undo)ではptCaretPos_Before/Afterがこの
					// ブロックで計算されない(詳細な後処理を丸ごと省略する分岐)ため対象外。
					if( !bFastMode && pcReplaceOpe->bHadSelection &&
						GetDllShareData().m_Common.m_sEdit.m_bUndoRestoreSelection ){
						CLayoutPoint ptRestoredTo;
						GetDocument()->m_cLayoutMgr.LogicToLayout( pcReplaceOpe->m_ptCaretPos_PHY_To, &ptRestoredTo );
						m_pCommanderView->GetSelectionInfo().m_sSelectBgn.Set( ptCaretPos_Before );
						m_pCommanderView->GetSelectionInfo().m_sSelect = CLayoutRange( ptCaretPos_Before, ptRestoredTo );
					}
#endif // NKMM_
				}
				break;
			case OPE_MOVECARET:
				/* カーソルを移動 */
				if( bFastMode ){
					GetCaret().MoveCursorFastMode( pcOpe->m_ptCaretPos_PHY_After );
				}else{
					GetCaret().MoveCursor( ptCaretPos_After, false );
				}
				break;
			}

			if( bFastMode ){
				if( i == 0 ){
					GetDocument()->m_cLayoutMgr._DoLayout(false);
					GetEditWindow()->ClearViewCaretPosInfo();
					if( GetDocument()->m_nTextWrapMethodCur == WRAP_NO_TEXT_WRAP ){
						GetDocument()->m_cLayoutMgr.CalculateTextWidth();
					}
					GetDocument()->m_cLayoutMgr.LogicToLayout(
						pcOpe->m_ptCaretPos_PHY_Before,
						&ptCaretPos_Before
					);
					GetCaret().MoveCursor( ptCaretPos_Before, true );
					// 通常モードではReplaceData_CEditViewの中で設定される
					GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX();
				}else{
					GetCaret().MoveCursorFastMode( pcOpe->m_ptCaretPos_PHY_Before );
				}
			}else{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					pcOpe->m_ptCaretPos_PHY_Before,
					&ptCaretPos_Before
				);
				if( i == 0 ){
					/* カーソルを移動 */
					GetCaret().MoveCursor( ptCaretPos_Before, true );
				}else{
					/* カーソルを移動 */
					GetCaret().MoveCursor( ptCaretPos_Before, false );
				}
			}
			if( hwndProgress && (i % 100) == 0 ){
				int newPos = ::MulDiv(nOpeBlkNum - i, 100, nOpeBlkNum);
				if( newPos != nProgressPos ){
					nProgressPos = newPos;
					Progress_SetPos( hwndProgress, newPos + 1 );
					Progress_SetPos( hwndProgress, newPos );
				}
			}
			
#ifdef NKMM_FIX_UNDOREDO
			// nOpeBlkNum 分だけ必要になる
			{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					{pcOpe->m_ptCaretPos_PHY_After.x, pcOpe->m_ptCaretPos_PHY_After.y + 1},
					&ptCaretPos_After
				);
				
				m_pCommanderView->SetDrawSwitch(bDrawSwitchOld);	//	hor
				m_pCommanderView->RedrawLines(ptCaretPos_Before.y, ptCaretPos_After.y);
				m_pCommanderView->SetDrawSwitch(bDraw);
			}
#endif // NKMM_
		}

#ifdef NKMM_MULTI_CURSOR
		// マルチカーソルの一括編集(ApplyToAllCursors)がこのブロックに複数カーソル分のOpeを
		// 積んでいた場合、上のループは最後に処理したOpe(降順でi==0、ドキュメント最下段の
		// カーソルのOpeであり、プライマリが一番上にあるほど後から処理され高いindexに積まれる
		// ため、必ずしもプライマリのOpeではない)の結果をそのまま最終状態として残してしまう。
		// RestoreMultiCursorAfterUndoRedo()がnCursorSlotを手がかりに、プライマリ・各extra
		// それぞれ自身の直前の状態(位置・選択)へ明示的に戻す。bFastMode(大量Opeの一括Undo)
		// ではptCaretPos_Before等がループ内で計算されない分岐のため対象外(元々ここまで大量の
		// Opeが1コマンドに積まれるのは通常のマルチカーソル編集では起こらない規模) 20260831
		if( !bFastMode && pcOpeBlk ){
			RestoreMultiCursorAfterUndoRedo( pcOpeBlk, nOpeBlkNum, true /* bIsUndo */ );
		}
#endif // NKMM_
#ifdef NKMM_FIX_EDITVIEW_SCRBAR
		m_pCommanderView->SB_Marker_Clear(1000);
#endif // NKMM_
		m_pCommanderView->SetDrawSwitch(bDrawSwitchOld);	//	hor
		m_pCommanderView->AdjustScrollBars(); // 2007.07.22 ryoji
		if (!bDraw) {
			GetCaret().ShowEditCaret();
		}

		/* Undo後の変更フラグ */
		GetDocument()->m_cDocEditor.SetModified(bIsModified,true);	//	Jan. 22, 2002 genta

		m_pCommanderView->m_bDoing_UndoRedo = false;	/* アンドゥ・リドゥの実行中か */

		m_pCommanderView->SetBracketPairPos( true );	// 03/03/07 ai

		/* 再描画 */
		// ルーラー再描画の必要があるときは DispRuler() ではなく他の部分と同時に Call_OnPaint() で描画する	// 2010.08.20 ryoji
		// ・DispRuler() はルーラーとテキストの隙間（左側は行番号の幅に合わせた帯）を描画してくれない
		// ・行番号表示に必要な幅は OPE_INSERT/OPE_DELETE 処理内で更新されており変更があればルーラー再描画フラグに反映されている
		// ・水平スクロールもルーラー再描画フラグに反映されている
		const bool bRedrawRuler = m_pCommanderView->GetRuler().GetRedrawFlag();
#ifdef NKMM_FIX_UNDOREDO
		{
			// 変更後の行位置
			CLogicPoint ptCaretLogic_Next2 = {GetCaret().GetCaretLogicPos().x, GetCaret().GetCaretLogicPos().y + 1};  // 次の行
			CLayoutPoint ptCaretLayout_Next2;
			GetDocument()->m_cLayoutMgr.LogicToLayout(ptCaretLogic_Next2, &ptCaretLayout_Next2);
			
			// 全画面更新
			if (ptCaretLogic_Start.y != GetCaret().GetCaretLogicPos().y ||  // 物理行が違う
			    ptCaretLayout_Next.y != ptCaretLayout_Next2.y ||  // 次の論理行が違う
			    nViewLeftCol != m_pCommanderView->GetTextArea().GetViewLeftCol()  // 桁位置が変わっている
			) {
				m_pCommanderView->Call_OnPaint( PAINT_LINENUMBER | PAINT_BODY | (bRedrawRuler? PAINT_RULER: 0), false );
			}
		}
#else
		m_pCommanderView->Call_OnPaint( PAINT_LINENUMBER | PAINT_BODY | (bRedrawRuler? PAINT_RULER: 0), false );
#endif // NKMM_
		if( !bRedrawRuler ){
			// ルーラーのキャレットのみを再描画
			HDC hdc = m_pCommanderView->GetDC();
			m_pCommanderView->GetRuler().DispRuler( hdc );
			m_pCommanderView->ReleaseDC( hdc );
		}

		GetCaret().ShowCaretPosInfo();	// キャレットの行桁位置を表示する	// 2007.10.19 ryoji

		if( !GetEditWindow()->UpdateTextWrap() && bDrawAll ){	// 折り返し方法関連の更新	// 2008.06.10 ryoji
			GetEditWindow()->RedrawAllViews( m_pCommanderView );	//	他のペインの表示を更新
		}

		if(hwndProgress) ::ShowWindow( hwndProgress, SW_HIDE );
	}

	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().x;	// 2007.10.11 ryoji 追加
	m_pCommanderView->m_bDoing_UndoRedo = false;	/* アンドゥ・リドゥの実行中か */

	return;
}



//	from CViewCommander_New.cpp
/* Redo やり直し */
void CViewCommander::Command_REDO( void )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){	/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	{
		COpeBlk* opeBlk = m_pCommanderView->m_cCommander.GetOpeBlk();
		if( opeBlk ){
			int nCount = opeBlk->GetRefCount();
			opeBlk->SetRefCount(1); // 強制的にリセットするため1を指定
			m_pCommanderView->SetUndoBuffer();
			if( m_pCommanderView->m_cCommander.GetOpeBlk() == NULL && 0 < nCount ){
				m_pCommanderView->m_cCommander.SetOpeBlk(new COpeBlk());
				m_pCommanderView->m_cCommander.GetOpeBlk()->SetRefCount( nCount );
			}
		}
		// 注意：Opeを追加するとRedoはできなくなる
	}

	if( !GetDocument()->m_cDocEditor.IsEnableRedo() ){	/* Redo(やり直し)可能な状態か？ */
		return;
	}
	MY_RUNNINGTIMER( cRunningTimer, "CViewCommander::Command_REDO()" );

	COpe*		pcOpe = NULL;
	COpeBlk*	pcOpeBlk;
	int			nOpeBlkNum;
	int			i;
//	int			nNewLine;	/* 挿入された部分の次の位置の行 */
//	int			nNewPos;	/* 挿入された部分の次の位置のデータ位置 */
	bool		bIsModified;

	CLayoutPoint ptCaretPos_Before;
	CLayoutPoint ptCaretPos_To;
	CLayoutPoint ptCaretPos_After;

#ifdef NKMM_FIX_UNDOREDO
	// 表示域の一番左の桁
	CLayoutInt nViewLeftCol = m_pCommanderView->GetTextArea().GetViewLeftCol();
	
	// 処理前の行位置
	CLogicPoint ptCaretLogic_Start = GetCaret().GetCaretLogicPos();  // 現在行
	CLayoutPoint ptCaretLayout_Start = GetCaret().GetCaretLayoutPos();
	
	// 変更前の行位置
	CLogicPoint ptCaretLogic_Next = {ptCaretLogic_Start.x, ptCaretLogic_Start.y + 1};  // 次の行
	CLayoutPoint ptCaretLayout_Next;
	
	GetDocument()->m_cLayoutMgr.LogicToLayout(
		ptCaretLogic_Next,
		&ptCaretLayout_Next,
		ptCaretLayout_Start.y
	);
#endif // NKMM_

	/* 各種モードの取り消し */
	Command_CANCEL_MODE();

	m_pCommanderView->m_bDoing_UndoRedo = true;	/* アンドゥ・リドゥの実行中か */

	/* 現在のRedo対象の操作ブロックを返す */
	if( NULL != ( pcOpeBlk = GetDocument()->m_cDocEditor.m_cOpeBuf.DoRedo( &bIsModified ) ) ){
		nOpeBlkNum = pcOpeBlk->GetNum();
		bool bDraw = (nOpeBlkNum < 5) && m_pCommanderView->GetDrawSwitch();
		bool bDrawAll = false;
		const bool bDrawSwitchOld = m_pCommanderView->SetDrawSwitch(bDraw);	// 2007.07.22 ryoji

		CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd(), 1000 < nOpeBlkNum );
		HWND hwndProgress = NULL;
		int nProgressPos = 0;
		if( cWaitCursor.IsEnable() ){
			hwndProgress = m_pCommanderView->StartProgress();
		}

		const bool bFastMode = (100 < nOpeBlkNum);
		for( i = 0; i < nOpeBlkNum; ++i ){
			pcOpe = pcOpeBlk->GetOpe( i );
			if( bFastMode ){
				if( i == 0 ){
					GetDocument()->m_cLayoutMgr.LogicToLayout(
						pcOpe->m_ptCaretPos_PHY_Before,
						&ptCaretPos_Before
					);
					GetCaret().MoveCursor( ptCaretPos_Before, true );
				}else{
					GetCaret().MoveCursorFastMode( pcOpe->m_ptCaretPos_PHY_Before );
				}
			}else{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					pcOpe->m_ptCaretPos_PHY_Before,
					&ptCaretPos_Before
				);
				if( i == 0 ){
					GetCaret().MoveCursor( ptCaretPos_Before, true );
				}else{
					GetCaret().MoveCursor( ptCaretPos_Before, false );
				}
			}
			switch( pcOpe->GetCode() ){
			case OPE_INSERT:
				{
					CInsertOpe* pcInsertOpe = static_cast<CInsertOpe*>(pcOpe);

					//2007.10.17 kobake メモリリークしてました。修正。
					if( 0 < pcInsertOpe->m_cOpeLineData.size() ){
						/* データ置換 削除&挿入にも使える */
						CLayoutRange sRange;
						sRange.Set(ptCaretPos_Before);
						CLogicRange cSelectLogic;
						cSelectLogic.Set(pcOpe->m_ptCaretPos_PHY_Before);
						bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
							sRange,
							NULL,										/* 削除されたデータのコピー(NULL可能) */
							&pcInsertOpe->m_cOpeLineData,				/* 挿入するデータ */
							bDraw,										/*再描画するか否か*/
							NULL,
							0,
							&pcInsertOpe->m_nOrgSeq,
							bFastMode,
							&cSelectLogic
						);

					}
					pcInsertOpe->m_cOpeLineData.clear();
				}
				break;
			case OPE_DELETE:
				{
					CDeleteOpe* pcDeleteOpe = static_cast<CDeleteOpe*>(pcOpe);

					if( bFastMode ){
					}else{
						GetDocument()->m_cLayoutMgr.LogicToLayout(
							pcDeleteOpe->m_ptCaretPos_PHY_To,
							&ptCaretPos_To
						);
					}
					CLogicRange cSelectLogic;
					cSelectLogic.SetFrom(pcOpe->m_ptCaretPos_PHY_Before);
					cSelectLogic.SetTo(pcDeleteOpe->m_ptCaretPos_PHY_To);

					/* データ置換 削除&挿入にも使える */
					bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
						CLayoutRange(ptCaretPos_Before,ptCaretPos_To),
						&pcDeleteOpe->m_cOpeLineData,	/* 削除されたデータのコピー(NULL可能) */
						NULL,
						bDraw,
						NULL,
						pcDeleteOpe->m_nOrgSeq,
						NULL,
						bFastMode,
						&cSelectLogic
					);
				}
				break;
			case OPE_REPLACE:
				{
					CReplaceOpe* pcReplaceOpe = static_cast<CReplaceOpe*>(pcOpe);

					if( bFastMode ){
					}else{
						GetDocument()->m_cLayoutMgr.LogicToLayout(
							pcReplaceOpe->m_ptCaretPos_PHY_To,
							&ptCaretPos_To
						);
					}
					CLogicRange cSelectLogic;
					cSelectLogic.SetFrom(pcOpe->m_ptCaretPos_PHY_Before);
					cSelectLogic.SetTo(pcReplaceOpe->m_ptCaretPos_PHY_To);

					/* データ置換 削除&挿入にも使える */
					bDrawAll |= m_pCommanderView->ReplaceData_CEditView3(
						CLayoutRange(ptCaretPos_Before,ptCaretPos_To),
						&pcReplaceOpe->m_pcmemDataDel,	// 削除されたデータのコピー(NULL可能)
						&pcReplaceOpe->m_pcmemDataIns,	// 挿入するデータ
						bDraw,
						NULL,
						pcReplaceOpe->m_nOrgDelSeq,
						&pcReplaceOpe->m_nOrgInsSeq,
						bFastMode,
						&cSelectLogic
					);
					pcReplaceOpe->m_pcmemDataIns.clear();
				}
				break;
			case OPE_MOVECARET:
				break;
			}
			if( bFastMode ){
				if( i == nOpeBlkNum - 1	){
					GetDocument()->m_cLayoutMgr._DoLayout(false);
					GetEditWindow()->ClearViewCaretPosInfo();
					if( GetDocument()->m_nTextWrapMethodCur == WRAP_NO_TEXT_WRAP ){
						GetDocument()->m_cLayoutMgr.CalculateTextWidth();
					}
					GetDocument()->m_cLayoutMgr.LogicToLayout(
						pcOpe->m_ptCaretPos_PHY_After, &ptCaretPos_After );
					GetCaret().MoveCursor( ptCaretPos_After, true );
					// 通常モードではReplaceData_CEditViewの中で設定される
					GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX();
				}else{
					GetCaret().MoveCursorFastMode( pcOpe->m_ptCaretPos_PHY_After );
				}
			}else{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					pcOpe->m_ptCaretPos_PHY_After, &ptCaretPos_After );
				if( i == nOpeBlkNum - 1	){
					GetCaret().MoveCursor( ptCaretPos_After, true );
				}else{
					GetCaret().MoveCursor( ptCaretPos_After, false );
				}
			}
			if( hwndProgress && (i % 100) == 0 ){
				int newPos = ::MulDiv(i + 1, 100, nOpeBlkNum);
				if( newPos != nProgressPos ){
					nProgressPos = newPos;
					Progress_SetPos( hwndProgress, newPos + 1 );
					Progress_SetPos( hwndProgress, newPos );
				}
			}
			
#ifdef NKMM_FIX_UNDOREDO
			// nOpeBlkNum 分だけ必要になる
			{
				GetDocument()->m_cLayoutMgr.LogicToLayout(
					{pcOpe->m_ptCaretPos_PHY_After.x, pcOpe->m_ptCaretPos_PHY_After.y + 1},
					&ptCaretPos_After
				);
				
				m_pCommanderView->SetDrawSwitch(bDrawSwitchOld);	//	hor
				m_pCommanderView->RedrawLines(ptCaretPos_Before.y, ptCaretPos_After.y);
				m_pCommanderView->SetDrawSwitch(bDraw);
			}
#endif // NKMM_
		}

#ifdef NKMM_MULTI_CURSOR
		// Undo側(Command_UNDO)と全く同じ理由: マルチカーソルの一括編集(ApplyToAllCursors)が
		// このブロックに複数カーソル分のOpeを積んでいた場合、昇順走査でも「最後に処理したOpe」
		// (i==nOpeBlkNum-1)が必ずしもプライマリのOpeとは限らない。RestoreMultiCursorAfterUndoRedo()
		// がnCursorSlotを手がかりに、プライマリ・各extraそれぞれ自身の直後の位置へ明示的に戻す。
		// Redoでは元々選択状態を復元しない(削除後に選択すべき対象が残らないため、単一カーソル時の
		// Redoも一貫して選択を出さない仕様) 20260831
		if( !bFastMode && pcOpeBlk ){
			RestoreMultiCursorAfterUndoRedo( pcOpeBlk, nOpeBlkNum, false /* bIsUndo */ );
		}
#endif // NKMM_
		m_pCommanderView->SetDrawSwitch(bDrawSwitchOld); // 2007.07.22 ryoji
		m_pCommanderView->AdjustScrollBars(); // 2007.07.22 ryoji
		if (!bDraw) {
			GetCaret().ShowEditCaret();
		}

		/* Redo後の変更フラグ */
		GetDocument()->m_cDocEditor.SetModified(bIsModified,true);	//	Jan. 22, 2002 genta

		m_pCommanderView->m_bDoing_UndoRedo = false;	/* アンドゥ・リドゥの実行中か */

		m_pCommanderView->SetBracketPairPos( true );	// 03/03/07 ai

		/* 再描画 */
		// ルーラー再描画の必要があるときは DispRuler() ではなく他の部分と同時に Call_OnPaint() で描画する	// 2010.08.20 ryoji
		// ・DispRuler() はルーラーとテキストの隙間（左側は行番号の幅に合わせた帯）を描画してくれない
		// ・行番号表示に必要な幅は OPE_INSERT/OPE_DELETE 処理内で更新されており変更があればルーラー再描画フラグに反映されている
		// ・水平スクロールもルーラー再描画フラグに反映されている
		const bool bRedrawRuler = m_pCommanderView->GetRuler().GetRedrawFlag();
#ifdef NKMM_FIX_UNDOREDO
		{
			// 変更後の行位置
			CLogicPoint ptCaretLogic_Next2 = {GetCaret().GetCaretLogicPos().x, GetCaret().GetCaretLogicPos().y + 1};  // 次の行
			CLayoutPoint ptCaretLayout_Next2;
			GetDocument()->m_cLayoutMgr.LogicToLayout(ptCaretLogic_Next2, &ptCaretLayout_Next2);
			
			// 全画面更新
			if (ptCaretLogic_Start.y != GetCaret().GetCaretLogicPos().y ||  // 物理行が違う
			    ptCaretLayout_Next.y != ptCaretLayout_Next2.y ||  // 次の論理行が違う
			    nViewLeftCol != m_pCommanderView->GetTextArea().GetViewLeftCol()  // 桁位置が変わっている
			) {
				m_pCommanderView->Call_OnPaint( PAINT_LINENUMBER | PAINT_BODY | (bRedrawRuler? PAINT_RULER: 0), false );
			}
		}
#else
		m_pCommanderView->Call_OnPaint( PAINT_LINENUMBER | PAINT_BODY | (bRedrawRuler? PAINT_RULER: 0), false );
#endif // NKMM_
		if( !bRedrawRuler ){
			// ルーラーのキャレットのみを再描画
			HDC hdc = m_pCommanderView->GetDC();
			m_pCommanderView->GetRuler().DispRuler( hdc );
			m_pCommanderView->ReleaseDC( hdc );
		}

		GetCaret().ShowCaretPosInfo();	// キャレットの行桁位置を表示する	// 2007.10.19 ryoji

		if( !GetEditWindow()->UpdateTextWrap() && bDrawAll ){	// 折り返し方法関連の更新	// 2008.06.10 ryoji
			GetEditWindow()->RedrawAllViews( m_pCommanderView );	//	他のペインの表示を更新
		}

		if(hwndProgress) ::ShowWindow( hwndProgress, SW_HIDE );
	}

	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().x;	// 2007.10.11 ryoji 追加
	m_pCommanderView->m_bDoing_UndoRedo = false;	/* アンドゥ・リドゥの実行中か */

	return;
}



//カーソル位置または選択エリアを削除
void CViewCommander::Command_DELETE( void )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){		/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	if( !m_pCommanderView->GetSelectionInfo().IsTextSelected() ){	/* テキストが選択されているか */
		// 2008.08.03 nasukoji	選択範囲なしでDELETEを実行した場合、カーソル位置まで半角スペースを挿入した後改行を削除して次行と連結する
		if( GetDocument()->m_cLayoutMgr.GetLineCount() > GetCaret().GetCaretLayoutPos().GetY2() ){
			const CLayout* pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( GetCaret().GetCaretLayoutPos().GetY2() );
			if( pcLayout ){
				CLayoutInt nLineLen;
				CLogicInt nIndex;
				nIndex = m_pCommanderView->LineColumnToIndex2( pcLayout, GetCaret().GetCaretLayoutPos().GetX2(), &nLineLen );
				if( nLineLen != 0 ){	// 折り返しや改行コードより右の場合には nLineLen に行全体の表示桁数が入る
					if( EOL_NONE != pcLayout->GetLayoutEol().GetType() ){	// 行終端は改行コードか?
						Command_INSTEXT( true, L"", CLogicInt(0), FALSE );	// カーソル位置まで半角スペース挿入
					}else{	// 行終端が折り返し
						// 折り返し行末ではスペース挿入後、次の文字を削除する	// 2009.02.19 ryoji

						// フリーカーソル時の折り返し越え位置での削除はどうするのが妥当かよくわからないが
						// 非フリーカーソル時（ちょうどカーソルが折り返し位置にある）には次の行の先頭文字を削除したい

						if( nLineLen < GetCaret().GetCaretLayoutPos().GetX2() ){	// 折り返し行末とカーソルの間に隙間がある
							Command_INSTEXT( true, L"", CLogicInt(0), FALSE );	// カーソル位置まで半角スペース挿入
							pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( GetCaret().GetCaretLayoutPos().GetY2() );
							nIndex = m_pCommanderView->LineColumnToIndex2( pcLayout, GetCaret().GetCaretLayoutPos().GetX2(), &nLineLen );
						}
						if( nLineLen != 0 ){	// （スペース挿入後も）折り返し行末なら次文字を削除するために次行の先頭に移動する必要がある
							if( pcLayout->GetNextLayout() != NULL ){	// 最終行末ではない
								CLayoutPoint ptLay;
								CLogicPoint ptLog(pcLayout->GetLogicOffset() + nIndex, pcLayout->GetLogicLineNo());
								GetDocument()->m_cLayoutMgr.LogicToLayout( ptLog, &ptLay );
								GetCaret().MoveCursor( ptLay, true );
								GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();
							}
						}
					}
				}
			}
		}
	}
	m_pCommanderView->DeleteData( true );
	return;
}



//カーソル前を削除
void CViewCommander::Command_DELETE_BACK( void )
{
	if( m_pCommanderView->GetSelectionInfo().IsMouseSelecting() ){	/* マウスによる範囲選択中 */
		ErrorBeep();
		return;
	}

	//	May 29, 2004 genta 実際に削除された文字がないときはフラグをたてないように
	//GetDocument()->m_cDocEditor.SetModified(true,true);	//	Jan. 22, 2002 genta
	if( m_pCommanderView->GetSelectionInfo().IsTextSelected() ){				/* テキストが選択されているか */
		m_pCommanderView->DeleteData( true );
	}
	else{
		CLayoutPoint	ptLayoutPos_Old = GetCaret().GetCaretLayoutPos();
		CLogicPoint		ptLogicPos_Old = GetCaret().GetCaretLogicPos();
		BOOL	bBool = Command_LEFT( false, false );
		if( bBool ){
			const CLayout* pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( GetCaret().GetCaretLayoutPos().GetY2() );
			if( pcLayout ){
				CLayoutInt nLineLen;
				CLogicInt nIdx = m_pCommanderView->LineColumnToIndex2( pcLayout, GetCaret().GetCaretLayoutPos().GetX2(), &nLineLen );
				if( nLineLen == 0 ){	// 折り返しや改行コードより右の場合には nLineLen に行全体の表示桁数が入る
					// 右からの移動では折り返し末尾文字は削除するが改行は削除しない
					// 下から（下の行の行頭から）の移動では改行も削除する
					if( nIdx < pcLayout->GetLengthWithoutEOL() || GetCaret().GetCaretLayoutPos().GetY2() < ptLayoutPos_Old.GetY2() ){
						if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* アンドゥ・リドゥの実行中か */
							/* 操作の追加 */
							GetOpeBlk()->AppendOpe(
								new CMoveCaretOpe(
									ptLogicPos_Old,
									GetCaret().GetCaretLogicPos()
								)
							);
						}
						m_pCommanderView->DeleteData( true );
					}
				}
			}
		}
	}
	m_pCommanderView->PostprocessCommand_hokan();	//	Jan. 10, 2005 genta 関数化
}



#ifdef NKMM_MULTI_CURSOR
//! Command_AddCursorUp/Downの共通実装。nDir<0で最上段カーソルの1行上、nDir>0で最下段
//! カーソルの1行下(いずれも同じ桁)に新しいカーソルを追加する。上下で対称なだけの処理
//! だったため統合 20260831
void CViewCommander::AddCursorInDirection( int nDir )
{
	// マルチカーソルと矩形選択は排他。矩形選択中であれば解除してから追加する
	if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( true );
	}

	if( (int)m_pCommanderView->m_vExtraCursors.size() >= NKMM_MULTICURSOR_MAX ){
		ErrorBeep();
		return;
	}

	// プライマリ(相対0)+追加カーソルの中から最上段/最下段(nRelLineが最小/最大)を探す。
	// 新カーソルの相対桁は「その端のカーソルと同じ相対桁」を引き継ぐ(プライマリなら0)
	int nEdgeRel = 0;
	int nEdgeRelColumn = 0;
	for( const auto& extra : m_pCommanderView->m_vExtraCursors ){
		if( ( nDir < 0 ) ? ( extra.nRelLine < nEdgeRel ) : ( extra.nRelLine > nEdgeRel ) ){
			nEdgeRel = extra.nRelLine;
			nEdgeRelColumn = extra.nRelColumn;
		}
	}

	int nNewRel = nEdgeRel + nDir;
	CLayoutInt nPrimaryLine = GetCaret().GetCaretLayoutPos().GetY2();
	if( nDir < 0 ){
		if( ToInt(nPrimaryLine) + nNewRel < 0 ){
			// 文書先頭を超えるので追加しない(この第一弾ではカーソル追加時点での
			// 境界超過は非対応。既存カーソルの上下移動での「非アクティブ」機構とは別)
			return;
		}
	}else{
		CLayoutInt nDocLineCount = GetDocument()->m_cLayoutMgr.GetLineCount();
		if( ToInt(nDocLineCount) <= ToInt(nPrimaryLine) + nNewRel ){
			// 文書末尾を超えるので追加しない
			return;
		}
	}

	{
		CEditView::SExtraCursor ne;
		ne.nRelLine = nNewRel;
		ne.nRelColumn = nEdgeRelColumn;
		// 希望桁(nDesiredRelColumn)は「今の実桁と同じ、プライマリの希望桁からの相対値」として
		// 初期化する(作成直後は希望と実桁が一致している)。プライマリ自身の実桁と希望桁が
		// (上下移動の途中で)食い違っているケースにも正しく対応するため、それぞれ別々に基準を取る
		int nNewAbsColumn = ToInt(GetCaret().GetCaretLayoutPos().GetX2()) + nEdgeRelColumn;
		ne.nDesiredRelColumn = nNewAbsColumn - ToInt(GetCaret().m_nCaretPosX_Prev);
		m_pCommanderView->m_vExtraCursors.push_back( ne );
	}
	m_pCommanderView->Redraw();
}

//! マルチカーソル: 現在の最上段カーソルの1行上(同じ桁)に新しいカーソルを追加する	20260830
void CViewCommander::Command_AddCursorUp( void )
{
	AddCursorInDirection( -1 );
}

//! マルチカーソル: 現在の最下段カーソルの1行下(同じ桁)に新しいカーソルを追加する	20260830
void CViewCommander::Command_AddCursorDown( void )
{
	AddCursorInDirection( 1 );
}

//! マルチカーソル: 直近に追加したカーソルを1個取り消す(VS CodeのCtrl+U相当)	20260830
//! テキスト編集のUndo(Ctrl+Z)とは無関係の別系統。m_vExtraCursorsは追加順のLIFOに
//! なっているため、末尾を1個popするだけでよい。
void CViewCommander::Command_MULTICURSOR_UNDO( void )
{
	if( !m_pCommanderView->m_vExtraCursors.empty() ){
		m_pCommanderView->m_vExtraCursors.pop_back();
		m_pCommanderView->Redraw();
	}
}

//! マルチカーソル: プライマリ+追加カーソルの各位置に対して1回ずつfnEditOnceを実行する	20260830
//!
//! ドキュメント降順(末尾側から)で処理する。ある位置への編集は、その位置より手前
//! (まだ処理していない)カーソルの位置には影響しないため、編集のたびに他カーソルの
//! 位置を補正し直す必要がない。fnEditOnceにはCommand_WCHAR等、現在のGetCaret()位置に
//! 作用する既存コマンドをそのまま渡す想定(挙動を完全に共有できる)。
//!
//! Undo/Redoは、この呼び出し元(HandleCommand)が既に開いている単一のCOpeBlkに
//! 全カーソル分のCOpeがそのまま積まれるため、追加の実装なしに1回のUndo/Redoとしてまとまる
//! (CDocVisitor::SetAllEolと同じ考え方)。
void CViewCommander::ApplyToAllCursors( const std::function<void()>& fnEditOnce )
{
	struct SSlot{ CLayoutPoint ptCaret; bool bPrimary; size_t nExtraIdx; };
	std::vector<SSlot> vAll;
	vAll.reserve( m_pCommanderView->m_vExtraCursors.size() + 1 );
	vAll.push_back( { GetCaret().GetCaretLayoutPos(), true, 0 } );
	for( size_t i = 0; i < m_pCommanderView->m_vExtraCursors.size(); ++i ){
		// 非アクティブ(実位置がドキュメント範囲外)のカーソルは編集にも参加させない。
		// プライマリが動いて範囲内に戻るまで待つ
		CLayoutPoint ptResolved;
		if( !m_pCommanderView->ResolveExtraCursor( m_pCommanderView->m_vExtraCursors[i], &ptResolved ) ) continue;
		vAll.push_back( { ptResolved, false, i } );
	}

	std::sort( vAll.begin(), vAll.end(), []( const SSlot& a, const SSlot& b ){
		if( a.ptCaret.GetY2() != b.ptCaret.GetY2() ) return a.ptCaret.GetY2() > b.ptCaret.GetY2();
		return a.ptCaret.GetX2() > b.ptCaret.GetX2();
	} );

	// 各extraの選択起点(アンカー)の実位置は、ループでプライマリのm_sSelectBgnが書き換わって
	// しまう前に確定させておく(ResolveExtraCursorAnchorはプライマリの現在のm_sSelectBgnを
	// 参照するため)。この関数はfnEditOnceとして編集系(タイピング・削除)と選択系移動の両方を
	// 受け取れる。各カーソルは「自分自身のアンカー〜自分自身の現在位置」という、プライマリの
	// 結果を平行移動するのではない独立した選択状態を持つ(SExtraCursor参照) 20260831
	// プライマリ自身の選択状態(ループ開始前の本来の状態)も、extraと同じくここで確定させて
	// おく。ループ中は他カーソルの処理でGetSelectionInfo()のm_sSelectBgn/m_sSelectが
	// 都度上書きされるため、「プライマリはそのまま触らない」という前提は、プライマリより先に
	// 処理されるextraが1つでもあると成立しない(降順ソートのため、プライマリより後ろの行に
	// extraがあると必ずそうなる)。プライマリの番が来るたびに明示的に復元する必要がある 20260831
	CLayoutRange sPrimarySelectBgnOrig = m_pCommanderView->GetSelectionInfo().m_sSelectBgn;
	CLayoutRange sPrimarySelectOrig = m_pCommanderView->GetSelectionInfo().m_sSelect;
	// プライマリの「希望桁」(CCaret::m_nCaretPosX_Prev、上下移動時の着地列を決めるためだけに
	// 単一カーソル側が既に持っている値)も、選択状態と全く同じ理由でループ開始前に確定させておく。
	// ApplyToAllCursorsは実カーソルを使い回して各カーソルを順番に"演じる"ため、fnEditOnceの
	// 中身(Command_UP/DOWN等)は単一カーソル用の実装がそのままGetCaret().m_nCaretPosX_Prevを
	// 読み書きする。これを各extra自身の「希望桁」の記憶場所としてそのまま流用する 20260831
	CLayoutInt nPrimaryGoalOrig = GetCaret().m_nCaretPosX_Prev;

	std::vector<CLayoutPoint> vExtraAnchor( vAll.size() );
	std::vector<bool> vHadSelection( vAll.size(), false );
	std::vector<CLayoutInt> vExtraGoal( vAll.size() );
	for( size_t j = 0; j < vAll.size(); ++j ){
		if( vAll[j].bPrimary ){
			vHadSelection[j] = m_pCommanderView->GetSelectionInfo().IsTextSelected();
			continue;
		}
		const auto& extra = m_pCommanderView->m_vExtraCursors[vAll[j].nExtraIdx];
		if( extra.bHasSelection && m_pCommanderView->ResolveExtraCursorAnchor( extra, &vExtraAnchor[j] ) ){
			vHadSelection[j] = true;
		}
		vExtraGoal[j] = nPrimaryGoalOrig + CLayoutInt(extra.nDesiredRelColumn);
	}

	CLayoutPoint ptPrimaryNew;
	CLayoutRange sPrimarySelectBgnNew, sPrimarySelectNew;
	CLayoutInt nPrimaryGoalNew = nPrimaryGoalOrig;
	for( size_t j = 0; j < vAll.size(); ++j ){
		auto& slot = vAll[j];
		GetCaret().MoveCursor( slot.ptCaret, false );

		if( slot.bPrimary ){
			// プライマリの選択状態・希望桁を、ループ開始前の本来の状態に明示的に復元してから使う
			// (単一カーソル時と全く同じ挙動になる)
			m_pCommanderView->GetSelectionInfo().m_sSelectBgn = sPrimarySelectBgnOrig;
			m_pCommanderView->GetSelectionInfo().m_sSelect = sPrimarySelectOrig;
			GetCaret().m_nCaretPosX_Prev = nPrimaryGoalOrig;
		}
		else if( vHadSelection[j] ){
			CLayoutPoint ptFrom = vExtraAnchor[j];
			CLayoutPoint ptTo = slot.ptCaret;
			m_pCommanderView->GetSelectionInfo().m_sSelectBgn.Set( ptFrom );
			if( PointCompare( ptTo, ptFrom ) < 0 ) std::swap( ptFrom, ptTo );
			m_pCommanderView->GetSelectionInfo().m_sSelect = CLayoutRange( ptFrom, ptTo );
			GetCaret().m_nCaretPosX_Prev = vExtraGoal[j];
		}
		else{
			m_pCommanderView->GetSelectionInfo().DisableSelectArea( false );
			GetCaret().m_nCaretPosX_Prev = vExtraGoal[j];
		}

		// Undo/Redo時にこのOpeがどのカーソルの編集によるものか識別できるよう、fnEditOnceが
		// このターンで積んだOpe(1個とは限らない。選択の左右関係次第でCMoveCaretOpeが
		// 前置されることがある)にマークを付ける。プライマリ=0、extraはこの編集時点での
		// m_vExtraCursors内index+1。Command_UNDO/REDO側でブロック内の「最後に処理したOpe」
		// に頼らず各カーソル自身の状態へ確実に戻すために使う 20260831
		COpeBlk* pcOpeBlkForMark = GetOpeBlk();
		int nOpeCountBeforeEdit = pcOpeBlkForMark ? pcOpeBlkForMark->GetNum() : 0;
		int nCursorSlotForMark = slot.bPrimary ? 0 : (int)(slot.nExtraIdx + 1);

		fnEditOnce();

		if( pcOpeBlkForMark ){
			for( int nMarkIdx = nOpeCountBeforeEdit; nMarkIdx < pcOpeBlkForMark->GetNum(); ++nMarkIdx ){
				pcOpeBlkForMark->GetOpe( nMarkIdx )->nCursorSlot = nCursorSlotForMark;
			}
		}

		CLayoutPoint ptNew = GetCaret().GetCaretLayoutPos();
		bool bNowSelected = m_pCommanderView->GetSelectionInfo().IsTextSelected();
		CLayoutPoint ptAnchorNew = m_pCommanderView->GetSelectionInfo().m_sSelectBgn.GetFrom();
		// fnEditOnceが実際に呼んだ単一カーソル用コマンドが、GetCaret().m_nCaretPosX_Prevを
		// 適切に更新済み(上下移動なら据え置き、それ以外なら新しい実桁に上書き)なので、
		// ここでそのまま読み出すだけでよい。判定ロジックをここで作り直す必要はない
		CLayoutInt nGoalResult = GetCaret().m_nCaretPosX_Prev;

		if( slot.bPrimary ){
			ptPrimaryNew = ptNew;
			sPrimarySelectBgnNew = m_pCommanderView->GetSelectionInfo().m_sSelectBgn;
			sPrimarySelectNew = m_pCommanderView->GetSelectionInfo().m_sSelect;
			nPrimaryGoalNew = nGoalResult;
		}else{
			// 結果を基に、新しいプライマリ位置が確定してから相対値を再計算する必要があるため、
			// いったん絶対位置のまま覚えておく(下で相対化する)
			auto& extra = m_pCommanderView->m_vExtraCursors[slot.nExtraIdx];
			extra.nRelLine = ToInt(ptNew.GetY2());
			extra.nRelColumn = ToInt(ptNew.GetX2());
			extra.bHasSelection = bNowSelected;
			if( bNowSelected ){
				extra.nAnchorRelLine = ToInt(ptAnchorNew.GetY2());
				extra.nAnchorRelColumn = ToInt(ptAnchorNew.GetX2());
			}
			extra.nDesiredRelColumn = ToInt(nGoalResult);
		}
	}

	// 降順ループの都合上、最後に処理したのがプライマリとは限らないため、
	// プライマリカーソル・選択状態・希望桁を新しい位置へ明示的に戻し、ビューへ反映する
	GetCaret().MoveCursor( ptPrimaryNew, true );
	m_pCommanderView->GetSelectionInfo().m_sSelectBgn = sPrimarySelectBgnNew;
	m_pCommanderView->GetSelectionInfo().m_sSelect = sPrimarySelectNew;
	GetCaret().m_nCaretPosX_Prev = nPrimaryGoalNew;

	// 上のループで一時的に絶対位置を入れていた処理済みextraを、プライマリの確定位置基準の
	// 相対値に変換し直す(未処理=非アクティブだったextraは元の相対値のままなので触らない)
	for( auto& slot : vAll ){
		if( slot.bPrimary ) continue;
		auto& extra = m_pCommanderView->m_vExtraCursors[slot.nExtraIdx];
		extra.nRelLine = extra.nRelLine - ToInt(ptPrimaryNew.GetY2());
		extra.nRelColumn = extra.nRelColumn - ToInt(ptPrimaryNew.GetX2());
		extra.nDesiredRelColumn = extra.nDesiredRelColumn - ToInt(nPrimaryGoalNew);
		if( extra.bHasSelection ){
			extra.nAnchorRelLine = extra.nAnchorRelLine - ToInt(sPrimarySelectBgnNew.GetFrom().GetY2());
			extra.nAnchorRelColumn = extra.nAnchorRelColumn - ToInt(sPrimarySelectBgnNew.GetFrom().GetX2());
		}
	}

	MergeOverlappingCursorsIfNeeded();

	m_pCommanderView->Redraw();
}

//! マルチカーソル: 重なった(接した)カーソル同士を1個に統合する	20260831
//! VS Codeのeditor.multiCursorMergeOverlapping(既定オン)と同じ考え方。設定
//! (m_Common.m_sEdit.m_bMultiCursorMergeOverlapping)がオフなら何もしない。
//!
//! プライマリ+各extraを「アンカー〜現在位置」の範囲(選択なしは幅0)として集め、開始位置
//! 昇順に並べて隣接ペアを走査する。VS Codeのnormalize()と同じ判定式(片方が幅0の選択なら
//! 接しているだけでも統合、両方に幅があれば真に重なっている場合のみ統合)。統合後の範囲は
//! 両者を包含する形にし、方向(アンカー側)は「後から追加された側」を優先して引き継ぐ
//! (m_vExtraCursorsは追加順のLIFOなので、vector内indexが大きいほど後から追加=優先)。
//! プライマリは常にnOrder最大として扱い、プライマリが関わる統合では必ずプライマリの
//! 識別が生き残る(プライマリという実体は消せないため)。
//!
//! この関数はApplyToAllCursorsの末尾、全カーソルの位置が確定した直後に呼ばれる想定。
void CViewCommander::MergeOverlappingCursorsIfNeeded( void )
{
	if( !GetDllShareData().m_Common.m_sEdit.m_bMultiCursorMergeOverlapping ) return;
	if( m_pCommanderView->m_vExtraCursors.empty() ) return;

	struct SMergeEntry{
		bool		bPrimary;
		size_t		nExtraIdx;		// bPrimary==falseのときのみ有効(統合前の元index)
		int			nOrder;			// 大きいほど後から追加=優先。プライマリは常に最大
		CLayoutPoint ptLow, ptHigh;	// 選択なしならLow==High==カーソル位置
		bool		bAnchorHigh;	// アンカーがptHigh側にあるか(選択なしなら無意味)
		bool		bHasSelection;
	};
	std::vector<SMergeEntry> vEntries;
	vEntries.reserve( m_pCommanderView->m_vExtraCursors.size() + 1 );

	{
		SMergeEntry e;
		e.bPrimary = true;
		e.nExtraIdx = 0;
		e.nOrder = (int)m_pCommanderView->m_vExtraCursors.size();	// 常にどのextraのindexよりも大きい
		e.bHasSelection = m_pCommanderView->GetSelectionInfo().IsTextSelected();
		if( e.bHasSelection ){
			e.ptLow  = m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom();
			e.ptHigh = m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo();
			e.bAnchorHigh = ( m_pCommanderView->GetSelectionInfo().m_sSelectBgn.GetFrom() == e.ptHigh );
		}else{
			e.ptLow = e.ptHigh = GetCaret().GetCaretLayoutPos();
			e.bAnchorHigh = false;
		}
		vEntries.push_back( e );
	}
	for( size_t i = 0; i < m_pCommanderView->m_vExtraCursors.size(); ++i ){
		const auto& extra = m_pCommanderView->m_vExtraCursors[i];
		CLayoutPoint ptCur;
		if( !m_pCommanderView->ResolveExtraCursor( extra, &ptCur ) ) continue;	// 非アクティブは統合対象外

		SMergeEntry e;
		e.bPrimary = false;
		e.nExtraIdx = i;
		e.nOrder = (int)i;
		e.bHasSelection = false;
		e.ptLow = e.ptHigh = ptCur;
		e.bAnchorHigh = false;
		if( extra.bHasSelection ){
			CLayoutPoint ptAnchor;
			if( m_pCommanderView->ResolveExtraCursorAnchor( extra, &ptAnchor ) && ptAnchor != ptCur ){
				e.bHasSelection = true;
				if( PointCompare( ptAnchor, ptCur ) <= 0 ){ e.ptLow = ptAnchor; e.ptHigh = ptCur;    e.bAnchorHigh = false; }
				else                                      { e.ptLow = ptCur;    e.ptHigh = ptAnchor; e.bAnchorHigh = true;  }
			}
		}
		vEntries.push_back( e );
	}

	if( vEntries.size() < 2 ) return;

	std::sort( vEntries.begin(), vEntries.end(), []( const SMergeEntry& a, const SMergeEntry& b ){
		int nCmp = PointCompare( a.ptLow, b.ptLow );
		if( nCmp != 0 ) return nCmp < 0;
		return PointCompare( a.ptHigh, b.ptHigh ) < 0;
	} );

	std::vector<SMergeEntry> vMerged;
	vMerged.reserve( vEntries.size() );
	vMerged.push_back( vEntries[0] );
	for( size_t i = 1; i < vEntries.size(); ++i ){
		SMergeEntry& cur = vMerged.back();
		const SMergeEntry& next = vEntries[i];

		bool bEitherEmpty = !cur.bHasSelection || !next.bHasSelection;
		bool bShouldMerge = bEitherEmpty
			? PointCompare( next.ptLow, cur.ptHigh ) <= 0
			: PointCompare( next.ptLow, cur.ptHigh ) < 0;
		if( !bShouldMerge ){
			vMerged.push_back( next );
			continue;
		}

		bool bNextWins = next.nOrder > cur.nOrder;
		const SMergeEntry& winner = bNextWins ? next : cur;
		CLayoutPoint ptLow  = PointCompare( cur.ptLow,  next.ptLow  ) <= 0 ? cur.ptLow  : next.ptLow;
		CLayoutPoint ptHigh = PointCompare( cur.ptHigh, next.ptHigh ) >= 0 ? cur.ptHigh : next.ptHigh;

		SMergeEntry merged;
		merged.bPrimary = winner.bPrimary;
		merged.nExtraIdx = winner.nExtraIdx;
		merged.nOrder = ( cur.nOrder > next.nOrder ) ? cur.nOrder : next.nOrder;
		merged.ptLow = ptLow;
		merged.ptHigh = ptHigh;
		merged.bHasSelection = ( ptLow != ptHigh );
		if( !winner.bHasSelection ){
			// 無選択カーソルが吸収された側: 自分の位置がLow側でなければHigh側とみなす
			merged.bAnchorHigh = ( winner.ptLow != ptLow );
		}else{
			merged.bAnchorHigh = winner.bAnchorHigh;
		}
		cur = merged;
	}

	if( vMerged.size() == vEntries.size() ) return;	// 統合対象なし

	// プライマリの統合結果を確定して反映する(プライマリという実体は必ず1個生き残る)
	const SMergeEntry* pPrimaryMerged = nullptr;
	for( const auto& e : vMerged ){
		if( e.bPrimary ){ pPrimaryMerged = &e; break; }
	}

	CLayoutPoint ptPrimaryAnchor = pPrimaryMerged->bAnchorHigh ? pPrimaryMerged->ptHigh : pPrimaryMerged->ptLow;
	CLayoutPoint ptPrimaryCaret  = pPrimaryMerged->bAnchorHigh ? pPrimaryMerged->ptLow  : pPrimaryMerged->ptHigh;

	GetCaret().MoveCursor( ptPrimaryCaret, true );
	m_pCommanderView->GetSelectionInfo().m_sSelectBgn.Set( ptPrimaryAnchor );
	if( pPrimaryMerged->bHasSelection ){
		m_pCommanderView->GetSelectionInfo().m_sSelect = CLayoutRange( pPrimaryMerged->ptLow, pPrimaryMerged->ptHigh );
	}else{
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( false );
	}
	// 統合は「実際に桁が変わり得る」操作なので、希望桁もここで実桁に合わせてリセットする
	// (横移動・編集と同じ扱い)。統合に一切関わらなかった他のカーソルの希望桁は、下のループで
	// nRelColumnと同じ値にそろえて作り直される点も含め、この関数が呼ばれた=何かしら統合が
	// 起きた回であれば全カーソル分リセットする単純な仕様にしている(統合と無関係なカーソルが
	// たまたま短い行を上下移動で通過中だった場合に希望桁が一足早くリセットされる可能性はあるが、
	// 極めて稀な複合ケースであり、常に「安全側(クランプされた実桁に合わせる)」に倒れるだけで
	// 実害はないため許容している) 20260831
	GetCaret().m_nCaretPosX_Prev = ptPrimaryCaret.GetX2();

	// 生き残ったextra(統合されなかったもの・統合の勝者になったもの)を、新しいプライマリ
	// 位置基準の相対値として作り直す。統合で消えたextra(敗者)はここで自然に脱落する。
	std::vector<CEditView::SExtraCursor> vNewExtras;
	vNewExtras.reserve( vMerged.size() );
	for( const auto& e : vMerged ){
		if( e.bPrimary ) continue;
		CLayoutPoint ptAnchor = e.bAnchorHigh ? e.ptHigh : e.ptLow;
		CLayoutPoint ptCaret  = e.bAnchorHigh ? e.ptLow  : e.ptHigh;

		CEditView::SExtraCursor ne;
		ne.nRelLine = ToInt(ptCaret.GetY2()) - ToInt(ptPrimaryCaret.GetY2());
		ne.nRelColumn = ToInt(ptCaret.GetX2()) - ToInt(ptPrimaryCaret.GetX2());
		ne.bHasSelection = e.bHasSelection;
		if( e.bHasSelection ){
			ne.nAnchorRelLine = ToInt(ptAnchor.GetY2()) - ToInt(ptPrimaryAnchor.GetY2());
			ne.nAnchorRelColumn = ToInt(ptAnchor.GetX2()) - ToInt(ptPrimaryAnchor.GetX2());
		}
		ne.nDesiredRelColumn = ne.nRelColumn;	// 統合は実桁が変わり得る操作なので希望桁もリセットする
		vNewExtras.push_back( ne );
	}
	m_pCommanderView->m_vExtraCursors = std::move( vNewExtras );
}

//! 移動系コマンド(F_UP/DOWN/LEFT/RIGHT/GOLINETOP/GOLINEEND等、および対応する
//! _SEL系コマンド)をマルチカーソル対応にするための分岐	20260830, 選択対応 20260831
//!
//! 追加カーソルの位置・選択範囲は「プライマリの現在位置/選択範囲 + nRelLine/nRelColumn
//! (作成時に固定)」としてその都度算出する方式(SExtraCursor、ResolveExtraCursor参照)の
//! ため、選択を伴わない移動系コマンドではプライマリを通常通り動かすだけでよい(各カーソルを
//! 個別に動かす必要が無い)。バッファ端・短い行でのクランプも含め、プライマリは既存の単一
//! カーソルと全く同じ挙動のまま。追加カーソルは何もしなくても自動的に追従し、算出結果が
//! ドキュメント範囲外になる間だけ自動的に非表示・編集対象外になり、プライマリが戻れば
//! 自動的に元の相対位置へ復活する(nRelLine/nRelColumnを一切変更しないため、境界を挟んでも
//! 相対位置が厳密に保たれ、他カーソルと衝突することもない)。
//!
//! 選択(bSelect==true)を伴う場合は事情が異なる: 各カーソルの選択を「プライマリの結果を
//! 平行移動」しただけで済ませると、行の長さがカーソルごとに違う場合(短い行の末尾で選択が
//! 止まってしまい、次の行へ回り込まない)に正しく折り返せない。ユーザー要望により、選択操作は
//! ApplyToAllCursors経由で各カーソルの実位置において実際にコマンドを再実行する方式に変更した
//! (編集系コマンドと全く同じ扱い。実際、typing/delete用に作った仕組みがそのまま転用できた)。
//! 選択を伴わない移動はこれまで通り「動かさず自動追従」のまま(短い行ではクランプして止まる、
//! という以前からの仕様を意図的に維持) 20260831
//!
//! ただし「選択が既にある状態でbSelect==falseの移動(Shiftなし矢印)」だけは例外的に
//! ApplyToAllCursorsを使う。これは実際には「動く」のではなく「選択を選択端へ収束させる」
//! (Command_CANCEL_MODE)操作であり、各extra自身の選択(アンカー〜現在位置)を正しい側の
//! 端へ収束させるには、そのextra自身の実位置・実選択状態で実際にCommand_LEFT/RIGHT(false,...)
//! を再実行する必要がある。プライマリの結果を平行移動するだけでは、extraの選択がプライマリと
//! 異なる行に跨って回り込んでいた場合(短い行で折り返した後)にextraの位置・選択状態が
//! 更新されず、折り返したままの位置に取り残されてしまう不具合があった 20260831 二回目 */
void CViewCommander::DispatchMoveMultiCursor( bool bSelect, const std::function<void()>& fnMoveOnce )
{
	bool bAnyHasSelection = m_pCommanderView->GetSelectionInfo().IsTextSelected();
	if( !bAnyHasSelection ){
		for( const auto& extra : m_pCommanderView->m_vExtraCursors ){
			if( extra.bHasSelection ){ bAnyHasSelection = true; break; }
		}
	}
	if( ( bSelect || bAnyHasSelection ) && !m_pCommanderView->m_vExtraCursors.empty() ){
		ApplyToAllCursors( fnMoveOnce );
		return;
	}
	fnMoveOnce();
	if( !m_pCommanderView->m_vExtraCursors.empty() ){
		m_pCommanderView->Redraw();
	}
}
#endif // NKMM_



/* 	上書き用の一文字削除	2009.04.11 ryoji */
void CViewCommander::DelCharForOverwrite( const wchar_t* pszInput, int nLen )
{
	bool bEol = false;
	BOOL bDelete = TRUE;
	const CLayout* pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( GetCaret().GetCaretLayoutPos().GetY2() );
	int nDelLen = 0;
	CKetaXInt nKetaDiff = CKetaXInt(0);
	CKetaXInt nKetaAfterIns = CKetaXInt(0);
	if( NULL != pcLayout ){
		/* 指定された桁に対応する行のデータ内の位置を調べる */
		CLogicInt nIdxTo = m_pCommanderView->LineColumnToIndex( pcLayout, GetCaret().GetCaretLayoutPos().GetX2() );
		if( nIdxTo >= pcLayout->GetLengthWithoutEOL() ){
			bEol = true;	// 現在位置は改行または折り返し以後
			if( pcLayout->GetLayoutEol() != EOL_NONE ){
				if( GetDllShareData().m_Common.m_sEdit.m_bNotOverWriteCRLF ){	/* 改行は上書きしない */
					/* 現在位置が改行ならば削除しない */
					bDelete = FALSE;
				}
			}
		}else{
			// 文字幅に合わせてスペースを詰める
			if( GetDllShareData().m_Common.m_sEdit.m_bOverWriteFixMode ){
				const CStringRef line = pcLayout->GetDocLineRef()->GetStringRefWithEOL();
				CLogicInt nPos = GetCaret().GetCaretLogicPos().GetX();
				if( line.At(nPos) != WCODE::TAB ){
					CKetaXInt nKetaBefore = CNativeW::GetKetaOfChar(line, nPos);
					CKetaXInt nKetaAfter = CNativeW::GetKetaOfChar(pszInput, nLen, 0);
					nKetaDiff = nKetaBefore - nKetaAfter;
					nPos += CNativeW::GetSizeOfChar(line.GetPtr(), line.GetLength(), nPos);
					nDelLen = 1;
					if( nKetaDiff < 0 && nPos < line.GetLength() ){
						wchar_t c = line.At(nPos);
						if( c != WCODE::TAB && !WCODE::IsLineDelimiter(c,
								GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) ){
							nDelLen = 2;
							CKetaXInt nKetaBefore2 = CNativeW::GetKetaOfChar(line, nPos);
							nKetaAfterIns = nKetaBefore + nKetaBefore2 - nKetaAfter;
						}
					}
				}
			}
		}
	}
	if( bDelete ){
		/* 上書きモードなので、現在位置の文字を１文字消去 */
		CLayoutPoint posBefore;
		if( bEol ){
			Command_DELETE();	//行数減では再描画が必要＆行末以後の削除を処理統一
			posBefore = GetCaret().GetCaretLayoutPos();
		}else{
			// 1文字削除
			m_pCommanderView->DeleteData( false );
			posBefore = GetCaret().GetCaretLayoutPos();
			for(int i = 1; i < nDelLen; i++){
				m_pCommanderView->DeleteData( false );
			}
		}
		CNativeW tmp;
		for(CKetaXInt i = CKetaXInt(0); i < nKetaDiff; i++){
			tmp.AppendString(L" ");
		}
		for(CKetaXInt i = CKetaXInt(0); i < nKetaAfterIns; i++){
			tmp.AppendString(L" ");
		}
		if( 0 < tmp.GetStringLength() ){
			Command_INSTEXT( false, tmp.GetStringPtr(), tmp.GetStringLength(), false, false);
			GetCaret().MoveCursor(posBefore, false);
		}
	}
}
