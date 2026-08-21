/*!	@file
	@brief コマンドパレットダイアログボックス

	@author Yu-zuki.
	@date 2026.08.18 新規作成 // NKMM_COMMAND_PALETTE
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"

#ifdef NKMM_COMMAND_PALETTE

#include "dlg/CDlgCommandPalette.h"
#include "func/CFuncLookup.h"
#include "func/CKeyBind.h"
#include "mem/CNativeT.h"
#include "env/CAppNodeManager.h"
#include "env/CShareData.h"
#include "recent/CMRUFile.h"
#include "util/window.h"
#include "util/CFuzzyMatchJp.h"
#include "_main/global.h"
#include "sakura_rc.h"
#include "sakura.hh"
#include "view/CEditView.h"
#include "window/CEditWnd.h"
#include "doc/CEditDoc.h"
#include "outline/CFuncInfoArr.h"
#include "outline/CFuncInfo.h"
#include "types/CType.h"

#include <algorithm>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#ifdef NKMM_COMMAND_PALETTE_ROMAJI
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif // NKMM_COMMAND_PALETTE_ROMAJI

namespace {
	const UINT_PTR	ID_TIMER_PALETTE_SLIDEIN = 1;
	const int		SLIDE_DURATION_MS = 150;
	const int		SLIDE_DISTANCE_DIP = 24;	// スライド開始位置のオフセット
	const int		SLIDE_INTERVAL_MS = 10;

	// 非アクティブ化による自滅を1回だけ遅延実行させるためのタイマー。WM_ACTIVATE
	// ハンドラの中で直接SetForegroundWindow()やDestroyWindow()を行うと、まだOS側の
	// アクティブ化遷移処理が完了しきっていない状態に割り込むことになり、こちらが
	// 前面に戻した直後にOS側の処理で別のウィンドウへ戻されてしまう(=結果的に後ろに
	// 隠れる)ことがある。タイマーで1回分メッセージループを回してから処理することで、
	// OS側の遷移が完了した後に実行されるようにする 20260819
	const UINT_PTR	ID_TIMER_PALETTE_DEFERRED_CLOSE = 2;

	const wchar_t	szWindowPrefix[] = L"edt ";	//!< 開いているウィンドウへの絞り込みモードに切り替える接頭辞
}

//! フィルタ欄先頭の1文字が、絞り込みモードを切り替える記号("コマンド"の">"、"アウトライン"の"@"、"ブックマーク"の"#")かどうか
static bool IsModeSymbolChar( wchar_t ch )
{
	return L'>' == ch || L'@' == ch || L'#' == ch;
}

//! フィルタ欄(Edit)の絞り込み中に一覧側の選択を上下に動かす。アウトライン/ブックマークの
//! 行へ移った場合、ライブプレビュー移動も行う(OnNotify(LVN_ITEMCHANGED)頼みだと、
//! ListView_SetItemState()の呼び方次第では通知が来ないことがあったため、ここで直接呼ぶ) 20260821
//! 現在位置(nCur)もListView_GetNextItem()には問い合わせない。LVS_OWNERDATA化後は
//! comctl32側の選択状態問い合わせ全般が信用できなくなった(OnListCustomDrawで発覚したのと
//! 同じ問題)ため、CDlgCommandPalette::GetSelectedDispIndex()で自前追跡した値を使う 20260821
static void MoveSelection( HWND hwndDlg, int nDelta )
{
	HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_COMMANDPALETTE );
	int	nCount = ListView_GetItemCount( hListView );
	if( 0 == nCount ){
		return;
	}
	CDlgCommandPalette*	pDlg = (CDlgCommandPalette*)::GetWindowLongPtr( hwndDlg, DWLP_USER );
	int	nCur = ( NULL != pDlg ) ? pDlg->GetSelectedDispIndex() : -1;
	int	nNext = ( nCur < 0 ) ? 0 : nCur + nDelta;
	if( nNext < 0 ){ nNext = 0; }
	if( nNext >= nCount ){ nNext = nCount - 1; }
	if( 0 <= nCur ){
		ListView_SetItemState( hListView, nCur, 0, LVIS_SELECTED | LVIS_FOCUSED );
	}
	ListView_SetItemState( hListView, nNext, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_EnsureVisible( hListView, nNext, FALSE );

	if( NULL != pDlg ){
		pDlg->LivePreviewSelection( nNext );
	}
}

#ifdef NKMM_COMMAND_PALETTE_ROMAJI
/*! フィルタ欄の入力途中ローマ字文字列を、確定できたモーラ分だけかなへ変換する。
	">"/"@"/"edt "接頭辞(絞り込みモード切り替え記号)はそのまま残し、それより後ろだけを
	変換する。"edt "は確定するまで('e'→'ed'→'edt'の途中)変換を保留し、
	接頭辞と誤認して変換されないようにする 20260819
	"@"は">"と同じく1文字の記号のため、同じ扱いにする 20260821
*/
static std::wstring ApplyLiveKanaConversion( const std::wstring& sText )
{
	size_t	nPrefixLen = wcslen( szWindowPrefix );
	if( sText.size() <= nPrefixLen && 0 == wcsncmp( szWindowPrefix, sText.c_str(), sText.size() ) ){
		return sText;
	}
	if( sText.size() >= nPrefixLen && 0 == wcsncmp( szWindowPrefix, sText.c_str(), nPrefixLen ) ){
		return std::wstring( szWindowPrefix ) + ConvertRomajiToKana( sText.substr( nPrefixLen ) );
	}
	if( !sText.empty() && IsModeSymbolChar( sText[0] ) ){
		return sText.substr( 0, 1 ) + ConvertRomajiToKana( sText.substr( 1 ) );
	}
	return ConvertRomajiToKana( sText );
}

/*! IMEが変換前の文字列(未確定文字列)を編集中かどうか。Escapeキーをパレット閉じるのに
	使ってしまうと、IME入力中に期待される「まずIMEの未確定文字列を取り消す」という
	標準動作ができなくなるため、この間はEscapeを横取りしないようにする 20260819
*/
static bool IsImeComposing( HWND hwnd )
{
	HIMC	hImc = ::ImmGetContext( hwnd );
	if( NULL == hImc ){
		return false;
	}
	bool	bComposing = false;
	if( FALSE != ::ImmGetOpenStatus( hImc ) ){
		bComposing = ( 0 < ::ImmGetCompositionStringW( hImc, GCS_COMPSTR, NULL, 0 ) );
	}
	::ImmReleaseContext( hwnd, hImc );
	return bComposing;
}
#endif // NKMM_COMMAND_PALETTE_ROMAJI

/*! フィルタ欄(Edit)のサブクラスプロシージャ

	上下矢印キーで一覧の選択を動かし、Enter/EscapeはIDOK/IDCANCELとして
	親ダイアログへ通知する(既定のダイアログ管理に頼らず確実に効かせるため、
	WM_GETDLGCODEで明示的に横取りする)。それ以外の文字入力は素通しして
	通常のフィルタ入力として使えるようにする。

	NKMM_COMMAND_PALETTE_ROMAJI有効時は、末尾へのASCII文字入力(a-z/A-Z/-)を
	WM_CHARで横取りし、確定したモーラ分だけその場でかなへ変換して表示する
	(Windows検索ボックス等と同じ簡易IME体験)。IME入力中の確定済み文字は
	既にひらがな/漢字等で来るため対象外(素通し)になる 20260819
*/
static LRESULT CALLBACK PaletteFilterEditSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	if( WM_GETDLGCODE == uMsg ){
		LRESULT	nCode = DLGC_WANTCHARS | DLGC_WANTARROWS;
		const MSG*	pMsg = (const MSG*)lParam;
		if( NULL != pMsg && WM_KEYDOWN == pMsg->message ){
			if( VK_RETURN == pMsg->wParam ){
				nCode |= DLGC_WANTMESSAGE;
			}else if( VK_ESCAPE == pMsg->wParam
#ifdef NKMM_COMMAND_PALETTE_ROMAJI
			 && !IsImeComposing( hwnd )
#endif // NKMM_COMMAND_PALETTE_ROMAJI
			){
				// IME変換中(未確定文字列あり)のときはEscapeを横取りせず、まずIME側の
				// 「未確定文字列を取り消す」という標準動作に委ねる 20260819
				nCode |= DLGC_WANTMESSAGE;
			}
		}
		return nCode;
	}else if( WM_KEYDOWN == uMsg ){
		HWND	hwndDlg = ::GetParent( hwnd );
		if( VK_RETURN == wParam ){
			::SendMessage( hwndDlg, WM_COMMAND, MAKEWPARAM( IDOK, BN_CLICKED ), (LPARAM)::GetDlgItem( hwndDlg, IDOK ) );
			return 0;
		}else if( VK_ESCAPE == wParam
#ifdef NKMM_COMMAND_PALETTE_ROMAJI
		 && !IsImeComposing( hwnd )
#endif // NKMM_COMMAND_PALETTE_ROMAJI
		){
			::SendMessage( hwndDlg, WM_COMMAND, MAKEWPARAM( IDCANCEL, BN_CLICKED ), (LPARAM)::GetDlgItem( hwndDlg, IDCANCEL ) );
			return 0;
		}else if( VK_UP == wParam || VK_DOWN == wParam ){
			MoveSelection( hwndDlg, ( VK_UP == wParam ) ? -1 : 1 );
			return 0;
		}
#ifdef NKMM_COMMAND_PALETTE_ROMAJI
	}else if( WM_IME_COMPOSITION == uMsg ){
		// 未確定の変換中文字列が変わるたびに、確定を待たず一覧をライブ更新する。
		// 通常のWM_CHAR/EN_CHANGEは変換確定時にしか飛んでこないため、ここで明示的に
		// EN_CHANGE相当を親へ送ってUpdateList()を動かす 20260819
		LRESULT	lRet = ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
		if( 0 != ( lParam & GCS_COMPSTR ) ){
			HWND	hwndDlg = ::GetParent( hwnd );
			::SendMessage( hwndDlg, WM_COMMAND, MAKEWPARAM( IDC_EDIT_COMMANDPALETTE_FILTER, EN_CHANGE ), (LPARAM)hwnd );
		}
		return lRet;
	}else if( WM_CHAR == uMsg ){
		wchar_t	ch = (wchar_t)wParam;
		bool	bConvertible = ( L'a' <= ch && ch <= L'z' ) || ( L'A' <= ch && ch <= L'Z' ) || L'-' == ch;
		// 実際のIMEのON/OFF状態に従う。IME OFF(半角英数直接入力)のときはこのボックスも
		// 素通しにし、変換はIME ON(かなモード)のときだけ働かせる 20260819
		if( bConvertible ){
			bool	bImeOpen = false;
			HIMC	hImc = ::ImmGetContext( hwnd );
			if( NULL != hImc ){
				bImeOpen = ( FALSE != ::ImmGetOpenStatus( hImc ) );
				::ImmReleaseContext( hwnd, hImc );
			}
			bConvertible = bImeOpen;
		}
		if( bConvertible ){
			DWORD	dwSel = (DWORD)::SendMessage( hwnd, EM_GETSEL, 0, 0 );
			int	nSelStart = LOWORD( dwSel );
			int	nSelEnd = HIWORD( dwSel );
			int	nTextLen = ::GetWindowTextLength( hwnd );
			if( nSelStart == nSelEnd && nSelStart == nTextLen ){
				WCHAR	szBuf[256];
				::GetWindowText( hwnd, szBuf, _countof( szBuf ) );
				std::wstring	sConverted = ApplyLiveKanaConversion( std::wstring( szBuf ) + ch );
				::SetWindowText( hwnd, sConverted.c_str() );
				::SendMessage( hwnd, EM_SETSEL, (WPARAM)sConverted.size(), (LPARAM)sConverted.size() );
				return 0;
			}
		}
#endif // NKMM_COMMAND_PALETTE_ROMAJI
	}else if( WM_NCDESTROY == uMsg ){
		::RemoveWindowSubclass( hwnd, PaletteFilterEditSubclassProc, uIdSubclass );
	}
	return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
}


CDlgCommandPalette::CDlgCommandPalette()
	: CDialog( false )
	, m_pcFuncLookup( NULL )
	, m_pcView( NULL )
	, m_bOutlineRowsBuilt( false )
	, m_bBookmarkRowsBuilt( false )
	, m_nOrigCaretX( 0 )
	, m_nOrigCaretY( 0 )
	, m_nSelectedDispIndex( -1 )
	, m_nMaxListHeight( 0 )
	, m_nChromeHeight( 0 )
	, m_nListRowHeight( 0 )
	, m_nSlideX( 0 )
	, m_nSlideTargetY( 0 )
	, m_nSlideStartY( 0 )
	, m_dwSlideStartTick( 0 )
	, m_bReactivateParentOnClose( false )
	, m_hFontList( NULL )
	, m_hFontSub( NULL )
	, m_hThemeListView( NULL )
{
}


/*! モードレスダイアログの表示。検索ダイアログ(CDlgFind)と同じく、生成直後は
	SW_HIDEで隠しておき、StartSlideAnimation()で最終位置より少し上から
	スライドインさせながら表示する 20260818
*/
HWND CDlgCommandPalette::DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView )
{
	m_pcFuncLookup = pcFuncLookup;
	m_pcView = pcView;
	m_bOutlineRowsBuilt = false;
	m_bBookmarkRowsBuilt = false;

	// アウトライン/ブックマークのライブプレビュー移動をEscapeでキャンセルしたときに
	// 戻る位置として、パレットを開いた時点のカーソル位置を覚えておく 20260821
	m_nOrigCaretX = 0;
	m_nOrigCaretY = 0;
	if( NULL != m_pcView ){
		CLogicPoint	poOrigCaret = m_pcView->GetCaret().GetCaretLogicPos();
		m_nOrigCaretX = poOrigCaret.x;
		m_nOrigCaretY = poOrigCaret.y;
	}

	// m_xPos/m_yPosはCDialogのメンバで、閉じても(このオブジェクト自体はCEditWndに
	// 保持され続けるため)前回開いたときの値が残ったままになる。2回目以降、
	// CDialog::OnInitDialog()内のSetDialogPosSize()がその古い値を使って
	// ウィンドウ生成直後(まだ自前でSW_HIDEにする前)に本来位置へ動かしてしまい、
	// 結果としてスライドインの前に一瞬全体が見えてしまっていた。ここで-1に戻し、
	// 毎回FollowParentWindow()/StartSlideAnimation()だけが位置決めするようにする 20260819
	m_xPos = -1;
	m_yPos = -1;

	HWND	hWnd = CDialog::DoModeless( hInstance, hwndParent, IDD_DLG_COMMANDPALETTE, 0, SW_HIDE );
	if( NULL != hWnd ){
		// ダイアログテンプレートの既定位置(0,0)のままスライドインすると、直後に
		// FollowParentWindow()相当の補正が入って本来位置へ飛んで見えるため、
		// スライド開始前に一度呼んで最終位置を確定させておく 20260819
#ifdef NKMM_FIX_DIALOG_POS
		FollowParentWindow();
#endif // NKMM_
		StartSlideAnimation();
	}
	return hWnd;
}


/*! 上からのスライドインアニメーションを開始する(CDlgFind::StartSlideAnimationと同じ方式) 20260818 */
void CDlgCommandPalette::StartSlideAnimation()
{
	RECT	rc;
	::GetWindowRect( GetHwnd(), &rc );

	m_nSlideX          = rc.left;
	m_nSlideTargetY    = rc.top;
	m_nSlideStartY     = rc.top - DpiScaleY( SLIDE_DISTANCE_DIP );
	m_dwSlideStartTick = ::GetTickCount();

	::SetWindowPos( GetHwnd(), NULL, m_nSlideX, m_nSlideStartY, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	::ShowWindow( GetHwnd(), SW_SHOW );	// 表示とアクティブ化(絞り込み欄へフォーカス)
	// ShowWindow直後は背景(灰色)だけ先に見えて、一覧などの子コントロールの描画が
	// 1フレーム遅れて追いつく形になり一瞬ちらつくため、ここで子コントロールも含めて
	// 同期的に描画を完了させてから返す 20260819
	::RedrawWindow( GetHwnd(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );

	::SetTimer( GetHwnd(), ID_TIMER_PALETTE_SLIDEIN, SLIDE_INTERVAL_MS, NULL );
}


BOOL CDlgCommandPalette::OnTimer( WPARAM wParam )
{
	if( wParam == ID_TIMER_PALETTE_DEFERRED_CLOSE ){
		::KillTimer( GetHwnd(), ID_TIMER_PALETTE_DEFERRED_CLOSE );
		CloseOnDeactivate();
		return TRUE;
	}

	if( wParam != ID_TIMER_PALETTE_SLIDEIN ){
		return CDialog::OnTimer( wParam );
	}

	DWORD	dwElapsed = ::GetTickCount() - m_dwSlideStartTick;
	if( dwElapsed >= (DWORD)SLIDE_DURATION_MS ){
		::SetWindowPos( GetHwnd(), NULL, m_nSlideX, m_nSlideTargetY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
		::KillTimer( GetHwnd(), ID_TIMER_PALETTE_SLIDEIN );
		return TRUE;
	}

	// ease-out (3次): 1 - (1-t)^3
	double	t = (double)dwElapsed / SLIDE_DURATION_MS;
	double	u = 1.0 - t;
	double	eased = 1.0 - u * u * u;
	int	y = m_nSlideStartY + (int)( ( m_nSlideTargetY - m_nSlideStartY ) * eased );

	::SetWindowPos( GetHwnd(), NULL, m_nSlideX, y, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	return TRUE;
}


#ifdef NKMM_FIX_DIALOG_POS
/*! 親ウィンドウ(エディタ)の移動・サイズ変更に追従して表示位置を更新する(CDlgFind::FollowParentWindowと同じ方式)。
	SWP_NOACTIVATEにより、追従による再配置がエディタ側のフォーカス・操作を妨げないようにする 20260818
*/
void CDlgCommandPalette::FollowParentWindow()
{
	if( NULL == GetHwnd() ){
		return;
	}
	RECT	rcParent;
	::GetWindowRect( m_hwndParent, &rcParent );
	SetPlaceOfWindow( m_hwndParent, &rcParent, CDialog::DLGPLACE_TC );
	::SetWindowPos( GetHwnd(), NULL, m_xPos, m_yPos, 0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
}
#endif // NKMM_


BOOL CDlgCommandPalette::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	CDialog::OnInitDialog( hwndDlg, wParam, lParam );

	// 枠なしフローティングパネル化(検索ダイアログと同じ方式)。DWMの非クライアント描画
	// (影)を無効化し、その代わりにsakura_rc.rcのWS_BORDERで薄い縁取りを付けている 20260818
	{
		const int	ncRenderingPolicy = DWMNCRP_DISABLED;
		::DwmSetWindowAttribute( hwndDlg, DWMWA_NCRENDERING_POLICY, &ncRenderingPolicy, sizeof( ncRenderingPolicy ) );

		// DWMNCRP_DISABLEDだけだと、キャプション無しのポップアップだと判別が
		// 付きにくいためかWindows 11がアクセントカラーの太い縁を自動で描き足して
		// しまう(このウィンドウ自体は描いていない)。DWMWA_BORDER_COLORを
		// DWMWA_COLOR_NONEにしてこのDWM側の縁取りを止め、sakura_rc.rcのWS_BORDER
		// による地味な1pxの縁(エディタ本体のウィンドウ枠と同じ見え方)だけにする 20260820
		const COLORREF	crNoBorder = DWMWA_COLOR_NONE;
		::DwmSetWindowAttribute( hwndDlg, DWMWA_BORDER_COLOR, &crNoBorder, sizeof( crNoBorder ) );
	}

	HWND	hListView = GetItemHwnd( IDC_LIST_COMMANDPALETTE );

	// AdjustListHeight()用に、sakura_rc.rcのダイアログテンプレート設計値
	// (一覧の最大高さ、それを除いたダイアログ全体の高さ)を一度だけ控えておく。
	// これらは以後リサイズしても変わらない固定値として扱う 20260821
	{
		RECT	rcListInit;
		::GetWindowRect( hListView, &rcListInit );
		m_nMaxListHeight = rcListInit.bottom - rcListInit.top;

		RECT	rcWndInit;
		::GetWindowRect( hwndDlg, &rcWndInit );
		m_nChromeHeight = ( rcWndInit.bottom - rcWndInit.top ) - m_nMaxListHeight;
	}

	// VSCodeのクイックオープン/コマンドパレット風に、複数列の表形式ではなく
	// アイコン+太字の名前+(ファイル系のみ)グレーのパス、右寄せでショートカット
	// キーまたは「最近使用」タグを描く1行形式にする。列見出しは使わないため
	// LVS_NOCOLUMNHEADERを持たせている(sakura_rc.rcのダイアログテンプレート側で
	// 生成時スタイルとして指定済み)。GWL_STYLEを生成後にSetWindowLongPtr()で
	// 後から立てる方式は、ヘッダーウィンドウが「表示状態のまま高さ0」に
	// 中途半端に壊れ、行の矩形計算まで破綻してcomctl32内でクラッシュする
	// 不具合があったため、生成時スタイルでの指定に変更した 20260819
	ListView_SetExtendedListViewStyleEx( hListView,
		LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );

	// 選択行の背景をVSCode風の単色反転(COLOR_HIGHLIGHT)ではなく、エクスプローラーの
	// ファイル一覧と同じ半透明の選択色にするため、"Explorer::ListView"のテーマデータを
	// 取得しておく。自前のNM_CUSTOMDRAWで全描画しているためSetWindowTheme()で見た目が
	// 変わることはなく、DrawThemeBackground()に渡すHTHEMEを得る目的だけで開いている。
	// テーマが無効(クラシックテーマ等)ならNULLのままになり、OnListCustomDraw側で
	// 従来のCOLOR_HIGHLIGHT反転色へフォールバックする 20260821
	if( CUxTheme::getInstance()->IsThemeActive() ){
		m_hThemeListView = CUxTheme::getInstance()->OpenThemeData( hListView, L"Explorer::ListView" );
	}

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

	// 共有システムアイコン一覧を割り当てる。索引自体はGetShellIconIndex()経由で
	// 描画時に自前で選び直すため、ここでの目的は行の高さをアイコンサイズに
	// 合わせることだけ(このHIMAGELISTはシェル共有のため破棄しない) 20260819
	{
		SHFILEINFO	sfi = {};
		HIMAGELIST	hSysSmallIL = (HIMAGELIST)::SHGetFileInfo( L"dummy.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof( sfi ),
			SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
		if( NULL != hSysSmallIL ){
			ListView_SetImageList( hListView, hSysSmallIL, LVSIL_SMALL );
		}
	}

	// 一覧の文字表示用に、既定フォントより少し大きい通常太さのフォントを
	// 用意しておく(OnDestroyで破棄)。太字は見にくいとの指摘があったため、
	// 名前とキーの差はフォントの太さではなく色(黒/グレー)だけで付ける 20260819
	// グレーのパス表示(格納フォルダ)は、色に加えてこれより小さいフォント
	// (既定フォントそのままの大きさ)にし、ファイル名との違いを一目で
	// わかるようにする 20260821
	{
		HFONT	hFontBase = (HFONT)::SendMessage( hListView, WM_GETFONT, 0, 0 );
		LOGFONT	lf;
		::ZeroMemory( &lf, sizeof_raw( lf ) );
		if( NULL != hFontBase && 0 != ::GetObject( hFontBase, sizeof( lf ), &lf ) ){
			LOGFONT	lfList = lf;
			lfList.lfHeight = (LONG)( lfList.lfHeight * 13 / 10 );
			lfList.lfWeight = FW_NORMAL;
			m_hFontList = ::CreateFontIndirect( &lfList );

			LOGFONT	lfSub = lf;
			lfSub.lfWeight = FW_NORMAL;
			m_hFontSub = ::CreateFontIndirect( &lfSub );
		}
	}

	::SetWindowSubclass( GetItemHwnd( IDC_EDIT_COMMANDPALETTE_FILTER ), PaletteFilterEditSubclassProc, 0, 0 );

	BuildAllRows();

	// 既定は「>」(コマンドのみ)から始める。カーソルは末尾に置き、続けて絞り込み文字を
	// 打てるようにする 20260818
	{
		HWND	hEditFilter = GetItemHwnd( IDC_EDIT_COMMANDPALETTE_FILTER );
		::SetWindowText( hEditFilter, L">" );
		::SendMessage( hEditFilter, EM_SETSEL, (WPARAM)1, (LPARAM)1 );
	}
	UpdateList();

	::SetFocus( GetItemHwnd( IDC_EDIT_COMMANDPALETTE_FILTER ) );

	return FALSE;	// 自前でフォーカスを設定したため
}


BOOL CDlgCommandPalette::OnBnClicked( int wID )
{
	switch( wID ){
	case IDOK:
		ExecuteSelected();
		CloseDialog( 0 );
		return TRUE;
	case IDCANCEL:
		// アウトライン/ブックマークの選択に追従してカーソルをライブ移動していた場合、
		// キャンセル時はパレットを開く前の位置へ戻す。一度もそのモードへ入らずに
		// キャンセルしたときは同じ位置へ戻すだけの無害な呼び出しになる 20260821
		MoveCaretTo( m_nOrigCaretX, m_nOrigCaretY );
		CloseDialog( 0 );
		return TRUE;
	}
	return CDialog::OnBnClicked( wID );
}


BOOL CDlgCommandPalette::OnEnChange( HWND hwndCtl, int wID )
{
	if( IDC_EDIT_COMMANDPALETTE_FILTER == wID ){
		UpdateList();
		return TRUE;
	}
	return FALSE;
}


BOOL CDlgCommandPalette::OnNotify( WPARAM wParam, LPARAM lParam )
{
	NMHDR*	pNMHDR = (NMHDR*)lParam;
	if( NULL == pNMHDR || IDC_LIST_COMMANDPALETTE != pNMHDR->idFrom ){
		return FALSE;
	}
	if( NM_DBLCLK == pNMHDR->code ){
		ExecuteSelected();
		CloseDialog( 0 );
		return TRUE;
	}
	if( LVN_ITEMCHANGED == pNMHDR->code ){
		// マウスクリックによる選択変更用(キーボードでの上下移動やフィルタ再構築時の
		// 自動選択は、それぞれMoveSelection()/UpdateList()から直接LivePreviewSelection()を
		// 呼んでいるため、ここでの二重呼び出しは無害だが基本的には素通りになる)。
		// 選択状態が新たに立った行だけを対象にする 20260821
		NMLISTVIEW*	pNMLV = (NMLISTVIEW*)lParam;
		if( 0 != ( pNMLV->uNewState & LVIS_SELECTED ) && 0 == ( pNMLV->uOldState & LVIS_SELECTED ) ){
			LivePreviewSelection( pNMLV->iItem );
		}
		return FALSE;
	}
	if( LVN_GETDISPINFO == pNMHDR->code ){
		// LVS_OWNERDATA(仮想リスト)は行データを保持しないため、表示に必要な分だけ
		// ここで都度供給する。実際の見た目はOnListCustomDraw()が総取り換えで描くが、
		// スクリーンリーダー等のフォールバック・内部の文字列前方一致ジャンプ機能等の
		// ためにLVIF_TEXTだけは渡しておく 20260821
		NMLVDISPINFO*	pDispInfo = (NMLVDISPINFO*)lParam;
		if( 0 != ( pDispInfo->item.mask & LVIF_TEXT ) ){
			size_t	nDispIndex = (size_t)pDispInfo->item.iItem;
			if( nDispIndex < m_vMatchedRowIndices.size() ){
				size_t	nRowIndex = m_vMatchedRowIndices[nDispIndex];
				if( nRowIndex < m_vAllRows.size() ){
					pDispInfo->item.pszText = const_cast<wchar_t*>( m_vAllRows[nRowIndex].sName.c_str() );
				}
			}
		}
		return TRUE;
	}
	if( NM_CUSTOMDRAW == pNMHDR->code ){
		// WM_NOTIFYはダイアログプロシージャの戻り値ではなくDWLP_MSGRESULTで
		// 結果(CDRF_*)を伝える必要がある 20260819
		LRESULT	lResult = OnListCustomDraw( lParam );
		::SetWindowLongPtr( GetHwnd(), DWLP_MSGRESULT, lResult );
		return TRUE;
	}
	return FALSE;
}


/*! 一覧の描画(NM_CUSTOMDRAW)。VSCodeのクイックオープン/コマンドパレット風に、
	アイコン(コマンドは無し)+太字の名前+(ファイル系のみ)グレーの格納フォルダを
	左詰めで描き、右にコマンドのショートカットキーまたは「最近使用」タグを
	グレーで右寄せ表示する 20260819
*/
LRESULT CDlgCommandPalette::OnListCustomDraw( LPARAM lParam )
{
	NMLVCUSTOMDRAW*	pCD = (NMLVCUSTOMDRAW*)lParam;

	switch( pCD->nmcd.dwDrawStage ){
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT:
		{
			// LVS_OWNERDATA(仮想リスト)にはlParamが無いため、表示上の行番号
			// (dwItemSpec)をm_vMatchedRowIndices経由でm_vAllRowsの添字に変換する 20260821
			size_t	nDispIndex = (size_t)pCD->nmcd.dwItemSpec;
			if( nDispIndex >= m_vMatchedRowIndices.size() ){
				break;
			}
			size_t	nRowIndex = m_vMatchedRowIndices[nDispIndex];
			if( nRowIndex >= m_vAllRows.size() ){
				break;
			}
			const PaletteRow&	row = m_vAllRows[nRowIndex];
			HDC		hdc = pCD->nmcd.hdc;
			HWND	hListView = GetItemHwnd( IDC_LIST_COMMANDPALETTE );

			// レポート表示のCDDS_ITEMPREPAINT時点ではnmcd.rcが信頼できない
			// (comctl32のバージョンによって不定になる既知の癖)ため、
			// ListView_GetItemRect()で改めて矩形を取得する。アイコンは
			// ImageList_Draw()が矩形に依存せず座標指定だけで描くため見えていたが、
			// DrawText()はこの壊れた矩形でクリップされ何も描画されていなかった 20260819
			RECT	rc;
			ListView_GetItemRect( hListView, (int)pCD->nmcd.dwItemSpec, &rc, LVIR_BOUNDS );

			// pCD->nmcd.uItemStateのCDIS_SELECTEDは実際の選択行に関わらず全行で
			// 立ってしまう(デバッグ確認済み。原因不明のcomctl32側の癖)ため信用せず、
			// 一時はListView_GetItemState()で実際の選択状態を取得していたが、
			// LVS_OWNERDATA(仮想リスト)化した後はこちらも同様に全行「選択中」を
			// 返すようになってしまった(実機確認: 絞り込み結果の全行が選択色で
			// 塗られる不具合が発生)。そのためcomctl32には一切問い合わせず、選択行が
			// 変わる経路をすべて集約しているLivePreviewSelection()で自前追跡する
			// m_nSelectedDispIndexとだけ比較する 20260819 20260821
			bool	bSelected = ( (int)nDispIndex == m_nSelectedDispIndex );

			// 選択行は単色反転(COLOR_HIGHLIGHT)ではなく、エクスプローラーのファイル一覧と
			// 同じ半透明の選択色(m_hThemeListView、OnInitDialogで取得した
			// "Explorer::ListView"テーマ)で描く。反転(1色で完全に塗りつぶす)は選択行だけ
			// 周囲から浮いて見えるとの指摘があり、半透明合成なら下地の色を活かしたまま
			// 選択状態が分かる。半透明合成の上では白文字への反転も不要になる
			// (エクスプローラーも選択行の文字色は変えない)ため、テーマが使えたときは
			// 文字色を非選択時と同じにする。テーマ非対応環境(クラシックテーマ等)だけ
			// 従来通りCOLOR_HIGHLIGHTの単色反転+白文字にフォールバックする 20260821
			bool	bThemedSelection = ( bSelected && NULL != m_hThemeListView );
			COLORREF	crText = ::GetSysColor( ( bSelected && !bThemedSelection ) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT );
			COLORREF	crSub  = ( bSelected && !bThemedSelection ) ? crText : ::GetSysColor( COLOR_GRAYTEXT );

			// CDRF_SKIPDEFAULTで既定描画を止めているため、背景は選択・非選択どちらも
			// 自前で塗る必要がある。以前は非選択行の背景塗りを省略し、既定の白色に
			// 既に消されている前提でいたが、LVS_OWNERDATA(仮想リスト)化した後は
			// その前提が崩れ、スクロールや絞り込みで表示行が入れ替わったときに前の
			// 描画(選択色の青)が消されずそのまま残ってしまう不具合が出た。
			// 選択行かどうかによらず必ず背景を塗りつぶすことで解消する 20260821
			{
				if( bThemedSelection ){
					// DrawThemeBackground()のLISS_SELECTEDは下地に対する半透明合成描画のため、
					// 単なるFillRect()と違って「同じ行を下地クリアせず繰り返し描くと塗るたびに
					// 色が濃くなっていく」。上端/下端で矢印キーを押し続けたときのように
					// 選択行が変わらないまま再描画だけが繰り返されるケースで、選択行の背景が
					// どんどん濃く重なって見える不具合として実際に発生した。DrawThemeBackground()の
					// 前に必ずCOLOR_WINDOWで一度下地をクリアしておくことで、何度描き直しても
					// 常に同じ結果になるようにする 20260821
					HBRUSH	hBrushBase = ::CreateSolidBrush( ::GetSysColor( COLOR_WINDOW ) );
					::FillRect( hdc, &rc, hBrushBase );
					::DeleteObject( hBrushBase );
					CUxTheme::getInstance()->DrawThemeBackground( m_hThemeListView, hdc, LVP_LISTITEM, LISS_SELECTED, &rc, NULL );
				}else{
					HBRUSH	hBrush = ::CreateSolidBrush( ::GetSysColor( bSelected ? COLOR_HIGHLIGHT : COLOR_WINDOW ) );
					::FillRect( hdc, &rc, hBrush );
					::DeleteObject( hBrush );
				}
			}

			::SetBkMode( hdc, TRANSPARENT );

			// 一覧全体の文字を、既定より少し大きい通常太さのフォントで揃える。
			// 太字は見にくいとの指摘があったため使わず、名前とパス/キーの差は
			// 色(黒/グレー)だけで付ける 20260819
			HFONT	hOldFont = ( NULL != m_hFontList ) ? (HFONT)::SelectObject( hdc, m_hFontList ) : NULL;

			// VSCodeのように行の左右に余裕を持たせる。アイコン〜名前〜パスの間の
			// 小さい間隔(nPad)と、行の左端/右端の余白(nEdgePad)を分けて、
			// 余白をより広く取る 20260819
			int	nPad = DpiScaleX( 4 );
			int	nEdgePad = DpiScaleX( 10 );
			int	x = rc.left + nEdgePad;

			// アイコン(ウィンドウ/最近使ったファイルは拡張子から。コマンド/アウトラインには付けない) 20260819 20260821
			if( ROWKIND_WINDOW == row.kind || ROWKIND_RECENT == row.kind ){
				HIMAGELIST	hIL = ListView_GetImageList( hListView, LVSIL_SMALL );
				int	nIconIndex = GetShellIconIndex( row.sSub );
				int	nIconSize = DpiScaleX( 16 );
				if( NULL != hIL && 0 <= nIconIndex ){
					::ImageList_Draw( hIL, nIconIndex, hdc, x, rc.top + ( ( rc.bottom - rc.top ) - nIconSize ) / 2, ILD_TRANSPARENT );
				}
				x += nIconSize + nPad;
			}

			// 右寄せ(コマンドはショートカットキー、最近使ったファイルは「最近使用」タグ、
			// アウトライン/ブックマークはジャンプ先の行番号) 20260819 20260821
			std::wstring	sRight;
			if( ROWKIND_COMMAND == row.kind ){
				sRight = row.sSub;
			}else if( ROWKIND_RECENT == row.kind ){
				sRight = L"最近使用";
			}else if( ROWKIND_OUTLINE == row.kind || ROWKIND_BOOKMARK == row.kind ){
				sRight = std::to_wstring( row.nPostId ) + L" 行目";
			}
			int	nRightWidth = 0;
			if( !sRight.empty() ){
				SIZE	sz;
				::GetTextExtentPoint32( hdc, sRight.c_str(), (int)sRight.size(), &sz );
				nRightWidth = sz.cx;
				RECT	rcRight = { rc.right - nRightWidth - nEdgePad, rc.top, rc.right - nEdgePad, rc.bottom };
				::SetTextColor( hdc, crSub );
				::DrawText( hdc, sRight.c_str(), -1, &rcRight, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX );
			}

			int	nNameRight = rc.right - nRightWidth - nEdgePad - nPad * 2;

			// 名前(黒)。NKMM_DEBUG_COMMAND_PALETTE_KANJI_COVERAGE有効時は
			// g_aMultiMoraKanjiTableに登録済みの漢字だけ色を変えて描く 20260819 20260821
			SIZE	szName = { 0, 0 };
			::GetTextExtentPoint32( hdc, row.sName.c_str(), (int)row.sName.size(), &szName );
			int	nNameRightEdge = ( x + szName.cx + nPad < nNameRight ) ? ( x + szName.cx + nPad ) : nNameRight;
			RECT	rcName = { x, rc.top, nNameRightEdge, rc.bottom };
#ifdef NKMM_DEBUG_COMMAND_PALETTE_KANJI_COVERAGE
			// DT_END_ELLIPSISは複数回のDrawText呼び出しをまたいでは効かないため、
			// この表示のときは末尾省略の精度は落ちる(デバッグ専用機能のため許容)
			{
				int		xCur = x;
				size_t	i = 0;
				const std::wstring&	sName = row.sName;
				while( i < sName.size() && xCur < nNameRightEdge ){
					bool	bCovered = IsKanjiInMultiMoraTable( sName[i] );
					size_t	nRunStart = i;
					while( i < sName.size() && IsKanjiInMultiMoraTable( sName[i] ) == bCovered ){ ++i; }
					std::wstring	sRun = sName.substr( nRunStart, i - nRunStart );
					SIZE	szRun = { 0, 0 };
					::GetTextExtentPoint32( hdc, sRun.c_str(), (int)sRun.size(), &szRun );
					::SetTextColor( hdc, bCovered ? RGB( 0, 150, 0 ) : crText );
					int	xRunEnd = ( xCur + szRun.cx < nNameRightEdge ) ? ( xCur + szRun.cx ) : nNameRightEdge;
					RECT	rcRun = { xCur, rc.top, xRunEnd, rc.bottom };
					::DrawText( hdc, sRun.c_str(), -1, &rcRun, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX );
					xCur += szRun.cx;
				}
			}
#else
			{
				// VSCodeのクイックオープン風に、絞り込み文字列に一致した部分だけ色を変えて
				// 描く。m_sLastQueryはUpdateList()が絞り込みに使った実クエリ(先頭の">"等の
				// 接頭辞を除き小文字化済み)。ここではローマ字/漢字読み変換を伴うあいまい
				// 一致までは追わず、表示名に対する素直な部分文字列一致だけを探す
				// (あいまい一致の場所を正規化前後で対応付けるのは割に合わない)。
				// 見つからなければ従来通りハイライト無しで描く 20260821
				std::wstring	sNameLower = row.sName;
				for( size_t k = 0; k < sNameLower.size(); ++k ){ sNameLower[k] = (wchar_t)::towlower( sNameLower[k] ); }
				size_t	nMatchPos = m_sLastQuery.empty() ? std::wstring::npos : sNameLower.find( m_sLastQuery );

				if( std::wstring::npos == nMatchPos ){
					::SetTextColor( hdc, crText );
					::DrawText( hdc, row.sName.c_str(), -1, &rcName, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS );
				}else{
					// マッチ箇所を挟んだ3分割描画。DT_END_ELLIPSISは複数回のDrawText呼び出しを
					// またいでは効かないため、この経路では末尾省略ができなくなるが、パレットの
					// 表示名は元々短いものがほとんどで実害は小さい 20260821
					// 選択行が反転フォールバック(bThemedSelection==false)のときは、濃い
					// COLOR_HIGHLIGHT地に別の青を重ねると読みにくくなるため、ハイライト色を
					// 使わず地の文字色(白反転)のまま揃える 20260821
					COLORREF	crMatch = ( bSelected && !bThemedSelection ) ? crText : ::GetSysColor( COLOR_HOTLIGHT );
					std::wstring	sPre   = row.sName.substr( 0, nMatchPos );
					std::wstring	sMatch = row.sName.substr( nMatchPos, m_sLastQuery.size() );
					std::wstring	sPost  = row.sName.substr( nMatchPos + m_sLastQuery.size() );

					int	xCur = x;
					if( !sPre.empty() && xCur < nNameRightEdge ){
						SIZE	sz = { 0, 0 };
						::GetTextExtentPoint32( hdc, sPre.c_str(), (int)sPre.size(), &sz );
						RECT	rcPre = { xCur, rc.top, ( xCur + sz.cx < nNameRightEdge ) ? xCur + sz.cx : nNameRightEdge, rc.bottom };
						::SetTextColor( hdc, crText );
						::DrawText( hdc, sPre.c_str(), -1, &rcPre, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX );
						xCur += sz.cx;
					}
					if( !sMatch.empty() && xCur < nNameRightEdge ){
						SIZE	sz = { 0, 0 };
						::GetTextExtentPoint32( hdc, sMatch.c_str(), (int)sMatch.size(), &sz );
						RECT	rcMatch = { xCur, rc.top, ( xCur + sz.cx < nNameRightEdge ) ? xCur + sz.cx : nNameRightEdge, rc.bottom };
						::SetTextColor( hdc, crMatch );
						::DrawText( hdc, sMatch.c_str(), -1, &rcMatch, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX );
						xCur += sz.cx;
					}
					if( !sPost.empty() && xCur < nNameRightEdge ){
						RECT	rcPost = { xCur, rc.top, nNameRightEdge, rc.bottom };
						::SetTextColor( hdc, crText );
						::DrawText( hdc, sPost.c_str(), -1, &rcPost, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX );
					}
				}
			}
#endif // NKMM_DEBUG_COMMAND_PALETTE_KANJI_COVERAGE

			// グレーのサブ情報(ファイル系(ウィンドウ/最近使ったファイル)だけ格納フォルダを表示。
			// コマンド/アウトラインには付けない)。名前より小さいフォント(m_hFontSub)にし、
			// 色だけでなく大きさでもファイル名との違いを一目でわかるようにする 20260819 20260821
			if( ( ROWKIND_WINDOW == row.kind || ROWKIND_RECENT == row.kind ) && !row.sSub.empty() ){
				size_t	nSlash = row.sSub.find_last_of( L"\\/" );
				std::wstring	sDir = ( std::wstring::npos == nSlash ) ? L"" : row.sSub.substr( 0, nSlash );
				int	xSub = rcName.right + nPad;
				if( !sDir.empty() && xSub < nNameRight ){
					RECT	rcSub = { xSub, rc.top, nNameRight, rc.bottom };
					::SetTextColor( hdc, crSub );
					HFONT	hFontListSaved = ( NULL != m_hFontSub ) ? (HFONT)::SelectObject( hdc, m_hFontSub ) : NULL;
					::DrawText( hdc, sDir.c_str(), -1, &rcSub, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS );
					if( NULL != hFontListSaved ){
						::SelectObject( hdc, hFontListSaved );
					}
				}
			}

			if( NULL != hOldFont ){
				::SelectObject( hdc, hOldFont );
			}

			return CDRF_SKIPDEFAULT;
		}
	}
	return CDRF_DODEFAULT;
}


/*! パスの拡張子から共有システムアイコン一覧の索引を得る。拡張子ごとにキャッシュし、
	SHGetFileInfo()の呼び出し(シェルへの問い合わせ)を毎回の再描画で繰り返さないようにする。
	SHGFI_USEFILEATTRIBUTESにより実ファイルへのディスクアクセスは発生しない 20260819
*/
int CDlgCommandPalette::GetShellIconIndex( const std::wstring& sPath )
{
	std::wstring	sExt;
	size_t	nDot = sPath.find_last_of( L'.' );
	size_t	nSlash = sPath.find_last_of( L"\\/" );
	if( std::wstring::npos != nDot && ( std::wstring::npos == nSlash || nDot > nSlash ) ){
		sExt = sPath.substr( nDot );
		for( size_t i = 0; i < sExt.size(); ++i ){ sExt[i] = (wchar_t)::towlower( sExt[i] ); }
	}

	std::map<std::wstring, int>::const_iterator	it = m_mapExtToIconIndex.find( sExt );
	if( m_mapExtToIconIndex.end() != it ){
		return it->second;
	}

	std::wstring	sDummy = L"dummy" + sExt;
	SHFILEINFO	sfi = {};
	::SHGetFileInfo( sDummy.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof( sfi ),
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES );
	m_mapExtToIconIndex[sExt] = sfi.iIcon;
	return sfi.iIcon;
}


BOOL CDlgCommandPalette::OnDestroy()
{
	if( NULL != m_hFontList ){
		::DeleteObject( m_hFontList );
		m_hFontList = NULL;
	}
	if( NULL != m_hFontSub ){
		::DeleteObject( m_hFontSub );
		m_hFontSub = NULL;
	}
	if( NULL != m_hThemeListView ){
		CUxTheme::getInstance()->CloseThemeData( m_hThemeListView );
		m_hThemeListView = NULL;
	}
	return CDialog::OnDestroy();
}


//! フルパスからファイル名部分だけを取り出す(区切りが無ければそのまま返す)
static std::wstring ExtractFileName( const std::wstring& sPath )
{
	size_t	nPos = sPath.find_last_of( L"\\/" );
	return ( std::wstring::npos == nPos ) ? sPath : sPath.substr( nPos + 1 );
}


/*! コマンド一覧(CFuncLookup経由、ショートカットはCKeyBind::GetKeyStr)、
	現在開いているウィンドウ一覧(CAppNodeManager、タブ=別プロセスをまたいで列挙)、
	最近使ったファイル一覧(CMRUFile)を、それぞれm_vAllRowsへまとめる。
	絞り込み(UpdateList)のたびには呼ばない(ダイアログを開いたときの1回だけ) 20260818
*/
void CDlgCommandPalette::BuildAllRows()
{
	m_vAllRows.clear();

	if( NULL != m_pcFuncLookup ){
		CommonSetting_KeyBind&	sKeyBind = GetDllShareData().m_Common.m_sKeyBind;
		int	nCategoryCount = m_pcFuncLookup->GetCategoryCount();
		for( int nCategory = 0; nCategory < nCategoryCount; ++nCategory ){
			int	nItemCount = m_pcFuncLookup->GetItemCount( nCategory );
			for( int nPos = 0; nPos < nItemCount; ++nPos ){
				// bGetUnavailable=falseで、未登録の外部マクロ枠は一覧から除く
				EFunctionCode	nFuncCode = m_pcFuncLookup->Pos2FuncCode( nCategory, nPos, false );
				if( F_DISABLE == nFuncCode ){
					continue;
				}

				WCHAR	szName[256] = L"";
				if( !m_pcFuncLookup->Funccode2Name( nFuncCode, szName, _countof(szName) ) || L'\0' == szName[0] ){
					continue;
				}

				PaletteRow	row;
				row.kind      = ROWKIND_COMMAND;
				row.nPostId   = (int)nFuncCode;
				row.hwndFile  = NULL;
				row.sName     = szName;
				row.sType     = L"コマンド";

				wchar_t	szId[16];
				auto_sprintf( szId, L"%d", (int)nFuncCode );
				row.sId = szId;

				CNativeT	cKeyStr;
				if( 0 != CKeyBind::GetKeyStr( G_AppInstance(), sKeyBind.m_nKeyNameArrNum, (KEYDATA*)sKeyBind.m_pKeyNameArr, cKeyStr, (int)nFuncCode ) ){
					row.sSub = cKeyStr.GetStringPtr();
				}

				m_vAllRows.push_back( row );
			}
		}
	}

	EditNode*	pEditNodeArr = NULL;
	int	nRowNum = CAppNodeManager::getInstance()->GetOpenedWindowArr( &pEditNodeArr, TRUE );
	for( int i = 0; i < nRowNum; ++i ){
		PaletteRow	row;
		row.kind      = ROWKIND_WINDOW;
		row.nPostId   = 0;
		row.hwndFile  = pEditNodeArr[i].GetHwnd();
		row.sName     = pEditNodeArr[i].m_szTabCaption;
		row.sType     = L"ウィンドウ";
		if( L'\0' != pEditNodeArr[i].m_szFilePath[0] ){
			row.sSub = pEditNodeArr[i].m_szFilePath.c_str();
		}
		m_vAllRows.push_back( row );
	}
	if( 0 < nRowNum ){
		delete [] pEditNodeArr;
	}

	// 最近使ったファイル(共通設定の「最近使ったファイル」履歴と同じ内容。
	// IDM_SELMRU+i をWM_COMMANDで送ると、CEditWnd::OnCommand()が同じ番号で
	// 実際に開く処理を行う(ウィンドウ一覧のIDM_SELWINDOWと同じ仕組み) 20260819
	{
		const CMRUFile	cMRU;
		int	nMruCount = cMRU.Length();
		for( int i = 0; i < nMruCount; ++i ){
			EditInfo	fi;
			if( !cMRU.GetEditInfo( i, &fi ) || L'\0' == fi.m_szPath[0] ){
				continue;
			}

			PaletteRow	row;
			row.kind      = ROWKIND_RECENT;
			row.nPostId   = IDM_SELMRU + i;
			row.hwndFile  = NULL;
			row.sSub      = fi.m_szPath;
			row.sName     = ExtractFileName( row.sSub );
			row.sType     = L"最近使ったファイル";

			m_vAllRows.push_back( row );
		}
	}
}


/*! 「@」モードへ実際に切り替えたときだけ、現在アクティブなビューの文書種別に
	応じたアウトライン解析(CViewCommander::Command_FUNCLIST/Ctrl+F11の
	アウトラインダイアログと同じCDocOutlineの解析ルーチン)を1回だけ実行し、
	結果をm_vAllRowsへ追加する。コマンド/ウィンドウ/最近使ったファイルと違い
	文書全体の走査を伴うため、BuildAllRows()(パレットを開くたび)には含めない。
	プラグイン(WSH)によるアウトライン解析は、コマンドパレット側にその呼び出し
	土台(CJackManager等)を持ち込む必要が生じるため対象外とし、通常のCtrl+F11
	ダイアログと同じくプラグインが担当する種別・ルールファイル種別は、素の
	テキスト・トピック解析(MakeTopicList_txt)にフォールバックする 20260821
*/
void CDlgCommandPalette::BuildOutlineRows()
{
	m_bOutlineRowsBuilt = true;

	if( NULL == m_pcView || NULL == m_pcView->m_pTypeData || NULL == m_pcView->GetDocument() ){
		return;
	}
	CEditDoc*	pcDoc = m_pcView->GetDocument();

	EOutlineType	nOutlineType = m_pcView->m_pTypeData->m_eDefaultOutline;
#ifdef NKMM_FIX_OUTLINE
	// OUTLINE_C_CPP(「C/C++」自動判別)をファイル拡張子からOUTLINE_C/OUTLINE_CPPへ
	// 解決しておく(CViewCommander::Command_FUNCLISTと同じ理由) 20260821
	nOutlineType = CDocOutline::ResolveOutlineType_C_CPP( nOutlineType, pcDoc->m_cDocFile.GetFilePath() );
#endif // NKMM_

	CFuncInfoArr	cFuncInfoArr;
	switch( nOutlineType ){
	case OUTLINE_C:
	case OUTLINE_C_CPP:
	case OUTLINE_CPP:
		pcDoc->m_cDocOutline.MakeFuncList_C( &cFuncInfoArr, nOutlineType, pcDoc->m_cDocFile.GetFilePath() );
		break;
	case OUTLINE_PLSQL:		pcDoc->m_cDocOutline.MakeFuncList_PLSQL( &cFuncInfoArr );			break;
	case OUTLINE_JAVA:		pcDoc->m_cDocOutline.MakeFuncList_Java( &cFuncInfoArr );			break;
	case OUTLINE_COBOL:		pcDoc->m_cDocOutline.MakeTopicList_cobol( &cFuncInfoArr );			break;
	case OUTLINE_ASM:		pcDoc->m_cDocOutline.MakeTopicList_asm( &cFuncInfoArr );			break;
	case OUTLINE_PERL:		pcDoc->m_cDocOutline.MakeFuncList_Perl( &cFuncInfoArr );			break;
	case OUTLINE_VB:		pcDoc->m_cDocOutline.MakeFuncList_VisualBasic( &cFuncInfoArr );	break;
	case OUTLINE_WZTXT:		pcDoc->m_cDocOutline.MakeTopicList_wztxt( &cFuncInfoArr );			break;
	case OUTLINE_HTML:		pcDoc->m_cDocOutline.MakeTopicList_html( &cFuncInfoArr, false );	break;
	case OUTLINE_TEX:		pcDoc->m_cDocOutline.MakeTopicList_tex( &cFuncInfoArr );			break;
	case OUTLINE_PYTHON:	pcDoc->m_cDocOutline.MakeFuncList_python( &cFuncInfoArr );			break;
	case OUTLINE_ERLANG:	pcDoc->m_cDocOutline.MakeFuncList_Erlang( &cFuncInfoArr );			break;
	case OUTLINE_XML:		pcDoc->m_cDocOutline.MakeTopicList_html( &cFuncInfoArr, true );	break;
	case OUTLINE_BOOKMARK:	pcDoc->m_cDocOutline.MakeFuncList_BookMark( &cFuncInfoArr );		break;
	case OUTLINE_FILETREE:	/* ファイルツリーはジャンプ先の行を持たないため対象外 */			break;
	default:
		pcDoc->m_cDocOutline.MakeTopicList_txt( &cFuncInfoArr );
		break;
	}

	int	nNum = cFuncInfoArr.GetNum();
	for( int i = 0; i < nNum; ++i ){
		CFuncInfo*	pcInfo = cFuncInfoArr.GetAt( i );
		if( NULL == pcInfo || 0 == pcInfo->m_cmemFuncName.GetStringLength() ){
			continue;
		}
		// 他ファイルを指す要素(ブックマーク等の一部)は、現在の文書内へのジャンプでは
		// 扱えないため一覧から除く 20260821
		if( 0 < pcInfo->m_cmemFileName.GetStringLength() ){
			continue;
		}

		PaletteRow	row;
		row.kind        = ROWKIND_OUTLINE;
		row.nPostId     = (int)pcInfo->m_nFuncLineCRLF;
		row.nOutlineCol = (int)pcInfo->m_nFuncColCRLF;
		row.hwndFile    = NULL;
		row.sName       = pcInfo->m_cmemFuncName.GetStringPtr();
		row.sType       = L"アウトライン";

		m_vAllRows.push_back( row );
	}
}


/*! 「#」モードへ実際に切り替えたときだけ、現在アクティブなビューの文書の
	ブックマーク一覧(CViewCommander::Command_FUNCLIST(OUTLINE_BOOKMARK)/
	ブックマーク一覧ダイアログと同じCDocOutline::MakeFuncList_BookMark)を
	1回だけ取得し、結果をm_vAllRowsへ追加する。アウトライン解析と違い
	文書の構文種別に関係なく常に同じ関数を呼べるため、型の判定は不要 20260821
*/
void CDlgCommandPalette::BuildBookmarkRows()
{
	m_bBookmarkRowsBuilt = true;

	if( NULL == m_pcView || NULL == m_pcView->GetDocument() ){
		return;
	}
	CEditDoc*	pcDoc = m_pcView->GetDocument();

	CFuncInfoArr	cFuncInfoArr;
	pcDoc->m_cDocOutline.MakeFuncList_BookMark( &cFuncInfoArr );

	int	nNum = cFuncInfoArr.GetNum();
	for( int i = 0; i < nNum; ++i ){
		CFuncInfo*	pcInfo = cFuncInfoArr.GetAt( i );
		if( NULL == pcInfo ){
			continue;
		}
		if( 0 < pcInfo->m_cmemFileName.GetStringLength() ){
			continue;
		}

		PaletteRow	row;
		row.kind        = ROWKIND_BOOKMARK;
		row.nPostId     = (int)pcInfo->m_nFuncLineCRLF;
		row.nOutlineCol = (int)pcInfo->m_nFuncColCRLF;
		row.hwndFile    = NULL;
		// 空行につけたブックマークは行の内容(前後空白を除いた文字列)が空になるため、
		// アウトラインの「名前が空なら除外」とは違いここでは除外せず、それとわかる
		// プレースホルダを出す(除外すると、その行だけジャンプできなくなってしまう) 20260821
		row.sName       = ( 0 < pcInfo->m_cmemFuncName.GetStringLength() ) ? pcInfo->m_cmemFuncName.GetStringPtr() : L"(空行)";
		row.sType       = L"ブックマーク";

		m_vAllRows.push_back( row );
	}
}


/*! 絞り込み欄の文字列で一覧を再構築する 20260818

	先頭の記号で対象を切り替える(VSCode風)。
	  「>」    : コマンドのみ
	  「edt 」 : 開いているウィンドウのみ(edtの後に半角空白が必須)
	  「@」    : 現在の文書のアウトライン解析結果のみ 20260821
	  「#」    : 現在の文書のブックマーク一覧のみ 20260821
	  それ以外(空欄を含む) : 最近使ったファイル(既定) 20260819
	記号の後ろに続く文字列は、名前に対する部分一致(大文字小文字無視)の絞り込みに使う 20260818
*/
void CDlgCommandPalette::UpdateList()
{
	HWND	hListView = GetItemHwnd( IDC_LIST_COMMANDPALETTE );

	WCHAR	szFilter[256] = L"";
	::GetWindowText( GetItemHwnd( IDC_EDIT_COMMANDPALETTE_FILTER ), szFilter, _countof(szFilter) );
	std::wstring	sFilter( szFilter );

#ifdef NKMM_COMMAND_PALETTE_ROMAJI
	// IME変換中(未確定文字列)はGetWindowText()にまだ反映されない(EDITコントロールは
	// 確定するまで実際のテキストバッファへ書き込まない)ため、ImmGetCompositionStringW()で
	// 直接読み取って末尾へ補う。これが無いと変換中は絞り込み文字列が空のまま扱われ、
	// 全件表示されてしまう 20260819
	{
		HWND	hEditFilter = GetItemHwnd( IDC_EDIT_COMMANDPALETTE_FILTER );
		HIMC	hImc = ::ImmGetContext( hEditFilter );
		if( NULL != hImc ){
			if( FALSE != ::ImmGetOpenStatus( hImc ) ){
				LONG	nBytes = ::ImmGetCompositionStringW( hImc, GCS_COMPSTR, NULL, 0 );
				if( 0 < nBytes ){
					std::vector<wchar_t>	vComp( nBytes / sizeof( wchar_t ) + 1, L'\0' );
					::ImmGetCompositionStringW( hImc, GCS_COMPSTR, vComp.data(), nBytes );
					sFilter += vComp.data();
				}
			}
			::ImmReleaseContext( hEditFilter, hImc );
		}
	}
#endif // NKMM_COMMAND_PALETTE_ROMAJI

	for( size_t i = 0; i < sFilter.size(); ++i ){ sFilter[i] = (wchar_t)::towlower( sFilter[i] ); }

	EPaletteRowKind	eMode = ROWKIND_RECENT;
	std::wstring	sQuery = sFilter;
	if( !sFilter.empty() && L'>' == sFilter[0] ){
		eMode = ROWKIND_COMMAND;
		sQuery = sFilter.substr( 1 );
	}else if( 0 == sFilter.compare( 0, wcslen( szWindowPrefix ), szWindowPrefix ) ){
		eMode = ROWKIND_WINDOW;
		sQuery = sFilter.substr( wcslen( szWindowPrefix ) );
	}else if( !sFilter.empty() && L'@' == sFilter[0] ){
		eMode = ROWKIND_OUTLINE;
		sQuery = sFilter.substr( 1 );
	}else if( !sFilter.empty() && L'#' == sFilter[0] ){
		eMode = ROWKIND_BOOKMARK;
		sQuery = sFilter.substr( 1 );
	}
	// 記号の直後に残った先頭の空白は絞り込み対象から除く(「> save」のような入力を素直に扱うため)
	size_t	nQueryBegin = sQuery.find_first_not_of( L' ' );
	sQuery = ( std::wstring::npos == nQueryBegin ) ? L"" : sQuery.substr( nQueryBegin );

	// OnListCustomDraw()が一致部分のハイライトに使う。あいまい検索(ローマ字/漢字の
	// 読み展開)でヒットしていても、ここでは表示名に対する単純な部分文字列一致だけを
	// ハイライトの対象にする(あいまい一致した箇所すべてを厳密に追うのは正規化前後の
	// 位置対応が複雑になるため割に合わない。VSCode等でも同種の割り切りは一般的) 20260821
	m_sLastQuery = sQuery;

	// アウトライン解析・ブックマーク取得は他の一覧と違って現在の文書の走査を伴うため、
	// 実際にそのモードへ切り替えたときだけ1回遅延実行する(BuildAllRows()では行わない) 20260821
	if( ROWKIND_OUTLINE == eMode && !m_bOutlineRowsBuilt ){
		BuildOutlineRows();
	}else if( ROWKIND_BOOKMARK == eMode && !m_bBookmarkRowsBuilt ){
		BuildBookmarkRows();
	}

	// 一覧はLVS_OWNERDATA(仮想リスト)。以前はListView_DeleteAllItems()+行ごとの
	// ListView_InsertItem()で組み立てていたが、行数が数千〜数万になると1件ごとの
	// LVN_INSERTITEM往復コストが支配的になり(実測: 1万件で約1.8秒)、キー入力のたびに
	// 固まって見えていた。LVS_OWNERDATAでは実際の行データ(m_vAllRows)を渡さず、
	// 該当する添字の一覧(m_vMatchedRowIndices)とItemCountだけを伝え、表示に必要な
	// 分だけLVN_GETDISPINFO/NM_CUSTOMDRAWで都度引き当てる(可視行数分、数十件程度)。
	// これによりファイルサイズによらず絞り込みが一定時間で終わるようになる 20260821
	m_vMatchedRowIndices.clear();

	// 絞り込み文字列がある間だけ、一致の良さ(スコア)で並べ替える。空欄時は
	// BuildAllRows()が積んだ元の並び順(コマンドはカテゴリ順、最近使ったファイルは
	// 履歴順)をそのまま使う 20260820
#ifdef NKMM_COMMAND_PALETTE_ROMAJI
	std::vector<std::pair<int, size_t> >	vScoredRowIndices;	// (スコア, m_vAllRows添字)
#endif // NKMM_COMMAND_PALETTE_ROMAJI

	for( size_t i = 0; i < m_vAllRows.size(); ++i ){
		const PaletteRow&	row = m_vAllRows[i];

		if( row.kind != eMode ){ continue; }

		if( sQuery.empty() ){
			m_vMatchedRowIndices.push_back( i );
			continue;
		}

#ifdef NKMM_COMMAND_PALETTE_ROMAJI
		// IME未使用でもローマ字入力のまま日本語のコマンド名を絞り込めるように、
		// ローマ字→かな/カナ/(有効時は)漢字の表記ゆれを吸収したあいまい一致を使う。
		// 連続一致・単語先頭一致ほど高くなるスコアも受け取り、後でスコア降順に
		// 並べ替える(最もタイトに一致したものを上に出す) 20260819 20260820
		int	nScore = 0;
		if( !FuzzyMatchJapanese( sQuery, row.sName, &nScore ) ){
			continue;
		}
		vScoredRowIndices.push_back( std::make_pair( nScore, i ) );
#else
		std::wstring	sHay = row.sName;
		for( size_t j = 0; j < sHay.size(); ++j ){ sHay[j] = (wchar_t)::towlower( sHay[j] ); }
		if( std::wstring::npos == sHay.find( sQuery ) ){
			continue;
		}
		m_vMatchedRowIndices.push_back( i );
#endif // NKMM_COMMAND_PALETTE_ROMAJI
	}

#ifdef NKMM_COMMAND_PALETTE_ROMAJI
	if( !sQuery.empty() ){
		std::stable_sort( vScoredRowIndices.begin(), vScoredRowIndices.end(),
			[]( const std::pair<int, size_t>& a, const std::pair<int, size_t>& b ){ return a.first > b.first; } );
		for( size_t k = 0; k < vScoredRowIndices.size(); ++k ){ m_vMatchedRowIndices.push_back( vScoredRowIndices[k].second ); }
	}
#endif // NKMM_COMMAND_PALETTE_ROMAJI

	// 新しい行数を伝える前に選択状態を全解除しておく(件数が減った場合に、もう
	// 存在しない添字の選択状態が残ったまま次回に持ち越されるのを防ぐ) 20260821
	ListView_SetItemState( hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED );
	ListView_SetItemCount( hListView, (int)m_vMatchedRowIndices.size() );

	if( !m_vMatchedRowIndices.empty() ){
		ListView_SetItemState( hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		ListView_EnsureVisible( hListView, 0, FALSE );
		// OnNotify(LVN_ITEMCHANGED)経由だと、一覧をまるごと作り直した直後の自動選択に対して
		// 通知が来ないことがあったため、ここで直接呼ぶ 20260821
		LivePreviewSelection( 0 );
	}

	AdjustListHeight();
}


/*! 絞り込み結果の件数がスクロールを要しない(一覧の最大高さに収まる)ときは、
	一覧とダイアログの高さをその件数ぶんまで狭める。結果が増えて収まらなくなれば、
	sakura_rc.rcのダイアログテンプレート設計値(m_nMaxListHeight)まで戻す。
	VSCodeのクイックオープン等と同じく、少数の結果しかないのに下に無駄な余白が
	残るのを避けるため 20260821
*/
void CDlgCommandPalette::AdjustListHeight()
{
	if( 0 == m_nMaxListHeight ){
		return;
	}
	HWND	hListView = GetItemHwnd( IDC_LIST_COMMANDPALETTE );

	int	nCount = (int)m_vMatchedRowIndices.size();

	// 一覧1行の実際の高さ(アイコンサイズ・m_hFontListから一覧生成時に確定し、
	// 以後は変化しない)は、初めて1件以上表示された時点で一度だけ求めてキャッシュする 20260821
	if( 0 == m_nListRowHeight && 0 < nCount ){
		RECT	rcItem;
		if( ListView_GetItemRect( hListView, 0, &rcItem, LVIR_BOUNDS ) ){
			m_nListRowHeight = rcItem.bottom - rcItem.top;
		}
	}

	int	nDesiredListHeight = m_nMaxListHeight;
	if( 0 < m_nListRowHeight ){
		int	nNaturalHeight = nCount * m_nListRowHeight;
		if( nNaturalHeight < m_nMaxListHeight ){
			nDesiredListHeight = nNaturalHeight;
		}
	}

	RECT	rcListNow;
	::GetWindowRect( hListView, &rcListNow );
	if( ( rcListNow.bottom - rcListNow.top ) == nDesiredListHeight ){
		return;	// 変化なし
	}
	int	nListWidth = rcListNow.right - rcListNow.left;
	::SetWindowPos( hListView, NULL, 0, 0, nListWidth, nDesiredListHeight, SWP_NOMOVE | SWP_NOZORDER );

	// ダイアログ本体も、一覧を除いた固定分(m_nChromeHeight)+新しい一覧の高さに
	// 合わせて縮める。位置(左上)はSWP_NOMOVEで変えない(上端固定でスライドインした
	// 位置からそのまま下端だけが動く) 20260821
	RECT	rcWndNow;
	::GetWindowRect( GetHwnd(), &rcWndNow );
	int	nWndWidth = rcWndNow.right - rcWndNow.left;
	::SetWindowPos( GetHwnd(), NULL, 0, 0, nWndWidth, m_nChromeHeight + nDesiredListHeight, SWP_NOMOVE | SWP_NOZORDER );
}


/*! 一覧で選択中の行を実行する。コマンドと最近使ったファイルは、呼び出し元
	ウィンドウへWM_COMMANDを送って通常のメニュー選択と同じ経路
	(コマンドはCViewCommander::HandleCommand、最近使ったファイルは
	CEditWnd::OnCommand()のIDM_SELMRU処理)で実行させる(ダイアログを開いたまま
	直接呼び出さない)。開いているウィンドウはActivateFrameWindow()で
	(別プロセスでも)アクティブ化する 20260818
	アウトライン/ブックマークは、同じ文書内の位置へキャレットを移すだけなので、
	CDlgFuncList::OnJump()の「同じファイル内」の分岐と同じ方式
	(m_sWorkBuffer.m_LogicPointを設定してMYWM_SETCARETPOSを送る)で移動させる。
	選択状態を保つPM_SETCARETPOS_KEEPSELECTを付け、カーソル移動で選択範囲が
	消えてしまわないようにする 20260821
*/
void CDlgCommandPalette::ExecuteRow( const PaletteRow& row )
{
	switch( row.kind ){
	case ROWKIND_WINDOW:
		if( NULL != row.hwndFile && ::IsWindow( row.hwndFile ) ){
			ActivateFrameWindow( row.hwndFile );
		}
		break;
	case ROWKIND_COMMAND:
	case ROWKIND_RECENT:
		if( NULL != m_hwndParent && ::IsWindow( m_hwndParent ) ){
			::PostMessage( m_hwndParent, WM_COMMAND, MAKELONG( (WORD)row.nPostId, 0 ), 0 );
		}
		break;
	case ROWKIND_OUTLINE:
	case ROWKIND_BOOKMARK:
		// Enterで確定した時点では、既にLivePreviewSelection()で同じ位置へ
		// 移動済みのはずだが、経路の単純化のためここでも同じ処理を呼んでおく
		// (同じ位置への再移動は無害) 20260821
		JumpToRow( row );
		break;
	}
}


/*! kind==ROWKIND_OUTLINE/BOOKMARKの行が指す位置へ、現在の文書内でカーソルを移動する。
	CDlgFuncList::OnJump()の「同じファイル内」の分岐と同じ方式
	(m_sWorkBuffer.m_LogicPointを設定してMYWM_SETCARETPOSを送る)を使う 20260821
*/
void CDlgCommandPalette::JumpToRow( const PaletteRow& row )
{
	MoveCaretTo( row.nOutlineCol - 1, row.nPostId - 1 );
}


//! 現在の文書内でカーソルを指定位置(0開始)へ移動する。選択状態を保つ
//! PM_SETCARETPOS_KEEPSELECTを付け、カーソル移動で選択範囲が消えてしまわないようにする 20260821
void CDlgCommandPalette::MoveCaretTo( int nLogicX, int nLogicY )
{
	if( NULL == m_pcView || NULL == m_pcView->m_pcEditWnd ){
		return;
	}
	CLogicPoint	poCaret;
	poCaret.x = nLogicX;
	poCaret.y = nLogicY;
	GetDllShareData().m_sWorkBuffer.m_LogicPoint = poCaret;
	::SendMessageAny( m_pcView->m_pcEditWnd->GetHwnd(), MYWM_SETCARETPOS, 0, PM_SETCARETPOS_KEEPSELECT );
}


/*! 一覧の選択行が変わるたびに呼ばれる(UpdateList()での絞り込みによる自動選択、
	MoveSelection()での矢印キーによる移動、OnNotify(LVN_ITEMCHANGED)でのマウス
	クリックによる選択、いずれの経路からも同じここへ集約する)。アウトライン/
	ブックマークの行を選択している間は、Enterで確定するのを待たずにその位置へ
	カーソルをライブ移動する(VSCodeのGo to Symbol風のプレビュー)。コマンド/ウィンドウ/
	最近使ったファイルの行は、選択しただけでコマンド実行やウィンドウ切り替えが起きると
	困るため対象外 20260821

	選択行そのものの記録(m_nSelectedDispIndex)も、選択が変わる経路をすべて集約している
	ここでまとめて行う。OnListCustomDraw()での選択色描画にこれを使う(comctl32の
	ListView_GetItemState()に問い合わせない)理由は関数末尾のコメント参照 20260821
*/
void CDlgCommandPalette::LivePreviewSelection( int nItemIndex )
{
	m_nSelectedDispIndex = nItemIndex;

	// LVS_OWNERDATA(仮想リスト)にはlParamが無いため、表示上の行番号(nItemIndex)を
	// m_vMatchedRowIndices経由でm_vAllRowsの添字に変換する 20260821
	if( nItemIndex < 0 || (size_t)nItemIndex >= m_vMatchedRowIndices.size() ){
		return;
	}
	size_t	nRowIndex = m_vMatchedRowIndices[(size_t)nItemIndex];
	if( nRowIndex >= m_vAllRows.size() ){
		return;
	}

	const PaletteRow&	row = m_vAllRows[nRowIndex];
	if( ROWKIND_OUTLINE == row.kind || ROWKIND_BOOKMARK == row.kind ){
		JumpToRow( row );
	}
}


void CDlgCommandPalette::ExecuteSelected()
{
	// LVS_OWNERDATA化後はListView_GetNextItem(LVNI_SELECTED)も信用せず、
	// LivePreviewSelection()で自前追跡しているm_nSelectedDispIndexを使う
	// (OnListCustomDraw/MoveSelectionと同じ理由) 20260821
	int	nSel = m_nSelectedDispIndex;
	if( nSel < 0 || (size_t)nSel >= m_vMatchedRowIndices.size() ){
		return;
	}

	// LVS_OWNERDATA(仮想リスト)にはlParamが無いため、表示上の行番号(nSel)を
	// m_vMatchedRowIndices経由でm_vAllRowsの添字に変換する 20260821
	size_t	nRowIndex = m_vMatchedRowIndices[(size_t)nSel];
	if( nRowIndex >= m_vAllRows.size() ){
		return;
	}
	ExecuteRow( m_vAllRows[nRowIndex] );
}

/*! 非アクティブ化されたときに親ウィンドウをアクティブに戻してから自滅する(ESC相当) 20260819

	ここでSetForegroundWindow()等やDestroyWindow()を直接呼ばないのは、IDOK/IDCANCEL側の
	CloseDialog()が先にDestroyWindow()を呼んでいる場合、その内部処理としてこのWM_ACTIVATEが
	再入的に(同じDestroyWindow呼び出しの最中に)飛んでくることがあり、ここでさらに
	DestroyWindow()を呼ぶと二重破棄になってしまうため。また、フォーカス喪失による
	自滅の場合も、OS側のアクティブ化遷移が完了する前にSetForegroundWindow()すると、
	直後にOS側の処理で別のウィンドウへ活性が戻ってしまい、結果的にsakura本体が
	後ろに隠れてしまう。どちらもタイマーで1回分メッセージループを回してから
	CloseOnDeactivate()で実処理することで回避する 20260819
*/
BOOL CDlgCommandPalette::OnActivate( WPARAM wParam, LPARAM lParam )
{
	// 基底クラスの処理を先に呼ぶ
    CDialog::OnActivate( wParam, lParam );

    if( WA_INACTIVE == LOWORD(wParam) && NULL != GetHwnd() ){
        // フォーカスがどこへ移った場合であっても(別アプリへ移った場合も含めて)、
        // 自滅と同時に親ウィンドウ(エディタ)を前面に戻す。そうしないと、
        // 特に他プロセス(VS等)からフォーカスを奪われて閉じたケースで、
        // 親ウィンドウがそのまま後ろに隠れたままになってしまう。
        m_bReactivateParentOnClose = ( NULL != m_hwndParent && ::IsWindow( m_hwndParent ) );

        ::SetTimer( GetHwnd(), ID_TIMER_PALETTE_DEFERRED_CLOSE, USER_TIMER_MINIMUM, NULL );
    }

    return TRUE;
}


/*! OnActivate(WA_INACTIVE)を受けての実処理。タイマー経由で1回分メッセージループを
	回した後に呼ばれるため、CloseDialog()側のDestroyWindow()とは再入しない。
	既にCloseDialog()等で破棄済み(GetHwnd()==NULL)なら何もしない 20260819
*/
void CDlgCommandPalette::CloseOnDeactivate()
{
	if( NULL == GetHwnd() ){
		return;
	}

	if( m_bReactivateParentOnClose && NULL != m_hwndParent && ::IsWindow( m_hwndParent ) ){
		::SetForegroundWindow( m_hwndParent );
		::SetActiveWindow( m_hwndParent );
		::SetFocus( m_hwndParent );
		::SetWindowPos( m_hwndParent, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	}

	::DestroyWindow( GetHwnd() );
}

#endif // NKMM_COMMAND_PALETTE
