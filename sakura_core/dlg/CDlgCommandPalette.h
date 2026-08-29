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
#include "extmodule/CUxTheme.h"
#include "util/CSlideInAnimator.h"

#include <vector>
#include <string>
#include <map>

class CFuncLookup;
class CEditView;

/*!
	@brief コマンドパレット(Shift+Ctrl+P)

	全コマンド(コマンドID・機能名・ショートカットキー)と、現在開いている
	ファイルの一覧をひとつの絞り込みリストにまとめ、Enterで実行/切り替えする。
	「@」接頭辞で、現在編集中の文書のアウトライン解析結果(関数名等)も
	同じ絞り込みリストから選んでジャンプできる 20260821
	「#」接頭辞で、現在編集中の文書のブックマーク一覧からも選んでジャンプできる 20260821
	アウトライン/ブックマークの選択行は、Enterで確定するまでもなく選択を
	動かすたびにライブでカーソル位置へ移動する(VSCodeのGo to Symbol風)。
	Escapeでキャンセルした場合はパレットを開く前の位置に戻す 20260821
*/
class CDlgCommandPalette : public CDialog
{
public:
	CDlgCommandPalette();

	HWND DoModeless( HINSTANCE hInstance, HWND hwndParent, CFuncLookup* pcFuncLookup, CEditView* pcView );

	void FollowParentWindow();	//!< 親ウィンドウ(エディタ)の移動・サイズ変更に追従して表示位置を更新する
	void LivePreviewSelection( int nItemIndex );	//!< 一覧の選択行がROWKIND_OUTLINE/BOOKMARKなら、確定前にライブでカーソルを移動する。フィルタ欄サブクラスプロシージャ側の矢印キー処理(MoveSelection)からも呼べるようpublic 20260821
	int GetSelectedDispIndex() const { return m_nSelectedDispIndex; }	//!< 現在選択中の表示行索引(-1=選択なし)。MoveSelection()がLVS_OWNERDATA下でのListView_GetNextItem()の不確実性を避けてこれを頼るためpublic 20260821
	int GetMatchedRowCount() const { return (int)m_vMatchedRowIndices.size(); }	//!< 絞り込み後の件数。右下の件数表示用にPaletteDlgSubclassProc(WM_PAINT)から呼べるようpublic 20260829

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

	CSlideInAnimator	m_cSlideAnimator;	//!< 上からのスライドインアニメーション(CDlgFindと共通) 20260829

	bool	m_bReactivateParentOnClose;	//!< CloseOnDeactivate()で親ウィンドウを前面に戻すかどうか

	HFONT	m_hFontList;	//!< 一覧の文字表示用(既定フォントより大きめ、通常太さ)。既定フォントから生成しOnDestroyで破棄する
	HFONT	m_hFontSub;		//!< 一覧のグレーのパス表示用(m_hFontListより小さい)。既定フォントから生成しOnDestroyで破棄する 20260821
	HTHEME	m_hThemeListView;	//!< 一覧の選択行背景を反転色ではなくエクスプローラー風の半透明選択色で描くための"Explorer::ListView"テーマ。OnInitDialogでOpenThemeData、OnDestroyでCloseThemeData 20260821
	std::wstring	m_sLastQuery;	//!< UpdateList()が直近に絞り込みへ使った実クエリ(先頭の">"等の接頭辞を除いたもの、小文字化済み)。OnListCustomDraw()が一致部分のハイライトに使う 20260821
	std::map<std::wstring, int>	m_mapExtToIconIndex;	//!< 拡張子(小文字)→共有システムアイコン一覧の索引

	//! 一覧1行の種別
	enum EPaletteRowKind {
		ROWKIND_COMMAND,	//!< コマンド(" > "で絞り込み)
		ROWKIND_WINDOW,		//!< 開いているウィンドウ("edt "で絞り込み)
		ROWKIND_RECENT,		//!< 最近使ったファイル(既定、絞り込み記号なし)
		ROWKIND_OUTLINE,	//!< アウトライン解析結果("@"で絞り込み) 20260821
		ROWKIND_BOOKMARK,	//!< ブックマーク一覧("#"で絞り込み) 20260821
	};

	//! 一覧1行分
	struct PaletteRow {
		EPaletteRowKind	kind;
		int				nPostId;	//!< kind==ROWKIND_COMMAND/RECENTのとき、WM_COMMANDで送るID(機能番号 または IDM_SELMRU+i)。kind==ROWKIND_OUTLINE/BOOKMARKのとき、ジャンプ先の行番号(1開始) 20260821
		int				nOutlineCol;	//!< kind==ROWKIND_OUTLINE/BOOKMARKのとき、ジャンプ先の桁番号(1開始) 20260821
		HWND			hwndFile;	//!< kind==ROWKIND_WINDOWのとき、切り替え先ウィンドウ
		std::wstring	sName;		//!< 表示名(絞り込みの対象にもなる)
		std::wstring	sType;		//!< 種別表示("コマンド"/"ウィンドウ"/"最近使ったファイル"/"アウトライン"/"ブックマーク")
		std::wstring	sId;		//!< コマンドIDの表示文字列(コマンド行以外は空)
		std::wstring	sSub;		//!< コマンドのショートカットキー、またはファイルのフルパス
#ifdef NKMM_COMMAND_PALETTE_ROMAJI
		std::string	sFuzzyTarget;	//!< PrecomputeFuzzyMatchTarget(sName)の結果。行を積んだ時点(BuildAllRows/
									//!< BuildOutlineRows/BuildBookmarkRows)で1回だけ計算しておき、UpdateList()の
									//!< 絞り込みループから毎回FuzzyMatchJapaneseCached()へ渡す。sNameの正規化・
									//!< 漢字読み展開はクエリに依存せず行の内容だけで決まるため、キー入力のたびに
									//!< 全行分再計算すると数万行規模で数秒かかっていた不具合の対策 20260829
#endif // NKMM_COMMAND_PALETTE_ROMAJI
	};

	void BuildAllRows();
	void BuildOutlineRows();	//!< 「@」モード初回選択時にだけ現在の文書のアウトライン解析を行い、結果をm_vAllRowsへ追加する 20260821
	void BuildBookmarkRows();	//!< 「#」モード初回選択時にだけ現在の文書のブックマーク一覧を取得し、結果をm_vAllRowsへ追加する 20260821
	void UpdateList();
	void ExecuteRow( const PaletteRow& row );
	void ExecuteSelected();
	void MoveCaretTo( int nLogicX, int nLogicY );	//!< 現在の文書内でカーソルを指定位置(0開始)へ移動する(選択状態は保持) 20260821
	void JumpToRow( const PaletteRow& row );	//!< kind==ROWKIND_OUTLINE/BOOKMARKの行が指す位置へMoveCaretTo()する 20260821
	void AdjustListHeight();	//!< 絞り込み結果がスクロールを要しない件数のときは、一覧とダイアログの高さをその件数ぶんまで狭める(結果が増えれば元の最大高さまで戻る) 20260821

	CFuncLookup*				m_pcFuncLookup;
	CEditView*					m_pcView;			//!< アウトライン解析・ブックマーク取得・ジャンプ先の対象ビュー 20260821
	bool						m_bOutlineRowsBuilt;	//!< BuildOutlineRows()を実行済みか 20260821
	bool						m_bBookmarkRowsBuilt;	//!< BuildBookmarkRows()を実行済みか 20260821
	int							m_nOrigCaretX;		//!< パレットを開いた時点のカーソル位置(桁、0開始)。Escapeでのキャンセル時に戻す 20260821
	int							m_nOrigCaretY;		//!< パレットを開いた時点のカーソル位置(行、0開始)。Escapeでのキャンセル時に戻す 20260821
	std::vector<PaletteRow>	m_vAllRows;
	//! 一覧(LVS_OWNERDATA)の表示行索引→m_vAllRows添字。UpdateList()が絞り込むたびに
	//! 作り直し、以後選択行の解決(OnListCustomDraw/LivePreviewSelection/ExecuteSelected)は
	//! すべてこれ経由で行う。LVS_OWNERDATAの一覧は行ごとにlParamを保持できない
	//! (アプリ側がインデックスだけを頼りに毎回引き当てる)ため 20260821
	std::vector<size_t>			m_vMatchedRowIndices;
	//! 現在選択中の表示行索引(-1=選択なし)。LVS_OWNERDATA化後、OnListCustomDraw()の
	//! 描画時にListView_GetItemState()で選択状態を問い合わせると常に「選択されている」
	//! 扱いになってしまい全行が選択色で塗られる不具合が出たため(旧来のnmcd.uItemStateが
	//! 全行で立ってしまう不具合と同種、LVS_OWNERDATAではGetItemStateの方も同様に信用でき
	//! ない)、選択行はcomctl32に問い合わせず全経路(UpdateList/MoveSelection/
	//! LVN_ITEMCHANGED)でLivePreviewSelection()を通じて自前追跡する 20260821
	int							m_nSelectedDispIndex;

	//! AdjustListHeight()用。いずれもOnInitDialog()でダイアログテンプレート設計値
	//! (sakura_rc.rcのIDC_LIST_COMMANDPALETTEの高さ等)から一度だけ求めて以後固定 20260821
	int							m_nMaxListHeight;	//!< 一覧の最大高さ(px、スクロール開始する高さ)
	int							m_nChromeHeight;	//!< ダイアログ全体の高さのうち一覧を除いた分(px、フィルタ欄+余白)
	int							m_nListRowHeight;	//!< 一覧1行の実高さ(px)。初めて1件以上表示された時点でキャッシュ(0=未確定)
	int							m_nListBorderHeight;	//!< 一覧のウィンドウ矩形とクライアント矩形の高さの差(px、WS_BORDER分)。AdjustListHeight()が行数から求めた高さをウィンドウ矩形の高さへ変換する補正に使う 20260822
};

#endif // NKMM_COMMAND_PALETTE

#endif /* SAKURA_CDLGCOMMANDPALETTE_20260818_H_ */
