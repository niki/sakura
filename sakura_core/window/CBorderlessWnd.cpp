/*!	@file
	@brief 「本物の」ボーダーレスウィンドウの基底クラス

	@author Yu-zuki.
	@date 2026.09.07 新規作成。NKMM_UNDO_HISTORY_PANELのCDlgHistoryPanelから抽出・汎用化
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "window/CBorderlessWnd.h"
#include "util/window.h"

namespace {
	//! ウィンドウが最大化されているか(WINDOWPLACEMENT経由。IsZoomed()と等価)
	bool IsMaximized( HWND hwnd )
	{
		WINDOWPLACEMENT	wp = { sizeof( wp ) };
		return ( ::GetWindowPlacement( hwnd, &wp ) && SW_MAXIMIZE == wp.showCmd );
	}

	/*! WM_NCCALCSIZEでクライアント矩形を「提案されたウィンドウ矩形そのまま」に
		することでOS標準のキャプション・枠の描画領域を一切確保させない場合、
		万一最大化されるとウィンドウ矩形自体がモニタの外(タスクバーの下)まで
		含んでしまうため、最大化時だけモニタの作業領域に収まるよう補正する
		(https://github.com/melak47/BorderlessWindow の adjust_maximized_client_rect相当)
	*/
	void AdjustMaximizedClientRect( HWND hwnd, RECT& rc )
	{
		if( !IsMaximized( hwnd ) ){
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
}


CBorderlessWnd::CBorderlessWnd()
	: m_hwndCloseBtn( NULL )
	, m_nCloseBtnCtrlId( 0 )
	, m_hFontMain( NULL )
	, m_nTitleBarHeight( 0 )
	, m_nCloseBtnWidth( 0 )
	, m_nWidth( -1 )
	, m_nHeight( -1 )
	, m_nOffsetX( 0 )
	, m_nOffsetY( 0 )
	, m_bParentWasMinimized( false )
{
}


CBorderlessWnd::~CBorderlessWnd()
{
}


void CBorderlessWnd::GetMinTrackSize( int& nWidth, int& nHeight ) const
{
	nWidth  = DpiScaleX( 180 );
	nHeight = DpiScaleY( 200 );
}


COLORREF CBorderlessWnd::TitleBarBackColor()
{
	return ::GetSysColor( COLOR_ACTIVECAPTION );
}


COLORREF CBorderlessWnd::TitleBarTextColor()
{
	return ::GetSysColor( COLOR_CAPTIONTEXT );
}


COLORREF CBorderlessWnd::TitleBarBackColorPressed()
{
	COLORREF	cr = TitleBarBackColor();
	return RGB( GetRValue( cr ) * 3 / 4, GetGValue( cr ) * 3 / 4, GetBValue( cr ) * 3 / 4 );
}


/*! CWnd::Create()内のAfterCreateWindow()既定実装(即座にSW_SHOWで表示する)を
	抑止する。このパネルはWS_EX_NOACTIVATEでSW_SHOWNOACTIVATE表示すべきなので、
	表示自体はCreateBorderlessWindow()側で行う
*/
void CBorderlessWnd::AfterCreateWindow()
{
}


/*! ボーダーレスウィンドウの生成。生成時からWS_CAPTION|WS_THICKFRAMEを持つ
	独立したポップアップウィンドウとして作る(見た目のキャプション・枠はWM_NCCALCSIZE/
	WM_NCHITTESTで消す。詳細はCBorderlessWnd.hのクラスコメント参照)。位置は毎回、
	親ウィンドウの右下へ配置し直す。大きさは前回覚えた値(無ければGetDefaultSize())を
	使う。
*/
HWND CBorderlessWnd::CreateBorderlessWindow( HINSTANCE hInstance, HWND hwndParent )
{
	m_nTitleBarHeight = DpiScaleY( 28 );
	m_nCloseBtnWidth  = DpiScaleX( 32 );

	ATOM	atom = RegisterWC( hInstance, NULL, NULL, ::LoadCursorW( NULL, IDC_ARROW ),
		::GetSysColorBrush( COLOR_BTNFACE ), NULL, GetWindowClassName() );
	if( 0 == atom && ERROR_CLASS_ALREADY_EXISTS != (LONG)::GetLastError() ){
		return NULL;
	}

	const DWORD	style   = WS_POPUP | WS_CLIPCHILDREN | WS_THICKFRAME | WS_CAPTION;
	const DWORD	exStyle = WS_EX_NOACTIVATE;

	int	nWidth, nHeight;
	if( 0 < m_nWidth && 0 < m_nHeight ){
		nWidth  = m_nWidth;
		nHeight = m_nHeight;
	}else{
		GetDefaultSize( nWidth, nHeight );
	}

	int	x, y;
	ComputeBottomRightPosition( nWidth, nHeight, x, y );

	HWND	hwnd = Create( hwndParent, exStyle, GetWindowClassName(), GetTitleText(), style,
		x, y, nWidth, nHeight, NULL );
	if( NULL == hwnd ){
		return NULL;
	}

	m_bParentWasMinimized = ( 0 != ::IsIconic( hwndParent ) );
	// FollowParentWindow()が使う追従用の相対オフセットを、今置いた既定位置(右下)を
	// 基準に初期化する(WM_MOVEでも同じ値に更新されるはずだが、明示しておく)
	UpdateOffsetFromCurrentPosition();

	::ShowWindow( hwnd, SW_SHOWNOACTIVATE );
	return hwnd;
}


/*! 親ウィンドウ(エディタ)矩形の右下(20pxマージン)にこのウィンドウを配置すると
	した場合のx,yを計算する。表示するたびの既定位置(CreateBorderlessWindow())に
	のみ使う。以後の追従(FollowParentWindow())はここへ毎回スナップし直すのではなく、
	m_nOffsetX/Yに基づく相対追従にする(ユーザーがドラッグした位置を尊重するため)。
*/
void CBorderlessWnd::ComputeBottomRightPosition( int nWidth, int nHeight, int& x, int& y ) const
{
	RECT	rcParent;
	::GetWindowRect( GetParentHwnd(), &rcParent );
	x = rcParent.right  - nWidth  - DpiScaleX( 20 );
	y = rcParent.bottom - nHeight - DpiScaleY( 20 );
}


/*! 現在のこのウィンドウの位置と親ウィンドウの位置の差をm_nOffsetX/Yへ記録する。
	このウィンドウ自身のWM_MOVEのたびに呼ばれる(ユーザーがタイトル帯をドラッグして
	動かした場合も、FollowParentWindow()が追従のため動かした場合も、結果として
	「今の位置」を基準に記録し直すだけなので、そのつどの原因を区別する必要がない)
*/
void CBorderlessWnd::UpdateOffsetFromCurrentPosition()
{
	HWND	hwndParent = GetParentHwnd();
	if( NULL == GetHwnd() || NULL == hwndParent || !::IsWindow( hwndParent ) ){
		return;
	}
	RECT	rcSelf, rcParent;
	::GetWindowRect( GetHwnd(), &rcSelf );
	::GetWindowRect( hwndParent, &rcParent );
	m_nOffsetX = rcSelf.left - rcParent.left;
	m_nOffsetY = rcSelf.top  - rcParent.top;
}


/*! 親ウィンドウの移動・リサイズ・最小化/復元に位置・表示状態を追従させる。
	親ウィンドウ側のWM_MOVE/WM_SIZEハンドラから呼ぶ(CDlgFind::FollowParentWindow()と
	同様の外側からの呼び出しパターン)。
*/
void CBorderlessWnd::FollowParentWindow()
{
	HWND	hwndParent = GetParentHwnd();
	if( NULL == GetHwnd() || NULL == hwndParent || !::IsWindow( hwndParent ) ){
		return;
	}

	bool	bIsMinimizedNow = ( 0 != ::IsIconic( hwndParent ) );
	if( bIsMinimizedNow != m_bParentWasMinimized ){
		::ShowWindow( GetHwnd(), bIsMinimizedNow ? SW_HIDE : SW_SHOWNOACTIVATE );
		m_bParentWasMinimized = bIsMinimizedNow;
	}
	if( bIsMinimizedNow ){
		return;	// 最小化中は位置を動かす必要が無い
	}

	// 親ウィンドウとの相対オフセット(m_nOffsetX/Y、ユーザーがタイトル帯をドラッグして
	// 動かした位置を反映している)を保ったまま追従させる。表示のたびの既定位置(右下)へ
	// 毎回スナップし直すわけではない。
	RECT	rcParent;
	::GetWindowRect( hwndParent, &rcParent );
	::SetWindowPos( GetHwnd(), NULL, rcParent.left + m_nOffsetX, rcParent.top + m_nOffsetY, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
}


/*! 閉じるボタンをBS_OWNERDRAWで生成する */
HWND CBorderlessWnd::CreateCloseButton( int nCtrlId )
{
	m_nCloseBtnCtrlId = nCtrlId;
	m_hwndCloseBtn = ::CreateWindowExW( 0, L"BUTTON", L"×",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		0, 0, 0, 0, GetHwnd(), (HMENU)(INT_PTR)nCtrlId, GetAppInstance(), NULL );
	return m_hwndCloseBtn;
}


/*! ウィンドウプロシージャのメッセージ配送。CWndの標準ディスパッチ(WM_CREATE/
	WM_SIZE/WM_MOVE/WM_COMMAND/WM_NOTIFY/WM_DRAWITEM/WM_DESTROY等、いずれも
	仮想On*()メソッド経由)に加えて、CWndが素通りする非クライアント関連メッセージ
	(WM_NCCALCSIZE/WM_NCHITTEST/WM_ERASEBKGND/WM_MOUSEACTIVATE/WM_GETMINMAXINFO)を
	ここで横取りする。
*/
LRESULT CBorderlessWnd::DispatchEvent( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg ){
	case WM_NCCALCSIZE:
		// wParam==FALSEは::CreateWindowEx()の内部で最初の1回だけ送られてくる
		// (lParamは素のRECT*で、この時点の提案クライアント矩形をそのまま採用すれば
		// よい)。ここをTRUEの場合と同じく「変更しない」で処理しないと、生成直後の
		// 初回表示だけ本物のキャプション・枠のぶんクライアント矩形が縮められて
		// しまい、最初のリサイズでWM_NCCALCSIZE(TRUE)が飛んでくるまで本物のタイトル
		// バーが自前のタイトル帯の上に二重に見えてしまう(実機で発見された実バグ)。
		if( TRUE == wp ){
			AdjustMaximizedClientRect( hwnd, ( (NCCALCSIZE_PARAMS*)lp )->rgrc[0] );
		}
		return 0;

	case WM_NCHITTEST:
		{
			POINT	ptScreen = { (int)(short)LOWORD( lp ), (int)(short)HIWORD( lp ) };
			return HitTest( ptScreen );
		}

	case WM_ERASEBKGND:
		return OnEraseBkgnd( (HDC)wp );

	case WM_MOUSEACTIVATE:
		return OnMouseActivate();

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO*	pmmi = (MINMAXINFO*)lp;
			int	nMinWidth, nMinHeight;
			GetMinTrackSize( nMinWidth, nMinHeight );
			pmmi->ptMinTrackSize.x = nMinWidth;
			pmmi->ptMinTrackSize.y = nMinHeight;
		}
		return 0;

	case WM_NCDESTROY:
		{
			// CWndはWM_NCDESTROYを特別扱いしない(自分のデストラクタでしかm_hWndを
			// NULLに戻さない)。このパネルはCEditWndのメンバーとして表示/非表示の
			// たびにDestroyWindow()されるだけでC++オブジェクト自体は生き続けるため、
			// ここで明示的に_SetHwnd(NULL)しておかないとGetHwnd()が破棄後も古い
			// (無効な)ハンドルを返し続け、「一度閉じると二度と開けない」という
			// 実バグになる(LayoutUndoHistoryPanel()等の「NULL==GetHwnd()なら
			// 新規作成」判定が常にfalseになってしまうため)。
			LRESULT	lResult = ::DefWindowProc( hwnd, msg, wp, lp );
			_SetHwnd( NULL );
			return lResult;
		}
	}
	return CWnd::DispatchEvent( hwnd, msg, wp, lp );
}


/*! 背景+タイトル帯(色付き背景+タイトル文字)の描画。子ウィンドウは別途自分自身の
	WM_PAINTで描画され、ここでは上書きされない
*/
LRESULT CBorderlessWnd::OnEraseBkgnd( HDC hdc )
{
	RECT	rcClient;
	::GetClientRect( GetHwnd(), &rcClient );
	FillRectWithColor( hdc, &rcClient, ::GetSysColor( COLOR_BTNFACE ) );

	RECT	rcTitle = { rcClient.left, rcClient.top, rcClient.right, rcClient.top + m_nTitleBarHeight };
	FillRectWithColor( hdc, &rcTitle, TitleBarBackColor() );

	::SetBkMode( hdc, TRANSPARENT );
	::SetTextColor( hdc, TitleBarTextColor() );
	HFONT	hOldFont = (HFONT)::SelectObject( hdc, m_hFontMain );

	RECT	rcText = rcTitle;
	rcText.left  += DpiScaleX( 8 );
	rcText.right -= ( NULL != m_hwndCloseBtn ) ? ( m_nCloseBtnWidth + DpiScaleX( 8 ) ) : DpiScaleX( 8 );
	::DrawText( hdc, GetTitleText(), -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS );

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
LRESULT CBorderlessWnd::HitTest( POINT ptScreen ) const
{
	RECT	rcWindow;
	if( !::GetWindowRect( GetHwnd(), &rcWindow ) ){
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
			::ScreenToClient( GetHwnd(), &ptClient );
			return ( ptClient.y < m_nTitleBarHeight ) ? HTCAPTION : HTCLIENT;
		}
	default:
		return HTNOWHERE;
	}
}


/*! MA_NOACTIVATEでクリック自体は素通ししつつアクティブ化だけを止める(子コントロールの
	クリックはSetFocus()する経路でなお暗黙にアクティブ化してしまうことがあるため、
	派生クラス側でも必要に応じて明示的にフォーカスを戻すこと)。

	ただしBUTTON(Buttonウィンドウクラス)の子は、sakuraプロセス全体が非アクティブな
	状態からの最初のクリックだと、MA_NOACTIVATEを返してもクリックがボタンの通常の
	クリック処理(WM_LBUTTONDOWN→BN_CLICKED)まで届かず、ウィンドウをアクティブ化する
	だけで消費されてしまう(非アクティブ状態で1回目クリック→何も起きない、2回目で
	初めて実行される、という2度押しになる)。ボタンを押したら常に即座に動作させる
	ため、WM_MOUSEACTIVATEの時点(クリックが消費される前)でカーソル位置がBUTTONクラス
	の子の上かを判定し、その場で自前でBN_CLICKEDを合成して即時実行したうえで
	MA_NOACTIVATEANDEATを返し、本来のクリックメッセージは握りつぶす(ウィンドウが
	既にアクティブな場合はWM_MOUSEACTIVATE自体が飛んでこないため通常のBN_CLICKED
	経路と二重実行にはならない)。コントロールIDではなくウィンドウクラス名で判定する
	ため、閉じるボタンに限らず派生クラスが追加した任意のボタンにも自動的に効く。
*/
LRESULT CBorderlessWnd::OnMouseActivate()
{
	POINT	pt;
	::GetCursorPos( &pt );
	::ScreenToClient( GetHwnd(), &pt );
	HWND	hChild = ::ChildWindowFromPointEx( GetHwnd(), pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED );
	if( NULL != hChild ){
		TCHAR	szClass[32];
		::GetClassName( hChild, szClass, _countof( szClass ) );
		if( 0 == ::lstrcmpi( szClass, _T("Button") ) ){
			int	nCtrlId = ::GetDlgCtrlID( hChild );
			::SendMessage( GetHwnd(), WM_COMMAND, MAKEWPARAM( nCtrlId, BN_CLICKED ), (LPARAM)hChild );
			return MA_NOACTIVATEANDEAT;
		}
	}
	return MA_NOACTIVATE;
}


/*! 子コントロールの生成(基底クラス分)。ダイアログテンプレートを使わないため、
	フォントもsakura標準のダイアログフォント相当(9pt、NKMM_RES_FONT_NAME)を
	自前で構築する(CFuncKeyWnd.cppの表示用フォント構築と同じ流儀)。DWMの合成が
	有効なら1pxの余白をクライアント領域へ食い込ませ、非クライアント領域が実質0でも
	本来のウィンドウの影(ドロップシャドウ)を描画させる(BorderlessWindowの
	set_shadow()相当。見た目上の「細い枠」はこの影で表現し、自前でWS_BORDER等を
	描く必要は無い)。派生クラスは自分のOnCreate()内でこれを先に呼ぶこと。
*/
LRESULT CBorderlessWnd::OnCreate( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
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

	{
		BOOL	bCompositionEnabled = FALSE;
		if( SUCCEEDED( ::DwmIsCompositionEnabled( &bCompositionEnabled ) ) && bCompositionEnabled ){
			MARGINS	margins = { 1, 1, 1, 1 };
			::DwmExtendFrameIntoClientArea( hwnd, &margins );
		}
	}

	return 0;
}


LRESULT CBorderlessWnd::OnDestroy( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	// 次回表示時の既定サイズにするため、閉じる直前の大きさを覚えておく
	RECT	rc;
	if( ::GetWindowRect( hwnd, &rc ) ){
		m_nWidth  = rc.right - rc.left;
		m_nHeight = rc.bottom - rc.top;
	}

	if( NULL != m_hFontMain ){
		::DeleteObject( m_hFontMain );
		m_hFontMain = NULL;
	}
	m_hwndCloseBtn = NULL;

	return 0;
}


/*! WM_SIZEのたびに、閉じるボタン(基底が管理)を右上に固定配置しつつ、派生クラスの
	LayoutChildren()を呼んで残りの子コントロールを配置させる
*/
LRESULT CBorderlessWnd::OnSize( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	RECT	rcClient;
	::GetClientRect( hwnd, &rcClient );
	int	nWidth = rcClient.right - rcClient.left;

	if( NULL != m_hwndCloseBtn && 0 < nWidth ){
		::SetWindowPos( m_hwndCloseBtn, HWND_TOP,
			nWidth - m_nCloseBtnWidth, 0, m_nCloseBtnWidth, m_nTitleBarHeight, SWP_NOZORDER );
	}

	LayoutChildren();

	::InvalidateRect( hwnd, NULL, TRUE );
	return 0;
}


/*! このウィンドウ自身が動いた(ユーザーがタイトル帯をドラッグした、または
	FollowParentWindow()自身が追従のため動かした)たびに、親ウィンドウとの相対
	オフセットを記録し直す。FollowParentWindow()が動かした場合も、動かした後の
	位置=(親位置+既存オフセット)から同じオフセットが再計算されるだけなので
	ズレは生じない。
*/
LRESULT CBorderlessWnd::OnMove( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	UpdateOffsetFromCurrentPosition();
	return 0;
}


/*! 閉じるボタンのクリックを処理する(基底クラス分)。それ以外のIDは派生クラスの
	OnCommand()が処理する
*/
LRESULT CBorderlessWnd::OnCommand( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	int		nId = LOWORD( wp );
	UINT	nNotifyCode = HIWORD( wp );
	HWND	hwndCtl = (HWND)lp;
	if( NULL != hwndCtl && BN_CLICKED == nNotifyCode && nId == m_nCloseBtnCtrlId ){
		OnCloseRequested();
		return 0;
	}
	return CWnd::OnCommand( hwnd, msg, wp, lp );
}


/*! 閉じるボタン(BS_OWNERDRAW)の描画(基底クラス分)。タイトル帯(OnEraseBkgnd)と
	地続きに見えるよう同系色で塗り、押下中(ODS_SELECTED)だけ暗くして押下フィード
	バックを出す。それ以外のIDは派生クラスのOnDrawItem()が処理する
*/
LRESULT CBorderlessWnd::OnDrawItem( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	LPDRAWITEMSTRUCT	pDIS = (LPDRAWITEMSTRUCT)lp;
	if( (int)pDIS->CtlID != m_nCloseBtnCtrlId ){
		return CWnd::OnDrawItem( hwnd, msg, wp, lp );
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
