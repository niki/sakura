/*!	@file
	@brief 置換ダイアログ

	@author Norio Nakatani
	@date 1998/10/02  新規作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2001, hor
	Copyright (C) 2002, hor
	Copyright (C) 2007, ryoji
	Copyright (C) 2009, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CDLGREPLACE_H_
#define SAKURA_CDLGREPLACE_H_

#include "dlg/CDialog.h"
#include "recent/CRecent.h"
#include "util/window.h"

/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief 置換ダイアログボックス
*/
class CDlgReplace : public CDialog
{
public:
	/*
	||  Constructors
	*/
	CDlgReplace();
	/*
	||  Attributes & Operations
	*/
	HWND DoModeless( HINSTANCE, HWND, LPARAM, BOOL );	/* モーダルダイアログの表示 */
	void ChangeView( LPARAM );	/* モードレス時：置換・検索対象となるビューの変更 */

	SSearchOption	m_sSearchOption;	// 検索オプション
	int				m_bConsecutiveAll;	/* 「すべて置換」は置換の繰返し */	// 2007.01.16 ryoji
	std::wstring	m_strText;	// 検索文字列
	std::wstring	m_strText2;	// 置換後文字列
	int				m_nReplaceKeySequence;	//置換後シーケンス
	BOOL			m_bSelectedArea;	/* 選択範囲内置換 */
	int				m_bNOTIFYNOTFOUND;				/* 検索／置換  見つからないときメッセージを表示 */
	BOOL			m_bSelected;	/* テキスト選択中か */
	int				m_nReplaceTarget;	/* 置換対象 */	// 2001.12.03 hor
	int				m_nPaste;			/* 貼り付け？ */	// 2001.12.03 hor
	int				m_nReplaceCnt;		//すべて置換の実行結果		// 2002.02.08 hor
	bool			m_bCanceled;		//すべて置換で中断したか	// 2002.02.08 hor

	CLogicPoint		m_ptEscCaretPos_PHY;	// 検索/置換開始時のカーソル位置退避エリア

protected:
	CRecentSearch			m_cRecentSearch;
	SComboBoxItemDeleter	m_comboDelText;
	CRecentReplace			m_cRecentReplace;
	SComboBoxItemDeleter	m_comboDelText2;
	CFontAutoDeleter		m_cFontText;
	CFontAutoDeleter		m_cFontText2;

	/*
	||  実装ヘルパ関数
	*/
	BOOL OnCbnDropDown( HWND hwndCtl, int wID );
	BOOL OnInitDialog( HWND, WPARAM, LPARAM );
	BOOL OnDestroy();
	BOOL OnBnClicked( int );
	BOOL OnActivate( WPARAM wParam, LPARAM lParam );	// 2009.11.29 ryoji
	LPVOID GetHelpIdTable(void);	//@@@ 2002.01.18 add
#ifdef NKMM_FIX_REPLACE_PREVIEW
	BOOL OnCbnEditChange( HWND hwndCtl, int wID );	// 20260826 サンプル欄のライブ更新用
	INT_PTR DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam );	// 20260826 サンプル欄の配色用(WM_CTLCOLORSTATIC)
	BOOL OnDrawItem( WPARAM wParam, LPARAM lParam );	// 20260826 置換後サンプルの一致語句を青字で強調表示(オーナードロー)
#endif // NKMM_

	void SetData( void );		/* ダイアログデータの設定 */
	void SetCombosList( void );	/* 検索文字列/置換後文字列リストの設定 */
	int GetData( void );		/* ダイアログデータの取得 */
#ifdef NKMM_FIX_REPLACE_PREVIEW
	void UpdateSamplePreview( void );	/* 置換サンプル欄の更新(ライブプレビュー) 20260826 */
	void SetSampleAfterText( const std::wstring& str, int nHighlightPos, int nHighlightLen );	/* 置換後サンプル(オーナードロー)欄の内容差し替え+再描画 */

	COLORREF	m_crSampleText;		//!< サンプル欄の文字色(エディタの配色を反映) 20260826
	COLORREF	m_crSampleBack;		//!< サンプル欄の背景色(エディタの配色を反映) 20260826
	HBRUSH		m_hbrSampleBack;	//!< 上記背景色のブラシ(OnDestroyで解放)
	HFONT		m_hFontSample;		//!< サンプル欄のフォント(エディタの設定フォントを反映。CViewFont管理のためここでは解放しない) 20260826

	std::wstring	m_strSampleAfter;			//!< 置換後サンプルの表示文字列(オーナードローで使う) 20260826
	int				m_nSampleAfterHighlightPos;	//!< m_strSampleAfter中の強調表示開始位置(-1なら強調なし)
	int				m_nSampleAfterHighlightLen;	//!< m_strSampleAfter中の強調表示の長さ
#endif // NKMM_
};



///////////////////////////////////////////////////////////////////////
#endif /* SAKURA_CDLGREPLACE_H_ */



