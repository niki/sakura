/*!	@file
	@brief 置換ダイアログ

	@author Norio Nakatani
	@date 2001/06/23 N.Nakatani 単語単位で検索する機能を実装
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2001, genta, Stonee, hor, YAZAKI
	Copyright (C) 2002, MIK, hor, novice, genta, aroka, YAZAKI
	Copyright (C) 2006, かろと, ryoji
	Copyright (C) 2007, ryoji
	Copyright (C) 2009, ryoji
	Copyright (C) 2012, Uchi

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/
#include "StdAfx.h"
#include "dlg/CDlgReplace.h"
#include "view/CEditView.h"
#include "util/shell.h"
#include "util/window.h"
#include "sakura_rc.h"
#include "sakura.hh"
#ifdef NKMM_FIX_REPLACE_PREVIEW
#include "types/CTypeSupport.h"
#endif // NKMM_

//置換 CDlgReplace.cpp	//@@@ 2002.01.07 add start MIK
const DWORD p_helpids[] = {	//11900
	IDC_BUTTON_SEARCHNEXT,			HIDC_REP_BUTTON_SEARCHNEXT,			//下検索
	IDC_BUTTON_SEARCHPREV,			HIDC_REP_BUTTON_SEARCHPREV,			//上検索
	IDC_BUTTON_REPALCE,				HIDC_REP_BUTTON_REPALCE,			//置換
	IDC_BUTTON_REPALCEALL,			HIDC_REP_BUTTON_REPALCEALL,			//全置換
	IDCANCEL,						HIDCANCEL_REP,						//キャンセル
	IDC_BUTTON_HELP,				HIDC_REP_BUTTON_HELP,				//ヘルプ
	IDC_CHK_PASTE,					HIDC_REP_CHK_PASTE,					//クリップボードから貼り付け
	IDC_CHK_WORD,					HIDC_REP_CHK_WORD,					//単語単位
	IDC_CHK_LOHICASE,				HIDC_REP_CHK_LOHICASE,				//大文字小文字
	IDC_CHK_REGULAREXP,				HIDC_REP_CHK_REGULAREXP,			//正規表現
	IDC_CHECK_NOTIFYNOTFOUND,		HIDC_REP_CHECK_NOTIFYNOTFOUND,		//見つからないときに通知
	IDC_CHECK_bAutoCloseDlgReplace,	HIDC_REP_CHECK_bAutoCloseDlgReplace,	//自動的に閉じる
	IDC_COMBO_TEXT,					HIDC_REP_COMBO_TEXT,				//置換前
	IDC_COMBO_TEXT2,				HIDC_REP_COMBO_TEXT2,				//置換後
	IDC_RADIO_REPLACE,				HIDC_REP_RADIO_REPLACE,				//置換対象：置換
	IDC_RADIO_INSERT,				HIDC_REP_RADIO_INSERT,				//置換対象：挿入
	IDC_RADIO_ADD,					HIDC_REP_RADIO_ADD,					//置換対象：追加
	IDC_RADIO_LINEDELETE,			HIDC_REP_RADIO_LINEDELETE,			//置換対象：行削除
	IDC_RADIO_SELECTEDAREA,			HIDC_REP_RADIO_SELECTEDAREA,		//範囲：全体
	IDC_RADIO_ALLAREA,				HIDC_REP_RADIO_ALLAREA,				//範囲：選択範囲
	IDC_STATIC_JRE32VER,			HIDC_REP_STATIC_JRE32VER,			//正規表現バージョン
	IDC_BUTTON_SETMARK,				HIDC_REP_BUTTON_SETMARK,			//2002.01.16 hor 検索該当行をマーク
	IDC_CHECK_SEARCHALL,			HIDC_REP_CHECK_SEARCHALL,			//2002.01.26 hor 先頭（末尾）から再検索
	IDC_CHECK_CONSECUTIVEALL,		HIDC_REP_CHECK_CONSECUTIVEALL,		//「すべて置換」は置換の繰返し	// 2007.01.16 ryoji
//	IDC_STATIC,						-1,
	0, 0
};	//@@@ 2002.01.07 add end MIK

CDlgReplace::CDlgReplace()
{
	m_sSearchOption.Reset();	// 検索オプション
	m_bConsecutiveAll = FALSE;	// 「すべて置換」は置換の繰返し	// 2007.01.16 ryoji
	m_bSelectedArea = FALSE;	// 選択範囲内置換
	m_nReplaceTarget = 0;		// 置換対象		// 2001.12.03 hor
	m_nPaste = FALSE;			// 貼り付ける？	// 2001.12.03 hor
	m_nReplaceCnt = 0;			//すべて置換の実行結果		// 2002.02.08 hor
	m_bCanceled = false;		//すべて置換を中断したか	// 2002.02.08 hor
#ifdef NKMM_FIX_REPLACE_PREVIEW
	m_crSampleText = RGB(0,0,0);
	m_crSampleBack = RGB(255,255,255);
	m_hbrSampleBack = NULL;
	m_hFontSample = NULL;
	m_nSampleAfterHighlightPos = -1;
	m_nSampleAfterHighlightLen = 0;
#endif // NKMM_
	return;
}

/*!
	コンボボックスのドロップダウンメッセージを捕捉する

	@date 2013.03.24 novice 新規作成
*/
BOOL CDlgReplace::OnCbnDropDown( HWND hwndCtl, int wID )
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
	case IDC_COMBO_TEXT2:
		if ( ::SendMessage(hwndCtl, CB_GETCOUNT, 0L, 0L) == 0) {
			int nSize = m_pShareData->m_sSearchKeywords.m_aReplaceKeys.size();
			for (int i = 0; i < nSize; ++i) {
				Combo_AddString( hwndCtl, m_pShareData->m_sSearchKeywords.m_aReplaceKeys[i] );
			}
		}
		break;
	}
	return CDialog::OnCbnDropDown( hwndCtl, wID );
}

/* モードレスダイアログの表示 */
HWND CDlgReplace::DoModeless( HINSTANCE hInstance, HWND hwndParent, LPARAM lParam, BOOL bSelected )
{
	m_sSearchOption = m_pShareData->m_Common.m_sSearch.m_sSearchOption;		// 検索オプション
	m_bConsecutiveAll = m_pShareData->m_Common.m_sSearch.m_bConsecutiveAll;	// 「すべて置換」は置換の繰返し	// 2007.01.16 ryoji
	m_bSelectedArea = m_pShareData->m_Common.m_sSearch.m_bSelectedArea;		// 選択範囲内置換
	m_bNOTIFYNOTFOUND = m_pShareData->m_Common.m_sSearch.m_bNOTIFYNOTFOUND;	// 検索／置換  見つからないときメッセージを表示
	m_bSelected = bSelected;
	m_ptEscCaretPos_PHY = ((CEditView*)lParam)->GetCaret().GetCaretLogicPos();	// 検索/置換開始時のカーソル位置退避
	((CEditView*)lParam)->m_bSearch = TRUE;							// 検索/置換開始位置の登録有無			02/07/28 ai
	return CDialog::DoModeless( hInstance, hwndParent, IDD_REPLACE, lParam, SW_SHOW );
}

/* モードレス時：置換・検索対象となるビューの変更 */
void CDlgReplace::ChangeView( LPARAM pcEditView )
{
	m_lParam = pcEditView;
	return;
}




/* ダイアログデータの設定 */
void CDlgReplace::SetData( void )
{
	// 検索文字列/置換後文字列リストの設定(関数化)	2010/5/26 Uchi
	SetCombosList();

	/* 英大文字と英小文字を区別する */
	::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, m_sSearchOption.bLoHiCase );

	// 2001/06/23 N.Nakatani
	/* 単語単位で探す */
	::CheckDlgButton( GetHwnd(), IDC_CHK_WORD, m_sSearchOption.bWordOnly );

	/* 「すべて置換」は置換の繰返し */	// 2007.01.16 ryoji
	::CheckDlgButton( GetHwnd(), IDC_CHECK_CONSECUTIVEALL, m_bConsecutiveAll );

	// From Here Jun. 29, 2001 genta
	// 正規表現ライブラリの差し替えに伴う処理の見直し
	// 処理フロー及び判定条件の見直し。必ず正規表現のチェックと
	// 無関係にCheckRegexpVersionを通過するようにした。
	if( CheckRegexpVersion( GetHwnd(), IDC_STATIC_JRE32VER, false )
		&& m_sSearchOption.bRegularExp){
		/* 英大文字と英小文字を区別する */
		::CheckDlgButton( GetHwnd(), IDC_CHK_REGULAREXP, 1 );

		// 2001/06/23 N.Nakatani
		/* 単語単位で探す */
		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), FALSE );
	}
	else {
		::CheckDlgButton( GetHwnd(), IDC_CHK_REGULAREXP, 0 );

		/*「すべて置換」は置換の繰返し */
		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHECK_CONSECUTIVEALL ), FALSE );	// 2007.01.16 ryoji
	}
	// To Here Jun. 29, 2001 genta

	/* 検索／置換  見つからないときメッセージを表示 */
	::CheckDlgButton( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND, m_bNOTIFYNOTFOUND );


	/* 置換 ダイアログを自動的に閉じる */
	::CheckDlgButton( GetHwnd(), IDC_CHECK_bAutoCloseDlgReplace, m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgReplace );

	/* 先頭（末尾）から再検索 2002.01.26 hor */
	::CheckDlgButton( GetHwnd(), IDC_CHECK_SEARCHALL, m_pShareData->m_Common.m_sSearch.m_bSearchAll );

	// From Here 2001.12.03 hor
	// クリップボードから貼り付ける？
	::CheckDlgButton( GetHwnd(), IDC_CHK_PASTE, m_nPaste );
	// 置換対象
	if(m_nReplaceTarget==0){
		::CheckDlgButton( GetHwnd(), IDC_RADIO_REPLACE, TRUE );
	}else
	if(m_nReplaceTarget==1){
		::CheckDlgButton( GetHwnd(), IDC_RADIO_INSERT, TRUE );
	}else
	if(m_nReplaceTarget==2){
		::CheckDlgButton( GetHwnd(), IDC_RADIO_ADD, TRUE );
	}else
	if(m_nReplaceTarget==3){
		::CheckDlgButton( GetHwnd(), IDC_RADIO_LINEDELETE, TRUE );
		::EnableWindow( GetItemHwnd( IDC_COMBO_TEXT2 ), FALSE );
		::EnableWindow( GetItemHwnd( IDC_CHK_PASTE ), FALSE );
	}
	// To Here 2001.12.03 hor

#ifdef NKMM_FIX_DIALOG_POS
	RECT rcView;
	CEditView* pcEditView=(CEditView*)m_lParam;
	::GetWindowRect(pcEditView->GetHwnd(), &rcView);
	SetPlaceOfWindow(::GetParent(pcEditView->GetHwnd()), &rcView);
#endif // NKMM_

	return;
}



// 検索文字列/置換後文字列リストの設定
//	2010/5/26 Uchi
void CDlgReplace::SetCombosList( void )
{
	HWND	hwndCombo;

	/* 検索文字列 */
	hwndCombo = ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT );
	while (Combo_GetCount(hwndCombo) > 0) {
		Combo_DeleteString( hwndCombo, 0);
	}
	int nBufferSize = ::GetWindowTextLength( hwndCombo ) + 1;
	std::vector<TCHAR> vText;
	vText.resize( nBufferSize );
	Combo_GetText( hwndCombo, &vText[0], nBufferSize );
	if (auto_strcmp( to_wchar(&vText[0]), m_strText.c_str() ) != 0) {
		::DlgItem_SetText( GetHwnd(), IDC_COMBO_TEXT, m_strText.c_str() );
	}

	/* 置換後文字列 */
	hwndCombo = ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT2 );
	while (Combo_GetCount(hwndCombo) > 0) {
		Combo_DeleteString( hwndCombo, 0);
	}
	nBufferSize = ::GetWindowTextLength( hwndCombo ) + 1;
	vText.resize( nBufferSize );
	Combo_GetText( hwndCombo, &vText[0], nBufferSize );
	if (auto_strcmp( to_wchar(&vText[0]), m_strText2.c_str() ) != 0) {
		::DlgItem_SetText( GetHwnd(), IDC_COMBO_TEXT2, m_strText2.c_str() );
	}
}


/* ダイアログデータの取得 */
/* 0==条件未入力  0より大きい==正常   0より小さい==入力エラー */
int CDlgReplace::GetData( void )
{
	/* 英大文字と英小文字を区別する */
	m_sSearchOption.bLoHiCase = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_LOHICASE ));

	// 2001/06/23 N.Nakatani
	/* 単語単位で探す */
	m_sSearchOption.bWordOnly = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_WORD ));

	/* 「すべて置換」は置換の繰返し */	// 2007.01.16 ryoji
	m_bConsecutiveAll = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_CONSECUTIVEALL );

	/* 正規表現 */
	m_sSearchOption.bRegularExp = (0!=IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ));
	/* 選択範囲内置換 */
	m_bSelectedArea = ::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_SELECTEDAREA );
	/* 検索／置換  見つからないときメッセージを表示 */
	m_bNOTIFYNOTFOUND = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_NOTIFYNOTFOUND );

	m_pShareData->m_Common.m_sSearch.m_bConsecutiveAll = m_bConsecutiveAll;	// 1==「すべて置換」は置換の繰返し	// 2007.01.16 ryoji
	m_pShareData->m_Common.m_sSearch.m_bSelectedArea = m_bSelectedArea;		// 選択範囲内置換
	m_pShareData->m_Common.m_sSearch.m_bNOTIFYNOTFOUND = m_bNOTIFYNOTFOUND;	// 検索／置換  見つからないときメッセージを表示

	/* 検索文字列 */
	int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
	std::vector<TCHAR> vText(nBufferSize);
	::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize);
	m_strText = to_wchar(&vText[0]);
	/* 置換後文字列 */
	if( ::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_LINEDELETE ) ){
		m_strText2 = L"";
	}else{
		nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT2) ) + 1;
		vText.resize(nBufferSize);
		::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT2, &vText[0], nBufferSize);
		m_strText2 = to_wchar(&vText[0]);
	}

	/* 置換 ダイアログを自動的に閉じる */
	m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgReplace = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_bAutoCloseDlgReplace );

	/* 先頭（末尾）から再検索 2002.01.26 hor */
	m_pShareData->m_Common.m_sSearch.m_bSearchAll = ::IsDlgButtonChecked( GetHwnd(), IDC_CHECK_SEARCHALL );

	if( 0 < m_strText.size() ){
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
		//@@@ 2002.2.2 YAZAKI CShareData.AddToSearchKeyArr()追加に伴う変更
		if( m_strText.size() < _MAX_PATH ){
			CSearchKeywordManager().AddToSearchKeyArr( m_strText.c_str() );
			m_pShareData->m_Common.m_sSearch.m_sSearchOption = m_sSearchOption;		// 検索オプション
		}
		// 2011.12.18 viewに直接設定
		CEditView*	pcEditView = (CEditView*)m_lParam;
		if( pcEditView->m_strCurSearchKey == m_strText && pcEditView->m_sCurSearchOption == m_sSearchOption ){
		}else{
			pcEditView->m_strCurSearchKey = m_strText;
			pcEditView->m_sCurSearchOption = m_sSearchOption;
			pcEditView->m_bCurSearchUpdate = true;
		}
		pcEditView->m_nCurSearchKeySequence = GetDllShareData().m_Common.m_sSearch.m_nSearchKeySequence;

		/* 置換後文字列 */
		//@@@ 2002.2.2 YAZAKI CShareData.AddToReplaceKeyArr()追加に伴う変更
		if( m_strText2.size() < _MAX_PATH ){
			CSearchKeywordManager().AddToReplaceKeyArr( m_strText2.c_str() );
		}
		m_nReplaceKeySequence = GetDllShareData().m_Common.m_sSearch.m_nReplaceKeySequence;

		// From Here 2001.12.03 hor
		// クリップボードから貼り付ける？
		m_nPaste=IsDlgButtonChecked( GetHwnd(), IDC_CHK_PASTE );
		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT2 ), !m_nPaste );
		// 置換対象
		m_nReplaceTarget=0;
		if(::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_INSERT )){
			m_nReplaceTarget=1;
		}else
		if(::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_ADD )){
			m_nReplaceTarget=2;
		}else
		if(::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_LINEDELETE )){
			m_nReplaceTarget=3;
			m_nPaste = FALSE;
			::EnableWindow( GetItemHwnd( IDC_COMBO_TEXT2 ), FALSE );
		}
		
		// To Here 2001.12.03 hor

		// 検索文字列/置換後文字列リストの設定	2010/5/26 Uchi
		if (!m_bModal) {
			SetCombosList();
		}
		return 1;
	}else{
		return 0;
	}
}




BOOL CDlgReplace::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	_SetHwnd( hwndDlg );
	//	Jun. 26, 2001 genta
	//	この位置で正規表現の初期化をする必要はない
	//	他との一貫性を保つため削除

	/* ユーザーがコンボ ボックスのエディット コントロールに入力できるテキストの長さを制限する */
	//	Combo_LimitText( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT ), _MAX_PATH - 1 );
	//	Combo_LimitText( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT2 ), _MAX_PATH - 1 );

	/* コンボボックスのユーザー インターフェイスを拡張インターフェースにする */
	Combo_SetExtendedUI( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT ), TRUE );
	Combo_SetExtendedUI( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT2 ), TRUE );


	/* テキスト選択中か */
	if( m_bSelected ){
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_SEARCHPREV ), FALSE );	// 2001.12.03 hor コメント解除
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_SEARCHNEXT ), FALSE );	// 2001.12.03 hor コメント解除
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_REPALCE ), FALSE );		// 2001.12.03 hor コメント解除
		::CheckDlgButton( GetHwnd(), IDC_RADIO_SELECTEDAREA, TRUE );
//		::CheckDlgButton( GetHwnd(), IDC_RADIO_ALLAREA, FALSE );						// 2001.12.03 hor コメント
	}else{
//		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_RADIO_SELECTEDAREA ), FALSE );	// 2001.12.03 hor コメント
//		::CheckDlgButton( GetHwnd(), IDC_RADIO_SELECTEDAREA, FALSE );					// 2001.12.03 hor コメント
		::CheckDlgButton( GetHwnd(), IDC_RADIO_ALLAREA, TRUE );
	}

	m_comboDelText = SComboBoxItemDeleter();
	m_comboDelText.pRecent = &m_cRecentSearch;
	SetComboBoxDeleter(GetItemHwnd(IDC_COMBO_TEXT), &m_comboDelText);
	m_comboDelText2 = SComboBoxItemDeleter();
	m_comboDelText2.pRecent = &m_cRecentReplace;
	SetComboBoxDeleter(GetItemHwnd(IDC_COMBO_TEXT2), &m_comboDelText2);

	// フォント設定	2012/11/27 Uchi
	HFONT hFontOld = (HFONT)::SendMessageAny( GetItemHwnd( IDC_COMBO_TEXT ), WM_GETFONT, 0, 0 );
	HFONT hFont = SetMainFont( GetItemHwnd( IDC_COMBO_TEXT ) );
	m_cFontText.SetFont( hFontOld, hFont, GetItemHwnd( IDC_COMBO_TEXT ) );

	hFontOld = (HFONT)::SendMessageAny( GetItemHwnd( IDC_COMBO_TEXT2 ), WM_GETFONT, 0, 0 );
	hFont = SetMainFont( GetItemHwnd( IDC_COMBO_TEXT2 ) );
	m_cFontText2.SetFont( hFontOld, hFont, GetItemHwnd( IDC_COMBO_TEXT2 ) );

#ifdef NKMM_FIX_REPLACE_PREVIEW
	// サンプル欄をエディタの配色・設定フォント(通常テキスト)で表示する 20260826
	{
		CEditView* pcEditViewForColor = (CEditView*)m_lParam;
		CTypeSupport cTextType( pcEditViewForColor, COLORIDX_TEXT );
		m_crSampleText = cTextType.GetTextColor();
		m_crSampleBack = cTextType.GetBackColor();
		m_hbrSampleBack = ::CreateSolidBrush( m_crSampleBack );
		m_hFontSample = cTextType.GetTypeFont().m_hFont;
		if( NULL != m_hFontSample ){
			::SendMessageAny( GetItemHwnd(IDC_STATIC_REPLACESAMPLE_BEFORE), WM_SETFONT, (WPARAM)m_hFontSample, TRUE );
			::SendMessageAny( GetItemHwnd(IDC_STATIC_REPLACESAMPLE_AFTER), WM_SETFONT, (WPARAM)m_hFontSample, TRUE );
		}
	}
#endif // NKMM_

	/* 基底クラスメンバ */
	BOOL bRet = CDialog::OnInitDialog( hwndDlg, wParam, lParam );
#ifdef NKMM_FIX_REPLACE_PREVIEW
	UpdateSamplePreview();	// 20260826 初期状態のサンプル表示
#endif // NKMM_
	return bRet;

}




BOOL CDlgReplace::OnDestroy()
{
	m_cFontText.ReleaseOnDestroy();
	m_cFontText2.ReleaseOnDestroy();
#ifdef NKMM_FIX_REPLACE_PREVIEW
	if( NULL != m_hbrSampleBack ){
		::DeleteObject( m_hbrSampleBack );
		m_hbrSampleBack = NULL;
	}
#endif // NKMM_
	return CDialog::OnDestroy();
}

#ifdef NKMM_FIX_REPLACE_PREVIEW
/*!
	置換前サンプル欄(IDC_STATIC_REPLACESAMPLE_BEFORE)をエディタの配色
	(通常テキストの文字色・背景色)で描画するため、WM_CTLCOLORSTATICを
	横取りする。置換後サンプル欄(IDC_STATIC_REPLACESAMPLE_AFTER)は
	一致語句の強調表示のためオーナードロー化しており、WM_CTLCOLORSTATIC
	は発生しない(OnDrawItem参照)。

	@date 2026.08.26 新規作成
*/
INT_PTR CDlgReplace::DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
	if( WM_CTLCOLORSTATIC == wMsg ){
		HWND hwndCtl = (HWND)lParam;
		if( hwndCtl == GetItemHwnd( IDC_STATIC_REPLACESAMPLE_BEFORE ) ){
			HDC hdc = (HDC)wParam;
			::SetTextColor( hdc, m_crSampleText );
			::SetBkColor( hdc, m_crSampleBack );
			return (INT_PTR)m_hbrSampleBack;
		}
	}
	return CDialog::DispatchEvent( hWnd, wMsg, wParam, lParam );
}

/*!
	置換後サンプル欄(IDC_STATIC_REPLACESAMPLE_AFTER、オーナードロー)の
	描画。一致語句の置換結果(m_nSampleAfterHighlightPos/Len)だけ青字で
	強調表示する。

	@date 2026.08.26 新規作成
*/
BOOL CDlgReplace::OnDrawItem( WPARAM wParam, LPARAM lParam )
{
	LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
	if( (int)pdis->CtlID == IDC_STATIC_REPLACESAMPLE_AFTER ){
		HDC hdc = pdis->hDC;
		RECT rc = pdis->rcItem;

		::FillRect( hdc, &rc, m_hbrSampleBack );

		HFONT hFont = ( NULL != m_hFontSample ) ? m_hFontSample : (HFONT)::SendMessageAny( pdis->hwndItem, WM_GETFONT, 0, 0 );
		HFONT hFontOld = (HFONT)::SelectObject( hdc, hFont );
		int nOldBkMode = ::SetBkMode( hdc, TRANSPARENT );

		const std::wstring& str = m_strSampleAfter;
		int nPos = m_nSampleAfterHighlightPos;
		int nLen = m_nSampleAfterHighlightLen;
		bool bHighlight = ( 0 <= nPos && 0 < nLen && (size_t)(nPos + nLen) <= str.size() );

		int x = rc.left;
		SIZE sz;
		if( bHighlight ){
			if( 0 < nPos ){
				::SetTextColor( hdc, m_crSampleText );
				::GetTextExtentPoint32W( hdc, str.c_str(), nPos, &sz );
				::ExtTextOutW( hdc, x, rc.top, ETO_CLIPPED, &rc, str.c_str(), nPos, NULL );
				x += sz.cx;
			}
			::SetTextColor( hdc, RGB(0,0,255) );
			::GetTextExtentPoint32W( hdc, str.c_str() + nPos, nLen, &sz );
			::ExtTextOutW( hdc, x, rc.top, ETO_CLIPPED, &rc, str.c_str() + nPos, nLen, NULL );
			x += sz.cx;

			int nTailPos = nPos + nLen;
			int nTailLen = (int)str.size() - nTailPos;
			if( 0 < nTailLen ){
				::SetTextColor( hdc, m_crSampleText );
				::ExtTextOutW( hdc, x, rc.top, ETO_CLIPPED, &rc, str.c_str() + nTailPos, nTailLen, NULL );
			}
		}else if( !str.empty() ){
			::SetTextColor( hdc, m_crSampleText );
			::ExtTextOutW( hdc, x, rc.top, ETO_CLIPPED, &rc, str.c_str(), (int)str.size(), NULL );
		}

		::SetBkMode( hdc, nOldBkMode );
		::SelectObject( hdc, hFontOld );
		return TRUE;
	}
	return CDialog::OnDrawItem( wParam, lParam );
}
#endif // NKMM_



BOOL CDlgReplace::OnBnClicked( int wID )
{
	int			nRet;
	CEditView*	pcEditView = (CEditView*)m_lParam;

	switch( wID ){
	case IDC_CHK_PASTE:
		/* テキストの貼り付け */
		if( ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_PASTE ) &&
			!pcEditView->m_pcEditDoc->m_cDocEditor.IsEnablePaste() ){
			OkMessage( GetHwnd(), LS(STR_DLGREPLC_CLIPBOARD) );
			::CheckDlgButton( GetHwnd(), IDC_CHK_PASTE, FALSE );
		}
		::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_COMBO_TEXT2 ), !(::IsDlgButtonChecked( GetHwnd(), IDC_CHK_PASTE)) );
#ifdef NKMM_FIX_REPLACE_PREVIEW
		UpdateSamplePreview();
#endif // NKMM_
		return TRUE;
		// 置換対象
	case IDC_RADIO_REPLACE:
	case IDC_RADIO_INSERT:
	case IDC_RADIO_ADD:
	case IDC_RADIO_LINEDELETE:
		if( ::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_LINEDELETE ) ){
			::EnableWindow( GetItemHwnd( IDC_COMBO_TEXT2 ), FALSE );
			::EnableWindow( GetItemHwnd( IDC_CHK_PASTE ), FALSE );
		}else{
			::EnableWindow( GetItemHwnd( IDC_COMBO_TEXT2 ), TRUE );
			::EnableWindow( GetItemHwnd( IDC_CHK_PASTE ), TRUE );
		}
#ifdef NKMM_FIX_REPLACE_PREVIEW
		UpdateSamplePreview();
#endif // NKMM_
		return TRUE;
	case IDC_RADIO_SELECTEDAREA:
		/* 範囲範囲 */
		if( ::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_ALLAREA ) ){
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHPREV ), TRUE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHNEXT ), TRUE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_REPALCE ), TRUE );
		}else{
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHPREV ), FALSE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHNEXT ), FALSE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_REPALCE ), FALSE );
		}
		return TRUE;
	case IDC_RADIO_ALLAREA:
		/* ファイル全体 */
		if( ::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_ALLAREA ) ){
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHPREV ), TRUE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHNEXT ), TRUE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_REPALCE ), TRUE );
		}else{
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHPREV ), FALSE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_SEARCHNEXT ), FALSE );
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_BUTTON_REPALCE ), FALSE );
		}
		return TRUE;
// To Here 2001.12.03 hor
	case IDC_BUTTON_HELP:
		/* 「置換」のヘルプ */
		//Stonee, 2001/03/12 第四引数を、機能番号からヘルプトピック番号を調べるようにした
		MyWinHelp( GetHwnd(), HELP_CONTEXT, ::FuncID_To_HelpContextID(F_REPLACE_DIALOG) );	// 2006.10.10 ryoji MyWinHelpに変更に変更
		return TRUE;
#ifdef NKMM_FIX_REPLACE_PREVIEW
	case IDC_CHK_LOHICASE:	/* 大文字と小文字を区別する */
	case IDC_CHK_WORD:		/* 単語単位で探す */
		UpdateSamplePreview();
		return TRUE;
#endif // NKMM_
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

				// 2001/06/23 N.Nakatani
				/* 単語単位で探す */
				::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), FALSE );

				/*「すべて置換」は置換の繰返し */
				::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHECK_CONSECUTIVEALL ), TRUE );	// 2007.01.16 ryoji
			}
		}else{
			/* 英大文字と英小文字を区別する */
			//::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_LOHICASE ), TRUE );
			//	Jan. 31, 2002 genta
			//	大文字・小文字の区別は正規表現の設定に関わらず保存する
			//::CheckDlgButton( GetHwnd(), IDC_CHK_LOHICASE, 0 );

			// 2001/06/23 N.Nakatani
			/* 単語単位で探す */
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHK_WORD ), TRUE );

			/*「すべて置換」は置換の繰返し */
			::EnableWindow( ::GetDlgItem( GetHwnd(), IDC_CHECK_CONSECUTIVEALL ), FALSE );	// 2007.01.16 ryoji
		}
#ifdef NKMM_FIX_REPLACE_PREVIEW
		UpdateSamplePreview();
#endif // NKMM_
		return TRUE;
//	case IDOK:			/* 下検索 */
//		/* ダイアログデータの取得 */
//		nRet = GetData();
//		if( 0 < nRet ){
//			::EndDialog( hwndDlg, 2 );
//		}else
//		if( 0 == nRet ){
//			::EndDialog( hwndDlg, 0 );
//		}
//		return TRUE;


	case IDC_BUTTON_SEARCHPREV:	/* 上検索 */
		nRet = GetData();
		if( 0 < nRet ){

			// 検索開始位置を登録 02/07/28 ai start
			if( FALSE != pcEditView->m_bSearch ){
				pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
				pcEditView->m_bSearch = FALSE;
			}// 02/07/28 ai end

			/* コマンドコードによる処理振り分け */
			/* 前を検索 */
			pcEditView->GetCommander().HandleCommand( F_SEARCH_PREV, true, (LPARAM)GetHwnd(), 0, 0, 0 );
			/* 再描画（0文字幅マッチでキャレットを表示するため） */
			pcEditView->Redraw();	// 前回0文字幅マッチの消去にも必要
		}else if(nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGREPLC_STR) );
		}
		return TRUE;
	case IDC_BUTTON_SEARCHNEXT:	/* 下検索 */
		nRet = GetData();
		if( 0 < nRet ){

			// 検索開始位置を登録 02/07/28 ai start
			if( FALSE != pcEditView->m_bSearch ){
				pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
				pcEditView->m_bSearch = FALSE;
			}// 02/07/28 ai end

			/* コマンドコードによる処理振り分け */
			/* 次を検索 */
			pcEditView->GetCommander().HandleCommand( F_SEARCH_NEXT, true, (LPARAM)GetHwnd(), 0, 0, 0 );
			/* 再描画（0文字幅マッチでキャレットを表示するため） */
			pcEditView->Redraw();	// 前回0文字幅マッチの消去にも必要
		}else if(nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGREPLC_STR) );
		}
		return TRUE;

	case IDC_BUTTON_SETMARK:	//2002.01.16 hor 該当行マーク
		nRet = GetData();
		if( 0 < nRet ){
			pcEditView->GetCommander().HandleCommand( F_BOOKMARK_PATTERN, false, 0, 0, 0, 0 );
			::SendMessage(GetHwnd(),WM_NEXTDLGCTL,(WPARAM)::GetDlgItem(GetHwnd(),IDC_COMBO_TEXT ),TRUE);
		}
		return TRUE;

	case IDC_BUTTON_REPALCE:	/* 置換 */
		nRet = GetData();
		if( 0 < nRet ){

			// 置換開始位置を登録 02/07/28 ai start
			if( FALSE != pcEditView->m_bSearch ){
				pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
				pcEditView->m_bSearch = FALSE;
			}// 02/07/28 ai end

			/* 置換 */
			//@@@ 2002.2.2 YAZAKI 置換コマンドをCEditViewに新設
			//@@@ 2002/04/08 YAZAKI 親ウィンドウのハンドルを渡すように変更。
			pcEditView->GetCommander().HandleCommand( F_REPLACE, true, (LPARAM)GetHwnd(), 0, 0, 0 );
			/* 再描画 */
			pcEditView->GetCommander().HandleCommand( F_REDRAW, true, 0, 0, 0, 0 );
		}else if(nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGREPLC_STR) );
		}
		return TRUE;
	case IDC_BUTTON_REPALCEALL:	/* すべて置換 */
		nRet = GetData();
		if( 0 < nRet ){
			// 置換開始位置を登録 02/07/28 ai start
			if( FALSE != pcEditView->m_bSearch ){
				pcEditView->m_ptSrchStartPos_PHY = m_ptEscCaretPos_PHY;
				pcEditView->m_bSearch = FALSE;
			}// 02/07/28 ai end

			/* すべて行置換時の処置は「すべて置換」は置換の繰返しオプションOFFの場合にして削除 2007.01.16 ryoji */
			pcEditView->GetCommander().HandleCommand( F_REPLACE_ALL, true, 0, 0, 0, 0 );
			pcEditView->GetCommander().HandleCommand( F_REDRAW, true, 0, 0, 0, 0 );

			/* アクティブにする */
			ActivateFrameWindow( GetHwnd() );

			TopOkMessage( GetHwnd(), LS(STR_DLGREPLC_REPLACE), m_nReplaceCnt);

			if( !m_bCanceled ){
				if( m_bModal ){		/* モーダルダイアログか */
					/* 置換ダイアログを閉じる */
					::EndDialog( GetHwnd(), 0 );
				}else{
					/* 置換 ダイアログを自動的に閉じる */
					if( m_pShareData->m_Common.m_sSearch.m_bAutoCloseDlgReplace ){
						::DestroyWindow( GetHwnd() );
					}
				}
			}
			return TRUE;
		}else if(nRet == 0){
			OkMessage( GetHwnd(), LS(STR_DLGREPLC_REPSTR) );
		}
		return TRUE;
//	case IDCANCEL:
//		::EndDialog( hwndDlg, 0 );
//		return TRUE;
	}

	/* 基底クラスメンバ */
	return CDialog::OnBnClicked( wID );
}

BOOL CDlgReplace::OnActivate( WPARAM wParam, LPARAM lParam )
{
	// 0文字幅マッチ描画のON/OFF	// 2009.11.29 ryoji
	CEditView*	pcEditView = (CEditView*)m_lParam;
	CLayoutRange cRangeSel = pcEditView->GetSelectionInfo().m_sSelect;
	if( cRangeSel.IsValid() && cRangeSel.IsLineOne() && cRangeSel.IsOne() )
		pcEditView->InvalidateRect(NULL);	// アクティブ化／非アクティブ化が完了してから再描画

	return CDialog::OnActivate(wParam, lParam);
}

//@@@ 2002.01.18 add start
LPVOID CDlgReplace::GetHelpIdTable(void)
{
	return (LPVOID)p_helpids;
}
//@@@ 2002.01.18 add end

#ifdef NKMM_FIX_REPLACE_PREVIEW
namespace {
	//! サンプル欄に収まる程度の長さに切り詰める(位置情報が無い場合の保険)
	std::wstring ClipSampleText( const std::wstring& str )
	{
		const size_t nMaxLen = 200;
		if( str.size() <= nMaxLen ){
			return str;
		}
		return str.substr( 0, nMaxLen ) + L"...";
	}

	//! 一致箇所を中心に前後だけを切り出す(長い行でも一致部分が見切れないように)。
	//! 前後を省いた場合は"..."を付ける。切り出した文字列中の一致箇所の
	//! 位置をoutHighlightPosに返す(強調表示用。"..."の分だけ位置がずれる)。
	std::wstring MakeContextWindow( const std::wstring& str, int nMatchPos, int nMatchLen, int* outHighlightPos )
	{
		if( nMatchPos < 0 || nMatchLen < 0 || (size_t)(nMatchPos + nMatchLen) > str.size() ){
			*outHighlightPos = -1;
			return ClipSampleText( str );	// 位置情報が無効なら従来通りの単純な切り詰め
		}

		// 一致箇所の前後に残す文字数の目安。欄が1行で収まる程度に抑え、
		// 折り返しで変化箇所が分かりにくくならないようにする(欄自体は
		// 折り返さず、溢れた分は見切れさせる)。
		const int nRadius = 5;
		int nStart = nMatchPos - nRadius;
		if( nStart < 0 ){
			nStart = 0;
		}
		int nEnd = nMatchPos + nMatchLen + nRadius;
		if( (size_t)nEnd > str.size() ){
			nEnd = (int)str.size();
		}

		std::wstring strResult;
		if( 0 < nStart ){
			strResult += L"...";
		}
		*outHighlightPos = (int)strResult.size() + (nMatchPos - nStart);
		strResult += str.substr( nStart, nEnd - nStart );
		if( (size_t)nEnd < str.size() ){
			strResult += L"...";
		}
		return strResult;
	}

	//! 改行文字(\r\n・\r・\n)を目に見える記号(↵)に変換する。検索文字列/
	//! 置換後文字列のどちらから来た改行でもサンプル欄で見えるようにする
	//! (例: 検索側が"\r\n"にマッチする場合、置換側が"\r\n"を挿入する場合の
	//! 両方)。pPos1/pPos2に非負の位置(strの文字インデックス)を渡すと、
	//! 変換後の文字列での対応位置に書き換えて返す(強調表示範囲の追従用)。
	std::wstring VisualizeEol( const std::wstring& str, int* pPos1 = NULL, int* pPos2 = NULL )
	{
		std::wstring strResult;
		strResult.reserve( str.size() );
		size_t i = 0;
		while( i < str.size() ){
			if( pPos1 && 0 <= *pPos1 && (size_t)*pPos1 == i ){ *pPos1 = (int)strResult.size(); }
			if( pPos2 && 0 <= *pPos2 && (size_t)*pPos2 == i ){ *pPos2 = (int)strResult.size(); }
			if( str[i] == L'\r' && i + 1 < str.size() && str[i+1] == L'\n' ){
				strResult += L'↵';	// ↵
				i += 2;
			}else if( str[i] == L'\r' || str[i] == L'\n' ){
				strResult += L'↵';
				i += 1;
			}else{
				strResult += str[i];
				i += 1;
			}
		}
		if( pPos1 && 0 <= *pPos1 && (size_t)*pPos1 == str.size() ){ *pPos1 = (int)strResult.size(); }
		if( pPos2 && 0 <= *pPos2 && (size_t)*pPos2 == str.size() ){ *pPos2 = (int)strResult.size(); }
		return strResult;
	}
}

/*!
	コンボボックスの編集中テキストが変化したときに呼ばれる。
	置換前/置換後のテキストボックス入力に応じてサンプル欄をライブ更新する。

	@date 2026.08.26 新規作成
*/
BOOL CDlgReplace::OnCbnEditChange( HWND hwndCtl, int wID )
{
	switch( wID ){
	case IDC_COMBO_TEXT:
	case IDC_COMBO_TEXT2:
		UpdateSamplePreview();
		break;
	}
	return CDialog::OnCbnEditChange( hwndCtl, wID );
}

/*!
	置換サンプル欄(IDC_STATIC_REPLACESAMPLE_BEFORE/AFTER)を更新する。

	カーソルに近い一致箇所を1件だけ計算し、置換前欄には「一致した語句その
	もの」だけを表示する(何が一致したかの確認用。文脈は不要で、探さずに
	すぐ見えるようにする)。置換後欄には周辺の文脈込みで置換結果を表示する
	(何が起きるかの確認用。長い場合は一致箇所を中心に前後だけを切り出し、
	省いた部分は"..."で示す)。正規表現の$1/$&等も解決済みの実際の文字列で
	表示される。
	あわせて、CDlgFindのライブ入力プレビューと同じ手法でエディタの
	m_strCurSearchKey/m_sCurSearchOptionを更新し、一致箇所をスクロール
	バーにもマークする(検索文字列があれば全件、無ければマーク解除)。
	このコードベースではm_strCurSearchKey等は「最後に検索/プレビューした
	文字列」として扱われ上書き前提のため、副作用として問題ない。
	キー入力のたびに呼ばれるため、検索履歴への追加(CSearchKeywordManager)
	やダイアログの入力欄の書き換えはしない(GetData()は履歴登録等の副作用
	があるため使わない)。メッセージボックスも出さない。

	@date 2026.08.26 新規作成
*/
/*!
	置換後サンプル(オーナードロー)欄の内容を差し替え、再描画させる。
	ウィンドウテキストは読み上げ・コピー用にそのまま設定しておく
	(描画自体はOnDrawItemがm_strSampleAfter等のメンバを見て行う)。

	@date 2026.08.26 新規作成
*/
void CDlgReplace::SetSampleAfterText( const std::wstring& str, int nHighlightPos, int nHighlightLen )
{
	m_strSampleAfter = str;
	m_nSampleAfterHighlightPos = nHighlightPos;
	m_nSampleAfterHighlightLen = nHighlightLen;
	::DlgItem_SetText( GetHwnd(), IDC_STATIC_REPLACESAMPLE_AFTER, str.c_str() );
	::InvalidateRect( GetItemHwnd( IDC_STATIC_REPLACESAMPLE_AFTER ), NULL, TRUE );
}

void CDlgReplace::UpdateSamplePreview( void )
{
	CEditView* pcEditView = (CEditView*)m_lParam;
	if( NULL == pcEditView ){
		return;
	}

	/* 検索文字列(コントロールから直接読む。履歴登録等はしない) */
	int nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT) ) + 1;
	std::vector<TCHAR> vText(nBufferSize);
	::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT, &vText[0], nBufferSize );
	std::wstring strKey = to_wchar(&vText[0]);

	SSearchOption sOpt;
	sOpt.Reset();
	sOpt.bLoHiCase = (0 != ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_LOHICASE ));
	sOpt.bWordOnly = (0 != ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_WORD ));
	sOpt.bRegularExp = (0 != ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_REGULAREXP ));

	// スクロールバーに一致箇所をマークする(置換対象の設定に関わらず、検索
	// 文字列があれば常に反映する)。CDlgFind::SetData()の「最後に検索した
	// 文字列をマークする」処理と同じ手法(m_strCurSearchKey等はこの
	// コードベースでは「最後に検索/プレビューした文字列」として扱われ、
	// 常に上書きしてよいことになっている。検索ダイアログのライブ入力
	// プレビューも同様にこれを上書きする)。
	if( strKey.empty() ){
		pcEditView->m_bCurSrchKeyMark = false;
	}else{
		pcEditView->m_strCurSearchKey = strKey;
		pcEditView->m_sCurSearchOption = sOpt;
		pcEditView->m_bCurSearchUpdate = true;
		pcEditView->m_nCurSearchKeySequence = GetDllShareData().m_Common.m_sSearch.m_nSearchKeySequence;
		pcEditView->ChangeCurRegexp( false );
	}
	pcEditView->Redraw();
#ifdef NKMM_FIX_EDITVIEW_SCRBAR
	pcEditView->SB_Marker_Clear( 1710 );
#endif // NKMM_

	if( strKey.empty() ){
		::DlgItem_SetText( GetHwnd(), IDC_STATIC_REPLACESAMPLE_BEFORE, L"" );
		SetSampleAfterText( L"", -1, 0 );
		return;
	}

	/* 挿入/追加/行削除/クリップボード貼り付けは、位置依存・外部データ依存で
	   サンプルの事前計算が破綻しやすいため対象外とする。 */
	if( !::IsDlgButtonChecked( GetHwnd(), IDC_RADIO_REPLACE ) || ::IsDlgButtonChecked( GetHwnd(), IDC_CHK_PASTE ) ){
		::DlgItem_SetText( GetHwnd(), IDC_STATIC_REPLACESAMPLE_BEFORE, L"（この設定はサンプル表示に対応していません）" );
		SetSampleAfterText( L"", -1, 0 );
		return;
	}

	/* 置換後文字列 */
	nBufferSize = ::GetWindowTextLength( GetItemHwnd(IDC_COMBO_TEXT2) ) + 1;
	vText.resize(nBufferSize);
	::DlgItem_GetText( GetHwnd(), IDC_COMBO_TEXT2, &vText[0], nBufferSize );
	std::wstring strRep = to_wchar(&vText[0]);

	// カーソルに近い一致を優先して表示する(常に文書の先頭からだと、離れた
	// 場所の一致が表示されて分かりにくいため)。
	int nStartLine = (int)pcEditView->GetCaret().GetCaretLogicPos().y;

	std::wstring strBefore, strAfter;
	int nMatchPos, nMatchLenBefore, nMatchLenAfter;
	if( pcEditView->GetCommander().ComputeReplaceSample( strKey, strRep, sOpt, nStartLine, strBefore, strAfter, nMatchPos, nMatchLenBefore, nMatchLenAfter ) ){
		// 置換前は「何が一致したか」の確認なので、一致した語句そのものだけを
		// 1行目にそのまま表示する(文脈は不要。探さなくてもすぐ見えるように)。
		// 一致範囲が改行そのものにかかる場合(例:正規表現"\r\n")に備えて
		// 改行を可視化する。
		std::wstring strMatched = VisualizeEol( strBefore.substr( nMatchPos, nMatchLenBefore ) );
		::DlgItem_SetText( GetHwnd(), IDC_STATIC_REPLACESAMPLE_BEFORE, ClipSampleText(strMatched).c_str() );
		// 置換後は「結果がどうなるか」の確認なので、周辺の文脈込みで表示し、
		// 置換された語句の部分だけ青字で強調する。置換後文字列自体が改行を
		// 挿入する場合(例:置換後"あ\r\n")に備えて改行を可視化し、強調範囲の
		// 位置もずれないように追従させる。
		int nHighlightPos = -1;
		std::wstring strAfterWindow = MakeContextWindow( strAfter, nMatchPos, nMatchLenAfter, &nHighlightPos );
		int nHighlightEnd = ( 0 <= nHighlightPos ) ? nHighlightPos + nMatchLenAfter : -1;
		strAfterWindow = VisualizeEol( strAfterWindow, &nHighlightPos, &nHighlightEnd );
		int nHighlightLen = ( 0 <= nHighlightPos && 0 <= nHighlightEnd ) ? ( nHighlightEnd - nHighlightPos ) : 0;
		SetSampleAfterText( strAfterWindow, nHighlightPos, nHighlightLen );
	}else{
		::DlgItem_SetText( GetHwnd(), IDC_STATIC_REPLACESAMPLE_BEFORE, L"（一致なし）" );
		SetSampleAfterText( L"", -1, 0 );
	}
}
#endif // NKMM_


