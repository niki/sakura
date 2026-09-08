/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ウィンドウ

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
	@date 2026.09.07 CDialog依存をやめ、素のCreateWindowEx+自前WNDPROCへ全面書き直し
	@date 2026.09.07 ボーダーレスウィンドウの共通機構をwindow/CBorderlessWndへ抽出
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"

#ifdef NKMM_UNDO_HISTORY_PANEL

#include "dlg/CDlgHistoryPanel.h"
#include "Funccode_enum.h"
#include "func/CFuncLookup.h"
#include "cmd/CViewCommander.h"
#include "util/window.h"
#include "sakura_rc.h"
#include "view/CEditView.h"
#include "doc/CEditDoc.h"

namespace {
	//! 一覧の0行目(まだ何もUndoできない、ファイルを開いた時点)の固定ラベル
	const wchar_t	szInitialStateLabel[] = L"(編集開始時点)";
	//! CFuncLookup::Funccode2Name()が失敗した(名前を取れなかった)ときの代替ラベル
	const wchar_t	szUnknownOpeLabel[] = L"(操作)";

	const wchar_t	szWindowClassName[] = L"SakuraHistoryPanelWndClass";
	const wchar_t	szTitleText[]       = L"Undo履歴";
}


CDlgHistoryPanel::CDlgHistoryPanel()
	: m_hwndList( NULL )
	, m_hwndUndoBtn( NULL )
	, m_hwndRedoBtn( NULL )
	, m_hwndStatusBar( NULL )
	, m_hwndSizeGrip( NULL )
	, m_pcFuncLookup( NULL )
	, m_pcView( NULL )
	, m_bSuppressRefresh( false )
	, m_hFontItalic( NULL )
	, m_nStatusBarHeight( 0 )
{
}


/*! パネルの表示。実体はCBorderlessWnd::CreateBorderlessWindow()に委ねる
	(見た目・移動・リサイズ・親ウィンドウへの追従は基底クラスが担う)
*/
HWND CDlgHistoryPanel::DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView )
{
	m_pcFuncLookup = pcFuncLookup;
	m_pcView = pcView;
	return CreateBorderlessWindow( hInstance, hwndParent );
}


/*! アクティブなペイン/タブが切り替わったときに対象ビューを差し替える
	(CDlgFuncList::ChangeViewと同じ流儀)
*/
void CDlgHistoryPanel::ChangeView( CEditView* pcView )
{
	m_pcView = pcView;
	RefreshList();
}


/*! Undo/Redoバッファが変化した通知を受けて一覧を再構築する。ExecuteJump()の
	ループ中はm_bSuppressRefreshで抑制され、ループの最後に1回だけ実際に呼ばれる
*/
void CDlgHistoryPanel::OnUndoStackChanged()
{
	if( m_bSuppressRefresh ){
		return;
	}
	RefreshList();
}


LPCTSTR CDlgHistoryPanel::GetWindowClassName() const
{
	return szWindowClassName;
}


LPCTSTR CDlgHistoryPanel::GetTitleText() const
{
	return szTitleText;
}


void CDlgHistoryPanel::GetDefaultSize( int& nWidth, int& nHeight ) const
{
	// CDialog版だった頃の初期表示(ダイアログテンプレート220x296DLUの縦横それぞれ
	// 半分)に近い、小さめのコンパクトな大きさにする
	nWidth  = DpiScaleX( 220 );
	nHeight = DpiScaleY( 280 );
}


/*! 閉じるボタンが押された時の処理。F5キー(F_SHOWUNDOHISTORYPANEL)と同じ
	トグルコマンドを呼ぶことでこのパネルを閉じる
	(Command_SHOWUNDOHISTORYPANEL()がGetHwnd()!=NULLから::DestroyWindow()を行う経路)
*/
void CDlgHistoryPanel::OnCloseRequested()
{
	if( NULL != m_pcView ){
		m_pcView->GetCommander().HandleCommand( F_SHOWUNDOHISTORYPANEL, true, 0, 0, 0, 0 );
	}
}


/*! 子コントロール(一覧・「元に戻す」「やり直し」ボタン・飾りのステータスバー・
	サイズグリップ・閉じるボタン)の生成
*/
LRESULT CDlgHistoryPanel::OnCreate( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	CBorderlessWnd::OnCreate( hwnd, msg, wp, lp );	// フォント構築・DWMの影のセットアップ

	m_hwndList = ::CreateWindowExW( 0, WC_LISTVIEWW, L"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA | LVS_NOSORTHEADER | LVS_NOCOLUMNHEADER,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIST_UNDOHISTORY, GetAppInstance(), NULL );
	m_hwndUndoBtn = ::CreateWindowExW( 0, L"BUTTON", L"↶",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_BUTTON_HISTORYUNDO, GetAppInstance(), NULL );
	m_hwndRedoBtn = ::CreateWindowExW( 0, L"BUTTON", L"↷",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_BUTTON_HISTORYREDO, GetAppInstance(), NULL );
	m_hwndStatusBar = ::CreateStatusWindowW( WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, L"", hwnd, IDC_STATUSBAR_HISTORYPANEL );
	m_hwndSizeGrip = ::CreateWindowExW( 0, L"SCROLLBAR", L"",
		WS_CHILD | WS_VISIBLE | SBS_SIZEBOX | SBS_SIZEGRIP,
		0, 0, 0, 0, hwnd, NULL, GetAppInstance(), NULL );

	CreateCloseButton( IDC_BUTTON_HISTORYCLOSE );

	::SendMessage( m_hwndList, WM_SETFONT, (WPARAM)GetMainFont(), FALSE );
	::SendMessage( m_hwndUndoBtn, WM_SETFONT, (WPARAM)GetMainFont(), FALSE );
	::SendMessage( m_hwndRedoBtn, WM_SETFONT, (WPARAM)GetMainFont(), FALSE );

	RECT	rcStatus;
	::GetWindowRect( m_hwndStatusBar, &rcStatus );
	m_nStatusBarHeight = rcStatus.bottom - rcStatus.top;

	// LVS_EX_INFOTIP: 文字列が切れている行だけでなく、全ての行でLVN_GETINFOTIPを
	// 発生させる(このリストは全行をNM_CUSTOMDRAWの自前描画で済ませておりLVM_SETITEM
	// でテキストを設定していないため、これが無いと素の切れ表示判定が働かずツール
	// チップ自体が出ない) 20260908
	ListView_SetExtendedListViewStyleEx( m_hwndList, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP );
	LV_COLUMN	col = {};
	col.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	col.fmt      = LVCFMT_LEFT;
	col.cx       = 200;
	col.iSubItem = 0;
	ListView_InsertColumn( m_hwndList, 0, &col );

	// Redo待ち(取り消し済み)行の表示用に、既定フォントのイタリック版を用意しておく
	// (OnDestroyで破棄)
	m_hFontItalic = CreateFontVariant( m_hwndList, []( LOGFONT& lf ){
		lf.lfItalic = TRUE;
	} );

	LayoutChildren();
	RefreshList();
	return 0;
}


LRESULT CDlgHistoryPanel::OnDestroy( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	if( NULL != m_hFontItalic ){
		::DeleteObject( m_hFontItalic );
		m_hFontItalic = NULL;
	}
	m_hwndList = m_hwndUndoBtn = m_hwndRedoBtn = m_hwndStatusBar = m_hwndSizeGrip = NULL;
	return CBorderlessWnd::OnDestroy( hwnd, msg, wp, lp );
}


/*! WM_SIZEのたびに現在のクライアント矩形から全子ウィンドウの位置・大きさを
	計算し直す(初期矩形からの固定オフセット方式は使わない。閉じるボタン自身は
	基底クラス(CBorderlessWnd::OnSize())が自動配置する)
*/
void CDlgHistoryPanel::LayoutChildren()
{
	if( NULL == GetHwnd() || NULL == m_hwndList ){
		return;	// OnCreate完了前(最初のWM_SIZE)は何もしない
	}

	RECT	rcClient;
	::GetClientRect( GetHwnd(), &rcClient );
	int	nWidth  = rcClient.right - rcClient.left;
	int	nHeight = rcClient.bottom - rcClient.top;
	if( nWidth <= 0 || nHeight <= 0 ){
		return;
	}

	int	nStatusTop = nHeight - m_nStatusBarHeight;
	if( nStatusTop < GetTitleBarHeight() ){
		nStatusTop = GetTitleBarHeight();	// 極端に小さいサイズでも一覧が負にならないための安全弁
	}
	::SetWindowPos( m_hwndStatusBar, NULL, 0, nStatusTop, nWidth, m_nStatusBarHeight, SWP_NOZORDER );

	int	nBtnSize = m_nStatusBarHeight - DpiScaleY( 4 );
	if( nBtnSize < DpiScaleY( 8 ) ){
		nBtnSize = DpiScaleY( 8 );
	}
	int	nBtnY = nStatusTop + DpiScaleY( 2 );
	::SetWindowPos( m_hwndUndoBtn, HWND_TOP, DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_NOZORDER );
	::SetWindowPos( m_hwndRedoBtn, HWND_TOP, DpiScaleX( 2 ) + nBtnSize + DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_NOZORDER );

	int	nListTop    = GetTitleBarHeight() + DpiScaleY( 4 );
	int	nListLeft   = DpiScaleX( 4 );
	int	nListWidth  = nWidth - DpiScaleX( 8 );
	int	nListHeight = nStatusTop - DpiScaleY( 2 ) - nListTop;
	if( nListWidth  < 0 ) nListWidth  = 0;
	if( nListHeight < 0 ) nListHeight = 0;
	::SetWindowPos( m_hwndList, NULL, nListLeft, nListTop, nListWidth, nListHeight, SWP_NOZORDER );

	// リサイズ用の隅つまみ(サイズグリップ)は常に右下隅・最前面
	int	nGripW = ::GetSystemMetrics( SM_CXVSCROLL );
	int	nGripH = ::GetSystemMetrics( SM_CYHSCROLL );
	::SetWindowPos( m_hwndSizeGrip, HWND_TOP, nWidth - nGripW, nHeight - nGripH, nGripW, nGripH, SWP_NOZORDER );

	// 一覧(単一)列の幅を可視幅(垂直スクロールバー分を差し引いた幅)に合わせ直す
	// (合わせないと、パネルを狭くしたときに不要な水平スクロールバーが出てしまう)
	{
		RECT	rcList;
		::GetClientRect( m_hwndList, &rcList );
		int	nAvailWidth = ( rcList.right - rcList.left ) - ::GetSystemMetrics( SM_CXVSCROLL );
		if( 0 < nAvailWidth ){
			ListView_SetColumnWidth( m_hwndList, 0, nAvailWidth );
		}
	}
}


/*! 下部の「元に戻す」「やり直し」ボタン。COpeBuf::DoUndo/DoRedoは1ステップの
	ジャンプと同じ経路(HandleCommand)を使う。行クリックのExecuteJump()と違い
	1ステップだけなのでSetDrawSwitch()の抑制は不要。それ以外(閉じるボタン含む)は
	CBorderlessWnd::OnCommand()に任せる
*/
LRESULT CDlgHistoryPanel::OnCommand( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	int		nId = LOWORD( wp );
	UINT	nNotifyCode = HIWORD( wp );
	HWND	hwndCtl = (HWND)lp;

	if( NULL != hwndCtl && BN_CLICKED == nNotifyCode
	 && ( IDC_BUTTON_HISTORYUNDO == nId || IDC_BUTTON_HISTORYREDO == nId ) ){
		if( NULL != m_pcView ){
			m_pcView->GetCommander().HandleCommand(
				( IDC_BUTTON_HISTORYUNDO == nId ) ? F_UNDO : F_REDO, true, 0, 0, 0, 0 );
		}
		// BS_PUSHBUTTONの既定のクリック処理も一覧のクリック(OnNotify参照)と同様に
		// このパネル自体をアクティブ化してしまうため、同じ理由で戻す。
		RestoreEditorFocus();
		return 0;
	}
	return CBorderlessWnd::OnCommand( hwnd, msg, wp, lp );
}


/*! 一覧のNM_CLICK・ボタンのBN_CLICKEDいずれも、既定のクリック処理内で対象
	コントロールが自分自身へ::SetFocus()し、その結果WS_EX_NOACTIVATE/
	WM_MOUSEACTIVATE(MA_NOACTIVATE)を設定していてもなおこのパネル(親ウィンドウ)が
	GetForegroundWindow()になってしまう。常時表示の非対話的な参照用パネルが
	クリック1つでエディタ側のフォーカス・アクティブ状態を奪うのは避けたいため、
	明示的にエディタへ戻す。
*/
void CDlgHistoryPanel::RestoreEditorFocus()
{
	HWND	hwndParent = GetParentHwnd();
	if( NULL != hwndParent && ::IsWindow( hwndParent ) ){
		::SetForegroundWindow( hwndParent );
		::SetActiveWindow( hwndParent );
	}
	if( NULL != m_pcView ){
		::SetFocus( m_pcView->GetHwnd() );
	}
}


LRESULT CDlgHistoryPanel::OnNotify( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	NMHDR*	pNMHDR = (NMHDR*)lp;
	if( NULL == pNMHDR || IDC_LIST_UNDOHISTORY != pNMHDR->idFrom ){
		return CWnd::OnNotify( hwnd, msg, wp, lp );
	}
	if( NM_CLICK == pNMHDR->code ){
		// クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しでジャンプする
		// (Paint.NETの履歴パネルと同じ、クリック即実行のUI)
		NMITEMACTIVATE*	pNMIA = (NMITEMACTIVATE*)lp;
		if( 0 <= pNMIA->iItem ){
			ExecuteJump( pNMIA->iItem );
		}
		RestoreEditorFocus();
		return 0;
	}
	if( NM_CUSTOMDRAW == pNMHDR->code ){
		return OnListCustomDraw( lp );
	}
	if( LVN_GETINFOTIP == pNMHDR->code ){
		return OnListGetInfoTip( lp );
	}
	return 0;
}


/*! 一覧描画・ツールチップ共通のラベル文字列組み立て。行nDispIndex(0開始)は
	「ブロックnDispIndex個適用した状態」を表す(行0は固定のファイルを開いた時点)
*/
void CDlgHistoryPanel::BuildItemLabel( COpeBuf& cOpeBuf, int nDispIndex, wchar_t* pszBuf, int nBufLen ) const
{
	if( 0 == nDispIndex ){
		::lstrcpyn( pszBuf, szInitialStateLabel, nBufLen );
	}else{
		int	nFuncCode = cOpeBuf.GetBlkFuncCode( nDispIndex - 1 );
		if( NULL == m_pcFuncLookup
		 || !m_pcFuncLookup->Funccode2Name( nFuncCode, pszBuf, nBufLen )
		 || L'\0' == pszBuf[0] ){
			::lstrcpyn( pszBuf, szUnknownOpeLabel, nBufLen );
		}
	}
}


/*! 行のツールチップ(LVS_EX_INFOTIP指定によりLVN_GETINFOTIPが全行分発生する)。
	操作名のラベルに加えて、COpeBuf::GetBlkPreviewText()で実際に挿入/削除された
	文字列(先頭部分のみ)を2行目に添える。行0(編集開始時点)はプレビュー対象の
	ブロックが無いためラベルのみ 20260908
*/
LRESULT CDlgHistoryPanel::OnListGetInfoTip( LPARAM lParam )
{
	NMLVGETINFOTIPW*	pInfoTip = (NMLVGETINFOTIPW*)lParam;
	if( NULL == m_pcView || NULL == pInfoTip->pszText || pInfoTip->cchTextMax <= 0 || pInfoTip->iItem < 0 ){
		return 0;
	}
	COpeBuf&	cOpeBuf = m_pcView->GetDocument()->m_cDocEditor.m_cOpeBuf;

	wchar_t	szLabel[256];
	BuildItemLabel( cOpeBuf, pInfoTip->iItem, szLabel, _countof( szLabel ) );

	CNativeW	cmemDetail;
	if( 0 < pInfoTip->iItem && cOpeBuf.GetBlkPreviewText( pInfoTip->iItem - 1, cmemDetail ) ){
		wchar_t	szTooltip[512];
		auto_sprintf( szTooltip, L"%s\r\n%s", szLabel, cmemDetail.GetStringPtr() );
		::lstrcpyn( pInfoTip->pszText, szTooltip, pInfoTip->cchTextMax );
	}else{
		::lstrcpyn( pInfoTip->pszText, szLabel, pInfoTip->cchTextMax );
	}
	return 0;
}


/*! 一覧の描画。行i(0開始)は「ブロックi個適用した状態」を表す(行0は固定のファイルを
	開いた時点)。現在位置(COpeBuf::GetCurrentPointer())の行はハイライト表示、それより
	手前(実行済み)は通常表示、それより後ろ(Redo待ち=取り消し済み)はグレーの
	イタリックで表示する
*/
LRESULT CDlgHistoryPanel::OnListCustomDraw( LPARAM lParam )
{
	NMLVCUSTOMDRAW*	pCD = (NMLVCUSTOMDRAW*)lParam;

	switch( pCD->nmcd.dwDrawStage ){
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT:
		if( NULL == m_pcView ){
			break;
		}
		{
			COpeBuf&	cOpeBuf = m_pcView->GetDocument()->m_cDocEditor.m_cOpeBuf;
			int			nDispIndex = (int)pCD->nmcd.dwItemSpec;
			int			nCurrent = cOpeBuf.GetCurrentPointer();
			HDC			hdc = pCD->nmcd.hdc;

			// レポート表示のCDDS_ITEMPREPAINT時点ではnmcd.rcが信頼できない
			// (CDlgCommandPalette::OnListCustomDrawと同じ既知の癖)ため、
			// ListView_GetItemRect()で改めて矩形を取得する。
			RECT	rc;
			ListView_GetItemRect( m_hwndList, nDispIndex, &rc, LVIR_BOUNDS );

			bool	bCurrent = ( nDispIndex == nCurrent );
			bool	bRedoPending = ( nDispIndex > nCurrent );

			COLORREF	crBack = bCurrent ? ::GetSysColor( COLOR_HIGHLIGHT ) : ::GetSysColor( COLOR_WINDOW );
			COLORREF	crText = bCurrent ? ::GetSysColor( COLOR_HIGHLIGHTTEXT )
				: ( bRedoPending ? ::GetSysColor( COLOR_GRAYTEXT ) : ::GetSysColor( COLOR_WINDOWTEXT ) );

			FillRectWithColor( hdc, &rc, crBack );
			::SetBkMode( hdc, TRANSPARENT );
			::SetTextColor( hdc, crText );

			HFONT	hOldFont = ( bRedoPending && NULL != m_hFontItalic )
				? (HFONT)::SelectObject( hdc, m_hFontItalic ) : NULL;

			wchar_t	szLabel[256];
			BuildItemLabel( cOpeBuf, nDispIndex, szLabel, _countof( szLabel ) );

			int		nEdgePad = DpiScaleX( 8 );
			RECT	rcText = { rc.left + nEdgePad, rc.top, rc.right - nEdgePad, rc.bottom };
			::DrawText( hdc, szLabel, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS );

			if( NULL != hOldFont ){
				::SelectObject( hdc, hOldFont );
			}
		}
		return CDRF_SKIPDEFAULT;
	}
	return CDRF_DODEFAULT;
}


/*! クリックされた行(表示上のブロック適用個数)までCommand_UNDO/Command_REDOを
	1ステップずつループ呼び出しする。COpeBuf::DoUndo/DoRedoは1ステップのみのAPIしか
	持たず、キャレット・選択範囲・マルチカーソル復元まで含めて安全に移動する経路は
	HandleCommand(F_UNDO/F_REDO)経由のCommand_UNDO/Command_REDOのみのため
*/
void CDlgHistoryPanel::ExecuteJump( int nDispIndex )
{
	if( NULL == m_pcView ){
		return;
	}
	COpeBuf&	cOpeBuf = m_pcView->GetDocument()->m_cDocEditor.m_cOpeBuf;
	int			nTarget = nDispIndex;
	int			nCurrent = cOpeBuf.GetCurrentPointer();
	if( nTarget == nCurrent || nTarget < 0 || nTarget > cOpeBuf.GetBlkCount() ){
		return;
	}

	CViewCommander&	cCmd = m_pcView->GetCommander();
	bool	bOldDraw = m_pcView->GetDrawSwitch();
	bool	bUndo = ( nTarget < nCurrent );
	int		nSteps = bUndo ? ( nCurrent - nTarget ) : ( nTarget - nCurrent );
	m_bSuppressRefresh = true;

	// 中間ステップは描画を止めてまとめて飛ばす。最後の1ステップだけは元の
	// GetDrawSwitch()状態に戻してから実行し、キャレット位置表示・ステータスバー
	// 更新等(Command_UNDO/REDO内部のbDraw分岐に隠れている)を通常のUndo/Redo1回と
	// 同じように働かせる。ループ全体をSetDrawSwitch(false)のまま実行すると、
	// 最後の着地点でもステータスバーの文字数表示等が古いまま残ってしまう。
	for( int i = 0; i < nSteps; ++i ){
		if( i == nSteps - 1 ){
			m_pcView->SetDrawSwitch( bOldDraw );
		}else if( 0 == i ){
			m_pcView->SetDrawSwitch( false );
		}
		cCmd.HandleCommand( bUndo ? F_UNDO : F_REDO, false, 0, 0, 0, 0 );
	}

	m_bSuppressRefresh = false;
	RefreshList();
}


/*! COpeBuf::GetBlkCount()/GetBlkFuncCode()から一覧を作り直し、現在位置行を選択する */
void CDlgHistoryPanel::RefreshList()
{
	if( NULL == m_hwndList || NULL == m_pcView ){
		return;
	}
	COpeBuf&	cOpeBuf = m_pcView->GetDocument()->m_cDocEditor.m_cOpeBuf;
	int			nCount = cOpeBuf.GetBlkCount() + 1;	// +1: 行0(編集開始時点)
	int			nCurrent = cOpeBuf.GetCurrentPointer();

	// 新しい行数を伝える前に選択状態を全解除しておく(件数が減った場合に、もう
	// 存在しない添字の選択状態が残ったまま次回に持ち越されるのを防ぐ、
	// CDlgCommandPalette::UpdateListと同じ理由)
	ListView_SetItemState( m_hwndList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_SetItemCount( m_hwndList, nCount );
	ListView_SetItemState( m_hwndList, nCurrent, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_EnsureVisible( m_hwndList, nCurrent, FALSE );
	::InvalidateRect( m_hwndList, NULL, TRUE );

	// 下部ボタンの有効/無効も一覧と同じタイミングで更新する
	::EnableWindow( m_hwndUndoBtn, cOpeBuf.IsEnableUndo() );
	::EnableWindow( m_hwndRedoBtn, cOpeBuf.IsEnableRedo() );
}

#endif // NKMM_UNDO_HISTORY_PANEL
