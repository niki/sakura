/*!	@file
	@brief ファイルタイプ一覧ダイアログ

	@author Norio Nakatani
	@date 1998/12/23 新規作成
	@date 1999/12/05 再作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

class CDlgTypeList;

#ifndef _CDLGTYPELIST_H_
#define _CDLGTYPELIST_H_

#include "dlg/CDialog.h"
#ifdef NKMM_FIX_TYPELIST_ADD_ANY_TYPE
#include "uiparts/CMenuDrawer.h"
#endif // NKMM_
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
class CPropTypes;
struct STypeConfig;
#endif // NKMM_
using std::wstring;

/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief ファイルタイプ一覧ダイアログ
*/
class CDlgTypeList : public CDialog
{
public:
	// 型
	struct SResult{
		CTypeConfig	cDocumentType;	//!< 文書種類
		bool			bTempChange;	//!< 旧PROP_TEMPCHANGE_FLAG
	};

public:
	// インターフェース
	int DoModal( HINSTANCE, HWND, SResult* );	/* モーダルダイアログの表示 */

protected:
	// 実装ヘルパ関数
	BOOL OnLbnDblclk( int );
	BOOL OnBnClicked( int );
	BOOL OnActivate( WPARAM wParam, LPARAM lParam );
#ifdef NKMM_FIX_TYPELIST_ADD_ANY_TYPE
	BOOL OnDrawItem( WPARAM wParam, LPARAM lParam );
#endif // NKMM_
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	BOOL OnInitDialog( HWND, WPARAM, LPARAM );
	BOOL OnDestroy( void );
#endif // NKMM_
	INT_PTR DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam );
	void SetData();	/* ダイアログデータの設定 */
	void SetData(int);	/* ダイアログデータの設定 */
	LPVOID GetHelpIdTable(void);	//@@@ 2002.01.18 add
	bool Import( void );			// 2010/4/12 Uchi
	bool Export( void );			// 2010/4/12 Uchi
	bool InitializeType( void );	// 2010/4/12 Uchi
	bool CopyType();
	bool UpType();
	bool DownType();
	bool AddType();
	bool DelType();
	bool AlertFileAssociation();	// 2011/8/20 syat

#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	void CreateEmbeddedColorPanel();				//!< 埋め込みパネル(群)の生成
	void CommitEmbeddedColorPanel();				//!< 表示中タイプの設定パネルの内容を保存
	void RefreshEmbeddedColorPanel( int nIdx );	//!< 指定タイプの設定をパネルに表示
	void SwitchEmbeddedColorPanel( int nNewIdx );	//!< 表示タイプの切り替え(保存してから表示)
	void CaptureColorSnapshot( int nIdx );			//!< 初回表示時、キャンセル用に編集前の値を保存
	void RestoreColorSnapshots();					//!< キャンセル時、編集前の値に全て戻す
	void DiscardColorSnapshot( int nIdx );			//!< 並べ替え/削除などでインデックスの対応が
													//!< 崩れる場合に、そのスナップショットを確定させる(復元対象から外す)
	void OnTabSelChange();							//!< タブ切り替え(表示するページの切り替え)
#endif // NKMM_

private:
	CTypeConfig				m_nSettingType;
	// 関連付け状態
	bool m_bRegistryChecked[ MAX_TYPES ];	//レジストリ確認 未／済
	bool m_bExtRMenu[ MAX_TYPES ];			//右クリック登録 未／済
	bool m_bExtDblClick[ MAX_TYPES ];		//ダブルクリック 未／済
	bool m_bAlertFileAssociation;			//関連付け警告の表示フラグ
	bool m_bEnableTempChange;				//一時適用の有効化
#ifdef NKMM_FIX_TYPELIST_ADD_ANY_TYPE
	CMenuDrawer m_cMenuDrawer;				//「追加」ドロップダウンのオーナー描画メニュー
#endif // NKMM_
#ifdef NKMM_FIX_TYPELIST_EMBED_ALLTABS
	CPropTypes*	m_pcPropTypes;						//!< 全タブ共有の実体(各ページのCPropTypesXXXへ
													//!< reinterpret_castして使い回す。NULL=未生成)
	HWND		m_hwndTab;							//!< タブコントロール
	HWND		m_hwndPropPage[6];					//!< 各タブページの埋め込みウィンドウ
	int			m_nActiveTab;						//!< 現在表示中のタブIndex
	int					m_nEmbeddedColorTypeIdx;	//!< パネルが現在表示しているタイプIndex(-1=無効)
	STypeConfig*		m_pColorSnapshot[ MAX_TYPES ];	//!< キャンセル時に戻すための編集前の値(NULL=未取得)
	bool				m_bColorPanelFinalized;		//!< Commit/Restoreのどちらかを実行済みか
#endif // NKMM_
};



///////////////////////////////////////////////////////////////////////
#endif /* _CDLGTYPELIST_H_ */



