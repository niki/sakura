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

	//! cr1とcr2をnRatio100:(100-nRatio100)の比率(%)で混ぜる。Redo領域の背景色を
	//! Undo領域より少し濃くするのに使う 20260908
	COLORREF BlendColor( COLORREF cr1, COLORREF cr2, int nRatio100 )
	{
		int	r = ( GetRValue( cr1 ) * nRatio100 + GetRValue( cr2 ) * ( 100 - nRatio100 ) ) / 100;
		int	g = ( GetGValue( cr1 ) * nRatio100 + GetGValue( cr2 ) * ( 100 - nRatio100 ) ) / 100;
		int	b = ( GetBValue( cr1 ) * nRatio100 + GetBValue( cr2 ) * ( 100 - nRatio100 ) ) / 100;
		return RGB( r, g, b );
	}

	//! 一覧(SysListView32)のサブクラス化前の元のプロシージャ。CDlgHistoryPanel::
	//! ListWndProc()参照。SysListView32は常に同じ既定プロシージャを持つ共有クラスの
	//! ため、CEditView_Scroll.cppのg_pOldVScrollBarWndProcと同じく単一のグローバルで
	//! 構わない(複数のパネルが同時に存在してもOldProc自体は同じ値になる) 20260908
	WNDPROC	g_pOldListWndProc = NULL;
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
	, m_hThemeListView( NULL )
	, m_nHotDispIndex( -1 )
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
	// 半分)に近い、小さめのコンパクトな大きさにする。幅はさらに、常時表示でも
	// 邪魔になりにくいようやや狭めにする(GetMinTrackSize()の既定下限180pxより
	// 少し余裕を持たせた190px) 20260908
	nWidth  = DpiScaleX( 190 );
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

	// 20260908 以前はLVS_EX_INFOTIP+LVN_GETINFOTIPで詳細をツールチップ表示していたが、
	// 狭いパネル内での位置調整(隣の行に重なる、Zオーダーでエディタの裏に回る等)や、
	// LVS_OWNERDATA+LVS_EX_INFOTIPの組み合わせ特有のListView_HitTest()キャッシュ不具合
	// など不具合が多く、ユーザーの判断でツールチップ自体を廃止。代わりに
	// OnListCustomDraw()側でラベルへ直接プレビュー文字列を追記する方式にした
	// (DT_END_ELLIPSISで自動的に省略される)
	ListView_SetExtendedListViewStyleEx( m_hwndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
	LV_COLUMN	col = {};
	col.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	col.fmt      = LVCFMT_LEFT;
	col.cx       = 200;
	col.iSubItem = 0;
	ListView_InsertColumn( m_hwndList, 0, &col );

	// 一覧のどの行も覆っていない余白部分(最後の行より下)の既定背景は白(COLOR_WINDOW)の
	// ままだと、行の描画(OnListCustomDraw()参照。Undo領域は明るいグレー)から浮いて
	// 見えるため、Undo領域と同じ色に合わせておく 20260908
	ListView_SetBkColor( m_hwndList, ::GetSysColor( COLOR_3DFACE ) );

	// 現在位置行・ホバー行を単色反転ではなく半透明の選択色で描くため、
	// CDlgCommandPaletteと同じ"Explorer::ListView"テーマを開いておく
	// (OnDestroyでCloseThemeData) 20260908
	if( CUxTheme::getInstance()->IsThemeActive() ){
		m_hThemeListView = CUxTheme::getInstance()->OpenThemeData( m_hwndList, L"Explorer::ListView" );
	}

	// マウスホバー中の行を追跡するため、一覧(SysListView32)自体をサブクラス化する。
	// WM_MOUSEMOVE/WM_MOUSELEAVEは一覧のHWNDへ直接届き、このパネル(親)のWM_NOTIFY
	// 経由では受け取れないため 20260908
	::SetWindowLongPtr( m_hwndList, GWLP_USERDATA, (LONG_PTR)this );
	g_pOldListWndProc = (WNDPROC)::SetWindowLongPtr( m_hwndList, GWLP_WNDPROC, (LONG_PTR)ListWndProc );

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
	if( NULL != m_hThemeListView ){
		CUxTheme::getInstance()->CloseThemeData( m_hThemeListView );
		m_hThemeListView = NULL;
	}
	// サブクラス化を解除してから破棄する(CEditView_Scroll.cppのスクロールバー
	// サブクラスと同じ流儀) 20260908
	if( NULL != m_hwndList && NULL != g_pOldListWndProc ){
		::SetWindowLongPtr( m_hwndList, GWLP_WNDPROC, (LONG_PTR)g_pOldListWndProc );
	}
	m_hwndList = m_hwndUndoBtn = m_hwndRedoBtn = m_hwndStatusBar = m_hwndSizeGrip = NULL;
	return CBorderlessWnd::OnDestroy( hwnd, msg, wp, lp );
}


/*! 一覧(SysListView32)のサブクラスプロシージャ。WM_MOUSEMOVE/WM_MOUSELEAVEだけを
	横取りしてホバー中の行を追跡し、それ以外は必ず元のプロシージャへ委譲する
*/
LRESULT CALLBACK CDlgHistoryPanel::ListWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	CDlgHistoryPanel*	pThis = (CDlgHistoryPanel*)::GetWindowLongPtr( hwnd, GWLP_USERDATA );
	if( NULL != pThis ){
		if( WM_MOUSEMOVE == msg ){
			pThis->OnListMouseMove( lp );
		}else if( WM_MOUSELEAVE == msg ){
			pThis->OnListMouseLeave();
		}
	}
	return ::CallWindowProc( g_pOldListWndProc, hwnd, msg, wp, lp );
}


/*! ホバー中の行が変わったら、旧/新の行だけ再描画させてm_nHotDispIndexを更新する。
	TrackMouseEvent(TME_LEAVE)は呼ぶたびに1回分のWM_MOUSELEAVE監視を仕込むだけの
	軽い呼び出しのため、WM_MOUSEMOVEのたびに呼び直して構わない(標準的な作法)

	行判定にListView_HitTest()を使わない理由: 以前はLVS_EX_INFOTIP(ツールチップ)も
	併用しており、このLVS_OWNERDATA(仮想)+LVS_EX_INFOTIP構成では、ある行の
	ツールチップが一度表示されると以後ListView_HitTest()がその行(または直前に
	ホバーしていた行)を実際より大きく(複数行分)ヒットしてしまい、すぐ下の行へ
	マウスを動かしてもヒットテストの戻り値が古い行のまま変わらなくなる不具合が
	実際に見つかった(comctl32側の内部キャッシュの問題とみられる)。ツールチップ
	自体はその後廃止したが、ペイント位置の取得に使うLVM_GETITEMRECT/
	ListView_GetItemRect()はこの影響を受けず常に正しい座標を返すため、先頭表示行
	(LVM_GETTOPINDEX)とその行の高さから算術的に行を割り出すこの実装のまま
	(動作確認済みのため)維持する 20260908
*/
void CDlgHistoryPanel::OnListMouseMove( LPARAM lParam )
{
	int	x = (short)LOWORD( lParam );
	int	y = (short)HIWORD( lParam );
	int	nHit = -1;

	int	nTopIndex = ListView_GetTopIndex( m_hwndList );
	int	nCount = ListView_GetItemCount( m_hwndList );
	RECT	rcTop;
	if( 0 < nCount && ListView_GetItemRect( m_hwndList, nTopIndex, &rcTop, LVIR_BOUNDS ) ){
		int	nItemHeight = rcTop.bottom - rcTop.top;
		if( 0 < nItemHeight && rcTop.left <= x && x < rcTop.right && rcTop.top <= y ){
			int	nCandidate = nTopIndex + ( y - rcTop.top ) / nItemHeight;
			if( nTopIndex <= nCandidate && nCandidate < nCount ){
				nHit = nCandidate;
			}
		}
	}

	if( nHit != m_nHotDispIndex ){
		InvalidateHistoryRow( m_nHotDispIndex );
		m_nHotDispIndex = nHit;
		InvalidateHistoryRow( m_nHotDispIndex );
	}

	TRACKMOUSEEVENT	tme = { sizeof( tme ), TME_LEAVE, m_hwndList, 0 };
	::TrackMouseEvent( &tme );
}


void CDlgHistoryPanel::OnListMouseLeave()
{
	InvalidateHistoryRow( m_nHotDispIndex );
	m_nHotDispIndex = -1;
}


void CDlgHistoryPanel::InvalidateHistoryRow( int nDispIndex )
{
	if( 0 <= nDispIndex ){
		ListView_RedrawItems( m_hwndList, nDispIndex, nDispIndex );
	}
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
		// (Paint.NETの履歴パネルと同じ、クリック即実行のUI)。ここではPostMessageする
		// だけに留め、実際のジャンプ(ExecuteJump〜RefreshList)はDispatchEvent_WM_APP
		// (MYWM_HISTORYPANEL_JUMP)側で行う。このNM_CLICKハンドラ自身がまだ
		// SysListView32自身のWM_LBUTTONUP処理のコールスタック上にいるため、ここで
		// 同期的にRefreshList()のListView_SetItemCount/SetItemState等を呼ぶと
		// 自分自身のリストへ再入することになり、comctl32内部でクラッシュ/ハングする
		// (実機で確認、system_constants.h MYWM_HISTORYPANEL_JUMP参照) 20260909
		NMITEMACTIVATE*	pNMIA = (NMITEMACTIVATE*)lp;
		if( 0 <= pNMIA->iItem ){
			::PostMessage( GetHwnd(), MYWM_HISTORYPANEL_JUMP, (WPARAM)pNMIA->iItem, 0 );
		}
		return 0;
	}
	if( NM_CUSTOMDRAW == pNMHDR->code ){
		return OnListCustomDraw( lp );
	}
	return 0;
}


/*! MYWM_HISTORYPANEL_JUMP(一覧クリックの実処理をNM_CLICKハンドラから遅延させた
	もの。OnNotify参照)を受け取り、実際のジャンプを行う
*/
LRESULT CDlgHistoryPanel::DispatchEvent_WM_APP( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	if( MYWM_HISTORYPANEL_JUMP == msg ){
		ExecuteJump( (int)wp );
		RestoreEditorFocus();
		return 0;
	}
	return CBorderlessWnd::DispatchEvent_WM_APP( hwnd, msg, wp, lp );
}


/*! 一覧の行表示ラベルの組み立て。行nDispIndex(0開始)は「ブロックnDispIndex個
	適用した状態」を表す(行0は固定のファイルを開いた時点)。操作名に続けて、実際に
	挿入/削除された文字列(COpeBuf::GetBlkPreviewText())を1行にまとめて追記する。
	以前はこの詳細をツールチップ(LVS_EX_INFOTIP)で見せていたが、狭いパネル内での
	位置調整(隣の行に重なる、Zオーダーでエディタの裏に回る)やLVS_OWNERDATA+
	LVS_EX_INFOTIP特有のListView_HitTest()キャッシュ不具合など、不具合が多かった
	ためツールチップ自体を廃止し、ラベルへ直接追記する方式にした。長すぎる場合は
	呼び出し側のDrawText()のDT_END_ELLIPSISで自動的に省略される 20260908
*/
void CDlgHistoryPanel::BuildItemLabel( COpeBuf& cOpeBuf, int nDispIndex, wchar_t* pszBuf, int nBufLen ) const
{
	wchar_t	szOpeName[256];
	if( 0 == nDispIndex ){
		::lstrcpyn( szOpeName, szInitialStateLabel, _countof( szOpeName ) );
	}else{
		int	nFuncCode = cOpeBuf.GetBlkFuncCode( nDispIndex - 1 );
		if( NULL == m_pcFuncLookup
		 || !m_pcFuncLookup->Funccode2Name( nFuncCode, szOpeName, _countof( szOpeName ) )
		 || L'\0' == szOpeName[0] ){
			::lstrcpyn( szOpeName, szUnknownOpeLabel, _countof( szOpeName ) );
		}
	}

	CNativeW	cmemDetail;
	if( 0 < nDispIndex && cOpeBuf.GetBlkPreviewText( nDispIndex - 1, cmemDetail ) ){
		// 20260909 実機でクラッシュを確認: auto_sprintf_s()はMSVC>=1400では
		// tchar_snprintf_s()経由でtchar_vsprintf_s_imp()に落ちるが、これは1つの
		// %sフィールドがバッファに収まらない場合、内部で使うvswprintf_s()が
		// 切り詰めではなく_invalid_parameter_internal()/_invoke_watson()を呼んで
		// そのままプロセスを異常終了させる実装だった(tchar_snprintf_sという名前
		// にもかかわらず、tchar_vsprintf_s()を呼んでおりtchar_vsnprintf_s()を
		// 呼んでいないためsnprintf相当の安全な切り詰めになっていない、既存コードの
		// 罠)。cmemDetailはGetBlkPreviewText()で最大240文字程度になり得るため、
		// この罠を確実に踏む。printf系を使わず、必ず切り詰められるlstrcpyn()と
		// (長さ0でも安全な)CNativeW::AppendString(ptr,len)だけで組み立てる
		CNativeW	cmemCombined;
		cmemCombined.AppendString( szOpeName );
		cmemCombined.AppendString( L"  " );
		cmemCombined.AppendString( cmemDetail.GetStringPtr(), cmemDetail.GetStringLength() );
		::lstrcpyn( pszBuf, cmemCombined.GetStringPtr(), nBufLen );
	}else{
		::lstrcpyn( pszBuf, szOpeName, nBufLen );
	}
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
			bool	bHot = ( !bCurrent && nDispIndex == m_nHotDispIndex );	// 現在位置行には重ねて描かない

			// Undo領域(実行済み、まだ選択されていない行)は明るいグレー、Redo領域
			// (取り消し済み)はそれより少し濃いグレー。これが下地の色になる
			COLORREF	crLightGray = ::GetSysColor( COLOR_3DFACE );
			// 20260908 ユーザー確認: 濃度40%(=crLightGray側の重み40%)だと参考画像より
			// 少し濃かったため、crLightGray側の重みを70%まで上げて薄くした
			// (BlendColor()の第3引数は第1引数側=明るい色の重みであり、下げると逆に
			// 濃くなる。一度20に下げて実機確認したところ#C0C0C0→#B0B0B0と濃くなる
			// 逆効果を招いたため70に修正) 20260908
			COLORREF	crBack = bRedoPending ? BlendColor( crLightGray, ::GetSysColor( COLOR_3DSHADOW ), 70 ) : crLightGray;
			COLORREF	crText = bRedoPending ? ::GetSysColor( COLOR_GRAYTEXT ) : ::GetSysColor( COLOR_WINDOWTEXT );

			// カレント位置行は単色反転ではなく、CDlgCommandPaletteと同じ「エクスプローラーの
			// ファイル一覧」風の半透明の選択色(LISS_SELECTED)を下地の上に重ねて描く。
			// ホバー中の行(カレント位置を除く)も同じテーマのLISS_HOT(選択色よりさらに
			// 淡い、Windows標準のホバー色)を重ねる。テーマ非対応環境(クラシックテーマ等)
			// だけ、カレント位置行に限り従来通りCOLOR_HIGHLIGHTの単色反転+白文字へ
			// フォールバックする(ホバー演出は元々「あれば嬉しい」装飾のため、非対応環境
			// では単純に無しにする) 20260908
			bool	bThemedCurrent = ( bCurrent && NULL != m_hThemeListView );
			if( bThemedCurrent || ( bHot && NULL != m_hThemeListView ) ){
				FillRectWithColor( hdc, &rc, crBack );
				CUxTheme::getInstance()->DrawThemeBackground( m_hThemeListView, hdc, LVP_LISTITEM,
					bThemedCurrent ? LISS_SELECTED : LISS_HOT, &rc, NULL );
			}else if( bCurrent ){
				crBack = ::GetSysColor( COLOR_HIGHLIGHT );
				crText = ::GetSysColor( COLOR_HIGHLIGHTTEXT );
				FillRectWithColor( hdc, &rc, crBack );
			}else{
				FillRectWithColor( hdc, &rc, crBack );
			}

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

	// Command_UNDO/REDO内部の再描画は「その1ステップ単独で見て、キャレット行が
	// 変わった/折り返しが変わった」場合にのみ全体再描画(Call_OnPaint)する条件判定に
	// なっている(CViewCommander_Edit.cpp)。最後の1ステップ(描画有効の状態で実行される
	// 唯一のステップ)がその条件に当てはまらないと、中間ステップ群(描画停止中に実際には
	// 正しく適用されている)の変更が一切画面へ反映されないまま残ってしまう
	// (データは正しいのに描画だけが古いまま、というバグ。実機で確認 20260909)。
	// 2ステップ以上飛ばした場合は、ループの後で無条件に全体を再描画して確実に反映させる。
	if( 1 < nSteps ){
		m_pcView->RedrawAll();
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
