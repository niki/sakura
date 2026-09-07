/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ウィンドウ

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
	@date 2026.09.07 CDialog依存をやめ、素のCreateWindowEx+自前WNDPROCへ全面書き直し
		(詳細はCDlgHistoryPanel.hのクラスコメント参照) // NKMM_UNDO_HISTORY_PANEL
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"

#ifdef NKMM_UNDO_HISTORY_PANEL

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

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

	//! 自前タイトルバーの見た目(WM_ERASEBKGNDで塗る)。色は決め打ちにせず、OSの現在の
	//! 配色(GetSysColor(COLOR_ACTIVECAPTION)等)をそのつど問い合わせる。CDlgFuncList
	//! (アウトライン解析)のドッキング時タイトル描画も同じ流儀で、これによりWindowsの
	//! 「タイトルバーにアクセントカラーを表示する」設定に自動追従する(この設定が
	//! オンだと、素のWS_CAPTIONダイアログも含め全ウィンドウのタイトルバーがこの色に
	//! なる。オフなら既定の白系配色になる)。
	const wchar_t	szTitleBarText[] = L"Undo履歴";

	COLORREF TitleBarBackColor()
	{
		return ::GetSysColor( COLOR_ACTIVECAPTION );
	}
	COLORREF TitleBarTextColor()
	{
		return ::GetSysColor( COLOR_CAPTIONTEXT );
	}
	//! 閉じるボタン押下中のフィードバック用に、タイトル帯の色を少し暗くする
	COLORREF TitleBarBackColorPressed()
	{
		COLORREF	cr = TitleBarBackColor();
		return RGB( GetRValue( cr ) * 3 / 4, GetGValue( cr ) * 3 / 4, GetBValue( cr ) * 3 / 4 );
	}

	const wchar_t		szHistoryPanelWndClass[] = L"SakuraHistoryPanelWndClass";	// NKMM_UNDO_HISTORY_PANEL


	//! ウィンドウが最大化されているか(WINDOWPLACEMENT経由。IsZoomed()と等価)
	bool HistoryPanelIsMaximized( HWND hwnd )
	{
		WINDOWPLACEMENT	wp = { sizeof( wp ) };
		return ( ::GetWindowPlacement( hwnd, &wp ) && SW_MAXIMIZE == wp.showCmd );
	}


	/*! WM_NCCALCSIZEでクライアント矩形を「提案されたウィンドウ矩形そのまま」に
		することでOS標準のキャプション・枠の描画領域を一切確保させない場合、
		万一最大化されるとウィンドウ矩形自体がモニタの外(タスクバーの下)まで
		含んでしまうため、最大化時だけモニタの作業領域に収まるよう補正する
		(https://github.com/melak47/BorderlessWindow の adjust_maximized_client_rect相当。
		このパネルに最大化ボタンは無いが、Win+↑等で最大化され得るための保険)
	*/
	void HistoryPanelAdjustMaximizedClientRect( HWND hwnd, RECT& rc )
	{
		if( !HistoryPanelIsMaximized( hwnd ) ){
			return;
		}
		HMONITOR	hMonitor = ::MonitorFromWindow( hwnd, MONITOR_DEFAULTTONULL );
		if( NULL == hMonitor ){
			return;
		}
		MONITORINFO	mi = { sizeof( mi ) };
		if( ::GetMonitorInfoW( hMonitor, &mi ) ){
			rc = mi.rcWork;
		}
	}


	/*! ウィンドウクラスの登録(プロセス内で一度だけ。sakuraのタブは別プロセスなので
		プロセスごとの登録で十分)
	*/
	ATOM RegisterHistoryPanelClass( HINSTANCE hInstance )
	{
		static ATOM	atom = 0;
		if( 0 == atom ){
			WNDCLASSEXW	wcx = {};
			wcx.cbSize        = sizeof( wcx );
			wcx.style         = CS_HREDRAW | CS_VREDRAW;
			wcx.lpfnWndProc   = CDlgHistoryPanel::WndProc;
			wcx.hInstance     = hInstance;
			wcx.hCursor       = ::LoadCursorW( NULL, IDC_ARROW );
			wcx.hbrBackground = ::GetSysColorBrush( COLOR_BTNFACE );
			wcx.lpszClassName = szHistoryPanelWndClass;
			atom = ::RegisterClassExW( &wcx );
		}
		return atom;
	}
}


CDlgHistoryPanel::CDlgHistoryPanel()
	: m_hWnd( NULL )
	, m_hwndParent( NULL )
	, m_hwndList( NULL )
	, m_hwndUndoBtn( NULL )
	, m_hwndRedoBtn( NULL )
	, m_hwndCloseBtn( NULL )
	, m_hwndStatusBar( NULL )
	, m_hwndSizeGrip( NULL )
	, m_hInstance( NULL )
	, m_pcFuncLookup( NULL )
	, m_pcView( NULL )
	, m_bSuppressRefresh( false )
	, m_hFontMain( NULL )
	, m_hFontItalic( NULL )
	, m_nTitleBarHeight( 0 )
	, m_nCloseBtnWidth( 0 )
	, m_nStatusBarHeight( 0 )
	, m_nWidth( -1 )
	, m_nHeight( -1 )
	, m_nOffsetX( 0 )
	, m_nOffsetY( 0 )
	, m_bParentWasMinimized( false )
{
}


CDlgHistoryPanel::~CDlgHistoryPanel()
{
	if( NULL != m_hWnd && ::IsWindow( m_hWnd ) ){
		::DestroyWindow( m_hWnd );
	}
}


/*! 親ウィンドウ(エディタ)矩形の右下(20pxマージン)にこのパネルを配置するとした
	場合のx,yを計算する。表示するたびの既定位置(DoModeless())にのみ使う。
	以後の追従(FollowParentWindow())はここへ毎回スナップし直すのではなく、
	m_nOffsetX/Yに基づく相対追従にする(ユーザーがドラッグした位置を尊重するため。
	UpdateOffsetFromCurrentPosition()参照)。
*/
void CDlgHistoryPanel::ComputeBottomRightPosition( int nWidth, int nHeight, int& x, int& y ) const
{
	RECT	rcParent;
	::GetWindowRect( m_hwndParent, &rcParent );
	x = rcParent.right  - nWidth  - DpiScaleX( 20 );
	y = rcParent.bottom - nHeight - DpiScaleY( 20 );
}


/*! 現在のこのパネルの位置と親ウィンドウの位置の差をm_nOffsetX/Yへ記録する。
	このパネル自身のWM_MOVEのたびに呼ばれる(ユーザーがタイトル帯をドラッグして
	動かした場合も、FollowParentWindow()が追従のため動かした場合も、結果として
	「今の位置」を基準に記録し直すだけなので、そのつどの原因を区別する必要がない)
*/
void CDlgHistoryPanel::UpdateOffsetFromCurrentPosition()
{
	if( NULL == m_hWnd || NULL == m_hwndParent || !::IsWindow( m_hwndParent ) ){
		return;
	}
	RECT	rcSelf, rcParent;
	::GetWindowRect( m_hWnd, &rcSelf );
	::GetWindowRect( m_hwndParent, &rcParent );
	m_nOffsetX = rcSelf.left - rcParent.left;
	m_nOffsetY = rcSelf.top  - rcParent.top;
}


/*! パネルの表示。生成時からWS_CAPTION|WS_THICKFRAMEを持つ独立したポップアップ
	ウィンドウとして作る(見た目のキャプション・枠はWM_NCCALCSIZE/WM_NCHITTESTで
	消す。詳細はCDlgHistoryPanel.hのクラスコメント参照)。位置は毎回、親ウィンドウ
	(エディタ)の右下へ配置し直す。大きさは前回覚えた値(無ければ既定値)を使う。
*/
HWND CDlgHistoryPanel::DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView )
{
	m_pcFuncLookup = pcFuncLookup;
	m_pcView = pcView;
	m_hwndParent = hwndParent;
	m_hInstance = hInstance;

	RegisterHistoryPanelClass( hInstance );

	m_nTitleBarHeight = DpiScaleY( 28 );
	m_nCloseBtnWidth  = DpiScaleX( 32 );

	const DWORD	style   = WS_POPUP | WS_CLIPCHILDREN | WS_THICKFRAME | WS_CAPTION;
	const DWORD	exStyle = WS_EX_NOACTIVATE;

	// 既定サイズはCDialog版だった頃の初期表示(ダイアログテンプレート220x296DLUの
	// 縦横それぞれ半分)に近い、小さめのコンパクトな大きさにする
	int	nWidth  = ( 0 < m_nWidth  ) ? m_nWidth  : DpiScaleX( 220 );
	int	nHeight = ( 0 < m_nHeight ) ? m_nHeight : DpiScaleY( 280 );

	int	x, y;
	ComputeBottomRightPosition( nWidth, nHeight, x, y );

	HWND	hwnd = ::CreateWindowExW(
		exStyle, szHistoryPanelWndClass, L"Undo履歴", style,
		x, y, nWidth, nHeight,
		hwndParent, NULL, hInstance, this );
	if( NULL == hwnd ){
		return NULL;
	}

	m_bParentWasMinimized = ( 0 != ::IsIconic( hwndParent ) );
	// FollowParentWindow()が使う追従用の相対オフセットを、今置いた既定位置(右下)
	// を基準に初期化する(WM_MOVEでも同じ値に更新されるはずだが、明示しておく)
	UpdateOffsetFromCurrentPosition();

	::ShowWindow( hwnd, SW_SHOWNOACTIVATE );
	return hwnd;
}


/*! 親ウィンドウ(エディタ本体)の移動・リサイズ・最小化/復元にパネルの位置・表示
	状態を追従させる。CEditWnd::WM_MOVE/WM_SIZEハンドラから呼ばれる(CDlgFindの
	FollowParentWindow()と同じ、既存の呼び出し口)。
*/
void CDlgHistoryPanel::FollowParentWindow()
{
	if( NULL == m_hWnd || NULL == m_hwndParent || !::IsWindow( m_hwndParent ) ){
		return;
	}

	bool	bIsMinimizedNow = ( 0 != ::IsIconic( m_hwndParent ) );
	if( bIsMinimizedNow != m_bParentWasMinimized ){
		::ShowWindow( m_hWnd, bIsMinimizedNow ? SW_HIDE : SW_SHOWNOACTIVATE );
		m_bParentWasMinimized = bIsMinimizedNow;
	}
	if( bIsMinimizedNow ){
		return;	// 最小化中は位置を動かす必要が無い
	}

	// 親ウィンドウとの相対オフセット(m_nOffsetX/Y、ユーザーがタイトル帯をドラッグして
	// 動かした位置を反映している。UpdateOffsetFromCurrentPosition()参照)を保ったまま
	// 追従させる。表示のたびの既定位置(右下)へ毎回スナップし直すわけではない。
	RECT	rcParent;
	::GetWindowRect( m_hwndParent, &rcParent );
	::SetWindowPos( m_hWnd, NULL, rcParent.left + m_nOffsetX, rcParent.top + m_nOffsetY, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
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


/*! ウィンドウプロシージャ。WM_NCCREATE時にCREATESTRUCT::lpCreateParams(=DoModeless()
	に渡したthis)をGWLP_USERDATAへ格納し、以後はそこから読み戻して振り分ける。
	ダイアログテンプレート/DLGPROCを経由しないため、最初のWM_NCCALCSIZEから
	一貫してHandleMessage()側で処理できる(CDialog版で必要だった「生成後に
	WS_CAPTION|WS_THICKFRAMEを付与してSWP_FRAMECHANGEDで矯正する」回避策が丸ごと
	不要になる)。
*/
LRESULT CALLBACK CDlgHistoryPanel::WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	CDlgHistoryPanel*	pThis;
	if( WM_NCCREATE == uMsg ){
		CREATESTRUCTW*	pcs = (CREATESTRUCTW*)lParam;
		pThis = (CDlgHistoryPanel*)pcs->lpCreateParams;
		pThis->m_hWnd = hwnd;
		::SetWindowLongPtrW( hwnd, GWLP_USERDATA, (LONG_PTR)pThis );
	}else{
		pThis = (CDlgHistoryPanel*)::GetWindowLongPtrW( hwnd, GWLP_USERDATA );
	}
	if( NULL != pThis ){
		return pThis->HandleMessage( hwnd, uMsg, wParam, lParam );
	}
	return ::DefWindowProcW( hwnd, uMsg, wParam, lParam );
}


LRESULT CDlgHistoryPanel::HandleMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg ){
	case WM_NCCALCSIZE:
		// wParam==FALSEは::CreateWindowEx()の内部で最初の1回だけ送られてくる
		// (lParamは素のRECT*で、この時点の提案クライアント矩形をそのまま採用すれば
		// よい)。ここをTRUEの場合と同じく「変更しない」で処理しないと、生成直後の
		// 初回表示だけ本物のキャプション・枠のぶんクライアント矩形が縮められて
		// しまい、最初のリサイズでWM_NCCALCSIZE(TRUE)が飛んでくるまで本物のタイトル
		// バーが自前のタイトル帯の上に二重に見えてしまう(実機で発見された実バグ)。
		if( TRUE == wParam ){
			HistoryPanelAdjustMaximizedClientRect( hwnd, ( (NCCALCSIZE_PARAMS*)lParam )->rgrc[0] );
		}
		return 0;

	case WM_NCHITTEST:
		{
			POINT	ptScreen = { (int)(short)LOWORD( lParam ), (int)(short)HIWORD( lParam ) };
			return HitTest( ptScreen );
		}

	case WM_ERASEBKGND:
		return OnEraseBkgnd( (HDC)wParam );

	case WM_MOUSEACTIVATE:
		return OnMouseActivate();

	case WM_CREATE:
		OnCreate();
		return 0;

	case WM_SIZE:
		LayoutChildren();
		return 0;

	case WM_MOVE:
		// このパネル自身が動いた(ユーザーがタイトル帯をドラッグした、または
		// FollowParentWindow()自身が追従のため動かした)たびに、親ウィンドウとの
		// 相対オフセットを記録し直す。FollowParentWindow()が動かした場合も
		// 動かした後の位置=(親位置+既存オフセット)から同じオフセットが再計算
		// されるだけなのでズレは生じない。
		UpdateOffsetFromCurrentPosition();
		break;	// 既定処理へ(WM_MOVEは戻り値を使わない)

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO*	pmmi = (MINMAXINFO*)lParam;
			pmmi->ptMinTrackSize.x = DpiScaleX( 180 );
			pmmi->ptMinTrackSize.y = DpiScaleY( 200 );
		}
		return 0;

	case WM_COMMAND:
		OnCommandMsg( LOWORD( wParam ), (HWND)lParam, HIWORD( wParam ) );
		return 0;

	case WM_NOTIFY:
		return OnNotifyMsg( lParam );

	case WM_DRAWITEM:
		return OnDrawItemMsg( lParam );

	case WM_DESTROY:
		OnDestroyWindow();
		return 0;

	case WM_NCDESTROY:
		{
			LRESULT	lResult = ::DefWindowProcW( hwnd, uMsg, wParam, lParam );
			m_hWnd = NULL;
			return lResult;
		}
	}
	return ::DefWindowProcW( hwnd, uMsg, wParam, lParam );
}


/*! 子ウィンドウの生成。ダイアログテンプレートを使わないため、フォントも
	sakura標準のダイアログフォント相当(9pt、NKMM_RES_FONT_NAME)を自前で構築して
	WM_SETFONTで配る(CFuncKeyWnd.cppの表示用フォント構築と同じ流儀)
*/
void CDlgHistoryPanel::OnCreate()
{
	{
		LOGFONTW	lf = {};
		lf.lfHeight        = DpiPointsToPixels( -9 );
		lf.lfWeight        = FW_NORMAL;
		lf.lfCharSet       = DEFAULT_CHARSET;
		lf.lfOutPrecision  = OUT_TT_ONLY_PRECIS;
		lf.lfQuality       = DEFAULT_QUALITY;
		lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		::lstrcpynW( lf.lfFaceName, L"" NKMM_RES_FONT_NAME, _countof( lf.lfFaceName ) );
		m_hFontMain = ::CreateFontIndirectW( &lf );
	}

	m_hwndList = ::CreateWindowExW( 0, WC_LISTVIEWW, L"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA | LVS_NOSORTHEADER | LVS_NOCOLUMNHEADER,
		0, 0, 0, 0, m_hWnd, (HMENU)(INT_PTR)IDC_LIST_UNDOHISTORY, m_hInstance, NULL );
	m_hwndUndoBtn = ::CreateWindowExW( 0, L"BUTTON", L"↶",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
		0, 0, 0, 0, m_hWnd, (HMENU)(INT_PTR)IDC_BUTTON_HISTORYUNDO, m_hInstance, NULL );
	m_hwndRedoBtn = ::CreateWindowExW( 0, L"BUTTON", L"↷",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
		0, 0, 0, 0, m_hWnd, (HMENU)(INT_PTR)IDC_BUTTON_HISTORYREDO, m_hInstance, NULL );
	m_hwndCloseBtn = ::CreateWindowExW( 0, L"BUTTON", L"×",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		0, 0, 0, 0, m_hWnd, (HMENU)(INT_PTR)IDC_BUTTON_HISTORYCLOSE, m_hInstance, NULL );
	m_hwndStatusBar = ::CreateStatusWindowW( WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, L"", m_hWnd, IDC_STATUSBAR_HISTORYPANEL );
	m_hwndSizeGrip = ::CreateWindowExW( 0, L"SCROLLBAR", L"",
		WS_CHILD | WS_VISIBLE | SBS_SIZEBOX | SBS_SIZEGRIP,
		0, 0, 0, 0, m_hWnd, NULL, m_hInstance, NULL );

	::SendMessage( m_hwndList, WM_SETFONT, (WPARAM)m_hFontMain, FALSE );
	::SendMessage( m_hwndUndoBtn, WM_SETFONT, (WPARAM)m_hFontMain, FALSE );
	::SendMessage( m_hwndRedoBtn, WM_SETFONT, (WPARAM)m_hFontMain, FALSE );

	RECT	rcStatus;
	::GetWindowRect( m_hwndStatusBar, &rcStatus );
	m_nStatusBarHeight = rcStatus.bottom - rcStatus.top;

	ListView_SetExtendedListViewStyleEx( m_hwndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );
	LV_COLUMN	col = {};
	col.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	col.fmt      = LVCFMT_LEFT;
	col.cx       = 200;
	col.iSubItem = 0;
	ListView_InsertColumn( m_hwndList, 0, &col );

	// Redo待ち(取り消し済み)行の表示用に、m_hFontMainのイタリック版を用意しておく
	// (OnDestroyWindowで破棄)
	m_hFontItalic = CreateFontVariant( m_hwndList, []( LOGFONT& lf ){
		lf.lfItalic = TRUE;
	} );

	// DWMの合成が有効なら1pxの余白をクライアント領域へ食い込ませ、非クライアント
	// 領域が実質0でも本来のウィンドウの影(ドロップシャドウ)を描画させる
	// (BorderlessWindowのset_shadow()相当)。見た目上の「細い枠」はこの影で表現する。
	{
		BOOL	bCompositionEnabled = FALSE;
		if( SUCCEEDED( ::DwmIsCompositionEnabled( &bCompositionEnabled ) ) && bCompositionEnabled ){
			MARGINS	margins = { 1, 1, 1, 1 };
			::DwmExtendFrameIntoClientArea( m_hWnd, &margins );
		}
	}

	LayoutChildren();
	RefreshList();
}


void CDlgHistoryPanel::OnDestroyWindow()
{
	// 次回表示時の既定サイズにするため、閉じる直前の大きさを覚えておく
	RECT	rc;
	if( ::GetWindowRect( m_hWnd, &rc ) ){
		m_nWidth  = rc.right - rc.left;
		m_nHeight = rc.bottom - rc.top;
	}

	if( NULL != m_hFontItalic ){
		::DeleteObject( m_hFontItalic );
		m_hFontItalic = NULL;
	}
	if( NULL != m_hFontMain ){
		::DeleteObject( m_hFontMain );
		m_hFontMain = NULL;
	}

	m_hwndList = m_hwndUndoBtn = m_hwndRedoBtn = m_hwndCloseBtn = m_hwndStatusBar = m_hwndSizeGrip = NULL;
}


/*! WM_SIZEのたびに現在のクライアント矩形から全子ウィンドウの位置・大きさを
	計算し直す(CDialog::ResizeItem()のような「初期矩形からの固定オフセット」
	方式は使わない。DPI/DLU換算やダイアログ初回縮小との食い違いでレイアウトが
	ズレる実例が旧CDialog版であったため、常に「今のクライアント矩形」だけを
	根拠にする単純な計算にしている)
*/
void CDlgHistoryPanel::LayoutChildren()
{
	if( NULL == m_hWnd || NULL == m_hwndList ){
		return;	// OnCreate完了前(最初のWM_SIZE)は何もしない
	}

	RECT	rcClient;
	::GetClientRect( m_hWnd, &rcClient );
	int	nWidth  = rcClient.right - rcClient.left;
	int	nHeight = rcClient.bottom - rcClient.top;
	if( nWidth <= 0 || nHeight <= 0 ){
		return;
	}

	::SetWindowPos( m_hwndCloseBtn, HWND_TOP,
		nWidth - m_nCloseBtnWidth, 0, m_nCloseBtnWidth, m_nTitleBarHeight, SWP_NOZORDER );

	int	nStatusTop = nHeight - m_nStatusBarHeight;
	if( nStatusTop < m_nTitleBarHeight ){
		nStatusTop = m_nTitleBarHeight;	// 極端に小さいサイズでも一覧が負にならないための安全弁
	}
	::SetWindowPos( m_hwndStatusBar, NULL, 0, nStatusTop, nWidth, m_nStatusBarHeight, SWP_NOZORDER );

	int	nBtnSize = m_nStatusBarHeight - DpiScaleY( 4 );
	if( nBtnSize < DpiScaleY( 8 ) ){
		nBtnSize = DpiScaleY( 8 );
	}
	int	nBtnY = nStatusTop + DpiScaleY( 2 );
	::SetWindowPos( m_hwndUndoBtn, HWND_TOP, DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_NOZORDER );
	::SetWindowPos( m_hwndRedoBtn, HWND_TOP, DpiScaleX( 2 ) + nBtnSize + DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_NOZORDER );

	int	nListTop    = m_nTitleBarHeight + DpiScaleY( 4 );
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

	::InvalidateRect( m_hWnd, NULL, TRUE );
}


/*! 背景+自前タイトル帯(色付き背景+タイトル文字)の描画。一覧・ボタン等は
	子ウィンドウなので別途自分自身のWM_PAINTで描画され、ここでは上書きされない
*/
LRESULT CDlgHistoryPanel::OnEraseBkgnd( HDC hdc )
{
	RECT	rcClient;
	::GetClientRect( m_hWnd, &rcClient );
	FillRectWithColor( hdc, &rcClient, ::GetSysColor( COLOR_BTNFACE ) );

	RECT	rcTitle = { rcClient.left, rcClient.top, rcClient.right, rcClient.top + m_nTitleBarHeight };
	FillRectWithColor( hdc, &rcTitle, TitleBarBackColor() );

	::SetBkMode( hdc, TRANSPARENT );
	::SetTextColor( hdc, TitleBarTextColor() );
	HFONT	hOldFont = (HFONT)::SelectObject( hdc, m_hFontMain );

	RECT	rcText = rcTitle;
	rcText.left  += DpiScaleX( 8 );
	rcText.right -= m_nCloseBtnWidth + DpiScaleX( 8 );	// 右端の閉じるボタンぶんの余白
	::DrawText( hdc, szTitleBarText, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS );

	::SelectObject( hdc, hOldFont );
	return TRUE;
}


/*! WM_NCHITTESTのカスタム判定。ウィンドウ矩形(スクリーン座標)の外周
	SM_CXFRAME+SM_CXPADDEDBORDER相当の帯をリサイズ枠として扱い、それ以外は
	クライアント座標に変換したうえでタイトル帯(m_nTitleBarHeight)の範囲内なら
	HTCAPTION(ドラッグ移動)、それ以外はHTCLIENTを返す。一覧やボタン等の子
	ウィンドウ上の点は、この関数に来る前に子ウィンドウ自身がHTCLIENTを返して
	確定するため、ここに来る時点で「子ウィンドウの無い領域」であることが
	保証されている(https://github.com/melak47/BorderlessWindow のhit_test()相当)
*/
LRESULT CDlgHistoryPanel::HitTest( POINT ptScreen ) const
{
	RECT	rcWindow;
	if( !::GetWindowRect( m_hWnd, &rcWindow ) ){
		return HTNOWHERE;
	}

	POINT	border = {
		::GetSystemMetrics( SM_CXFRAME ) + ::GetSystemMetrics( SM_CXPADDEDBORDER ),
		::GetSystemMetrics( SM_CYFRAME ) + ::GetSystemMetrics( SM_CXPADDEDBORDER )
	};

	enum { REGION_CLIENT = 0, REGION_LEFT = 1, REGION_RIGHT = 2, REGION_TOP = 4, REGION_BOTTOM = 8 };
	int	nRegion =
		( ptScreen.x <  rcWindow.left   + border.x ? REGION_LEFT   : 0 ) |
		( ptScreen.x >= rcWindow.right  - border.x ? REGION_RIGHT  : 0 ) |
		( ptScreen.y <  rcWindow.top    + border.y ? REGION_TOP    : 0 ) |
		( ptScreen.y >= rcWindow.bottom - border.y ? REGION_BOTTOM : 0 );

	switch( nRegion ){
	case REGION_LEFT:                    return HTLEFT;
	case REGION_RIGHT:                   return HTRIGHT;
	case REGION_TOP:                     return HTTOP;
	case REGION_BOTTOM:                  return HTBOTTOM;
	case REGION_TOP    | REGION_LEFT:    return HTTOPLEFT;
	case REGION_TOP    | REGION_RIGHT:   return HTTOPRIGHT;
	case REGION_BOTTOM | REGION_LEFT:    return HTBOTTOMLEFT;
	case REGION_BOTTOM | REGION_RIGHT:   return HTBOTTOMRIGHT;
	case REGION_CLIENT:
		{
			POINT	ptClient = ptScreen;
			::ScreenToClient( m_hWnd, &ptClient );
			return ( ptClient.y < m_nTitleBarHeight ) ? HTCAPTION : HTCLIENT;
		}
	default:
		return HTNOWHERE;
	}
}


/*! MA_NOACTIVATEでクリック自体は素通ししつつアクティブ化だけを止める(一覧の
	クリックはSysListView32が自分自身へ::SetFocus()する経路でなお暗黙に
	アクティブ化してしまうため、OnNotifyMsg側でも明示的にエディタへフォーカスを
	戻している)。

	ただし元に戻す/やり直し/閉じるの各ボタンは、sakuraプロセス全体が非アクティブな
	状態からの最初のクリックだと、MA_NOACTIVATEを返してもクリックがボタンの通常の
	クリック処理(WM_LBUTTONDOWN→BN_CLICKED)まで届かず、ウィンドウをアクティブ化
	するだけで消費されてしまう(非アクティブ状態で1回目クリック→何も起きない、
	2回目で初めて実行される、という2度押しになる)。ボタンを押したら常に即座に
	動作させるため、WM_MOUSEACTIVATEの時点(クリックが消費される前)でカーソル位置が
	ボタン上かを判定し、その場で自前でBN_CLICKEDを合成して即時実行したうえで
	MA_NOACTIVATEANDEATを返し、本来のクリックメッセージは握りつぶす(パネルが
	既にアクティブな場合はWM_MOUSEACTIVATE自体が飛んでこないため通常のBN_CLICKED
	経路と二重実行にはならない)。
*/
LRESULT CDlgHistoryPanel::OnMouseActivate()
{
	POINT	pt;
	::GetCursorPos( &pt );
	::ScreenToClient( m_hWnd, &pt );
	HWND	hChild = ::ChildWindowFromPointEx( m_hWnd, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED );
	int		nCtrlId = ( NULL != hChild ) ? ::GetDlgCtrlID( hChild ) : 0;
	if( IDC_BUTTON_HISTORYUNDO == nCtrlId || IDC_BUTTON_HISTORYREDO == nCtrlId
	 || IDC_BUTTON_HISTORYCLOSE == nCtrlId ){
		::SendMessage( m_hWnd, WM_COMMAND, MAKEWPARAM( nCtrlId, BN_CLICKED ), (LPARAM)hChild );
		return MA_NOACTIVATEANDEAT;
	}
	return MA_NOACTIVATE;
}


/*! 下部の「元に戻す」「やり直し」ボタン、および自前タイトルバーの閉じるボタン。
	COpeBuf::DoUndo/DoRedoは1ステップのジャンプと同じ経路(HandleCommand)を使う。
	行クリックのExecuteJump()と違い1ステップだけなのでSetDrawSwitch()の抑制は不要
*/
void CDlgHistoryPanel::OnCommandMsg( int wID, HWND hwndCtl, UINT notifyCode )
{
	if( NULL == hwndCtl || BN_CLICKED != notifyCode ){
		return;
	}

	if( IDC_BUTTON_HISTORYUNDO == wID || IDC_BUTTON_HISTORYREDO == wID ){
		if( NULL != m_pcView ){
			m_pcView->GetCommander().HandleCommand(
				( IDC_BUTTON_HISTORYUNDO == wID ) ? F_UNDO : F_REDO, true, 0, 0, 0, 0 );
		}
		// 既定のクリック処理も一覧のクリック(OnNotifyMsg参照)と同様にこのパネル
		// 自体をアクティブ化してしまうため、同じ理由で戻す。
		RestoreEditorFocus();
		return;
	}

	if( IDC_BUTTON_HISTORYCLOSE == wID ){
		// F5キー(F_SHOWUNDOHISTORYPANEL)と同じトグルコマンドを呼ぶことで、
		// このパネルを閉じる(Command_SHOWUNDOHISTORYPANEL()がGetHwnd()!=NULLから
		// ::DestroyWindow()を行う経路。このパネル自身のWM_COMMAND処理から自分自身の
		// DestroyWindow()を呼ぶことになるが、HandleMessage()はOnCommandMsg()から
		// 戻った直後にreturnするだけでhwndに一切触れないため安全)。パネルは
		// WS_EX_NOACTIVATE/MA_NOACTIVATEANDEATによりこのクリックでエディタ側の
		// フォーカスを奪っていないため、Undo/Redoボタンと異なりRestoreEditorFocus()
		// は不要。
		if( NULL != m_pcView ){
			m_pcView->GetCommander().HandleCommand( F_SHOWUNDOHISTORYPANEL, true, 0, 0, 0, 0 );
		}
		return;
	}
}


/*! 自前タイトルバーの閉じるボタン(IDC_BUTTON_HISTORYCLOSE、BS_OWNERDRAW)の描画。
	タイトル帯(OnEraseBkgnd)と地続きに見えるよう同系色で塗り、押下中
	(ODS_SELECTED)だけ暗くして押下フィードバックを出す
*/
LRESULT CDlgHistoryPanel::OnDrawItemMsg( LPARAM lParam )
{
	LPDRAWITEMSTRUCT	pDIS = (LPDRAWITEMSTRUCT)lParam;
	if( IDC_BUTTON_HISTORYCLOSE != pDIS->CtlID ){
		return FALSE;
	}

	bool	bPressed = ( 0 != ( pDIS->itemState & ODS_SELECTED ) );
	FillRectWithColor( pDIS->hDC, &pDIS->rcItem, bPressed ? TitleBarBackColorPressed() : TitleBarBackColor() );

	::SetBkMode( pDIS->hDC, TRANSPARENT );
	::SetTextColor( pDIS->hDC, TitleBarTextColor() );
	HFONT	hOldFont = (HFONT)::SelectObject( pDIS->hDC, m_hFontMain );
	::DrawText( pDIS->hDC, L"×", -1, &pDIS->rcItem, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
	::SelectObject( pDIS->hDC, hOldFont );
	return TRUE;
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
	if( NULL != m_hwndParent && ::IsWindow( m_hwndParent ) ){
		::SetForegroundWindow( m_hwndParent );
		::SetActiveWindow( m_hwndParent );
	}
	if( NULL != m_pcView ){
		::SetFocus( m_pcView->GetHwnd() );
	}
}


LRESULT CDlgHistoryPanel::OnNotifyMsg( LPARAM lParam )
{
	NMHDR*	pNMHDR = (NMHDR*)lParam;
	if( NULL == pNMHDR || IDC_LIST_UNDOHISTORY != pNMHDR->idFrom ){
		return 0;
	}
	if( NM_CLICK == pNMHDR->code ){
		// クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しでジャンプする
		// (Paint.NETの履歴パネルと同じ、クリック即実行のUI)
		NMITEMACTIVATE*	pNMIA = (NMITEMACTIVATE*)lParam;
		if( 0 <= pNMIA->iItem ){
			ExecuteJump( pNMIA->iItem );
		}
		RestoreEditorFocus();
		return 0;
	}
	if( NM_CUSTOMDRAW == pNMHDR->code ){
		return OnListCustomDraw( lParam );
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
			if( 0 == nDispIndex ){
				::lstrcpyn( szLabel, szInitialStateLabel, _countof( szLabel ) );
			}else{
				int	nFuncCode = cOpeBuf.GetBlkFuncCode( nDispIndex - 1 );
				if( NULL == m_pcFuncLookup
				 || !m_pcFuncLookup->Funccode2Name( nFuncCode, szLabel, _countof( szLabel ) )
				 || L'\0' == szLabel[0] ){
					::lstrcpyn( szLabel, szUnknownOpeLabel, _countof( szLabel ) );
				}
			}

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
