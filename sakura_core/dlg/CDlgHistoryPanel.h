/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ウィンドウ

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
	@date 2026.09.07 CDialog依存をやめ、素のCreateWindowEx+自前WNDPROCへ全面書き直し
	@date 2026.09.07 ボーダーレスウィンドウの共通機構をwindow/CBorderlessWndへ
		抽出し、それを継承する形に整理(他機能でも再利用できるように)
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CDLGHISTORYPANEL_20260907_H_
#define SAKURA_CDLGHISTORYPANEL_20260907_H_

#ifdef NKMM_UNDO_HISTORY_PANEL

#include "window/CBorderlessWnd.h"

class CFuncLookup;
class CEditView;

/*!
	@brief Undo/Redo履歴パネル(F5)

	Undo/Redoバッファ(COpeBuf)の内容を一覧表示する常時表示のフローティングパネル。
	見た目・移動・リサイズ・親ウィンドウへの追従といった「本物のボーダーレス
	ウィンドウ」としての共通部分はwindow/CBorderlessWndに任せ、このクラスは
	Undo履歴パネル固有の中身(一覧・「元に戻す」「やり直し」ボタン・飾りの
	ステータスバー)だけを持つ。表示/非表示はF5コマンド、または自前タイトル
	バーの閉じるボタン(クリックするとF_SHOWUNDOHISTORYPANEL、つまりF5と同じ
	トグルコマンドを呼ぶ。OnCloseRequested()参照)が制御する。

	行をクリックすると、その時点までUndo/Redoをジャンプする。現在位置より手前
	(実行済み)は通常表示、手前より後ろ(Redo待ち=取り消し済み)はグレーの
	斜体で表示し、Paint.NETの「履歴」パネルと同様に見分けられるようにする。
	下部には飾りのステータスバー風の帯を敷き、その左側に「元に戻す」
	「やり直し」ボタンを常設する(Paint.NETの履歴パネルと同じ配置)。
*/
class CDlgHistoryPanel : public CBorderlessWnd
{
public:
	CDlgHistoryPanel();

	HWND DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView );

	void ChangeView( CEditView* pcView );	//!< アクティブなペイン/タブが切り替わったときに対象ビューを差し替えて再表示する
	void OnUndoStackChanged();	//!< Undo/Redoバッファが変化した通知を受けて一覧を再構築する
	// GetHwnd()/FollowParentWindow()はCBorderlessWnd(CWnd)から継承したものをそのまま使う

protected:
	// CBorderlessWndが要求する仮想関数
	virtual LPCTSTR GetWindowClassName() const;
	virtual LPCTSTR GetTitleText() const;
	virtual void GetDefaultSize( int& nWidth, int& nHeight ) const;
	virtual void LayoutChildren();	//!< 一覧・ボタン・ステータスバー・サイズグリップの配置(閉じるボタンは基底が自動配置する)
	virtual void OnCloseRequested();	//!< F5と同じトグルコマンド(F_SHOWUNDOHISTORYPANEL)を呼んで閉じる

	virtual LRESULT OnCreate( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnDestroy( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnCommand( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );	//!< 「元に戻す」「やり直し」ボタン。それ以外(閉じるボタン含む)は基底へ委譲する
	virtual LRESULT OnNotify( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );

private:
	void RefreshList();	//!< COpeBuf::GetBlkCount()/GetBlkFuncCode()から一覧を作り直し、現在位置行を選択する。ボタンの有効/無効も同時に更新する
	LRESULT OnListCustomDraw( LPARAM lParam );	//!< 一覧の描画(実行済み/Redo待ちの色分け)
	void ExecuteJump( int nDispIndex );	//!< クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しする
	void RestoreEditorFocus();	//!< 一覧クリック/ボタンクリックの既定処理がこのパネルをアクティブ化してしまうのを打ち消し、エディタへフォーカスを戻す

	HWND			m_hwndList;
	HWND			m_hwndUndoBtn;
	HWND			m_hwndRedoBtn;
	HWND			m_hwndStatusBar;
	HWND			m_hwndSizeGrip;
	CFuncLookup*	m_pcFuncLookup;
	CEditView*		m_pcView;
	bool			m_bSuppressRefresh;	//!< ExecuteJump()のループ中、通知経由のRefreshList()を抑制する(ループ末尾で1回だけ呼ぶ)
	HFONT			m_hFontItalic;		//!< Redo待ち行の表示用(GetMainFont()のイタリック版)。OnCreateで生成しOnDestroyで破棄
	int				m_nStatusBarHeight;	//!< 飾りのステータスバーの高さ(px、CreateStatusWindow直後に一度だけ取得する実測値)
};

#endif // NKMM_UNDO_HISTORY_PANEL

#endif /* SAKURA_CDLGHISTORYPANEL_20260907_H_ */
