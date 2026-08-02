/*!	@file
	@brief 共通設定ダイアログボックス、「強調キーワード」ページ

	@author Norio Nakatani
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000, MIK
	Copyright (C) 2001, genta, MIK
	Copyright (C) 2002, YAZAKI, MIK, genta
	Copyright (C) 2003, KEITA
	Copyright (C) 2005, genta, Moca
	Copyright (C) 2006, ryoji
	Copyright (C) 2007, ryoji
	Copyright (C) 2009, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "prop/CPropCommon.h"
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include "env/CDocTypeManager.h"
#include "typeprop/CImpExpManager.h"	// 20210/4/23 Uchi
#include "dlg/CDlgInput1.h"
#include "util/shell.h"
#include "util/file.h"	// 20260802 GetExedir (再読込ボタン用)
#include "view/colors/EColorIndexType.h"	// 20260802 COLORIDX_KEYWORD1 (色プレビュー用)
#include <memory>
#include "sakura_rc.h"
#include "sakura.hh"


//@@@ 2001.02.04 Start by MIK: Popup Help
static const DWORD p_helpids[] = {	//10800
	IDC_BUTTON_ADDSET,				HIDC_BUTTON_ADDSET,			//キーワードセット追加
	IDC_BUTTON_DELSET,				HIDC_BUTTON_DELSET,			//キーワードセット削除
	IDC_BUTTON_ADDKEYWORD,			HIDC_BUTTON_ADDKEYWORD,		//キーワード追加
	IDC_BUTTON_EDITKEYWORD,			HIDC_BUTTON_EDITKEYWORD,	//キーワード編集
	IDC_BUTTON_DELKEYWORD,			HIDC_BUTTON_DELKEYWORD,		//キーワード削除
	IDC_BUTTON_IMPORT,				HIDC_BUTTON_IMPORT_KEYWORD,	//インポート
	IDC_BUTTON_EXPORT,				HIDC_BUTTON_EXPORT_KEYWORD,	//エクスポート
	IDC_CHECK_KEYWORDCASE,			HIDC_CHECK_KEYWORDCASE,		//キーワードの英大文字小文字区別
	IDC_COMBO_SET,					HIDC_COMBO_SET,				//強調キーワードセット名
	IDC_LIST_KEYWORD,				HIDC_LIST_KEYWORD,			//キーワード一覧
	IDC_BUTTON_KEYCLEAN		,		HIDC_BUTTON_KEYCLEAN,		//キーワード整理	// 2006.08.06 ryoji
	IDC_BUTTON_KEYSETRENAME,		HIDC_BUTTON_KEYSETRENAME,	//セットの名称変更	// 2006.08.06 ryoji
#if defined(NKMM_FIX_KEYWORDSET_UI)
	IDC_BUTTON_KEYWORD_RELOAD,		HIDC_BUTTON_KEYWORD_RELOAD,	//キーワードセット再読込	// 20260802
#endif // NKMM_
//	IDC_STATIC,						-1,
	0, 0
};
//@@@ 2001.02.04 End

//	From Here Jun. 2, 2001 genta
/*!
	@param hwndDlg ダイアログボックスのWindow Handle
	@param uMsg メッセージ
	@param wParam パラメータ1
	@param lParam パラメータ2
*/
INT_PTR CALLBACK CPropKeyword::DlgProc_page(
	HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DlgProc( reinterpret_cast<pDispatchPage>(&CPropKeyword::DispatchEvent), hwndDlg, uMsg, wParam, lParam );
}
//	To Here Jun. 2, 2001 genta
INT_PTR CALLBACK CPropKeyword::DlgProc_dialog(
	HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DlgProc2( reinterpret_cast<pDispatchPage>(&CPropKeyword::DispatchEvent), hwndDlg, uMsg, wParam, lParam );
}

/* Keyword メッセージ処理 */
INT_PTR CPropKeyword::DispatchEvent(
	HWND	hwndDlg,	// handle to dialog box
	UINT	uMsg,		// message
	WPARAM	wParam,		// first message parameter
	LPARAM	lParam 		// second message parameter
)
{
	WORD				wNotifyCode;
	WORD				wID;
	HWND				hwndCtl;
	NMHDR*				pNMHDR;
	int					nIndex1;
	LV_COLUMN			lvc;
	LV_ITEM*			plvi;
	static HWND			hwndCOMBO_SET;
	static HWND			hwndLIST_KEYWORD;
	RECT				rc;
	CDlgInput1			cDlgInput1;
	wchar_t				szKeyWord[MAX_KEYWORDLEN + 1];
	LONG_PTR			lStyle;
	LV_DISPINFO*		plvdi;
	LV_KEYDOWN*			pnkd;

	switch( uMsg ){
	case WM_INITDIALOG:
		/* ダイアログデータの設定 Keyword */
		if( wParam == IDOK ){ // 独立ウィンドウ
			if( 0 <= m_nKeywordSet1 ){
				// タイプ別設定のものを表示
				m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx = m_nKeywordSet1;
			}else{
				// -1だったら共通設定で最後に表示したものを維持する
			}
		}
		SetData( hwndDlg );
		// Modified by KEITA for WIN64 2003.9.6
		::SetWindowLongPtr( hwndDlg, DWLP_USER, lParam );
		if( wParam == IDOK ){ // 独立ウィンドウ
			hwndCtl = ::GetDlgItem( hwndDlg, IDOK );
			GetWindowRect( hwndCtl, &rc );
			int i = rc.bottom; // OK,CANCELボタンの下端

			GetWindowRect( hwndDlg, &rc );
			SetWindowPos( hwndDlg, NULL, 0, 0, rc.right-rc.left, i-rc.top+10, SWP_NOZORDER|SWP_NOMOVE );
			std::tstring title = LS(STR_PROPCOMMON);
			title += _T(" - ");
			title += LS(STR_PROPCOMMON_KEYWORD);
			SetWindowText( hwndDlg, title.c_str() );
		}
		else{
			hwndCtl = ::GetDlgItem( hwndDlg, IDOK );
			ShowWindow( hwndCtl, SW_HIDE );
			hwndCtl = ::GetDlgItem( hwndDlg, IDCANCEL );
			ShowWindow( hwndCtl, SW_HIDE );
		}

		/* コントロールのハンドルを取得 */
		hwndCOMBO_SET = ::GetDlgItem( hwndDlg, IDC_COMBO_SET );
		hwndLIST_KEYWORD = ::GetDlgItem( hwndDlg, IDC_LIST_KEYWORD );
		::GetWindowRect( hwndLIST_KEYWORD, &rc );
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = rc.right - rc.left;
		lvc.pszText = const_cast<TCHAR*>(_T(""));
		lvc.iSubItem = 0;
		ListView_InsertColumn( hwndLIST_KEYWORD, 0, &lvc );

		lStyle = ::GetWindowLongPtr( hwndLIST_KEYWORD, GWL_STYLE );
		::SetWindowLongPtr( hwndLIST_KEYWORD, GWL_STYLE, lStyle | LVS_SHOWSELALWAYS );


		/* コントロール更新のタイミング用のタイマーを起動 */
		::SetTimer( hwndDlg, 1, 300, NULL );

		return TRUE;

	case WM_NOTIFY:
		pNMHDR = (NMHDR*)lParam;
		pnkd = (LV_KEYDOWN *)lParam;
		plvdi = (LV_DISPINFO*)lParam;
		plvi = &plvdi->item;

		if( hwndLIST_KEYWORD == pNMHDR->hwndFrom ){
			switch( pNMHDR->code ){
			case NM_DBLCLK:
//				MYTRACE( _T("NM_DBLCLK     \n") );
#if defined(NKMM_FIX_KEYWORDSET_UI)
				if( IsKeywordCsvLoaded() ){
					return TRUE;
				}
#endif // NKMM_
				/* リスト中で選択されているキーワードを編集する */
				Edit_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
				return TRUE;
			case LVN_BEGINLABELEDIT:
#ifdef _DEBUG
				MYTRACE( _T("LVN_BEGINLABELEDIT\n") );
												MYTRACE( _T("	plvi->mask =[%xh]\n"), plvi->mask );
												MYTRACE( _T("	plvi->iItem =[%d]\n"), plvi->iItem );
												MYTRACE( _T("	plvi->iSubItem =[%d]\n"), plvi->iSubItem );
				if (plvi->mask & LVIF_STATE)	MYTRACE( _T("	plvi->state =[%xf]\n"), plvi->state );
												MYTRACE( _T("	plvi->stateMask =[%xh]\n"), plvi->stateMask );
				if (plvi->mask & LVIF_TEXT)		MYTRACE( _T("	plvi->pszText =[%ts]\n"), plvi->pszText );
												MYTRACE( _T("	plvi->cchTextMax=[%d]\n"), plvi->cchTextMax );
				if (plvi->mask & LVIF_IMAGE)	MYTRACE( _T("	plvi->iImage=[%d]\n"), plvi->iImage );
				if (plvi->mask & LVIF_PARAM)	MYTRACE( _T("	plvi->lParam=[%xh(%d)]\n"), plvi->lParam, plvi->lParam );
#endif
				return TRUE;
			case LVN_ENDLABELEDIT:
#ifdef _DEBUG
				MYTRACE( _T("LVN_ENDLABELEDIT\n") );
												MYTRACE( _T("	plvi->mask =[%xh]\n"), plvi->mask );
												MYTRACE( _T("	plvi->iItem =[%d]\n"), plvi->iItem );
												MYTRACE( _T("	plvi->iSubItem =[%d]\n"), plvi->iSubItem );
				if (plvi->mask & LVIF_STATE)	MYTRACE( _T("	plvi->state =[%xf]\n"), plvi->state );
												MYTRACE( _T("	plvi->stateMask =[%xh]\n"), plvi->stateMask );
				if (plvi->mask & LVIF_TEXT)		MYTRACE( _T("	plvi->pszText =[%ts]\n"), plvi->pszText  );
												MYTRACE( _T("	plvi->cchTextMax=[%d]\n"), plvi->cchTextMax );
				if (plvi->mask & LVIF_IMAGE)	MYTRACE( _T("	plvi->iImage=[%d]\n"), plvi->iImage );
				if (plvi->mask & LVIF_PARAM)	MYTRACE( _T("	plvi->lParam=[%xh(%d)]\n"), plvi->lParam, plvi->lParam );
#endif
				if( NULL == plvi->pszText ){
					return TRUE;
				}
				if( plvi->pszText[0] != _T('\0') ){
					if( MAX_KEYWORDLEN < _tcslen( plvi->pszText ) ){
						InfoMessage( hwndDlg, LS(STR_PROPCOMKEYWORD_ERR_LEN), MAX_KEYWORDLEN );
						return TRUE;
					}
					/* ｎ番目のセットにキーワードを編集 */
					m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.UpdateKeyWord(
						m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx,
						plvi->lParam,
						to_wchar(plvi->pszText)
					);
				}else{
					/* ｎ番目のセットのｍ番目のキーワードを削除 */
					m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.DelKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, plvi->lParam );
				}
				/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
				SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );

				ListView_SetItemState( hwndLIST_KEYWORD, plvi->iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );

				return TRUE;
			case LVN_KEYDOWN:
//				MYTRACE( _T("LVN_KEYDOWN\n") );
#if defined(NKMM_FIX_KEYWORDSET_UI)
				if( IsKeywordCsvLoaded() ){
					return TRUE;
				}
#endif // NKMM_
				switch( pnkd->wVKey ){
				case VK_DELETE:
					/* リスト中で選択されているキーワードを削除する */
					Delete_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					break;
				case VK_SPACE:
					/* リスト中で選択されているキーワードを編集する */
					Edit_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					break;
				}
				return TRUE;
#if defined(NKMM_FIX_KEYWORDSET_UI)
			case NM_CUSTOMDRAW:
				// 選択中のセットに割り当てられた強調表示色・太字/下線でキーワード一覧をプレビュー表示する 20260802
				{
					LPNMLVCUSTOMDRAW plvcd = (LPNMLVCUSTOMDRAW)lParam;
					switch( plvcd->nmcd.dwDrawStage ){
					case CDDS_PREPAINT:
						::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW );
						return TRUE;
					case CDDS_ITEMPREPAINT:
						if( m_bKeywordSetColorValid ){
							plvcd->clrText = m_crKeywordSetText;
							plvcd->clrTextBk = m_crKeywordSetBack;
						}
						if( m_hKeywordPreviewFont ){
							::SelectObject( plvcd->nmcd.hdc, m_hKeywordPreviewFont );
							::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_NEWFONT );
						}else{
							::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT );
						}
						return TRUE;
					}
				}
				::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT );
				return TRUE;
#endif // NKMM_
			}
		}else{
			switch( pNMHDR->code ){
			case PSN_HELP:
				OnHelp( hwndDlg, IDD_PROP_KEYWORD );
				return TRUE;
			case PSN_KILLACTIVE:
				DEBUG_TRACE( _T("Keyword PSN_KILLACTIVE\n") );
				/* ダイアログデータの取得 Keyword */
				GetData( hwndDlg );
				return TRUE;
//@@@ 2002.01.03 YAZAKI 最後に表示していたシートを正しく覚えていないバグ修正
			case PSN_SETACTIVE:
				m_nPageNum = ID_PROPCOM_PAGENUM_KEYWORD;
				return TRUE;
			}
		}
		break;
	case WM_COMMAND:
		wNotifyCode = HIWORD(wParam);	/* 通知コード */
		wID = LOWORD(wParam);			/* 項目ID､ コントロールID､ またはアクセラレータID */
		hwndCtl = (HWND) lParam;		/* コントロールのハンドル */
		if( hwndCOMBO_SET == hwndCtl){
			switch( wNotifyCode ){
			case CBN_SELCHANGE:
				nIndex1 = Combo_GetCurSel( hwndCOMBO_SET );
				/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
				SetKeyWordSet( hwndDlg, nIndex1 );
				return TRUE;
			}
		}else{
			switch( wNotifyCode ){
			/* ボタン／チェックボックスがクリックされた */
			case BN_CLICKED:
				switch( wID ){
				case IDC_BUTTON_ADDSET:	/* セット追加 */
					if( MAX_SETNUM <= m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nKeyWordSetNum ){
						InfoMessage( hwndDlg, LS(STR_PROPCOMKEYWORD_SETMAX), MAX_SETNUM );
						return TRUE;
					}
					/* モードレスダイアログの表示 */
					szKeyWord[0] = L'\0';
					//	Oct. 5, 2002 genta 長さ制限の設定を修正．バッファオーバーランしていた．
					if( !cDlgInput1.DoModal(
						G_AppInstance(),
						hwndDlg,
						LS(STR_PROPCOMKEYWORD_SETNAME1),
						LS(STR_PROPCOMKEYWORD_SETNAME2),
						MAX_SETNAMELEN,
						szKeyWord
						)
					){
						return TRUE;
					}
					if( szKeyWord[0] != L'\0' ){
						/* セットの追加 */
						m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.AddKeyWordSet( szKeyWord, false );

						m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nKeyWordSetNum - 1;

						/* ダイアログデータの設定 Keyword */
						SetData( hwndDlg );
					}
					return TRUE;
				case IDC_BUTTON_DELSET:	/* セット削除 */
					{
						nIndex1 = Combo_GetCurSel( hwndCOMBO_SET );
						if( CB_ERR == nIndex1 ){
							return TRUE;
						}
						/* 削除対象のセットを使用しているファイルタイプを列挙 */
						std::tstring strLabel;
						for( size_t i = 0; i < m_Types_nKeyWordSetIdx.size() ; ++i ){
							// 2002/04/25 YAZAKI STypeConfig全体を保持する必要はないし、m_pShareDataを直接見ても問題ない。
							for( int k = 0; k < MAX_KEYWORDSET_PER_TYPE; k++ ){
								if( nIndex1 == m_Types_nKeyWordSetIdx[i].index[k] ){
									std::tstring name;
									std::tstring exts;
									bool bAdd = false;
									if( m_Types_nKeyWordSetIdx[i].typeId == -1 ){
										// タイプ別一時表示
										name = _T("(Temp)");
										name += m_tempTypeName;
										exts = m_tempTypeExts;
										bAdd = true;
									}else{
										const STypeConfigMini* type = NULL;
										CTypeConfig typeConfig = CDocTypeManager().GetDocumentTypeOfId( m_Types_nKeyWordSetIdx[i].typeId );
										if( CDocTypeManager().GetTypeConfigMini( typeConfig, &type ) ){
											name = type->m_szTypeName;
											exts = type->m_szTypeExts;
											bAdd = true;
										}
									}
									if( bAdd ){
										strLabel += _T("・");
										strLabel += name;
										strLabel +=  _T("(");
										strLabel += exts;
										strLabel += _T(")\n");
									}
									break;
								}
							}
						}
						if( IDCANCEL == ::MYMESSAGEBOX(	hwndDlg, MB_OKCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
							LS(STR_PROPCOMKEYWORD_SETDEL),
							m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetTypeName( nIndex1 ),
							strLabel.c_str()
						) ){
							return TRUE;
						}
						/* 削除対象のセットを使用しているファイルタイプのセットをクリア */
						for( size_t i = 0; i < m_Types_nKeyWordSetIdx.size(); ++i ){
							// 2002/04/25 YAZAKI STypeConfig全体を保持する必要はない。
							for( int j = 0; j < MAX_KEYWORDSET_PER_TYPE; j++ ){
								if( nIndex1 == m_Types_nKeyWordSetIdx[i].index[j] ){
									m_Types_nKeyWordSetIdx[i].index[j] = -1;
								}
								else if( nIndex1 < m_Types_nKeyWordSetIdx[i].index[j] ){
									m_Types_nKeyWordSetIdx[i].index[j]--;
								}
							}
						}
						/* ｎ番目のセットを削除 */
						m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.DelKeyWordSet( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
						/* ダイアログデータの設定 Keyword */
						SetData( hwndDlg );
					}
					return TRUE;
				case IDC_BUTTON_KEYSETRENAME: // キーワードセットの名称変更
					// モードレスダイアログの表示
					wcscpy( szKeyWord, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetTypeName( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx ) );
					{
						BOOL bDlgInputResult = cDlgInput1.DoModal(
							G_AppInstance(),
							hwndDlg,
							LS(STR_PROPCOMKEYWORD_RENAME1),
							LS(STR_PROPCOMKEYWORD_RENAME2),
							MAX_SETNAMELEN,
							szKeyWord
						);
						if( !bDlgInputResult ){
							return TRUE;
						}
					}
					if( szKeyWord[0] != L'\0' ){
						m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.SetTypeName( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, szKeyWord );

						// ダイアログデータの設定 Keyword
						SetData( hwndDlg );
					}
					return TRUE;
				case IDC_CHECK_KEYWORDCASE:	/* キーワードの英大文字小文字区別 */
//					m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_bKEYWORDCASEArr[ m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx ] = ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_KEYWORDCASE );	//MIK 2000.12.01 case sense
					m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.SetKeyWordCase(m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_KEYWORDCASE ));			//MIK 2000.12.01 case sense
					return TRUE;
				case IDC_BUTTON_ADDKEYWORD:	/* キーワード追加 */
					/* ｎ番目のセットのキーワードの数を返す */
					if( !m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.CanAddKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx ) ){
						InfoMessage( hwndDlg, LS(STR_PROPCOMKEYWORD_KEYMAX) );
						return TRUE;
					}
					/* モードレスダイアログの表示 */
					szKeyWord[0] = L'\0';
					if( !cDlgInput1.DoModal( G_AppInstance(), hwndDlg, LS(STR_PROPCOMKEYWORD_KEYADD1), LS(STR_PROPCOMKEYWORD_KEYADD2), MAX_KEYWORDLEN, szKeyWord ) ){
						return TRUE;
					}
					if( szKeyWord[0] != L'\0' ){
						/* ｎ番目のセットにキーワードを追加 */
						if( 0 == m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.AddKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, szKeyWord ) ){
							// ダイアログデータの設定 Keyword 指定キーワードセットの設定
							SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
						}
					}
					return TRUE;
				case IDC_BUTTON_EDITKEYWORD:	/* キーワード編集 */
					/* リスト中で選択されているキーワードを編集する */
					Edit_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
				case IDC_BUTTON_DELKEYWORD:	/* キーワード削除 */
					/* リスト中で選択されているキーワードを削除する */
					Delete_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
				// From Here 2005.01.26 Moca
				case IDC_BUTTON_KEYCLEAN:
					Clean_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
				// To Here 2005.01.26 Moca
				case IDC_BUTTON_IMPORT:	/* インポート */
					/* リスト中のキーワードをインポートする */
					Import_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
				case IDC_BUTTON_EXPORT:	/* エクスポート */
					/* リスト中のキーワードをエクスポートする */
					Export_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
#if defined(NKMM_FIX_KEYWORDSET_UI)
				case IDC_BUTTON_KEYWORD_RELOAD:	/* 再読込 20260802 */
					/* 選択中のセットをキーワードファイルから再読み込みする */
					Reload_List_KeyWord( hwndDlg, hwndLIST_KEYWORD );
					return TRUE;
#endif // NKMM_
				// 独立ウィンドウで使用する
				case IDOK:
					EndDialog( hwndDlg, IDOK );
					break;
				case IDCANCEL:
					EndDialog( hwndDlg, IDCANCEL );
					break;
				}
				break;	/* BN_CLICKED */
			}
		}
		break;	/* WM_COMMAND */

	case WM_TIMER:
		nIndex1 = ListView_GetNextItem( hwndLIST_KEYWORD, -1, LVNI_ALL | LVNI_SELECTED );
#if defined(NKMM_FIX_KEYWORDSET_UI)
		if( IsKeywordCsvLoaded() ){
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), FALSE );
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), FALSE );
		}else
#endif // NKMM_
		if( -1 == nIndex1 ){
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), FALSE );
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), FALSE );
		}else{
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), TRUE );
			::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), TRUE );
		}
		break;

	case WM_DESTROY:
		::KillTimer( hwndDlg, 1 );
#if defined(NKMM_FIX_KEYWORDSET_UI)
		if( m_hKeywordPreviewFont ){	// 20260802 色/フォントプレビュー用フォントの解放
			::DeleteObject( m_hKeywordPreviewFont );
			m_hKeywordPreviewFont = NULL;
		}
#endif // NKMM_
		break;

//@@@ 2001.02.04 Start by MIK: Popup Help
	case WM_HELP:
		{
			HELPINFO *p = (HELPINFO *)lParam;
			MyWinHelp( (HWND)p->hItemHandle, HELP_WM_HELP, (ULONG_PTR)(LPVOID)p_helpids );	// 2006.10.10 ryoji MyWinHelpに変更に変更
		}
		return TRUE;
		/*NOTREACHED*/
		//break;
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

/* リスト中で選択されているキーワードを編集する */
void CPropKeyword::Edit_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	int			nIndex1;
	LV_ITEM		lvi;
	wchar_t		szKeyWord[MAX_KEYWORDLEN + 1];
	CDlgInput1	cDlgInput1;

	nIndex1 = ListView_GetNextItem( hwndLIST_KEYWORD, -1, LVNI_ALL | LVNI_SELECTED );
	if( -1 == nIndex1 ){
		return;
	}
	lvi.mask = LVIF_PARAM;
	lvi.iItem = nIndex1;
	lvi.iSubItem = 0;
	ListView_GetItem( hwndLIST_KEYWORD, &lvi );

	/* ｎ番目のセットのｍ番目のキーワードを返す */
	wcscpy( szKeyWord, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, lvi.lParam ) );

	/* モードレスダイアログの表示 */
	if( !cDlgInput1.DoModal( G_AppInstance(), hwndDlg, LS(STR_PROPCOMKEYWORD_KEYEDIT1), LS(STR_PROPCOMKEYWORD_KEYEDIT2), MAX_KEYWORDLEN, szKeyWord ) ){
		return;
	}
	if( szKeyWord[0] != L'\0' ){
		/* ｎ番目のセットにキーワードを編集 */
		m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.UpdateKeyWord(
			m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx,
			lvi.lParam,
			szKeyWord
		);
	}else{
		/* ｎ番目のセットのｍ番目のキーワードを削除 */
		m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.DelKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, lvi.lParam );
	}
	/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
	SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );

	ListView_SetItemState( hwndLIST_KEYWORD, nIndex1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	return;
}



/* リスト中で選択されているキーワードを削除する */
void CPropKeyword::Delete_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	int			nIndex1;
	LV_ITEM		lvi;

	nIndex1 = ListView_GetNextItem( hwndLIST_KEYWORD, -1, LVNI_ALL | LVNI_SELECTED );
	if( -1 == nIndex1 ){
		return;
	}
	lvi.mask = LVIF_PARAM;
	lvi.iItem = nIndex1;
	lvi.iSubItem = 0;
	ListView_GetItem( hwndLIST_KEYWORD, &lvi );
	/* ｎ番目のセットのｍ番目のキーワードを削除 */
	m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.DelKeyWord( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, lvi.lParam );
	/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
	SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	ListView_SetItemState( hwndLIST_KEYWORD, nIndex1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );

	//キーワード数を表示する。
	DispKeywordCount( hwndDlg );

	return;
}


/* リスト中のキーワードをインポートする */
void CPropKeyword::Import_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	bool	bCase = false;
	int		nIdx = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx;
	m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.SetKeyWordCase( nIdx, bCase );
	CImpExpKeyWord	cImpExpKeyWord( m_Common, nIdx, bCase );

	// インポート
	if (!cImpExpKeyWord.ImportUI( G_AppInstance(), hwndDlg )) {
		// インポートをしていない
		return;
	}

	/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
	SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	return;
}


/* リスト中のキーワードをエクスポートする */
void CPropKeyword::Export_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
	SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );

	bool	bCase;
	CImpExpKeyWord	cImpExpKeyWord( m_Common, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx, bCase );

	// エクスポート
	if (!cImpExpKeyWord.ExportUI( G_AppInstance(), hwndDlg )) {
		// エクスポートをしていない
		return;
	}
}


#if defined(NKMM_FIX_KEYWORDSET_UI)
//! sakura.keywordset.csvから強調キーワードを読み込んだか 20260802
bool CPropKeyword::IsKeywordCsvLoaded()
{
	return !!GetDllShareData().m_sFlags.m_bKeywordSetLoadedFromCsv;
}

//! sakura.keywordset.csvから読み込んだ場合、強調キーワードを編集する各コントロールをDisableにする 20260802
//! (csvの内容が次回起動時に再読込・上書きされるため、ダイアログ上での編集を無効化する)
void CPropKeyword::EnableKeywordPropInput( HWND hwndDlg )
{
	if( !IsKeywordCsvLoaded() ){
		return;
	}
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_ADDSET ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELSET ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYSETRENAME ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_ADDKEYWORD ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_KEYWORDCASE ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_IMPORT ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EXPORT ), FALSE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYCLEAN ), FALSE );
}

//! 「変更」と「再読込」は同じ位置に重ねて配置しているため、どちらか一方だけを表示する 20260802
//! (再読込可能なときは「再読込」を、そうでない(通常編集/セットなし)ときは「変更」を表示する)
void CPropKeyword::SwitchKeywordRenameReloadButton( HWND hwndDlg, bool bReloadable )
{
	::ShowWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYWORD_RELOAD ), bReloadable ? SW_SHOW : SW_HIDE );
	::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYWORD_RELOAD ), bReloadable );
	::ShowWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYSETRENAME ), bReloadable ? SW_HIDE : SW_SHOW );
}

//! CSV読み込み時、「セット追加」「セット削除」を隠し、空いた場所にキーワードファイル名を表示する 20260802
//! (セットの追加・削除はCSV側で管理するため、その場所を使ってどのファイルが読み込まれているかを見せる)
void CPropKeyword::SwitchKeywordSetEditButtons( HWND hwndDlg, bool bCsvLoaded )
{
	::ShowWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_ADDSET ), bCsvLoaded ? SW_HIDE : SW_SHOW );
	::ShowWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELSET ), bCsvLoaded ? SW_HIDE : SW_SHOW );
	::ShowWindow( ::GetDlgItem( hwndDlg, IDC_STATIC_KEYWORD_FILE ), bCsvLoaded ? SW_SHOW : SW_HIDE );
}

//! セットに対応するキーワードファイル名の表示を更新する 20260802
void CPropKeyword::UpdateKeywordFileLabel( HWND hwndDlg, int nIdx )
{
	const wchar_t* pszFile = L"";
	if( 0 <= nIdx ){
		pszFile = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWordFile( nIdx );
		if( NULL == pszFile ){
			pszFile = L"";
		}
	}
	::SetWindowText( ::GetDlgItem( hwndDlg, IDC_STATIC_KEYWORD_FILE ), pszFile );
}

//! 選択中のセットをキーワードファイルから再読み込みする(セット単位) 20260802
//! (sakura.keywordset.csvで指定されたファイルの内容を反映し直す。セット自体やそれ以外のセットには影響しない)
void CPropKeyword::Reload_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	CKeyWordSetMgr& cKeyWordSetMgr = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr;
	int nIdx = cKeyWordSetMgr.m_nCurrentKeyWordSetIdx;
	if( !IsKeywordCsvLoaded() || nIdx < 0 ){
		return;
	}
	const wchar_t* pszFile = cKeyWordSetMgr.GetKeyWordFile( nIdx );
	if( NULL == pszFile || L'\0' == pszFile[0] ){
		return;
	}

	TCHAR szKeywordDir[_MAX_PATH];
	GetExedir( szKeywordDir, _T("Keyword\\") );
	std::wstring sPath = std::wstring(szKeywordDir) + pszFile;

	// 現在のキーワードを一旦空にしてからファイルを取り込み直す
	cKeyWordSetMgr.ClearKeyWord( nIdx );

	bool bCase = cKeyWordSetMgr.GetKeyWordCase( nIdx );
	CImpExpKeyWord cImpExpKeyWord( m_Common, nIdx, bCase );
	std::wstring sErrMsg;
	if( !cImpExpKeyWord.Import( sPath, sErrMsg ) ){
		::MessageBox( hwndDlg, sErrMsg.c_str(), GSTR_APPNAME, MB_OK | MB_ICONEXCLAMATION );
	}

	/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
	SetKeyWordSet( hwndDlg, nIdx );
}

//! 指定したキーワードセットに割り当てられた強調表示色・フォント属性を取得する 20260802
//! (色はNKMM_FIX_SHARED_TYPE_COLORにより常に「基本」(Types(0))の設定に統一されるが、
//!  太字・下線・フォントはタイプ別の表示/フォントを使う設定(m_bUseTypeDisp/m_bUseTypeFont)の
//!  場合はタイプごとに異なりうるため、実際にセットを参照している最初のタイプで
//!  CDocTypeManager::GetTypeConfig()を引く(色はこのAPI自身が基本にマージするので、
//!  色は結局常に基本と同じになる)。参照しているタイプが1つも無ければfalseを返す)
bool CPropKeyword::GetKeywordSetColor( int nIdx, COLORREF& crText, COLORREF& crBack, bool& bBold, bool& bUnderline, LOGFONT& lf )
{
	if( nIdx < 0 ){
		return false;
	}

	// セット番号から、参照している最初のタイプとスロット番号(強調キーワード1～10のどれか)を逆引きする
	// (タイプ別設定側で未保存のまま編集中の一時タイプ(typeId==-1)は後回しにし、実在するタイプを優先する。
	//  CPropCommon::InitData()はtypeId==-1の一時エントリ(あれば)を先頭に1件だけpushし、
	//  その後CTypeConfig(0),(1),...の順に実タイプをpushする。そのため実タイプの出現順を
	//  数えたカウンタが、そのままCTypeConfigに渡すべきインデックスと一致する。
	//  typeIdからCDocTypeManager::GetDocumentTypeOfId()で逆引きするより、この方が確実)
	bool bFoundReal = false;
	int nRealTypeConfigIdx = -1;
	int nSlot = -1;
	int nTempSlot = -1;
	int nTypeConfigIdx = 0;
	for( size_t i = 0; i < m_Types_nKeyWordSetIdx.size() && !bFoundReal; ++i ){
		const bool bIsTempEntry = ( -1 == m_Types_nKeyWordSetIdx[i].typeId );
		for( int j = 0; j < MAX_KEYWORDSET_PER_TYPE; ++j ){
			if( m_Types_nKeyWordSetIdx[i].index[j] != nIdx ){
				continue;
			}
			if( bIsTempEntry ){
				if( nTempSlot < 0 ){
					nTempSlot = j;
				}
			}else{
				nRealTypeConfigIdx = nTypeConfigIdx;
				nSlot = j;
				bFoundReal = true;
			}
			break;
		}
		if( !bIsTempEntry ){
			++nTypeConfigIdx;
		}
	}
	if( !bFoundReal && nTempSlot < 0 ){
		return false;	// どのタイプにも割り当てられていない
	}
	if( !bFoundReal ){
		nSlot = nTempSlot;	// 実在するタイプが無ければ、一時タイプのスロット番号だけ流用する(色・太字/下線は基本で代用)
	}

	STypeConfig type;
	CTypeConfig typeConfig = bFoundReal ? CTypeConfig( nRealTypeConfigIdx ) : CTypeConfig(0);
	if( !CDocTypeManager().GetTypeConfig( typeConfig, type ) ){
		return false;
	}
	const ColorInfo& info = type.m_ColorInfoArr[COLORIDX_KEYWORD1 + nSlot];
	crText = info.m_sColorAttr.m_cTEXT;
	crBack = info.m_sColorAttr.m_cBACK;
	bBold = info.m_sFontAttr.m_bBoldFont;
	bUnderline = info.m_sFontAttr.m_bUnderLine;
	// フォントは「タイプ別フォントを使う」設定の場合のみタイプ自身のものを使い、
	// それ以外は共通設定(m_Common.m_sView)のフォントを使う(CEditWnd::GetLogfont()と同じ規則)
	lf = type.m_bUseTypeFont ? type.m_lf : m_Common.m_sView.m_lf;
	return true;
}

//! プレビュー用フォントを作り直す 20260802
//! (lfBaseは共通設定/タイプ別設定の実際のフォント(GetKeywordSetColorが解決した値)。
//!  書体・太字・下線はlfBaseに合わせるが、高さ/幅はリスト自身の既定フォントのまま
//!  にする(リストの行の高さはリスト作成時のフォントで既に確定しているため、
//!  実フォントの高さをそのまま使うと行の高さに収まらず上下が欠けてしまう)。
//!  m_bKeywordSetColorValidがfalseなら何も作らずNULLのままにする=既定フォントで描画される)
void CPropKeyword::UpdateKeywordPreviewFont( HWND hwndList, const LOGFONT& lfBase )
{
	if( m_hKeywordPreviewFont ){
		::DeleteObject( m_hKeywordPreviewFont );
		m_hKeywordPreviewFont = NULL;
	}
	if( !m_bKeywordSetColorValid ){
		return;
	}

	LOGFONT lf = lfBase;
	HFONT hListFont = (HFONT)::SendMessage( hwndList, WM_GETFONT, 0, 0 );
	LOGFONT lfList;
	if( hListFont && ::GetObject( hListFont, sizeof(LOGFONT), &lfList ) ){
		lf.lfHeight = lfList.lfHeight;
		lf.lfWidth  = lfList.lfWidth;
	}
	if( m_bKeywordSetBold ){
		lf.lfWeight = FW_BOLD;
	}
	if( m_bKeywordSetUnderline ){
		lf.lfUnderline = TRUE;
	}
	m_hKeywordPreviewFont = ::CreateFontIndirect( &lf );
}
#endif // NKMM_


//! キーワードを整頓する
void CPropKeyword::Clean_List_KeyWord( HWND hwndDlg, HWND hwndLIST_KEYWORD )
{
	if( IDYES == ::MessageBox( hwndDlg, LS(STR_PROPCOMKEYWORD_DEL),
			GSTR_APPNAME, MB_YESNO | MB_ICONQUESTION ) ){	// 2009.03.26 ryoji MB_ICONSTOP->MB_ICONQUESTION
		if( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.CleanKeyWords( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx ) ){
		}
		SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	}
}

/* ダイアログデータの設定 Keyword */
void CPropKeyword::SetData( HWND hwndDlg )
{
	int		i;
	HWND	hwndWork;


	/* セット名コンボボックスの値セット */
	hwndWork = ::GetDlgItem( hwndDlg, IDC_COMBO_SET );
	Combo_ResetContent( hwndWork );  /* コンボボックスを空にする */
	if( 0 < m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nKeyWordSetNum ){
		for( i = 0; i < m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nKeyWordSetNum; ++i ){
			Combo_AddString( hwndWork, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetTypeName( i ) );
		}
		/* セット名コンボボックスのデフォルト選択 */
		Combo_SetCurSel( hwndWork, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );

		/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
		SetKeyWordSet( hwndDlg, m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	}else{
		/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
		SetKeyWordSet( hwndDlg, -1 );
	}

	return;
}


/* ダイアログデータの設定 Keyword 指定キーワードセットの設定 */
void CPropKeyword::SetKeyWordSet( HWND hwndDlg, int nIdx )
{
	int		i;
	int		nNum;
	HWND	hwndList;
	LV_ITEM	lvi;

	hwndList = ::GetDlgItem( hwndDlg, IDC_LIST_KEYWORD );
	ListView_DeleteAllItems( hwndList );

#if defined(NKMM_FIX_KEYWORDSET_UI)
	// 選択中のセットに割り当てられた強調表示色・フォント属性を取得する(プレビュー用) 20260802
	LOGFONT lfKeyword = {};
	m_bKeywordSetColorValid = GetKeywordSetColor( nIdx, m_crKeywordSetText, m_crKeywordSetBack, m_bKeywordSetBold, m_bKeywordSetUnderline, lfKeyword );
	UpdateKeywordPreviewFont( hwndList, lfKeyword );
	UpdateKeywordFileLabel( hwndDlg, nIdx );
#endif // NKMM_

	if( 0 <= nIdx ){
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELSET ), TRUE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_KEYWORDCASE ), TRUE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_LIST_KEYWORD ), TRUE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_ADDKEYWORD ), TRUE );
		//	Jan. 29, 2005 genta キーワードセット切り替え直後はキーワードは未選択
		//	そのため有効にしてすぐにタイマーで無効になる．
		//	なのでここで無効にしておく．
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_IMPORT ), TRUE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EXPORT ), TRUE );
	}else{
		::CheckDlgButton( hwndDlg, IDC_CHECK_KEYWORDCASE, FALSE );

		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELSET ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_CHECK_KEYWORDCASE ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_LIST_KEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_ADDKEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EDITKEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_DELKEYWORD ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_IMPORT ), FALSE );
		::EnableWindow( ::GetDlgItem( hwndDlg, IDC_BUTTON_EXPORT ), FALSE );
#if defined(NKMM_FIX_KEYWORDSET_UI)
		EnableKeywordPropInput( hwndDlg );
		// キーワードセットが1つもない場合は「変更」を表示する(再読込は無意味なため)
		SwitchKeywordRenameReloadButton( hwndDlg, false );
		SwitchKeywordSetEditButtons( hwndDlg, IsKeywordCsvLoaded() );
#endif // NKMM_
		return;
	}

	/* キーワードの英大文字小文字区別 */
	if( true == m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWordCase(nIdx) ){		//MIK 2000.12.01 case sense
		::CheckDlgButton( hwndDlg, IDC_CHECK_KEYWORDCASE, TRUE );
	}else{
		::CheckDlgButton( hwndDlg, IDC_CHECK_KEYWORDCASE, FALSE );
	}

	/* ｎ番目のセットのキーワードの数を返す */
	nNum = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWordNum( nIdx );

	// 2005.01.25 Moca/genta リスト追加中は再描画を抑制してすばやく表示
	::SendMessageAny( hwndList, WM_SETREDRAW, FALSE, 0 );

	for( i = 0; i < nNum; ++i ){
		/* ｎ番目のセットのｍ番目のキーワードを返す */
		const TCHAR* pszKeyWord = to_tchar(m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWord( nIdx, i ));

		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.pszText = const_cast<TCHAR*>(pszKeyWord);
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.lParam	= i;
		ListView_InsertItem( hwndList, &lvi );

	}
	m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx = nIdx;

	// 2005.01.25 Moca/genta リスト追加完了のため再描画許可
	::SendMessageAny( hwndList, WM_SETREDRAW, TRUE, 0 );

	//キーワード数を表示する。
	DispKeywordCount( hwndDlg );

#if defined(NKMM_FIX_KEYWORDSET_UI)
	EnableKeywordPropInput( hwndDlg );
	// 「再読込」は、CSVから読み込まれ、かつ対応するキーワードファイルを持つセットの時だけ有効
	// (それ以外は「変更」を表示する。同じ位置に重ねて配置しているため一方だけを見せる)
	bool bReloadable = IsKeywordCsvLoaded()
		&& L'\0' != m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWordFile( nIdx )[0];
	SwitchKeywordRenameReloadButton( hwndDlg, bReloadable );
	SwitchKeywordSetEditButtons( hwndDlg, IsKeywordCsvLoaded() );
#endif // NKMM_

	return;
}



/* ダイアログデータの取得 Keyword */
int CPropKeyword::GetData( HWND hwndDlg )
{
	return TRUE;
}

/* ダイアログデータの取得 Keyword 指定キーワードセットの取得 */
void CPropKeyword::GetKeyWordSet( HWND hwndDlg, int nIdx )
{
}

//キーワード数を表示する。
void CPropKeyword::DispKeywordCount( HWND hwndDlg )
{
	HWND	hwndList;
	int		n;
	TCHAR szCount[ 256 ];

	hwndList = ::GetDlgItem( hwndDlg, IDC_LIST_KEYWORD );
	n = ListView_GetItemCount( hwndList );
	if( n < 0 ) n = 0;

	int		nAlloc;
	nAlloc = m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetAllocSize( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	nAlloc -= m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetKeyWordNum( m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.m_nCurrentKeyWordSetIdx );
	nAlloc += m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr.GetFreeSize();
	
	auto_sprintf( szCount, LS(STR_PROPCOMKEYWORD_INFO), MAX_KEYWORDLEN, n, nAlloc );
	::SetWindowText( ::GetDlgItem( hwndDlg, IDC_STATIC_KEYWORD_COUNT ), szCount );
}


