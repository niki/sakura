/*!	@file
	@brief Undo/Redo履歴パネル(Paint.NET風)ウィンドウ

	@author Yu-zuki.
	@date 2026.09.07 新規作成 // NKMM_UNDO_HISTORY_PANEL
	@date 2026.09.07 CDialog(ダイアログテンプレート)依存をやめ、素のCreateWindowEx+
		自前WNDPROCへ全面書き直し(「本物の」ボーダーレス化がCDialogのDLGPROC/
		DLUテンプレート/ResizeItem機構と根本的に相性が悪かったため) // NKMM_UNDO_HISTORY_PANEL
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CDLGHISTORYPANEL_20260907_H_
#define SAKURA_CDLGHISTORYPANEL_20260907_H_

#ifdef NKMM_UNDO_HISTORY_PANEL

class CFuncLookup;
class CEditView;

/*!
	@brief Undo/Redo履歴パネル(F5)

	Undo/Redoバッファ(COpeBuf)の内容を一覧表示する常時表示のフローティングパネル。
	CDialog(ダイアログテンプレート)を使わず、素の::CreateWindowEx()+自前WNDPROCで
	実装した「本物の」ボーダーレスウィンドウ(https://github.com/melak47/
	BorderlessWindow 方式)。WS_CAPTION|WS_THICKFRAMEスタイル自体は生成時から
	保持するが、WM_NCCALCSIZE(クライアント矩形を提案されたウィンドウ矩形のまま
	変更しない)とWM_NCHITTEST(HitTest()参照、外周はリサイズ判定・タイトル帯は
	HTCAPTION)を自前WNDPROCが最初のメッセージから一貫して処理するため、OS標準の
	キャプション・枠は一切描画されない一方、全辺リサイズ・Aeroスナップ・DWMの影
	(DwmExtendFrameIntoClientArea)はOS標準のまま活きている。タイトル帯のドラッグは
	WM_NCHITTESTがHTCAPTIONを返すことでOS標準のタイトルバードラッグと全く同じに
	動作する(自前でドラッグループを回す必要は無い)。

	子コントロール(一覧・元に戻す/やり直しボタン・閉じるボタン・飾りの
	ステータスバー・隅のサイズグリップ)はダイアログテンプレートではなくすべて
	コードで::CreateWindowEx()する。レイアウトはCDialog::ResizeItem()のような
	「初期矩形からの固定オフセット」方式ではなく、WM_SIZEのたびにLayoutChildren()が
	現在のクライアント矩形から毎回素直に計算し直す(初期状態の記憶・DPI/DLU換算の
	食い違いによるレイアウト崩れを避けるため)。

	独立したモードレスウィンドウ(WS_EX_NOACTIVATEにより、表示中もエディタの
	フォーカス・アクティブ状態を奪わない)。表示のたびに毎回、親ウィンドウ
	(エディタ)の右下へ配置し直す(位置は記憶しない)。サイズはユーザーがリサイズ
	すると、閉じている間もこのオブジェクトが生きている間(m_nWidth/m_nHeight)覚えて
	いる。本体が最小化/復元されたときはパネルも連動して非表示/再表示する
	(FollowParentWindow())。表示/非表示はF5コマンド、または自前タイトルバーの
	閉じるボタンが制御する。

	行をクリックすると、その時点までUndo/Redoをジャンプする。現在位置より手前
	(実行済み)は通常表示、手前より後ろ(Redo待ち=取り消し済み)はグレーの
	斜体で表示し、Paint.NETの「履歴」パネルと同様に見分けられるようにする。
	下部には飾りのステータスバー風の帯を敷き、その左側に「元に戻す」
	「やり直し」ボタンを常設する(Paint.NETの履歴パネルと同じ配置)。
*/
class CDlgHistoryPanel
{
public:
	CDlgHistoryPanel();
	~CDlgHistoryPanel();

	HWND DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView );

	HWND GetHwnd() const{ return m_hWnd; }

	void ChangeView( CEditView* pcView );	//!< アクティブなペイン/タブが切り替わったときに対象ビューを差し替えて再表示する
	void OnUndoStackChanged();	//!< Undo/Redoバッファが変化した通知を受けて一覧を再構築する
	void FollowParentWindow();	//!< 親ウィンドウ(エディタ本体)の移動・リサイズ・最小化/復元にパネルの位置・表示状態を追従させる

	//! ウィンドウクラス登録(RegisterHistoryPanelClass())のlpfnWndProcに渡すため公開が必要
	static LRESULT CALLBACK WndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
	LRESULT HandleMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	void ComputeBottomRightPosition( int nWidth, int nHeight, int& x, int& y ) const;	//!< 親ウィンドウ矩形からこのパネルの右下配置(20pxマージン)のx,yを計算する。DoModeless()の初期配置でのみ使う(表示のたびの既定位置)
	void UpdateOffsetFromCurrentPosition();	//!< 現在のこのパネルの位置と親ウィンドウの位置の差をm_nOffsetX/Yへ記録する。生成直後、およびこのパネル自身のWM_MOVEのたびに呼ぶ(ユーザーがタイトル帯をドラッグして動かした位置を追従の基準にするため)

	void OnCreate();
	void OnDestroyWindow();
	void LayoutChildren();	//!< WM_SIZEのたびに現在のクライアント矩形から全子ウィンドウの位置・大きさを計算し直す
	LRESULT OnEraseBkgnd( HDC hdc );	//!< 背景+自前タイトル帯(色+文字)の描画
	LRESULT HitTest( POINT ptScreen ) const;	//!< WM_NCHITTESTのカスタム判定(外周はリサイズ、タイトル帯はHTCAPTION)
	LRESULT OnMouseActivate();
	void OnCommandMsg( int wID, HWND hwndCtl, UINT notifyCode );
	LRESULT OnNotifyMsg( LPARAM lParam );
	LRESULT OnDrawItemMsg( LPARAM lParam );

	void RefreshList();	//!< COpeBuf::GetBlkCount()/GetBlkFuncCode()から一覧を作り直し、現在位置行を選択する。ボタンの有効/無効も同時に更新する
	LRESULT OnListCustomDraw( LPARAM lParam );	//!< 一覧の描画(実行済み/Redo待ちの色分け)
	void ExecuteJump( int nDispIndex );	//!< クリックされた行までCommand_UNDO/Command_REDOをループ呼び出しする
	void RestoreEditorFocus();	//!< 一覧クリック/ボタンクリックの既定処理がこのパネルをアクティブ化してしまうのを打ち消し、エディタへフォーカスを戻す

	HWND			m_hWnd;
	HWND			m_hwndParent;	//!< オーナーウィンドウ(エディタ本体)のハンドル
	HWND			m_hwndList;
	HWND			m_hwndUndoBtn;
	HWND			m_hwndRedoBtn;
	HWND			m_hwndCloseBtn;
	HWND			m_hwndStatusBar;
	HWND			m_hwndSizeGrip;
	HINSTANCE		m_hInstance;
	CFuncLookup*	m_pcFuncLookup;
	CEditView*		m_pcView;
	bool			m_bSuppressRefresh;	//!< ExecuteJump()のループ中、通知経由のRefreshList()を抑制する(ループ末尾で1回だけ呼ぶ)
	HFONT			m_hFontMain;		//!< sakura標準のダイアログフォント相当(9pt、NKMM_RES_FONT_NAME)を自前構築したもの。OnCreateで生成しOnDestroyWindowで破棄
	HFONT			m_hFontItalic;		//!< Redo待ち行の表示用(m_hFontMainのイタリック版)。OnCreateで生成しOnDestroyWindowで破棄
	int				m_nTitleBarHeight;	//!< 自前タイトル帯の高さ(px)。LayoutChildren()/OnEraseBkgnd()/HitTest()で共用
	int				m_nCloseBtnWidth;	//!< 自前タイトルバーの閉じるボタンの幅(px)
	int				m_nStatusBarHeight;	//!< 飾りのステータスバーの高さ(px、CreateStatusWindow直後に一度だけ取得する実測値)
	int				m_nWidth;			//!< 直近のウィンドウ幅(px)。閉じるたびにOnDestroyWindowで記憶し、次回表示時の既定値にする(-1=未記憶)
	int				m_nHeight;			//!< 直近のウィンドウ高さ(px)。同上
	int				m_nOffsetX;			//!< 親ウィンドウ左上からのこのパネルの相対オフセット(px)。UpdateOffsetFromCurrentPosition()で更新し、FollowParentWindow()での追従に使う(表示中のみ有効、表示のたびDoModeless()の既定位置から再スタート)
	int				m_nOffsetY;			//!< 同上(Y方向)
	bool			m_bParentWasMinimized;	//!< FollowParentWindow()が最小化⇔復元の遷移を検出するための直前状態
};

#endif // NKMM_UNDO_HISTORY_PANEL

#endif /* SAKURA_CDLGHISTORYPANEL_20260907_H_ */
