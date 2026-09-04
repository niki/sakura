/*!	@file
	@brief 文書ウィンドウの管理

	@author Norio Nakatani
	@date	1998/03/13 作成
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000-2001, genta, jepro
	Copyright (C) 2001, asa-o, MIK, hor, Misaka, Stonee, YAZAKI
	Copyright (C) 2002, genta, hor, YAZAKI, Azumaiya, KK, novice, minfu, ai, aroka, MIK
	Copyright (C) 2003, genta, MIK, Moca
	Copyright (C) 2004, genta, Moca, novice, Kazika, isearch
	Copyright (C) 2005, genta, Moca, MIK, ryoji, maru
	Copyright (C) 2006, genta, aroka, fon, yukihane, ryoji
	Copyright (C) 2007, ryoji, maru
	Copyright (C) 2008, ryoji
	Copyright (C) 2009, nasukoji

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

#ifndef _CEDITVIEW_H_
#define _CEDITVIEW_H_

#include <Windows.h>
#include <ObjIdl.h>  // LPDATAOBJECT
#include <ShellAPI.h>  // HDROP
#include <atomic>  // ScrBarMarker: PTP_WORKワーカーとUIスレッド間のフラグ共有用
#include <mutex>   // ScrBarMarker: bBuildThreadRunning_/bRebuildPending_の複合操作を保護
#include "CTextMetrics.h"
#include "CTextDrawer.h"
#include "CTextArea.h"
#include "CColorGlyphCell.h"	// カラーフォント(絵文字等)描画キュー用POD
#include "CCaret.h"
#include "CViewCalc.h" // parent
#include "CEditView_Paint.h"	// parent
#include "CViewParser.h"
#include "CViewSelect.h"
#include "CSearchAgent.h"
#include "view/colors/EColorIndexType.h"
#include "window/CTipWnd.h"
#include "window/CAutoScrollWnd.h"
#include "CDicMgr.h"
//	Jun. 26, 2001 genta	正規表現ライブラリの差し替え
#include "extmodule/CBregexp.h"
#include "CEol.h"				// EEolType
#include "cmd/CViewCommander.h"
#include "mfclike/CMyWnd.h"		// parent
#include "doc/CDocListener.h"	// parent
#include "basis/SakuraBasis.h"	// CLogicInt, CLayoutInt
#include "util/container.h"		// vector_ex
#include "util/design_template.h"
#ifdef NKMM_FIX_EDITVIEW_SCRBAR
//#include <mutex>
#include <process.h>
#endif // NKMM_

class CViewFont;
class CRuler;
class CDropTarget; /// 2002/2/3 aroka ヘッダ軽量化
class COpeBlk;///
class CSplitBoxWnd;///
class CRegexKeyword;///
class CAutoMarkMgr; /// 2002/2/3 aroka ヘッダ軽量化 to here
class CEditDoc;	//	2002/5/13 YAZAKI ヘッダ軽量化
class CLayout;	//	2002/5/13 YAZAKI ヘッダ軽量化
class CMigemo;	// 2004.09.14 isearch
struct SColorStrategyInfo;
struct CColor3Setting;
class COutputAdapter;

// struct DispPos; //	誰かがincludeしてます
// class CColorStrategy;	// 誰かがincludeしてます
class CColor_Found;

#ifndef IDM_COPYDICINFO
#define IDM_COPYDICINFO 2000
#endif
#ifndef IDM_JUMPDICT
#define IDM_JUMPDICT 2001	// 2006.04.10 fon
#endif

#if !defined(RECONVERTSTRING) && (WINVER < 0x040A)
typedef struct tagRECONVERTSTRING {
    DWORD dwSize;
    DWORD dwVersion;
    DWORD dwStrLen;
    DWORD dwStrOffset;
    DWORD dwCompStrLen;
    DWORD dwCompStrOffset;
    DWORD dwTargetStrLen;
    DWORD dwTargetStrOffset;
} RECONVERTSTRING, *PRECONVERTSTRING;
#endif // RECONVERTSTRING

///	マウスからコマンドが実行された場合の上位ビット
///	@date 2006.05.19 genta
const int CMD_FROM_MOUSE = 2;


/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
/*!
	@brief 文書ウィンドウの管理
	
	1つの文書ウィンドウにつき1つのCEditDocオブジェクトが割り当てられ、
	1つのCEditDocオブジェクトにつき、4つのCEditViweオブジェクトが割り当てられる。
	ウィンドウメッセージの処理、コマンドメッセージの処理、
	画面表示などを行う。
	
	@date 2002.2.17 YAZAKI CShareDataのインスタンスは、CProcessにひとつあるのみ。
*/
//2007.08.25 kobake 文字間隔配列の機能をCTextMetricsに移動
//2007.10.02 kobake Command_TRIM2をCConvertに移動

class CEditView
: public CViewCalc //$$ これが親クラスである必要は無いが、このクラスのメソッド呼び出しが多いので、暫定的に親クラスとする。
, public CEditView_Paint
, public CMyWnd
, public CDocListenerEx
{
public:
	const CEditDoc* GetDocument() const
	{
		return m_pcEditDoc;
	}
	CEditDoc* GetDocument()
	{
		return m_pcEditDoc;
	}
public:
	//! 背景にビットマップを使用するかどうか
	//! 2010.10.03 背景実装
	bool IsBkBitmap() const{ return NULL != m_pcEditDoc->m_hBackImg; }

public:
	CEditView* GetEditView()
	{
		return this;
	}
	const CEditView* GetEditView() const
	{
		return this;
	}


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        生成と破棄                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	/* Constructors */
	CEditView(CEditWnd* pcEditWnd);
	~CEditView();
	void Close();
	/* 初期化系メンバ関数 */
	BOOL Create(
		HWND		hwndParent,	//!< 親
		CEditDoc*	pcEditDoc,	//!< 参照するドキュメント
		int			nMyIndex,	//!< ビューのインデックス
		BOOL		bShow		//!< 作成時に表示するかどうか
	);
	void CopyViewStatus( CEditView* ) const;					/* 自分の表示状態を他のビューにコピー */

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      クリップボード                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//取得
	bool MyGetClipboardData( CNativeW&, bool*, bool* = NULL );			/* クリップボードからデータを取得 */

	//設定
	bool MySetClipboardData( const ACHAR*, int, bool bColumnSelect, bool = false );	/* クリップボードにデータを設定 */
	bool MySetClipboardData( const WCHAR*, int, bool bColumnSelect, bool = false );	/* クリップボードにデータを設定 */

	//利用
	void CopyCurLine( bool bAddCRLFWhenCopy, EEolType neweol, bool bEnableLineModePaste );	/* カーソル行をクリップボードにコピーする */	// 2007.10.08 ryoji
	void CopySelectedAllLines( const wchar_t*, BOOL );			/* 選択範囲内の全行をクリップボードにコピーする */


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         イベント                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//ドキュメントイベント
	void OnAfterLoad(const SLoadInfo& sLoadInfo);
	/* メッセージディスパッチャ */
	LRESULT DispatchEvent( HWND, UINT, WPARAM, LPARAM );
	//
	void OnChangeSetting();										/* 設定変更を反映させる */
	void OnPaint( HDC, PAINTSTRUCT *, BOOL );			/* 通常の描画処理 */
	void OnPaint2( HDC, PAINTSTRUCT *, BOOL );			/* 通常の描画処理 */
	void DrawBackImage(HDC hdc, RECT& rcPaint, HDC hdcBgImg);
	void OnTimer( HWND, UINT, UINT_PTR, DWORD );
	//ウィンドウ
	void OnSize( int, int );							/* ウィンドウサイズの変更処理 */
	void OnMove( int, int, int, int );
	//フォーカス
	void OnSetFocus( void );
	void OnKillFocus( void );
	//スクロール
	CLayoutInt  OnVScroll( int, int );							/* 垂直スクロールバーメッセージ処理 */
	CLayoutInt  OnHScroll( int, int );							/* 水平スクロールバーメッセージ処理 */
	//マウス
	void OnLBUTTONDOWN( WPARAM, int, int );				/* マウス左ボタン押下 */
	void OnMOUSEMOVE( WPARAM, int, int );				/* マウス移動のメッセージ処理 */
	void OnLBUTTONUP( WPARAM, int, int );				/* マウス左ボタン開放のメッセージ処理 */
	void OnLBUTTONDBLCLK( WPARAM, int , int );			/* マウス左ボタンダブルクリック */
	void OnRBUTTONDOWN( WPARAM, int, int );				/* マウス右ボタン押下 */
	void OnRBUTTONUP( WPARAM, int, int );				/* マウス右ボタン開放 */
	void OnMBUTTONDOWN( WPARAM, int, int );				/* マウス中ボタン押下 */
	void OnMBUTTONUP( WPARAM, int, int );				/* マウス中ボタン開放 */
	void OnXLBUTTONDOWN( WPARAM, int, int );			/* マウスサイドボタン1押下 */
	void OnXLBUTTONUP( WPARAM, int, int );				/* マウスサイドボタン1開放 */		// 2009.01.17 nasukoji
	void OnXRBUTTONDOWN( WPARAM, int, int );			/* マウスサイドボタン2押下 */
	void OnXRBUTTONUP( WPARAM, int, int );				/* マウスサイドボタン2開放 */		// 2009.01.17 nasukoji
	LRESULT OnMOUSEWHEEL( WPARAM, LPARAM );				//!< 垂直マウスホイールのメッセージ処理
	LRESULT OnMOUSEHWHEEL( WPARAM, LPARAM );			//!< 水平マウスホイールのメッセージ処理
	LRESULT OnMOUSEWHEEL2( WPARAM, LPARAM, bool, EFunctionCode );		//!< マウスホイールのメッセージ処理
	bool IsSpecialScrollMode( int );					/* キー・マウスボタン状態よりスクロールモードを判定する */		// 2009.01.17 nasukoji

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           描画                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	// 2006.05.14 Moca  互換BMPによる画面バッファ
	// 2007.09.30 genta CompatibleDC操作関数
protected:
	//! ロジック行を1行描画
	bool DrawLogicLine(
		HDC				hdc,			//!< [in]     作画対象
		DispPos*		pDispPos,		//!< [in,out] 描画する箇所、描画元ソース
		CLayoutInt		nLineTo			//!< [in]     作画終了するレイアウト行番号
	);

	//! レイアウト行を1行描画
	bool DrawLayoutLine(SColorStrategyInfo* pInfo);

#ifdef NKMM_FIX_COLOR_FONT
	//カラーフォント(絵文字等)描画
public:
	//! GDIでの描画直後に、このグリフがカラーフォントのグリフなら描画待ちキューに積む。
	//! 実際の描画はまだ行わない(GDIが描画したフォールバック用グリフはそのまま残る)。
	//! @param bForceEmojiPresentation
	//!                     [in] このグリフの直後にVS16(U+FE0F)が続くか。trueなら、本文
	//!                          フォント自身がモノクロの持ち駒を持っていてもそれを無視し、
	//!                          カラー(絵文字)フォントへの解決を強制する。
	void TryQueueColorGlyph(HFONT hFont, const wchar_t* pData, int nLength, const RECT& rcCell, int nBaselineTopOffset, COLORREF crFore, COLORREF crBack, bool bForceEmojiPresentation);
	//! 1visual行分たまった描画待ちキューを、その行の全GDI描画が終わった後にまとめて描画する。
	void FlushColorGlyphQueue(CGraphics& gr);
private:
	std::vector<SColorGlyphCell> m_vPendingColorGlyphs;

	//! ZWJ合字クラスタ化用の一時バッファ・確定処理。詳細はCEditView_ColorFont.cpp参照。
	std::vector<SPendingGlyphCall> m_vPendingClusterCalls;
	void FlushPendingCluster();
#endif // NKMM_

	//色分け
public:
	CColor3Setting GetColorIndex( const CLayout* pcLayout, CLayoutYInt nLineNum, int nIndex, SColorStrategyInfo* pInfo, bool bPrev = false );	/* 指定位置のColorIndexの取得 02/12/13 ai */
	void SetCurrentColor( CGraphics& gr, EColorIndexType, EColorIndexType, EColorIndexType);
	COLORREF GetTextColorByColorInfo2(const ColorInfo& info, const ColorInfo& info2);
	COLORREF GetBackColorByColorInfo2(const ColorInfo& info, const ColorInfo& info2);

	//画面バッファ
protected:
	bool CreateOrUpdateCompatibleBitmap( int cx, int cy );	//!< メモリBMPを作成または更新
	void UseCompatibleDC(BOOL fCache);
public:
	void DeleteCompatibleBitmap();							//!< メモリBMPを削除

public:
	void DispTextSelected( HDC hdc, CLayoutInt nLineNum, const CMyPoint& ptXY, CLayoutInt nX_Layout );	/* テキスト反転 */
	void RedrawAll();											/* フォーカス移動時の再描画 */
	void Redraw();										// 2001/06/21 asa-o 再描画
	void RedrawLines( CLayoutYInt top, CLayoutYInt bottom );
	void CaretUnderLineON( bool, bool, bool );						/* カーソル行アンダーラインのON */
	void CaretUnderLineOFF( bool, bool, bool, bool );				/* カーソル行アンダーラインのOFF */
	bool GetDrawSwitch() const
	{
		return m_bDrawSWITCH;
	}
	bool SetDrawSwitch(bool b)
	{
		bool bOld = m_bDrawSWITCH;
		m_bDrawSWITCH = b;
		return bOld;
	}
	bool IsDrawCursorVLinePos( int );
	void DrawBracketCursorLine( bool );



	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        スクロール                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	void AdjustScrollBars();											/* スクロールバーの状態を更新する */
	void RepositionControlsForSize( int cx, int cy );					/* スクロールバー等の位置とテキスト表示領域を再計算する */
	BOOL CreateScrollBar();												/* スクロールバー作成 */	// 2006.12.19 ryoji
	void DestroyScrollBar();											/* スクロールバー破棄 */	// 2006.12.19 ryoji
	CLayoutInt GetWrapOverhang( void ) const;							/* 折り返し桁以後のぶら下げ余白計算 */	// 2008.06.08 ryoji
	CKetaXInt ViewColNumToWrapColNum( CLayoutXInt nViewColNum ) const;	/* 「右端で折り返す」用にビューの桁数から折り返し桁数を計算する */	// 2008.06.08 ryoji
	CLayoutInt GetRightEdgeForScrollBar( void );								/* スクロールバー制御用に右端座標を取得する */		// 2009.08.28 nasukoji

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           IME                               //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//	Aug. 25, 2002 genta protected->publicに移動
	bool IsImeON( void );	// IME ONか	// 2006.12.04 ryoji
	

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        スクロール                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	CLayoutInt  ScrollAtV( CLayoutInt );										/* 指定上端行位置へスクロール */
	CLayoutInt  ScrollAtH( CLayoutInt );										/* 指定左端桁位置へスクロール */
	//	From Here Sep. 11, 2004 genta ずれ維持の同期スクロール
	CLayoutInt  ScrollByV( CLayoutInt vl ){	return ScrollAtV( GetTextArea().GetViewTopLine() + vl );}	/* 指定行スクロール*/
	CLayoutInt  ScrollByH( CLayoutInt hl ){	return ScrollAtH( GetTextArea().GetViewLeftCol() + hl );}	/* 指定桁スクロール */
	void ScrollDraw(CLayoutInt, CLayoutInt, const RECT&, const RECT&, const RECT&);
public:
	void SyncScrollV( CLayoutInt );									/* 垂直同期スクロール */
	void SyncScrollH( CLayoutInt );									/* 水平同期スクロール */

	void SetBracketPairPos( bool );								/* 対括弧の強調表示位置設定 03/02/18 ai */

	void AutoScrollEnter();
	void AutoScrollExit();
	void AutoScrollMove( CMyPoint& point );
	void AutoScrollOnTimer();

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        過去の遺産                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	void SetIMECompFormPos( void );								/* IME編集エリアの位置を変更 */
	void SetIMECompFormFont( void );							/* IME編集エリアの表示フォントを変更 */


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                       テキスト選択                          //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	// 2002/01/19 novice public属性に変更
	bool GetSelectedDataSimple( CNativeW& );// 選択範囲のデータを取得
	bool GetSelectedDataOne( CNativeW& cmemBuf, int nMaxLen );
	bool GetSelectedData( CNativeW*, BOOL, const wchar_t*, BOOL, bool bAddCRLFWhenCopy, EEolType neweol = EOL_UNKNOWN);/* 選択範囲のデータを取得 */
	int IsCurrentPositionSelected( CLayoutPoint ptCaretPos );					/* 指定カーソル位置が選択エリア内にあるか */
	int IsCurrentPositionSelectedTEST( const CLayoutPoint& ptCaretPos, const CLayoutRange& sSelect ) const;/* 指定カーソル位置が選択エリア内にあるか */
	// 2006.07.09 genta 行桁指定によるカーソル移動(選択領域を考慮)
	void MoveCursorSelecting( CLayoutPoint ptWk_CaretPos, bool bSelect, int = _CARETMARGINRATE );
	void ConvSelectedArea( EFunctionCode );								/* 選択エリアのテキストを指定方法で変換 */
	//!指定位置または指定範囲がテキストの存在しないエリアかチェックする		// 2008.08.03 nasukoji
	bool IsEmptyArea( CLayoutPoint ptFrom, CLayoutPoint ptTo = CLayoutPoint( CLayoutInt(-1), CLayoutInt(-1) ), bool bSelect = false, bool bBoxSelect = false ) const;

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         各種判定                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	bool IsCurrentPositionURL( const CLayoutPoint& ptCaretPos, CLogicRange* pUrlRange, std::wstring* pwstrURL );/* カーソル位置にURLが有る場合のその範囲を調べる */
	BOOL CheckTripleClick( CMyPoint ptMouse );							/* トリプルクリックをチェックする */	// 2007.10.02 nasukoji



	bool ExecCmd(const TCHAR*, int, const TCHAR*, COutputAdapter* = NULL ) ;							// 子プロセスの標準出力をリダイレクトする
	void AddToCmdArr( const TCHAR* );
	BOOL ChangeCurRegexp(bool bRedrawIfChanged= true);									// 2002.01.16 hor 正規表現の検索パターンを必要に応じて更新する(ライブラリが使用できないときはFALSEを返す)
	void SendStatusMessage( const TCHAR* msg );					// 2002.01.26 hor 検索／置換／ブックマーク検索時の状態をステータスバーに表示する
	LRESULT SetReconvertStruct(PRECONVERTSTRING pReconv, bool bUnicode, bool bDocumentFeed = false);	/* 再変換用構造体を設定する 2002.04.09 minfu */
	LRESULT SetSelectionFromReonvert(const PRECONVERTSTRING pReconv, bool bUnicode);				/* 再変換用構造体の情報を元に選択範囲を変更する 2002.04.09 minfu */

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           D&D                               //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public: /* テスト用にアクセス属性を変更 */
	/* IDropTarget実装 */
	STDMETHODIMP DragEnter( LPDATAOBJECT, DWORD, POINTL, LPDWORD );
	STDMETHODIMP DragOver(DWORD, POINTL, LPDWORD );
	STDMETHODIMP DragLeave( void );
	STDMETHODIMP Drop( LPDATAOBJECT, DWORD, POINTL, LPDWORD );
	STDMETHODIMP PostMyDropFiles( LPDATAOBJECT pDataObject );		/* 独自ドロップファイルメッセージをポストする */	// 2008.06.20 ryoji
	void OnMyDropFiles( HDROP hDrop );								/* 独自ドロップファイルメッセージ処理 */	// 2008.06.20 ryoji
	CLIPFORMAT GetAvailableClipFormat( LPDATAOBJECT pDataObject );
	DWORD TranslateDropEffect( CLIPFORMAT cf, DWORD dwKeyState, POINTL pt, DWORD dwEffect );
	bool IsDragSource( void );

	void _SetDragMode(BOOL b)
	{
		m_bDragMode = b;
	}


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           編集                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	/* 指定位置の指定長データ削除 */
	void DeleteData2(
		const CLayoutPoint&	ptCaretPos,
		CLogicInt			nDelLen,
		CNativeW*			pcMem
	);

	/* 現在位置のデータ削除 */
	void DeleteData( bool bRedraw );

	/* 現在位置にデータを挿入 */
	void InsertData_CEditView(
		CLayoutPoint	ptInsertPos,
		const wchar_t*	pData,
		int				nDataLen,
		CLayoutPoint*	pptNewPos,	//挿入された部分の次の位置のデータ位置
		bool			bRedraw
	);

	/* データ置換 削除&挿入にも使える */
	void ReplaceData_CEditView(
		const CLayoutRange&	sDelRange,			// 削除範囲。レイアウト単位。
		const wchar_t*		pInsData,			// 挿入するデータ
		CLogicInt			nInsDataLen,		// 挿入するデータの長さ
		bool				bRedraw,
		COpeBlk*			pcOpeBlk,
		bool				bFastMode = false,
		const CLogicRange*	psDelRangeLogicFast = NULL,
		bool				bHadSelection = false	//!< 20260831 呼び出し前が選択状態だったか(NKMM_UNDO_RESTORE_SELECTION用)
	);
	void ReplaceData_CEditView2(
		const CLogicRange&	sDelRange,			// 削除範囲。ロジック単位。
		const wchar_t*		pInsData,			// 挿入するデータ
		CLogicInt			nInsDataLen,		// 挿入するデータの長さ
		bool				bRedraw,
		COpeBlk*			pcOpeBlk,
		bool				bFastMode = false,
		bool				bHadSelection = false	//!< 20260831 呼び出し前が選択状態だったか(NKMM_UNDO_RESTORE_SELECTION用)
	);
	bool ReplaceData_CEditView3(
		CLayoutRange	sDelRange,			// 削除範囲。レイアウト単位。
		COpeLineData*	pcmemCopyOfDeleted,	// 削除されたデータのコピー(NULL可能)
		COpeLineData*	pInsData,
		bool			bRedraw,
		COpeBlk*		pcOpeBlk,
		int				nDelSeq,
		int*			pnInsSeq,
		bool			bFastMode = false,
		const CLogicRange*	psDelRangeLogicFast = NULL,
		bool			bHadSelection = false	//!< 20260831 呼び出し前が選択状態だったか(NKMM_UNDO_RESTORE_SELECTION用)
	);
	void RTrimPrevLine( void );		/* 2005.10.11 ryoji 前の行にある末尾の空白を削除 */

	//	Oct. 2, 2005 genta 挿入モードの設定・取得
	bool IsInsMode() const;
	void SetInsMode(bool);

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           検索                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//2004.10.13 インクリメンタルサーチ関係
	void TranslateCommand_isearch( EFunctionCode&, bool&, LPARAM&, LPARAM&, LPARAM&, LPARAM& );
	bool ProcessCommand_isearch( int, bool, LPARAM, LPARAM, LPARAM, LPARAM );

	//	Jan. 10, 2005 genta HandleCommandからgrep関連処理を分離
	void TranslateCommand_grep( EFunctionCode&, bool&, LPARAM&, LPARAM&, LPARAM&, LPARAM& );

	//	Jan. 10, 2005 インクリメンタルサーチ
	bool IsISearchEnabled(int nCommand) const;

	BOOL KeySearchCore( const CNativeW* pcmemCurText );	// 2006.04.10 fon

	/*!	CEditView::KeyWordHelpSearchDictのコール元指定用ローカルID
		@date 2006.04.10 fon 新規作成
	*/
	enum LID_SKH {
		LID_SKH_ONTIMER		= 1,	/*!< CEditView::OnTimer */
		LID_SKH_POPUPMENU_R = 2,	/*!< CEditView::CreatePopUpMenu_R */
	};
	BOOL KeyWordHelpSearchDict( LID_SKH nID, POINT* po, RECT* rc );	// 2006.04.10 fon

	int IsSearchString( const CStringRef& cStr, CLogicInt, CLogicInt*, CLogicInt* ) const;	/* 現在位置が検索文字列に該当するか */	//2002.02.08 hor 引数追加

#ifdef NKMM_FIX_SEARCH_KEY_REGEXP_AUTO_QUOTE
	void GetCurrentTextForSearch( CNativeW&, bool bStripMaxPath = true, bool bTrimSpaceTab = false, bool bRegQuote = false );			/* 現在カーソル位置単語または選択範囲より検索等のキーを取得 */
	bool GetCurrentTextForSearchDlg( CNativeW&, bool bGetHistory = false, bool bRegQuote = false );		/* 現在カーソル位置単語または選択範囲より検索等のキーを取得（ダイアログ用） 2006.08.23 ryoji */
#else
	void GetCurrentTextForSearch( CNativeW&, bool bStripMaxPath = true, bool bTrimSpaceTab = false );			/* 現在カーソル位置単語または選択範囲より検索等のキーを取得 */
	bool GetCurrentTextForSearchDlg( CNativeW&, bool bGetHistory = false );		/* 現在カーソル位置単語または選択範囲より検索等のキーを取得（ダイアログ用） 2006.08.23 ryoji */
#endif // NKMM_

private:
	/* インクリメンタルサーチ */ 
	//2004.10.24 isearch migemo
	void ISearchEnter( ESearchMode mode, ESearchDirection direction);
	void ISearchExit();
	void ISearchExec(DWORD wChar);
	void ISearchExec(LPCWSTR pszText);
	void ISearchExec(bool bNext);
	void ISearchBack(void) ;
	void ISearchWordMake(void);
	void ISearchSetStatusMsg(CNativeT* msg) const;

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           括弧                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//	Jun. 16, 2000 genta
	bool  SearchBracket( const CLayoutPoint& ptPos, CLayoutPoint* pptLayoutNew, int* mode );	// 対括弧の検索		// modeの追加 02/09/18 ai
	bool  SearchBracketForward( CLogicPoint ptPos, CLayoutPoint* pptLayoutNew,
						const wchar_t* upChar, const wchar_t* dnChar, int* mode );	//	対括弧の前方検索	// modeの追加 02/09/19 ai
	bool  SearchBracketBackward( CLogicPoint ptPos, CLayoutPoint* pptLayoutNew,
						const wchar_t* dnChar, const wchar_t* upChar, int* mode );	//	対括弧の後方検索	// modeの追加 02/09/19 ai
	void DrawBracketPair( bool );								/* 対括弧の強調表示 02/09/18 ai */
	bool IsBracket( const wchar_t*, CLogicInt, CLogicInt );					/* 括弧判定 03/01/09 ai */
#ifdef NKMM_FIX_BRACKET_PAIR_INLINE
	void DispBracketPairInLine( SColorStrategyInfo* pInfo );	//!< 対括弧の強調表示(DrawLayoutLineへ統合されたオーバーレイ描画)
private:
	void RepaintBracketPairLines();							//!< 対括弧の強調位置を含む行だけを部分再描画する
	bool m_bBracketPairSuppressDraw = false;					//!< true の間はDispBracketPairInLineが何も描かない(消去用)
public:
#endif // NKMM_

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           補完                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	/* 支援 */
	//	Jan. 10, 2005 genta HandleCommandから補完関連処理を分離
	void PreprocessCommand_hokan( int nCommand );
	void PostprocessCommand_hokan(void);

	// 補完ウィンドウを表示する。Ctrl+Spaceや、文字の入力/削除時に呼び出されます。 YAZAKI 2002/03/11
	void ShowHokanMgr( CNativeW& cmemData, BOOL bAutoDecided );

	int HokanSearchByFile( const wchar_t*, bool, vector_ex<std::wstring>&, int ); // 2003.06.25 Moca


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         ジャンプ                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//@@@ 2003.04.13 MIK, Apr. 21, 2003 genta bClose追加
	//	Feb. 17, 2007 genta 相対パスの基準ディレクトリ指示を追加
	bool TagJumpSub( const TCHAR* pszJumpToFile, CMyPoint ptJumpTo,bool bClose = false,
		bool bRelFromIni = false, bool* pbJumpToSelf = NULL );


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         メニュー                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	int	CreatePopUpMenu_R( void );		/* ポップアップメニュー(右クリック) */
	int	CreatePopUpMenuSub( HMENU hMenu, int nMenuIdx, int* pParentMenus, EKeyHelpRMenuType eRmenuType );		/* ポップアップメニュー */
	void AddKeyHelpMenu( HMENU, EKeyHelpRMenuType );



	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           DIFF                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	void AnalyzeDiffInfo( const char*, int );	/* DIFF情報の解析 */	//@@@ 2002.05.25 MIK
	BOOL MakeDiffTmpFile( TCHAR*, HWND, ECodeType, bool );	/* DIFF一時ファイル作成 */	//@@@ 2002.05.28 MIK	//2005.10.29 maru
	BOOL MakeDiffTmpFile2( TCHAR*, const TCHAR*, ECodeType, ECodeType );
	void ViewDiffInfo( const TCHAR*, const TCHAR*, int, bool );		/* DIFF差分表示 */		//2005.10.29 maru

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                           履歴                              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//	Aug. 31, 2000 genta
	void AddCurrentLineToHistory(void);	//現在行を履歴に追加する


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                          その他                             //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	BOOL OPEN_ExtFromtoExt( BOOL, BOOL, const TCHAR* [], const TCHAR* [], int, int, const TCHAR* ); // 指定拡張子のファイルに対応するファイルを開く補助関数 // 2003.08.12 Moca
	//	Jan.  8, 2006 genta 折り返しトグル動作判定
	enum TOGGLE_WRAP_ACTION {
		TGWRAP_NONE = 0,
		TGWRAP_FULL,
		TGWRAP_WINDOW,
		TGWRAP_PROP,
	};
	TOGGLE_WRAP_ACTION GetWrapMode( CKetaXInt* newKetas );
	void SmartIndent_CPP( wchar_t );	/* C/C++スマートインデント処理 */
	/* コマンド操作 */
	void SetFont( void );										/* フォントの変更 */
	void SplitBoxOnOff( BOOL, BOOL, BOOL );						/* 縦・横の分割ボックス・サイズボックスのＯＮ／ＯＦＦ */

//	2001/06/18 asa-o
	bool  ShowKeywordHelp( POINT po, LPCWSTR pszHelp, LPRECT prcHokanWin);	// 補完ウィンドウ用のキーワードヘルプ表示
	void SetUndoBuffer( bool bPaintLineNumber = false );			// アンドゥバッファの処理
	HWND StartProgress();

#ifdef NKMM_LINE_MARGIN_TOP
	//! 行間のすきま取得
	int GetLineSpace() const {
		if (m_pTypeData) {
			return m_pTypeData->m_nLineSpace;
		} else {
			return 0;
		}
	}
	//! 行間のマージン取得
	int GetLineMargin() const {
		return GetLineSpace();
	}
#endif // NKMM_

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         アクセサ                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//主要構成部品アクセス
	CTextArea& GetTextArea(){ assert(m_pcTextArea); return *m_pcTextArea; }
	const CTextArea& GetTextArea() const{ assert(m_pcTextArea); return *m_pcTextArea; }
	CCaret& GetCaret(){ assert(m_pcCaret); return *m_pcCaret; }
	const CCaret& GetCaret() const{ assert(m_pcCaret); return *m_pcCaret; }
	CRuler& GetRuler(){ assert(m_pcRuler); return *m_pcRuler; }
	const CRuler& GetRuler() const{ assert(m_pcRuler); return *m_pcRuler; }

	//主要属性アクセス
	CTextMetrics& GetTextMetrics(){ return m_cTextMetrics; }
	const CTextMetrics& GetTextMetrics() const{ return m_cTextMetrics; }
	CViewSelect& GetSelectionInfo(){ return m_cViewSelect; }
	const CViewSelect& GetSelectionInfo() const{ return m_cViewSelect; }

	//主要オブジェクトアクセス
	CViewFont& GetFontset(){ assert(m_pcViewFont); return *m_pcViewFont; }
	const CViewFont& GetFontset() const{ assert(m_pcViewFont); return *m_pcViewFont; }

	//主要ヘルパアクセス
	const CViewParser& GetParser() const{ return m_cParser; }
	const CTextDrawer& GetTextDrawer() const{ return m_cTextDrawer; }
	CViewCommander& GetCommander(){ return m_cCommander; }


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                       メンバ変数群                          //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
public:
	//参照
	CEditWnd*		m_pcEditWnd;	//!< ウィンドウ
	CEditDoc*		m_pcEditDoc;	//!< ドキュメント
	const STypeConfig*	m_pTypeData;

	//主要構成部品
	CTextArea*		m_pcTextArea;
	CCaret*			m_pcCaret;
	CRuler*			m_pcRuler;

	//主要属性
	CTextMetrics	m_cTextMetrics;
	CViewSelect		m_cViewSelect;

#ifdef NKMM_MULTI_CURSOR
	//! マルチカーソル編集: プライマリ(m_pcCaret)以外の追加カーソル1個分の状態。
	//!
	//! 行・桁ともプライマリからの相対値(nRelLine/nRelColumn、レイアウト単位)として持ち、
	//! 実位置は「プライマリの現在位置 + 相対値」を毎回その場で算出する(絶対位置を保持して
	//! 逐次動かす方式ではない)。この方式だと、移動でバッファ端や短い行に達したカーソルが
	//! いても他のカーソルとの相対位置関係が絶対に崩れず、衝突もしない(整数オフセットが
	//! 常に一定のため)。移動系コマンド(F_UP/DOWN/LEFT/RIGHT等)はプライマリを動かすだけで
	//! 追加カーソルは全て自動追従する。編集系コマンド(タイピング・削除等、DispatchMoveMultiCursor
	//! ではなくApplyToAllCursorsを使うもの)は各カーソルの実際の編集結果を反映する必要があるため、
	//! 編集後に限りnRelLine/nRelColumnを再計算する(このときだけ相対値が更新されうる) 20260830
	//!
	//! - 行が算出結果でドキュメント範囲外になる間は「非アクティブ」(表示・編集の対象外)。
	//!   プライマリが動いて範囲内に戻れば自動的に復活する
	//! - 桁の基準はプライマリの「表示上クランプされている現在桁」ではなく、プライマリが
	//!   本来保持している桁(GetCaret().m_nCaretPosX_Prev)を使う。そうしないとプライマリ自身が
	//!   短い行で左にクランプされたとき、追加カーソルまでつられて左にずれてしまう 20260831
	//! - 桁は行の長さで随時クランプして表示・編集する(m_nCaretPosX_Prevと同じ考え方。
	//!   全角/半角混在で見た目がずれることはあるが、これは意図的な許容範囲。
	//!   nRelColumn自体は変更しないので、長さが足りる行に戻れば元の相対桁に復元される)
	//! - レイアウト単位で持つため、折り返し表示中でもプライマリ自身の上下移動(表示行基準)と
	//!   挙動が一致する(ロジック単位だった旧版では折り返し中にずれる既知の制約があった)
	//!
	//! 選択(bHasSelection/nAnchorRelLine/nAnchorRelColumn)は20260831に追加。当初は選択も
	//! 「プライマリのm_sSelectを平行移動するだけ」だったが、行の長さがカーソルごとに違う場合
	//! (短い行の末尾で選択が止まってしまい、次の行へ回り込まない)に正しく折り返せなかった。
	//! そこで選択の起点(アンカー)も、キャレット位置と全く同じ「プライマリ+固定オフセット」の
	//! 考え方で独立に持つように変更: 選択開始時のプライマリのアンカー位置+固定オフセットとして
	//! 記録し、以後は「アンカー(独立解決)〜現在位置(独立解決、nRelLine/nRelColumn)」を
	//! このカーソル自身の選択範囲として扱う。これにより選択移動コマンドをこのカーソルの実位置で
	//! 実際に再実行(ApplyToAllCursors経由)したときの折り返し等の結果が、次回の表示にもそのまま
	//! 正しく反映される(プライマリの結果を平行移動するのではなく、各カーソルが本当に独立して
	//! 動いた結果を毎回改めて解決するだけになるため)
	struct SExtraCursor {
		int			nRelLine;			//!< プライマリからの相対行数(レイアウト単位、作成時に固定)
		int			nRelColumn;			//!< プライマリからの相対桁数(レイアウト単位、実際に解決された/クランプされ得る値)
		bool		bHasSelection = false;		//!< このカーソル自身が選択中か
		int			nAnchorRelLine = 0;			//!< 選択起点(アンカー)のプライマリのアンカーからの相対行(選択開始時に固定)
		int			nAnchorRelColumn = 0;		//!< 選択起点(アンカー)のプライマリのアンカーからの相対桁(選択開始時に固定)
		//! 上下移動用の「希望桁」(CCaret::m_nCaretPosX_Prev相当)。プライマリのGetCaret().
		//! m_nCaretPosX_Prevからの相対値。nRelColumnと違い、短い行を上下移動で通過するときの
		//! クランプでは書き換わらない(横移動・編集など実際に桁が変わった操作でのみ更新)。
		//! これが無いと、短い行のせいで一時的にクランプされた実桁(nRelColumn)がそのまま
		//! 新しい"本来の"相対値として上書きされてしまい、長い行に戻ってもクランプ前の桁位置に
		//! 復元できなくなる 20260831
		int			nDesiredRelColumn = 0;
	};
	//! 基準点からnRelLine/nRelColumnだけオフセットした実位置を算出する共通ヘルパー。
	//! ResolveExtraCursor/ResolveExtraCursorAnchorの両方が使う
	bool ResolveExtraCursorFromBase( const CLayoutPoint& ptBase, int nBaseColumn, int nRelLine, int nRelColumn, CLayoutPoint* pOut ) const;
	//! extraの実位置を算出する。プライマリの現在行+nRelLineがドキュメント範囲外なら
	//! false(非アクティブ。表示・編集の対象外)を返す
	bool ResolveExtraCursor( const SExtraCursor& extra, CLayoutPoint* pOut ) const;
	//! extraの選択起点(アンカー)の実位置を算出する。bHasSelection==falseなら常にfalse
	bool ResolveExtraCursorAnchor( const SExtraCursor& extra, CLayoutPoint* pOut ) const;
	//! extraの選択範囲(nLineNum行分)を算出する。アンカー(ResolveExtraCursorAnchor)〜
	//! 現在位置(ResolveExtraCursor)を、このカーソル自身の選択範囲として独立に解決し、
	//! プライマリの選択範囲と全く同じGetSelectAreaLineFromRangeロジックでnLineNum行分を
	//! 切り出す(プライマリの範囲を平行移動するのではない。短い行での折り返し等、行ごとに
	//! 実際の文字数が違ってもこのカーソル自身の行として正しく扱われる) 20260831
	//! 選択中でない、またはアンカー/現在位置の行がドキュメント範囲外なら無効な範囲
	//! (IsValid()==false相当、Clear(-1))を返す
	CLayoutRange ResolveExtraCursorSelectAreaLine( const SExtraCursor& extra, CLayoutInt nLineNum ) const;
	//! プライマリ(m_pcCaret)以外の追加カーソル。
	//! 末尾 = 最後に追加したカーソルでもある(Ctrl+Shift+Uでの取り消しはpop_backのみでよい)。
	//! 空 = 従来通りの単一カーソル動作(既存コードパスは無変更) 20260830
	std::vector<SExtraCursor>	m_vExtraCursors;
	//! Alt+クリック中、クリック位置へキャレットを動かす直前のプライマリ位置。
	//! OnLBUTTONDOWNで記録し、OnLBUTTONUPでドラッグが起きなかった(=矩形選択にならなかった)
	//! ことが分かった時点で使う: プライマリをこの位置に戻し、クリック位置の方を新しい
	//! カーソルとして追加する(VS Code等のAlt+クリックと同じ挙動) 20260904
	CLayoutPoint	m_ptAltClickOrigCaretPos;
#endif // NKMM_

	//主要オブジェクト
	CViewFont*		m_pcViewFont;

	//主要ヘルパ
	CViewParser		m_cParser;
	CTextDrawer		m_cTextDrawer;
	CViewCommander	m_cCommander;

public:
	//ウィンドウ
	HWND			m_hwndParent;		/* 親ウィンドウハンドル */
	HWND			m_hwndVScrollBar;	/* 垂直スクロールバーウィンドウハンドル */
	int				m_nVScrollRate;		/* 垂直スクロールバーの縮尺 */
	HWND			m_hwndHScrollBar;	/* 水平スクロールバーウィンドウハンドル */
	HWND			m_hwndSizeBox;		/* サイズボックスウィンドウハンドル */
	CSplitBoxWnd*	m_pcsbwVSplitBox;	/* 垂直分割ボックス */
	CSplitBoxWnd*	m_pcsbwHSplitBox;	/* 水平分割ボックス */
	CAutoScrollWnd	m_cAutoScrollWnd;	//!< オートスクロール

public:
	//描画
	bool			m_bDrawSWITCH;
	COLORREF		m_crBack;				/* テキストの背景色 */			// 2006.12.07 ryoji
	COLORREF		m_crBack2;				// テキストの背景(キャレット用)
	CLayoutInt		m_nOldUnderLineY;		// 前回作画したカーソルアンダーラインの位置 0未満=非表示
	CLayoutInt		m_nOldUnderLineYBg;
	int				m_nOldUnderLineYMargin;
	int				m_nOldUnderLineYHeight;
	int				m_nOldUnderLineYHeightReal;
	int				m_nOldCursorLineX;		/* 前回作画したカーソル位置縦線の位置 */ // 2007.09.09 Moca
	int				m_nOldCursorVLineWidth;	// カーソル位置縦線の太さ(px)

public:
	//画面バッファ
	HDC				m_hdcCompatDC;		/* 再描画用コンパチブルＤＣ */
	HBITMAP			m_hbmpCompatBMP;	/* 再描画用メモリＢＭＰ */
	HBITMAP			m_hbmpCompatBMPOld;	/* 再描画用メモリＢＭＰ(OLD) */
	int				m_nCompatBMPWidth;  /* 再作画用メモリＢＭＰの幅 */	// 2007.09.09 Moca 互換BMPによる画面バッファ
	int				m_nCompatBMPHeight; /* 再作画用メモリＢＭＰの高さ */	// 2007.09.09 Moca 互換BMPによる画面バッファ

public:
	//D&D
	CDropTarget*	m_pcDropTarget;
	BOOL			m_bDragMode;	/* 選択テキストのドラッグ中か */
	CLIPFORMAT		m_cfDragData;	/* ドラッグデータのクリップ形式 */	// 2008.06.20 ryoji
	BOOL			m_bDragBoxData;	/* ドラッグデータは矩形か */
	CLayoutPoint	m_ptCaretPos_DragEnter;			/* ドラッグ開始時のカーソル位置 */	// 2007.12.09 ryoji
	CLayoutInt		m_nCaretPosX_Prev_DragEnter;	/* ドラッグ開始時のX座標記憶 */	// 2007.12.09 ryoji

	//括弧
	CLogicPoint		m_ptBracketCaretPos_PHY;	// 前カーソル位置の括弧の位置 (改行単位行先頭からのバイト数(0開始), 改行単位行の行番号(0開始))
	CLogicPoint		m_ptBracketPairPos_PHY;		// 対括弧の位置 (改行単位行先頭からのバイト数(0開始), 改行単位行の行番号(0開始))
	BOOL			m_bDrawBracketPairFlag;		/* 対括弧の強調表示を行なうか */						// 03/02/18 ai

	//マウス
	bool			m_bActivateByMouse;		//!< マウスによるアクティベート	//2007.10.02 nasukoji
	DWORD			m_dwTripleClickCheck;	//!< トリプルクリックチェック用時刻	//2007.10.02 nasukoji
	CMyPoint		m_cMouseDownPos;	//!< クリック時のマウス座標
	int				m_nWheelDelta;	//!< ホイール変化量
	EFunctionCode	m_eWheelScroll; //!< スクロールの種類
	int				m_nMousePouse;	// マウス停止時間
	CMyPoint		m_cMousePousePos;	// マウスの停止位置
	bool			m_bHideMouse;

	int				m_nAutoScrollMode;			//!< オートスクロールモード
	bool			m_bAutoScrollDragMode;		//!< ドラッグモード
	CMyPoint		m_cAutoScrollMousePos;		//!< オートスクロールのマウス基準位置
	bool			m_bAutoScrollVertical;		//!< 垂直スクロール可
	bool			m_bAutoScrollHorizontal;	//!< 水平スクロール可

	//検索
	CSearchStringPattern m_sSearchPattern;
	mutable CBregexp	m_CurRegexp;				/*!< コンパイルデータ */
	bool				m_bCurSrchKeyMark;			/* 検索文字列のマーク */
	bool				m_bCurSearchUpdate;			//!< コンパイルデータ更新要求
	int					m_nCurSearchKeySequence;	//!< 検索キーシーケンス
	std::wstring		m_strCurSearchKey;			//!< 検索文字列
	SSearchOption		m_sCurSearchOption;			// 検索／置換  オプション
	CLogicPoint			m_ptSrchStartPos_PHY;		// 検索/置換開始時のカーソル位置 (改行単位行先頭からのバイト数(0開始), 改行単位行の行番号(0開始))
	BOOL				m_bSearch;					/* 検索/置換開始位置を登録するか */											// 02/06/26 ai
	ESearchDirection	m_nISearchDirection;		//!< 検索方向
	ESearchMode			m_nISearchMode;				//!< 検索モード
	bool				m_bISearchWrap;
	bool				m_bISearchFlagHistory[256];
	int					m_nISearchHistoryCount;
	bool				m_bISearchFirst;
	CLayoutRange		m_sISearchHistory[256];

	//マクロ
	bool			m_bExecutingKeyMacro;		/* キーボードマクロの実行中 */
	BOOL			m_bCommandRunning;	/* コマンドの実行中 */

	// 入力補完
	BOOL			m_bHokan;			//	補完中か？＝補完ウィンドウが表示されているか？かな？

	//編集
	bool			m_bDoing_UndoRedo;	/* アンドゥ・リドゥの実行中か */

	// 辞書Tip関連
	DWORD			m_dwTipTimer;			/* Tip起動タイマー */
	CTipWnd			m_cTipWnd;				/* Tip表示ウィンドウ */
	POINT			m_poTipCurPos;			/* Tip起動時のマウスカーソル位置 */
	BOOL			m_bInMenuLoop;			/* メニュー モーダル ループに入っています */
	CDicMgr			m_cDicMgr;				/* 辞書マネージャ */

	TCHAR			m_szComposition[512]; // IMR_DOCUMENTFEED用入力中文字列データ

	// IME
private:
	UINT			m_uMSIMEReconvertMsg;
	UINT			m_uATOKReconvertMsg;
public:
	UINT			m_uWM_MSIME_RECONVERTREQUEST;
private:
	int				m_nLastReconvLine;             //2002.04.09 minfu 再変換情報保存用;
	int				m_nLastReconvIndex;            //2002.04.09 minfu 再変換情報保存用;

public:
	//ATOK専用再変換のAPI
	typedef BOOL (WINAPI *FP_ATOK_RECONV)( HIMC , int ,PRECONVERTSTRING , DWORD  );
	HMODULE			m_hAtokModule;
	FP_ATOK_RECONV	m_AT_ImmSetReconvertString;

	// その他
	CAutoMarkMgr*	m_cHistory;	//	Jump履歴
	CRegexKeyword*	m_cRegexKeyword;	//@@@ 2001.11.17 add MIK
	int				m_nMyIndex;	/* 分割状態 */
	CMigemo*		m_pcmigemo;

private:
	DISALLOW_COPY_AND_ASSIGN(CEditView);
	
#ifdef NKMM_FIX_FLICKER
public:
	void BeginIgnoreUpdateWindow();
	void EndIgnoreUpdateWindow(bool bUpdate = true);
	void RequestUpdateWindow();
	int m_ignore_update_window = 0;  // UpdateWindowを無視する
	bool m_request_update_window = false;
#endif // NKMM_

#ifdef NKMM_FIX_EDITVIEW_SCRBAR
public:
	//! スクロールバーマーカークラス
	class ScrBarMarker {
	public:
		explicit ScrBarMarker(CEditView *pEditView);
		~ScrBarMarker();
		
		void CallPaint(int foo);               // 描画要求 foo:マーキング用
		void Clear(int foo);                   // クリア (再構築要求) foo:マーキング用
		void Build(bool bCacheClear, int foo); // 再構築 foo:マーキング用
		void DrawRequest();                    // 描画リクエスト
		// 描画。bUpdateScrollInfo=falseにすると、GetScrollInfo/SetScrollInfo(re
		// draw=TRUE)による正規スクロールバーの強制再描画をスキップする(ホバー中の
		// 高頻度な上乗せ描画専用。SetScrollInfo(TRUE)はネイティブ側の全面再描画=
		// テーマのホバーアニメーション再トリガーを伴うため非常に重い)
		void Draw(bool bUpdateScrollInfo = true);

		// 登録・削除
		bool Add(int nLayoutY, uint32_t magic);
		bool Del(int nLayoutY, uint32_t magic);
		
		// 検索文字列のある行か確認
		bool IsFoundLine(const CDocLine *pCDocLine);

		// クリックされたスクロールバー上のY座標(クライアント座標)に最も近い
		// マーク行(検索/ブックマーク)を探す。見つかればそのレイアウト行を*pOutLayoutYへ返す
		bool HitTest(int nClientY, CLayoutInt *pOutLayoutY);

		// スレッドを待つ
		void WaitForBuild(bool abort = false);
		void WaitForDraw(bool abort = false);

	private:
		// 20260810 スレッドプール(PTP_WORK)化。編集のたびに_beginthreadexで
		// スレッドを新規生成していたのを、常駐ワーカーへの投入(SubmitThreadpoolWork)に
		// 変更しスレッド生成コストを無くす。詳細はmy_config.h NKMM_FIX_EDITVIEW_SCRBAR
		// のコメント参照。
		static void CALLBACK BuildWorkCallback(PTP_CALLBACK_INSTANCE instance, void *pv, PTP_WORK work);
		static void CALLBACK DrawWorkCallback(PTP_CALLBACK_INSTANCE instance, void *pv, PTP_WORK work);

	public:
		CEditView *pEditView_ = nullptr;

		int nLastLineCount_ = 0;                    // 最後に更新した時の行数
		std::vector<uint32_t> vLines_;              // キャッシュ

		std::tstring strKey_;                       // 構築時のキー

		int nSearchFoundLine_ = 0;                  // 見つかった検索行の数
		int nMarkFoundLine_ = 0;                    // 見つかったブックマーク行の数

		// 20260810 マーク描画色のキャッシュ。以前はDrawWorkCallback()の呼び出しの
		// たびにRegKey(NKMM_REGKEY).get_s()で都度レジストリを読んでいた(1回の
		// get_s()でRegOpenKeyEx+RegQueryValueEx x2+RegCloseKeyの計4回のレジストリ
		// アクセス、3色で12回)。ホバー中は50ms間隔でDrawWorkCallback()が繰り返し
		// 走るため、これが無視できないCPU負荷になっていた。色が変わるのは設定変更
		// 時だけなので、コンストラクタとBuildWorkCallback()(実際にドキュメントが
		// 変化した時だけ走る)でのみ読み直し、DrawWorkCallback()はこのキャッシュを
		// 読むだけにする。
		COLORREF clrSearchCache_ = 0;
		COLORREF clrMarkCache_ = 0;
		COLORREF clrCursorCache_ = 0;
		void RefreshColorCache();                   // レジストリから色キャッシュを再読み込み

		//std::mutex mtxCacheMutex_;
		// 20260810 bBuildThreadRunning_の解除とbRebuildPending_の確認を不可分に
		// 行うためのロック。「Build()が running を見てpendingを立てる」タイミングと
		// 「BuildWorkCallbackがpendingを見てrunningを解除する」タイミングがTOCTOU
		// レースを起こし、再構築要求を取りこぼす経路があったため導入した。
		// 呼び出し頻度が低い(デバウンス発火時・行数変化時のみ)ためロック競合の
		// 心配はない。bBuildThreadRunning_自体は他所(CallPaint()等)から非同期に
		// ロック無しで読まれる箇所があるためstd::atomic<bool>のままにしてある。
		std::mutex mtxBuildState_;
		PTP_WORK pBuildWork_ = nullptr;                    // キャッシュ作成ワーク
		std::atomic<bool> bBuildThreadRunning_ = false;    //   稼働状態 (mtxBuildState_で保護される複合操作あり)
		std::atomic<bool> bExitRequestBuildThread_ = false;//   終了リクエスト
		std::atomic<bool> bRebuildPending_ = false;        //   実行中に来た再構築要求 (mtxBuildState_で保護)

		// 20260810 bDrawThreadRunning_の解除とbRestartRequestDrawThread_の確認を
		// 不可分に行うためのロック。mtxBuildState_と同じ理由(TOCTOUレースによる
		// 再描画要求の取りこぼし防止)。
		std::mutex mtxDrawState_;
		PTP_WORK pDrawWork_ = nullptr;                     // キャッシュ描画ワーク
		std::atomic<bool> bDrawThreadRunning_ = false;     //   稼働状態 (mtxDrawState_で保護される複合操作あり)
		std::atomic<bool> bExitRequestDrawThread_ = false; //   終了リクエスト
		std::atomic<bool> bRestartRequestDrawThread_ = false; // 描画やり直しリクエスト (mtxDrawState_で保護)
		int    nDrawRequestCount_ = 0;              //   描画リクエスト回数

		SCROLLBARINFO sbi_;
	};
	
	std::unique_ptr<ScrBarMarker> SBMarker_;
	
	void _SB_Marker_CallPaint(int foo);               // 描画要求 foo:マーキング用
	void _SB_Marker_Clear(int foo);                   // クリア (再構築要求) foo:マーキング用
	void _SB_Marker_Build(bool bCacheClear, int foo); // 再構築 foo:マーキング用
	void _SB_Marker_DrawRequest();                    // 描画リクエスト
	void _SB_Marker_Draw();                           // 描画
#if NKMM_SCRBAR_MARKER_CLICK_JUMP
	bool _SB_Marker_HitTestAndJump(int nClientY);     // スクロールバークリック位置のマーク行へジャンプ
#endif // NKMM_SCRBAR_MARKER_CLICK_JUMP
#if NKMM_SCRBAR_MARKER_HOVER_REDRAW
	void _SB_Marker_HoverRedraw();                    // ホバー中の軽量再描画(SetScrollInfo更新を伴わない)
#endif // NKMM_SCRBAR_MARKER_HOVER_REDRAW

	int GetDocumentWordNum() const;
	
	// トレース用マクロ
	#define SCRBAR_MARKCACHE_TRACE  (0)
	#if SCRBAR_MARKCACHE_TRACE
		#define SB_Marker_Trace(...)              si::logln(__VA_ARGS__);
		#define SB_Marker_CallPaint(foo)          _SB_Marker_CallPaint(foo); DebugOutputCaller("    <- Caller, CallPaint")
		#define SB_Marker_Clear(foo)              _SB_Marker_Clear(foo); DebugOutputCaller("    <- Caller, Clear")
		#define SB_Marker_Build(bCacheClear, foo) _SB_Marker_Build(bCacheClear, foo); DebugOutputCaller("    <- Caller, Build")
		#define SB_Marker_DrawRequest()           _SB_Marker_DrawRequest(); DebugOutputCaller("    <- Caller, DrawRequest")
		#define SB_Marker_Draw()                  _SB_Marker_Draw(); DebugOutputCaller("    <- Caller, Draw")
	#else
		#define SB_Marker_Trace(...)              
		#define SB_Marker_CallPaint(foo)          _SB_Marker_CallPaint(foo)
		#define SB_Marker_Clear(foo)              _SB_Marker_Clear(foo)
		#define SB_Marker_Build(bCacheClear, foo) _SB_Marker_Build(bCacheClear, foo)
		#define SB_Marker_DrawRequest()           _SB_Marker_DrawRequest()
		#define SB_Marker_Draw()                  _SB_Marker_Draw()
	#endif

	CKetaXInt nMaxLineKetas_ = 0;  // 前更新時の折り返し桁数

#endif // NKMM_

#ifdef NKMM_FIX_ASYNC_SEARCH_NEXT
public:
	//! 「次を検索」の非同期実行(巨大ファイルでのUIフリーズ対策)
	//! 対象はCommand_SEARCH_NEXT内の「選択なし・単純な前方検索」の場合のみ。
	//! 詳細はmy_config.hのNKMM_FIX_ASYNC_SEARCH_NEXTの説明を参照。
	class AsyncFindNext {
	public:
		explicit AsyncFindNext(CEditView* pEditView) : pEditView_(pEditView) {}
		~AsyncFindNext() { WaitForAbort(); }

		//! 非同期検索を要求する(呼び出し前に走っているスレッドがあれば中断・待機してから起動)
		void Request(
			const CLogicPoint& ptBegin,
			bool bSearchAll,
			bool bRedraw,
			bool bReplaceAllUnused,
			HWND hwndParent,
			const std::wstring& strPattern,
			const SSearchOption& sSearchOption
		);

		//! 実行中のスレッドがあれば中断要求を出し、終了を待つ(結果は捨てる)。
		//! 文書を書き換える前に必ず呼ぶこと(CEditView::ReplaceData_CEditView3参照)。
		void WaitForAbort();

	public:
		CEditView* pEditView_ = nullptr;

		HANDLE hThread_ = 0;
		bool   bThreadRunning_ = false;
		volatile bool bAbortRequested_ = false;

		int nGeneration_ = 0;  // Request()のたびにインクリメント。完了メッセージの世代照合に使う。

		// スレッドへの入力(Request()内で設定してから起動。実行中は読み取り専用)
		CLogicPoint ptBegin_;
		bool bSearchAll_ = false;
		bool bRedrawForResult_ = false;
		HWND hwndParentForResult_ = nullptr;
		std::wstring strPatternOwned_;       // 共有バッファを直接参照しない独立コピー
		SSearchOption sSearchOptionOwned_;    // 同上
		std::unique_ptr<CSearchStringPattern> pPattern_;

		// スレッドからの出力(スレッド終了後、PostMessageより前に書き込み済み)
		int nResultFound_ = 0;
		CLogicRange sResultRange_;
	};

	std::unique_ptr<AsyncFindNext> AsyncFindNext_;
#endif // NKMM_
};



class COutputAdapter
{
public:
	COutputAdapter(){};
	virtual  ~COutputAdapter(){};

	virtual bool OutputW(const WCHAR* pBuf, int size = -1) = 0;
	virtual bool OutputA(const ACHAR* pBuf, int size = -1) = 0;
	virtual bool IsEnableRunningDlg(){ return true; }
	virtual bool IsActiveDebugWindow(){ return true; }
};
///////////////////////////////////////////////////////////////////////
#endif /* _CEDITVIEW_H_ */




