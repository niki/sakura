/*!	@file
	@brief 検索ダイアログボックス

	@author Norio Nakatani
	@date	1998/12/12 再作成
	@date 2001/06/23 N.Nakatani 単語単位で検索する機能を実装
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2001, genta, JEPRO, hor, Stonee
	Copyright (C) 2002, MIK, hor, YAZAKI, genta
	Copyright (C) 2005, zenryaku
	Copyright (C) 2006, ryoji
	Copyright (C) 2009, ryoji
	Copyright (C) 2012, Uchi

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"
#include "dlg/CDlgFind.h"
#include "view/CEditView.h"
#include "util/shell.h"
#include "sakura_rc.h"
#include "sakura.hh"
#ifdef UZ_FIX_FINDDLG
#include "window/CEditWnd.h"
#include "uiparts/CMenuDrawer.h"
#endif  // UZ_

//検索 CDlgFind.cpp	//@@@ 2002.01.07 add start MIK
const DWORD p_helpids[] = {	//11800
	IDC_BUTTON_SEARCHNEXT,			HIDC_FIND_BUTTON_SEARCHNEXT,		//次を検索
	IDC_BUTTON_SEARCHPREV,			HIDC_FIND_BUTTON_SEARCHPREV,		//前を検索
	IDCANCEL,						HIDCANCEL_FIND,						//キャンセル
	IDC_BUTTON_HELP,				HIDC_FIND_BUTTON_HELP,				//ヘルプ
	IDC_CHK_WORD,					HIDC_FIND_CHK_WORD,					//単語単位
	IDC_CHK_LOHICASE,				HIDC_FIND_CHK_LOHICASE,				//大文字小文字
	IDC_CHK_REGULAREXP,				HIDC_FIND_CHK_REGULAREXP,			//正規表現
	IDC_CHECK_NOTIFYNOTFOUND,		HIDC_FIND_CHECK_NOTIFYNOTFOUND,		//見つからないときに通知
	IDC_CHECK_bAutoCloseDlgFind,	HIDC_FIND_CHECK_bAutoCloseDlgFind,	//自動的に閉じる
	IDC_COMBO_TEXT,					HIDC_FIND_COMBO_TEXT,				//検索文字列
	IDC_STATIC_JRE32VER,			HIDC_FIND_STATIC_JRE32VER,			//正規表現バージョン
	IDC_BUTTON_SETMARK,				HIDC_FIND_BUTTON_SETMARK,			//2002.01.16 hor 検索該当行をマーク
	IDC_CHECK_SEARCHALL,			HIDC_FIND_CHECK_SEARCHALL,			//2002.01.26 hor 先頭（末尾）から再検索
//	IDC_STATIC,						-1,
	0, 0
};	//@@@ 2002.01.07 add end MIK

CDlgFind::CDlgFind()
{
	m_sSearchOption.Reset();

#ifdef UZ_FIX_FINDDLG
	HWND hwndBtn = GetDlgItem(GetHwnd(), IDC_BUTTON_SEARCHNEXT);
	CMenuDrawer cMenuDrawer;
	cMenuDrawer.Create(G_AppInstance(), hwndBtn, NULL);
	cMenuDrawer.ResetContents();
	HMENU hMenuPopUp = ::CreatePopupMenu();
	
	// 次を検索メニュー
	{ 
		TCHAR szLabel[_MAX_PATH * 2+ 30] = _T("");
		TCHAR szKey[10] = _T("");
		CKeyBind::GetMenuLabel(G_AppInstance(), 
			m_pShareData->m_Common.m_sKeyBind.m_nKeyNameArrNum,
			m_pShareData->m_Common.m_sKeyBind.m_pKeyNameArr,
			F_SEARCH_NEXT, szLabel, szKey, true, _countof(szLabel)
		);
		cMenuDrawer.MyAppendMenu(hMenuPopUp, MF_BYPOSITION | MF_STRING, 1, szLabel, _T(""));
	}
	
	// 前を検索メニュー
	{
		TCHAR szLabel[_MAX_PATH * 2+ 30] = _T("");
		TCHAR szKey[10] = _T("");
		CKeyBind::GetMenuLabel(G_AppInstance(), 
			m_pShareData->m_Common.m_sKeyBind.m_nKeyNameArrNum,
			m_pShareData->m_Common.m_sKeyBind.m_pKeyNameArr,
			F_SEARCH_PREV, szLabel, szKey, true, _countof(szLabel)
		);
		cMenuDrawer.MyAppendMenu(hMenuPopUp, MF_BYPOSITION | MF_STRING, 2, szLabel, _T(""));
	}
	
	m_hMenuPopUp = hMenuPopUp;
#endif  // UZ_
	return;
}

#ifdef UZ_FIX_FINDDLG
CDlgFind::~CDlgFind() {
	::DestroyMenu(m_hMenuPopUp);
}
#endif  // UZ_

#ifdef UZ_FIX_FINDDLG
/** 標準以外のメッセージを捕捉する
	@date 2008.05.28 ryoji 新規作成
*/
INT_PTR CDlgFind::DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
	CEditView *pcEditView = (CEditView *)m_lParam;
	
	INT_PTR result;
	result = CDialog::DispatchEvent( hWnd, wMsg, wParam, lParam );
	//si::logln(L"wMsg %x %x %x", wMsg, HIWORD(wParam), LOWORD(wParam));
	switch( wMsg ){
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code ==  BCN_DROPDOWN) {
			HWND hwndBtn = GetDlgItem(GetHwnd(), IDC_BUTTON_SEARCHNEXT);
			RECT rc;
			::GetClientRect(hwndBtn, &rc);
			POINT po = {rc.left, rc.bottom};
			::ClientToScreen(hwndBtn, &po);
			int nId = (int)::TrackPopupMenu(m_hMenuPopUp,
				TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON,
				po.x, po.y,
				0,
				hwndBtn,
				NULL
			);
			// 
			if (nId > 0) {
				int nRet = GetData();
				if (0 < nRet) {
					if (nId == 1) {
						/* 次を検索 */
						pcEditView->GetCommander().HandleCommand(F_SEARCH_NEXT, true, (LPARAM)GetHwnd(), 0, 0, 0);
						pcEditView->Redraw();
					} else if (nId == 2) {
						/* 前を検索 */
						pcEditView->GetCommander().HandleCommand(F_SEARCH_PREV, true, (LPARAM)GetHwnd(), 0, 0, 0);
						pcEditView->Redraw();
					}
					
					// 検索開始位置を登録
					if (FALSE != pcEditView->m_bSearch) {
						// 検索開始時のカーソル位置登録条件変更 02/07/28 ai start
						pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
						pcEditView->m_bSearch = FALSE;
					}
				}
			}
		}
		break;
	case WM_COMMAND:
		//si::logln(L"WM_COMMAND %x %x", wParam, lParam);
		
		// 検索ダイアログを自動的に閉じる場合は逐次検索はしない
		if (::IsDlgButtonChecked(GetHwnd(), IDC_CHECK_bAutoCloseDlgFind)) {
			break;
		}

		// リストボックスが表示されているときは処理しない
		if (::SendMessage(GetItemHwnd(IDC_COMBO_TEXT), CB_GETDROPPEDSTATE, 0, 0)) {
			break;
		}

		if (HIWORD(wParam) == 1) {  // アクセラレータからのメッセージ
			EFunctionCode nFuncCode = CKeyBind::GetFuncCode(
				LOWORD(wParam),
				m_pShareData->m_Common.m_sKeyBind.m_nKeyNameArrNum,
				m_pShareData->m_Common.m_sKeyBind.m_pKeyNameArr
			);
			if (nFuncCode == F_UP) {
				pcEditView->GetCommander().HandleCommand((EFunctionCode)(nFuncCode | FA_FROMKEYBOARD), true,
				                                         (LPARAM)GetHwnd(), 0, 0, 0);
			} else if (nFuncCode == F_DOWN) {
				pcEditView->GetCommander().HandleCommand((EFunctionCode)(nFuncCode | FA_FROMKEYBOARD), true,
				                                         (LPARAM)GetHwnd(), 0, 0, 0);
			} else if (nFuncCode == F_SEARCH_PREV) {
				int nRet = GetData();
				if (0 < nRet) {
					pcEditView->GetCommander().HandleCommand((EFunctionCode)(nFuncCode | FA_FROMKEYBOARD), true,
					                                         (LPARAM)GetHwnd(), 0, 0, 0);
				}
			} else if (nFuncCode == F_SEARCH_NEXT) {
				int nRet = GetData();
				if (0 < nRet) {
					pcEditView->GetCommander().HandleCommand((EFunctionCode)(nFuncCode | FA_FROMKEYBOARD), true,
					                                         (LPARAM)GetHwnd(), 0, 0, 0);
				}
			}
			break;
		}

		if (LOWORD(wParam) == IDC_COMBO_TEXT) {
			//si::logln(L"  IDC_COMBO_TEXT %x %x", HIWORD(lParam), LOWORD(lParam));
			int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
			std::vector<TCHAR> vText(nBufferSize);
			::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize);
			std::wstring text = to_wchar(&vText[0]);
			if (text != m_inputText) {
				m_inputText = text;

				pcEditView->GetSelectionInfo().DisableSelectArea(false);  // 選択解除

				pcEditView->SBMarker_->WaitForDraw(true);
				pcEditView->SBMarker_->WaitForBuild(true);
				
				int ret = InstantInput();
				
				if (ret < 0) {
					SetStatus(ret);
				}
				
				pcEditView->SB_Marker_Clear(1700);
			}
		}
		break;
	}
	return result;
}

void CDlgFind::SetStatus(int stat) {
	auto fnClearBG = [](HDC hdc, int x, int y, int w, int h) {
		HBRUSH hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
		HBRUSH hBrushOld = (HBRUSH)::SelectObject(hdc, hBrush);

		::PatBlt(hdc, x, y, w, h, PATCOPY);

		::SelectObject(hdc, hBrushOld);
		::SelectClipRgn(hdc, NULL);
	};
	
	auto fnDrawSysIcon = [](HDC hdc, int x, int y, LPCTSTR iconName) {
		HICON hIcon = ::LoadIcon(NULL, iconName);
		::DrawIcon(hdc, x, y, hIcon);
		::DestroyIcon(hIcon);
	};
	

	bool bClear = true;

	RECT rc;
	::GetClientRect(GetHwnd(), &rc);
	
	int fromWidth  = DpiScaleX(32);
	int fromHeight = DpiScaleY(32);
	int toWidth    = DpiScaleX(18);
	int toHeight   = DpiScaleY(18);

	int baseIconX = rc.right - fromWidth;
	int baseIconY = DpiScaleY(32);
	
	int iconX = DpiScaleX(7);
	int iconY = DpiScaleY(75);
	
	HDC hdc = ::GetDC(GetHwnd());
	int iStretchModeOld = ::SetStretchBltMode(hdc, HALFTONE);
	
	auto fnSrcCopyBlt = [=](HWND hwnd) {
		::StretchBlt(hdc, iconX, iconY, toWidth, toHeight, hdc, baseIconX, baseIconY, fromWidth, fromHeight, SRCCOPY);
		fnClearBG(hdc, baseIconX, baseIconY, fromWidth, fromHeight);  // 不要になった個所を消す
		::UpdateWindow(hwnd);
	};

	if (stat > 0) {  // 見つかった行の数
		fnDrawSysIcon(hdc, baseIconX, baseIconY, IDI_INFORMATION);
		fnSrcCopyBlt(GetHwnd());
		
		TCHAR szMsg[128];
		auto_sprintf(szMsg, _T("%d lines found!"), stat);
		::DlgItem_SetText(GetHwnd(), IDC_FIND_RESULT, szMsg);
		
		bClear = false;
		
	} else if (stat == -1) {  // 正規表現失敗
		fnDrawSysIcon(hdc, baseIconX, baseIconY, IDI_ERROR);
		fnSrcCopyBlt(GetHwnd());
		
		::DlgItem_SetText(GetHwnd(), IDC_FIND_RESULT, _T("invalid regex!!"));
		
		bClear = false;
		
	} else if (stat == -2) {  // 前方
		fnDrawSysIcon(hdc, baseIconX, baseIconY, IDI_WARNING);
		fnSrcCopyBlt(GetHwnd());
		
		::DlgItem_SetText(GetHwnd(), IDC_FIND_RESULT, _T("not found in ↓"));
		
		bClear = false;
		
	} else if (stat == -3) {  // 後方
		fnDrawSysIcon(hdc, baseIconX, baseIconY, IDI_WARNING);
		fnSrcCopyBlt(GetHwnd());
		
		::DlgItem_SetText(GetHwnd(), IDC_FIND_RESULT, _T("not found in ↑"));
		
		bClear = false;
	}

	::SetStretchBltMode(hdc, iStretchModeOld);

	if (bClear) {
		fnClearBG(hdc, iconX, iconY, toWidth, toHeight);  // 不要な個所を消す
		::UpdateWindow(GetHwnd());
		
		::DlgItem_SetText(GetHwnd(), IDC_FIND_RESULT, _T(""));
	}
	
	::ReleaseDC(GetHwnd(), hdc);
	
	//si::logln(L"   **** SetStatus stat=%d", stat);
}
#endif  // UZ_

/*!
	コンボボックスのドロップダウンメッセージを捕捉する

	@date 2013.03.24 novice 新規作成
*/
BOOL CDlgFind::OnCbnDropDown( HWND hwndCtl, int wID )
{
	switch( wID ){
	case IDC_COMBO_TEXT:
		if ( ::SendMessage(hwndCtl, CB_GETCOUNT, 0L, 0L) == 0) {
			int nSize = m_pShareData->m_sSearchKeywords.m_aSearchKeys.size();
			for (int i = 0; i < nSize; ++i) {
				Combo_AddString( hwndCtl, m_pShareData->m_sSearchKeywords.m_aSearchKeys[i] );
			}
		}
		break;
	}
	return CDialog::OnCbnDropDown( hwndCtl, wID );
}

/* モードレスダイアログの表示 */
HWND CDlgFind::DoModeless( HINSTANCE hInstance, HWND hwndParent, LPARAM lParam )
{
	m_sSearchOption = m_pShareData->m_Common.m_sSearch.m_sSearchOption;		// 検索オプション
	m_bNOTIFYNOTFOUND = m_pShareData->m_Common.m_sSearch.m_bNOTIFYNOTFOUND;	// 検索／置換  見つからないときメッセージを表示
	m_ptEscCaretPos_PHY = ((CEditView*)lParam)->GetCaret().GetCaretLogicPos();	// 検索開始時のカーソル位置退避
	((CEditView*)lParam)->m_bSearch = TRUE;							// 検索開始位置の登録有無		02/07/28 ai
	return CDialog::DoModeless( hInstance, hwndParent, IDD_FIND, lParam, SW_SHOW );
}

/* モードレス時：検索対象となるビューの変更 */
void CDlgFind::ChangeView( LPARAM pcEditView )
{
	m_lParam = pcEditView;
	return;
}



BOOL CDlgFind::OnInitDialog( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
	BOOL bRet = CDialog::OnInitDialog(hwnd, wParam, lParam);
	m_comboDel = SComboBoxItemDeleter();
	m_comboDel.pRecent = &m_cRecentSearch;
	SetComboBoxDeleter(GetItemHwnd(IDC_COMBO_TEXT), &m_comboDel);

	// フォント設定	2012/11/27 Uchi
	HFONT hFontOld = (HFONT)::SendMessageAny( GetItemHwnd( IDC_COMBO_TEXT ), WM_GETFONT, 0, 0 );
#ifdef UZ_FIX_FINDDLG
	HFONT hFont = SetMainFont( GetItemHwnd( IDC_COMBO_TEXT ), 2 );
#else
	HFONT hFont = SetMainFont( GetItemHwnd( IDC_COMBO_TEXT ) );
#endif  // UZ_
	m_cFontText.SetFont( hFontOld, hFont, GetItemHwnd( IDC_COMBO_TEXT ) );
	return bRet;
}



BOOL CDlgFind::OnDestroy()
{
#ifdef UZ_FIX_FINDDLG
	// 最後に入力した文字列を履歴に追加する
	if (!m_inputText.empty()) {  // 入力中の検索は履歴に残さない
		CSearchKeywordManager().AddToSearchKeyArr( m_strText.c_str() );
	}
#endif  // UZ_
	m_cFontText.ReleaseOnDestroy();
	return CDialog::OnDestroy();
}



/* ダイアログデータの設定 */
void CDlgFind::SetData( void )
{
//	MYTRACE( _T("CDlgFind::SetData()") );

	/*****************************
	*           初期化           *
	*****************************/
	// Here Jun. 26, 2001 genta
	// 正規表現ライブラリの差し替えに伴う処理の見直しによりjre.dll判定を削除

	/* ユーザーがコンボ ボックスのエディット コントロールに入力できるテキストの長さを制限する */
	// 2011.12.18 長さ制限撤廃
	// Combo_LimitText( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT ), _MAX_PATH - 1 );
	/* コンボボックスのユーザー インターフェイスを拡張インターフェースにする */
	Combo_SetExtendedUI( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT ), TRUE );


	/*****************************
	*         データ設定         *
	*****************************/
	/* 検索文字列 */
	// 検索文字列リストの設定(関数化)	2010/5/28 Uchi
	SetCombosList();

	/* 英大文字と英小文字を区別する */
	::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, m_sSearchOption.bLoHiCase );

	// 2001/06/23 Norio Nakatani
	/* 単語単位で検索 */
	::CheckDlgButton( GetHwnd(), IDC_CHK_WORD, m_sSearchOption.bWordOnly );

	/* 検索／置換  見つからないときメッセージを表示 */
#ifdef UZ_FIX_FINDDLG
	::CheckDlgButton( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND, FALSE );
#else
	::CheckDlgButton( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND, m_bNOTIFYNOTFOUND );
#endif  // UZ_

	// From Here Jun. 29, 2001 genta
	// 正規表現ライブラリの差し替えに伴う処理の見直し
	// 処理フロー及び判定条件の見直し。必ず正規表現のチェックと
	// 無関係にCheckRegexpVersionを通過するようにした。
	if( CheckRegexpVersion( GetHwnd(), IDC_STATIC_JRE32VER, false )
		&& m_sSearchOption.bRegularExp){
		/* 英大文字と英小文字を区別する */
		::CheckDlgButton( GetHwnd(), IDC_CHK_REGULAREXP, 1 );
//正規表現がONでも、大文字小文字を区別する／しないを選択できるように。
//		::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, 1 );
//		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_LOHICASE ), FALSE );

		// 2001/06/23 N.Nakatani
		/* 単語単位で探す */
		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), FALSE );
	}
	else {
		::CheckDlgButton( GetHwnd(), IDC_CHK_REGULAREXP, 0 );
	}
	// To Here Jun. 29, 2001 genta

	/* 検索ダイアログを自動的に閉じる */
#ifdef UZ_FIX_FINDDLG
	::CheckDlgButton( GetHwnd(), IDC_CHECK_bAutoCloseDlgFind, FALSE );
#else
	::CheckDlgButton( GetHwnd(), IDC_CHECK_bAutoCloseDlgFind, m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind );
#endif  // UZ_

	/* 先頭（末尾）から再検索 2002.01.26 hor */
	::CheckDlgButton( GetHwnd(), IDC_CHECK_SEARCHALL, m_pShareData->m_Common.m_sSearch.m_bSearchAll );

#ifdef UZ_FIX_DIALOG_POS
	{
		RECT rcView;
		CEditView* pcEditView=(CEditView*)m_lParam;
		::GetWindowRect(pcEditView->GetHwnd(), &rcView);
#ifdef UZ_FIX_FINDDLG
		SetPlaceOfWindow(::GetParent(pcEditView->GetHwnd()), &rcView, CDialog::DLGPLACE_TR);
#else
		SetPlaceOfWindow(::GetParent(pcEditView->GetHwnd()), &rcView);
#endif
	}
#endif  // UZ_

#ifdef UZ_FIX_FINDDLG
	// 最後に検索した文字列をマークする 2017.6.28 
	{
		CEditView* pcEditView=(CEditView*)m_lParam;
		
		int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
		std::vector<TCHAR> vText(nBufferSize);
		::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize);
		std::wstring text = to_wchar(&vText[0]);
		m_strText = to_wchar(&vText[0]);
		
		if (text.empty()) {
			pcEditView->m_bCurSrchKeyMark = false;	/* 検索文字列のマーク */
		} else {
			pcEditView->m_nCurSearchKeySequence = GetDllShareData().m_Common.m_sSearch.m_nSearchKeySequence;
			pcEditView->m_bCurSearchUpdate = true;
			pcEditView->ChangeCurRegexp(false);
		}
		pcEditView->Redraw();
#ifdef UZ_FIX_EDITVIEW_SCRBAR
		pcEditView->SB_Marker_Clear(1701);
#endif  // UZ_
	}
#endif  // UZ_

	return;
}


// 検索文字列リストの設定
//	2010/5/28 Uchi
void CDlgFind::SetCombosList( void )
{
	HWND	hwndCombo;

	/* 検索文字列 */
	hwndCombo = ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT );
	while (Combo_GetCount(hwndCombo) > 0) {
		Combo_DeleteString( hwndCombo, 0);
	}
	int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
	std::vector<TCHAR> vText(nBufferSize);
	Combo_GetText( hwndCombo, &vText[0], nBufferSize );
	if (auto_strcmp( to_wchar(&vText[0]), m_strText.c_str() ) != 0) {
		::DlgItem_SetText( GetHwnd(), IDC_COMBO_TEXT, m_strText.c_str() );
	}
}


/* ダイアログデータの取得 */
int CDlgFind::GetData( void )
{
//	MYTRACE( _T("CDlgFind::GetData()") );

	/* 英大文字と英小文字を区別する */
	m_sSearchOption.bLoHiCase = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_LOHICASE ));

	// 2001/06/23 Norio Nakatani
	/* 単語単位で検索 */
	m_sSearchOption.bWordOnly = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_WORD ));

	/* 一致する単語のみ検索する */
	/* 正規表現 */
	m_sSearchOption.bRegularExp = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ));

	/* 検索／置換  見つからないときメッセージを表示 */
	m_bNOTIFYNOTFOUND = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND );

	m_pShareData->m_Common.m_sSearch.m_bNOTIFYNOTFOUND = m_bNOTIFYNOTFOUND;	// 検索／置換  見つからないときメッセージを表示

	/* 検索文字列 */
	int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
	std::vector<TCHAR> vText(nBufferSize);
	::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize);
	m_strText = to_wchar(&vText[0]);

	/* 検索ダイアログを自動的に閉じる */
	m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_bAutoCloseDlgFind );

	/* 先頭（末尾）から再検索 2002.01.26 hor */
	m_pShareData->m_Common.m_sSearch.m_bSearchAll = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_SEARCHALL );

	if( 0 < m_strText.length() ){
		/* 正規表現？ */
		// From Here Jun. 26, 2001 genta
		//	正規表現ライブラリの差し替えに伴う処理の見直し
		int nFlag = 0x00;
		nFlag |= m_sSearchOption.bLoHiCase ? 0x01 : 0x00;
		if( m_sSearchOption.bRegularExp && !CheckRegexpSyntax( m_strText.c_str(), GetHwnd(), true, nFlag ) ){
			return -1;
		}
		// To Here Jun. 26, 2001 genta 正規表現ライブラリ差し替え

		/* 検索文字列 */
		//@@@ 2002.2.2 YAZAKI CShareDataに移動
		if( m_strText.size() < _MAX_PATH ){
			CSearchKeywordManager().AddToSearchKeyArr( m_strText.c_str() );
			m_pShareData->m_Common.m_sSearch.m_sSearchOption = m_sSearchOption;		// 検索オプション
		}
		CEditView*	pcEditView = (CEditView*)m_lParam;
		if( pcEditView->m_strCurSearchKey == m_strText && pcEditView->m_sCurSearchOption == m_sSearchOption ){
		}else{
			pcEditView->m_strCurSearchKey = m_strText;
			pcEditView->m_sCurSearchOption = m_sSearchOption;
			pcEditView->m_bCurSearchUpdate = true;
		}
		pcEditView->m_nCurSearchKeySequence = GetDllShareData().m_Common.m_sSearch.m_nSearchKeySequence;
		if( !m_bModal ){
			/* ダイアログデータの設定 */
			//SetData();
			SetCombosList();		//	コンボのみの初期化	2010/5/28 Uchi
		}
		return 1;
	}else{
		return 0;
	}
}
#ifdef UZ_FIX_FINDDLG
int CDlgFind::InstantInput( void )
{
//	MYTRACE( _T("CDlgFind::GetData()") );

	/* 英大文字と英小文字を区別する */
	m_sSearchOption.bLoHiCase = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_LOHICASE ));

	// 2001/06/23 Norio Nakatani
	/* 単語単位で検索 */
	m_sSearchOption.bWordOnly = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_WORD ));

	/* 一致する単語のみ検索する */
	/* 正規表現 */
	m_sSearchOption.bRegularExp = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ));

	/* 検索／置換  見つからないときメッセージを表示 */
	m_bNOTIFYNOTFOUND = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND );

	m_pShareData->m_Common.m_sSearch.m_bNOTIFYNOTFOUND = m_bNOTIFYNOTFOUND;	// 検索／置換  見つからないときメッセージを表示

	/* 検索文字列 */
	int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
	std::vector<TCHAR> vText(nBufferSize);
	::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize);
	m_strText = to_wchar(&vText[0]);

	/* 検索ダイアログを自動的に閉じる */
	m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_bAutoCloseDlgFind );

	/* 先頭（末尾）から再検索 2002.01.26 hor */
	m_pShareData->m_Common.m_sSearch.m_bSearchAll = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_SEARCHALL );

	CEditView *pcEditView = (CEditView*)m_lParam;

	if( 0 < m_strText.length() ){
		/* 正規表現？ */
		// From Here Jun. 26, 2001 genta
		//	正規表現ライブラリの差し替えに伴う処理の見直し
		int nFlag = 0x00;
		nFlag |= m_sSearchOption.bLoHiCase ? 0x01 : 0x00;
		if( m_sSearchOption.bRegularExp && !CheckRegexpSyntax( m_strText.c_str(), GetHwnd(), false, nFlag ) ){
			// 失敗!!
			pcEditView->m_bCurSrchKeyMark = false;	/* 検索文字列のマーク */
			pcEditView->Redraw();
#ifdef UZ_FIX_EDITVIEW_SCRBAR
			//pcEditView->SB_Marker_CallPaint(1502);
#endif  // UZ_
			return -1;
		} else {
			// nop
		}
		// To Here Jun. 26, 2001 genta 正規表現ライブラリ差し替え

		/* 検索文字列 */
		//@@@ 2002.2.2 YAZAKI CShareDataに移動
		if( m_strText.size() < _MAX_PATH ){
			// 入力中の検索は履歴に残さない	CSearchKeywordManager().AddToSearchKeyArr( m_strText.c_str() );
			m_pShareData->m_Common.m_sSearch.m_sSearchOption = m_sSearchOption;		// 検索オプション
		}
		if( pcEditView->m_strCurSearchKey == m_strText && pcEditView->m_sCurSearchOption == m_sSearchOption ){
		}else{
			pcEditView->m_strCurSearchKey = m_strText;
			pcEditView->m_sCurSearchOption = m_sSearchOption;
			pcEditView->m_bCurSearchUpdate = true;
		}
		pcEditView->m_nCurSearchKeySequence = GetDllShareData().m_Common.m_sSearch.m_nSearchKeySequence;
		//-pcEditView->ChangeCurRegexp(false);
		//-pcEditView->Redraw();
#ifdef UZ_FIX_EDITVIEW_SCRBAR
		//pcEditView->SB_Marker_CallPaint(1503);
#endif  // UZ_
		pcEditView->GetCommander().HandleCommand((EFunctionCode)(F_SEARCH_NEXT | FA_FROMKEYBOARD), true,
		                                         (LPARAM)GetHwnd(), 0, 0, 0);
		return 1;
	}else{
		pcEditView->m_bCurSrchKeyMark = false;	/* 検索文字列のマーク */
		pcEditView->Redraw();
#ifdef UZ_FIX_EDITVIEW_SCRBAR
		//pcEditView->SB_Marker_CallPaint(1504);
#endif  // UZ_
		SetStatus(0);
		return 0;
	}
}
#endif  // UZ_



BOOL CDlgFind::OnBnClicked( int wID )
{
	int			nRet;
	CEditView*	pcEditView = (CEditView*)m_lParam;
	switch( wID ){
	case IDC_BUTTON_HELP:
		/* 「検索」のヘルプ */
		//Stonee, 2001/03/12 第四引数を、機能番号からヘルプトピック番号を調べるようにした
		MyWinHelp( GetHwnd(), HELP_CONTEXT, ::FuncID_To_HelpContextID(F_SEARCH_DIALOG) );	//Apr. 5, 2001 JEPRO 修正漏れを追加	// 2006.10.10 ryoji MyWinHelpに変更に変更
		break;
	case IDC_CHK_REGULAREXP:	/* 正規表現 */
//		MYTRACE( _T("IDC_CHK_REGULAREXP ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ) = %d\n"), ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ) );
		if( ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ) ){

			// From Here Jun. 26, 2001 genta
			//	正規表現ライブラリの差し替えに伴う処理の見直し
			if( !CheckRegexpVersion( GetHwnd(), IDC_STATIC_JRE32VER, true ) ){
				::CheckDlgButton( GetHwnd(), IDC_CHK_REGULAREXP, 0 );
			}else{
			// To Here Jun. 26, 2001 genta

				/* 英大文字と英小文字を区別する */
				//	Jan. 31, 2002 genta
				//	大文字・小文字の区別は正規表現の設定に関わらず保存する
				//::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, 1 );
				//::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_LOHICASE ), FALSE );

				// 2001/06/23 Norio Nakatani
				/* 単語単位で検索 */
				::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), FALSE );
			}
		}else{
			/* 英大文字と英小文字を区別する */
			//::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_LOHICASE ), TRUE );
			//	Jan. 31, 2002 genta
			//	大文字・小文字の区別は正規表現の設定に関わらず保存する
			//::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, 0 );

			// 2001/06/23 Norio Nakatani
			/* 単語単位で検索 */
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), TRUE );
		}
		break;
#ifndef UZ_FIX_FINDDLG
	case IDC_BUTTON_SEARCHPREV:	/* 上検索 */	//Feb. 13, 2001 JEPRO ボタン名を[IDC_BUTTON1]→[IDC_BUTTON_SERACHPREV]に変更
		/* ダイアログデータの取得 */
		nRet = GetData();
		if( 0 < nRet ){
			if( m_bModal ){		/* モーダルダイアログか */
				CloseDialog( 1 );
			}else{
				/* 前を検索 */
				pcEditView->GetCommander().HandleCommand( F_SEARCH_PREV, true, (LPARAM)GetHwnd(), 0, 0, 0 );

				/* 再描画 2005.04.06 zenryaku 0文字幅マッチでキャレットを表示するため */
				pcEditView->Redraw();	// 前回0文字幅マッチの消去にも必要

				// 02/06/26 ai Start
				// 検索開始位置を登録
				if( FALSE != pcEditView->m_bSearch ){
					// 検索開始時のカーソル位置登録条件変更 02/07/28 ai start
					pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
					pcEditView->m_bSearch = FALSE;
					// 02/07/28 ai end
				}//  02/06/26 ai End

				/* 検索ダイアログを自動的に閉じる */
				if( m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind ){
					CloseDialog( 0 );
				}
			}
		}
		else if (nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGFIND1) );	// 検索条件を指定してください。
		}
		return TRUE;
#endif  // UZ_
	case IDC_BUTTON_SEARCHNEXT:		/* 下検索 */	//Feb. 13, 2001 JEPRO ボタン名を[IDOK]→[IDC_BUTTON_SERACHNEXT]に変更
		/* ダイアログデータの取得 */
		nRet = GetData();
		if( 0 < nRet ){
			if( m_bModal ){		/* モーダルダイアログか */
				CloseDialog( 2 );
			}
			else{
				/* 次を検索 */
				pcEditView->GetCommander().HandleCommand( F_SEARCH_NEXT, true, (LPARAM)GetHwnd(), 0, 0, 0 );

				/* 再描画 2005.04.06 zenryaku 0文字幅マッチでキャレットを表示するため */
				pcEditView->Redraw();	// 前回0文字幅マッチの消去にも必要

				// 検索開始位置を登録
				if( FALSE != pcEditView->m_bSearch ){
					// 検索開始時のカーソル位置登録条件変更 02/07/28 ai start
					pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
					pcEditView->m_bSearch = FALSE;
				}

				/* 検索ダイアログを自動的に閉じる */
				if( m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind ){
					CloseDialog( 0 );
				}
			}
		}
		else if (nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGFIND1) );	// 検索条件を指定してください。
		}
		return TRUE;
#ifndef UZ_FIX_FINDDLG
	case IDC_BUTTON_SETMARK:	//2002.01.16 hor 該当行マーク
		if( 0 < GetData() ){
			if( m_bModal ){		/* モーダルダイアログか */
				CloseDialog( 2 );
			}else{
				pcEditView->GetCommander().HandleCommand( F_BOOKMARK_PATTERN, false, 0, 0, 0, 0 );
				/* 検索ダイアログを自動的に閉じる */
				if( m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgFind ){
					CloseDialog( 0 );
				}
				else{
					::SendMessage(GetHwnd(),WM_NEXTDLGCTL,(WPARAM)::GetDlgItem(GetHwnd(),IDC_COMBO_TEXT ),TRUE);
				}
			}
		}
		return TRUE;
#endif  // UZ_
	case IDCANCEL:
		CloseDialog( 0 );
		return TRUE;
	}
	return FALSE;
}

BOOL CDlgFind::OnActivate( WPARAM wParam, LPARAM lParam )
{
	// 0文字幅マッチ描画のON/OFF	// 2009.11.29 ryoji
	CEditView*	pcEditView = (CEditView*)m_lParam;
	CLayoutRange cRangeSel = pcEditView->GetSelectionInfo().m_sSelect;
	if( cRangeSel.IsValid() && cRangeSel.IsLineOne() && cRangeSel.IsOne() )
		pcEditView->InvalidateRect(NULL);	// アクティブ化／非アクティブ化が完了してから再描画

	return CDialog::OnActivate(wParam, lParam);
}

//@@@ 2002.01.18 add start
LPVOID CDlgFind::GetHelpIdTable(void)
{
	return (LPVOID)p_helpids;
}
//@@@ 2002.01.18 add end


