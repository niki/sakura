/*!	@file
	@brief 共通設定ダイアログボックス、「全般」ページ

	@author Uchi
	@date 2010/5/9 CPropCommon.cより分離
*/
/*
	Copyright (C) 2010, Uchi

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "prop/CPropCommon.h"
#include "CPropertyManager.h"
#include "recent/CMRUFile.h"
#include "recent/CMRUFolder.h"
#include "util/shell.h"
#include "sakura_rc.h"
#include "sakura.hh"

#ifdef NKMM_FIX_FONT_QUALITY
// 20260810 描画品質(LOGFONT.lfQuality)
//  - ClearType(5,6)は背景色に依存した描画のため、二重バッファ構成との相性が
//    悪く色にじみが強調されうる。選択肢からは外し、0/1/2/4のみを提示する
struct SFontQualityItem { BYTE nValue; const wchar_t* pszName; };
static const SFontQualityItem FontQualityArr[] = {
	{ DRAFT_QUALITY,          L"標準(既定)" },
	{ DEFAULT_QUALITY,        L"自動(Windowsにお任せ)" },
	{ PROOF_QUALITY,          L"高品質" },
	{ ANTIALIASED_QUALITY,    L"滑らかに表示(アンチエイリアス)" },
};
#endif // NKMM_

//@@@ 2001.02.04 Start by MIK: Popup Help
TYPE_NAME_ID<int> SpecialScrollModeArr[] = {
	{ 0,						STR_SCROLL_WITH_NO_KEY },		//_T("組み合わせなし") },
	{ MOUSEFUNCTION_CENTER,		STR_SCROLL_WITH_MID_BTN },		//_T("マウス中ボタン") },
	{ MOUSEFUNCTION_LEFTSIDE,	STR_SCROLL_WITH_SIDE_1_BTN },	//_T("マウスサイドボタン1") },
	{ MOUSEFUNCTION_RIGHTSIDE,	STR_SCROLL_WITH_SIDE_2_BTN },	//_T("マウスサイドボタン2") },
	{ VK_CONTROL,				STR_SCROLL_WITH_CTRL_KEY },	//_T("CONTROLキー") },
	{ VK_SHIFT,					STR_SCROLL_WITH_SHIFT_KEY },	//_T("SHIFTキー") },
};

static const DWORD p_helpids[] = {	//10900
	IDC_BUTTON_CLEAR_MRU_FILE,		HIDC_BUTTON_CLEAR_MRU_FILE,			//履歴をクリア（ファイル）
	IDC_BUTTON_CLEAR_MRU_FOLDER,	HIDC_BUTTON_CLEAR_MRU_FOLDER,		//履歴をクリア（フォルダ）
	IDC_CHECK_FREECARET,			HIDC_CHECK_FREECARET,				//フリーカーソル
//DEL	IDC_CHECK_INDENT,				HIDC_CHECK_INDENT,					//自動インデント ：タイプ別へ移動
//DEL	IDC_CHECK_INDENT_WSPACE,		HIDC_CHECK_INDENT_WSPACE,			//全角空白もインデント ：タイプ別へ移動
	IDC_CHECK_USETRAYICON,			HIDC_CHECK_USETRAYICON,				//タスクトレイを使う
	IDC_CHECK_STAYTASKTRAY,			HIDC_CHECK_STAYTASKTRAY,			//タスクトレイに常駐
	IDC_CHECK_REPEATEDSCROLLSMOOTH,	HIDC_CHECK_REPEATEDSCROLLSMOOTH,	//少し滑らかにする
	IDC_CHECK_FILE_OPEN2OPEN,		HIDC_CHECK_FILE_OPEN2OPEN,			//既に開いているとき新しいウィンドウで開く
	IDC_CHECK_CLOSEALLCONFIRM,		HIDC_CHECK_CLOSEALLCONFIRM,			//[すべて閉じる]で他に編集用のウィンドウがあれば確認する	// 2006.12.25 ryoji
	IDC_CHECK_EXITCONFIRM,			HIDC_CHECK_EXITCONFIRM,				//終了の確認
	IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_WORD, HIDC_CHECK_STOPS_WORD, //単語単位で移動するときに単語の両端に止まる
	IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_PARAGRAPH, HIDC_CHECK_STOPS_PARAGRAPH, // 段落単位で移動するときに段落の両端に止まる
	IDC_CHECK_NOMOVE_ACTIVATE_BY_MOUSE, HIDC_CHECK_NOMOVE_ACTIVATE_BY_MOUSE,	// マウスクリックでアクティブになったときはカーソルをクリック位置に移動しない 2007.10.08 genta
	IDC_HOTKEY_TRAYMENU,			HIDC_HOTKEY_TRAYMENU,				//左クリックメニューのショートカットキー
	IDC_EDIT_REPEATEDSCROLLLINENUM,	HIDC_EDIT_REPEATEDSCROLLLINENUM,	//スクロール行数
	IDC_EDIT_MAX_MRU_FILE,			HIDC_EDIT_MAX_MRU_FILE,				//ファイル履歴の最大数
	IDC_EDIT_MAX_MRU_FOLDER,		HIDC_EDIT_MAX_MRU_FOLDER,			//フォルダ履歴の最大数
#if defined(NKMM_FIX_PROFILES) && NKMM_DELETE_HISTORY_NOT_EXIST_AT_STARTUP
	IDC_CHECK_DELETE_MISSING_HISTORY, HIDC_CHECK_DELETE_MISSING_HISTORY,	//起動時に存在しない履歴を確認して削除する
#endif // NKMM_
	IDC_RADIO_CARETTYPE0,			HIDC_RADIO_CARETTYPE0,				//カーソル形状（Windows風）
	IDC_RADIO_CARETTYPE1,			HIDC_RADIO_CARETTYPE1,				//カーソル形状（MS-DOS風）
	IDC_SPIN_REPEATEDSCROLLLINENUM,	HIDC_EDIT_REPEATEDSCROLLLINENUM,
	IDC_SPIN_MAX_MRU_FILE,			HIDC_EDIT_MAX_MRU_FILE,
	IDC_SPIN_MAX_MRU_FOLDER,		HIDC_EDIT_MAX_MRU_FOLDER,
	IDC_CHECK_MEMDC,				HIDC_CHECK_MEMDC,					//画面キャッシュを使う
#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE
	IDC_CHECK_GLYPHATLASCACHE,		HIDC_CHECK_GLYPHATLASCACHE,			//グリフキャッシュを使う
#endif // NKMM_
#ifdef NKMM_FIX_FONT_QUALITY
	IDC_COMBO_FONTQUALITY,			HIDC_COMBO_FONTQUALITY,				//描画品質
#endif // NKMM_
#ifdef NKMM_FIX_COLOR_FONT
	IDC_CHECK_USEEMOJIFONT,			HIDC_CHECK_USEEMOJIFONT,			//絵文字フォントを固定指定する
	IDC_BUTTON_EMOJIFONT,			HIDC_BUTTON_EMOJIFONT,				//絵文字フォントの選択
	IDC_CHECK_EMOJILIGATURE,		HIDC_CHECK_EMOJILIGATURE,			//絵文字の合字を有効にする
#endif // NKMM_
	IDC_COMBO_WHEEL_PAGESCROLL,		HIDC_COMBO_WHEEL_PAGESCROLL,		// 組み合わせてホイール操作した時ページスクロールする		// 2009.01.17 nasukoji
	IDC_COMBO_WHEEL_HSCROLL,		HIDC_COMBO_WHEEL_HSCROLL,			// 組み合わせてホイール操作した時横スクロールする			// 2009.01.17 nasukoji
//	IDC_STATIC,						-1,
	0, 0
};
//@@@ 2001.02.04 End

/*!
	@param hwndDlg ダイアログボックスのWindow Handle
	@param uMsg メッセージ
	@param wParam パラメータ1
	@param lParam パラメータ2
*/
INT_PTR CALLBACK CPropGeneral::DlgProc_page(
	HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DlgProc( reinterpret_cast<pDispatchPage>(&CPropGeneral::DispatchEvent), hwndDlg, uMsg, wParam, lParam );
}


/* General メッセージ処理 */
INT_PTR CPropGeneral::DispatchEvent(
	HWND	hwndDlg,	// handle to dialog box
	UINT	uMsg,		// message
	WPARAM	wParam,		// first message parameter
	LPARAM	lParam 		// second message parameter
)
{
	WORD		wNotifyCode;
	WORD		wID;
//	HWND		hwndCtl;
	NMHDR*		pNMHDR;
	NM_UPDOWN*	pMNUD;
	int			idCtrl;
	int			nVal;
//	LPDRAWITEMSTRUCT pDis;

	switch( uMsg ){

	case WM_INITDIALOG:
		/* ダイアログデータの設定 General */
		SetData( hwndDlg );
		// Modified by KEITA for WIN64 2003.9.6
		::SetWindowLongPtr( hwndDlg, DWLP_USER, lParam );

		/* ユーザーがエディット コントロールに入力できるテキストの長さを制限する */

		return TRUE;
	case WM_COMMAND:
		wNotifyCode	= HIWORD(wParam);	/* 通知コード */
		wID			= LOWORD(wParam);	/* 項目ID､ コントロールID､ またはアクセラレータID */
//		hwndCtl		= (HWND) lParam;	/* コントロールのハンドル */
		switch( wNotifyCode ){
		/* ボタン／チェックボックスがクリックされた */
		case BN_CLICKED:
			switch( wID ){

			case IDC_CHECK_USETRAYICON:	/* タスクトレイを使う */
			// From Here 2001.12.03 hor
			//		操作しにくいって評判だったのでタスクトレイ関係のEnable制御をやめました
			//@@@ YAZAKI 2001.12.31 IDC_CHECKSTAYTASKTRAYのアクティブ、非アクティブのみ制御。
				if( ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_USETRAYICON ) ){
					::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), TRUE );
				}else{
					::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), FALSE );
				}
			// To Here 2001.12.03 hor
				return TRUE;

			case IDC_CHECK_STAYTASKTRAY:	/* タスクトレイに常駐 */
				return TRUE;

			case IDC_BUTTON_CLEAR_MRU_FILE:
				/* ファイルの履歴をクリア */
				if( IDCANCEL == ::MYMESSAGEBOX( hwndDlg, MB_OKCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
					LS(STR_PROPCOMGEN_FILE1) ) ){
					return TRUE;
				}
//@@@ 2001.12.26 YAZAKI MRUリストは、CMRUに依頼する
//				m_pShareData->m_sHistory.m_nMRUArrNum = 0;
				{
					CMRUFile cMRU;
					cMRU.ClearAll();
				}
				InfoMessage( hwndDlg, LS(STR_PROPCOMGEN_FILE2) );
				return TRUE;
			case IDC_BUTTON_CLEAR_MRU_FOLDER:
				/* フォルダの履歴をクリア */
				if( IDCANCEL == ::MYMESSAGEBOX( hwndDlg, MB_OKCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
					LS(STR_PROPCOMGEN_DIR1) ) ){
					return TRUE;
				}
//@@@ 2001.12.26 YAZAKI OPENFOLDERリストは、CMRUFolderにすべて依頼する
//				m_pShareData->m_sHistory.m_nOPENFOLDERArrNum = 0;
				{
					CMRUFolder cMRUFolder;	//	MRUリストの初期化。ラベル内だと問題あり？
					cMRUFolder.ClearAll();
				}
				InfoMessage( hwndDlg, LS(STR_PROPCOMGEN_DIR2) );
				return TRUE;

#ifdef NKMM_FIX_COLOR_FONT
			case IDC_BUTTON_EMOJIFONT:	/* 絵文字フォントの選択 20260816 */
				{
					LOGFONT lf = m_Common.m_sWindow.m_lfEmoji;
					INT nPointSize = m_Common.m_sWindow.m_nEmojiPointSize;
					if( MySelectFont( &lf, &nPointSize, hwndDlg, false ) ){
						m_Common.m_sWindow.m_lfEmoji = lf;
						m_Common.m_sWindow.m_nEmojiPointSize = nPointSize;
						//フォントを選んだら「使用する」扱いにする(タイプ別フォントと同じ挙動)
						m_Common.m_sWindow.m_bUseEmojiFont = TRUE;
						::CheckDlgButton( hwndDlg, IDC_CHECK_USEEMOJIFONT, TRUE );
						::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_EMOJILIGATURE ), TRUE );
						HFONT hFont = UpdateEmojiFontLabel( hwndDlg );
						if( m_hEmojiFont != NULL ){ ::DeleteObject( m_hEmojiFont ); }
						m_hEmojiFont = hFont;
					}
				}
				return TRUE;

			case IDC_CHECK_USEEMOJIFONT:	/* 絵文字フォントを使う 20260816 */
				{
					m_Common.m_sWindow.m_bUseEmojiFont = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_USEEMOJIFONT );
					// 20260817 「絵文字フォント」がOFFなら「合字」も連動して無効化
					::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_EMOJILIGATURE ), m_Common.m_sWindow.m_bUseEmojiFont );
					HFONT hFont = UpdateEmojiFontLabel( hwndDlg );
					if( m_hEmojiFont != NULL ){ ::DeleteObject( m_hEmojiFont ); }
					m_hEmojiFont = hFont;
				}
				return TRUE;
#endif // NKMM_

			}
			break;	/* BN_CLICKED */
		// 2009.01.12 nasukoji	コンボボックスのリストの項目が選択された
		case CBN_SELENDOK:
			HWND	hwndCombo;
			int		nSelPos;

			switch( wID ){
			// 組み合わせてホイール操作した時ページスクロールする
			case IDC_COMBO_WHEEL_PAGESCROLL:
				hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_PAGESCROLL );
				nSelPos = Combo_GetCurSel( hwndCombo );
				hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_HSCROLL );
				if( nSelPos && nSelPos == Combo_GetCurSel( hwndCombo ) ){
					Combo_SetCurSel( hwndCombo, 0 );
				}
				return TRUE;
			// 組み合わせてホイール操作した時横スクロールする
			case IDC_COMBO_WHEEL_HSCROLL:
				hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_HSCROLL );
				nSelPos = Combo_GetCurSel( hwndCombo );
				hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_PAGESCROLL );
				if( nSelPos && nSelPos == Combo_GetCurSel( hwndCombo ) ){
					Combo_SetCurSel( hwndCombo, 0 );
				}
				return TRUE;
			}
			break;	// CBN_SELENDOK
		}
		break;	/* WM_COMMAND */
	case WM_NOTIFY:
		idCtrl = (int)wParam;
		pNMHDR = (NMHDR*)lParam;
		pMNUD  = (NM_UPDOWN*)lParam;
		switch( idCtrl ){
		case IDC_SPIN_REPEATEDSCROLLLINENUM:
			/* キーリピート時のスクロール行数 */
//			MYTRACE( _T("IDC_SPIN_REPEATEDSCROLLLINENUM\n") );
			nVal = ::GetDlgItemInt( hwndDlg, IDC_EDIT_REPEATEDSCROLLLINENUM, NULL, FALSE );
			if( pMNUD->iDelta < 0 ){
				++nVal;
			}else
			if( pMNUD->iDelta > 0 ){
				--nVal;
			}
			if( nVal < 1 ){
				nVal = 1;
			}
			if( nVal > 10 ){
				nVal = 10;
			}
			::SetDlgItemInt( hwndDlg, IDC_EDIT_REPEATEDSCROLLLINENUM, nVal, FALSE );
			return TRUE;
		case IDC_SPIN_MAX_MRU_FILE:
			/* ファイルの履歴MAX */
//			MYTRACE( _T("IDC_SPIN_MAX_MRU_FILE\n") );
			nVal = ::GetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FILE, NULL, FALSE );
			if( pMNUD->iDelta < 0 ){
				++nVal;
			}else
			if( pMNUD->iDelta > 0 ){
				--nVal;
			}
			if( nVal < 0 ){
				nVal = 0;
			}
			if( nVal > MAX_MRU ){
				nVal = MAX_MRU;
			}
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FILE, nVal, FALSE );
			return TRUE;
		case IDC_SPIN_MAX_MRU_FOLDER:
			/* フォルダの履歴MAX */
//			MYTRACE( _T("IDC_SPIN_MAX_MRU_FOLDER\n") );
			nVal = ::GetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FOLDER, NULL, FALSE );
			if( pMNUD->iDelta < 0 ){
				++nVal;
			}else
			if( pMNUD->iDelta > 0 ){
				--nVal;
			}
			if( nVal < 0 ){
				nVal = 0;
			}
			if( nVal > MAX_OPENFOLDER ){
				nVal = MAX_OPENFOLDER;
			}
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FOLDER, nVal, FALSE );
			return TRUE;
		default:
			switch( pNMHDR->code ){
			case PSN_HELP:
				OnHelp( hwndDlg, IDD_PROP_GENERAL );
				return TRUE;
			case PSN_KILLACTIVE:
//				MYTRACE( _T("General PSN_KILLACTIVE\n") );
				/* ダイアログデータの取得 General */
				GetData( hwndDlg );
				return TRUE;
//@@@ 2002.01.03 YAZAKI 最後に表示していたシートを正しく覚えていないバグ修正
			case PSN_SETACTIVE:
				m_nPageNum = ID_PROPCOM_PAGENUM_GENERAL;	//Oct. 25, 2000 JEPRO ZENPAN1→ZENPAN に変更(参照しているのはCPropCommon.cppのみの1箇所)
				return TRUE;
			}
			break;
		}

//		MYTRACE( _T("pNMHDR->hwndFrom=%xh\n"), pNMHDR->hwndFrom );
//		MYTRACE( _T("pNMHDR->idFrom  =%xh\n"), pNMHDR->idFrom );
//		MYTRACE( _T("pNMHDR->code    =%xh\n"), pNMHDR->code );
//		MYTRACE( _T("pMNUD->iPos    =%d\n"), pMNUD->iPos );
//		MYTRACE( _T("pMNUD->iDelta  =%d\n"), pMNUD->iDelta );
		break;

#ifdef NKMM_FIX_COLOR_FONT
	case WM_DESTROY:
		// 20260816 絵文字フォント ラベル用フォントハンドルの破棄
		if( m_hEmojiFont != NULL ){
			::DeleteObject( m_hEmojiFont );
			m_hEmojiFont = NULL;
		}
		break;
#endif // NKMM_

//@@@ 2001.02.04 Start by MIK: Popup Help
	case WM_HELP:
		{
			HELPINFO *p = (HELPINFO *)lParam;
			MyWinHelp( (HWND)p->hItemHandle, HELP_WM_HELP, (ULONG_PTR)(LPVOID)p_helpids );	// 2006.10.10 ryoji MyWinHelpに変更に変更
		}
		return TRUE;
		/*NOTREACHED*/
//		break;
//@@@ 2001.02.04 End

//@@@ 2001.12.22 Start by MIK: Context Menu Help
	//Context Menu
	case WM_CONTEXTMENU:
		MyWinHelp( hwndDlg, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)p_helpids );	// 2006.10.10 ryoji MyWinHelpに変更に変更
		return TRUE;
//@@@ 2001.12.22 End

	}
	return FALSE;
}





/* ダイアログデータの設定 General */
void CPropGeneral::SetData( HWND hwndDlg )
{

	/* カーソルのタイプ 0=win 1=dos  */
	if( 0 == m_Common.m_sGeneral.GetCaretType() ){
		::CheckDlgButton( hwndDlg, IDC_RADIO_CARETTYPE0, TRUE );
		::CheckDlgButton( hwndDlg, IDC_RADIO_CARETTYPE1, FALSE );
	}else{
		::CheckDlgButton( hwndDlg, IDC_RADIO_CARETTYPE0, FALSE );
		::CheckDlgButton( hwndDlg, IDC_RADIO_CARETTYPE1, TRUE );
	}


	/* フリーカーソルモード */
	::CheckDlgButton( hwndDlg, IDC_CHECK_FREECARET, m_Common.m_sGeneral.m_bIsFreeCursorMode ? 1 : 0 );

	/* 単語単位で移動するときに、単語の両端で止まるか */
	::CheckDlgButton( hwndDlg, IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_WORD, m_Common.m_sGeneral.m_bStopsBothEndsWhenSearchWord );

	/* 段落単位で移動するときに、段落の両端で止まるか */
	::CheckDlgButton( hwndDlg, IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_PARAGRAPH, m_Common.m_sGeneral.m_bStopsBothEndsWhenSearchParagraph );

	//	2007.10.08 genta マウスクリックでアクティブになったときはカーソルをクリック位置に移動しない (2007.10.02 by nasukoji)
	::CheckDlgButton( hwndDlg, IDC_CHECK_NOMOVE_ACTIVATE_BY_MOUSE, m_Common.m_sGeneral.m_bNoCaretMoveByActivation );

	// 既に開いているときは新しいウィンドウで開く
	::CheckDlgButton( hwndDlg, IDC_CHECK_FILE_OPEN2OPEN, m_Common.m_sGeneral.m_bFileOpen2Open );

	/* [すべて閉じる]で他に編集用のウィンドウがあれば確認する */	// 2006.12.25 ryoji
	::CheckDlgButton( hwndDlg, IDC_CHECK_CLOSEALLCONFIRM, m_Common.m_sGeneral.m_bCloseAllConfirm );

	/* 終了時の確認をする */
	::CheckDlgButton( hwndDlg, IDC_CHECK_EXITCONFIRM, m_Common.m_sGeneral.m_bExitConfirm );

	/* キーリピート時のスクロール行数 */
	::SetDlgItemInt( hwndDlg, IDC_EDIT_REPEATEDSCROLLLINENUM, (Int)m_Common.m_sGeneral.m_nRepeatedScrollLineNum, FALSE );

	/* キーリピート時のスクロールを滑らかにするか */
	::CheckDlgButton( hwndDlg, IDC_CHECK_REPEATEDSCROLLSMOOTH, m_Common.m_sGeneral.m_nRepeatedScroll_Smooth );

	// 2009.01.17 nasukoji	組み合わせてホイール操作した時ページスクロールする
	HWND	hwndCombo;
	int		nSelPos;
	int		i;

	hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_PAGESCROLL );
	Combo_ResetContent( hwndCombo );
	nSelPos = 0;
	for( i = 0; i < _countof( SpecialScrollModeArr ); ++i ){
		Combo_InsertString( hwndCombo, i, LS( SpecialScrollModeArr[i].nNameId ) );
		if( SpecialScrollModeArr[i].nMethod == m_Common.m_sGeneral.m_nPageScrollByWheel ){	// ページスクロールとする組み合わせ操作
			nSelPos = i;
		}
	}
	Combo_SetCurSel( hwndCombo, nSelPos );

	// 2009.01.12 nasukoji	組み合わせてホイール操作した時横スクロールする
	hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_HSCROLL );
	Combo_ResetContent( hwndCombo );
	nSelPos = 0;
	for( i = 0; i < _countof( SpecialScrollModeArr ); ++i ){
		Combo_InsertString( hwndCombo, i, LS( SpecialScrollModeArr[i].nNameId ) );
		if( SpecialScrollModeArr[i].nMethod == m_Common.m_sGeneral.m_nHorizontalScrollByWheel ){	// 横スクロールとする組み合わせ操作
			nSelPos = i;
		}
	}
	Combo_SetCurSel( hwndCombo, nSelPos );

	// 2007.09.09 Moca 画面キャッシュ設定追加
	// 画面キャッシュを使う
	::CheckDlgButton( hwndDlg, IDC_CHECK_MEMDC, m_Common.m_sWindow.m_bUseCompatibleBMP );

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE
	// 20260801 グリフキャッシュ(グリフアトラス)を使う
	::CheckDlgButton( hwndDlg, IDC_CHECK_GLYPHATLASCACHE, m_Common.m_sWindow.m_bUseGlyphAtlasCache );
#endif // NKMM_

#ifdef NKMM_FIX_FONT_QUALITY
	// 20260810 描画品質
	{
		HWND hwndComboQuality = ::GetDlgItem( hwndDlg, IDC_COMBO_FONTQUALITY );
		Combo_ResetContent( hwndComboQuality );
		int nSelPosQuality = 0;
		for( int q = 0; q < _countof( FontQualityArr ); ++q ){
			Combo_InsertString( hwndComboQuality, q, FontQualityArr[q].pszName );
			if( FontQualityArr[q].nValue == (BYTE)m_Common.m_sWindow.m_nFontQuality ){
				nSelPosQuality = q;
			}
		}
		Combo_SetCurSel( hwndComboQuality, nSelPosQuality );
	}
#endif // NKMM_

#ifdef NKMM_FIX_COLOR_FONT
	// 20260816 絵文字フォントの固定指定
	::CheckDlgButton( hwndDlg, IDC_CHECK_USEEMOJIFONT, m_Common.m_sWindow.m_bUseEmojiFont );
	m_hEmojiFont = UpdateEmojiFontLabel( hwndDlg );
	// 20260816 絵文字の合字(ZWJ結合絵文字・キーキャップ等)を有効にするか（「絵文字フォント」がOFFなら無効化）
	::CheckDlgButton( hwndDlg, IDC_CHECK_EMOJILIGATURE, m_Common.m_sWindow.m_bUseEmojiLigature );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_EMOJILIGATURE ), m_Common.m_sWindow.m_bUseEmojiFont );
#endif // NKMM_

	/* ファイルの履歴MAX */
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FILE, m_Common.m_sGeneral.m_nMRUArrNum_MAX, FALSE );

	/* フォルダの履歴MAX */
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FOLDER, m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX, FALSE );

#if defined(NKMM_FIX_PROFILES) && NKMM_DELETE_HISTORY_NOT_EXIST_AT_STARTUP
	// 20260813 起動時に存在しない履歴を確認して削除する
	::CheckDlgButton( hwndDlg, IDC_CHECK_DELETE_MISSING_HISTORY, m_Common.m_sGeneral.m_bConfirmDeleteMissingHistory );
#endif // NKMM_

	/* タスクトレイを使う */
	::CheckDlgButton( hwndDlg, IDC_CHECK_USETRAYICON, m_Common.m_sGeneral.m_bUseTaskTray );
// From Here 2001.12.03 hor
//@@@ YAZAKI 2001.12.31 ここは制御する。
	if( m_Common.m_sGeneral.m_bUseTaskTray ){
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), TRUE );
	}else{
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), FALSE );
	}
// To Here 2001.12.03 hor
	/* タスクトレイに常駐 */
	::CheckDlgButton( hwndDlg, IDC_CHECK_STAYTASKTRAY, m_Common.m_sGeneral.m_bStayTaskTray );

	/* タスクトレイ左クリックメニューのショートカット */
	HotKey_SetHotKey( ::GetDlgItem( hwndDlg, IDC_HOTKEY_TRAYMENU ), m_Common.m_sGeneral.m_wTrayMenuHotKeyCode, m_Common.m_sGeneral.m_wTrayMenuHotKeyMods );

	return;
}





/* ダイアログデータの取得 General */
int CPropGeneral::GetData( HWND hwndDlg )
{
	/* カーソルのタイプ 0=win 1=dos  */
	if( ::IsDlgButtonChecked( hwndDlg, IDC_RADIO_CARETTYPE0 ) ){
		m_Common.m_sGeneral.SetCaretType(0);
	}
	if( ::IsDlgButtonChecked( hwndDlg, IDC_RADIO_CARETTYPE1 ) ){
		m_Common.m_sGeneral.SetCaretType(1);
	}

	/* フリーカーソルモード */
	m_Common.m_sGeneral.m_bIsFreeCursorMode = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_FREECARET ) != 0;

	/* 単語単位で移動するときに、単語の両端で止まるか */
	m_Common.m_sGeneral.m_bStopsBothEndsWhenSearchWord = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_WORD );
	//	2007.10.08 genta マウスクリックでアクティブになったときはカーソルをクリック位置に移動しない (2007.10.02 by nasukoji)
	m_Common.m_sGeneral.m_bNoCaretMoveByActivation = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_NOMOVE_ACTIVATE_BY_MOUSE );

	/* 段落単位で移動するときに、段落の両端で止まるか */
	m_Common.m_sGeneral.m_bStopsBothEndsWhenSearchParagraph = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_STOPS_BOTH_ENDS_WHEN_SEARCH_PARAGRAPH );

	// 
	m_Common.m_sGeneral.m_bFileOpen2Open = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_FILE_OPEN2OPEN );

	/* [すべて閉じる]で他に編集用のウィンドウがあれば確認する */	// 2006.12.25 ryoji
	m_Common.m_sGeneral.m_bCloseAllConfirm = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_CLOSEALLCONFIRM );

	/* 終了時の確認をする */
	m_Common.m_sGeneral.m_bExitConfirm = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_EXITCONFIRM );

	/* キーリピート時のスクロール行数 */
	m_Common.m_sGeneral.m_nRepeatedScrollLineNum = (CLayoutInt)::GetDlgItemInt( hwndDlg, IDC_EDIT_REPEATEDSCROLLLINENUM, NULL, FALSE );
	if( m_Common.m_sGeneral.m_nRepeatedScrollLineNum < CLayoutInt(1) ){
		m_Common.m_sGeneral.m_nRepeatedScrollLineNum = CLayoutInt(1);
	}
	if( m_Common.m_sGeneral.m_nRepeatedScrollLineNum > CLayoutInt(10) ){
		m_Common.m_sGeneral.m_nRepeatedScrollLineNum = CLayoutInt(10);
	}

	/* キーリピート時のスクロールを滑らかにするか */
	m_Common.m_sGeneral.m_nRepeatedScroll_Smooth = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_REPEATEDSCROLLSMOOTH );

	// 2009.01.17 nasukoji	組み合わせてホイール操作した時ページスクロールする
	HWND	hwndCombo;
	int		nSelPos;

	// 2007.09.09 Moca 画面キャッシュ設定追加
	// 画面キャッシュを使う
	m_Common.m_sWindow.m_bUseCompatibleBMP = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_MEMDC );

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE
	// 20260801 グリフキャッシュ(グリフアトラス)を使う
	m_Common.m_sWindow.m_bUseGlyphAtlasCache = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_GLYPHATLASCACHE );
#endif // NKMM_

#ifdef NKMM_FIX_FONT_QUALITY
	// 20260810 描画品質
	{
		HWND hwndComboQuality = ::GetDlgItem( hwndDlg, IDC_COMBO_FONTQUALITY );
		int nSelPosQuality = Combo_GetCurSel( hwndComboQuality );
		if( 0 <= nSelPosQuality && nSelPosQuality < _countof( FontQualityArr ) ){
			m_Common.m_sWindow.m_nFontQuality = FontQualityArr[nSelPosQuality].nValue;
		}
	}
#endif // NKMM_

#ifdef NKMM_FIX_COLOR_FONT
	// 20260816 絵文字フォントの固定指定(フォント自体はIDC_BUTTON_EMOJIFONTクリック時に
	// 即時反映済み。ここではチェック状態のみ再同期する)
	m_Common.m_sWindow.m_bUseEmojiFont = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_USEEMOJIFONT );
	// 20260816 絵文字の合字(ZWJ結合絵文字・キーキャップ等)を有効にするか
	m_Common.m_sWindow.m_bUseEmojiLigature = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_EMOJILIGATURE );
#endif // NKMM_

	hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_PAGESCROLL );
	nSelPos = Combo_GetCurSel( hwndCombo );
	m_Common.m_sGeneral.m_nPageScrollByWheel = SpecialScrollModeArr[nSelPos].nMethod;		// ページスクロールとする組み合わせ操作

	// 2009.01.17 nasukoji	組み合わせてホイール操作した時横スクロールする
	hwndCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_WHEEL_HSCROLL );
	nSelPos = Combo_GetCurSel( hwndCombo );
	m_Common.m_sGeneral.m_nHorizontalScrollByWheel = SpecialScrollModeArr[nSelPos].nMethod;	// 横スクロールとする組み合わせ操作

	/* ファイルの履歴MAX */
	m_Common.m_sGeneral.m_nMRUArrNum_MAX = ::GetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FILE, NULL, FALSE );
	if( m_Common.m_sGeneral.m_nMRUArrNum_MAX < 0 ){
		m_Common.m_sGeneral.m_nMRUArrNum_MAX = 0;
	}
	if( m_Common.m_sGeneral.m_nMRUArrNum_MAX > MAX_MRU ){
		m_Common.m_sGeneral.m_nMRUArrNum_MAX = MAX_MRU;
	}

	{	//履歴の管理	//@@@ 2003.04.09 MIK
		CRecentFile	cRecentFile;
		cRecentFile.UpdateView();
		cRecentFile.Terminate();
	}

	/* フォルダの履歴MAX */
	m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX = ::GetDlgItemInt( hwndDlg, IDC_EDIT_MAX_MRU_FOLDER, NULL, FALSE );
	if( m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX < 0 ){
		m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX = 0;
	}
	if( m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX > MAX_OPENFOLDER ){
		m_Common.m_sGeneral.m_nOPENFOLDERArrNum_MAX = MAX_OPENFOLDER;
	}

	{	//履歴の管理	//@@@ 2003.04.09 MIK
		CRecentFolder	cRecentFolder;
		cRecentFolder.UpdateView();
		cRecentFolder.Terminate();
	}

#if defined(NKMM_FIX_PROFILES) && NKMM_DELETE_HISTORY_NOT_EXIST_AT_STARTUP
	// 20260813 起動時に存在しない履歴を確認して削除する
	m_Common.m_sGeneral.m_bConfirmDeleteMissingHistory = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_DELETE_MISSING_HISTORY );
#endif // NKMM_

	/* タスクトレイを使う */
	m_Common.m_sGeneral.m_bUseTaskTray = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_USETRAYICON );
//@@@ YAZAKI 2001.12.31 m_bUseTaskTrayに引きづられるように。
	if( m_Common.m_sGeneral.m_bUseTaskTray ){
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), TRUE );
	}else{
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_STAYTASKTRAY ), FALSE );
	}
	/* タスクトレイに常駐 */
	m_Common.m_sGeneral.m_bStayTaskTray = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_STAYTASKTRAY );

	/* タスクトレイ左クリックメニューのショートカット */
	LRESULT	lResult;
	lResult = HotKey_GetHotKey( ::GetDlgItem( hwndDlg, IDC_HOTKEY_TRAYMENU ) );
	m_Common.m_sGeneral.m_wTrayMenuHotKeyCode = LOBYTE( lResult );
	m_Common.m_sGeneral.m_wTrayMenuHotKeyMods = HIBYTE( lResult );

	return TRUE;
}

#ifdef NKMM_FIX_COLOR_FONT
/*!	絵文字フォントのラベル表示(IDC_STATIC_EMOJIFONT)を、現在の使用有無/選択フォントに
	合わせて更新する。

	「使用する」チェックが外れている場合、またはチェックはあるがフォントが一度も
	選択されていない場合(lfFaceNameが空)は、「システムに任せる」ことが伝わるよう
	ラベルを空にする(タイプ別フォントのSetFontLabelのbUse=false相当。ただしこちらの
	チェックボックスはタイプ別フォントと異なり常時有効で、いつでも付け外しできる)。

	@date 20260816
*/
HFONT CPropGeneral::UpdateEmojiFontLabel( HWND hwndDlg )
{
	bool bUse = ( FALSE != m_Common.m_sWindow.m_bUseEmojiFont )
		&& ( 0 != m_Common.m_sWindow.m_lfEmoji.lfFaceName[0] );
	if( !bUse ){
		::DlgItem_SetText( hwndDlg, IDC_STATIC_EMOJIFONT, _T("(システムの自動選択)") );
		return NULL;
	}
	return SetFontLabel( hwndDlg, IDC_STATIC_EMOJIFONT, m_Common.m_sWindow.m_lfEmoji, m_Common.m_sWindow.m_nEmojiPointSize );
}
#endif // NKMM_
