/*!	@file
	@brief コマンドパレットダイアログボックス

	@author Yu-zuki.
	@date 2026.08.18 新規作成 // NKMM_COMMAND_PALETTE
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CDLGCOMMANDPALETTE_20260818_H_
#define SAKURA_CDLGCOMMANDPALETTE_20260818_H_

#ifdef NKMM_COMMAND_PALETTE

#include "dlg/CDialog.h"
#include "Funccode_enum.h"

#include <vector>
#include <string>
#include <map>

class CFuncLookup;

/*!
	@brief コマンドパレット(Shift+Ctrl+P)

	全コマンド(コマンドID・機能名・ショートカットキー)と、現在開いている
	ファイルの一覧をひとつの絞り込みリストにまとめ、Enterで実行/切り替えする。
*/
class CDlgCommandPalette : public CDialog
{
public:
	CDlgCommandPalette();

	HWND DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup );

	void FollowParentWindow();	//!< 親ウィンドウ(エディタ)の移動・サイズ変更に追従して表示位置を更新する

protected:
	BOOL OnInitDialog( HWND, WPARAM, LPARAM );
	BOOL OnBnClicked( int wID );
	BOOL OnEnChange( HWND hwndCtl, int wID );
	BOOL OnNotify( WPARAM wParam, LPARAM lParam );
	BOOL OnTimer( WPARAM wParam );	//!< スライドインアニメーション用
	BOOL OnActivate( WPARAM wParam, LPARAM lParam );	//!< 非アクティブ化時に自滅する
	BOOL OnDestroy();	//!< 生成したGDIオブジェクトの後始末
private:
	void StartSlideAnimation();	//!< 上からのスライドインアニメーションを開始する
	void CloseOnDeactivate();	//!< 非アクティブ化による自滅処理の実体(OnTimerから遅延実行)
	LRESULT OnListCustomDraw( LPARAM lParam );	//!< 一覧(VSCode風の1行=アイコン+名前+右寄せ)の描画
	int GetShellIconIndex( const std::wstring& sPath );	//!< パスの拡張子から共有システムアイコン一覧の索引を得る(拡張子ごとにキャッシュ)

	int		m_nSlideX;			//!< スライドインアニメーション中のX座標(固定)
	int		m_nSlideTargetY;	//!< スライドインアニメーションの最終Y座標
	int		m_nSlideStartY;		//!< スライドインアニメーションの開始Y座標
	DWORD	m_dwSlideStartTick;	//!< スライドインアニメーション開始時刻(GetTickCount)

	bool	m_bReactivateParentOnClose;	//!< CloseOnDeactivate()で親ウィンドウを前面に戻すかどうか

	HFONT	m_hFontList;	//!< 一覧の文字表示用(既定フォントより大きめ、通常太さ)。既定フォントから生成しOnDestroyで破棄する
	std::map<std::wstring, int>	m_mapExtToIconIndex;	//!< 拡張子(小文字)→共有システムアイコン一覧の索引

	//! 一覧1行の種別
	enum EPaletteRowKind {
		ROWKIND_COMMAND,	//!< コマンド(" > "で絞り込み)
		ROWKIND_WINDOW,		//!< 開いているウィンドウ("edt "で絞り込み)
		ROWKIND_RECENT,		//!< 最近使ったファイル(既定、絞り込み記号なし)
	};

	//! 一覧1行分
	struct PaletteRow {
		EPaletteRowKind	kind;
		int				nPostId;	//!< kind==ROWKIND_COMMAND/RECENTのとき、WM_COMMANDで送るID(機能番号 または IDM_SELMRU+i)
		HWND			hwndFile;	//!< kind==ROWKIND_WINDOWのとき、切り替え先ウィンドウ
		std::wstring	sName;		//!< 表示名(絞り込みの対象にもなる)
		std::wstring	sType;		//!< 種別表示("コマンド"/"ウィンドウ"/"最近使ったファイル")
		std::wstring	sId;		//!< コマンドIDの表示文字列(コマンド行以外は空)
		std::wstring	sSub;		//!< コマンドのショートカットキー、またはファイルのフルパス
	};

	void BuildAllRows();
	void UpdateList();
	void ExecuteRow( const PaletteRow& row );
	void ExecuteSelected();

	CFuncLookup*				m_pcFuncLookup;
	std::vector<PaletteRow>	m_vAllRows;
};

#endif // NKMM_COMMAND_PALETTE

#endif /* SAKURA_CDLGCOMMANDPALETTE_20260818_H_ */
