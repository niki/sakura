/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ダイアログボックス

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
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
}

/*! ダイアログのサブクラスプロシージャ。WM_MOUSEACTIVATEを横取りし、このパネルが
	クリックでアクティブ化されてエディタ側からフォーカスを奪わないようにする。
*/
static LRESULT CALLBACK HistoryPanelDlgSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	if( WM_MOUSEACTIVATE == uMsg ){
		// MA_NOACTIVATEでクリック自体は素通ししつつアクティブ化だけを止める
		// (一覧のクリックはSysListView32が自分自身へ::SetFocus()する経路でなお暗黙に
		// アクティブ化してしまうため、OnNotify側でも明示的にエディタへフォーカスを
		// 戻している)。
		//
		// ただし元に戻す/やり直しボタンは、sakuraプロセス全体が非アクティブな状態から
		// の最初のクリックだと、MA_NOACTIVATEを返してもクリックがボタンの通常の
		// クリック処理(WM_LBUTTONDOWN→BN_CLICKED)まで届かず、ウィンドウをアクティブ
		// 化するだけで消費されてしまう(非アクティブ状態で1回目クリック→何も起きない、
		// 2回目で初めてUndo/Redoが実行される、という2度押しになる)。ボタンを押したら
		// 常に即座に動作させるため、WM_MOUSEACTIVATEの時点(クリックが消費される前)で
		// カーソル位置がボタン上かを判定し、その場で自前でBN_CLICKEDを合成して即時
		// 実行したうえでMA_NOACTIVATEANDEATを返し、本来のクリックメッセージは握り
		// つぶす(パネルが既にアクティブな場合はWM_MOUSEACTIVATE自体が飛んでこないため
		// 通常のBN_CLICKED経路と二重実行にはならない)。
		POINT	pt;
		::GetCursorPos( &pt );
		::ScreenToClient( hwnd, &pt );
		HWND	hChild = ::ChildWindowFromPointEx( hwnd, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED );
		int		nCtrlId = ( NULL != hChild ) ? ::GetDlgCtrlID( hChild ) : 0;
		if( IDC_BUTTON_HISTORYUNDO == nCtrlId || IDC_BUTTON_HISTORYREDO == nCtrlId ){
			::SendMessage( hwnd, WM_COMMAND, MAKEWPARAM( nCtrlId, BN_CLICKED ), (LPARAM)hChild );
			return MA_NOACTIVATEANDEAT;
		}
		return MA_NOACTIVATE;
	}else if( WM_NCDESTROY == uMsg ){
		::RemoveWindowSubclass( hwnd, HistoryPanelDlgSubclassProc, uIdSubclass );
	}
	return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
}


CDlgHistoryPanel::CDlgHistoryPanel()
	: CDialog( true )
	, m_pcFuncLookup( NULL )
	, m_pcView( NULL )
	, m_bSuppressRefresh( false )
	, m_hFontItalic( NULL )
	, m_bParentWasMinimized( false )
	, m_hwndTrueParent( NULL )
{
	m_ptDefaultSize.x = 0;
	m_ptDefaultSize.y = 0;
	::SetRectEmpty( &m_rcListDefault );
}


/*! モードレスダイアログの表示。CDialog(true)によりタイトルバー・サイズ変更枠を
	持つ通常の可変ダイアログとして扱われ、ユーザーが自由に移動・リサイズできる
	(初期位置・サイズの決定はOnInitDialog参照)
*/
HWND CDlgHistoryPanel::DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView )
{
	m_pcFuncLookup = pcFuncLookup;
	m_pcView = pcView;

	// CDialog::DoModeless()はCreateDialogParam()がWM_INITDIALOGを同期的に配送するため、
	// OnInitDialog()完了後に戻ってくる。そのOnInitDialog()の初回配置ロジックが使う
	// 「正しい」オーナーを、下記のResolveDialogOwnerWindow()による誤補正より前に
	// 確定させておく必要があるため、m_hwndParentへの反映を待たずここで控えておく。
	m_hwndTrueParent = hwndParent;

	HWND	hWnd = CDialog::DoModeless( hInstance, hwndParent, IDD_DLG_UNDOHISTORY, 0, SW_HIDE );
	if( NULL != hWnd ){
		// CDialog::DoModeless()内部のResolveDialogOwnerWindow()(NKMM_FIX_DIALOG_OWNER)は、
		// 渡されたhwndParentがその時点で非表示ならGetForegroundWindow()に「補正」して
		// しまう。トリガーとなった操作元ウィンドウが不明瞭なダイアログ/メッセージ
		// ボックス向けの救済策だが、このパネルは常に所属するCEditWndがhwndParent引数
		// として明確にわかっているため、この補正はむしろ有害(タブ切替直後などで
		// hwndParentが一時的に非表示だと無関係な別ウィンドウ―別プロセスのことも
		// ある―がオーナーにされ、以後::ShowOwnedPopups()によるタブ切替時の自動
		// 非表示が本来のオーナーには効かなくなり、パネルが元のタブの位置に取り残
		// される)。補正結果を明示的に上書きして確定させる。
		::SetWindowLongPtr( hWnd, GWLP_HWNDPARENT, (LONG_PTR)hwndParent );
		m_hwndParent = hwndParent;	// RestoreEditorFocus()/SetPlaceOfWindow()等が参照する側も合わせて訂正する

		// WM_MOUSEACTIVATE(HistoryPanelDlgSubclassProc)でMA_NOACTIVATEを返すだけでは
		// 不十分だった。一覧(SysListView32)は既定のクリック処理内で自分自身へ
		// ::SetFocus()するが、Win32のSetFocus()は対象がアクティブでないトップレベル
		// ウィンドウに属する場合、MA_NOACTIVATEの有無に関わらずそのトップレベルを
		// 暗黙にアクティブ化してしまう。WS_EX_NOACTIVATEを立てるとこの暗黙アクティブ化
		// 自体が起きなくなる(NM_CLICK自体はヒットテストで届くため行クリックの
		// ジャンプ機能には影響しない)。
		LONG_PTR	exStyle = ::GetWindowLongPtr( hWnd, GWL_EXSTYLE );
		::SetWindowLongPtr( hWnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE );
		::ShowWindow( hWnd, SW_SHOWNOACTIVATE );
	}
	return hWnd;
}


/*! 親ウィンドウ(エディタ本体)の最小化/復元にパネルの表示状態を追従させる。
	位置は毎回の表示時にOnInitDialog()が既定位置へ配置し直すので、ここでは
	追従させない。
*/
void CDlgHistoryPanel::FollowParentWindow()
{
	if( NULL == GetHwnd() || NULL == m_hwndParent || !::IsWindow( m_hwndParent ) ){
		return;
	}

	bool	bIsMinimizedNow = ( 0 != ::IsIconic( m_hwndParent ) );
	if( bIsMinimizedNow != m_bParentWasMinimized ){
		::ShowWindow( GetHwnd(), bIsMinimizedNow ? SW_HIDE : SW_SHOWNOACTIVATE );
		m_bParentWasMinimized = bIsMinimizedNow;
	}
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


BOOL CDlgHistoryPanel::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	// CDlgWindowList(サイズ変更可能ダイアログの他の実例)と同じ手順: 先に_SetHwnd()して
	// 自前の初期化を済ませ、CDialog::OnInitDialog()(SetData/SetDialogPosSize等を行う)は
	// 最後にまとめて呼ぶ。ただしDWLP_USERだけはここで先に設定しておく必要がある。
	// MyDialogProc()はWM_INITDIALOG以外の全メッセージをGetWindowLongPtr(hwndDlg,
	// DWLP_USER)経由でこのオブジェクトへ振り分けるため、DWLP_USERを設定する前に
	// 発生するWM_SIZE等(下記の初期リサイズ呼び出しが引き起こす分を含む)は誰にも
	// 配送されずOnSize()が呼ばれないまま無視されてしまう。
	_SetHwnd( hwndDlg );
	::SetWindowLongPtr( hwndDlg, DWLP_USER, lParam );

	// CDialog::OnSize()(この後)がGetWindowRect()の現在値でm_nWidth/m_nHeightを
	// 問答無用で上書きしてしまうため、「サイズを記憶したことが一度もないか」の判定は
	// その副作用が起きる前に確定させ、2回目以降の表示では失われた前回のサイズを
	// 後で復元する必要がある。位置(m_xPos/m_yPos)は表示のたびに必ず既定位置へ
	// 再計算するため(下記)、ここで記憶・復元する対象はサイズのみでよい。
	bool	bFirstShowSize = ( -1 == m_nWidth || -1 == m_nHeight );
	int		nSavedWidth = m_nWidth, nSavedHeight = m_nHeight;

	// クリックでこのウィンドウ自体がアクティブ化されるのを防ぐ(HistoryPanelDlgSubclassProc
	// のWM_MOUSEACTIVATE参照)
	::SetWindowSubclass( hwndDlg, HistoryPanelDlgSubclassProc, 0, 0 );

	HWND	hListView = GetItemHwnd( IDC_LIST_UNDOHISTORY );

	// 飾りのステータスバー(CMainStatusBar::CreateStatusBar()と同じCreateStatusWindow)を
	// 下部に作成し、その高さぶんだけ一覧を切り詰め、元に戻す/やり直しボタンを
	// その左側に重ねて配置し直す(Paint.NETの履歴パネルと同じ見た目)。ボタンは
	// テンプレート内で先に生成済みのため、後から作るステータスバーの背面に
	// 隠れないようZ順をHWND_TOPで明示的に前面へ上げる。
	{
		// WS_CLIPSIBLINGSが無いと、このステータスバーが自身を再描画するたびに
		// (兄弟である元に戻す/やり直しボタンの領域も含めて)矩形全体を塗りつぶし、
		// Z順ではボタンの方が前面でも見た目上ボタンが消えてしまう。ダイアログ
		// テンプレート側のボタンは既定でWS_CLIPSIBLINGSが付くが、CreateStatusWindow()
		// はコードでの生成のため明示的に指定する必要がある。
		HWND	hStatusBar = ::CreateStatusWindow( WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, _T(""), hwndDlg, IDC_STATUSBAR_HISTORYPANEL );

		RECT	rcStatus;
		GetItemClientRect( IDC_STATUSBAR_HISTORYPANEL, rcStatus );
		int	nStatusHeight = rcStatus.bottom - rcStatus.top;

		RECT	rcDlgClient;
		::GetClientRect( hwndDlg, &rcDlgClient );

		RECT	rcListNow;
		GetItemClientRect( IDC_LIST_UNDOHISTORY, rcListNow );
		::SetWindowPos( hListView, NULL, 0, 0,
			rcListNow.right - rcListNow.left,
			( rcDlgClient.bottom - nStatusHeight ) - rcListNow.top - DpiScaleY( 2 ),
			SWP_NOMOVE | SWP_NOZORDER );

		int	nBtnSize = nStatusHeight - DpiScaleY( 4 );
		int	nBtnY = rcStatus.top + DpiScaleY( 2 );
		::SetWindowPos( GetItemHwnd( IDC_BUTTON_HISTORYUNDO ), HWND_TOP,
			rcStatus.left + DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_SHOWWINDOW );
		::SetWindowPos( GetItemHwnd( IDC_BUTTON_HISTORYREDO ), HWND_TOP,
			rcStatus.left + DpiScaleX( 2 ) + nBtnSize + DpiScaleX( 2 ), nBtnY, nBtnSize, nBtnSize, SWP_SHOWWINDOW );
	}

	// リサイズ用の隅つまみ(サイズボックス)は上のステータスバー作成より後に作ることで、
	// 常にステータスバーより前面(隅に重なって見える)にする。
	CreateSizeBox();
	CDialog::OnSize();

	// 直前のCDialog::OnSize()が上書きしたm_nWidth/m_nHeightを、このOnInitDialog
	// 呼び出しの開始時点の値へ戻す。bFirstShowSizeなら-1のままで良く(この後の
	// bFirstShowSizeブロックが改めて決定する)、そうでなければ前回OnDestroy()で
	// 記憶したユーザーのサイズをそのまま復元し、このあとCDialog::OnInitDialog()内の
	// SetDialogPosSize()が正しい値を使えるようにする。m_xPos/m_yPosは表示のたびに
	// 必ず下で既定位置へ再計算するので、ここでは復元しない。
	m_nWidth = nSavedWidth; m_nHeight = nSavedHeight;

	// OnSize()のResizeItem()計算用に、ここまでで確定したレイアウトを基準値として
	// 一度だけ記録しておく。この後の初回配置・SetDialogPosSize()による位置復元の
	// どちらでも、この基準値からの相対計算で追従させる。
	{
		RECT	rc;
		::GetWindowRect( hwndDlg, &rc );
		m_ptDefaultSize.x = rc.right - rc.left;
		m_ptDefaultSize.y = rc.bottom - rc.top;
		GetItemClientRect( IDC_LIST_UNDOHISTORY, m_rcListDefault );
		GetItemClientRect( IDC_BUTTON_HISTORYUNDO, m_rcUndoBtnDefault );
		GetItemClientRect( IDC_BUTTON_HISTORYREDO, m_rcRedoBtnDefault );
		GetItemClientRect( IDC_STATUSBAR_HISTORYPANEL, m_rcStatusBarDefault );
	}

	if( bFirstShowSize ){
		// 初回作成時のみ、ダイアログテンプレートの既定サイズの縦横それぞれ半分
		// (面積では1/4)に縮小する。縮小自体はOnSize()自身のResizeItem()ロジックに
		// 任せる(ここでm_ptDefaultSizeを基準にSetWindowPosするだけで、WM_SIZE経由で
		// 一覧・ボタン・ステータスバーも連動して縮む)。以後ユーザーがリサイズした
		// サイズはCDialog(true)によりOnDestroy()でm_nWidth/m_nHeightへ記憶される。
		::SetWindowPos( hwndDlg, NULL, 0, 0, m_ptDefaultSize.x / 2, m_ptDefaultSize.y / 2,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
	}

	// 位置は表示されるたびに毎回、親ウィンドウ(エディタ)の右下へ配置し直す
	// (CDlgFuncList「アウトライン解析」の非ドッキング時と同じ要領)。ユーザーが
	// ドラッグした位置は記憶しない。m_hwndParentではなくm_hwndTrueParentを使う
	// (この時点ではまだResolveDialogOwnerWindow()による誤補正後の値のままなので)。
	{
		// SetPlaceOfWindow()はm_bPlaceHorizontal/m_bPlaceVertical(既定true)が立って
		// いると、呼び出し時点のこのウィンドウの「現在の」物理サイズでm_nWidth/
		// m_nHeightを勝手に上書きしてしまう。bFirstShowSize==falseの回(サイズは
		// 縮小し直さずCDialog::OnInitDialog()内のSetDialogPosSize()に復元を任せている)
		// は、この時点ではまだウィンドウが生成直後の既定(自然)サイズのままなので、
		// 上のnSavedWidth/nSavedHeightで正しく復元したはずのサイズがここで再び
		// 上書きされてしまう。位置計算にはこのパネル自身の幅・高さは不要(親矩形と
		// DLGPLACE_BRの組から一意に決まる)ため、ここでは無効化しておく。
		bool	bSavedPlaceH = m_bPlaceHorizontal, bSavedPlaceV = m_bPlaceVertical;
		m_bPlaceHorizontal = false;
		m_bPlaceVertical = false;
		RECT	rcParent;
		::GetWindowRect( m_hwndTrueParent, &rcParent );
		SetPlaceOfWindow( m_hwndTrueParent, &rcParent, CDialog::DLGPLACE_BR );
		m_bPlaceHorizontal = bSavedPlaceH;
		m_bPlaceVertical = bSavedPlaceV;
		::SetWindowPos( hwndDlg, NULL, m_xPos, m_yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	}

	m_bParentWasMinimized = ( 0 != ::IsIconic( m_hwndTrueParent ) );

	ListView_SetExtendedListViewStyleEx( hListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );

	RECT	rc;
	::GetClientRect( hListView, &rc );
	int	nAvailWidth = ( rc.right - rc.left ) - ::GetSystemMetrics( SM_CXVSCROLL );

	LV_COLUMN	col;
	::ZeroMemory( &col, sizeof_raw( col ) );
	col.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
	col.fmt      = LVCFMT_LEFT;
	col.cx       = nAvailWidth;
	col.iSubItem = 0;
	ListView_InsertColumn( hListView, 0, &col );

	// Redo待ち(取り消し済み)行の表示用に、既定フォントのイタリック版を用意しておく
	// (OnDestroyで破棄)
	m_hFontItalic = CreateFontVariant( hListView, []( LOGFONT& lf ){
		lf.lfItalic = TRUE;
	} );

	RefreshList();

	CDialog::OnInitDialog( hwndDlg, wParam, lParam );

	// TRUEを返すと既定のダイアログ管理が一覧(唯一のWS_TABSTOPコントロール)へ
	// ::SetFocus()してしまう。フォーカスはスレッド単位でありウィンドウのアクティブ
	// 状態とは無関係にすぐ奪われるため、SW_SHOWNOACTIVATEで表示しても編集中の
	// キャレットからキーボード入力が奪われてしまう。このパネルは常時表示の
	// 非対話的な参照用であり自らフォーカスを取りに行ってはならないため、
	// CDlgCommandPaletteの「自前でフォーカスを設定したため」とは逆の理由でFALSEを返す。
	return FALSE;
}


/*! ユーザーのリサイズ操作(タイトルバー付きウィンドウのサイズ変更枠のドラッグ)に
	応じて、一覧をダイアログいっぱいに追従させる(CDlgWindowList::OnSizeと同じ
	ANCHOR_ALLパターン)
*/
BOOL CDlgHistoryPanel::OnSize( WPARAM wParam, LPARAM lParam )
{
	CDialog::OnSize( wParam, lParam );

	if( 0 == m_ptDefaultSize.x || 0 == m_ptDefaultSize.y ){
		// OnInitDialogでの記録より前(生成直後の最初のWM_SIZE)は何もしない
		return TRUE;
	}

	RECT	rc;
	::GetWindowRect( GetHwnd(), &rc );
	POINT	ptNew = { rc.right - rc.left, rc.bottom - rc.top };

	ResizeItem( GetItemHwnd( IDC_LIST_UNDOHISTORY ), m_ptDefaultSize, ptNew, m_rcListDefault, ANCHOR_ALL );
	// 飾りのステータスバーは幅いっぱいに伸ばしつつ下端に貼り付ける。元に戻す/やり直し
	// ボタンは大きさを変えず、Paint.NETの履歴パネルと同じくその左側に貼り付いたまま
	// 追従させる。
	ResizeItem( GetItemHwnd( IDC_STATUSBAR_HISTORYPANEL ), m_ptDefaultSize, ptNew, m_rcStatusBarDefault, ANCHOR_BOTTOM_LEFT_RIGHT );
	ResizeItem( GetItemHwnd( IDC_BUTTON_HISTORYUNDO ), m_ptDefaultSize, ptNew, m_rcUndoBtnDefault, ANCHOR_BOTTOM_LEFT );
	ResizeItem( GetItemHwnd( IDC_BUTTON_HISTORYREDO ), m_ptDefaultSize, ptNew, m_rcRedoBtnDefault, ANCHOR_BOTTOM_LEFT );

	// ResizeItem()は一覧コントロール自体の外枠は追従させるが、内部の(単一)列の幅は
	// OnInitDialogで設定した固定値のままになる。パネルを初期サイズより狭くすると
	// 列幅の方が可視幅より広いままになり不要な水平スクロールバーが出てしまうため、
	// リサイズのたびに列幅を可視幅(垂直スクロールバー分を差し引いた幅)に合わせ直す。
	{
		HWND	hListView = GetItemHwnd( IDC_LIST_UNDOHISTORY );
		RECT	rcList;
		::GetClientRect( hListView, &rcList );
		int		nAvailWidth = ( rcList.right - rcList.left ) - ::GetSystemMetrics( SM_CXVSCROLL );
		if( 0 < nAvailWidth ){
			ListView_SetColumnWidth( hListView, 0, nAvailWidth );
		}
	}

	::InvalidateRect( GetHwnd(), NULL, TRUE );
	return TRUE;
}


/*! 下部の「元に戻す」「やり直し」ボタン。COpeBuf::DoUndo/DoRedoは1ステップの
	ジャンプと同じ経路(HandleCommand)を使う。行クリックのExecuteJump()と違い
	1ステップだけなのでSetDrawSwitch()の抑制は不要
*/
BOOL CDlgHistoryPanel::OnBnClicked( int wID )
{
	if( IDC_BUTTON_HISTORYUNDO == wID || IDC_BUTTON_HISTORYREDO == wID ){
		if( NULL != m_pcView ){
			m_pcView->GetCommander().HandleCommand(
				( IDC_BUTTON_HISTORYUNDO == wID ) ? F_UNDO : F_REDO, true, 0, 0, 0, 0 );
		}
		// BS_PUSHBUTTONの既定のクリック処理も一覧のクリック(OnNotify参照)と同様に
		// このパネル自体をアクティブ化してしまうため、同じ理由で戻す。
		RestoreEditorFocus();
		return TRUE;
	}
	return CDialog::OnBnClicked( wID );
}


/*! 一覧のNM_CLICK・ボタンのBN_CLICKEDいずれも、既定のクリック処理内で対象
	コントロールが自分自身へ::SetFocus()し、その結果WS_EX_NOACTIVATE/
	WM_MOUSEACTIVATE(MA_NOACTIVATE)を設定していてもなおこのパネル(親ダイアログ)が
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


BOOL CDlgHistoryPanel::OnDestroy()
{
	if( NULL != m_hFontItalic ){
		::DeleteObject( m_hFontItalic );
		m_hFontItalic = NULL;
	}
	return CDialog::OnDestroy();
}


BOOL CDlgHistoryPanel::OnNotify( WPARAM wParam, LPARAM lParam )
{
	NMHDR*	pNMHDR = (NMHDR*)lParam;
	if( NULL == pNMHDR || IDC_LIST_UNDOHISTORY != pNMHDR->idFrom ){
		return FALSE;
	}
	if( NM_CLICK == pNMHDR->code ){
		// クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しでジャンプする
		// (Paint.NETの履歴パネルと同じ、クリック即実行のUI)
		NMITEMACTIVATE*	pNMIA = (NMITEMACTIVATE*)lParam;
		if( 0 <= pNMIA->iItem ){
			ExecuteJump( pNMIA->iItem );
		}
		RestoreEditorFocus();
		return TRUE;
	}
	if( NM_CUSTOMDRAW == pNMHDR->code ){
		LRESULT	lResult = OnListCustomDraw( lParam );
		::SetWindowLongPtr( GetHwnd(), DWLP_MSGRESULT, lResult );
		return TRUE;
	}
	return FALSE;
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
			HWND		hListView = GetItemHwnd( IDC_LIST_UNDOHISTORY );

			// レポート表示のCDDS_ITEMPREPAINT時点ではnmcd.rcが信頼できない
			// (CDlgCommandPalette::OnListCustomDrawと同じ既知の癖)ため、
			// ListView_GetItemRect()で改めて矩形を取得する。
			RECT	rc;
			ListView_GetItemRect( hListView, nDispIndex, &rc, LVIR_BOUNDS );

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
	HWND	hListView = GetItemHwnd( IDC_LIST_UNDOHISTORY );
	if( NULL == hListView || NULL == m_pcView ){
		return;
	}
	COpeBuf&	cOpeBuf = m_pcView->GetDocument()->m_cDocEditor.m_cOpeBuf;
	int			nCount = cOpeBuf.GetBlkCount() + 1;	// +1: 行0(編集開始時点)
	int			nCurrent = cOpeBuf.GetCurrentPointer();

	// 新しい行数を伝える前に選択状態を全解除しておく(件数が減った場合に、もう
	// 存在しない添字の選択状態が残ったまま次回に持ち越されるのを防ぐ、
	// CDlgCommandPalette::UpdateListと同じ理由)
	ListView_SetItemState( hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_SetItemCount( hListView, nCount );
	ListView_SetItemState( hListView, nCurrent, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_EnsureVisible( hListView, nCurrent, FALSE );
	::InvalidateRect( hListView, NULL, TRUE );

	// 下部ボタンの有効/無効も一覧と同じタイミングで更新する
	::EnableWindow( GetItemHwnd( IDC_BUTTON_HISTORYUNDO ), cOpeBuf.IsEnableUndo() );
	::EnableWindow( GetItemHwnd( IDC_BUTTON_HISTORYREDO ), cOpeBuf.IsEnableRedo() );
}

#endif // NKMM_UNDO_HISTORY_PANEL
