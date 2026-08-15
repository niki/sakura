/*!	@file
	@brief 共通タブバー(社内呼称:方式A) Step 0: 単体生成検証用の浮遊タブウィンドウ

	@date 2026.08.15 新規作成
*/
#ifndef SAKURA_WINDOW_CUNIFIEDTABWND_H_
#define SAKURA_WINDOW_CUNIFIEDTABWND_H_

#include "CWnd.h"
#include "util/design_template.h"

struct EditNode;

//! 共通タブバー(社内呼称:方式A)候補ウィンドウ
/*!
	各エディタプロセスが個別に持つCTabWndとは別に、コントロールプロセスが
	1つだけ生成する浮遊ウィンドウ。Step 0で単体生成・実データ表示を確認し、
	Step 1でタブクリックでの実切替(ActivateFrameWindow)・フォアグラウンドの
	エディタウィンドウへの位置追従・各プロセスの個別タブバー無効化に対応、
	Step 2でタブのドラッグ並び替え、Step 3でアイコン表示・未保存マークに対応した。
	グループ分離・閉じるボタンは未実装。
	詳細はchangelog/NKMM_UNIFIED_TABBAR.md、my_config.hのNKMM_UNIFIED_TABBAR参照。
*/
class CUnifiedTabWnd : public CWnd
{
public:
	CUnifiedTabWnd();
	virtual ~CUnifiedTabWnd();

	HWND Open( HINSTANCE, HWND hwndOwner );	/*!< ウィンドウ オープン */
	void Close( void );							/*!< ウィンドウ クローズ */
	void Refresh( void );						/*!< 共有メモリの内容をタブへ反映(選択状態はm_hwndTrackedActiveに追従) */

protected:
	virtual void AfterCreateWindow( void );			/*!< ウィンドウ作成後の処理(自動表示を止める) */
	virtual LRESULT OnNotify( HWND, UINT, WPARAM, LPARAM );	/*!< WM_NOTIFY処理(TCN_SELCHANGEを検知し、実際の切替はPostMessageで遅延実行する) */
	virtual LRESULT OnTimer( HWND, UINT, WPARAM, LPARAM );	/*!< WM_TIMER処理(位置追従・タブ更新) */
	virtual LRESULT OnSize( HWND, UINT, WPARAM, LPARAM );		/*!< WM_SIZE処理(子のタブコントロールを追従リサイズ) */
	virtual LRESULT DispatchEvent_WM_APP( HWND, UINT, WPARAM, LPARAM );	/*!< タブクリック後の実切替をここで遅延実行する */

private:
	void Tick( void );		/*!< 位置追従＋タブ再構築の共通処理(初回Open()時とWM_TIMER時の両方から呼ぶ) */

private:
	/* Step 2: タブのドラッグ並び替え(タブコントロールのサブクラス化) */
	static LRESULT CALLBACK TabSubclassProc( HWND, UINT, WPARAM, LPARAM );
	LRESULT OnTabLButtonDown( WPARAM, LPARAM );
	LRESULT OnTabLButtonUp( WPARAM, LPARAM );
	LRESULT OnTabMouseMove( WPARAM, LPARAM );
	bool ReorderTabByIndex( int nSrcTab, int nDstTab );	/*!< CTabWnd::ReorderTabと同じ薄いラッパー */

	/* Step 3: アイコン表示。CTabWnd::GetImageIndexと違い、システムの共有
	   イメージリストをそのまま使う簡易版(複製しないのでImageList_Destroyは呼ばない) */
	int GetIconIndex( EditNode* pNode );

private:
	HWND	m_hwndTab;				/*!< タブコントロール */
	HFONT	m_hFont;				/*!< 表示用フォント */
	HWND	m_hwndTrackedActive;	/*!< 追従中のエディタウィンドウ */
	std::tstring	m_strLastSignature;	/*!< 直近のタブ内容の署名(変化が無ければ全再構築を省略する) */

	/* Step 2: ドラッグ状態 */
	WNDPROC	m_pOldTabWndProc;		/*!< タブコントロールの元のWndProc */
	bool	m_bDragCheck;			/*!< ボタン押下～しきい値判定待ち */
	bool	m_bDragging;			/*!< しきい値を超えてドラッグ確定後 */
	int		m_nSrcTab;				/*!< ドラッグ元タブ番号 */
	POINT	m_ptSrcCursor;			/*!< ドラッグ開始時のカーソル位置 */

private:
	DISALLOW_COPY_AND_ASSIGN(CUnifiedTabWnd);
};

#endif /* SAKURA_WINDOW_CUNIFIEDTABWND_H_ */
