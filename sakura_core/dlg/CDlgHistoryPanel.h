/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ダイアログボックス

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CDLGHISTORYPANEL_20260907_H_
#define SAKURA_CDLGHISTORYPANEL_20260907_H_

#ifdef NKMM_UNDO_HISTORY_PANEL

#include "dlg/CDialog.h"

class CFuncLookup;
class CEditView;

/*!
	@brief Undo/Redo履歴パネル(F5)

	Undo/Redoバッファ(COpeBuf)の内容を一覧表示する常時表示のフローティングパネル。
	タイトルバー・サイズ変更枠付きの独立したモードレスウィンドウで、表示中は
	ユーザーが自由にドラッグ移動・リサイズできる(CDlgWindowListと同じCDialog
	可変ダイアログ機構を流用)。フォーカスを失っても自滅しない点がコマンドパレット
	(CDlgCommandPalette)と異なり、表示/非表示はF5コマンドのみが制御する。

	位置は表示されるたびに毎回、親ウィンドウ(エディタ)の右下へ配置し直す
	(CDlgFuncList「アウトライン解析」の非ドッキング時の配置と同じ要領)。
	ユーザーがドラッグした位置は記憶しない。サイズ(初回のみダイアログ
	テンプレートの既定サイズの縦横それぞれ半分)はユーザーがリサイズすると
	CDialog::OnDestroy()経由でこのオブジェクトが生きている間覚えている。
	本体が最小化/復元されたときはパネルも連動して非表示/再表示する
	(FollowParentWindow())。

	行をクリックすると、その時点までUndo/Redoをジャンプする。現在位置より手前
	(実行済み)は通常表示、手前より後ろ(Redo待ち=取り消し済み)はグレーの
	斜体で表示し、Paint.NETの「履歴」パネルと同様に見分けられるようにする。
	下部には飾りのステータスバー風の帯を敷き、その左側に「元に戻す」
	「やり直し」ボタンを常設する(Paint.NETの履歴パネルと同じ配置)。
*/
class CDlgHistoryPanel : public CDialog
{
public:
	CDlgHistoryPanel();

	HWND DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView );

	void ChangeView( CEditView* pcView );	//!< アクティブなペイン/タブが切り替わったときに対象ビューを差し替えて再表示する
	void OnUndoStackChanged();	//!< Undo/Redoバッファが変化した通知を受けて一覧を再構築する
	void FollowParentWindow();	//!< 親ウィンドウ(エディタ本体)の最小化/復元にパネルの表示状態を追従させる

protected:
	BOOL OnInitDialog( HWND, WPARAM, LPARAM );
	BOOL OnSize( WPARAM wParam, LPARAM lParam );	//!< ユーザーのリサイズ操作に応じて一覧・ボタンをダイアログに追従させる
	BOOL OnNotify( WPARAM wParam, LPARAM lParam );
	BOOL OnBnClicked( int wID );	//!< 下部の「元に戻す」「やり直し」ボタン
	BOOL OnDestroy();

private:
	void RefreshList();	//!< COpeBuf::GetBlkCount()/GetBlkFuncCode()から一覧を作り直し、現在位置行を選択する。ボタンの有効/無効も同時に更新する
	LRESULT OnListCustomDraw( LPARAM lParam );	//!< 一覧の描画(実行済み/Redo待ちの色分け)
	void ExecuteJump( int nDispIndex );	//!< クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しする
	void RestoreEditorFocus();	//!< 一覧クリック/ボタンクリックの既定処理がこのパネルをアクティブ化してしまうのを打ち消し、エディタへフォーカスを戻す

	CFuncLookup*	m_pcFuncLookup;
	CEditView*		m_pcView;
	bool			m_bSuppressRefresh;	//!< ExecuteJump()のループ中、通知経由のRefreshList()を抑制する(ループ末尾で1回だけ呼ぶ)
	HFONT			m_hFontItalic;		//!< Redo待ち行の表示用(既定フォントのイタリック版)。OnInitDialogで生成しOnDestroyで破棄
	POINT			m_ptDefaultSize;	//!< OnSize()のResizeItem()計算用、ダイアログテンプレートの初期サイズ(OnInitDialogで一度だけ記録)
	RECT			m_rcListDefault;	//!< OnSize()のResizeItem()計算用、一覧の初期クライアント矩形(OnInitDialogで一度だけ記録)
	RECT			m_rcUndoBtnDefault;	//!< OnSize()のResizeItem()計算用、「元に戻す」ボタンの初期クライアント矩形
	RECT			m_rcRedoBtnDefault;	//!< OnSize()のResizeItem()計算用、「やり直し」ボタンの初期クライアント矩形
	RECT			m_rcStatusBarDefault;	//!< OnSize()のResizeItem()計算用、下部ステータスバー(飾り)の初期クライアント矩形
	bool			m_bParentWasMinimized;	//!< FollowParentWindow()が最小化⇔復元の遷移を検出するための直前状態
	HWND			m_hwndTrueParent;	//!< DoModeless()に渡された本来のオーナーハンドル。CDialog::DoModeless()内のResolveDialogOwnerWindow()がhwndParent非表示時にオーナーを誤って差し替えることがあるため、m_hwndParentとは別に保持する
};

#endif // NKMM_UNDO_HISTORY_PANEL

#endif /* SAKURA_CDLGHISTORYPANEL_20260907_H_ */
