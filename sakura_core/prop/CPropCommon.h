/*!	@file
	@brief 共通設定ダイアログボックスの処理

	@author	Norio Nakatani
	@date 1998/12/24 新規作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000, genta, jepro
	Copyright (C) 2001, genta
	Copyright (C) 2002, YAZAKI, aroka, Moca
	Copyright (C) 2005, MIK, Moca, aroka
	Copyright (C) 2006, ryoji
	Copyright (C) 2007, genta, ryoji
	Copyright (C) 2010, Uchi
	Copyright (C) 2013, Uchi

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

#ifndef SAKURA_CPROPCOMMON_8B67EE84_54E5_4541_A820_EE4FC61CCF0D_H_
#define SAKURA_CPROPCOMMON_8B67EE84_54E5_4541_A820_EE4FC61CCF0D_H_

#include "func/CFuncLookup.h"
#include "env/CommonSetting.h"

struct DLLSHAREDATA;
class CImageListMgr;
class CSMacroMgr;
class CMenuDrawer;// 2002/2/10 aroka to here

/*! プロパティシート番号
	@date 2008.6.22 Uchi #define -> enum に変更	
	@date 2008.6.22 Uchi順序変更 Win,Toolbar,Tab,Statusbarの順に、File,FileName 順に
*/
enum PropComSheetOrder {
	ID_PROPCOM_PAGENUM_GENERAL = 0,		//!< 全般
	ID_PROPCOM_PAGENUM_WIN,				//!< ウィンドウ
#ifndef NKMM_FIX_MAINMENU_FORCE_DEFAULT
	ID_PROPCOM_PAGENUM_MAINMENU,		//!< メインメニュー
#endif // NKMM_
	ID_PROPCOM_PAGENUM_TOOLBAR,			//!< ツールバー
	ID_PROPCOM_PAGENUM_TAB,				//!< タブバー
	ID_PROPCOM_PAGENUM_STATUSBAR,		//!< ステータスバー
	ID_PROPCOM_PAGENUM_EDIT,			//!< 編集
	ID_PROPCOM_PAGENUM_FILE,			//!< ファイル
	ID_PROPCOM_PAGENUM_FILENAME,		//!< ファイル名表示
	ID_PROPCOM_PAGENUM_BACKUP,			//!< バックアップ
	ID_PROPCOM_PAGENUM_FORMAT,			//!< 書式
	ID_PROPCOM_PAGENUM_GREP,			//!< 検索
	ID_PROPCOM_PAGENUM_KEYBOARD,		//!< キー割り当て
#ifdef NKMM_FIX_KEYBIND_LIST_TAB
	ID_PROPCOM_PAGENUM_KEYLIST,			//!< ショートカット一覧 20260803
#endif // NKMM_
	ID_PROPCOM_PAGENUM_CUSTMENU,		//!< カスタムメニュー
	ID_PROPCOM_PAGENUM_KEYWORD,			//!< 強調キーワード
	ID_PROPCOM_PAGENUM_HELPER,			//!< 支援
	ID_PROPCOM_PAGENUM_MACRO,			//!< マクロ
	ID_PROPCOM_PAGENUM_PLUGIN,			//!< プラグイン
	ID_PROPCOM_PAGENUM_MAX,
};
/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief 共通設定ダイアログボックスクラス

	1つのダイアログボックスに複数のプロパティページが入った構造に
	なっており、Dialog procedureとEvent Dispatcherがページごとにある．

	@date 2002.2.17 YAZAKI CShareDataのインスタンスは、CProcessにひとつあるのみ。
*/
class CPropCommon
{
public:
	/*
	||  Constructors
	*/
	CPropCommon();
	~CPropCommon();
	//	Sep. 29, 2001 genta マクロクラスを渡すように;
//@@@ 2002.01.03 YAZAKI m_tbMyButtonなどをCShareDataからCMenuDrawerへ移動したことによる修正。
	void Create( HWND, CImageListMgr*, CMenuDrawer* );	/* 初期化 */

	/*
	||  Attributes & Operations
	*/
	INT_PTR DoPropertySheet( int, bool );	/* プロパティシートの作成 */

	// 2002.12.11 Moca 追加
	void InitData( const int* = NULL, const TCHAR* = NULL, const TCHAR* = NULL );	//!< DLLSHAREDATAから一時データ領域に設定を複製する
	void ApplyData( int* = NULL );	//!< 一時データ領域からにDLLSHAREDATA設定をコピーする
	int GetPageNum(){ return m_nPageNum; }

	//
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

	//	Jun. 2, 2001 genta
	//	ここにあったEvent Handlerはprotectedエリアに移動した．

	HWND				m_hwndParent;	/* オーナーウィンドウのハンドル */
	HWND				m_hwndThis;		/* このダイアログのハンドル */
	PropComSheetOrder	m_nPageNum;
	DLLSHAREDATA*		m_pShareData;
	int					m_nKeywordSet1;
	//	Oct. 16, 2000 genta
	CImageListMgr*	m_pcIcons;	//	Image List
	
	//	Oct. 2, 2001 genta 外部マクロ追加に伴う，対応部分の別クラス化
	//	Oct. 15, 2001 genta Lookupはダイアログボックス内で別インスタンスを作るように
	//	(検索対象として，設定用common領域を指すようにするため．)
	CFuncLookup			m_cLookup;

	CMenuDrawer*		m_pcMenuDrawer;
	/*
	|| ダイアログデータ
	*/
	CommonSetting	m_Common;

	// 2005.01.13 MIK セット数増加
	struct SKeywordSetIndex{
		int typeId;
		int index[MAX_KEYWORDSET_PER_TYPE];
	};
	std::vector<SKeywordSetIndex>	m_Types_nKeyWordSetIdx;
	TCHAR			m_tempTypeName[MAX_TYPES_NAME];	//!< タイプ属性：名称
	TCHAR			m_tempTypeExts[MAX_TYPES_EXTS];	//!< タイプ属性：拡張子リスト
	bool			m_bTrayProc;
	HFONT			m_hKeywordHelpFont;		//!< キーワードヘルプ フォント ハンドル
	HFONT			m_hTabFont;				//!< タブ フォント ハンドル

protected:
	/*
	||  実装ヘルパ関数
	*/
	void OnHelp( HWND, int );	/* ヘルプ */
	int	SearchIntArr( int , int* , int );
//	void DrawToolBarItemList( DRAWITEMSTRUCT* );	/* ツールバーボタンリストのアイテム描画 */
//	void DrawColorButton( DRAWITEMSTRUCT* , COLORREF );	/* 色ボタンの描画 */ // 2002.11.09 Moca 未使用
//	BOOL SelectColor( HWND , COLORREF* );	/* 色選択ダイアログ */

	//	Jun. 2, 2001 genta
	//	Event Handler, Dialog Procedureの見直し
	//	Global関数だったDialog procedureをclassのstatic methodとして
	//	組み込んだ．
	//	ここから以下 Macroまで配置の見直しとstatic methodの追加

	//! 汎用ダイアログプロシージャ
	static INT_PTR DlgProc(
		INT_PTR (CPropCommon::*DispatchPage)( HWND, UINT, WPARAM, LPARAM ),
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	static INT_PTR DlgProc2( //独立ウィンドウ用
		INT_PTR (CPropCommon::*DispatchPage)( HWND, UINT, WPARAM, LPARAM ),
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	typedef	INT_PTR (CPropCommon::*pDispatchPage)( HWND, UINT, WPARAM, LPARAM );

	int nLastPos_Macro; //!< 前回フォーカスのあった場所
	int m_nLastPos_FILENAME; //!< 前回フォーカスのあった場所 ファイル名タブ用

	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得
	void Import( HWND );	//!< インポートする
	void Export( HWND );	//!< エクスポートする

	HFONT SetCtrlFont( HWND hwndDlg, int idc_static, const LOGFONT& lf );			//!< コントロールにフォント設定する		// 2013/4/24 Uchi
	HFONT SetFontLabel( HWND hwndDlg, int idc_static, const LOGFONT& lf, int nps );	//!< フォントラベルにフォントとフォント名設定する	// 2013/4/24 Uchi
#ifdef NKMM_FIX_KEYBIND_LIST_TAB
	HFONT		m_hKeybindListBoldFont = NULL;			//!< ショートカット一覧の種別(カテゴリ)見出し用の太字フォント。
														//!< 変数はCPropCommon自身に置く(ページクラスに置くと、全ページで
														//!< 共有される単一のCPropCommonインスタンスをそのページの型として
														//!< reinterpretする際に実際の確保サイズを超えてアクセスすることになるため) 20260803
#endif // NKMM_
#if defined(NKMM_FIX_KEYWORDSET_UI)
	//! 強調キーワードの色・フォントプレビュー表示用データ。CPropKeyword固有に見えるが、
	//! 変数はCPropCommon自身に置く必要がある(上記m_hKeybindListBoldFontと同じ理由。
	//! 以前はCPropKeywordクラス側に置かれており、実際の確保サイズを超えてアクセスする
	//! バグになっていたため、ここに移動した) 20260803
	//@{
	COLORREF	m_crKeywordSetText = RGB(0,0,0);		//!< 現在選択中のセットの強調表示色(文字) - プレビュー用
	COLORREF	m_crKeywordSetBack = RGB(255,255,255);	//!< 現在選択中のセットの強調表示色(背景) - プレビュー用
	bool		m_bKeywordSetColorValid = false;		//!< 上記(色)が有効か
	bool		m_bKeywordSetBold = false;				//!< 現在選択中のセットが太字か - プレビュー用
	bool		m_bKeywordSetUnderline = false;		//!< 現在選択中のセットが下線か - プレビュー用
	HFONT		m_hKeywordPreviewFont = NULL;			//!< プレビュー用フォント(m_bKeywordSetColorValidがtrueの間だけ作成される)
	//@}
#endif // NKMM_
};


/*!
	@brief 共通設定プロパティページクラス

	1つのプロパティページ毎に定義
	Dialog procedureとEvent Dispatcherがページごとにある．
	変数の定義はCPropCommonで行う
*/
//==============================================================
//!	全般ページ
class CPropGeneral : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得
};

//==============================================================
//!	ファイルページ
class CPropFile : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	//	Aug. 21, 2000 genta
	void EnableFilePropInput(HWND hwndDlg);	//	ファイル設定のON/OFF
};

//==============================================================
//!	キー割り当てページ
class CPropKeybind : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

	void Import( HWND );	//!< インポートする
	void Export( HWND );	//!< エクスポートする

private:
	void ChangeKeyList( HWND ); /* キーリストをチェックボックスの状態に合わせて更新する*/
#ifdef NKMM_FIX_KEYBIND_TOOLBAR_RESET
	void InitializeToDefault( HWND );	//!< キー割り当てを既定値に戻す 20260731
#endif // NKMM_FIX_KEYBIND_TOOLBAR_RESET
};

#ifdef NKMM_FIX_KEYBIND_LIST_TAB
//==============================================================
//!	ショートカット一覧ページ 20260803
class CPropKeybindList : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void UpdateList( HWND );	//!< 一覧(機能名/ショートカット)を再構築する
	void ReflectSelection( HWND, int );	//!< 選択行のショートカットを上部の設定コントロールに反映する 20260803
	void FocusMatchingRow( HWND );	//!< 上部の設定コントロールと一致する行を一覧内でフォーカスする 20260803
	int  FindMatchingRow( HWND );	//!< 上部の設定コントロールと一致する行の番号を返す(無ければ-1) 20260803
	bool IsKeyAssigned( int nKeyIndex, int nModifier );	//!< 指定したキー+修飾キーの組み合わせが既に何かに割り当て済みか調べる(ドロップダウン各項目の警告色判定用) 20260805
	void UpdateActionArea( HWND );	//!< 機能名欄・登録/解除ボタン・キー警告色を選択状態に合わせて更新する 20260803
	void UpdateStickyHeader( HWND );	//!< 先頭に見えている行の種別を、スクロールしても固定表示するオーバーレイに反映する 20260804
	void Import( HWND );	//!< キー割り当て設定をインポートする(キー割り当てタブと共通) 20260804
	void Export( HWND );	//!< キー割り当て設定をエクスポートする(キー割り当てタブと共通) 20260804
#ifdef NKMM_FIX_KEYBIND_TOOLBAR_RESET
	void InitializeToDefault( HWND );	//!< キー割り当てを既定値に戻す(キー割り当てタブと共通) 20260804
#endif // NKMM_FIX_KEYBIND_TOOLBAR_RESET
};
#endif // NKMM_FIX_KEYBIND_LIST_TAB

//==============================================================
//!	ツールバーページ
class CPropToolbar : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void DrawToolBarItemList( DRAWITEMSTRUCT* );	/* ツールバーボタンリストのアイテム描画 */
};

//==============================================================
//!	キーワードページ
class CPropKeyword : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	static INT_PTR CALLBACK DlgProc_dialog(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void SetKeyWordSet( HWND , int );	/* 指定キーワードセットの設定 */
	void GetKeyWordSet( HWND , int );	/* 指定キーワードセットの取得 */
	void DispKeywordCount( HWND hwndDlg );

	void Edit_List_KeyWord( HWND, HWND );		//!< リスト中で選択されているキーワードを編集する
	void Delete_List_KeyWord( HWND , HWND );	//!< リスト中で選択されているキーワードを削除する
	void Import_List_KeyWord( HWND , HWND );	//!< リスト中のキーワードをインポートする
	void Export_List_KeyWord( HWND , HWND );	//!< リスト中のキーワードをエクスポートする
	void Clean_List_KeyWord( HWND , HWND );		//!< リスト中のキーワードを整理する 2005.01.26 Moca
#if defined(NKMM_FIX_KEYWORDSET_UI)
	//! sakura.keywordset.csv読み込み時の編集ロック・再読込・セット編集ボタンの出し分け 20260802
	//@{
	bool IsKeywordCsvLoaded();					//!< sakura.keywordset.csvから強調キーワードを読み込んだか
	void EnableKeywordPropInput( HWND hwndDlg );	//!< CSV読み込み時、強調キーワードの編集系コントロールをDisableにする
	void SwitchKeywordRenameReloadButton( HWND hwndDlg, bool bReloadable );	//!< 「変更」「再読込」ボタンの表示切り替え(同じ位置に重ねて配置)
	void Reload_List_KeyWord( HWND, HWND );		//!< 選択中のセットをキーワードファイルから再読み込みする(セット単位)
	void SwitchKeywordSetEditButtons( HWND hwndDlg, bool bCsvLoaded );	//!< CSV読み込み時、「セット追加」「セット削除」を隠し、空いた場所にキーワードファイル名を表示する
	void UpdateKeywordFileLabel( HWND hwndDlg, int nIdx );	//!< セットに対応するキーワードファイル名の表示を更新する
	std::wstring MakeKeywordSetDisplayName( int nIdx );	//!< セット名コンボボックスの表示文字列を作る(組み込みキーワードなら"(embed)"を付与。実際のセット名は変更しない) 20260809
	void RefreshKeywordSetComboLabel( HWND hwndDlg, int nIdx );	//!< セット名コンボボックスの指定項目の表示だけを更新する(選択状態は維持) 20260809
	//@}

	//! 強調キーワードの色・フォントプレビュー表示
	//@{
	bool GetKeywordSetColor( int nIdx, COLORREF& crText, COLORREF& crBack, bool& bBold, bool& bUnderline, LOGFONT& lf );	//!< セットに割り当てられた強調表示色・フォント属性(共通/タイプ別の実フォント含む)を取得する(いずれのタイプにも未割り当てならfalse)
	void UpdateKeywordPreviewFont( HWND hwndList, const LOGFONT& lfBase );	//!< プレビュー用フォントを作り直す(書体はlfBase、高さ/幅はリスト自身のフォントに合わせる)
	//@}
#endif // NKMM_
};

//==============================================================
//!	カスタムメニューページ
class CPropCustmenu : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	void SetDataMenuList( HWND, int );
	int  GetData( HWND );	//!< ダイアログデータの取得
	void Import( HWND );	//!< カスタムメニュー設定をインポートする
	void Export( HWND );	//!< カスタムメニュー設定をエクスポートする
#ifdef NKMM_FIX_CUSTMENU_RESET
	void InitializeToDefault( HWND );	//!< カスタムメニューを既定値に戻す 20260731
#endif // NKMM_FIX_CUSTMENU_RESET
};

//==============================================================
//!	書式ページ
class CPropFormat : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void ChangeDateExample( HWND hwndDlg );
	void ChangeTimeExample( HWND hwndDlg );

	//	Sept. 10, 2000 JEPRO	次行を追加
	void EnableFormatPropInput( HWND hwndDlg );	//	書式設定のON/OFF
};

//==============================================================
//!	支援ページ
class CPropHelper : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得
};

//==============================================================
//!	バックアップページ
class CPropBackup : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	//	Aug. 16, 2000 genta
	void EnableBackupInput(HWND hwndDlg);	//	バックアップ設定のON/OFF
	//	20051107 aroka
	void UpdateBackupFile(HWND hwndDlg);	//	バックアップファイルの詳細設定
};

//==============================================================
//!	ウィンドウページ
class CPropWin : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	//	Sept. 9, 2000 JEPRO		次行を追加
	void EnableWinPropInput( HWND hwndDlg) ;	//	ウィンドウ設定のON/OFF
};

//==============================================================
//!	タブ動作ページ
class CPropTab : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void EnableTabPropInput(HWND hwndDlg);
};

//==============================================================
//!	編集ページ
class CPropEdit : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void EnableEditPropInput( HWND hwndDlg );
};

//==============================================================
//!	検索ページ
class CPropGrep : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void SetRegexpVersion( HWND ); // 2007.08.12 genta バージョン表示
};

//==============================================================
//!	マクロページ
class CPropMacro : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void InitDialog( HWND hwndDlg );//!< Macroページの初期化
	//	To Here Jun. 2, 2001 genta
	void SetMacro2List_Macro( HWND hwndDlg );//!< Macroデータの設定
	void SelectBaseDir_Macro( HWND hwndDlg );//!< Macroディレクトリの選択
	void OnFileDropdown_Macro( HWND hwndDlg );//!< ファイルドロップダウンが開かれるとき
	void CheckListPosition_Macro( HWND hwndDlg );//!< リストビューのFocus位置確認
	static int CALLBACK DirCallback_Macro( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData );
};

//==============================================================
//!	ファイル名表示ページ
class CPropFileName : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	static int SetListViewItem_FILENAME( HWND hListView, int, LPTSTR, LPTSTR, bool );//!<ListViewのアイテムを設定
	static void GetListViewItem_FILENAME( HWND hListView, int, LPTSTR, LPTSTR );//!<ListViewのアイテムを取得
	static int MoveListViewItem_FILENAME( HWND hListView, int, int );//!<ListViewのアイテムを移動する
};

//==============================================================
//!	ステータスバーページ
class CPropStatusbar : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得
};

//==============================================================
//!	プラグインページ
class CPropPlugin : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	std::tstring GetReadMeFile(const std::tstring& sName);	//	Readme ファイルの取得
	bool BrowseReadMe(const std::tstring& sReadMeName);		//	Readme ファイルの表示
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得

private:
	void SetData_LIST( HWND );
	void InitDialog( HWND hwndDlg );	//!< Pluginページの初期化
	void EnablePluginPropInput(HWND hwndDlg);
};

#ifndef NKMM_FIX_MAINMENU_FORCE_DEFAULT
//==============================================================
//!	メインメニューページ
class CPropMainMenu : CPropCommon
{
public:
	//!	Dialog Procedure
	static INT_PTR CALLBACK DlgProc_page(
		HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
protected:
	//! Message Handler
	INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	void SetData( HWND );	//!< ダイアログデータの設定
	int  GetData( HWND );	//!< ダイアログデータの取得
	void Import( HWND );	//!< メニュー設定をインポートする
	void Export( HWND );	//!< メニュー設定をエクスポートする

private:
	bool GetDataTree( HWND, HTREEITEM, int );

	bool Check_MainMenu( HWND, std::wstring& );						// メニューの検査
	bool Check_MainMenu_Sub( HWND, HTREEITEM, int, std::wstring& );	// メニューの検査
};
#endif // NKMM_



///////////////////////////////////////////////////////////////////////
#endif /* SAKURA_CPROPCOMMON_8B67EE84_54E5_4541_A820_EE4FC61CCF0D_H_ */
