/*!	@file
	@brief ファイルタイプ一覧ダイアログ

	@author Norio Nakatani
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000-2001, genta
	Copyright (C) 2001, Stonee
	Copyright (C) 2002, MIK
	Copyright (C) 2006, ryoji
	Copyright (C) 2010, Uchi, Beta.Ito, syat

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/
#include "StdAfx.h"
#include "types/CType.h" // use CDlgTypeList定義
#include "window/CEditWnd.h"
#include "typeprop/CDlgTypeList.h"
#include "typeprop/CImpExpManager.h"	// 2010/4/24 Uchi
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
#include "typeprop/CPropTypes.h"
#endif // NKMM_
#include "env/CShareData.h"
#include "env/CDocTypeManager.h"
#include "util/shell.h"
#include "util/window.h"
#include "util/RegKey.h"
#include "util/string_ex2.h"
#include <memory>
#include "sakura_rc.h"
#include "sakura.hh"

typedef std::basic_string<TCHAR> tstring;

#define BUFFER_SIZE 1024
#define ACTION_NAME	(_T("SakuraEditor"))
#define PROGID_BACKUP_NAME	(_T("SakuraEditorBackup"))
#define ACTION_BACKUP_PATH	(_T("\\ShellBackup"))

//関数プロトタイプ
int CopyRegistry(HKEY srcRoot, const tstring& srcPath, HKEY destRoot, const tstring& destPath);
int DeleteRegistry(HKEY root, const tstring& path);
int RegistExt(LPCTSTR sExt, bool bDefProg);
int UnregistExt(LPCTSTR sExt);
int CheckExt(LPCTSTR sExt, bool *pbRMenu, bool *pbDblClick);

//内部使用定数
static const int PROP_TEMPCHANGE_FLAG = 0x10000;

// タイプ別設定一覧 CDlgTypeList.cpp	//@@@ 2002.01.07 add start MIK
const DWORD p_helpids[] = {	//12700
	IDC_BUTTON_TEMPCHANGE,	HIDC_TL_BUTTON_TEMPCHANGE,	//一時適用
	IDOK,					HIDOK_TL,					//設定
	IDCANCEL,				HIDCANCEL_TL,				//キャンセル
	IDC_BUTTON_HELP,		HIDC_TL_BUTTON_HELP,		//ヘルプ
	IDC_LIST_TYPES,			HIDC_TL_LIST_TYPES,			//リスト
	IDC_BUTTON_IMPORT,		HIDC_TL_BUTTON_IMPORT,		//インポート
	IDC_BUTTON_EXPORT,		HIDC_TL_BUTTON_EXPORT,		//エクスポート
	IDC_BUTTON_INITIALIZE,	HIDC_TL_BUTTON_INIT,		//初期化
	IDC_BUTTON_COPY_TYPE,	HIDC_BUTTON_COPY_TYPE,		//複製
	IDC_BUTTON_UP_TYPE,		HIDC_BUTTON_UP_TYPE,		//↑
	IDC_BUTTON_DOWN_TYPE,	HIDC_BUTTON_DOWN_TYPE,		//↓
	IDC_BUTTON_ADD_TYPE,	HIDC_BUTTON_ADD_TYPE,		//追加
	IDC_BUTTON_DEL_TYPE,	HIDC_BUTTON_DEL_TYPE,		//削除
	IDC_CHECK_EXT_RMENU,	HIDC_TL_CHECK_RMENU,		//右クリックメニューに追加
	IDC_CHECK_EXT_DBLCLICK,	HIDC_TL_CHECK_DBLCLICK,		//ダブルクリックで開く
//	IDC_STATIC,				-1,
	0, 0
};	//@@@ 2002.01.07 add end MIK

/* モーダルダイアログの表示 */
int CDlgTypeList::DoModal( HINSTANCE hInstance, HWND hwndParent, SResult* psResult )
{
	int	nRet;
	m_nSettingType = psResult->cDocumentType;
	m_bAlertFileAssociation = true;
	m_bEnableTempChange = psResult->bTempChange;
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	m_pcPropTypes = NULL;
	m_hwndTab = NULL;
	m_nActiveTab = 0;
	for( int i = 0; i < 6; ++i ){
		m_hwndPropPage[i] = NULL;
	}
	m_nEmbeddedColorTypeIdx = -1;
	m_bColorPanelFinalized = false;
	for( int i = 0; i < MAX_TYPES; ++i ){
		m_pColorSnapshot[i] = NULL;
	}
#endif // NKMM_
	nRet = (int)CDialog::DoModal( hInstance, hwndParent, IDD_TYPELIST, (LPARAM)NULL );
	if( -1 == nRet ){
		return FALSE;
	}
	else{
		//結果
		psResult->cDocumentType = CTypeConfig(nRet & ~PROP_TEMPCHANGE_FLAG);
		psResult->bTempChange   = ((nRet & PROP_TEMPCHANGE_FLAG) != 0);
		return TRUE;
	}
}


BOOL CDlgTypeList::OnLbnDblclk( int wID )
{
	switch( wID ){
	case IDC_LIST_TYPES:
		//	Nov. 29, 2000	genta
		//	動作変更: 指定タイプの設定ダイアログ→一時的に別の設定を適用
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
		CommitEmbeddedColorPanel();
#endif // NKMM_
		::EndDialog(
			GetHwnd(),
			List_GetCurSel( GetDlgItem( GetHwnd(), IDC_LIST_TYPES ) )
			| PROP_TEMPCHANGE_FLAG
		);
		return TRUE;
	}
	return FALSE;
}

BOOL CDlgTypeList::OnBnClicked( int wID )
{
	switch( wID ){
	case IDC_BUTTON_HELP:
		/* 「タイプ別設定一覧」のヘルプ */
		//Stonee, 2001/03/12 第四引数を、機能番号からヘルプトピック番号を調べるようにした
		MyWinHelp( GetHwnd(), HELP_CONTEXT, ::FuncID_To_HelpContextID(F_TYPE_LIST) );	// 2006.10.10 ryoji MyWinHelpに変更に変更
		return TRUE;
	//	Nov. 29, 2000	From Here	genta
	//	適用する型の一時的変更
	case IDC_BUTTON_TEMPCHANGE:
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
		CommitEmbeddedColorPanel();
#endif // NKMM_
		::EndDialog(
			GetHwnd(),
 			List_GetCurSel( GetDlgItem( GetHwnd(), IDC_LIST_TYPES ) )
			| PROP_TEMPCHANGE_FLAG
		);
		return TRUE;
	//	Nov. 29, 2000	To Here
	case IDOK:
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
		CommitEmbeddedColorPanel();
#endif // NKMM_
		::EndDialog( GetHwnd(), List_GetCurSel( GetDlgItem( GetHwnd(), IDC_LIST_TYPES ) ) );
		return TRUE;
	case IDCANCEL:
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
		if( HasColorPanelChanges() ){
			// 「キャンセルで閉じる」こと自体が目的であって、変更を捨てることが
			// 目的ではないため、「破棄しますか」ではなく「保存しますか」で聞く
			int nRet = ::MYMESSAGEBOX( GetHwnd(), MB_YESNOCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
				_T("色/フォント/拡張子などの設定に変更があります。保存しますか？") );
			if( IDCANCEL == nRet ){
				return TRUE;	// 閉じない
			}
			if( IDNO == nRet ){
				// 保存しない: 編集前の値に戻す
				RestoreColorSnapshots();
			}
			// IDYESの場合はHasColorPanelChanges()内で既に反映済み
		}
#endif // NKMM_
		::EndDialog( GetHwnd(), -1 );
		return TRUE;
	case IDC_BUTTON_IMPORT:
		Import();
		return TRUE;
	case IDC_BUTTON_EXPORT:
		Export();
		return TRUE;
	case IDC_BUTTON_INITIALIZE:
		InitializeType();
		return TRUE;
	case IDC_BUTTON_COPY_TYPE:
		CopyType();
		return TRUE;
	case IDC_BUTTON_UP_TYPE:
		UpType();
		return TRUE;
	case IDC_BUTTON_DOWN_TYPE:
		DownType();
		return TRUE;
	case IDC_BUTTON_ADD_TYPE:
		AddType();
		return TRUE;
	case IDC_BUTTON_DEL_TYPE:
		DelType();
		return TRUE;
	}
	/* 基底クラスメンバ */
	return CDialog::OnBnClicked( wID );

}



BOOL CDlgTypeList::OnActivate( WPARAM wParam, LPARAM lParam )
{
	switch( LOWORD( wParam ) )
	{
	case WA_ACTIVE:
	case WA_CLICKACTIVE:
		SetData(-1);
		return TRUE;

	case WA_INACTIVE:
	default:
		break;
	}

	/* 基底クラスメンバ */
	return CDialog::OnActivate( wParam, lParam );
}

#ifdef NKMM_FIX_TYPELIST_ADD_ANY_TYPE
BOOL CDlgTypeList::OnDrawItem( WPARAM wParam, LPARAM lParam )
{
	DRAWITEMSTRUCT* lpdis = (DRAWITEMSTRUCT*)lParam;
	if( lpdis->CtlType == ODT_MENU ){
		m_cMenuDrawer.DrawItem( lpdis );
		return TRUE;
	}
	return CDialog::OnDrawItem( wParam, lParam );
}
#endif // NKMM_

#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
BOOL CDlgTypeList::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	BOOL bRet = CDialog::OnInitDialog( hwndDlg, wParam, lParam );
	CreateEmbeddedColorPanel();
	// 埋め込みパネル(タブ/各ページ)の生成過程でフォーカスが移動してしまうことが
	// あるため、一覧のリストボックスへ明示的にフォーカスを戻す。
	// 自分でSetFocusした場合はFALSEを返し、既定のフォーカス設定を抑止する
	::SetFocus( GetDlgItem( hwndDlg, IDC_LIST_TYPES ) );
	return FALSE;
}

BOOL CDlgTypeList::OnDestroy( void )
{
	// OK/一時適用/キャンセルのいずれかで既にCommit/Restoreされているはず。
	// 想定外の終了経路(強制終了等)のフォールバックとして、未確定なら保存しておく
	if( !m_bColorPanelFinalized ){
		CommitEmbeddedColorPanel();
	}
	for( int i = 0; i < MAX_TYPES; ++i ){
		if( NULL != m_pColorSnapshot[i] ){
			delete m_pColorSnapshot[i];
			m_pColorSnapshot[i] = NULL;
		}
	}
	if( NULL != m_pcPropTypes ){
		delete m_pcPropTypes;
		m_pcPropTypes = NULL;
	}
	return CDialog::OnDestroy();
}

/*! タイプ別設定の全タブ(スクリーン/カラー/ウィンドウ/支援/正規表現キーワード/
	キーワードヘルプ)をタブコントロールとして一覧の右側に埋め込み生成する。

	各ページはCPropTypesと同一サイズの派生クラス群であることを利用し、
	1つのCPropTypesインスタンスをページごとに該当サブクラスへ
	reinterpret_castして使い回す(m_Typesも共有されるため、あるページでの
	編集は他ページのCommit時にも反映される)。
	リソースの座標を直接編集せずに済むよう、埋め込み位置は既存コントロール
	(一覧・「設定変更(S)」ボタン)の実際の表示位置から実行時に計算する。
*/
void CDlgTypeList::CreateEmbeddedColorPanel()
{
	if( NULL != m_pcPropTypes ) return;

	HWND hwndList   = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	HWND hwndRefBtn = GetDlgItem( GetHwnd(), IDOK );	// 「設定変更(S)」ボタン(右端の目安)
	RECT rcList, rcBtn;
	::GetWindowRect( hwndList, &rcList );
	::GetWindowRect( hwndRefBtn, &rcBtn );
	::MapWindowPoints( NULL, GetHwnd(), (LPPOINT)&rcList, 2 );
	::MapWindowPoints( NULL, GetHwnd(), (LPPOINT)&rcBtn, 2 );

	// 一覧側とタブ側の間にセパレータ(縦の区切り線)を入れるための余白(ダイアログ単位)
	RECT rcSep = { 0, 0, 14, 0 };
	::MapDialogRect( GetHwnd(), &rcSep );
	const int nMargin = rcSep.right;
	int x = rcBtn.right + nMargin;
	int y = rcList.top;

	// 各ページ(IDD_PROP_XXX)の元サイズ(302x244 ダイアログ単位)をピクセルに変換
	RECT rcPage = { 0, 0, 302, 244 };
	::MapDialogRect( GetHwnd(), &rcPage );
	int cxPage = rcPage.right;
	int cyPage = rcPage.bottom;

	// タブ見出し分の余白(ダイアログ単位。十分大きめに確保しておく)
	RECT rcMargin = { 0, 0, 8, 24 };
	::MapDialogRect( GetHwnd(), &rcMargin );
	int cxTab = cxPage + rcMargin.right;
	int cyTab = cyPage + rcMargin.bottom;

	// 一覧側とタブ側の間に縦の区切り線を入れる(高さはタブ側と一覧側の高い方に合わせる)
	{
		int cySep = t_max( cyTab, (int)(rcList.bottom - rcList.top) );
		::CreateWindowEx( 0, _T("STATIC"), _T(""),
			WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
			rcBtn.right + nMargin / 2 - 1, y, 2, cySep,
			GetHwnd(), NULL, G_AppInstance(), NULL );
	}

	// タブコントロールを生成
	m_hwndTab = ::CreateWindowEx( 0, WC_TABCONTROL, _T(""),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		x, y, cxTab, cyTab,
		GetHwnd(), NULL, G_AppInstance(), NULL );
	// 実行時にCreateWindowExで作った子ウィンドウには、ダイアログ本体のフォントが
	// 自動適用されないため、明示的に設定する(既定のシステムフォントのままだと
	// タブ見出しの文字が大きく浮いて見える)
	{
		HFONT hDlgFont = (HFONT)::SendMessage( GetHwnd(), WM_GETFONT, 0, 0 );
		if( NULL != hDlgFont ){
			::SendMessage( m_hwndTab, WM_SETFONT, (WPARAM)hDlgFont, TRUE );
		}
	}

	// タブ項目を追加(CPropTypes.cppのTypePropSheetInfoListと同じ順序)
	// タブの行数(折り返し)がAdjustRectの結果に影響するため、必ず項目追加後に呼ぶこと
	static const int s_nResId[6]     = { IDD_PROP_SCREEN, IDD_PROP_COLOR, IDD_PROP_WINDOW, IDD_PROP_SUPPORT, IDD_PROP_REGEX, IDD_PROP_KEYHELP };
	static const int s_nTabNameId[6] = { STR_PROPTYPE_SCREEN, STR_PROPTYPE_COLOR, STR_PROPTYPE_WINDOW, STR_PROPTYPE_SUPPORT, STR_PROPTYPE_REGEX_KEYWORD, STR_PROPTYPE_KEYWORD_HELP };
	for( int i = 0; i < 6; ++i ){
		TCITEM item;
		memset_raw( &item, 0, sizeof_raw(item) );
		item.mask = TCIF_TEXT;
		TCHAR szTab[64];
		auto_strcpy( szTab, LS(s_nTabNameId[i]) );
		item.pszText = szTab;
		TabCtrl_InsertItem( m_hwndTab, i, &item );
	}

	// タブコントロールの実際の配置矩形から、コンテンツ表示領域(ページの置き場所)を求める
	// (fLarger=FALSEは「ウィンドウ矩形→表示領域」という一般的なAdjustRectの使い方)
	RECT rcDisplay = { x, y, x + cxTab, y + cyTab };
	TabCtrl_AdjustRect( m_hwndTab, FALSE, &rcDisplay );
	int xPage = rcDisplay.left;
	int yPage = rcDisplay.top;
	// タブ見出しの実際の高さは、指定した余白(24 DLU)と厳密には一致しないため、
	// ページを元サイズ(cxPage x cyPage)固定で配置すると表示領域との差分が
	// タブコントロール自身の背景色として隙間に見えてしまう。表示領域ぴったりの
	// サイズに合わせて埋める
	int cxPageActual = rcDisplay.right  - rcDisplay.left;
	int cyPageActual = rcDisplay.bottom - rcDisplay.top;

	m_pcPropTypes = new CPropTypes();
	m_pcPropTypes->Create( G_AppInstance(), GetHwnd() );

	int selIdx = List_GetCurSel( hwndList );
	if( selIdx < 0 ) selIdx = 0;
	CaptureColorSnapshot( selIdx );
	STypeConfig type;
	CDocTypeManager().GetTypeConfig( CTypeConfig(selIdx), type );
	m_pcPropTypes->SetTypeData( type );

	m_hwndPropPage[0] = ((CPropTypesScreen*) m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );
	m_hwndPropPage[1] = ((CPropTypesColor*)  m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );
	m_hwndPropPage[2] = ((CPropTypesWindow*) m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );
	m_hwndPropPage[3] = ((CPropTypesSupport*)m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );
	m_hwndPropPage[4] = ((CPropTypesRegex*)  m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );
	m_hwndPropPage[5] = ((CPropTypesKeyHelp*)m_pcPropTypes)->CreateEmbeddedPage( GetHwnd(), xPage, yPage, cxPageActual, cyPageActual );

	// 生成直後は全ページ非表示のはずだが、初回表示のタブとページのズレを防ぐため
	// 念のため明示的に隠してから、アクティブにするページだけを最前面に表示する
	for( int i = 0; i < 6; ++i ){
		if( NULL != m_hwndPropPage[i] ){
			::ShowWindow( m_hwndPropPage[i], SW_HIDE );
		}
	}
	m_nActiveTab = 0;
	TabCtrl_SetCurSel( m_hwndTab, 0 );
	if( NULL != m_hwndPropPage[0] ){
		::SetWindowPos( m_hwndPropPage[0], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	}

	m_nEmbeddedColorTypeIdx = selIdx;

	// リソース上のダイアログサイズ(.rcで大きめに確保している)は実際に必要な
	// サイズより余白が大きくなりがちなため、一覧側・タブ側それぞれの実際の
	// 表示範囲に合わせてダイアログ全体を詰める
	{
		HWND hwndHelpBtn = GetDlgItem( GetHwnd(), IDC_BUTTON_HELP );
		RECT rcHelp;
		::GetWindowRect( hwndHelpBtn, &rcHelp );
		::MapWindowPoints( NULL, GetHwnd(), (LPPOINT)&rcHelp, 2 );

		RECT rcSmallMargin = { 0, 0, 7, 7 };
		::MapDialogRect( GetHwnd(), &rcSmallMargin );

		int cxClientNeeded = t_max( x + cxTab, (int)rcHelp.right )  + rcSmallMargin.right;
		int cyClientNeeded = t_max( y + cyTab, (int)rcHelp.bottom ) + rcSmallMargin.bottom;

		RECT rcWindowNow, rcClientNow;
		::GetWindowRect( GetHwnd(), &rcWindowNow );
		::GetClientRect( GetHwnd(), &rcClientNow );
		int cxFrame = (rcWindowNow.right - rcWindowNow.left) - rcClientNow.right;
		int cyFrame = (rcWindowNow.bottom - rcWindowNow.top) - rcClientNow.bottom;

		::SetWindowPos( GetHwnd(), NULL, 0, 0,
			cxClientNeeded + cxFrame, cyClientNeeded + cyFrame,
			SWP_NOMOVE | SWP_NOZORDER );
	}
}

/*! タブ切り替え。現在表示中のページを隠し、選択されたタブのページを表示する */
void CDlgTypeList::OnTabSelChange()
{
	if( NULL == m_hwndTab ) return;
	int nNewTab = TabCtrl_GetCurSel( m_hwndTab );
	if( nNewTab == m_nActiveTab ) return;
	if( 0 <= m_nActiveTab && m_nActiveTab < 6 && NULL != m_hwndPropPage[m_nActiveTab] ){
		::ShowWindow( m_hwndPropPage[m_nActiveTab], SW_HIDE );
	}
	m_nActiveTab = nNewTab;
	if( 0 <= m_nActiveTab && m_nActiveTab < 6 && NULL != m_hwndPropPage[m_nActiveTab] ){
		::SetWindowPos( m_hwndPropPage[m_nActiveTab], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	}
}

/*! パネルが現在表示しているタイプの内容を保存する(全ページ分をまとめて) */
void CDlgTypeList::CommitEmbeddedColorPanel()
{
	if( NULL == m_pcPropTypes || m_nEmbeddedColorTypeIdx < 0 ) return;
	STypeConfig type;
	// m_Typesは全ページ共有の実体なので、順にGetDataを反映させてから最後に取得すればよい
	if( NULL != m_hwndPropPage[0] ) ((CPropTypesScreen*) m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[0] );
	if( NULL != m_hwndPropPage[1] ) ((CPropTypesColor*)  m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[1] );
	if( NULL != m_hwndPropPage[2] ) ((CPropTypesWindow*) m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[2] );
	if( NULL != m_hwndPropPage[3] ) ((CPropTypesSupport*)m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[3] );
	if( NULL != m_hwndPropPage[4] ) ((CPropTypesRegex*)  m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[4] );
	if( NULL != m_hwndPropPage[5] ) ((CPropTypesKeyHelp*)m_pcPropTypes)->CommitEmbeddedPage( type, m_hwndPropPage[5] );
	CDocTypeManager().SetTypeConfig( CTypeConfig(m_nEmbeddedColorTypeIdx), type );
	m_bColorPanelFinalized = true;
}

/*! 指定タイプの設定を全ページに表示する(保存はしない) */
void CDlgTypeList::RefreshEmbeddedColorPanel( int nIdx )
{
	if( NULL == m_pcPropTypes ) return;
	if( nIdx < 0 || GetDllShareData().m_nTypesCount <= nIdx ){
		m_nEmbeddedColorTypeIdx = -1;
		return;
	}
	CaptureColorSnapshot( nIdx );
	STypeConfig type;
	CDocTypeManager().GetTypeConfig( CTypeConfig(nIdx), type );
	if( NULL != m_hwndPropPage[0] ) ((CPropTypesScreen*) m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[0] );
	if( NULL != m_hwndPropPage[1] ) ((CPropTypesColor*)  m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[1] );
	if( NULL != m_hwndPropPage[2] ) ((CPropTypesWindow*) m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[2] );
	if( NULL != m_hwndPropPage[3] ) ((CPropTypesSupport*)m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[3] );
	if( NULL != m_hwndPropPage[4] ) ((CPropTypesRegex*)  m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[4] );
	if( NULL != m_hwndPropPage[5] ) ((CPropTypesKeyHelp*)m_pcPropTypes)->RefreshEmbeddedPage( type, m_hwndPropPage[5] );
	m_nEmbeddedColorTypeIdx = nIdx;
}

/*! 表示タイプを切り替える(現在の表示中タイプを保存してから切り替える) */
void CDlgTypeList::SwitchEmbeddedColorPanel( int nNewIdx )
{
	if( NULL == m_pcPropTypes ) return;
	if( m_nEmbeddedColorTypeIdx == nNewIdx ) return;
	CommitEmbeddedColorPanel();
	RefreshEmbeddedColorPanel( nNewIdx );
}

/*! 初回表示時、キャンセル用に編集前の値を保存しておく(2回目以降は何もしない) */
void CDlgTypeList::CaptureColorSnapshot( int nIdx )
{
	if( nIdx < 0 || MAX_TYPES <= nIdx ) return;
	if( NULL != m_pColorSnapshot[nIdx] ) return;	// 取得済み
	m_pColorSnapshot[nIdx] = new STypeConfig();
	CDocTypeManager().GetTypeConfig( CTypeConfig(nIdx), *m_pColorSnapshot[nIdx], false );
}

/*! キャンセル時、埋め込みパネルで表示/編集した全タイプを編集前の値に戻す */
void CDlgTypeList::RestoreColorSnapshots()
{
	for( int i = 0; i < MAX_TYPES; ++i ){
		if( NULL == m_pColorSnapshot[i] ) continue;
		if( i < GetDllShareData().m_nTypesCount ){
			CDocTypeManager().SetTypeConfig( CTypeConfig(i), *m_pColorSnapshot[i] );
		}
	}
	m_bColorPanelFinalized = true;
}

/*! 編集前のスナップショットと比較し、実際に値が変化したタイプがあるかどうかを判定する。
	表示中タイプの未反映分はここで確定させたうえで比較する(比較後は確定済みのままでよい:
	変更なしならスナップショットと同じ値が再度書き込まれるだけ、変更ありなら呼び出し側が
	RestoreColorSnapshots()で元に戻す)。

	以下は編集の有無に関わらずコミットのたびに値が変わり得るため、比較対象から除外する:
	- m_ColorInfoArr/m_nColorInfoArrNum: 基本設定に追従して常に上書きされる
	  (CDocTypeManager::GetTypeConfigのマージ処理を参照)
	- m_nRegexKeyMagicNumber: 正規表現キーワードタブのGetDataが、コミットの都度
	  無条件に採番し直す更新用マジックナンバー(CPropTypesRegex::GetData参照)。
	  設定内容そのものではないため比較材料にならない
	- m_nImeState: 「起動時のIME」コンボの選択位置から再構成される値のため、
	  テーブルに一致しない値(出荷時の基本設定など)を経由すると、編集していなくても
	  正規化されて値が変わることがある(CPropTypesWindow::SetData/GetData参照) */
bool CDlgTypeList::HasColorPanelChanges()
{
	CommitEmbeddedColorPanel();

	for( int i = 0; i < MAX_TYPES; ++i ){
		if( NULL == m_pColorSnapshot[i] ) continue;
		STypeConfig cur;
		if( !CDocTypeManager().GetTypeConfig( CTypeConfig(i), cur, false ) ) continue;
		STypeConfig snap = *m_pColorSnapshot[i];
		// m_ColorInfoArrのうち色(m_sColorAttr)・名前・インデックスは基本設定に追従して
		// 常に上書きされるノイズだが、表示(m_bDisp)・太字・下線(m_sFontAttr)は編集対象の
		// 実データなので、これらだけ退避してから配列全体をゼロクリアし、戻す。20260801
		// (以前はm_ColorInfoArr全体を比較対象から除外していたため、半角数値などの表示
		//  チェックボックスの変更がキャンセル時の「変更あり」判定に反映されず、保存確認
		//  なしに変更が破棄されてしまうバグがあった)
		bool bDispCur[COLORIDX_LAST], bDispSnap[COLORIDX_LAST];
		SFontAttr sFontAttrCur[COLORIDX_LAST], sFontAttrSnap[COLORIDX_LAST];
		for( int c = 0; c < COLORIDX_LAST; ++c ){
			bDispCur[c]  = cur.m_ColorInfoArr[c].m_bDisp;
			bDispSnap[c] = snap.m_ColorInfoArr[c].m_bDisp;
			sFontAttrCur[c]  = cur.m_ColorInfoArr[c].m_sFontAttr;
			sFontAttrSnap[c] = snap.m_ColorInfoArr[c].m_sFontAttr;
		}
		memset_raw( cur.m_ColorInfoArr, 0, sizeof_raw(cur.m_ColorInfoArr) );
		memset_raw( snap.m_ColorInfoArr, 0, sizeof_raw(snap.m_ColorInfoArr) );
		for( int c = 0; c < COLORIDX_LAST; ++c ){
			cur.m_ColorInfoArr[c].m_bDisp  = bDispCur[c];
			snap.m_ColorInfoArr[c].m_bDisp = bDispSnap[c];
			cur.m_ColorInfoArr[c].m_sFontAttr  = sFontAttrCur[c];
			snap.m_ColorInfoArr[c].m_sFontAttr = sFontAttrSnap[c];
		}
		cur.m_nColorInfoArrNum = 0;
		snap.m_nColorInfoArrNum = 0;
		cur.m_nRegexKeyMagicNumber = 0;
		snap.m_nRegexKeyMagicNumber = 0;
		cur.m_nImeState = 0;
		snap.m_nImeState = 0;
		if( 0 != memcmp( &cur, &snap, sizeof(STypeConfig) ) ){
			return true;
		}
	}
	return false;
}

/*! 並べ替え/削除等でインデックスの対応が崩れる前に、そのスナップショットを
	復元対象から外す(その時点までの内容を確定させる) */
void CDlgTypeList::DiscardColorSnapshot( int nIdx )
{
	if( nIdx < 0 || MAX_TYPES <= nIdx ) return;
	if( NULL != m_pColorSnapshot[nIdx] ){
		delete m_pColorSnapshot[nIdx];
		m_pColorSnapshot[nIdx] = NULL;
	}
}
#endif // NKMM_


INT_PTR CDlgTypeList::DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{


	HWND hwndRMenu = GetDlgItem( GetHwnd(), IDC_CHECK_EXT_RMENU );
	HWND hwndDblClick = GetDlgItem( GetHwnd(), IDC_CHECK_EXT_DBLCLICK );

	INT_PTR result;
	result = CDialog::DispatchEvent( hWnd, wMsg, wParam, lParam );
	switch( wMsg ){
#ifdef NKMM_FIX_TYPELIST_ADD_ANY_TYPE
	case WM_NOTIFY:
		// 任意のタイプ設定を追加する
		if (((LPNMHDR)lParam)->code ==  BCN_DROPDOWN) {
			HWND hwndBtn = GetDlgItem(GetHwnd(), IDC_BUTTON_ADD_TYPE);
			// オーナー描画メニューのWM_MEASUREITEM/WM_DRAWITEMはTrackPopupMenuの
			// オーナーウィンドウ(=このダイアログ自身)に送られるため、m_cMenuDrawerも
			// このダイアログのDispatchEventからアクセスできるメンバにする必要がある
			m_cMenuDrawer.Create(G_AppInstance(), GetHwnd(), NULL);
			m_cMenuDrawer.ResetContents();
			HMENU hMenuPopUp = ::CreatePopupMenu();
			std::vector<std::tstring> names;
			CShareData::getInstance()->GetTypeNames(names);
			int nIdx = 0;
			for (auto v : names) {
				int nFlag = MF_BYPOSITION | MF_STRING;
				m_cMenuDrawer.MyAppendMenu(hMenuPopUp, nFlag, nIdx + 1, v.c_str(), _T(""), FALSE);
				nIdx++;
			}
			RECT rc;
			::GetClientRect(hwndBtn, &rc);
			POINT po = {rc.left, rc.bottom};
			::ClientToScreen(hwndBtn, &po);
			int nId = (int)::TrackPopupMenu(hMenuPopUp,
				TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON,
				po.x, po.y,
				0,
				GetHwnd(),
				NULL
			);
			::DestroyMenu( hMenuPopUp );
			// タイプIDを指定して追加
			if (nId > 0) {
				int nNewTypeIndex = GetDllShareData().m_nTypesCount;
				if (!CDocTypeManager().AddTypeConfig(CTypeConfig(nNewTypeIndex), (uint32_t)nId)) {
					break;
				}
				SetData(nNewTypeIndex);
			}
		}
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
		else if( ((LPNMHDR)lParam)->code == TCN_SELCHANGE && ((LPNMHDR)lParam)->hwndFrom == m_hwndTab ){
			OnTabSelChange();
		}
#endif // NKMM_
		break;
	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT* lpmis = (MEASUREITEMSTRUCT*)lParam;
			if( lpmis->CtlType == ODT_MENU ){
				int nItemHeight = 0;
				int nItemWidth = m_cMenuDrawer.MeasureItem( (int)lpmis->itemID, &nItemHeight );
				if( 0 < nItemWidth ){
					lpmis->itemWidth = nItemWidth;
					lpmis->itemHeight = nItemHeight;
				}
				return TRUE;
			}
		}
		break;
#endif // NKMM_
	case WM_COMMAND:
		{
		HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
		int nIdx = List_GetCurSel( hwndList );
		const STypeConfigMini* type = NULL;
		CDocTypeManager().GetTypeConfigMini(CTypeConfig(nIdx), &type);
		if( LOWORD(wParam) == IDC_LIST_TYPES )
		{
			switch( HIWORD(wParam) )
			{
			case LBN_SELCHANGE:
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
				SwitchEmbeddedColorPanel( nIdx );
#endif // NKMM_
				DlgItem_Enable( GetHwnd(), IDC_BUTTON_UP_TYPE, 1 < nIdx );
				DlgItem_Enable( GetHwnd(), IDC_BUTTON_DOWN_TYPE, nIdx != 0 && nIdx < GetDllShareData().m_nTypesCount - 1 );
				DlgItem_Enable( GetHwnd(), IDC_BUTTON_DEL_TYPE, nIdx != 0 );
				if( type->m_szTypeExts[0] == '\0' ){
					::EnableWindow( hwndRMenu, FALSE );
					::EnableWindow( hwndDblClick, FALSE );
				}else{
					::EnableWindow( GetDlgItem( GetHwnd(), IDC_CHECK_EXT_RMENU ), TRUE );
					if( !m_bRegistryChecked[ nIdx ] ){
						TCHAR exts[_countof(type->m_szTypeExts)] = {0};
						_tcscpy( exts, type->m_szTypeExts );
						TCHAR *ext = _tcstok( exts, CDocTypeManager::m_typeExtSeps );

						m_bExtRMenu[ nIdx ] = true;
						m_bExtDblClick[ nIdx ] = true;
						while( NULL != ext ){
							if (_tcspbrk(ext, CDocTypeManager::m_typeExtWildcards) == NULL) {
								bool bRMenu;
								bool bDblClick;
								CheckExt( ext, &bRMenu, &bDblClick );
								m_bExtRMenu[ nIdx ] &= bRMenu;
								m_bExtDblClick[ nIdx ] &= bDblClick;
							}
							ext = _tcstok( NULL, CDocTypeManager::m_typeExtSeps );
						}
						m_bRegistryChecked[ nIdx ] = true;
					}
					BtnCtl_SetCheck( hwndRMenu, m_bExtRMenu[ nIdx ] );
					::EnableWindow( hwndDblClick, m_bExtRMenu[ nIdx ] );
					BtnCtl_SetCheck( hwndDblClick, m_bExtDblClick[ nIdx ] );
				}
				return TRUE;
			}
		}
		else if( LOWORD(wParam) == IDC_CHECK_EXT_RMENU && HIWORD(wParam) == BN_CLICKED )
		{
			bool checked = ( BtnCtl_GetCheck( hwndRMenu ) != FALSE ? true : false );
			if( ! AlertFileAssociation() ){		//レジストリ変更確認
				BtnCtl_SetCheck( hwndRMenu, !checked );
				break;
			}
			TCHAR exts[_countof(type->m_szTypeExts)] = {0};
			_tcscpy( exts, type->m_szTypeExts );
			TCHAR *ext = _tcstok( exts, CDocTypeManager::m_typeExtSeps );
			int nRet;
			while( NULL != ext ){
				if (_tcspbrk(ext, CDocTypeManager::m_typeExtWildcards) == NULL) {
					if( checked ){	//「右クリック」チェックON
						if( (nRet = RegistExt( ext, true )) != 0 )
						{
							TCHAR buf[BUFFER_SIZE] = {0};
							::FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, nRet, 0, buf, _countof(buf), NULL );
							::MessageBox( GetHwnd(), (tstring(LS(STR_DLGTYPELIST_ERR1)) + buf).c_str(), GSTR_APPNAME, MB_OK );
							break;
						}
					}else{			//「右クリック」チェックOFF
						if( (nRet = UnregistExt( ext )) != 0 )
						{
							TCHAR buf[BUFFER_SIZE] = {0};
							::FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, nRet, 0, buf, _countof(buf), NULL );
							::MessageBox( GetHwnd(), (tstring(LS(STR_DLGTYPELIST_ERR2)) + buf).c_str(), GSTR_APPNAME, MB_OK );
							break;
						}
					}
				}
				ext = _tcstok( NULL, CDocTypeManager::m_typeExtSeps );
			}
			m_bExtRMenu[nIdx] = checked;
			::EnableWindow(hwndDblClick, checked);
			m_bExtDblClick[nIdx] = checked;
			BtnCtl_SetCheck(hwndDblClick, checked);
			return TRUE;
		}
		else if( LOWORD(wParam) == IDC_CHECK_EXT_DBLCLICK && HIWORD(wParam) == BN_CLICKED )
		{
			bool checked = ( BtnCtl_GetCheck( hwndDblClick ) != FALSE ? true : false );
			if( ! AlertFileAssociation() ){		//レジストリ変更確認
				BtnCtl_SetCheck( hwndDblClick, !checked );
				break;
			}
			TCHAR exts[_countof(type->m_szTypeExts)] = {0};
			_tcscpy( exts, type->m_szTypeExts );
			TCHAR *ext = _tcstok( exts, CDocTypeManager::m_typeExtSeps );
			int nRet;
			while( NULL != ext ){
				if (_tcspbrk(ext, CDocTypeManager::m_typeExtWildcards) == NULL) {
					if( (nRet = RegistExt( ext, checked )) != 0 )
					{
						TCHAR buf[BUFFER_SIZE] = {0};
						::FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, nRet, 0, buf, _countof(buf), NULL );
						::MessageBox( GetHwnd(), (tstring(LS(STR_DLGTYPELIST_ERR1)) + buf).c_str(), GSTR_APPNAME, MB_OK );
						break;
					}
				}
				ext = _tcstok( NULL, CDocTypeManager::m_typeExtSeps );
			}
			m_bExtDblClick[ nIdx ] = checked;
			return TRUE;
		}
		}
	}
	return result;
}


/* ダイアログデータの設定 */
void CDlgTypeList::SetData( void )
{
	SetData(m_nSettingType.GetIndex());
}

void CDlgTypeList::SetData( int selIdx )
{
	int		nIdx;
	TCHAR	szText[64 + MAX_TYPES_EXTS + 10];
	int		nExtent = 0;
	HWND	hwndList = ::GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	HDC		hDC = ::GetDC( hwndList );
	HFONT	hFont = (HFONT)::SendMessageAny(hwndList, WM_GETFONT, 0, 0);
	HFONT	hFontOld = (HFONT)::SelectObject(hDC, hFont);

	if( -1 == selIdx ){
		selIdx = List_GetCurSel( hwndList );
		if( -1 == selIdx ){
			selIdx = 0;
		}
	}
	if( GetDllShareData().m_nTypesCount <= selIdx ){
		selIdx = GetDllShareData().m_nTypesCount - 1;
	}
	List_ResetContent( hwndList );	/* リストを空にする */
	for( nIdx = 0; nIdx < GetDllShareData().m_nTypesCount; ++nIdx ){
		const STypeConfigMini* type;
		CDocTypeManager().GetTypeConfigMini(CTypeConfig(nIdx), &type);
		if( type->m_szTypeExts[0] != _T('\0') ){		/* タイプ属性：拡張子リスト */
			auto_sprintf( szText, _T("%ts ( %ts )"),
				type->m_szTypeName,	/* タイプ属性：名称 */
				type->m_szTypeExts	/* タイプ属性：拡張子リスト */
			);
		}else{
			auto_sprintf( szText, _T("%ts"),
				type->m_szTypeName	/* タイプ属性：拡称 */
			);
		}
		::List_AddString( hwndList, szText );
		m_bRegistryChecked[ nIdx ] = FALSE;
		m_bExtRMenu[ nIdx ] = FALSE;
		m_bExtDblClick[ nIdx ] = FALSE;

		SIZE sizeExtent;
		if( ::GetTextExtentPoint32( hDC, szText, _tcslen(szText), &sizeExtent) && sizeExtent.cx > nExtent ){
			nExtent = sizeExtent.cx;
		}
	}
	for( ; nIdx < MAX_TYPES; ++nIdx ){
		m_bRegistryChecked[ nIdx ] = FALSE;
		m_bExtRMenu[ nIdx ] = FALSE;
		m_bExtDblClick[ nIdx ] = FALSE;
	}
	::SelectObject(hDC, hFontOld);
	::ReleaseDC( hwndList, hDC );
	List_SetHorizontalExtent( hwndList, nExtent + 8 );
	if( GetDllShareData().m_nTypesCount <= selIdx ){
		selIdx = GetDllShareData().m_nTypesCount - 1;
	}
	List_SetCurSel( hwndList, selIdx );

	::SendMessageAny( GetHwnd(), WM_COMMAND, MAKEWPARAM(IDC_LIST_TYPES, LBN_SELCHANGE), 0 );
	DlgItem_Enable( GetHwnd(), IDC_BUTTON_TEMPCHANGE, m_bEnableTempChange );
	DlgItem_Enable( GetHwnd(), IDC_BUTTON_COPY_TYPE, GetDllShareData().m_nTypesCount < MAX_TYPES );
	DlgItem_Enable( GetHwnd(), IDC_BUTTON_ADD_TYPE, GetDllShareData().m_nTypesCount < MAX_TYPES );
	return;
}

//@@@ 2002.01.18 add start
LPVOID CDlgTypeList::GetHelpIdTable(void)
{
	return (LPVOID)p_helpids;
}
//@@@ 2002.01.18 add end

static void SendChangeSetting()
{
	CAppNodeGroupHandle(0).SendMessageToAllEditors(
		MYWM_CHANGESETTING,
		(WPARAM)0,
		(LPARAM)PM_CHANGESETTING_ALL,
		CEditWnd::getInstance()->GetHwnd()
	);
}

static void SendChangeSettingType(int nType)
{
	CAppNodeGroupHandle(0).SendMessageToAllEditors(
		MYWM_CHANGESETTING,
		(WPARAM)nType,
		(LPARAM)PM_CHANGESETTING_TYPE,
		CEditWnd::getInstance()->GetHwnd()
	);
}

static void SendChangeSettingType2(int nType)
{
	CAppNodeGroupHandle(0).SendMessageToAllEditors(
		MYWM_CHANGESETTING,
		(WPARAM)nType,
		(LPARAM)PM_CHANGESETTING_TYPE2,
		CEditWnd::getInstance()->GetHwnd()
	);
}

// タイプ別設定インポート
//		2010/4/12 Uchi
bool CDlgTypeList::Import()
{
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int nIdx = List_GetCurSel( hwndList );
	STypeConfig type;
	// ベースのデータは基本
	CDocTypeManager().GetTypeConfig(CTypeConfig(0), type);

	CImpExpType	cImpExpType( nIdx, type, hwndList );
	const STypeConfigMini* typeMini;
	CDocTypeManager().GetTypeConfigMini(CTypeConfig(nIdx), &typeMini);
	int id = typeMini->m_id;

	// インポート
	cImpExpType.SetBaseName( to_wchar( type.m_szTypeName ) );
	if (!cImpExpType.ImportUI( G_AppInstance(), GetHwnd() )) {
		// インポートをしていない
		return false;
	}
	bool bAdd = cImpExpType.IsAddType();
	if( bAdd ){
		AddType();
		nIdx = GetDllShareData().m_nTypesCount - 1;
		type.m_nIdx = nIdx;
	}else{
		// UIを表示している間にずれているかもしれないのでindex再取得
		nIdx = CDocTypeManager().GetDocumentTypeOfId(id).GetIndex();
		if( -1 == nIdx ){
			return false;
		}
		type.m_nIdx = nIdx;
	}
	type.m_nRegexKeyMagicNumber = CRegexKeyword::GetNewMagicNumber();
	// 適用
	CDocTypeManager().SetTypeConfig(CTypeConfig(nIdx), type);
	if( !bAdd ){
		SendChangeSettingType(nIdx);
	}

	// リスト再初期化
	SetData(nIdx);

	return true;
}

// タイプ別設定エクスポート
//		2010/4/12 Uchi
bool CDlgTypeList::Export()
{
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int nIdx = List_GetCurSel( hwndList );
	STypeConfig types;
	CDocTypeManager().GetTypeConfig(CTypeConfig(nIdx), types);

	CImpExpType	cImpExpType( nIdx, types, hwndList );

	// エクスポート
	cImpExpType.SetBaseName( to_wchar( types.m_szTypeName) );
	if (!cImpExpType.ExportUI( G_AppInstance(), GetHwnd() )) {
		// エクスポートをしていない
		return false;
	}

	return true;
}

/*! タイプ別設定初期化
	@date 2010/4/12 Uchi
	@date 2016.03.09 Moca 基本の初期化をサポート。基本の時は内蔵設定に戻す動作にする

	@retval true  正常
	@retval false 異常
*/
bool CDlgTypeList::InitializeType( void )
{
	HWND hwndDlg = GetHwnd();
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int iDocType = List_GetCurSel( hwndList );
	const STypeConfigMini* typeMini;
	if( !CDocTypeManager().GetTypeConfigMini(CTypeConfig(iDocType), &typeMini) ){
		// なんかエラーだった
		return false;
	}
	int			nRet;
	if( typeMini->m_szTypeExts[0] != _T('\0') || iDocType == 0 ){ 
		nRet = ::MYMESSAGEBOX(
			GetHwnd(),
			MB_YESNO | MB_ICONQUESTION,
			GSTR_APPNAME,
			LS(STR_DLGTYPELIST_INIT1),
			typeMini->m_szTypeName );
		if (nRet != IDYES) {
			return false;
		}
	}

	iDocType = CDocTypeManager().GetDocumentTypeOfId(typeMini->m_id).GetIndex();
	if( -1 == iDocType ){
		return false;
	}
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	// 初期化対象が埋め込みパネルの表示中タイプと同じ場合、未保存分は破棄すべき
	// (初期化結果を上書きしてしまうため)なのでコミットせず追跡を無効化するだけに
	// する。別のタイプを表示中なら影響しないのでそのまま。
	// また初期化は他の一覧操作と同様に即時・確定の操作として扱い、キャンセルしても
	// 元に戻らないようにする(スナップショットを確定させる)
	if( m_nEmbeddedColorTypeIdx == iDocType ){
		m_nEmbeddedColorTypeIdx = -1;
	}
	DiscardColorSnapshot( iDocType );
#endif // NKMM_
//	_DefaultConfig(&types);		//規定値をコピー
	std::unique_ptr<STypeConfig> type(new STypeConfig());
	if( 0 != iDocType ){
		CDocTypeManager().GetTypeConfig(CTypeConfig(0), *type); 	// 基本をコピー

		// 同じ名前にならないように数字をつける
		int nNameNum = iDocType + 1;
		bool bUpdate = true;
		for(int i = 1; i < GetDllShareData().m_nTypesCount; i++){
			if( bUpdate ){
				auto_sprintf( type->m_szTypeName, LS(STR_DLGTYPELIST_SETNAME), nNameNum );
				nNameNum++;
				bUpdate = false;
			}
			if( iDocType == i ){
				continue;
			}
			const STypeConfigMini* typeMini2;
			CDocTypeManager().GetTypeConfigMini(CTypeConfig(i), &typeMini2);
			if( auto_strcmp(typeMini2->m_szTypeName, type->m_szTypeName) == 0 ){
				i = 0;
				bUpdate = true;
			}
		}
		_tcscpy( type->m_szTypeExts, _T("") );
		type->m_nIdx = iDocType;
		type->m_id = (::GetTickCount() & 0x3fffffff) + iDocType * 0x10000;
		type->m_nRegexKeyMagicNumber = CRegexKeyword::GetNewMagicNumber();
	}else{
		// 2016.03.09 基本の初期化
		CType_Basis basis;
		basis.InitTypeConfig(0, *type);
	}

	CDocTypeManager().SetTypeConfig(CTypeConfig(iDocType), *type);

	SendChangeSettingType(iDocType);

	// リスト再初期化
	SetData(iDocType);

	InfoMessage( hwndDlg, LS(STR_DLGTYPELIST_INIT2), type->m_szTypeName );

	return true;
}

bool CDlgTypeList::CopyType()
{
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	// コピー元に埋め込みパネルの未保存分があれば、複製に含められるよう先に反映する
	CommitEmbeddedColorPanel();
#endif // NKMM_
	int nNewTypeIndex = GetDllShareData().m_nTypesCount;
	HWND hwndDlg = GetHwnd();
	HWND hwndList = GetDlgItem( hwndDlg, IDC_LIST_TYPES );
	int iDocType = List_GetCurSel( hwndList );
	STypeConfig type;
	CDocTypeManager().GetTypeConfig(CTypeConfig(iDocType), type);
	// 名前に2等を付ける
	int n = 1;
	bool bUpdate = true;
	for(int i = 0; i < nNewTypeIndex; i++){
		if( bUpdate ){
			TCHAR* p = NULL;
			for(int k = (int)auto_strlen(type.m_szTypeName) - 1; 0 <= k; k--){
				if( WCODE::Is09(type.m_szTypeName[k]) ){
					p = &type.m_szTypeName[k];
				}else{
					break;
				}
			}
			if( p ){
				n = _ttoi(p) + 1;
				*p = _T('\0');
			}else{
				n++;
			}
			TCHAR szNum[12];
			auto_sprintf( szNum, _T("%d"), n );
			int nLen = auto_strlen( szNum );
			TCHAR szTemp[_countof(type.m_szTypeName) + 12];
			auto_strcpy( szTemp, type.m_szTypeName );
			int nTempLen = auto_strlen( szTemp );
			CNativeT cmem;
			// バッファをはみ出さないように
			LimitStringLengthT( szTemp, nTempLen, _countof(type.m_szTypeName) - nLen - 1, cmem );
			auto_strcpy( type.m_szTypeName, cmem.GetStringPtr() );
			auto_strcat( type.m_szTypeName, szNum );
			bUpdate = false;
		}
		const STypeConfigMini* typeMini;
		CDocTypeManager().GetTypeConfigMini(CTypeConfig(i), &typeMini);
		if( auto_strcmp(typeMini->m_szTypeName, type.m_szTypeName) == 0 ){
			i = -1;
			bUpdate = true;
		}
	}
	if( !CDocTypeManager().AddTypeConfig(CTypeConfig(nNewTypeIndex)) ){
		return false;
	}
	type.m_id = (::GetTickCount() & 0x3fffffff) + nNewTypeIndex * 0x10000;
	type.m_nIdx = nNewTypeIndex;
	type.m_nRegexKeyMagicNumber = CRegexKeyword::GetNewMagicNumber();
	CDocTypeManager().SetTypeConfig(CTypeConfig(nNewTypeIndex), type);
	SetData(nNewTypeIndex);
	return true;
}

bool CDlgTypeList::UpType()
{
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int iDocType = List_GetCurSel( hwndList );
	if (iDocType == 0 ) {
		// 基本の場合には何もしない
		return true;
	}
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	// 並べ替えでインデックスの対応が変わるため、埋め込みパネルの未保存分を
	// 先に反映してから追跡インデックスを無効化しておく(古いインデックスへの
	// 誤コミットを防ぐ)。またこの2つのインデックスのスナップショットは
	// 入れ替え後は意味を持たないため、確定させて復元対象から外す
	CommitEmbeddedColorPanel();
	m_nEmbeddedColorTypeIdx = -1;
	DiscardColorSnapshot( iDocType );
	DiscardColorSnapshot( iDocType - 1 );
#endif // NKMM_
	std::unique_ptr<STypeConfig> type1(new STypeConfig());
	std::unique_ptr<STypeConfig> type2(new STypeConfig());
	CDocTypeManager().GetTypeConfig(CTypeConfig(iDocType), *type1);
	CDocTypeManager().GetTypeConfig(CTypeConfig(iDocType - 1), *type2);
	--(type1->m_nIdx);
	++(type2->m_nIdx);
	CDocTypeManager().SetTypeConfig(CTypeConfig(iDocType), *type2);
	CDocTypeManager().SetTypeConfig(CTypeConfig(iDocType - 1), *type1);
	SendChangeSettingType2(iDocType);
	SendChangeSettingType2(iDocType - 1);
	SetData(iDocType - 1);
	return true;
}

bool CDlgTypeList::DownType()
{
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int iDocType = List_GetCurSel( hwndList );
	if (iDocType == 0 || GetDllShareData().m_nTypesCount <= iDocType + 1 ) {
		// 基本、最後の場合には何もしない
		return true;
	}
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	// 並べ替えでインデックスの対応が変わるため、埋め込みパネルの未保存分を
	// 先に反映してから追跡インデックスを無効化しておく(古いインデックスへの
	// 誤コミットを防ぐ)。またこの2つのインデックスのスナップショットは
	// 入れ替え後は意味を持たないため、確定させて復元対象から外す
	CommitEmbeddedColorPanel();
	m_nEmbeddedColorTypeIdx = -1;
	DiscardColorSnapshot( iDocType );
	DiscardColorSnapshot( iDocType + 1 );
#endif // NKMM_
	std::unique_ptr<STypeConfig> type1(new STypeConfig());
	std::unique_ptr<STypeConfig> type2(new STypeConfig());
	CDocTypeManager().GetTypeConfig(CTypeConfig(iDocType), *type1);
	CDocTypeManager().GetTypeConfig(CTypeConfig(iDocType + 1), *type2);
	++(type1->m_nIdx);
	--(type2->m_nIdx);
	CDocTypeManager().SetTypeConfig(CTypeConfig(iDocType), *type2);
	CDocTypeManager().SetTypeConfig(CTypeConfig(iDocType + 1), *type1);
	SendChangeSettingType2(iDocType);
	SendChangeSettingType2(iDocType + 1);
	SetData(iDocType + 1);
	return true;
}

bool CDlgTypeList::AddType()
{
	int nNewTypeIndex = GetDllShareData().m_nTypesCount;
	if( !CDocTypeManager().AddTypeConfig(CTypeConfig(nNewTypeIndex)) ){
		return false;
	}
	SetData(nNewTypeIndex);
	return true;
}

bool CDlgTypeList::DelType()
{
	HWND hwndDlg = GetHwnd();
	HWND hwndList = GetDlgItem( GetHwnd(), IDC_LIST_TYPES );
	int iDocType = List_GetCurSel( hwndList );
	if (iDocType == 0) {
		// 基本の場合には何もしない
		return true;
	}
	const STypeConfigMini* typeMini;
	if( !CDocTypeManager().GetTypeConfigMini(CTypeConfig(iDocType), &typeMini) ){
		// 謎のエラー
		return false;
	}
	const STypeConfigMini type = *typeMini; // ダイアログを出している間に変更されるかもしれないのでコピーする
	int nRet = ConfirmMessage( hwndDlg,
		LS(STR_DLGTYPELIST_DEL), type.m_szTypeName );
	if (nRet != IDYES) {
		return false;
	}
	// ダイアログを出している間にタイプ別リストが更新されたかもしれないのでidから再検索
	CTypeConfig config = CDocTypeManager().GetDocumentTypeOfId(type.m_id);
	if( !config.IsValidType() ){
		return false;
	}
	iDocType = config.GetIndex();
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	// 削除するとインデックスの対応がずれるため、追跡インデックスを無効化しておく
	// (削除対象自身の未保存分は破棄してよいのでコミットしない)
	if( m_nEmbeddedColorTypeIdx != iDocType ){
		CommitEmbeddedColorPanel();
	}
	m_nEmbeddedColorTypeIdx = -1;
	// 削除位置以降は全てインデックスがずれるため、スナップショットを確定させる
	for( int i = iDocType; i < MAX_TYPES; ++i ){
		DiscardColorSnapshot( i );
	}
#endif // NKMM_
	CDocTypeManager().DelTypeConfig(config);
	if( GetDllShareData().m_nTypesCount <= iDocType ){
		iDocType = GetDllShareData().m_nTypesCount - 1;
	}
	SetData(iDocType);
	SendChangeSetting();
	return true;
}


/*! 再帰的レジストリコピー */
int CopyRegistry(HKEY srcRoot, const tstring& srcPath, HKEY destRoot, const tstring& destPath)
{
	int errorCode;
	CRegKey keySrc;
	if((errorCode = keySrc.Open(srcRoot, srcPath.c_str(), KEY_READ)) != 0){ return errorCode; }

	CRegKey keyDest;
	if((errorCode = keyDest.Open(destRoot, destPath.c_str(), KEY_READ | KEY_WRITE)) != 0)
	{
		if( (errorCode = keyDest.Create(destRoot, destPath.c_str())) != 0 ){ return errorCode; }
	}

	int index = 0;
	for (;;)
	{
		TCHAR szValue[ BUFFER_SIZE ] = {0};
		BYTE data[ BUFFER_SIZE ] = {0};
		DWORD dwDataLen;
		DWORD dwType;

		errorCode = keySrc.EnumValue(index, szValue, _countof(szValue), &dwType, data, _countof(data), &dwDataLen );
		if( errorCode == ERROR_NO_MORE_ITEMS ){
			errorCode = 0;
			break;
		}else if( errorCode ){
			return errorCode;
		}else{
			// 手抜き：データのサイズがBUFFER_SIZE(=1024)を超える場合を考慮していない
			if( (errorCode = keyDest.SetValue(szValue, data, dwDataLen, dwType)) != 0 ){ return errorCode; }
			index++;
		}
	}

	index = 0;
	TCHAR szSubKey[ BUFFER_SIZE ] = {0};
	for (;;)
	{
		errorCode = keySrc.EnumKey(index, szSubKey, _countof(szSubKey));
		if( errorCode == ERROR_NO_MORE_ITEMS ){
			errorCode = 0;
			break;
		}else if( errorCode ){
			return errorCode;
		}else{
			if( (errorCode = CopyRegistry(srcRoot, srcPath + _T("\\") + szSubKey, destRoot, destPath + _T("\\") + szSubKey)) ){ return errorCode; }
			index++;
		}
	}

	return errorCode;
}

/*! 再帰的レジストリ削除 */
int DeleteRegistry(HKEY root, const tstring& path)
{
	int errorCode;
	CRegKey keySrc;
	if((errorCode = keySrc.Open(root, path.c_str(), KEY_READ | KEY_WRITE)) != 0){ return ERROR_SUCCESS; }

	int index = 0;
	index = 0;
	TCHAR szSubKey[ BUFFER_SIZE ] = {0};
	for (;;)
	{
		errorCode = keySrc.EnumKey(index, szSubKey, _countof(szSubKey));
		if( errorCode == ERROR_NO_MORE_ITEMS ){
			errorCode = 0;
			break;
		}else if( errorCode ){
			return errorCode;
		}else{
			if( (errorCode = DeleteRegistry(root, path + _T("\\") + szSubKey)) != 0 ){ return errorCode; }
		}
	}
	keySrc.Close();
	if( (errorCode = CRegKey::DeleteKey(root, path.c_str())) != 0 ){ return errorCode; }

	return errorCode;
}

/*!
	@brief 拡張子ごとの関連付けレジストリ設定を行う
	@param sExt	拡張子
	@param bDefProg [in]既定フラグ（ダブルクリックで起動させるか）
	レジストリアクセス方針
	・管理者権限なしで実施したいため、HKLMは読み込みのみとし、書き込みはHKCUに行う。
	処理の流れ
	・[HKCU\Software\Classes\.(拡張子)]の存在チェック
		存在しなければ
		・[HKCU\Software\Classes\.(拡張子)]を作成。値は「SakuraEditor_(拡張子)」
	・[HKCU\Software\Classes\.(拡張子)]の値が「SakuraEditor_(拡張子)」以外の場合、
		[HKCU\Software\Classes\.(拡張子)\SakuraEditorBackup]に値をコピーする
		値に「SakuraEditor_(拡張子)」を設定する
	・ProgID <- [HKCR\Software\Classes\.(拡張子)]の値
	・[HKCU\Software\Classes\(ProgID)]の存在チェック
		存在しなければ
		・[HKLM\Software\Classes\(HKLMのProgID)]の存在チェック
		存在すれば
			・[HKLM\Software\Classes\(ProgID)]の構造を[HKCU\Software\Classes\(ProgID)]にコピーする
	・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor\command]を作成。値は「"(サクラEXEパス)" "%1"」
	・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor]の値を「Sakura &Editor」とする
	・既定フラグ判定
		trueなら
			・[HKCU\Software\Classes\(ProgID)\shell]の値が空でなければ[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor\ShellBackup]に退避する
			・[HKCU\Software\Classes\(ProgID)\shell]の値を「SakuraEditor」とする
		falseなら
			・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor\ShellBackup]の存在チェック
				存在すれば、退避した値を[HKCU\Software\Classes\(ProgID)\shell]に設定
				存在しなければ、[HKCU\Software\Classes\(ProgID)\shell]の値を削除
*/
int RegistExt(LPCTSTR sExt, bool bDefProg)
{
	int errorCode = ERROR_SUCCESS;
	tstring sBasePath = tstring( _T("Software\\Classes\\") );

	//小文字化
	TCHAR szLowerExt[MAX_PATH] = {0};
	_tcsncpy_s(szLowerExt, sizeof(szLowerExt) / sizeof(szLowerExt[0]), sExt, _tcslen(sExt));
	CharLower(szLowerExt);

	tstring sDotExt = sBasePath + _T(".") + szLowerExt;
	tstring sGenProgID = tstring() + _T("SakuraEditor_") + szLowerExt;

	CRegKey keyExt_HKLM;
	TCHAR szProgID_HKLM[ BUFFER_SIZE ] = {0};
	if( ( errorCode = keyExt_HKLM.Open(HKEY_LOCAL_MACHINE, sDotExt.c_str(), KEY_READ) ) == 0 )
	{
		keyExt_HKLM.GetValue(NULL, szProgID_HKLM, _countof(szProgID_HKLM));
	}

	CRegKey keyExt;
	if((errorCode = keyExt.Open(HKEY_CURRENT_USER, sDotExt.c_str(), KEY_READ | KEY_WRITE)) != 0)
	{
		if( (errorCode = keyExt.Create(HKEY_CURRENT_USER, sDotExt.c_str())) != 0 ){ return errorCode; }
	}

	TCHAR szProgID[ BUFFER_SIZE ] = {0};
	keyExt.GetValue(NULL, szProgID, _countof(szProgID));

	if(_tcscmp( sGenProgID.c_str(), szProgID ) != 0) {
		if( szProgID[0] != _T('\0') )
		{
			if( (errorCode = keyExt.SetValue(PROGID_BACKUP_NAME, szProgID)) != 0 ){ return errorCode; }
		} 
		if( (errorCode = keyExt.SetValue(NULL, sGenProgID.c_str())) != 0 ){ return errorCode; }
	}

	tstring sProgIDPath = sBasePath + sGenProgID;
	if( ! CRegKey::ExistsKey(HKEY_CURRENT_USER, sProgIDPath.c_str()) )
	{
		if( szProgID_HKLM[0] != _T('\0') )
		{
			if( (errorCode = CopyRegistry(HKEY_LOCAL_MACHINE, (sBasePath + szProgID_HKLM).c_str(), HKEY_CURRENT_USER, sProgIDPath.c_str())) != 0 ){ return errorCode; }
		}
	}

	tstring sShellPath = sProgIDPath + _T("\\shell");
	tstring sShellActionPath = sShellPath + _T("\\") + ACTION_NAME;
	tstring sShellActionCommandPath = sShellActionPath + _T("\\command");
	tstring sBackupPath = sShellActionPath + ACTION_BACKUP_PATH;

	CRegKey keyShellActionCommand;
	if( (errorCode = keyShellActionCommand.Open(HKEY_CURRENT_USER, sShellActionCommandPath.c_str(), KEY_READ | KEY_WRITE)) != 0 )
	{
		if( (errorCode = keyShellActionCommand.Create(HKEY_CURRENT_USER, sShellActionCommandPath.c_str())) != 0 ){ return errorCode; }
	}

	TCHAR sExePath[_MAX_PATH] = {0};
	::GetModuleFileName( NULL, sExePath, _countof(sExePath) );
	tstring sCommandPathArg = tstring() + _T("\"") + sExePath + _T("\" \"%1\"");
	if( (errorCode = keyShellActionCommand.SetValue(NULL, sCommandPathArg.c_str())) != 0 ){ return errorCode; }

	CRegKey keyShellAction;
	if( (errorCode = keyShellAction.Open(HKEY_CURRENT_USER, sShellActionPath.c_str(), KEY_READ | KEY_WRITE)) !=0 ){ return errorCode; }
	if( (errorCode = keyShellAction.SetValue(NULL, _T("Sakura &Editor"))) != 0 ){ return errorCode; }

	CRegKey keyShell;
	if( (errorCode = keyShell.Open(HKEY_CURRENT_USER, sShellPath.c_str(), KEY_READ | KEY_WRITE)) != 0 ){ return errorCode; }
	TCHAR szShellValue[ BUFFER_SIZE ] = {0};
	keyShell.GetValue(NULL, szShellValue, _countof(szShellValue));
	if(bDefProg)
	{
		if( _tcscmp(szShellValue, ACTION_NAME) != 0 )
		{
			if( szShellValue[0] != '\0')
			{
				CRegKey keyBackup;
				if( (errorCode = keyBackup.Open(HKEY_CURRENT_USER, sBackupPath.c_str(), KEY_READ | KEY_WRITE)) != 0 )
				{
					if( (errorCode = keyBackup.Create(HKEY_CURRENT_USER, sBackupPath.c_str())) != 0 ){ return errorCode; }
				}
				keyBackup.SetValue(NULL, szShellValue);
			}
			keyShell.SetValue(NULL, ACTION_NAME);
		}
	}
	else
	{
		CRegKey keyBackup;
		if( (errorCode = keyBackup.Open(HKEY_CURRENT_USER, sBackupPath.c_str(), KEY_READ | KEY_WRITE)) != 0 )
		{
			keyShell.DeleteValue(_T(""));
		}
		else
		{
			TCHAR sBackupValue[ BUFFER_SIZE ] = {0};
			keyBackup.GetValue(NULL, sBackupValue, _countof(sBackupValue));
			keyShell.SetValue(NULL, sBackupValue);
		}
	}

	return ERROR_SUCCESS;
}

/*!
	@brief 拡張子ごとの関連付けレジストリ設定を削除する
	@param sExt	[in]拡張子
	処理の流れ
	・[HKCU\Software\Classes\.(拡張子)]の存在チェック
		存在しなければ終了
	・ProgID <- [HKCU\Software\Classes\.(拡張子)]の値
	・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor]の存在チェック
		存在しなければ終了
	・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor\ShellBackup]の存在チェック
		存在すれば、退避した値を[HKCU\Software\Classes\(ProgID)\shell]に設定
		存在しなければ、[HKCU\Software\Classes\(ProgID)\shell]の値を削除
	・ProgIDの先頭が"SakuraEditor_"か？
		そうなら[HKCU\Software\Classes\(ProgID)]と[HKCU\Software\Classes\.(拡張子)]を削除
	　　そうでなければ[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor]を削除
*/
int UnregistExt(LPCTSTR sExt)
{
	int errorCode = ERROR_SUCCESS;
	tstring sBasePath = tstring( _T("Software\\Classes\\") );

	//小文字化
	TCHAR szLowerExt[MAX_PATH] = {0};
	_tcsncpy_s(szLowerExt, sizeof(szLowerExt) / sizeof(szLowerExt[0]), sExt, _tcslen(sExt));
	CharLower(szLowerExt);

	tstring sDotExt = sBasePath + _T(".") + szLowerExt;
	tstring sGenProgID = tstring() + szLowerExt + _T("file");

	CRegKey keyExt;
	if((errorCode = keyExt.Open(HKEY_CURRENT_USER, sDotExt.c_str(), KEY_READ | KEY_WRITE)) != 0)
	{
		return errorCode;
	}

	TCHAR szProgID[ BUFFER_SIZE ] = {0};
	keyExt.GetValue(NULL, szProgID, _countof(szProgID));

	if( szProgID[0] == _T('\0') )
	{
		return ERROR_SUCCESS;
	}

	tstring sProgIDPath = sBasePath + szProgID;
	tstring sShellPath = sProgIDPath + _T("\\shell");
	tstring sShellActionPath = sShellPath + _T("\\") + ACTION_NAME;
	tstring sShellActionCommandPath = sShellActionPath + _T("\\command");
	tstring sBackupPath = sShellActionPath + ACTION_BACKUP_PATH;

	CRegKey keyShellAction;
	if( (errorCode = keyShellAction.Open(HKEY_CURRENT_USER, sShellActionPath.c_str(), KEY_READ | KEY_WRITE)) != 0 )
	{
		return ERROR_SUCCESS;
	}

	CRegKey keyShell;
	if( (errorCode = keyShell.Open(HKEY_CURRENT_USER, sShellPath.c_str(), KEY_READ | KEY_WRITE)) != 0 ){ return errorCode; }
	CRegKey keyBackup;
	if( (errorCode = keyBackup.Open(HKEY_CURRENT_USER, sBackupPath.c_str(), KEY_READ | KEY_WRITE)) != 0 )
	{
		keyShell.DeleteValue(_T(""));
	}
	else
	{
		TCHAR szBackupValue[ BUFFER_SIZE ] = {0};
		keyBackup.GetValue(NULL, szBackupValue, _countof(szBackupValue));
		keyShell.SetValue(NULL, szBackupValue);
	}

	keyBackup.Close();
	keyShellAction.Close();
	if( _tcsncmp(szProgID, _T("SakuraEditor_"), 13) == 0)
	{
		if( (errorCode = DeleteRegistry(HKEY_CURRENT_USER, sProgIDPath)) != 0 ){ return errorCode; }

		TCHAR szBackupValue[ BUFFER_SIZE ] = {0};
		keyExt.GetValue(PROGID_BACKUP_NAME, szBackupValue, _countof(szBackupValue));
		if( szBackupValue[0] != _T('\0') ){
			keyExt.SetValue(NULL, szBackupValue);
		}else{
			if( (errorCode = DeleteRegistry(HKEY_CURRENT_USER, sDotExt)) != 0 ){ return errorCode; }
		}
	}else{
		if( (errorCode = DeleteRegistry(HKEY_CURRENT_USER, sShellActionPath)) != 0 ){ return errorCode; }
	}

	return ERROR_SUCCESS;
}

/*!
	@brief 拡張子ごとの関連付けレジストリ設定を確認する
	@param sExt			[in]拡張子
	@param pbRMenu		[out]関連付け設定
	@param pbDblClick	[out]既定設定
	処理の流れ
	・pbRMenu <- false, pbDblClick <- false
	・[HKCU\Software\Classes\.(拡張子)]の存在チェック
		存在しなければ終了
	・ProgID <- [HKCU\Software\Classes\.(拡張子)]の値
	・[HKCU\Software\Classes\(ProgID)\shell\SakuraEditor]の存在チェック
		存在しなければ終了
	・pbRMenu <- true
	・[HKCU\Software\Classes\(ProgID)\shell]の値をチェック
		「SakuraEditor」なら、pbDblClick <- true
*/
int CheckExt(LPCTSTR sExt, bool *pbRMenu, bool *pbDblClick)
{
	int errorCode = ERROR_SUCCESS;
	tstring sBasePath = tstring( _T("Software\\Classes\\") );

	*pbRMenu = false;
	*pbDblClick = false;

	//小文字化
	TCHAR szLowerExt[MAX_PATH] = {0};
	_tcsncpy_s(szLowerExt, sizeof(szLowerExt) / sizeof(szLowerExt[0]), sExt, _tcslen(sExt));
	CharLower(szLowerExt);

	tstring sDotExt = sBasePath + _T(".") + szLowerExt;
	tstring sGenProgID = tstring() + szLowerExt + _T("file");

	CRegKey keyExt;
	if((errorCode = keyExt.Open(HKEY_CURRENT_USER, sDotExt.c_str(), KEY_READ)) != 0)
	{
		return ERROR_SUCCESS;
	}

	TCHAR szProgID[ BUFFER_SIZE ] = {0};
	keyExt.GetValue(NULL, szProgID, _countof(szProgID));

	if(szProgID[0] == _T('\0'))
	{
		return ERROR_SUCCESS;
	}

	tstring sShellPath = tstring() + _T("Software\\Classes\\") + szProgID + _T("\\shell");
	tstring sShellActionPath = sShellPath + _T("\\") + ACTION_NAME;
	if( ! CRegKey::ExistsKey(HKEY_CURRENT_USER, sShellActionPath.c_str()) )
	{
		return ERROR_SUCCESS;
	}
	*pbRMenu = true;

	CRegKey keyShell;
	if( (errorCode = keyShell.Open(HKEY_CURRENT_USER, sShellPath.c_str(), KEY_READ)) != 0 ){ return errorCode; }
	TCHAR szShellValue[ BUFFER_SIZE ] = {0};
	keyShell.GetValue(NULL, szShellValue, _countof(szShellValue));
	if( _tcscmp( szShellValue, ACTION_NAME ) == 0 )
	{
		*pbDblClick = true;
	}

	return ERROR_SUCCESS;
}

/*!
	@brief レジストリ変更の警告メッセージを表示する
*/
bool CDlgTypeList::AlertFileAssociation()
{
	if( m_bAlertFileAssociation ){
		if( IDYES == ::MYMESSAGEBOX( 
						NULL, MB_YESNO | MB_ICONEXCLAMATION | MB_APPLMODAL | MB_TOPMOST,
						GSTR_APPNAME,
						LS(STR_DLGTYPELIST_ACC))
					)
		{
			m_bAlertFileAssociation = false;	//「はい」なら最初の一度だけ確認する
			return true;
		}else{
			return false;
		}
	}
	return true;
}