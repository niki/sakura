/*!	@file
	@brief 共通タブバー(社内呼称:方式A) Step 0: 単体生成検証用の浮遊タブウィンドウ

	@date 2026.08.15 新規作成
*/
#include "StdAfx.h"
#include "CUnifiedTabWnd.h"
#include "env/CAppNodeManager.h"
#include "env/CSakuraEnvironment.h"
#include "util/window.h"

#ifdef NKMM_UNIFIED_TABBAR

// Step 0の見た目パラメータ。CTabWnd.cpp のTAB_MARGIN_*/TAB_WINDOW_HEIGHTを参考に、
// 単体ウィンドウとして分かりやすいサイズに独自設定する。
// Step 1では、UNIFIED_TAB_WND_WIDTH/X/Yは追従先が無いとき(エディタウィンドウが
// 0件)の初期値としてのみ使い、追従先があるときはTick()が対象ウィンドウの矩形で
// 上書きする。UNIFIED_TAB_WND_HEIGHTはタブバー自体の高さと、対象ウィンドウの
// 直上に配置する際の縦オフセットを兼ねる。
#define UNIFIED_TAB_MARGIN_LEFT	DpiScaleX(4)
#define UNIFIED_TAB_MARGIN_TOP		DpiScaleY(4)
#define UNIFIED_TAB_WND_WIDTH		DpiScaleX(480)
#define UNIFIED_TAB_WND_HEIGHT		DpiScaleY(32)
#define UNIFIED_TAB_WND_X			DpiScaleX(100)
#define UNIFIED_TAB_WND_Y			DpiScaleY(100)
#define UNIFIED_TAB_ITEM_WIDTH		DpiScaleX(160)
#define UNIFIED_TAB_ITEM_HEIGHT		DpiScaleY(24)
#define UNIFIED_TAB_TIMER_ID		1

// タブクリック直後、タブコントロール自身のWM_LBUTTONUP処理のコールスタックの
// 中(COMCTL32側)からSendMessage経由でTCN_SELCHANGEが届く。この中で
// ActivateFrameWindow()のような重い処理(SetForegroundWindow/クロスプロセスの
// SendMessageTimeout/DwmFlush等)を直接呼ぶと、COMCTL32がまだマウスキャプチャ中の
// 状態で他ウィンドウがフォアグラウンド化されWM_CAPTURECHANGED等が割り込む再入が
// 起こり、クラッシュ(COMCTL32.dll内でアクセス違反)する事象を実機で確認した。
// そのため実際の切替はPostMessageで遅延させ、現在のメッセージ処理が完全に
// 抜けてから実行する。
#define UNIFIED_TAB_WM_ACTIVATE		(WM_APP + 1)

CUnifiedTabWnd::CUnifiedTabWnd()
	: m_hwndTab( NULL )
	, m_hFont( NULL )
	, m_hwndTrackedActive( NULL )
	, m_pOldTabWndProc( NULL )
	, m_bDragCheck( false )
	, m_bDragging( false )
	, m_nSrcTab( -1 )
{
	m_ptSrcCursor.x = 0;
	m_ptSrcCursor.y = 0;
}

CUnifiedTabWnd::~CUnifiedTabWnd()
{
	if( m_hFont ){
		::DeleteObject( m_hFont );
		m_hFont = NULL;
	}
}

/* ウィンドウ オープン */
HWND CUnifiedTabWnd::Open( HINSTANCE hInstance, HWND hwndOwner )
{
	LPCTSTR pszClassName = _T("CUnifiedTabWnd");

	/* ウィンドウクラス作成 */
	RegisterWC(
		hInstance,
		NULL,								// Handle to the class icon.
		NULL,								// Handle to a small icon
		::LoadCursor( NULL, IDC_ARROW ),	// Handle to the class cursor.
		(HBRUSH)( COLOR_3DFACE + 1 ),		// Handle to the class background brush.
		NULL,								// menu name
		pszClassName
	);

	/* 基底クラスメンバ呼び出し。Step 1では追従先ウィンドウの直上に自動で
	   位置合わせされるため、ドラッグ可能なキャプションは付けない
	   (WS_POPUP+WS_BORDERのみ)。WS_EX_TOOLWINDOWでタスクバーには出さない */
	CWnd::Create(
		hwndOwner,
		WS_EX_TOOLWINDOW,
		pszClassName,
		_T("共通タブバー"),
		WS_POPUP | WS_BORDER,
		UNIFIED_TAB_WND_X,
		UNIFIED_TAB_WND_Y,
		UNIFIED_TAB_WND_WIDTH,
		UNIFIED_TAB_WND_HEIGHT,
		NULL
	);
	if( NULL == GetHwnd() ){
		return NULL;
	}

	RECT rcClient;
	::GetClientRect( GetHwnd(), &rcClient );

	/* タブコントロールを作成する */
	m_hwndTab = ::CreateWindow(
		WC_TABCONTROL,
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TOOLTIPS,
		UNIFIED_TAB_MARGIN_LEFT,
		UNIFIED_TAB_MARGIN_TOP,
		( rcClient.right - rcClient.left ) - UNIFIED_TAB_MARGIN_LEFT * 2,
		( rcClient.bottom - rcClient.top ) - UNIFIED_TAB_MARGIN_TOP * 2,
		GetHwnd(),
		(HMENU)NULL,
		GetAppInstance(),
		(LPVOID)NULL
	);
	if( m_hwndTab ){
		UINT lngStyle = (UINT)::GetWindowLongPtr( m_hwndTab, GWL_STYLE );
		lngStyle &= ~( TCS_BUTTONS | TCS_MULTILINE );
		lngStyle |= TCS_TABS | TCS_SINGLELINE | TCS_FOCUSNEVER | TCS_FIXEDWIDTH;
		::SetWindowLongPtr( m_hwndTab, GWL_STYLE, lngStyle );
		// 可変幅のままだとRefresh()での毎回の全件作り直しでタブ境界が微妙に
		// ずれ、古い描画が残って見える不具合があったため固定幅にする
		TabCtrl_SetItemSize( m_hwndTab, UNIFIED_TAB_ITEM_WIDTH, UNIFIED_TAB_ITEM_HEIGHT );

		/* 表示用フォント(CTabWnd::CreateMenuFontと同じ手法。メッセージフォントを流用) */
		NONCLIENTMETRICS ncm;
		ncm.cbSize = CCSIZEOF_STRUCT( NONCLIENTMETRICS, lfMessageFont );
		::SystemParametersInfo( SPI_GETNONCLIENTMETRICS, ncm.cbSize, (PVOID)&ncm, 0 );
		m_hFont = ::CreateFontIndirect( &ncm.lfMessageFont );
		::SendMessage( m_hwndTab, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM( TRUE, 0 ) );

		// Step 3: アイコン表示。CTabWnd::InitImageListと違い、独自に複製せず
		// システムの共有イメージリストをそのまま使う簡易版(自前で作った
		// ものではないため、Close()等でImageList_Destroyしないこと)
		{
			SHFILEINFO sfi;
			HIMAGELIST hImlSys = (HIMAGELIST)::SHGetFileInfo(
				_T("dummy.txt"), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
			if( hImlSys ){
				TabCtrl_SetImageList( m_hwndTab, hImlSys );
			}
		}

		// Step 2: タブのドラッグ並び替えのため、タブコントロールをサブクラス化する
		// (CTabWnd.cpp:1079-1080と同じパターン)
		::SetWindowLongPtr( m_hwndTab, GWLP_USERDATA, (LONG_PTR)this );
		m_pOldTabWndProc = (WNDPROC)::SetWindowLongPtr( m_hwndTab, GWLP_WNDPROC, (LONG_PTR)TabSubclassProc );

		::SetTimer( GetHwnd(), UNIFIED_TAB_TIMER_ID, 150, NULL );
		Tick();	// 初回の位置合わせ・タブ構築をしてから表示する(AfterCreateWindowは無効化済み)
		::ShowWindow( GetHwnd(), SW_SHOW );
	}

	return GetHwnd();
}

/* ウィンドウ作成後の処理。Step 1では初回位置合わせ(Tick())が終わるまで
   表示したくないため、基底クラスの既定動作(即ShowWindow)を止める */
void CUnifiedTabWnd::AfterCreateWindow( void )
{
}

/* ウィンドウ クローズ */
void CUnifiedTabWnd::Close( void )
{
	if( GetHwnd() ){
		::KillTimer( GetHwnd(), UNIFIED_TAB_TIMER_ID );
	}
	DestroyWindow();
	m_hwndTab = NULL;
}

/* 共有メモリの内容をタブへ反映。グループ絞り込み・アイコン・ハイライトは
   引き続き行わないが、Step 1ではlParamに対象HWNDを持たせ、
   m_hwndTrackedActiveと一致するタブを選択状態にする。
   Step 2で、内容(HWNDとキャプションの並び)が前回から変化していなければ
   TabCtrl_DeleteAllItems～再構築そのものを省略するようにした。150ms毎に
   何も変わっていなくても全削除→全挿入→背景の強制消去を繰り返していたため、
   タブが常時ちかちかして見える不具合があった */
void CUnifiedTabWnd::Refresh( void )
{
	if( NULL == m_hwndTab ){
		return;
	}

	EditNode* pEditNodeArr = NULL;
	int nRowNum = CAppNodeManager::getInstance()->GetOpenedWindowArr( &pEditNodeArr, TRUE );

	// 変化検出用の署名(HWND+キャプション+ファイルパス+未保存状態の並び)を
	// 作りつつ、選択すべきタブ番号も同時に求める
	TCHAR szBuf[64];
	std::tstring strSignature;
	int nSelectIndex = -1;
	for( int i = 0; i < nRowNum; ++i ){
		auto_sprintf( szBuf, _T("%p|"), pEditNodeArr[i].GetHwnd() );
		strSignature += szBuf;
		strSignature += pEditNodeArr[i].m_szTabCaption;
		strSignature += _T('|');
		strSignature += pEditNodeArr[i].m_szFilePath;
#ifdef NKMM_FIX_TAB_CAPTION_COLOR
		strSignature += pEditNodeArr[i].m_bIsModified? _T("|M"): _T("|-");
#endif // NKMM_
		strSignature += _T('\n');
		if( pEditNodeArr[i].GetHwnd() == m_hwndTrackedActive ){
			nSelectIndex = i;
		}
	}

	if( strSignature != m_strLastSignature ){
		TabCtrl_DeleteAllItems( m_hwndTab );

		TCITEM tcItem;
		if( 0 < nRowNum ){
			tcItem.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
			for( int i = 0; i < nRowNum; ++i ){
				// Step 3: 未保存はオーナードロー無しでも分かるよう、色変更の
				// 代わりにキャプション先頭へマーカー文字を付ける簡易対応
				TCHAR szDisplay[_MAX_PATH + 4];
				bool bModified = false;
#ifdef NKMM_FIX_TAB_CAPTION_COLOR
				bModified = pEditNodeArr[i].m_bIsModified;
#endif // NKMM_
				if( bModified ){
					auto_sprintf( szDisplay, _T("* %s"), pEditNodeArr[i].m_szTabCaption );
				}else{
					_tcscpy( szDisplay, pEditNodeArr[i].m_szTabCaption );
				}
				tcItem.pszText = szDisplay;
				tcItem.iImage  = GetIconIndex( &pEditNodeArr[i] );
				tcItem.lParam  = (LPARAM)pEditNodeArr[i].GetHwnd();
				TabCtrl_InsertItem( m_hwndTab, i, &tcItem );
			}
		}else{
			TCHAR szDummy[] = _T("(no window)");
			tcItem.mask = TCIF_TEXT;
			tcItem.pszText = szDummy;
			TabCtrl_InsertItem( m_hwndTab, 0, &tcItem );
		}
		m_strLastSignature = strSignature;

		// 全件作り直し後、古い描画がタブ境界のずれで残って見えることが
		// あったため明示的に再描画する(内容が変わったときだけ)
		::InvalidateRect( m_hwndTab, NULL, TRUE );
		::UpdateWindow( m_hwndTab );
	}

	if( 0 <= nSelectIndex && nSelectIndex != TabCtrl_GetCurSel( m_hwndTab ) ){
		TabCtrl_SetCurSel( m_hwndTab, nSelectIndex );
	}

	if( pEditNodeArr ){
		delete [] pEditNodeArr;
	}
}

/* 位置追従＋タブ再構築の共通処理。Open()の初回呼び出しとWM_TIMERの両方から呼ぶ */
void CUnifiedTabWnd::Tick( void )
{
	// 1. フォアグラウンドがサクラのメインウィンドウなら追従先を更新する
	HWND hwndFore = ::GetAncestor( ::GetForegroundWindow(), GA_ROOT );
	if( IsSakuraMainWindow( hwndFore ) ){
		m_hwndTrackedActive = hwndFore;
	}else if( m_hwndTrackedActive && !::IsWindow( m_hwndTrackedActive ) ){
		m_hwndTrackedActive = NULL;
	}

	// 2. 追従先が無ければ、開いているウィンドウの先頭を仮の追従先にする
	//    (起動直後などフォアグラウンド判定がまだ間に合っていないケースの救済)
	if( NULL == m_hwndTrackedActive ){
		EditNode* pEditNodeArr = NULL;
		int nRowNum = CAppNodeManager::getInstance()->GetOpenedWindowArr( &pEditNodeArr, TRUE );
		if( 0 < nRowNum ){
			m_hwndTrackedActive = pEditNodeArr[0].GetHwnd();
			delete [] pEditNodeArr;
		}
	}

	// 3. 追従先の直上にタブバーを配置する
	if( m_hwndTrackedActive && ::IsWindow( m_hwndTrackedActive ) ){
		RECT rc;
		::GetWindowRect( m_hwndTrackedActive, &rc );
		::SetWindowPos( GetHwnd(), NULL,
			rc.left, rc.top - UNIFIED_TAB_WND_HEIGHT,
			rc.right - rc.left, UNIFIED_TAB_WND_HEIGHT,
			SWP_NOZORDER | SWP_NOACTIVATE );
	}

	// 4. タブの内容・選択状態を更新する
	Refresh();
}

/* WM_TIMER処理 */
LRESULT CUnifiedTabWnd::OnTimer( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	if( UNIFIED_TAB_TIMER_ID != (UINT_PTR)wp ){
		return CallDefWndProc( hwnd, msg, wp, lp );
	}
	if( m_bDragging ){
		// ドラッグ並び替え中は、ポーリングによる位置追従・タブ再構築が
		// 割り込んで見た目がガタつくのを避けるためスキップする(Step 2)
		return 0;
	}
	Tick();
	return 0;
}

/* WM_NOTIFY処理。タブがユーザーにクリックされて選択が変わったときだけ
   TCN_SELCHANGEが飛んでくる(プログラムからのTabCtrl_SetCurSel等では飛ばない)。
   ここではCOMCTL32がまだ処理中のため、実際の切替は行わずPostMessageで
   自分自身に依頼するだけに留める(理由はUNIFIED_TAB_WM_ACTIVATEの定義部参照) */
LRESULT CUnifiedTabWnd::OnNotify( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	NMHDR* pnmhdr = (NMHDR*)lp;
	if( pnmhdr && TCN_SELCHANGE == pnmhdr->code && pnmhdr->hwndFrom == m_hwndTab ){
		int nSel = TabCtrl_GetCurSel( m_hwndTab );
		if( 0 <= nSel ){
			TCITEM tcItem;
			tcItem.mask = TCIF_PARAM;
			if( TabCtrl_GetItem( m_hwndTab, nSel, &tcItem ) ){
				HWND hwndTarget = (HWND)tcItem.lParam;
				if( hwndTarget && ::IsWindow( hwndTarget ) ){
					::PostMessage( GetHwnd(), UNIFIED_TAB_WM_ACTIVATE, (WPARAM)hwndTarget, 0 );
				}
			}
		}
	}
	return CallDefWndProc( hwnd, msg, wp, lp );
}

/* WM_SIZE処理。追従先ウィンドウの矩形に合わせて自分自身がリサイズされた際、
   子のタブコントロールも追従してリサイズする(Step 1で追加、以前は初回サイズの
   まま固定でずれていた) */
LRESULT CUnifiedTabWnd::OnSize( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	if( m_hwndTab ){
		RECT rcClient;
		::GetClientRect( GetHwnd(), &rcClient );
		::MoveWindow( m_hwndTab,
			UNIFIED_TAB_MARGIN_LEFT, UNIFIED_TAB_MARGIN_TOP,
			( rcClient.right - rcClient.left ) - UNIFIED_TAB_MARGIN_LEFT * 2,
			( rcClient.bottom - rcClient.top ) - UNIFIED_TAB_MARGIN_TOP * 2,
			TRUE );
	}
	return CallDefWndProc( hwnd, msg, wp, lp );
}

/* アプリケーション定義メッセージ処理。TCN_SELCHANGEのコールスタックを完全に
   抜けたあとで、実際のウィンドウ切替(ActivateFrameWindow)をここで実行する */
LRESULT CUnifiedTabWnd::DispatchEvent_WM_APP( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	if( UNIFIED_TAB_WM_ACTIVATE == msg ){
		HWND hwndTarget = (HWND)wp;
		if( hwndTarget && ::IsWindow( hwndTarget ) ){
			::ActivateFrameWindow( hwndTarget );
		}
		return 0;
	}
	return CallDefWndProc( hwnd, msg, wp, lp );
}

/* タブコントロールのサブクラスプロシージャ(Step 2)。GWLP_USERDATAから
   インスタンスを引き、ドラッグ関連のメッセージだけ横取りしてそれ以外は
   元のプロシージャ(m_pOldTabWndProc)にそのまま委譲する。CTabWnd.cpp:156-182
   のTabWndProcと同じ役割 */
LRESULT CALLBACK CUnifiedTabWnd::TabSubclassProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	CUnifiedTabWnd* pThis = (CUnifiedTabWnd*)::GetWindowLongPtr( hwnd, GWLP_USERDATA );
	if( NULL == pThis || NULL == pThis->m_pOldTabWndProc ){
		return ::DefWindowProc( hwnd, msg, wp, lp );
	}

	switch( msg ){
	case WM_LBUTTONDOWN:
		return pThis->OnTabLButtonDown( wp, lp );
	case WM_LBUTTONUP:
		return pThis->OnTabLButtonUp( wp, lp );
	case WM_MOUSEMOVE:
		return pThis->OnTabMouseMove( wp, lp );
	case WM_CAPTURECHANGED:
		// キャプチャが奪われた(Alt+Tab等)場合はドラッグ状態を強制解除してから
		// 既定の処理にも回す
		pThis->m_bDragCheck = false;
		pThis->m_bDragging = false;
		return ::CallWindowProc( pThis->m_pOldTabWndProc, hwnd, msg, wp, lp );
	}
	return ::CallWindowProc( pThis->m_pOldTabWndProc, hwnd, msg, wp, lp );
}

/* タブ部 WM_LBUTTONDOWN 処理(Step 2)。既定の処理(選択変更・キャプチャ開始)を
   先にやらせてから、並び替え用にドラッグ元タブとカーソル位置を記録するだけに
   留める。CTabWndのように既定のクリック処理を乗っ取らないため、Step 1で確認済み
   のクリック選択(TCN_SELCHANGE)の流れはそのまま活きる */
LRESULT CUnifiedTabWnd::OnTabLButtonDown( WPARAM wp, LPARAM lp )
{
	TCHITTESTINFO hitinfo;
	hitinfo.pt.x = LOWORD( (DWORD)lp );
	hitinfo.pt.y = HIWORD( (DWORD)lp );
	int nSrcTab = TabCtrl_HitTest( m_hwndTab, (LPARAM)&hitinfo );

	LRESULT lr = ::CallWindowProc( m_pOldTabWndProc, m_hwndTab, WM_LBUTTONDOWN, wp, lp );

	if( 0 <= nSrcTab ){
		m_bDragCheck = true;
		m_bDragging = false;
		m_nSrcTab = nSrcTab;
		::GetCursorPos( &m_ptSrcCursor );
	}
	return lr;
}

/* タブ部 WM_LBUTTONUP 処理(Step 2)。ドラッグ確定していた場合は自前で完結させ、
   既定の処理には回さない(誤ってクリック扱いされ選択が変わるのを防ぐ)。
   ドラッグ確定前(単なるクリック)の場合は既定の処理に委譲する */
LRESULT CUnifiedTabWnd::OnTabLButtonUp( WPARAM wp, LPARAM lp )
{
	bool bWasDragging = m_bDragging;
	m_bDragCheck = false;
	m_bDragging = false;

	if( bWasDragging ){
		if( ::GetCapture() == m_hwndTab ){
			::ReleaseCapture();
		}
		return 0L;
	}
	return ::CallWindowProc( m_pOldTabWndProc, m_hwndTab, WM_LBUTTONUP, wp, lp );
}

/* タブ部 WM_MOUSEMOVE 処理(Step 2)。ドラッグしきい値判定はCTabWnd.cpp:521-531
   と同じ考え方(SM_CXDRAG/SM_CYDRAG)。ドラッグ確定後は現在カーソル位置のタブ番号を
   都度TabCtrl_HitTestで求め、元の位置と違えばReorderTabByIndex()で並び替える
   (TCS_FIXEDWIDTHのため、CTabWndのようなタブ境界のスナップショット配列は不要) */
LRESULT CUnifiedTabWnd::OnTabMouseMove( WPARAM wp, LPARAM lp )
{
	if( !m_bDragCheck && !m_bDragging ){
		return ::CallWindowProc( m_pOldTabWndProc, m_hwndTab, WM_MOUSEMOVE, wp, lp );
	}
	if( 0 == ( wp & MK_LBUTTON ) ){
		// ボタンが離れているのにここに来た(取りこぼし等)ので状態をリセットする
		m_bDragCheck = false;
		m_bDragging = false;
		return ::CallWindowProc( m_pOldTabWndProc, m_hwndTab, WM_MOUSEMOVE, wp, lp );
	}

	if( m_bDragCheck && !m_bDragging ){
		POINT ptNow;
		::GetCursorPos( &ptNow );
		if( abs( ptNow.x - m_ptSrcCursor.x ) < ::GetSystemMetrics( SM_CXDRAG )
			&& abs( ptNow.y - m_ptSrcCursor.y ) < ::GetSystemMetrics( SM_CYDRAG ) ){
			return ::CallWindowProc( m_pOldTabWndProc, m_hwndTab, WM_MOUSEMOVE, wp, lp );
		}
		m_bDragging = true;
	}

	TCHITTESTINFO hitinfo;
	hitinfo.pt.x = LOWORD( (DWORD)lp );
	hitinfo.pt.y = HIWORD( (DWORD)lp );
	int nDstTab = TabCtrl_HitTest( m_hwndTab, (LPARAM)&hitinfo );
	if( 0 <= nDstTab && nDstTab != m_nSrcTab ){
		if( ReorderTabByIndex( m_nSrcTab, nDstTab ) ){
			m_nSrcTab = nDstTab;
			Refresh();
		}
	}
	return 0L;	// 自前でドラッグ処理したので既定処理には回さない
}

/* CTabWnd::ReorderTab(CTabWnd.cpp:723-748)と同じ薄いラッパー。タブ番号を
   HWNDに変換して、既存の安全なCAppNodeManager::ReorderTab()(ミューテックス
   保護済み、無改修)を呼ぶだけ */
bool CUnifiedTabWnd::ReorderTabByIndex( int nSrcTab, int nDstTab )
{
	if( 0 > nSrcTab || 0 > nDstTab || nSrcTab == nDstTab ){
		return false;
	}

	TCITEM tcItem;
	tcItem.mask = TCIF_PARAM;

	tcItem.lParam = 0;
	TabCtrl_GetItem( m_hwndTab, nSrcTab, &tcItem );
	HWND hwndSrc = (HWND)tcItem.lParam;

	tcItem.lParam = 0;
	TabCtrl_GetItem( m_hwndTab, nDstTab, &tcItem );
	HWND hwndDst = (HWND)tcItem.lParam;

	if( NULL == hwndSrc || NULL == hwndDst ){
		return false;
	}
	return CAppNodeManager::getInstance()->ReorderTab( hwndSrc, hwndDst );
}

/* アイコンのインデックス取得(簡易版、Step 3)。CTabWnd::GetImageIndex
   (CTabWnd.cpp:2791-2843)と違い、イメージリストを自前で複製せず、拡張子から
   得られるシステム共有イメージリストのインデックスをそのまま使う。
   ファイルパスが無い(新規未保存等)場合はアイコン無し(-1)を返す */
int CUnifiedTabWnd::GetIconIndex( EditNode* pNode )
{
	if( NULL == pNode || _T('\0') == pNode->m_szFilePath[0] ){
		return -1;
	}

	TCHAR szExt[_MAX_EXT];
	_tsplitpath( pNode->m_szFilePath, NULL, NULL, NULL, szExt );

	SHFILEINFO sfi;
	if( ::SHGetFileInfo( szExt, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
			SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES ) ){
		return sfi.iIcon;
	}
	return -1;
}

#endif // NKMM_UNIFIED_TABBAR
