/*!	@file
	@brief 共通設定ダイアログボックス、「ショートカット一覧」ページ

	@date 20260803 新規作成
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_KEYBIND_LIST_TAB

#include "prop/CPropCommon.h"
#include "func/CKeyBind.h"
#include "env/CShareData.h"	// CShareData::ResetKeyBindToDefault (初期化ボタン用)
#include "typeprop/CImpExpManager.h"	// CImpExpKeybind (インポート/エクスポートボタン用)
#include "util/os.h"	// PreventVisualStyle (カテゴリ区切り行・キー警告色の背景色プレビュー用)
#include "sakura_rc.h"
#include "sakura.hh"

#include <string>
#include <vector>

#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
//! 組み込みキー割り当てプリセット1件分の情報 20260812
struct SKeybindPresetInfo {
	int				nResId;		//!< sakura_rc.rcのRCDATA(IDR_KEYBINDPRESET_*)のリソースID。
									//!< 0の場合はインポートせず初期化のみ行う(「サクラエディタ」用) 20260813
	const wchar_t*	pszName;	//!< プリセット選択コンボの表示名
};

/*! 組み込みキー割り当てプリセットの一覧 20260812

	元データはリポジトリ直下のkeybind_presets/*.key。sakura_rc.rcでRCDATAとして
	実行ファイルへ埋め込まれているため、実行時にkeybind_presetsフォルダそのものは
	不要(インストール環境を問わず動作させるため)。表示名の並びはkeybind_presets/README.md
	の対応表に合わせている。
	@date 20260813 先頭の「サクラエディタ」はnResId=0にして、CShareData::
		ResetKeyBindToDefault()による初期化のみで表現するように変更(初期化ボタンを
		廃止しプリセットコンボに一本化したため)。ビルド時点の既定値を常にそのまま使う
		形になり、Default.key(既定のスナップショットをエクスポートしたもの)を
		インポートする方式のような、ファイルが実際の既定値と食い違う心配が無くなる。
*/
static const SKeybindPresetInfo	s_KeybindPresetTable[] = {
	{ 0,                                L"サクラエディタ 2.3.2.0" },
	{ IDR_KEYBINDPRESET_VSCODE,        L"Visual Studio Code" },
	{ IDR_KEYBINDPRESET_VISUALSTUDIO,  L"Visual Studio" },
	{ IDR_KEYBINDPRESET_VISUALSTUDIO6, L"Visual Studio 6 / Visual C++ 6" },
	{ IDR_KEYBINDPRESET_VISUALBASIC6,  L"Visual Basic 6" },
	{ IDR_KEYBINDPRESET_CSHARP2005,    L"Visual C# 2005(Visual Studio 2005)" },
	{ IDR_KEYBINDPRESET_RESHARPER,     L"ReSharper(Visual Studio拡張)" },
};

//! プリセット選択コンボの先頭項目(index 0)の表示文字列。現在のキー割り当てが
//! s_KeybindPresetTableのどれとも一致しないときにこれを選択状態にする 20260813
static const wchar_t	szKeybindPresetCustomLabel[] = L"カスタマイズ";

/*! 指定したプリセットのRCDATA(元はkeybind_presets/*.key)を一時ファイル経由で
	targetCommonへインポートする
	@date 20260813 ApplyPreset()から切り出し。プリセットの一致判定(シミュレーション)にも
		使うため、対象をm_Commonに固定せず引数で受け取るようにした

	CImpExpKeybind::Import()はファイルパスしか受け付けない(内部でCDataProfile::ReadProfileが
	ファイルを開くため)ので、リソースの内容を一時ファイルへそのままのバイト列で書き出してから
	インポートし、インポート後に削除する。
*/
static bool ImportPresetKeyBindInto( HINSTANCE hInst, CommonSetting& targetCommon, int nPresetIndex, wstring& sErrMsg )
{
	if( nPresetIndex < 0 || _countof(s_KeybindPresetTable) <= (size_t)nPresetIndex ){
		return false;
	}
	const SKeybindPresetInfo&	info = s_KeybindPresetTable[nPresetIndex];

	HRSRC	hResInfo = ::FindResource( hInst, MAKEINTRESOURCE(info.nResId), RT_RCDATA );
	if( NULL == hResInfo ){
		return false;	// プリセットのリソースが見つからない(ビルド不整合)
	}
	HGLOBAL	hResData = ::LoadResource( hInst, hResInfo );
	const void*	pResData = ( NULL != hResData ) ? ::LockResource( hResData ) : NULL;
	DWORD	dwResSize = ::SizeofResource( hInst, hResInfo );
	if( NULL == pResData || 0 == dwResSize ){
		return false;
	}

	TCHAR	szTempDir[_MAX_PATH];
	TCHAR	szTempFile[_MAX_PATH];
	if( 0 == ::GetTempPath( _MAX_PATH, szTempDir )
	 || 0 == ::GetTempFileName( szTempDir, _T("skb"), 0, szTempFile )
	){
		return false;
	}

	HANDLE	hFile = ::CreateFile( szTempFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL );
	bool	bWriteOk = false;
	if( INVALID_HANDLE_VALUE != hFile ){
		DWORD	dwWritten = 0;
		bWriteOk = ( 0 != ::WriteFile( hFile, pResData, dwResSize, &dwWritten, NULL ) ) && ( dwWritten == dwResSize );
		::CloseHandle( hFile );
	}

	bool	bImportOk = false;
	if( bWriteOk ){
		CImpExpKeybind	cImpExpKeybind( targetCommon );
		bImportOk = cImpExpKeybind.Import( to_wchar( szTempFile ), sErrMsg );
	}

	::DeleteFile( szTempFile );

	return bImportOk;
}

/*! キー割り当て2つが機能的に等しいか調べる 20260813

	有効データ数(m_nKeyNameArrNum)と、そのぶんのm_pKeyNameArrの内容だけを比較する。
	m_VKeyToKeyNameArrはm_pKeyNameArrから機械的に導出される逆引き用のキャッシュに過ぎず、
	比較対象に含めなくても機能的な一致判定としては十分なため対象外にしている。
*/
static bool IsSameKeyBind( const CommonSetting_KeyBind& a, const CommonSetting_KeyBind& b )
{
	if( a.m_nKeyNameArrNum != b.m_nKeyNameArrNum ){
		return false;
	}
	return 0 == memcmp( a.m_pKeyNameArr, b.m_pKeyNameArr, sizeof(KEYDATA) * a.m_nKeyNameArrNum );
}
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO

//! 一覧の各行(nRow)が表す機能コード。ヘッダ行はF_DISABLEを積む。
//! 「登録」ボタンで選択中の行に何を割り当てるか調べるために使う 20260803
static std::vector<EFunctionCode>	s_vRowFuncCodes;

//! キーが既に何かに割り当て済みのときの警告色(セマンティックカラー:警告黄。
//! クリーム背景+琥珀色文字の配色) 20260803
//! @date 20260805 警告バナーの見本に合わせた配色に変更
//! @date 20260805 背景が白地とほぼ差がなく、枠のないリスト項目の塗りでは
//! 行の端まで塗られているのに中央付近しか色が見えず「高さが縮んだ」ように
//! 見えてしまっていたため、判別しやすい濃さに調整
static const COLORREF	NKMM_KEYBINDLIST_WARN_COLOR      = RGB( 255, 244, 196 );
static const COLORREF	NKMM_KEYBINDLIST_WARN_TEXT_COLOR = RGB( 180, 120, 10 );

//! 一覧で選択中の行の背景色・文字色(セマンティックカラー:エラー赤。
//! エラーバナー等で使われる薄紅背景+濃い赤文字の配色) 20260803
static const COLORREF	NKMM_KEYBINDLIST_SELECTED_COLOR      = RGB( 253, 231, 233 );
static const COLORREF	NKMM_KEYBINDLIST_SELECTED_TEXT_COLOR = RGB( 164, 38, 44 );

//! 種別区切り行・種別固定表示オーバーレイの背景色。以前はCOLOR_BTNFACEから
//! 計算していたが、環境によって値が変わり得り、区切り行とオーバーレイの
//! 色が食い違って見える原因になっていたため、固定値にして必ず一致させる 20260805
static const COLORREF	NKMM_KEYBINDLIST_HEADER_COLOR = RGB( 170, 170, 170 );

//! 種別固定表示オーバーレイの背景ブラシ。WM_CTLCOLORSTATICで返した後も
//! システム側が使い続けるため、FillRect用の使い捨てブラシと違い即座には
//! 破棄できない。WM_DESTROYで破棄する 20260804
static HBRUSH	s_hbrStickyHeaderBack = NULL;

#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
//! 「キー入力」欄(直接入力)が、既に何かに割り当て済みのキーを示しているときの
//! 警告色背景ブラシ。s_hbrStickyHeaderBackと同じ理由でWM_DESTROYまで使い回す 20260814
static HBRUSH	s_hbrCaptureWarnBack = NULL;

//! 「キー入力」欄(直接入力)で、装飾キーを押している間のライブプレビュー表示中かどうか。
//! 実キーが押されて確定するとfalseに戻る 20260814
static bool	s_bCapturePreviewActive = false;
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW

//! Shift/Ctrl/Altチェックボックスのチェック状態。
//! WM_CTLCOLORBTNの背景色返却は標準のBS_AUTOCHECKBOXでは効かない
//! (ビジュアルスタイル無効化後も無視される。CPropComKeybind.cppの
//! CPropComKeybindWndProc内の古いコメント「うまくいってません」の通り、
//! このコードベースで既知の制限)ため、この3つはBS_OWNERDRAWにして自前で
//! チェック状態・背景色を管理する。BS_OWNERDRAWはBM_GETCHECK/SETCHECKに
//! 対応しないため、チェック状態はここに保持する 20260803
static bool	s_bShiftChecked = false;
static bool	s_bCtrlChecked  = false;
static bool	s_bAltChecked   = false;

//! 絞り込み欄のプレースホルダーの文字色(セマンティックカラーではなく、通常の
//! グレー系文字より薄いヒント用の色) 20260804
static const COLORREF	NKMM_KEYBINDLIST_PLACEHOLDER_COLOR = RGB( 190, 190, 190 );

//! 絞り込み欄(Edit)のサブクラスプロシージャ。EM_SETCUEBANNERは文字色を
//! 指定できない(システム既定の濃さで描画される)ため、未入力かつ非フォーカス時は
//! このプロシージャで自前にヒント文字を薄い色で描画する 20260804
static LRESULT CALLBACK FilterEditSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	if( WM_PAINT == uMsg ){
		TCHAR	szText[8];
		::GetWindowText( hwnd, szText, _countof(szText) );
		if( L'\0' == szText[0] && ::GetFocus() != hwnd ){
			PAINTSTRUCT	ps;
			HDC	hdc = ::BeginPaint( hwnd, &ps );
			RECT	rc;
			::GetClientRect( hwnd, &rc );
			::FillRect( hdc, &rc, (HBRUSH)( COLOR_WINDOW + 1 ) );
			::SetBkMode( hdc, TRANSPARENT );
			::SetTextColor( hdc, NKMM_KEYBINDLIST_PLACEHOLDER_COLOR );
			HFONT	hFont = (HFONT)::SendMessage( hwnd, WM_GETFONT, 0, 0 );
			HFONT	hOldFont = ( NULL != hFont ) ? (HFONT)::SelectObject( hdc, hFont ) : NULL;
			RECT	rcLabel = rc;
			rcLabel.left += 2;
			::DrawText( hdc, LS(STR_ERR_DLGKEYBINDLIST_FILTERHINT), -1, &rcLabel,
				DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
			if( NULL != hOldFont ){
				::SelectObject( hdc, hOldFont );
			}
			::EndPaint( hwnd, &ps );
			return 0;
		}
	}else if( WM_SETFOCUS == uMsg || WM_KILLFOCUS == uMsg ){
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		if( WM_KILLFOCUS == uMsg && s_bCapturePreviewActive ){
			// Alt+Tabなどでフォーカスが外れ、装飾キーのWM_KEYUPを取れないまま
			// プレビュー表示が残ってしまうのを防ぐ。Alt+Tab中はGetKeyStateがまだ
			// Alt押下中を返すことがあるため、ここは実際の押下状態を見ずに
			// 強制的に全て未チェック・コンボ未選択にする 20260814
			HWND	hwndParent = ::GetParent( hwnd );
			::SetWindowText( hwnd, L"" );
			s_bShiftChecked = s_bCtrlChecked = s_bAltChecked = false;
			::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_SHIFT ), NULL, TRUE );
			::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_CTRL ), NULL, TRUE );
			::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_ALT ), NULL, TRUE );
			Combo_SetCurSel( ::GetDlgItem( hwndParent, IDC_COMBO_KEYBINDLIST_KEY ), -1 );
			s_bCapturePreviewActive = false;
		}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		LRESULT	lr = ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
		::InvalidateRect( hwnd, NULL, TRUE );	// ヒント文字の表示/非表示切り替えを反映する
		return lr;
	}else if( WM_NCDESTROY == uMsg ){
		::RemoveWindowSubclass( hwnd, FilterEditSubclassProc, uIdSubclass );
	}
	return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
}

//! 「キー入力」欄(直接入力)でキーを検知したことを親ダイアログへ伝える私用メッセージ。
//! wParam=検知した仮想キーコード、lParam=検知した修飾キー(_SHIFT|_CTRL|_ALT) 20260810
static const UINT WM_NKMM_KEYBINDLIST_CAPTURE = WM_APP + 0x210;

#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
//! 指定した仮想キーコードがShift/Ctrl/Alt(左右どちらでも)かどうかを調べる 20260814
static bool IsModifierVKey( int nVirtKey )
{
	return VK_SHIFT == nVirtKey || VK_LSHIFT == nVirtKey || VK_RSHIFT == nVirtKey
	    || VK_CONTROL == nVirtKey || VK_LCONTROL == nVirtKey || VK_RCONTROL == nVirtKey
	    || VK_MENU == nVirtKey || VK_LMENU == nVirtKey || VK_RMENU == nVirtKey;
}

//! 上部の設定コントロール(Shift/Ctrl/Altチェックボックス + キーコンボ)を、
//! 「キー入力」欄(直接入力)でのライブプレビュー中の装飾キー状態に同期させる
//! @date 20260814
//!
//! s_bShiftChecked等を現在の実際の押下状態(GetKeyState)に合わせて更新し、
//! チェックボックスを再描画する。まだ実キーは押されていない(=どのキーへの
//! 割り当てかは未定)ため、キーコンボの選択は解除して「未選択」にする。
//! 全キーが離された状態でこれを呼べば、結果としてチェックボックスは全て
//! 未チェックに、コンボも未選択に戻る 20260814
static void SyncKeySetControlsToLiveModifierState( HWND hwnd )
{
	HWND	hwndParent = ::GetParent( hwnd );

	s_bShiftChecked = ( 0 != ( ::GetKeyState( VK_SHIFT )   & 0x8000 ) );
	s_bCtrlChecked  = ( 0 != ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) );
	s_bAltChecked   = ( 0 != ( ::GetKeyState( VK_MENU )    & 0x8000 ) );

	::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_SHIFT ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_CTRL ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndParent, IDC_CHECK_ALT ), NULL, TRUE );

	// まだ実キーが押されていない間は、直前まで選ばれていたキーをコンボに
	// 残さない(装飾キーだけの状態と食い違って見えるため) 20260814
	Combo_SetCurSel( ::GetDlgItem( hwndParent, IDC_COMBO_KEYBINDLIST_KEY ), -1 );
}

//! 現在押されている装飾キーだけを"Shift+Ctrl+Alt+"のように並べた文字列を欄へ表示する
//! (まだ実キーが押されておらず確定していないことが分かるよう、末尾は"+"で終わる)。
//! 上部のチェックボックス+キーコンボも同じ状態に同期させる 20260814
static void UpdateCapturePreviewText( HWND hwnd )
{
	std::wstring	sPreview;
	if( 0 != ( ::GetKeyState( VK_SHIFT )   & 0x8000 ) ){ sPreview += L"Shift+"; }
	if( 0 != ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) ){ sPreview += L"Ctrl+";  }
	if( 0 != ( ::GetKeyState( VK_MENU )    & 0x8000 ) ){ sPreview += L"Alt+";   }
	::SetWindowText( hwnd, sPreview.c_str() );

	SyncKeySetControlsToLiveModifierState( hwnd );
}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW

/*! 「キー入力」欄(直接入力)のサブクラスプロシージャ。
	既存のコンボ+Shift/Ctrl/Altチェックボックス方式とは別の、もう一つの入力方法として
	追加する(まずは2系統を並行して用意し、結果は共通の状態(コンボ+チェックボックス)へ
	反映する)。フォーカス中に押されたキーを検知し、仮想キーコードと修飾キーの状態を
	WM_NKMM_KEYBINDLIST_CAPTUREで親ダイアログへ通知するだけで、一覧への反映自体は
	親側(DispatchEvent)が行う 20260810

	@date 20260810 新規作成。WM_GETDLGCODEでDLGC_WANTALLKEYSを返し、Tab/Esc/矢印
		キーなどダイアログ側が先取りするキーもできるだけこの欄で検知できるようにして
		いるが、それでも一部のキー(例えばAltを伴う組み合わせなど)はシステムメニュー等に
		奪われて検知できないことがある既知の制限
	@date 20260814 装飾キーだけを押している間、何も表示が変わらず反応しているのか
		分かりにくいという指摘を受け、装飾キーの押下/解放(WM_KEYDOWN/WM_KEYUP)ごとに
		「Shift+Ctrl+」のような未確定プレビューを表示するようにした。実キーを押して
		確定する動作(WM_NKMM_KEYBINDLIST_CAPTUREを送る)は従来通り変更していない
	@date 20260814 実キーで確定した後でも、次に装飾キーを押した時点でその確定表示を
		リセットする(以前の内容を保持・復元することはしない)ように変更。以後のキー
		入力を常に優先し、確定済みの表示に固執しないようにするため
*/
static LRESULT CALLBACK CaptureEditSubclassProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	if( WM_KEYDOWN == uMsg || WM_SYSKEYDOWN == uMsg ){
		int	nVirtKey = (int)wParam;
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		if( IsModifierVKey( nVirtKey ) ){
			// 装飾キー単体の押下。以前に実キーで確定した表示が残っていても、
			// 新しい入力を始めた時点でリセットする(その後のキー入力を優先し、
			// 確定済みの内容を保持・復元することはしない) 20260814
			s_bCapturePreviewActive = true;
			UpdateCapturePreviewText( hwnd );
		}else{
			int	nModifier = 0;
			if( 0 != ( ::GetKeyState( VK_SHIFT )   & 0x8000 ) ){ nModifier |= _SHIFT; }
			if( 0 != ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) ){ nModifier |= _CTRL;  }
			if( 0 != ( ::GetKeyState( VK_MENU )    & 0x8000 ) ){ nModifier |= _ALT;   }
			// 実キーが押されて確定するので、プレビュー中の表示は親側のUpdateCaptureText()に
			// 委ねる(この後の処理で上書きされる) 20260814
			s_bCapturePreviewActive = false;
			::SendMessage( ::GetParent( hwnd ), WM_NKMM_KEYBINDLIST_CAPTURE, (WPARAM)nVirtKey, (LPARAM)nModifier );
		}
#else
		// 修飾キー単体の押下はキー本体ではないので無視し、次のキーを待つ
		if( VK_SHIFT != nVirtKey && VK_LSHIFT != nVirtKey && VK_RSHIFT != nVirtKey
		 && VK_CONTROL != nVirtKey && VK_LCONTROL != nVirtKey && VK_RCONTROL != nVirtKey
		 && VK_MENU != nVirtKey && VK_LMENU != nVirtKey && VK_RMENU != nVirtKey
		){
			int	nModifier = 0;
			if( 0 != ( ::GetKeyState( VK_SHIFT )   & 0x8000 ) ){ nModifier |= _SHIFT; }
			if( 0 != ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) ){ nModifier |= _CTRL;  }
			if( 0 != ( ::GetKeyState( VK_MENU )    & 0x8000 ) ){ nModifier |= _ALT;   }
			::SendMessage( ::GetParent( hwnd ), WM_NKMM_KEYBINDLIST_CAPTURE, (WPARAM)nVirtKey, (LPARAM)nModifier );
		}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		return 0;	// Editの既定処理(フォーカス移動・システムメニュー起動等)をさせない
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
	}else if( WM_KEYUP == uMsg || WM_SYSKEYUP == uMsg ){
		int	nVirtKey = (int)wParam;
		if( IsModifierVKey( nVirtKey ) && s_bCapturePreviewActive ){
			// このキー自身はGetKeyStateで既に「離した」状態を返すはずなので、
			// 他に押されたままの装飾キーが無ければ空欄に戻す(リセットしたままにする。
			// 確定済みの内容へは戻さない) 20260814
			bool	bAnyModifierDown =
				( 0 != ( ::GetKeyState( VK_SHIFT )   & 0x8000 ) )
			 || ( 0 != ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) )
			 || ( 0 != ( ::GetKeyState( VK_MENU )    & 0x8000 ) );
			if( bAnyModifierDown ){
				UpdateCapturePreviewText( hwnd );
			}else{
				::SetWindowText( hwnd, L"" );
				SyncKeySetControlsToLiveModifierState( hwnd );	// 全チェックボックス解除・コンボ未選択に戻す
				s_bCapturePreviewActive = false;
			}
		}
		return 0;
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
	}else if( WM_CHAR == uMsg || WM_SYSCHAR == uMsg ){
		return 0;	// ES_READONLYでも文字入力扱いでビープが鳴るため、確実に止める
	}else if( WM_GETDLGCODE == uMsg ){
		return DLGC_WANTALLKEYS;	// Tab/Esc/矢印キーもこの欄で(できるだけ)検知する
	}else if( WM_PAINT == uMsg ){
		// 未入力かつ非フォーカス時は、絞り込み欄(FilterEditSubclassProc)と同じ要領で
		// ヒント文字を薄い色で自前描画する 20260810
		TCHAR	szText[8];
		::GetWindowText( hwnd, szText, _countof(szText) );
		if( L'\0' == szText[0] && ::GetFocus() != hwnd ){
			PAINTSTRUCT	ps;
			HDC	hdc = ::BeginPaint( hwnd, &ps );
			RECT	rc;
			::GetClientRect( hwnd, &rc );
			::FillRect( hdc, &rc, (HBRUSH)( COLOR_WINDOW + 1 ) );
			::SetBkMode( hdc, TRANSPARENT );
			::SetTextColor( hdc, NKMM_KEYBINDLIST_PLACEHOLDER_COLOR );
			HFONT	hFont = (HFONT)::SendMessage( hwnd, WM_GETFONT, 0, 0 );
			HFONT	hOldFont = ( NULL != hFont ) ? (HFONT)::SelectObject( hdc, hFont ) : NULL;
			RECT	rcLabel = rc;
			rcLabel.left += 2;
			::DrawText( hdc, LS(STR_ERR_DLGKEYBINDLIST_CAPTUREHINT), -1, &rcLabel,
				DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
			if( NULL != hOldFont ){
				::SelectObject( hdc, hOldFont );
			}
			::EndPaint( hwnd, &ps );
			return 0;
		}
	}else if( WM_SETFOCUS == uMsg || WM_KILLFOCUS == uMsg ){
		if( WM_KILLFOCUS == uMsg && s_bCapturePreviewActive ){
			// Alt+Tabなどでフォーカスが外れ、装飾キーのWM_KEYUPを取れないまま
			// プレビュー表示が残ってしまうのを防ぐ 20260814
			::SetWindowText( hwnd, L"" );
			s_bCapturePreviewActive = false;
		}
		LRESULT	lr = ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
		::InvalidateRect( hwnd, NULL, TRUE );	// ヒント文字の表示/非表示切り替えを反映する
		return lr;
	}else if( WM_NCDESTROY == uMsg ){
		::RemoveWindowSubclass( hwnd, CaptureEditSubclassProc, uIdSubclass );
	}
	return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
}

INT_PTR CALLBACK CPropKeybindList::DlgProc_page(
	HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DlgProc( reinterpret_cast<pDispatchPage>(&CPropKeybindList::DispatchEvent), hwndDlg, uMsg, wParam, lParam );
}

/* ショートカット一覧 メッセージ処理 */
INT_PTR CPropKeybindList::DispatchEvent(
	HWND	hwndDlg,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam
)
{
	NMHDR*	pNMHDR;

	switch( uMsg ){
	case WM_INITDIALOG:
		{
			HWND		hListView;
			HWND		hCombo;
			LV_COLUMN	col;
			std::wstring	sHeader( LS(STR_ERR_DLGKEYBIND1) );	// "キー\t機能名\t関数名\t機能番号\t..."(先頭2列を列見出しとして使い回す)
			size_t		nTab1 = sHeader.find( L'\t' );
			size_t		nTab2 = ( nTab1 == std::wstring::npos ) ? std::wstring::npos : sHeader.find( L'\t', nTab1 + 1 );
			std::wstring	sKeyColumn  = ( nTab1 == std::wstring::npos ) ? sHeader : sHeader.substr( 0, nTab1 );
			std::wstring	sFuncColumn = ( nTab1 == std::wstring::npos ) ? std::wstring() :
				( nTab2 == std::wstring::npos ) ? sHeader.substr( nTab1 + 1 ) : sHeader.substr( nTab1 + 1, nTab2 - nTab1 - 1 );

			// Modified by KEITA for WIN64 2003.9.6
			::SetWindowLongPtr( hwndDlg, DWLP_USER, lParam );

			// 上部の設定コントロール(Shift/Ctrl/Alt + キー)。Winキーは対象外
			// (キー割り当てタブ自体がShift/Ctrl/Altの3つしか扱っていないため) 20260803
			hCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );
			for( int i = 0; i < m_Common.m_sKeyBind.m_nKeyNameArrNum; ++i ){
				Combo_AddString( hCombo, m_Common.m_sKeyBind.m_pKeyNameArr[i].m_szKeyName );
			}
			// オーナードローの項目高さはCB_SETITEMHEIGHTで直接指定する。WM_MEASUREITEMは
			// ダイアログテンプレートの子コントロール生成時(WM_INITDIALOGより前、まだ
			// DWLP_USERが設定されておらずCPropCommon::DlgProcのdefault:分岐がインスタンスを
			// 取得できずFALSEを返してしまう時点)に飛んでくる可能性があり信頼できないため 20260803
			::SendMessage( hCombo, CB_SETITEMHEIGHT, (WPARAM)-1, (LPARAM)14 );	// 閉じているときの表示部分
			// ドロップダウンリストの項目。14だと警告色の塗りが窮屈に見えたため、
			// ファイル一覧のアイコン行くらいの余裕を目安にさらに高くする 20260805
			::SendMessage( hCombo, CB_SETITEMHEIGHT, (WPARAM)0, (LPARAM)22 );
			// チェックボックスは開くたびに未チェック状態から始める
			// (BS_OWNERDRAWのためチェック状態はこちらで保持している) 20260803
			s_bShiftChecked = false;
			s_bCtrlChecked  = false;
			s_bAltChecked   = false;

#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
			// プリセット選択コンボ。index 0は「カスタマイズ」(現在のキー割り当てが
			// どのプリセットとも一致しないときの表示用。選択操作としての意味は持たない)、
			// 以降にs_KeybindPresetTableの各プリセット名を並べる。実際に現在の状態に
			// 合わせた選択は、この後のSetData()経由のUpdatePresetCombo()で行う 20260813
			{
				HWND	hComboPreset = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_PRESET );
				Combo_AddString( hComboPreset, szKeybindPresetCustomLabel );
				for( size_t i = 0; i < _countof(s_KeybindPresetTable); ++i ){
					Combo_AddString( hComboPreset, s_KeybindPresetTable[i].pszName );
				}

				// 本体(閉じた状態)は初期化ボタンと同じ幅のまま、ドロップダウンだけ
				// 一番長い項目に合わせて広げる。CB_SETDROPPEDWIDTHはダイアログの
				// 右端をはみ出しても構わない(ポップアップとして別ウィンドウで
				// 表示されるため)ので、本体の幅に制約されず見切れを防げる 20260812
				{
					HDC	hdc = ::GetDC( hComboPreset );
					HFONT	hFont = (HFONT)::SendMessage( hComboPreset, WM_GETFONT, 0, 0 );
					HFONT	hOldFont = ( NULL != hFont ) ? (HFONT)::SelectObject( hdc, hFont ) : NULL;

					int	nMaxWidth = 0;
					SIZE	sz;
					::GetTextExtentPoint32( hdc, szKeybindPresetCustomLabel, (int)wcslen( szKeybindPresetCustomLabel ), &sz );
					nMaxWidth = sz.cx;
					for( size_t i = 0; i < _countof(s_KeybindPresetTable); ++i ){
						::GetTextExtentPoint32( hdc, s_KeybindPresetTable[i].pszName,
							(int)wcslen( s_KeybindPresetTable[i].pszName ), &sz );
						if( nMaxWidth < sz.cx ){ nMaxWidth = sz.cx; }
					}

					if( NULL != hOldFont ){
						::SelectObject( hdc, hOldFont );
					}
					::ReleaseDC( hComboPreset, hdc );

					// 左右の文字余白 + 垂直スクロールバー分の余裕を足す 20260812
					::SendMessage( hComboPreset, CB_SETDROPPEDWIDTH,
						(WPARAM)( nMaxWidth + 24 + ::GetSystemMetrics( SM_CXVSCROLL ) ), 0 );
				}
			}
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO

			hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
			// LVS_EX_GRIDLINESで行・列を罫線で区切って見やすくする。
			// LVS_EX_DOUBLEBUFFERは画面全体をオフスクリーンで合成してから
			// 一括で転送するため、スクロールで選択行(自前描画)が一瞬古い状態
			// (未着色など)で見えてから追いつくちらつき・ズレを抑える 20260804
			ListView_SetExtendedListViewStyleEx( hListView,
				LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER,
				LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER );
			// ビジュアルスタイル有効時、ListViewはNM_CUSTOMDRAWで自前に塗った背景色を無視して
			// テーマの背景を上描きしてしまうため、このリストはテーマを無効化する
			// (カテゴリ区切り行の背景色を効かせるため。CPropComKeyword.cppと同じ対処) 20260803
			PreventVisualStyle( hListView );
			// リスト本体だけ無効化すると、内蔵の列見出し(SysHeader32)だけテーマ有効の
			// ままになり、行の高さと見出しのテーマ描画(パディング等)がちぐはぐになって
			// 「機能名」ヘッダーの文字下端が欠けて見える不具合があったため、
			// 見出し側にも同じ処理を行い表示を揃える 20260804
			{
				HWND	hHeader = ListView_GetHeader( hListView );
				PreventVisualStyle( hHeader );

				// HDS_FLAT(列見出しの立体的なボタン風の縁取りを消す)も試したが、
				// 文字位置が2pxほど左にずれて見えたため取りやめた 20260804

				// テーマ無効化で見出しのフォント計量が変わり、高さが文字に対して
				// 逆に不足することがあるため、HDM_LAYOUTで正しい高さを取り直して
				// 明示的に反映する 20260804
				RECT	rcHeaderArea;
				::GetClientRect( hListView, &rcHeaderArea );
				WINDOWPOS	wp;
				HDLAYOUT	hdl;
				hdl.prc   = &rcHeaderArea;
				hdl.pwpos = &wp;
				if( Header_Layout( hHeader, &hdl ) ){
					::SetWindowPos( hHeader, NULL, wp.x, wp.y, wp.cx, wp.cy, SWP_NOZORDER );
				}
			}

			// 種別(カテゴリ)見出し行を[]で囲む代わりに太字で強調するためのフォントを
			// リストの現在のフォントから作る(WM_DESTROYで破棄する) 20260803
			{
				HFONT	hListFont = (HFONT)::SendMessage( hListView, WM_GETFONT, 0, 0 );
				LOGFONT	lf;
				::ZeroMemory( &lf, sizeof_raw( lf ) );
				if( NULL != hListFont && ::GetObject( hListFont, sizeof_raw( lf ), &lf ) ){
					lf.lfWeight = FW_BOLD;
					m_hKeybindListBoldFont = ::CreateFontIndirect( &lf );
				}
			}

#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
			// 「キー入力」欄(直接入力)は、ダイアログの既定フォント(9pt)のままだと
			// 高さに対して文字が小さく、下側に余白(灰色部分)が目立って見えるため、
			// 少し大きいフォント(11pt相当)を専用に作って設定する(WM_DESTROYで破棄する) 20260814
			{
				HWND	hCapture = ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_CAPTURE );
				HFONT	hCaptureBaseFont = (HFONT)::SendMessage( hCapture, WM_GETFONT, 0, 0 );
				LOGFONT	lfCapture;
				::ZeroMemory( &lfCapture, sizeof_raw( lfCapture ) );
				if( NULL != hCaptureBaseFont && ::GetObject( hCaptureBaseFont, sizeof_raw( lfCapture ), &lfCapture ) ){
					lfCapture.lfHeight = ::MulDiv( lfCapture.lfHeight, 11, 9 );	// 9pt -> 11pt相当
					m_hKeybindListCaptureFont = ::CreateFontIndirect( &lfCapture );
					if( NULL != m_hKeybindListCaptureFont ){
						::SendMessage( hCapture, WM_SETFONT, (WPARAM)m_hKeybindListCaptureFont, TRUE );
					}
				}
			}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW

			RECT	rc;
			::GetClientRect( hListView, &rc );
			// この時点ではまだ項目が入っておらず垂直スクロールバーが出ていないため、
			// クライアント幅をそのまま使うと後で項目を入れて縦スクロールバーが出た
			// 瞬間に列の合計幅がはみ出し、初期表示から横スクロールバーが出てしまう。
			// あらかじめ縦スクロールバー分の幅を差し引いておく 20260803
			int	nAvailWidth = ( rc.right - rc.left ) - ::GetSystemMetrics( SM_CXVSCROLL );

			::ZeroMemory( &col, sizeof_raw( col ) );
			col.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			col.fmt      = LVCFMT_LEFT;
			col.cx       = nAvailWidth * 50 / 100;	// 機能名
			col.pszText  = const_cast<TCHAR*>( sFuncColumn.c_str() );
			col.iSubItem = 0;
			ListView_InsertColumn( hListView, 0, &col );

			col.cx       = nAvailWidth * 46 / 100;	// キー(ショートカット。カスタムメニュー等は複数キーをまとめて表示するため長くなりがち) 20260803
			col.pszText  = const_cast<TCHAR*>( sKeyColumn.c_str() );
			col.iSubItem = 1;
			ListView_InsertColumn( hListView, 1, &col );

			// 絞り込み欄に検索アイコン付きテキストボックス風のヒント文字列を表示する
			// (実際のアイコン描画までは行わず、カーソルが無い間だけ薄く出るヒント文字で代用)。
			// EM_SETCUEBANNERは文字色を指定できずシステム既定の濃さになってしまうため、
			// サブクラス化して自前で薄い色のヒント文字を描画する 20260804
			::SetWindowSubclass( ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_FILTER ),
				FilterEditSubclassProc, 0, 0 );

			// 「キー入力」欄(直接入力)。既存のコンボ+チェックボックスとは別の、
			// もう一つの入力方法として並行して使えるようにする 20260810
			::SetWindowSubclass( ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_CAPTURE ),
				CaptureEditSubclassProc, 0, 0 );

			/* ダイアログデータの設定 */
			SetData( hwndDlg );

			// スクロールしても常に「現在の種別」が見えるよう、リスト先頭行に重ねて
			// 種別名を固定表示するオーバーレイを作る(区切り行がスクロールで見えなく
			// なっても、今どの種別を見ているか分かるようにするため)。ネイティブの
			// グループ表示(LVM_ENABLEGROUPVIEW)へ作り直すのではなく、区切り行を
			// 通常行として挿入する今の方式のまま、専用コントロールを重ねる軽量な
			// 方法で実現する。背景色はWM_CTLCOLORSTATICで種別区切り行と揃える。
			// SS_CENTERIMAGEは単一行のSS_LEFT staticで文字を垂直中央に揃える
			// ための組み合わせ(画像用のスタイルだが単一行テキストにも効く) 20260804
			{
				RECT	rcListScreen;
				::GetWindowRect( hListView, &rcListScreen );
				POINT	topLeft = { rcListScreen.left, rcListScreen.top };
				::ScreenToClient( hwndDlg, &topLeft );

				RECT	rcRow0;
				int	nStickyHeight = 18;	// 行がまだ無い場合のフォールバック
				if( 0 < ListView_GetItemCount( hListView )
				 && ListView_GetItemRect( hListView, 0, &rcRow0, LVIR_BOUNDS )
				){
					nStickyHeight = rcRow0.bottom - rcRow0.top;
				}

				RECT	rcListClient;
				::GetClientRect( hListView, &rcListClient );

				// STATICの既定の文字位置は種別区切り行(NM_CUSTOMDRAWでrcItem.left+4に
				// 描画している)より4pxほど左にずれる(実測)ため、コントロール自体を
				// 4px右にずらして位置を合わせる 20260804
				HWND	hSticky = ::CreateWindowEx( 0, L"STATIC", L"",
					WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
					topLeft.x + 4, topLeft.y, rcListClient.right - 4, nStickyHeight,
					hwndDlg, (HMENU)(INT_PTR)IDC_STATIC_KEYBINDLIST_STICKYHEADER,
					(HINSTANCE)::GetWindowLongPtr( hwndDlg, GWLP_HINSTANCE ), NULL );
				if( NULL != hSticky ){
					if( NULL != m_hKeybindListBoldFont ){
						::SendMessage( hSticky, WM_SETFONT, (WPARAM)m_hKeybindListBoldFont, TRUE );
					}
					// テーマ有効のままだと、WM_CTLCOLORSTATICで同じRGBを指定しても
					// テーマの描画(グラデーション等)がかかり、種別区切り行の単純な
					// FillRectと色味が微妙に食い違って見えるため、こちらもテーマを
					// 無効化して同じ単色描画に揃える 20260804
					PreventVisualStyle( hSticky );
				}
			}

			UpdateActionArea( hwndDlg );
			UpdateStickyHeader( hwndDlg );
		}
		return TRUE;

	case WM_DESTROY:
		// 種別見出し用の太字フォントを破棄する 20260803
		if( NULL != m_hKeybindListBoldFont ){
			::DeleteObject( m_hKeybindListBoldFont );
			m_hKeybindListBoldFont = NULL;
		}
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		// 「キー入力」欄の拡大フォントを破棄する 20260814
		if( NULL != m_hKeybindListCaptureFont ){
			::DeleteObject( m_hKeybindListCaptureFont );
			m_hKeybindListCaptureFont = NULL;
		}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		// 種別固定表示オーバーレイの背景ブラシを破棄する 20260804
		if( NULL != s_hbrStickyHeaderBack ){
			::DeleteObject( s_hbrStickyHeaderBack );
			s_hbrStickyHeaderBack = NULL;
		}
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		// 「キー入力」欄の警告色背景ブラシを破棄する 20260814
		if( NULL != s_hbrCaptureWarnBack ){
			::DeleteObject( s_hbrCaptureWarnBack );
			s_hbrCaptureWarnBack = NULL;
		}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		break;

	case WM_CTLCOLORSTATIC:
		// 種別固定表示オーバーレイの背景・文字色を、種別区切り行と揃える 20260804
		if( (HWND)lParam == ::GetDlgItem( hwndDlg, IDC_STATIC_KEYBINDLIST_STICKYHEADER ) ){
			HDC	hdcStatic = (HDC)wParam;
			::SetBkColor( hdcStatic, NKMM_KEYBINDLIST_HEADER_COLOR );
			::SetTextColor( hdcStatic, ::GetSysColor( COLOR_BTNTEXT ) );
			if( NULL == s_hbrStickyHeaderBack ){
				s_hbrStickyHeaderBack = ::CreateSolidBrush( NKMM_KEYBINDLIST_HEADER_COLOR );
			}
			::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR)s_hbrStickyHeaderBack );
			return TRUE;
		}
#ifdef NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		// 「キー入力」欄(直接入力)。ES_READONLYのEditはWM_CTLCOLOREDITではなく
		// WM_CTLCOLORSTATICが飛んでくる。既に何かに割り当て済みのキーを表示している
		// ときは、コンボやチェックボックスと同じ警告色で塗る 20260814
		// @date 20260814 フォーカスが無いとビジュアルスタイルの影響で背景が黄色ではなく
		// 灰色になってしまう(既知の制限。ListView等で使っているPreventVisualStyle()を
		// このEditにも使えば解消できる可能性はあるが未検証)。その状態で警告色の文字色
		// (琥珀)のままだと灰色地に読みづらいため、非フォーカス時は文字色だけ黒にする
		if( (HWND)lParam == ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_CAPTURE ) ){
			HDC	hdcStatic = (HDC)wParam;
			bool	bConflict = ( 0 <= FindMatchingRow( hwndDlg ) );
			bool	bFocused  = ( ::GetFocus() == (HWND)lParam );
			if( bConflict ){
				::SetBkColor( hdcStatic, NKMM_KEYBINDLIST_WARN_COLOR );
				::SetTextColor( hdcStatic, bFocused ? NKMM_KEYBINDLIST_WARN_TEXT_COLOR : ::GetSysColor( COLOR_WINDOWTEXT ) );
				if( NULL == s_hbrCaptureWarnBack ){
					s_hbrCaptureWarnBack = ::CreateSolidBrush( NKMM_KEYBINDLIST_WARN_COLOR );
				}
				::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR)s_hbrCaptureWarnBack );
			}else{
				::SetBkColor( hdcStatic, ::GetSysColor( COLOR_WINDOW ) );
				::SetTextColor( hdcStatic, ::GetSysColor( COLOR_WINDOWTEXT ) );
				::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR)(HBRUSH)( COLOR_WINDOW + 1 ) );
			}
			return TRUE;
		}
		// 「機能名」欄(選択中の機能名の表示専用、ES_READONLY)。既定のまま(グレーがかった
		// 「無効」風の見た目)だと、上の「キー入力」欄(白背景にした)とだけ見た目が違って
		// 目立ってしまうため、こちらも同じ白背景にして揃える 20260814
		if( (HWND)lParam == ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_FUNCNAME ) ){
			HDC	hdcStatic = (HDC)wParam;
			::SetBkColor( hdcStatic, ::GetSysColor( COLOR_WINDOW ) );
			::SetTextColor( hdcStatic, ::GetSysColor( COLOR_WINDOWTEXT ) );
			::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, (LONG_PTR)(HBRUSH)( COLOR_WINDOW + 1 ) );
			return TRUE;
		}
#endif // NKMM_FIX_KEYBIND_CAPTURE_LIVEPREVIEW
		break;

	case WM_DRAWITEM:
		// キーのコンボボックス(オーナードロー)を描画する。既に何かに割り当て済みの
		// キーを指定しているときは警告色で塗る。CBS_DROPDOWNLISTは子のEditを
		// 持たないためWM_CTLCOLOREDIT等では背景色を変えられず、オーナードローが必要 20260803
		{
			LPDRAWITEMSTRUCT	pdis = (LPDRAWITEMSTRUCT)lParam;
			if( IDC_COMBO_KEYBINDLIST_KEY == pdis->CtlID ){
				bool	bEditArea = ( 0 != ( pdis->itemState & ODS_COMBOBOXEDIT ) );
				bool	bConflict;
				if( bEditArea ){
					bConflict = ( 0 <= FindMatchingRow( hwndDlg ) );
				}else{
					// ドロップダウンの各項目は、その項目自体のキー+現在チェック中の
					// 修飾キーの組み合わせで割り当て済みか個別に判定する 20260805
					int	nModifier = 0;
					if( s_bShiftChecked ){ nModifier |= _SHIFT; }
					if( s_bCtrlChecked )  { nModifier |= _CTRL;  }
					if( s_bAltChecked )   { nModifier |= _ALT;   }
					bConflict = ( (int)pdis->itemID >= 0 ) && IsKeyAssigned( (int)pdis->itemID, nModifier );
				}

				COLORREF	crBack;
				COLORREF	crText;
				if( bConflict ){
					crBack = NKMM_KEYBINDLIST_WARN_COLOR;
					crText = NKMM_KEYBINDLIST_WARN_TEXT_COLOR;
				}else if( 0 != ( pdis->itemState & ODS_SELECTED ) ){
					crBack = ::GetSysColor( COLOR_HIGHLIGHT );
					crText = ::GetSysColor( COLOR_HIGHLIGHTTEXT );
				}else{
					crBack = ::GetSysColor( COLOR_WINDOW );
					crText = ::GetSysColor( COLOR_WINDOWTEXT );
				}

				HBRUSH	hbrBack = ::CreateSolidBrush( crBack );
				::FillRect( pdis->hDC, &pdis->rcItem, hbrBack );
				::DeleteObject( hbrBack );

				if( (int)pdis->itemID >= 0 ){
					TCHAR	szText[64];
					::SendMessage( pdis->hwndItem, CB_GETLBTEXT, pdis->itemID, (LPARAM)szText );
					::SetBkMode( pdis->hDC, TRANSPARENT );
					::SetTextColor( pdis->hDC, crText );
					RECT	rcLabel = pdis->rcItem;
					rcLabel.left += 2;
					::DrawText( pdis->hDC, szText, -1, &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
				}
				if( 0 != ( pdis->itemState & ODS_FOCUS ) ){
					::DrawFocusRect( pdis->hDC, &pdis->rcItem );
				}
				return TRUE;
			}
			if( IDC_CHECK_SHIFT == pdis->CtlID || IDC_CHECK_CTRL == pdis->CtlID || IDC_CHECK_ALT == pdis->CtlID ){
				// Shift/Ctrl/Altチェックボックス(オーナードロー)を描画する。標準の
				// BS_AUTOCHECKBOXはWM_CTLCOLORBTNの返却ブラシを背景に反映しないため
				// (このコードベースのCPropComKeybind.cppに残る「うまくいってません」
				// という古いコメントの通り既知の制限)、オーナードローにして自前で
				// チェック状態・背景色(警告色)を管理する 20260803
				bool	bChecked;
				switch( pdis->CtlID ){
				case IDC_CHECK_SHIFT: bChecked = s_bShiftChecked; break;
				case IDC_CHECK_CTRL:  bChecked = s_bCtrlChecked;  break;
				default:               bChecked = s_bAltChecked;  break;
				}
				// チェックされていないチェックボックスは、他が競合していても警告色にしない 20260803
				bool	bConflict = bChecked && ( 0 <= FindMatchingRow( hwndDlg ) );

				COLORREF	crBack = bConflict ? NKMM_KEYBINDLIST_WARN_COLOR : ::GetSysColor( COLOR_3DFACE );
				HBRUSH	hbrBack = ::CreateSolidBrush( crBack );
				::FillRect( pdis->hDC, &pdis->rcItem, hbrBack );
				::DeleteObject( hbrBack );

				RECT	rcCheck = pdis->rcItem;
				rcCheck.right = rcCheck.left + 12;
				rcCheck.top  += ( ( rcCheck.bottom - rcCheck.top ) - 12 ) / 2;
				rcCheck.bottom = rcCheck.top + 12;
				UINT	nState = DFCS_BUTTONCHECK;
				if( bChecked ){ nState |= DFCS_CHECKED; }
				if( 0 != ( pdis->itemState & ODS_SELECTED ) ){ nState |= DFCS_PUSHED; }
				if( !::IsWindowEnabled( pdis->hwndItem ) ){ nState |= DFCS_INACTIVE; }
				::DrawFrameControl( pdis->hDC, &rcCheck, DFC_BUTTON, nState );

				TCHAR	szLabel[32];
				::GetWindowText( pdis->hwndItem, szLabel, _countof(szLabel) );
				::SetBkMode( pdis->hDC, TRANSPARENT );
				::SetTextColor( pdis->hDC, bConflict ? NKMM_KEYBINDLIST_WARN_TEXT_COLOR : ::GetSysColor( COLOR_WINDOWTEXT ) );
				RECT	rcLabel = pdis->rcItem;
				rcLabel.left = rcCheck.right + 4;
				::DrawText( pdis->hDC, szLabel, -1, &rcLabel, DT_SINGLELINE | DT_VCENTER );	// &の下線(アクセスキー)を活かすためDT_NOPREFIXは付けない

				if( 0 != ( pdis->itemState & ODS_FOCUS ) ){
					::DrawFocusRect( pdis->hDC, &pdis->rcItem );
				}
				return TRUE;
			}
		}
		break;

	case WM_COMMAND:
		// 上部の設定コントロール(Shift/Ctrl/Alt/キー)を操作したら、一致する行があれば
		// 見える位置までスクロールしてフォーカスする 20260803
		switch( LOWORD(wParam) ){
		case IDC_CHECK_SHIFT:
		case IDC_CHECK_CTRL:
		case IDC_CHECK_ALT:
			// BS_OWNERDRAWは自動でチェック状態が変わらないため、ここで手動反転させる 20260803
			if( BN_CLICKED == HIWORD(wParam) ){
				switch( LOWORD(wParam) ){
				case IDC_CHECK_SHIFT: s_bShiftChecked = !s_bShiftChecked; break;
				case IDC_CHECK_CTRL:  s_bCtrlChecked  = !s_bCtrlChecked;  break;
				case IDC_CHECK_ALT:   s_bAltChecked   = !s_bAltChecked;   break;
				}
				::InvalidateRect( (HWND)lParam, NULL, TRUE );
				FocusMatchingRow( hwndDlg );
				return TRUE;
			}
			break;
		case IDC_COMBO_KEYBINDLIST_KEY:
			if( CBN_SELCHANGE == HIWORD(wParam) ){
				FocusMatchingRow( hwndDlg );
				return TRUE;
			}
			if( CBN_CLOSEUP == HIWORD(wParam) ){
				// ドロップダウンリストが下のリストビューに重なって表示されるため、
				// 閉じたときにその領域の再描画が行われずゴミ(残像)が残ることがある。
				// ダイアログ全体を明示的に再描画させて解消する 20260804
				::InvalidateRect( hwndDlg, NULL, TRUE );
				::UpdateWindow( hwndDlg );
				return TRUE;
			}
			break;
#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
		case IDC_COMBO_KEYBINDLIST_PRESET:
			// プリセットを選んだら適用する。index 0の「カスタマイズ」は現在の状態を
			// 表す表示専用の項目で、選んでも何もしない(選び直せば元々の選択に戻る) 20260813
			if( CBN_SELCHANGE == HIWORD(wParam) ){
				HWND	hComboPreset = (HWND)lParam;
				int	nSel = Combo_GetCurSel( hComboPreset );
				if( 0 < nSel ){
					ApplyPreset( hwndDlg, nSel - 1 );	// ApplyPreset内でコンボの選択も更新する
				}
				return TRUE;
			}
			break;
#elif defined(NKMM_FIX_KEYBIND_TOOLBAR_RESET)
		case IDC_BUTTON_INITIALIZE:	/* 初期化 */
			if( BN_CLICKED == HIWORD(wParam) ){
				InitializeToDefault( hwndDlg );
				return TRUE;
			}
			break;
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO
		case IDC_EDIT_KEYBINDLIST_FILTER:
			// 機能名/キーの一部一致で一覧を絞り込む。入力のたびに一覧を作り直す 20260803
			if( EN_CHANGE == HIWORD(wParam) ){
				UpdateList( hwndDlg );
				UpdateActionArea( hwndDlg );
				return TRUE;
			}
			break;
		case IDC_BUTTON_IMPORT:	/* インポート */
			// キー割り当てタブと同じCImpExpKeybindを使う。インポート結果は
			// m_Common.m_sKeyBind経由でキー割り当てタブと共有されているため、
			// このタブ側の一覧を作り直すだけでよい 20260804
			if( BN_CLICKED == HIWORD(wParam) ){
				Import( hwndDlg );
				return TRUE;
			}
			break;
		case IDC_BUTTON_EXPORT:	/* エクスポート */
			if( BN_CLICKED == HIWORD(wParam) ){
				Export( hwndDlg );
				return TRUE;
			}
			break;
		case IDC_BUTTON_KEYBINDLIST_REGISTER:
			// 追加: キーセット→リスト。現在指定中のキーセットを選択中の機能に追加する。
			// 貼り付け(F9, Shift+Ins, Ctrl+V)のように1つの機能に複数キーを持たせたい
			// ケースがあるため、選択中の機能が既に持っている他のキーには触れない。
			// 割り当て済みの他の機能を上書きする場合は、キー割り当てタブの「割付」
			// ボタンと同様に確認なしで上書きする(警告色で事前に知らせているため) 20260803
			if( BN_CLICKED == HIWORD(wParam) ){
				HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
				HWND	hCombo    = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );

				int	nSelected = ListView_GetNextItem( hListView, -1, LVNI_SELECTED );
				if( nSelected < 0 || (size_t)nSelected >= s_vRowFuncCodes.size()
				 || F_DISABLE == s_vRowFuncCodes[nSelected]
				){
					return TRUE;	// 機能(区切り行ではない行)が選ばれていなければ何もしない
				}

				int	nKeyIndex = Combo_GetCurSel( hCombo );
				if( CB_ERR == nKeyIndex ){
					return TRUE;	// キーが選ばれていなければ何もしない
				}
				int	nModifier = 0;
				if( s_bShiftChecked ){ nModifier |= _SHIFT; }
				if( s_bCtrlChecked )  { nModifier |= _CTRL;  }
				if( s_bAltChecked )   { nModifier |= _ALT;   }

				m_Common.m_sKeyBind.m_pKeyNameArr[nKeyIndex].m_nFuncCodeArr[nModifier] = s_vRowFuncCodes[nSelected];

				UpdateList( hwndDlg );

				// 対象の機能(nSelected)は一覧の並び順(カテゴリ・位置)を変えないので、
				// 同じ行番号を選び直すだけでよい 20260803
				ListView_SetItemState( hListView, -1, 0, LVIS_SELECTED );
				ListView_SetItemState( hListView, nSelected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
				ListView_EnsureVisible( hListView, nSelected, TRUE );

				UpdateActionArea( hwndDlg );
#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
				UpdatePresetCombo( hwndDlg );
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO
				return TRUE;
			}
			break;
		case IDC_BUTTON_KEYBINDLIST_RELEASE:
			// 解除: 追加と同じくキーセット側の指定内容が対象。上部でキーセットを
			// 指定して押すと、リストの選択状態とは無関係にそのキーの割り当てを
			// 解除する(キー割り当てタブの「解除」ボタンと同じ考え方) 20260803
			if( BN_CLICKED == HIWORD(wParam) ){
				HWND	hCombo    = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );
				HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );

				int	nKeyIndex = Combo_GetCurSel( hCombo );
				if( CB_ERR == nKeyIndex ){
					return TRUE;	// キーが選ばれていなければ何もしない
				}
				int	nModifier = 0;
				if( s_bShiftChecked ){ nModifier |= _SHIFT; }
				if( s_bCtrlChecked )  { nModifier |= _CTRL;  }
				if( s_bAltChecked )   { nModifier |= _ALT;   }

				m_Common.m_sKeyBind.m_pKeyNameArr[nKeyIndex].m_nFuncCodeArr[nModifier] = F_DEFAULT;

				// リストの選択行(カテゴリ・位置)は変わらないので、UpdateList()後に
				// 同じ行番号を選び直してスクロール位置を保つ 20260803
				int	nSelected = ListView_GetNextItem( hListView, -1, LVNI_SELECTED );

				UpdateList( hwndDlg );

				if( 0 <= nSelected ){
					ListView_SetItemState( hListView, -1, 0, LVIS_SELECTED );
					ListView_SetItemState( hListView, nSelected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
					ListView_EnsureVisible( hListView, nSelected, TRUE );
				}

				UpdateActionArea( hwndDlg );
#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
				UpdatePresetCombo( hwndDlg );
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO
				return TRUE;
			}
			break;
		}
		break;

	case WM_NKMM_KEYBINDLIST_CAPTURE:
		// 「キー入力」欄(直接入力)でキーを検知した通知。wParam=仮想キーコード、
		// lParam=検知した修飾キー(_SHIFT|_CTRL|_ALT)。一覧のキー名と一致すれば、
		// もう一方の入力方法(コンボ+チェックボックス)へ結果を反映して共有する 20260810
		{
			int	nVirtKey  = (int)wParam;
			int	nModifier = (int)lParam;

			int	nKeyIndex = -1;
			for( int i = 0; i < m_Common.m_sKeyBind.m_nKeyNameArrNum; ++i ){
				if( m_Common.m_sKeyBind.m_pKeyNameArr[i].m_nKeyCode == nVirtKey ){
					nKeyIndex = i;
					break;
				}
			}

			if( 0 <= nKeyIndex ){
				HWND	hCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );

				s_bShiftChecked = 0 != ( nModifier & _SHIFT );
				s_bCtrlChecked  = 0 != ( nModifier & _CTRL );
				s_bAltChecked   = 0 != ( nModifier & _ALT );
				Combo_SetCurSel( hCombo, nKeyIndex );

				::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_SHIFT ), NULL, TRUE );
				::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_CTRL ), NULL, TRUE );
				::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_ALT ), NULL, TRUE );

				// 直接入力欄自体にも、検知した組み合わせを表示してフィードバックする
				UpdateCaptureText( hwndDlg, nKeyIndex );

				FocusMatchingRow( hwndDlg );
			}
		}
		return TRUE;

	case WM_NOTIFY:
		pNMHDR = (NMHDR*)lParam;
		switch( pNMHDR->code ){
		case PSN_HELP:
			OnHelp( hwndDlg, IDD_PROP_KEYBIND_LIST );
			return TRUE;
		case PSN_KILLACTIVE:
			GetData( hwndDlg );
			return TRUE;
		case PSN_SETACTIVE:
			m_nPageNum = ID_PROPCOM_PAGENUM_KEYLIST;
			// 表示を更新する(キー割り当てタブでの未保存の変更を反映) 20260803
			SetData( hwndDlg );
			return TRUE;
		case LVN_ITEMCHANGED:
			// シングルクリックでの選択変更では、上部のキー設定(チェックボックス/コンボ)は
			// 変えない(見比べている最中に指定していたキーセットが変わってしまう問題が
			// あったため)。機能名欄・登録/解除ボタンの表示だけは常に選択行に合わせて
			// 更新する。キー設定側への同期はダブルクリック(NM_DBLCLK)で行う 20260803
			{
				NM_LISTVIEW*	pnmv = (NM_LISTVIEW*)lParam;
				if( ( pnmv->uChanged & LVIF_STATE )
				 && ( pnmv->uNewState & LVIS_SELECTED )
				 && !( pnmv->uOldState & LVIS_SELECTED )
				){
					UpdateActionArea( hwndDlg );
				}
				// マウス/キー操作による選択変更時、Windowsは項目本来の(列幅ぶんの)
				// 矩形しか再描画対象にせず、NM_CUSTOMDRAWでrcItem.right =
				// rcClient.rightとして拡張して塗っている右側(スクロールバー際まで)が
				// クリップされて古い状態のまま残ることがあった。一覧をまるごと作り直す
				// UpdateList()経由の場合は起きない(全体が再描画対象になるため)ので、
				// 選択状態が変わったときはリスト全体を明示的に無効化して、右端まで
				// 正しく塗り直させる 20260805
				if( pnmv->uChanged & LVIF_STATE ){
					::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL ), NULL, FALSE );
				}
			}
			return TRUE;
		case NM_DBLCLK:
			// ダブルクリックされた行のショートカットを上部の設定コントロールに同期する 20260803
			if( IDC_LIST_KEYBINDALL == pNMHDR->idFrom ){
				HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
				int	nSelected = ListView_GetNextItem( hListView, -1, LVNI_SELECTED );
				if( 0 <= nSelected ){
					ReflectSelection( hwndDlg, nSelected );
				}
			}
			return TRUE;
		case NM_CUSTOMDRAW:
			// カテゴリ区切り行([カテゴリ名]の行、lParam==-1で判別)の背景色を変えて表示する 20260803
			{
				LPNMLVCUSTOMDRAW	plvcd = (LPNMLVCUSTOMDRAW)lParam;
				switch( plvcd->nmcd.dwDrawStage ){
				case CDDS_PREPAINT:
					// リストが(スクロール後を含めて)再描画されるたびに呼ばれるので、
					// ここで固定表示オーバーレイの種別名も更新する。オーバーレイを
					// HWND_TOPに上げてから再描画させることで、リスト側のスクロール
					// 最適化(前回描画済みビットマップの単純スクロール転送)でオーバー
					// レイの下に隠れた古い内容が透けて見える問題を防ぐ 20260804
					UpdateStickyHeader( hwndDlg );
					::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW );
					return TRUE;
				case CDDS_ITEMPREPAINT:
					{
						HWND	hwndListDraw = plvcd->nmcd.hdr.hwndFrom;
						int	nItem = (int)plvcd->nmcd.dwItemSpec;

						LV_ITEM	lvi;
						::ZeroMemory( &lvi, sizeof_raw( lvi ) );
						lvi.mask  = LVIF_PARAM;
						lvi.iItem = nItem;
						ListView_GetItem( hwndListDraw, &lvi );

						if( -1 == lvi.lParam ){	// カテゴリ区切り行
							HDC	hdc = plvcd->nmcd.hdc;
							RECT	rcItem;
							RECT	rcClient;
							ListView_GetItemRect( hwndListDraw, nItem, &rcItem, LVIR_BOUNDS );
							::GetClientRect( hwndListDraw, &rcClient );
							rcItem.right = rcClient.right;	// 複数列ぶんまとめて全幅塗るため
							::InflateRect( &rcItem, 0, 1 );	// 行間に隙間が見えないように上下に広げる

							// 種別固定表示オーバーレイ(WM_CTLCOLORSTATIC)と必ず同じ色になるよう、
							// 固定値のNKMM_KEYBINDLIST_HEADER_COLORを使う 20260805
							HBRUSH	hbrBack = ::CreateSolidBrush( NKMM_KEYBINDLIST_HEADER_COLOR );
							::FillRect( hdc, &rcItem, hbrBack );
							::DeleteObject( hbrBack );

							::SetBkMode( hdc, TRANSPARENT );
							::SetTextColor( hdc, ::GetSysColor( COLOR_BTNTEXT ) );

							// []で囲む代わりに太字にして種別見出しであることを示す 20260803
							HFONT	hOldFont = NULL;
							if( NULL != m_hKeybindListBoldFont ){
								hOldFont = (HFONT)::SelectObject( hdc, m_hKeybindListBoldFont );
							}

							TCHAR	szText[256];
							ListView_GetItemText( hwndListDraw, nItem, 0, szText, _countof(szText) );
							RECT	rcLabel = rcItem;
							rcLabel.left += 4;	// ListView既定のラベル余白に合わせる
							::DrawText( hdc, szText, -1, &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );

							if( NULL != hOldFont ){
								::SelectObject( hdc, hOldFont );
							}

							::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT );
							return TRUE;
						}

						// 選択中の行はセマンティックカラー(選択赤)で塗る。
						// nmcd.uItemStateのCDIS_SELECTEDは実際の選択状態とずれることがある
						// (未選択行がCDIS_SELECTED付きで来たり、選択行に付かなかったりする)ため、
						// ListView_GetItemState()で実際の選択状態を取り直して判定する。
						// また、LVS_EX_FULLROWSELECTの選択行はclrTextBk/clrText+CDRF_NEWFONTを
						// 返すだけでは背景の塗りが既定のハイライト色のまま変わらない(clrTextBkは
						// 主に非選択行の色付けにしか使われない)ため、カテゴリ区切り行と同様に
						// 背景・文字を自前で描画してCDRF_SKIPDEFAULTを返す 20260803
						bool	bRowSelected = 0 != ( ListView_GetItemState( hwndListDraw, nItem, LVIS_SELECTED ) & LVIS_SELECTED );
						if( bRowSelected ){
							HDC	hdc = plvcd->nmcd.hdc;
							RECT	rcItem;
							RECT	rcClient;
							ListView_GetItemRect( hwndListDraw, nItem, &rcItem, LVIR_BOUNDS );
							::GetClientRect( hwndListDraw, &rcClient );
							rcItem.right = rcClient.right;	// 複数列ぶんまとめて全幅塗るため
							::InflateRect( &rcItem, 0, 1 );	// 行間に隙間が見えないように上下に広げる

							HBRUSH	hbrBack = ::CreateSolidBrush( NKMM_KEYBINDLIST_SELECTED_COLOR );
							::FillRect( hdc, &rcItem, hbrBack );
							::DeleteObject( hbrBack );

							::SetBkMode( hdc, TRANSPARENT );
							::SetTextColor( hdc, NKMM_KEYBINDLIST_SELECTED_TEXT_COLOR );

							for( int nSubItem = 0; nSubItem < 2; ++nSubItem ){
								RECT	rcSub;
								if( 0 == nSubItem ){
									// LVM_GETSUBITEMRECTはiSubItem=0だとLVIR_BOUNDSでも行全体の矩形を
									// 返してしまう(MSDNに明記された既知の仕様)ため、0列目だけは
									// LVIR_LABELでネイティブ描画と同じラベル位置を取得する。
									// LVIR_LABELの矩形そのままだと、既定の(自前描画していない)行の
									// 文字開始位置より少し左にずれる(実測)ため、既定の見た目に
									// 合わせて2px足す 20260804
									ListView_GetItemRect( hwndListDraw, nItem, &rcSub, LVIR_LABEL );
									rcSub.left += 2;
								}else{
									// 0列目と同じく、既定の文字開始位置に合わせて左余白を足す 20260804
									ListView_GetSubItemRect( hwndListDraw, nItem, nSubItem, LVIR_BOUNDS, &rcSub );
									rcSub.left += 6;
								}
								TCHAR	szText[256];
								ListView_GetItemText( hwndListDraw, nItem, nSubItem, szText, _countof(szText) );
								::DrawText( hdc, szText, -1, &rcSub, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
							}

							if( 0 != ( plvcd->nmcd.uItemState & CDIS_FOCUS ) ){
								::DrawFocusRect( hdc, &rcItem );
							}

							::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT );
							return TRUE;
						}
					}
					::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT );
					return TRUE;
				}
			}
			::SetWindowLongPtr( hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT );
			return TRUE;
		}
		break;
	}
	return FALSE;
}

/* ダイアログデータの設定: 一覧(機能名/ショートカット)を再構築する */
void CPropKeybindList::SetData( HWND hwndDlg )
{
	UpdateList( hwndDlg );
#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
	UpdatePresetCombo( hwndDlg );
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO
}

/* ダイアログデータの取得(読み取り専用ページのため何もしない) */
int CPropKeybindList::GetData( HWND hwndDlg )
{
	return TRUE;
}

/*! 一覧(機能名/ショートカット)を再構築する
	@date 20260803
	@date 20260803 カテゴリ順に並べ、カテゴリの境目に[カテゴリ名]の区切り行を挿む形に変更
	@date 20260803 空のカテゴリ(該当する機能が1つもない場合)も区切り行だけは出すように変更。
		機能一覧コンボボックス(SetCategory2Combo)の並びと一致させるため

	CFuncLookupのカテゴリ列挙(機能一覧コンボボックスや機能一覧リストボックスと
	同じもの)をそのままたどり、カテゴリ内の並び順もそれに従う。各機能の
	ショートカットはCKeyBind::GetKeyStrList()で取得する(機能一覧で選択した
	機能に「割り当てられているキー」を表示する処理と同じ関数)。
	ショートカット未割り当ての機能も含め、すべて一覧に出す(このリストから
	ショートカットを設定できるようにする予定のため) 20260803
*/
//! 大文字小文字を無視して sHaystack が sNeedle を含むか調べる
//! (機能名/キーの絞り込み欄用) 20260803
static bool ContainsCaseInsensitive( const std::wstring& sHaystack, const std::wstring& sNeedle )
{
	if( sNeedle.empty() ){
		return true;
	}
	std::wstring	sHay = sHaystack;
	std::wstring	sNdl = sNeedle;
	for( size_t i = 0; i < sHay.size(); ++i ){ sHay[i] = ::towlower( sHay[i] ); }
	for( size_t i = 0; i < sNdl.size(); ++i ){ sNdl[i] = ::towlower( sNdl[i] ); }
	return std::wstring::npos != sHay.find( sNdl );
}

void CPropKeybindList::UpdateList( HWND hwndDlg )
{
	HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );

	// 絞り込み欄の文字列を機能名/キーの部分一致で判定する 20260803
	WCHAR	szFilter[256];
	::GetWindowText( ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_FILTER ), szFilter, _countof(szFilter) );
	std::wstring	sFilter( szFilter );

	ListView_DeleteAllItems( hListView );
	s_vRowFuncCodes.clear();

	int	nRow = 0;
	int	nCategoryCount = m_cLookup.GetCategoryCount();

	for( int nCategory = 0; nCategory < nCategoryCount; ++nCategory ){
		int	nItemCount = m_cLookup.GetItemCount( nCategory );

		// カテゴリの区切り行は、該当する機能が1つもない(空の)カテゴリでも出す。
		// 機能一覧コンボボックス(SetCategory2Combo)がカテゴリの中身に関わらず
		// 全カテゴリを列挙するのと同じく、一覧としての網羅性を優先する。
		// ただし絞り込み中は、該当する機能が1つもないカテゴリの見出しは
		// あとで取り除く(下記bAnyChildAdded参照) 20260803
		int	nHeaderRow = nRow;
		bool	bAnyChildAdded = false;
		{
			// []で囲む代わりに太字(NM_CUSTOMDRAW側)で見出しであることを示す 20260803
			const TCHAR*	pszCategoryName = m_cLookup.Category2Name( nCategory );
			std::wstring	sHeader( ( NULL != pszCategoryName ) ? pszCategoryName : L"?" );

			LV_ITEM	HeaderItem;
			::ZeroMemory( &HeaderItem, sizeof_raw( HeaderItem ) );
			HeaderItem.mask     = LVIF_TEXT | LVIF_PARAM;
			HeaderItem.iItem    = nRow;
			HeaderItem.iSubItem = 0;
			HeaderItem.pszText  = const_cast<TCHAR*>( sHeader.c_str() );
			HeaderItem.lParam   = -1;	// カテゴリ区切り行の目印(NM_CUSTOMDRAWで判別)
			ListView_InsertItem( hListView, &HeaderItem );
			s_vRowFuncCodes.push_back( F_DISABLE );
			++nRow;
		}

		for( int nPos = 0; nPos < nItemCount; ++nPos ){
			// bGetUnavailable=trueにして、未登録の外部マクロなどのスロットも表示する。
			// このリストからショートカットを設定できるようにする予定であり、未登録の
			// マクロ枠にも先に登録しておきたいというケースがあるため、機能一覧
			// コンボボックス(SetListItem)と同じくすべて出す 20260803
			EFunctionCode	nFuncCode = m_cLookup.Pos2FuncCode( nCategory, nPos, true );
			if( F_DISABLE == nFuncCode ){
				continue;
			}

			CNativeT**	ppcAssignedKeyList = NULL;
			int	nAssignedKeyNum = CKeyBind::GetKeyStrList(
				G_AppInstance(),
				m_Common.m_sKeyBind.m_nKeyNameArrNum,
				(KEYDATA*)m_Common.m_sKeyBind.m_pKeyNameArr,
				&ppcAssignedKeyList,
				nFuncCode,
				TRUE	// デフォルト機能割り当ても含めた、実際に効いているショートカットにする
			);

			WCHAR	szFuncName[256];
			m_cLookup.Funccode2Name( nFuncCode, szFuncName, _countof(szFuncName) );

			// 先頭のショートカット("Shift+Ctrl+Alt+キー名"形式)を(修飾ビット,キー番号)に
			// 分解し、行選択時に上部の設定コントロールへ反映できるようlParamに詰めておく。
			// 複数キーが割り当てられている機能は先頭の1つだけを反映対象にする。
			// 何も割り当てられていない機能はショートカット列を空のままにする 20260803
			std::wstring	sKeys;
			int	nModifier = 0;
			int	nKeyIndex = -1;
			if( 0 < nAssignedKeyNum ){
				std::wstring	sFirstKeyLabel = ppcAssignedKeyList[0]->GetStringPtr();
				for( int i = 0; i < nAssignedKeyNum; ++i ){
					if( 0 < i ){
						sKeys += L", ";
					}
					sKeys += ppcAssignedKeyList[i]->GetStringPtr();
					delete ppcAssignedKeyList[i];
				}
				delete [] ppcAssignedKeyList;

				std::wstring	s = sFirstKeyLabel;
				if( 0 == s.compare( 0, 6, L"Shift+" ) ){ nModifier |= _SHIFT; s = s.substr( 6 ); }
				if( 0 == s.compare( 0, 5, L"Ctrl+" ) )  { nModifier |= _CTRL;  s = s.substr( 5 ); }
				if( 0 == s.compare( 0, 4, L"Alt+" ) )   { nModifier |= _ALT;   s = s.substr( 4 ); }
				for( int j = 0; j < m_Common.m_sKeyBind.m_nKeyNameArrNum; ++j ){
					if( s == m_Common.m_sKeyBind.m_pKeyNameArr[j].m_szKeyName ){
						nKeyIndex = j;
						break;
					}
				}
			}

			// 絞り込み欄に文字が入っていれば、機能名・キーのどちらにも
			// 一致しない行は一覧から除く 20260803
			if( !ContainsCaseInsensitive( szFuncName, sFilter ) && !ContainsCaseInsensitive( sKeys, sFilter ) ){
				continue;
			}

			LPARAM	nRowParam = ( -1 == nKeyIndex ) ? -2 : ( ( (LPARAM)nKeyIndex << 4 ) | (LPARAM)nModifier );
			s_vRowFuncCodes.push_back( nFuncCode );

			LV_ITEM	Item;
			::ZeroMemory( &Item, sizeof_raw( Item ) );
			Item.mask     = LVIF_TEXT | LVIF_PARAM;
			Item.iItem    = nRow;
			Item.iSubItem = 0;
			Item.pszText  = szFuncName;
			Item.lParam   = nRowParam;
			ListView_InsertItem( hListView, &Item );

			// LVM_SETITEMはiSubItem!=0のときmaskにLVIF_PARAMが入っていると失敗し、
			// テキストが設定されない(ショートカット列が空のまま表示される不具合の原因) 20260803
			Item.mask     = LVIF_TEXT;
			Item.iSubItem = 1;
			Item.pszText  = const_cast<TCHAR*>( sKeys.c_str() );
			ListView_SetItem( hListView, &Item );

			bAnyChildAdded = true;
			++nRow;
		}

		// 絞り込み中、該当する機能が1つもなかったカテゴリは見出し行ごと取り除く。
		// この時点でnHeaderRowは常に一覧の末尾の行なので、削除してもそれ以前の
		// 行番号には影響しない 20260803
		if( !sFilter.empty() && !bAnyChildAdded ){
			ListView_DeleteItem( hListView, nHeaderRow );
			s_vRowFuncCodes.erase( s_vRowFuncCodes.begin() + nHeaderRow );
			nRow = nHeaderRow;
		}
	}

	// 一覧を作り直すたびに、固定表示オーバーレイの種別名も最新にしておく
	// (再描画を待たず、絞り込みや登録/解除の直後にも即座に反映するため) 20260804
	UpdateStickyHeader( hwndDlg );
}

/*! 選択行のショートカットを上部の設定コントロール(Shift/Ctrl/Alt + キー)に反映する
	@date 20260803
	@date 20260803 シングルクリック(選択変更)では呼ばず、ダブルクリック(NM_DBLCLK)
		からのみ呼ぶように変更。単に見比べるためにクリックしただけで指定していた
		キーセットが変わってしまう問題があったため

	行のlParamに詰めておいた(修飾ビット,キー番号)を取り出してチェックボックスと
	コンボボックスへ反映する。逆方向(コントロール側の設定に合う行を一覧内で
	探してフォーカスする)はFocusMatchingRow()が担当する。
*/
void CPropKeybindList::ReflectSelection( HWND hwndDlg, int nItem )
{
	HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
	HWND	hCombo    = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );

	LV_ITEM	lvi;
	::ZeroMemory( &lvi, sizeof_raw( lvi ) );
	lvi.mask  = LVIF_PARAM;
	lvi.iItem = nItem;
	ListView_GetItem( hListView, &lvi );

	if( 0 <= lvi.lParam ){
		int	nKeyIndex = (int)( lvi.lParam >> 4 );
		int	nModifier = (int)( lvi.lParam & 0xF );

		s_bShiftChecked = 0 != ( nModifier & _SHIFT );
		s_bCtrlChecked  = 0 != ( nModifier & _CTRL );
		s_bAltChecked   = 0 != ( nModifier & _ALT );
		Combo_SetCurSel( hCombo, nKeyIndex );
	}else{
		// カテゴリ区切り行など、反映できるショートカットを持たない行を選んだ場合はクリアする
		s_bShiftChecked = s_bCtrlChecked = s_bAltChecked = false;
		Combo_SetCurSel( hCombo, -1 );
	}

	// チェックボックスはBS_OWNERDRAWのため、状態を変えたら明示的に再描画させる必要がある
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_SHIFT ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_CTRL ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_ALT ), NULL, TRUE );

	// 「キー入力」欄(直接入力)にも、一覧側から選んだショートカットを反映する。
	// 逆方向(直接入力欄でキーを押す→一覧に反映)はWM_NKMM_KEYBINDLIST_CAPTUREの
	// ハンドラが担当しており、この欄はどちらの操作をしても常に今の指定内容を
	// 表示する 20260810
	UpdateCaptureText( hwndDlg, Combo_GetCurSel( hCombo ) );

	UpdateActionArea( hwndDlg );
}

/*! 上部の設定コントロール(Shift/Ctrl/Alt + キー)が実際に割り当てられている
	機能の行の番号を返す
	@date 20260803
	@date 20260803 lParamの「先頭キー」比較ではなく、CKeyBind::GetFuncCodeAt()で
		実際にそのキーへ割り当てられている機能コードを調べ、s_vRowFuncCodesから
		該当行を探す方式に変更。1つの機能に複数キーを割り当てた場合、2つ目以降の
		キーも(先頭キーでなくても)正しく「割り当て済み」と認識できるようにするため

	@return 一致する行が無ければ-1。キー未選択(コンボが未選択状態)でも-1
*/
int CPropKeybindList::FindMatchingRow( HWND hwndDlg )
{
	HWND	hCombo = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );

	int	nKeyIndex = Combo_GetCurSel( hCombo );
	if( CB_ERR == nKeyIndex ){
		return -1;	// キー未選択なら探しようがない
	}

	int	nModifier = 0;
	if( s_bShiftChecked ){ nModifier |= _SHIFT; }
	if( s_bCtrlChecked )  { nModifier |= _CTRL;  }
	if( s_bAltChecked )   { nModifier |= _ALT;   }

	EFunctionCode	nFuncCode = CKeyBind::GetFuncCodeAt(
		m_Common.m_sKeyBind.m_pKeyNameArr[nKeyIndex], nModifier, TRUE );
	if( F_DISABLE == nFuncCode ){
		return -1;	// このキーには何も割り当てられていない
	}

	for( size_t i = 0; i < s_vRowFuncCodes.size(); ++i ){
		if( s_vRowFuncCodes[i] == nFuncCode ){
			return (int)i;
		}
	}
	return -1;
}

/*! 指定したキー(nKeyIndex)+修飾キー(nModifier)の組み合わせが既に
	何らかの機能に割り当て済みか調べる
	@date 20260805

	キーのドロップダウンリストを開いたとき、各項目(キー)ごとに現在チェック中の
	修飾キーとの組み合わせで割り当て済みかどうかを個別に判定して警告色表示するために使う。
	FindMatchingRow()と同じくCKeyBind::GetFuncCodeAt()で判定するが、コンボの
	現在の選択ではなく任意のキー番号を指定できる点が異なる。
*/
bool CPropKeybindList::IsKeyAssigned( int nKeyIndex, int nModifier )
{
	if( nKeyIndex < 0 || m_Common.m_sKeyBind.m_nKeyNameArrNum <= nKeyIndex ){
		return false;
	}
	EFunctionCode	nFuncCode = CKeyBind::GetFuncCodeAt(
		m_Common.m_sKeyBind.m_pKeyNameArr[nKeyIndex], nModifier, TRUE );
	return F_DISABLE != nFuncCode;
}

/*! 上部の設定コントロール(Shift/Ctrl/Alt + キー)と一致する行を一覧内で探し、
	見える位置までスクロールしてフォーカスする
	@date 20260803

	ReflectSelection()の逆方向。一致する行がなければ選択はそのままにする。
	いずれの場合もUpdateActionArea()で機能名欄・登録/解除ボタン・キーの
	警告色を最新の状態に更新する。
*/
void CPropKeybindList::FocusMatchingRow( HWND hwndDlg )
{
	int	nMatch = FindMatchingRow( hwndDlg );
	if( 0 <= nMatch ){
		HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
		ListView_SetItemState( hListView, -1, 0, LVIS_SELECTED );	// 既存の選択を解除
		ListView_SetItemState( hListView, nMatch, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		ListView_EnsureVisible( hListView, nMatch, FALSE );
		// EnsureVisibleでのスクロールはNM_CUSTOMDRAW(CDDS_PREPAINT)経由の
		// UpdateStickyHeader()呼び出しに任せると、実際に再描画されるまで
		// (例えばマウスを動かすまで)スティッキーの内容が古いまま反映されない
		// ことがあるため、ここで直接呼んで即座に反映する 20260804
		UpdateStickyHeader( hwndDlg );
	}
	UpdateActionArea( hwndDlg );
}

/*! 機能名欄・追加ボタン・解除ボタン・キーの警告色を、選択中の行と現在の
	キー指定に合わせて更新する
	@date 20260803
	@date 20260803 「登録」を、選択中の機能が既に持つ他のキーには触れない
		「追加」に変更(貼り付けのF9/Shift+Ins/Ctrl+Vのように1機能に複数キーを
		持たせられるようにするため)。「解除」もリスト選択ではなく、キー割り当て
		タブの「解除」ボタンと同じくキーセット側の指定内容だけを見るように変更

	・機能名欄: リストで選択中の行の機能名(未選択または区切り行なら空にする)
	・追加ボタン: 機能が選択されていて、かつキーセット側でキーが選ばれていれば
	  有効にする(現在指定中のキーセットを選択中の機能に追加できる)
	・解除ボタン: キーセット側でキーが選ばれていれば有効にする(リストの選択
	  状態とは無関係。キー割り当てタブの「解除」ボタンと同じ判定)
	・チェックボックス/コンボの警告色は、それぞれWM_CTLCOLORBTN/WM_DRAWITEMが
	  FindMatchingRow()を直接呼んで判定するため、ここでは再描画だけ要求する
*/
void CPropKeybindList::UpdateActionArea( HWND hwndDlg )
{
	HWND	hListView       = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
	HWND	hCombo          = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY );
	HWND	hEditFuncName   = ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_FUNCNAME );
	HWND	hButtonRegister = ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYBINDLIST_REGISTER );
	HWND	hButtonRelease  = ::GetDlgItem( hwndDlg, IDC_BUTTON_KEYBINDLIST_RELEASE );

	int	nSelected = ListView_GetNextItem( hListView, -1, LVNI_SELECTED );
	bool	bSelectedIsData = ( 0 <= nSelected )
		&& ( (size_t)nSelected < s_vRowFuncCodes.size() )
		&& ( F_DISABLE != s_vRowFuncCodes[nSelected] );

	bool	bKeySelected = ( CB_ERR != Combo_GetCurSel( hCombo ) );

	if( bSelectedIsData ){
		TCHAR	szFuncName[256];
		ListView_GetItemText( hListView, nSelected, 0, szFuncName, _countof(szFuncName) );
		::SetWindowText( hEditFuncName, szFuncName );
	}else{
		::SetWindowText( hEditFuncName, L"" );
	}
	::EnableWindow( hButtonRegister, bSelectedIsData && bKeySelected );
	::EnableWindow( hButtonRelease, bKeySelected );

	// キー関連コントロールの警告色を反映させるため再描画させる
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_SHIFT ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_CTRL ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_CHECK_ALT ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_KEY ), NULL, TRUE );
	::InvalidateRect( ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_CAPTURE ), NULL, TRUE );	// 20260814
}

/*! 「キー入力」欄(直接入力)の表示文字列を更新する
	@date 20260810

	nKeyIndexが指すキー名と、現在のs_bShiftChecked/CtrlChecked/AltCheckedを
	組み合わせた文字列("Ctrl+Z"等)を表示する。nKeyIndexが負なら空にして
	プレースホルダー("キーを押す")が見える状態に戻す。コンボ+チェックボックスと
	直接入力欄のどちらを操作しても、もう一方に結果を反映して常に一致させるために
	ReflectSelection()とWM_NKMM_KEYBINDLIST_CAPTUREのハンドラの両方から呼ぶ。
*/
void CPropKeybindList::UpdateCaptureText( HWND hwndDlg, int nKeyIndex )
{
	HWND	hCapture = ::GetDlgItem( hwndDlg, IDC_EDIT_KEYBINDLIST_CAPTURE );

	if( nKeyIndex < 0 || m_Common.m_sKeyBind.m_nKeyNameArrNum <= nKeyIndex ){
		::SetWindowText( hCapture, L"" );
		return;
	}

	std::wstring	sCombo;
	if( s_bShiftChecked ){ sCombo += L"Shift+"; }
	if( s_bCtrlChecked )  { sCombo += L"Ctrl+";  }
	if( s_bAltChecked )   { sCombo += L"Alt+";   }
	sCombo += m_Common.m_sKeyBind.m_pKeyNameArr[nKeyIndex].m_szKeyName;
	::SetWindowText( hCapture, sCombo.c_str() );
}

/*! リスト先頭に見えている行の種別を、固定表示オーバーレイに反映する
	@date 20260804
	@date 20260804 リスト自身のDCに直接重ね描き(CDDS_POSTPAINT)する方式も
		試したが、この環境ではCDRF_NOTIFYPOSTPAINTを返しても実際に描画した
		内容が反映されず(診断用の単色塗りすら表示されない)、原因を切り分け
		きれなかったため、確実に動作するSTATICオーバーレイ方式に戻した。
		スクロール時にゴミが残る問題は、再描画のたびにオーバーレイを最前面に
		上げてから明示的に再描画させることで対処する 20260804
	@date 20260804 先頭に見えている行自体が区切り行のときは、その区切り行と
		同じ種別名を重ねて表示すると2行連続で同じ種別が並んで見えてしまうため、
		1つ前の種別を表示するように変更。一番先頭(区切り行の前に何も種別が
		無い、つまり一覧最初の種別)のときは表示するものが無いのでオーバーレイ
		自体を隠す

	先頭に見えている行(ListView_GetTopIndex)から逆順に走査し、直近の種別
	区切り行(s_vRowFuncCodesにF_DISABLEを積んだ行、区切り行にしか積まれない)
	を探してその見出し文字列をオーバーレイへコピーする。先頭に見えている行
	自体が区切り行のときは、その行を含めず1つ手前から探し始めることで
	「1つ前の種別」を表示する。
*/
void CPropKeybindList::UpdateStickyHeader( HWND hwndDlg )
{
	HWND	hListView = ::GetDlgItem( hwndDlg, IDC_LIST_KEYBINDALL );
	HWND	hSticky   = ::GetDlgItem( hwndDlg, IDC_STATIC_KEYBINDLIST_STICKYHEADER );
	if( NULL == hSticky ){
		return;
	}

	int	nTop = ListView_GetTopIndex( hListView );
	bool	bTopIsHeader = ( (size_t)nTop < s_vRowFuncCodes.size() ) && ( F_DISABLE == s_vRowFuncCodes[nTop] );

	int	nCategoryRow = -1;
	for( int i = ( bTopIsHeader ? nTop - 1 : nTop ); 0 <= i; --i ){
		if( (size_t)i < s_vRowFuncCodes.size() && F_DISABLE == s_vRowFuncCodes[i] ){
			nCategoryRow = i;
			break;
		}
	}

	if( 0 <= nCategoryRow ){
		TCHAR	szText[256];
		ListView_GetItemText( hListView, nCategoryRow, 0, szText, _countof(szText) );
		::SetWindowText( hSticky, szText );
		::ShowWindow( hSticky, SW_SHOW );
	}else{
		::ShowWindow( hSticky, SW_HIDE );
	}

	// リストのスクロール内部最適化(前回描画済みビットマップの単純スクロール
	// 転送)がこのオーバーレイの存在を考慮せず、ゴミ(前の内容の残り)が見える
	// ことがあったため、リストが再描画されるたびに最前面へ上げてから明示的に
	// 再描画させる 20260804
	::SetWindowPos( hSticky, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	::InvalidateRect( hSticky, NULL, TRUE );
	::UpdateWindow( hSticky );
}

/*! キー割り当て設定をインポートする(キー割り当てタブのImport()と同じ
	CImpExpKeybindを使う。取り込んだ内容はm_Common.m_sKeyBind経由で
	共有されるため、このタブ側は一覧を作り直すだけでよい) 20260804
*/
void CPropKeybindList::Import( HWND hwndDlg )
{
	CImpExpKeybind	cImpExpKeybind( m_Common );

	if( !cImpExpKeybind.ImportUI( G_AppInstance(), hwndDlg ) ){
		return;	// インポートをしていない
	}

	UpdateList( hwndDlg );
	UpdateActionArea( hwndDlg );
#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
	UpdatePresetCombo( hwndDlg );
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO
}

/*! キー割り当て設定をエクスポートする(キー割り当てタブのExport()と同じ) 20260804 */
void CPropKeybindList::Export( HWND hwndDlg )
{
	CImpExpKeybind	cImpExpKeybind( m_Common );

	cImpExpKeybind.ExportUI( G_AppInstance(), hwndDlg );
}

#ifdef NKMM_FIX_KEYBIND_PRESET_COMBO
/*! 組み込みのキー割り当てプリセット(s_KeybindPresetTable)を適用する
	@date 20260812
	@date 20260813 「サクラエディタの既定へ初期化してから、プリセットの差分を再適用する」
		方式に変更。直前まで別のプリセットが適用されていた場合でも、そのプリセットにしか
		無い上書きが残ってしまわないようにするため(CShareData::ResetKeyBindToDefault()を
		先に呼ぶ)。適用後はUpdatePresetCombo()で選択項目も実際の状態に合わせて更新する
	@date 20260813 単独の「初期化」ボタンはプリセットコンボの先頭項目「サクラエディタ」
		(nResId=0、差分インポート無しで初期化のみ行う)に統合し、廃止した

	プリセットの実体はsakura_rc.rcにRCDATAとして埋め込まれた.keyファイル相当のデータ
	(元はkeybind_presets/*.key)。
*/
void CPropKeybindList::ApplyPreset( HWND hwndDlg, int nPresetIndex )
{
	if( nPresetIndex < 0 || _countof(s_KeybindPresetTable) <= (size_t)nPresetIndex ){
		return;
	}
	const SKeybindPresetInfo&	info = s_KeybindPresetTable[nPresetIndex];

	if( IDCANCEL == ::MYMESSAGEBOX( hwndDlg, MB_OKCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
		LS(STR_PROPCOMKEYBINDLIST_PRESET_CONFIRM), info.pszName ) ){
		// キャンセルされても、コンボ自体は選んだ項目の表示に切り替わってしまっている
		// (ネイティブのコンボボックスは、ドロップダウンで項目をクリックした時点で
		// CBN_SELCHANGEの前に表示が変わる仕様のため)。実際には何も変更していないので、
		// 現在の状態に合わせて表示を戻す 20260813
		UpdatePresetCombo( hwndDlg );
		return;
	}

	CShareData::ResetKeyBindToDefault( m_Common.m_sKeyBind );

	if( 0 != info.nResId ){	// 0(「サクラエディタ」)は初期化のみで、差分インポートは無い
		std::wstring	sErrMsg;
		if( !ImportPresetKeyBindInto( G_AppInstance(), m_Common, nPresetIndex, sErrMsg ) ){
			if( !sErrMsg.empty() ){
				ErrorMessage( hwndDlg, _T("%ls"), sErrMsg.c_str() );
			}
		}
	}

	UpdateList( hwndDlg );
	UpdateActionArea( hwndDlg );
	UpdatePresetCombo( hwndDlg );
}

/*! 現在のキー割り当てと一致する組み込みプリセットのインデックスを返す
	@return s_KeybindPresetTable内のインデックス。どれとも一致しなければ-1 20260813

	各プリセットについて「サクラエディタの既定へ初期化してからプリセットをインポートした
	結果」をm_Commonのコピー上でシミュレートし、現在のキー割り当て(m_Common.m_sKeyBind)と
	比較する。実データ(m_Common)には一切触れない。
*/
int CPropKeybindList::DetectCurrentPresetIndex()
{
	for( size_t i = 0; i < _countof(s_KeybindPresetTable); ++i ){
		CommonSetting	cScratch = m_Common;
		CShareData::ResetKeyBindToDefault( cScratch.m_sKeyBind );

		if( 0 != s_KeybindPresetTable[i].nResId ){	// 0(「サクラエディタ」)は初期化のみ
			std::wstring	sDummyErr;
			if( !ImportPresetKeyBindInto( G_AppInstance(), cScratch, (int)i, sDummyErr ) ){
				continue;
			}
		}
		if( IsSameKeyBind( cScratch.m_sKeyBind, m_Common.m_sKeyBind ) ){
			return (int)i;
		}
	}
	return -1;
}

/*! プリセット選択コンボの選択項目を、現在のキー割り当てに合わせて更新する
	@date 20260813

	どのプリセットとも一致しなければ「カスタマイズ」(index 0)を選択する。
*/
void CPropKeybindList::UpdatePresetCombo( HWND hwndDlg )
{
	HWND	hComboPreset = ::GetDlgItem( hwndDlg, IDC_COMBO_KEYBINDLIST_PRESET );
	int	nMatch = DetectCurrentPresetIndex();
	Combo_SetCurSel( hComboPreset, ( 0 <= nMatch ) ? ( nMatch + 1 ) : 0 );
}
#elif defined(NKMM_FIX_KEYBIND_TOOLBAR_RESET)
/*! キー割り当てを既定値に戻す(キー割り当てタブのInitializeToDefault()と同じ) 20260804 */
void CPropKeybindList::InitializeToDefault( HWND hwndDlg )
{
	if( IDCANCEL == ::MYMESSAGEBOX( hwndDlg, MB_OKCANCEL | MB_ICONQUESTION, GSTR_APPNAME,
		LS(STR_PROPCOMKEYBIND_INIT) ) ){
		return;
	}

	CShareData::ResetKeyBindToDefault( m_Common.m_sKeyBind );

	UpdateList( hwndDlg );
	UpdateActionArea( hwndDlg );
}
#endif // NKMM_FIX_KEYBIND_PRESET_COMBO

#endif // NKMM_FIX_KEYBIND_LIST_TAB
