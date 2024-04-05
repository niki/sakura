﻿/*!	@file
	@brief Dialog Box基底クラスヘッダファイル

	@author Norio Nakatani
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2002, YAZAKI
	Copyright (C) 2003, MIK
	Copyright (C) 2005, MIK
	Copyright (C) 2006, ryoji
	Copyright (C) 2011, nasukoji
	Copyright (C) 2012, Uchi

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/


#ifndef _CDIALOG_H_
#define _CDIALOG_H_

class CDialog;

struct DLLSHAREDATA;
class CRecent;

enum EAnchorStyle
{
	ANCHOR_NONE              = 0,
	ANCHOR_LEFT              = 1,
	ANCHOR_RIGHT             = 2,
	ANCHOR_LEFT_RIGHT        = 3,
	ANCHOR_TOP               = 4,
	ANCHOR_TOP_LEFT          = 5,
	ANCHOR_TOP_RIGHT         = 6,
	ANCHOR_TOP_LEFT_RIGHT    = 7,
	ANCHOR_BOTTOM            = 8,
	ANCHOR_BOTTOM_LEFT       = 9,
	ANCHOR_BOTTOM_RIGHT      = 10,
	ANCHOR_BOTTOM_LEFT_RIGHT = 11,
	ANCHOR_TOP_BOTTOM        = 12,
	ANCHOR_TOP_BOTTOM_LEFT   = 13,
	ANCHOR_TOP_BOTTOM_RIGHT  = 14,
	ANCHOR_ALL               = 15
};

struct SAnchorList
{
	int id;
	EAnchorStyle anchor;
};

struct SComboBoxItemDeleter
{
	CRecent*	pRecent;
	HWND		hwndCombo;
	WNDPROC		pComboBoxWndProc;
	WNDPROC		pEditWndProc;
	WNDPROC		pListBoxWndProc;
	SComboBoxItemDeleter(): pRecent(NULL), hwndCombo(NULL), pComboBoxWndProc(NULL), pEditWndProc(NULL), pListBoxWndProc(NULL){}
};

/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief ダイアログウィンドウを扱うクラス

	ダイアログボックスを作るときにはここから継承させる．

	@date 2002.2.17 YAZAKI CShareDataのインスタンスは、CProcessにひとつあるのみ。
*/
class CDialog
{
public:
	/*
	||  Constructors
	*/
	CDialog( bool bSizable = false, bool bCheckShareData = true );
	virtual ~CDialog();
	/*
	||  Attributes & Operations
	*/
	virtual INT_PTR DispatchEvent( HWND, UINT, WPARAM, LPARAM );	/* ダイアログのメッセージ処理 */
	INT_PTR DoModal( HINSTANCE, HWND, int, LPARAM );	/* モーダルダイアログの表示 */
	HWND DoModeless( HINSTANCE, HWND, int, LPARAM, int );	/* モードレスダイアログの表示 */
	HWND DoModeless( HINSTANCE, HWND, LPCDLGTEMPLATE, LPARAM, int );	/* モードレスダイアログの表示 */
	void CloseDialog( INT_PTR );

	virtual BOOL OnInitDialog( HWND, WPARAM wParam, LPARAM lParam );
	virtual void SetDialogPosSize();
	virtual BOOL OnDestroy( void );
	virtual BOOL OnNotify( WPARAM wParam, LPARAM lParam ){return FALSE;}
	BOOL OnSize();
	virtual BOOL OnSize( WPARAM wParam, LPARAM lParam );
	virtual BOOL OnMove( WPARAM wParam, LPARAM lParam );
	virtual BOOL OnDrawItem( WPARAM wParam, LPARAM lParam ){return TRUE;}
	virtual BOOL OnTimer( WPARAM wParam ){return TRUE;}
	virtual BOOL OnKeyDown( WPARAM wParam, LPARAM lParam ){return TRUE;}
	virtual BOOL OnDeviceChange( WPARAM wParam, LPARAM lParam ){return TRUE;}
	virtual int GetData( void ){return 1;}/* ダイアログデータの取得 */
	virtual void SetData( void ){return;}/* ダイアログデータの設定 */
	virtual BOOL OnBnClicked( int );
	virtual BOOL OnStnClicked( int ){return FALSE;}
	virtual BOOL OnEnChange( HWND hwndCtl, int wID ){return FALSE;}
	virtual BOOL OnEnKillFocus( HWND hwndCtl, int wID ){return FALSE;}
	virtual BOOL OnLbnSelChange( HWND hwndCtl, int wID ){return FALSE;}
	virtual BOOL OnLbnDblclk( int wID ){return FALSE;}
	virtual BOOL OnCbnSelChange( HWND hwndCtl, int wID ){return FALSE;}
	virtual BOOL OnCbnEditChange( HWND hwndCtl, int wID ){return FALSE;} // @@2005.03.31 MIK タグジャンプDialog
	virtual BOOL OnCbnDropDown( HWND hwndCtl, int wID );
	static BOOL OnCbnDropDown( HWND hwndCtl, bool scrollBar );
//	virtual BOOL OnCbnCloseUp( HWND hwndCtl, int wID ){return FALSE;}
	virtual BOOL OnCbnSelEndOk( HWND hwndCtl, int wID );

	virtual BOOL OnKillFocus( WPARAM wParam, LPARAM lParam ){return FALSE;}
	virtual BOOL OnActivate( WPARAM wParam, LPARAM lParam ){return FALSE;}	//@@@ 2003.04.08 MIK
	virtual int OnVKeyToItem( WPARAM wParam, LPARAM lParam ){ return -1; }
	virtual LRESULT OnCharToItem( WPARAM wParam, LPARAM lParam ){ return -1; }
	virtual BOOL OnPopupHelp( WPARAM, LPARAM );	//@@@ 2002.01.18 add
	virtual BOOL OnContextMenu( WPARAM, LPARAM );	//@@@ 2002.01.18 add
	virtual LPVOID GetHelpIdTable(void);	//@@@ 2002.01.18 add

	void ResizeItem( HWND hTarget, const POINT& ptDlgDefalut, const POINT& ptDlgNew, const RECT& rcItemDefault, EAnchorStyle anchor, bool bUpdate = true);
	void GetItemClientRect( int wID, RECT& rc );
	static void SetComboBoxDeleter( HWND hwndCtl, SComboBoxItemDeleter* data );
public:

	static bool DirectoryUp( TCHAR* );

public:
	HWND GetHwnd() const{ return m_hWnd; }
	//特殊インターフェース (使用は好ましくない)
	void _SetHwnd(HWND hwnd){ m_hWnd = hwnd; }

public:
	HINSTANCE		m_hInstance;	/* アプリケーションインスタンスのハンドル */
	HWND			m_hwndParent;	/* オーナーウィンドウのハンドル */
private:
	HWND			m_hWnd;			/* このダイアログのハンドル */
public:
	HWND			m_hwndSizeBox;
	LPARAM			m_lParam;
	BOOL			m_bModal;		/* モーダル ダイアログか */
	bool			m_bSizable;		// 可変ダイアログかどうか
	int				m_nShowCmd;		//	最大化/最小化
	int				m_nWidth;
	int				m_nHeight;
	int				m_xPos;
	int				m_yPos;
//	void*			m_pcEditView;
	DLLSHAREDATA*	m_pShareData;
	BOOL			m_bInited;
	HINSTANCE		m_hLangRsrcInstance;		// メッセージリソースDLLのインスタンスハンドル	// 2011.04.10 nasukoji
#ifdef NKMM_FIX_DIALOG_POS
	HWND			m_hwndPlaceOfWindow;
#endif // NKMM_

protected:
	void CreateSizeBox( void );
	BOOL OnCommand( WPARAM, LPARAM );

	HWND GetItemHwnd(int nID){ return ::GetDlgItem( GetHwnd(), nID ); }

	// コントロールに画面のフォントを設定	2012/11/27 Uchi
#ifdef NKMM_FIX_SETMAINFONT
	HFONT SetMainFont( HWND hTarget, int ptOfs = 0 );
#else
	HFONT SetMainFont( HWND hTarget );
#endif // NKMM_

#ifdef NKMM_FIX_DIALOG_POS
	/*!
	 * 配置モード
	 * @param bVertical 縦
	 * @param bHorizontal 横
	 */
	void SetPlaceSizeMode(bool bVertical, bool bHorizontal);
	
	/*!
	 * ウィンドウの位置を中央に設定 (すでに設定されているハンドルを使用する)
	 */
	void SetPlaceOfWindow();
	
	enum eDLGPLACE {
		DLGPLACE_TL,  // 左上
		DLGPLACE_TC,  // 中央上
		DLGPLACE_TR,  // 右上
		DLGPLACE_CL,  // 左中央
		DLGPLACE_CC,  // 中央
		DLGPLACE_CR,  // 右中央
		DLGPLACE_BL,  // 左下
		DLGPLACE_BC,  // 中央下
		DLGPLACE_BR,  // 右下
	};
	/*!
	 * ウィンドウの位置サイズを調整
	 * @param hWnd ハンドル
	 * @param prcView ビューウィンドウ矩形
	 * @param place 表示位置
	 * @param vertical 縦方向
	 * @param horizontal 横方向
	 */
	void SetPlaceOfWindow(HWND hWnd, const RECT *prcView = nullptr, eDLGPLACE place = DLGPLACE_CC);
	
	bool m_bPlaceVertical;
	bool m_bPlaceHorizontal;
#endif // NKMM_
};



///////////////////////////////////////////////////////////////////////
#endif /* _CDIALOG_H_ */



