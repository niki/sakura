/*!	@file
@brief CViewCommanderクラスのコマンド(検索系 アウトライン解析)関数群

	2012/12/17	CViewCommander.cppから分離
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000-2001, jepro, genta
	Copyright (C) 2001, hor
	Copyright (C) 2002, YAZAKI
	Copyright (C) 2003, zenryaku
	Copyright (C) 2006, aroka
	Copyright (C) 2007, genta, kobake
	Copyright (C) 2009, genta
	Copyright (C) 2011, syat

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "CViewCommander.h"
#include "CViewCommander_inline.h"

#include "outline/CFuncInfoArr.h"
#include "plugin/CJackManager.h"
#include "plugin/COutlineIfObj.h"
#include "sakura_rc.h"
#ifdef NKMM_CODE_FOLDING
#include "docplus/CFoldManager.h"
#endif // NKMM_

#ifdef NKMM_FIX_EDITVIEW_SCRBAR
namespace {
	/*! 20260911 Command_OUTLINE_FOLD_TOGGLE用のRAIIガード。

		CLayoutMgr::ToggleFoldAll()は_DoLayout()により文書全体のCLayoutを一旦
		すべて破棄して作り直す(総レイアウト行数が例えば79→9のように激減しうる)。
		この最中にスクロールバーマーカーのバックグラウンド構築スレッド
		(ScrBarMarker::BuildWorkCallback、折り返しあり文書では
		CLayoutMgr::LogicToLayout()経由でCLayout連結リストや共有ヒントキャッシュを
		参照する)が並行して走っていると、メインスレッドが解放中/解放済みの
		CLayoutを背景スレッドが参照する競合状態になり、ヒープ破壊経由で
		COMCTL32等の無関係なモジュール内で不可解に(かつ再現性高く同一アドレスで)
		クラッシュする。CViewCommander_Search.cppのCSuppressSrchKeyMarkForReplaceAll
		が保護しているCommand_REPLACE_ALLと同種の、既知の競合パターン。
		コンストラクタで全ビューの描画を一時停止し(SetDrawSwitch(false))背景
		スレッドを同期的に止め(SB_Marker_Clear)、デストラクタで必ず元へ戻す。
		@date 2026.09.11 Yu-zuki. 新規作成(COMCTL32内クラッシュの根本原因対策)
	*/
	class CSuppressScrBarMarkerForFoldToggle {
	public:
		explicit CSuppressScrBarMarkerForFoldToggle(CEditWnd* pEditWnd)
			: m_pEditWnd(pEditWnd)
		{
			for( int i = 0; i < m_pEditWnd->GetAllViewCount(); i++ ){
				CEditView& view = m_pEditWnd->GetView(i);
				if( view.GetHwnd() ){
					m_vIndices.push_back(i);
					m_vOldDrawSwitch.push_back( view.SetDrawSwitch(false) );
					view.SB_Marker_Clear(820);
				}
			}
		}
		~CSuppressScrBarMarkerForFoldToggle()
		{
			for( size_t k = 0; k < m_vIndices.size(); k++ ){
				m_pEditWnd->GetView(m_vIndices[k]).SetDrawSwitch( m_vOldDrawSwitch[k] );
			}
		}
	private:
		CEditWnd*			m_pEditWnd;
		std::vector<int>	m_vIndices;
		std::vector<bool>	m_vOldDrawSwitch;
	};
}
#endif // NKMM_


/*!	アウトライン解析

	@date 2002/03/13 YAZAKI nOutlineTypeとnListTypeを統合。
	@date 2006/02/01 aroka トグル用のフラグに変更
*/
BOOL CViewCommander::Command_FUNCLIST(
	int nAction,
	EOutlineType nOutlineType = OUTLINE_DEFAULT
)
{
	static bool bIsProcessing = false;	//アウトライン解析処理中フラグ

	//アウトラインプラグイン内でのEditor.Outline呼び出しによる再入を禁止する
	if( bIsProcessing )return FALSE;

	bIsProcessing = true;

	// 自プロセスが前面にいるかどうか調べる
	DWORD dwPid1, dwPid2;
	dwPid1 = ::GetCurrentProcessId();
	::GetWindowThreadProcessId( ::GetForegroundWindow(), &dwPid2 );
	bool bForeground = (dwPid1 == dwPid2);

//	if( bCheckOnly ){
//		return TRUE;
//	}

	static CFuncInfoArr	cFuncInfoArr;
	std::tstring sTitleOverride;				//プラグインによるダイアログタイトル上書き

	//	2001.12.03 hor & 2002.3.13 YAZAKI
	if( nOutlineType == OUTLINE_DEFAULT ){
		/* タイプ別に設定されたアウトライン解析方法 */
		nOutlineType = m_pCommanderView->m_pTypeData->m_eDefaultOutline;
	}
#ifdef NKMM_FIX_OUTLINE
	// 20260811: OUTLINE_C_CPP(「C/C++」自動判別)のまま以下のSHOW_NORMAL/
	// SHOW_TOGGLE判定に入ると、ダイアログ側が保持する解決済みの型
	// (MakeFuncList_C内でファイル拡張子から求めたOUTLINE_C/OUTLINE_CPP)との
	// 比較(CheckListType)が常に不一致になり、「型が違うので再解析」の経路に
	// 落ちてしまう。ここで先に解決しておく(この関数はOUTLINE_C_CPP以外は
	// 素通しするため、他の種別には影響しない)。
	nOutlineType = CDocOutline::ResolveOutlineType_C_CPP( nOutlineType, GetDocument()->m_cDocFile.GetFilePath() );
#endif // NKMM_

	if( NULL != GetEditWindow()->m_cDlgFuncList.GetHwnd() && nAction != SHOW_RELOAD ){
		switch(nAction ){
		case SHOW_NORMAL: // アクティブにする
			//	開いているものと種別が同じならActiveにするだけ．異なれば再解析
			GetEditWindow()->m_cDlgFuncList.SyncColor();
			if( GetEditWindow()->m_cDlgFuncList.CheckListType( nOutlineType )){
				if( bForeground ){
					::SetFocus( GetEditWindow()->m_cDlgFuncList.GetHwnd() );
				}
				bIsProcessing = false;
				return TRUE;
			}
			break;
		case SHOW_TOGGLE: // 閉じる
			//	開いているものと種別が同じなら閉じる．異なれば再解析
			if( GetEditWindow()->m_cDlgFuncList.CheckListType( nOutlineType )){
				if( GetEditWindow()->m_cDlgFuncList.IsDocking() )
					::DestroyWindow( GetEditWindow()->m_cDlgFuncList.GetHwnd() );
				else
					::SendMessageAny( GetEditWindow()->m_cDlgFuncList.GetHwnd(), WM_CLOSE, 0, 0 );
				bIsProcessing = false;
				return TRUE;
			}
			break;
		default:
			break;
		}
	}

	/* 解析結果データを空にする */
	cFuncInfoArr.Empty();
	int		nListType = nOutlineType;			//2011.06.25 syat

	switch( nOutlineType ){
	// 2015.11.14 「C」「C++」「C/C++」から選べるように
	case OUTLINE_C:			// C/C++ は MakeFuncList_C
	case OUTLINE_C_CPP:
	case OUTLINE_CPP:
		{
			GetDocument()->m_cDocOutline.MakeFuncList_C( &cFuncInfoArr,
				nOutlineType, GetDocument()->m_cDocFile.GetFilePath() );
			nListType = nOutlineType; // 変更された可能性あり
			break;
		}
	case OUTLINE_PLSQL:		GetDocument()->m_cDocOutline.MakeFuncList_PLSQL( &cFuncInfoArr );break;
	case OUTLINE_JAVA:		GetDocument()->m_cDocOutline.MakeFuncList_Java( &cFuncInfoArr );break;
	case OUTLINE_COBOL:		GetDocument()->m_cDocOutline.MakeTopicList_cobol( &cFuncInfoArr );break;
	case OUTLINE_ASM:		GetDocument()->m_cDocOutline.MakeTopicList_asm( &cFuncInfoArr );break;
	case OUTLINE_PERL:		GetDocument()->m_cDocOutline.MakeFuncList_Perl( &cFuncInfoArr );break;	//	Sep. 8, 2000 genta
	case OUTLINE_VB:		GetDocument()->m_cDocOutline.MakeFuncList_VisualBasic( &cFuncInfoArr );break;	//	June 23, 2001 N.Nakatani
	case OUTLINE_WZTXT:		GetDocument()->m_cDocOutline.MakeTopicList_wztxt(&cFuncInfoArr);break;		// 2003.05.20 zenryaku 階層付テキスト アウトライン解析
	case OUTLINE_HTML:		GetDocument()->m_cDocOutline.MakeTopicList_html(&cFuncInfoArr, false);break;		// 2003.05.20 zenryaku HTML アウトライン解析
	case OUTLINE_TEX:		GetDocument()->m_cDocOutline.MakeTopicList_tex(&cFuncInfoArr);break;		// 2003.07.20 naoh TeX アウトライン解析
	case OUTLINE_BOOKMARK:	GetDocument()->m_cDocOutline.MakeFuncList_BookMark( &cFuncInfoArr );break;	//	2001.12.03 hor
	case OUTLINE_FILE:		GetDocument()->m_cDocOutline.MakeFuncList_RuleFile( &cFuncInfoArr, sTitleOverride );break;	//	2002.04.01 YAZAKI アウトライン解析にルールファイルを導入
//	case OUTLINE_UNKNOWN:	//Jul. 08, 2001 JEPRO 使わないように変更
	case OUTLINE_PYTHON:	GetDocument()->m_cDocOutline.MakeFuncList_python(&cFuncInfoArr);break;		// 2007.02.08 genta
	case OUTLINE_ERLANG:	GetDocument()->m_cDocOutline.MakeFuncList_Erlang(&cFuncInfoArr);break;		// 2009.08.10 genta
	case OUTLINE_XML:		GetDocument()->m_cDocOutline.MakeTopicList_html(&cFuncInfoArr, true);break;		// 2014.12.25 Moca
	case OUTLINE_FILETREE:	/* 特に何もしない*/ ;break;	// 2013.12.08 Moca
	case OUTLINE_TEXT:
		//	fall though
		//	ここには何も入れてはいけない 2007.02.28 genta 注意書き
	default:
		//プラグインから検索する
		{
			CPlug::Array plugs;
			CJackManager::getInstance()->GetUsablePlug( PP_OUTLINE, nOutlineType, &plugs );

			if( plugs.size() > 0 ){
				assert_warning( 1 == plugs.size() );
				//インタフェースオブジェクト準備
				CWSHIfObj::List params;
				COutlineIfObj* objOutline = new COutlineIfObj( cFuncInfoArr );
				objOutline->AddRef();
				params.push_back( objOutline );
				//プラグイン呼び出し
				( *plugs.begin() )->Invoke( m_pCommanderView, params );

				nListType = objOutline->m_nListType;			//ダイアログの表示方法をを上書き
				sTitleOverride = objOutline->m_sOutlineTitle;	//ダイアログタイトルを上書き

				objOutline->Release();
				break;
			}
		}

		//それ以外
		GetDocument()->m_cDocOutline.MakeTopicList_txt( &cFuncInfoArr );
		break;
	}

	/* 解析対象ファイル名 */
	auto_strcpy_s( cFuncInfoArr.m_szFilePath, _countof2(cFuncInfoArr.m_szFilePath), GetDocument()->m_cDocFile.GetFilePath() );

	/* アウトライン ダイアログの表示 */
	CLayoutPoint poCaret = GetCaret().GetCaretLayoutPos();
	if( NULL == GetEditWindow()->m_cDlgFuncList.GetHwnd() ){
		GetEditWindow()->m_cDlgFuncList.DoModeless(
			G_AppInstance(),
			m_pCommanderView->GetHwnd(),
			(LPARAM)m_pCommanderView,
			&cFuncInfoArr,
			poCaret.GetY2() + CLayoutInt(1),
			poCaret.GetX2() + CLayoutInt(1),
			nOutlineType,
			nListType,
			m_pCommanderView->m_pTypeData->m_bLineNumIsCRLF	/* 行番号の表示 false=折り返し単位／true=改行単位 */
		);
	}else{
		/* アクティブにする */
		GetEditWindow()->m_cDlgFuncList.Redraw( nOutlineType, nListType, &cFuncInfoArr, poCaret.GetY2() + 1, poCaret.GetX2() + 1 );
		if( bForeground ){
			::SetFocus( GetEditWindow()->m_cDlgFuncList.GetHwnd() );
		}
	}

	// ダイアログタイトルを上書き
	if( ! sTitleOverride.empty() ){
		GetEditWindow()->m_cDlgFuncList.SetWindowText( sTitleOverride.c_str() );
	}

	bIsProcessing = false;
	return TRUE;
}

#ifdef NKMM_CODE_FOLDING
/*! 文書全体の折りたたみをトグルする(関数/構造体単位でアウトライン表示⇔全展開)

	@note 折りたたみ範囲の解析(UpdateFoldRanges)は文書を開いてから最初の1回だけ行う。
		以降の編集でアウトライン構造が変化しても自動では再解析しない(既知の制限)。
	@date 2026.09.11 Yu-zuki. 新規作成
	@date 2026.09.11 Yu-zuki. カーソル行単位のトグルから文書全体のトグルへ変更(ユーザー指摘)
*/
void CViewCommander::Command_OUTLINE_FOLD_TOGGLE( void )
{
	if( !GetDocument()->m_bFoldRangesReady ){
		GetDocument()->m_cDocOutline.UpdateFoldRanges();
		GetDocument()->m_bFoldRangesReady = true;
	}

	// キャレットの論理位置は、_DoLayout()を伴うToggleFoldAll()を呼ぶ「前」に
	// 取得しておくこと。後で取得すると、ToggleFoldAll()内の全体再レイアウトで
	// 総レイアウト行数が激減した直後のレイアウト位置ベースで再計算されてしまい、
	// 範囲外に丸められて常にEOF行になってしまう(実際に発生していたバグ) 20260911
	CLogicPoint ptCaretLogic = GetCaret().GetCaretLogicPos();

	bool bToggled;
	{
#ifdef NKMM_FIX_EDITVIEW_SCRBAR
		// ToggleFoldAll()内の_DoLayout()実行中は、スクロールバーマーカーの
		// バックグラウンド構築スレッドを止めておく(理由はクラス定義のコメント参照)。
		// このブロックを抜けると同時に(早期returnでも)必ず元の状態へ戻る。
		CSuppressScrBarMarkerForFoldToggle cSuppressScrBarMarker( GetEditWindow() );
#endif // NKMM_
		bToggled = GetDocument()->m_cLayoutMgr.ToggleFoldAll();
	}
	if( !bToggled ){
		return;	// 折りたたみ可能な範囲が無い
	}

	if( GetDocument()->m_bOutlineFolded ){
		// 今回アウトライン表示に入った → 元のキャレット位置を記憶しておく(Escで戻る用)
		GetDocument()->m_ptPreFoldCaretLogic = ptCaretLogic;
	}

	GetEditWindow()->ClearViewCaretPosInfo();

	// キャレットが折りたたみで非表示になった行にいる場合、表示されている行まで移動する。
	// 直近の手前の行(=キャレットが属していたスコープのヘッダ行)を優先し、
	// 文書先頭付近など手前に表示行が無い場合のみ直後の表示行まで進める 20260911
	{
		CFoldManager cFoldMgr;
		CDocLine* pLine = GetDocument()->m_cDocLineMgr.GetLine( ptCaretLogic.GetY2() );
		if( NULL != pLine && cFoldMgr.GetLineFoldHidden( pLine ) ){
			CDocLine* pPrev = pLine;
			CLogicInt nPrevLine = ptCaretLogic.GetY2();
			while( NULL != pPrev && cFoldMgr.GetLineFoldHidden( pPrev ) ){
				pPrev = pPrev->GetPrevLine();
				nPrevLine--;
			}
			if( NULL != pPrev ){
				ptCaretLogic = CLogicPoint( CLogicInt(0), nPrevLine );
			}else{
				CDocLine* pNext = GetDocument()->m_cDocLineMgr.GetDocLineTop();
				CLogicInt nNextLine = CLogicInt(0);
				while( NULL != pNext && cFoldMgr.GetLineFoldHidden( pNext ) ){
					pNext = pNext->GetNextLine();
					nNextLine++;
				}
				if( NULL != pNext ){
					ptCaretLogic = CLogicPoint( CLogicInt(0), nNextLine );
				}
			}
		}
	}

#ifdef NKMM_FIX_CENTERING_CURSOR_JUMP
	// ジャンプ先の行が画面中央に来るようにする(ユーザー指摘) 20260911
	GetDllShareData().m_sFlags.m_nCenteringCursor++;
#endif // NKMM_

	CLayoutPoint ptCaretNew;
	GetDocument()->m_cLayoutMgr.LogicToLayout( ptCaretLogic, &ptCaretNew );
	GetCaret().MoveCursor( ptCaretNew, true );

	for( int i = 0; i < GetEditWindow()->GetAllViewCount(); i++ ){
		if( GetEditWindow()->GetView(i).GetHwnd() ){
			::InvalidateRect( GetEditWindow()->GetView(i).GetHwnd(), NULL, TRUE );
			GetEditWindow()->GetView(i).AdjustScrollBars();
		}
	}
	m_pCommanderView->RedrawAll();
}

/*! アウトライン表示を終了し、折りたたみに入る直前のキャレット位置へ戻る(Esc用)

	@note Command_OUTLINE_FOLD_TOGGLE(Enter/Tabによる「ジャンプ」)との違いは、展開後の
		キャレット位置。ジャンプはアウトライン表示中に選択していたヘッダ行へ
		留まるが、こちらは折りたたみに入る前にいた位置へ戻す。
	@date 2026.09.11 Yu-zuki. 新規作成
*/
void CViewCommander::Command_FOLD_CANCEL( void )
{
	if( !GetDocument()->m_bOutlineFolded ){
		return;	// アウトライン表示中でなければ何もしない
	}

	CLogicPoint ptRestoreLogic = GetDocument()->m_ptPreFoldCaretLogic;

	bool bToggled;
	{
#ifdef NKMM_FIX_EDITVIEW_SCRBAR
		CSuppressScrBarMarkerForFoldToggle cSuppressScrBarMarker( GetEditWindow() );
#endif // NKMM_
		bToggled = GetDocument()->m_cLayoutMgr.ToggleFoldAll();
	}
	if( !bToggled ){
		return;
	}

	GetEditWindow()->ClearViewCaretPosInfo();

#ifdef NKMM_FIX_CENTERING_CURSOR_JUMP
	GetDllShareData().m_sFlags.m_nCenteringCursor++;
#endif // NKMM_

	CLayoutPoint ptCaretNew;
	GetDocument()->m_cLayoutMgr.LogicToLayout( ptRestoreLogic, &ptCaretNew );
	GetCaret().MoveCursor( ptCaretNew, true );

	for( int i = 0; i < GetEditWindow()->GetAllViewCount(); i++ ){
		if( GetEditWindow()->GetView(i).GetHwnd() ){
			::InvalidateRect( GetEditWindow()->GetView(i).GetHwnd(), NULL, TRUE );
			GetEditWindow()->GetView(i).AdjustScrollBars();
		}
	}
	m_pCommanderView->RedrawAll();
}
#endif // NKMM_
