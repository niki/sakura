/*!	@file
	@brief 「本物の」ボーダーレスウィンドウの基底クラス

	@author Yu-zuki.
	@date 2026.09.07 新規作成。NKMM_UNDO_HISTORY_PANELのCDlgHistoryPanelから抽出・汎用化
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef _CBORDERLESSWND_H_
#define _CBORDERLESSWND_H_

#include "window/CWnd.h"

/*!
	@brief 「本物の」ボーダーレスウィンドウの基底クラス
		(https://github.com/melak47/BorderlessWindow の技法に準拠)

	WS_CAPTION|WS_THICKFRAMEスタイル自体は保持する(OS標準の全辺リサイズ・Aeroスナップ・
	DWMの正しい合成(影)を活かすため)が、WM_NCCALCSIZE(クライアント矩形を提案された
	ウィンドウ矩形のまま変更しない)とWM_NCHITTEST(外周はリサイズ判定、タイトル帯は
	HTCAPTION)を横取りすることで、OS標準のキャプション・枠の描画だけを消す。見た目上の
	タイトル帯(色付き背景+タイトル文字+閉じるボタン)は自前描画で、ドラッグ移動は
	WM_NCHITTESTがHTCAPTIONを返すことでOS標準のタイトルバードラッグと全く同じに動作する
	(自前でドラッグループを回す必要は無い)。常時WS_EX_NOACTIVATE(クリックでアクティブ
	化されず、フォーカスも奪わない)。

	CWndを継承する具象クラスの実装手順:
	@li GetWindowClassName()/GetTitleText()/GetDefaultSize()を実装する
	@li CreateBorderlessWindow()を呼んで生成する
	@li 自分の子コントロールはOnCreate()をオーバーライドして生成する
		(必ずCBorderlessWnd::OnCreate()を先に呼ぶこと。フォント(GetMainFont())・
		DWMの影のセットアップを行っている)
	@li 閉じるボタンが必要ならCreateCloseButton()を呼ぶ(任意の子ウィンドウIDを指定。
		自動的にBS_OWNERDRAWで生成され、描画・クリック時のOnCloseRequested()呼び出しは
		基底クラスが処理する)
	@li LayoutChildren()をオーバーライドして、WM_SIZEのたびに自分の子コントロールを
		現在のクライアント矩形に合わせて配置し直す(閉じるボタン自身の再配置は基底が
		自動で行う)
	@li 閉じるボタンを押したときの挙動を既定(::DestroyWindow())から変えたい場合は
		OnCloseRequested()をオーバーライドする
	@li OnCommand()/OnNotify()/OnDrawItem()/OnDestroy()をオーバーライドする場合は、
		自分で処理しないIDメッセージについて必ず基底クラス版(CBorderlessWnd::Onxxx())
		を呼ぶこと(閉じるボタンのクリック・描画・共通クリーンアップを基底が担っている)

	親ウィンドウの移動・リサイズ・最小化/復元に位置・表示状態を追従させたい場合は、
	親ウィンドウ側のWM_MOVE/WM_SIZEハンドラからFollowParentWindow()を呼ぶ
	(CDlgFind::FollowParentWindow()と同様の外側からの呼び出しパターン)。追従は
	「親ウィンドウとの相対オフセットを保ったまま平行移動」で、このパネル自身の
	WM_MOVE(ユーザーがタイトル帯をドラッグした場合を含む)のたびにオフセットを
	記録し直すため、ユーザーがドラッグした位置を追従の基準にできる。
*/
class CBorderlessWnd : public CWnd
{
public:
	CBorderlessWnd();
	virtual ~CBorderlessWnd();

	//! ボーダーレスウィンドウの生成。既定サイズ・タイトル文字は仮想関数から取得する
	HWND CreateBorderlessWindow( HINSTANCE hInstance, HWND hwndParent );

	//! 親ウィンドウの移動・リサイズ・最小化/復元に位置・表示状態を追従させる
	void FollowParentWindow();

protected:
	// 派生クラスが実装する仮想関数
	virtual LPCTSTR GetWindowClassName() const = 0;	//!< ウィンドウクラス名(プロセス内で一意な文字列)
	virtual LPCTSTR GetTitleText() const = 0;			//!< タイトル帯に表示する文字列
	virtual void GetDefaultSize( int& nWidth, int& nHeight ) const = 0;	//!< 初回表示時の既定サイズ(px。DpiScaleX/Y()適用後の値を返すこと)
	virtual void GetMinTrackSize( int& nWidth, int& nHeight ) const;		//!< リサイズ可能な最小サイズ(px)。既定は180x200相当
	virtual void LayoutChildren(){}	//!< WM_SIZEのたびに呼ばれる。派生クラスの子コントロールを現在のクライアント矩形に合わせて配置し直す(閉じるボタン自身は基底が自動配置するので触れなくてよい)
	virtual void OnCloseRequested(){ ::DestroyWindow( GetHwnd() ); }	//!< 閉じるボタンが押された時の処理。既定は::DestroyWindow()

	//! 閉じるボタンをBS_OWNERDRAWで生成する。nCtrlIdは派生クラスが選んだ任意の
	//! 子ウィンドウID(OnCommand/OnDrawItemでの判定に使われる)。派生クラスの
	//! OnCreate()内、CBorderlessWnd::OnCreate()を呼んだ後に呼ぶこと
	HWND CreateCloseButton( int nCtrlId );

	HFONT GetMainFont() const{ return m_hFontMain; }	//!< 自前構築した既定フォント(9pt、NKMM_RES_FONT_NAME)。派生クラスも自分の子コントロールへWM_SETFONTしてよい
	int GetTitleBarHeight() const{ return m_nTitleBarHeight; }
	int GetCloseButtonWidth() const{ return m_nCloseBtnWidth; }

	//! タイトル帯・閉じるボタンの配色。OSの現在の配色(GetSysColor(COLOR_ACTIVECAPTION)等)
	//! をそのつど問い合わせる(決め打ちにしない。Windowsの「タイトルバーにアクセント
	//! カラーを表示する」設定に自動追従させるため)
	static COLORREF TitleBarBackColor();
	static COLORREF TitleBarTextColor();
	static COLORREF TitleBarBackColorPressed();

	// CWnd仮想関数のオーバーライド。派生クラスがさらにオーバーライドする場合は、
	// 自分で処理しないメッセージについて必ず基底クラス版を呼ぶこと
	virtual LRESULT DispatchEvent( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnCreate( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnDestroy( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnSize( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnMove( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnCommand( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual LRESULT OnDrawItem( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	virtual void AfterCreateWindow();	//!< CWnd::Create()内で呼ばれる。既定のSW_SHOW即時表示を抑止する(表示はCreateBorderlessWindow()がSW_SHOWNOACTIVATEで行う)

private:
	void ComputeBottomRightPosition( HWND hwndParent, int nWidth, int nHeight, int& x, int& y ) const;	//!< 親ウィンドウ矩形からこのウィンドウの右下配置(20pxマージン)のx,yを計算する。初回表示の既定位置にのみ使う。hwndParentは呼び出し側から明示的に渡す(CreateBorderlessWindow()内での初回呼び出し時点ではGetParentHwnd()=m_hwndParentがまだCWnd::Create()によって設定されておらずNULLのため、GetParentHwnd()を内部で読むと使えない) 20260908
	void UpdateOffsetFromCurrentPosition();	//!< 現在のこのウィンドウの位置と親ウィンドウの位置の差をm_nOffsetX/Yへ記録する
	LRESULT OnEraseBkgnd( HDC hdc );	//!< 背景+タイトル帯(色+文字)の描画
	LRESULT HitTest( POINT ptScreen ) const;	//!< WM_NCHITTESTのカスタム判定(外周はリサイズ、タイトル帯はHTCAPTION)
	LRESULT OnMouseActivate();	//!< クリックでアクティブ化されないようにしつつ、ボタン類の最初のクリックが握りつぶされる問題を回避する

	HWND	m_hwndCloseBtn;
	int		m_nCloseBtnCtrlId;
	HFONT	m_hFontMain;
	int		m_nTitleBarHeight;
	int		m_nCloseBtnWidth;
	int		m_nWidth;		//!< 直近のウィンドウ幅(px)。閉じるたびに記憶し、次回表示時の既定値にする(-1=未記憶)
	int		m_nHeight;		//!< 直近のウィンドウ高さ(px)。同上
	int		m_nOffsetX;		//!< 親ウィンドウ左上からのこのウィンドウの相対オフセット(px)。FollowParentWindow()の追従に使う(表示中のみ有効)
	int		m_nOffsetY;		//!< 同上(Y方向)
	bool	m_bParentWasMinimized;	//!< FollowParentWindow()が最小化⇔復元の遷移を検出するための直前状態
	//! CreateBorderlessWindow()直後はtrue。次にFollowParentWindow()が呼ばれた時、
	//! (通常のオフセット追従ではなく)ComputeBottomRightPosition()を今の親矩形で
	//! 取り直して1回だけ位置を補正し直す。起動直後の「前回表示状態の自動復元」経路
	//! (CEditWnd::Create()の子ウィンドウ生成中)ではメインウィンドウ自身がまだ最終的な
	//! 位置・大きさになっていないことがあり、その時点のComputeBottomRightPosition()の
	//! 結果を鵜呑みにしたoffsetのまま追従を続けると、親が本当の位置に落ち着いた後も
	//! ずれた位置に留まり続けてしまうため 20260908
	bool	m_bNeedsInitialResnap;
};

#endif /* _CBORDERLESSWND_H_ */
