﻿/*
	Copyright (C) 2008, kobake

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

#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CFigure_Eol.h"
#include "types/CTypeSupport.h"
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include "window/CEditWnd.h"

//折り返し描画
void _DispWrap(CGraphics& gr, DispPos* pDispPos, const CEditView* pcView);

//EOF描画関数
//実際には pX と nX が更新される。
//2004.05.29 genta
//2007.08.25 kobake 戻り値を void に変更。引数 x, y を DispPos に変更
//2007.08.25 kobake 引数から nCharWidth, nLineHeight を削除
//2007.08.28 kobake 引数 fuOptions を削除
//void _DispEOF( CGraphics& gr, DispPos* pDispPos, const CEditView* pcView, bool bTrans);

//改行記号描画
//2007.08.30 kobake 追加
void _DispEOL(CGraphics& gr, DispPos* pDispPos, CEol cEol, const CEditView* pcView, bool bTrans);


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                        CFigure_Eol                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CFigure_Eol::Match(const wchar_t* pText, int nTextLen) const
{
	// 2014.06.18 折り返し・最終行だとDrawImpでcEol.GetLen()==0になり無限ループするので
	// もしも行の途中に改行コードがあった場合はMatchさせない
	if(nTextLen == 2 && pText[0]==L'\r' && pText[1]==L'\n')return true;
	if(nTextLen == 1 && WCODE::IsLineDelimiterExt(pText[0]))return true;
	return false;
}

// 2006.04.29 Moca 選択処理のため縦線処理を追加
//$$ 高速化可能。
bool CFigure_Eol::DrawImp(SColorStrategyInfo* pInfo)
{
	CEditView* pcView = pInfo->m_pcView;

	// 改行取得
	const CLayout* pcLayout = pInfo->m_pDispPos->GetLayoutRef();
	CEol cEol = pcLayout->GetLayoutEol();
	if(cEol.GetLen()){
#ifdef NKMM_FIX_WS_COLOR
		bool bTrans = DrawImp_StyleSelect(pInfo);
#else
		// CFigureSpace::DrawImp_StyleSelectもどき。選択・検索色を優先する
		CTypeSupport cCurrentType(pcView, pInfo->GetCurrentColor());	// 周辺の色（現在の指定色/選択色）
		CTypeSupport cCurrentType2(pcView, pInfo->GetCurrentColor2());	// 周辺の色（現在の指定色）
		CTypeSupport cTextType(pcView, COLORIDX_TEXT);				// テキストの指定色
		CTypeSupport cSpaceType(pcView, GetDispColorIdx());	// 空白の指定色
		CTypeSupport cSearchType(pcView, COLORIDX_SEARCH);	// 検索色(EOL固有)
		CTypeSupport cCurrentTypeBg(pcView, pInfo->GetCurrentColorBg());
		CTypeSupport& cCurrentType3 = (cCurrentType2.GetBackColor() == cTextType.GetBackColor() ? cCurrentTypeBg: cCurrentType2);
		COLORREF crText;
		COLORREF crBack;
		bool bSelecting = pInfo->GetCurrentColor() != pInfo->GetCurrentColor2();
		bool blendColor = bSelecting && cCurrentType.GetTextColor() == cCurrentType.GetBackColor(); // 選択混合色
		CTypeSupport& currentStyle = blendColor ? cCurrentType2 : cCurrentType;
		CTypeSupport *pcText, *pcBack;
#ifdef NKMM_FIX_SELAREA
		blendColor = true;
#endif // NKMM_FIX_SELAREA
		if( bSelecting && !blendColor ){
			// 選択文字色固定指定
			pcText = &cCurrentType;
			pcBack = &cCurrentType;
		}else if( pInfo->GetCurrentColor2() == COLORIDX_SEARCH ){
			// 検索色優先
			pcText = &cSearchType;
			pcBack = &cSearchType;
		}else{
			pcText = cSpaceType.GetTextColor() == cTextType.GetTextColor() ? &cCurrentType2 : &cSpaceType;
			pcBack = cSpaceType.GetBackColor() == cTextType.GetBackColor() ? &cCurrentType3 : &cSpaceType;
		}
		if( blendColor ){
			// 混合色(検索色を優先しつつ)
			crText = pcView->GetTextColorByColorInfo2(cCurrentType.GetColorInfo(), pcText->GetColorInfo());
			crBack = pcView->GetBackColorByColorInfo2(cCurrentType.GetColorInfo(), pcBack->GetColorInfo());
		}else{
			crText = pcText->GetTextColor();
			crBack = pcBack->GetBackColor();
		}
		pInfo->m_gr.PushTextForeColor(crText);
		pInfo->m_gr.PushTextBackColor(crBack);
		bool bTrans = pcView->IsBkBitmap() && cTextType.GetBackColor() == crBack;
		SFONT sFont;
		sFont.m_sFontAttr.m_bBoldFont = cSpaceType.IsBoldFont() || currentStyle.IsBoldFont();
		sFont.m_sFontAttr.m_bUnderLine = cSpaceType.HasUnderLine();
		sFont.m_hFont = pInfo->m_pcView->GetFontset().ChooseFontHandle( 0, sFont.m_sFontAttr );
		pInfo->m_gr.PushMyFont(sFont);
#endif // NKMM_

		DispPos sPos(*pInfo->m_pDispPos);	// 現在位置を覚えておく
		_DispEOL(pInfo->m_gr, pInfo->m_pDispPos, cEol, pcView, bTrans);
		DrawImp_StylePop(pInfo);
		DrawImp_DrawUnderline(pInfo, sPos);

		pInfo->m_nPosInLogic+=cEol.GetLen();
	}else{
		// 無限ループ対策
		pInfo->m_nPosInLogic += 1;
		assert_warning( 1 );
	}

	return true;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     折り返し描画実装                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

// 折り返し描画
void _DispWrap(CGraphics& gr, DispPos* pDispPos, const CEditView* pcView, CLayoutYInt nLineNum )
{
	CTypeSupport cWrapType(pcView,COLORIDX_WRAP);
	const wchar_t* szText;
	if (cWrapType.IsDisp()) {
		szText = L"￢";
	}else{
		return;
	}
	CLayoutXInt width = CLayoutXInt(pcView->GetTextMetrics().CalcTextWidth3(szText, 1));
	RECT rcClip2;
	if(pcView->GetTextArea().GenerateClipRect(&rcClip2, *pDispPos, width))
	{
		//サポートクラス
		CTypeSupport cWrapType(pcView,COLORIDX_WRAP);
		CTypeSupport cTextType(pcView,COLORIDX_TEXT);
		CTypeSupport cBgLineType(pcView,COLORIDX_CARETLINEBG);
		CTypeSupport cEvenBgLineType(pcView,COLORIDX_EVENLINEBG);
		CTypeSupport cPageViewBgLineType(pcView,COLORIDX_PAGEVIEW);
#ifdef NKMM_FIX_COMMENT
		bool bBgcolor = true;
#else
		bool bBgcolor = cWrapType.GetBackColor() == cTextType.GetBackColor();
#endif // NKMM_
		EColorIndexType eBgcolorOverwrite = COLORIDX_WRAP;
		bool bTrans = pcView->IsBkBitmap();
		if( cWrapType.IsDisp() ){
			CEditView& cActiveView = pcView->m_pcEditWnd->GetActiveView();
			if( cBgLineType.IsDisp() && pcView->GetCaret().GetCaretLayoutPos().GetY2() == nLineNum ){
				if( bBgcolor ){
					eBgcolorOverwrite = COLORIDX_CARETLINEBG;
					bTrans = bTrans && cBgLineType.GetBackColor() == cTextType.GetBackColor();
				}
			}else if( cEvenBgLineType.IsDisp() && nLineNum % 2 == 1 ){
				if( bBgcolor ){
					eBgcolorOverwrite = COLORIDX_EVENLINEBG;
					bTrans = bTrans && cEvenBgLineType.GetBackColor() == cTextType.GetBackColor();
				}
			}
		}
		bool bChangeColor = false;

		//描画文字列と色の決定
		if( cWrapType.IsDisp() )
		{
			cWrapType.SetGraphicsState_WhileThisObj(gr);
			if( eBgcolorOverwrite != COLORIDX_WRAP ){
				bChangeColor = true;
				gr.PushTextBackColor( CTypeSupport(pcView, eBgcolorOverwrite).GetBackColor() );
			}
		}
		int fontNo = WCODE::GetFontNo(*szText);
		int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
		int nDx[1] = {(Int)width};

		//描画
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
#ifdef NKMM_LINE_MARGIN_TOP
			pcView->GetLineMargin() +
#endif // NKMM_
			pDispPos->GetDrawPos().y + nHeightMargin,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClip2,
			szText,
			wcslen(szText),
			nDx
		);
		if( bChangeColor ){
			gr.PopTextBackColor();
		}
	}
	pDispPos->ForwardDrawCol(width);
}
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       EOF描画実装                           //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
/*!
EOF記号の描画
@date 2004.05.29 genta  MIKさんのアドバイスにより関数にくくりだし
@date 2007.08.28 kobake 引数 nCharWidth 削除
@date 2007.08.28 kobake 引数 fuOptions 削除
@date 2007.08.30 kobake 引数 EofColInfo 削除
*/
void _DispEOF(
	CGraphics&			gr,			//!< [in] 描画対象のDevice Context
	DispPos*			pDispPos,	//!< [in] 表示座標
	const CEditView*	pcView
)
{
	// 描画に使う色情報
	CTypeSupport cEofType(pcView,COLORIDX_EOF);
	if(!cEofType.IsDisp())
		return;
	CTypeSupport cTextType(pcView,COLORIDX_TEXT);
	bool bTrans = pcView->IsBkBitmap() && cEofType.GetBackColor() == cTextType.GetBackColor();

#ifdef NKMM_FIX_WS_COLOR
	COLORREF crText = cTextType.GetTextColor();
	COLORREF crBack = cTextType.GetBackColor();
	//! 色をマージする
	//! @param colText テキスト色
	//! @param colBase ベースとなる色
	//! @return 合成後の色
	auto fnMeargeColor = [](COLORREF colText, COLORREF colBase, int blendPer)
	{
		COLORREF c1 = colText;
		COLORREF c2 = colBase;
		float blendPerN = 1.0f / 100.0f * blendPer;
		const float r1 = (float)GetRValue(c1);
		const float g1 = (float)GetGValue(c1);
		const float b1 = (float)GetBValue(c1);
		const float r2 = (float)GetRValue(c2);
		const float g2 = (float)GetGValue(c2);
		const float b2 = (float)GetBValue(c2);
		float r = r2 + (r1 - r2) * blendPerN;
		float g = g2 + (g1 - g2) * blendPerN;
		float b = b2 + (b1 - b2) * blendPerN;
		return RGB( (BYTE)r, (BYTE)g, (BYTE)b );
	};
	
	static int nBlendPer = RegKey(NKMM_REGKEY).get(_T("WhiteSpaceBlendPer"), NKMM_WS_BLEND_PER);
	// 現在のテキスト色と現在の背景色をブレンドする (空白TABのカラー設定は無視されます)
	COLORREF col1 = cTextType.GetTextColor();
	COLORREF col2 = crBack;	// 合成済みの色を使用する
	crText = fnMeargeColor(col1, col2, nBlendPer);
#endif // NKMM_

	//必要なインターフェースを取得
	const CTextMetrics* pMetrics=&pcView->GetTextMetrics();
	const CTextArea* pArea=&pcView->GetTextArea();

	//定数
	static const wchar_t	szEof[] = L"#␃";
	const int		nEofLen = _countof(szEof) - 1;

	cEofType.SetGraphicsState_WhileThisObj(gr);
	int fontNo = WCODE::GetFontNo('E');
	int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
#ifdef NKMM_FIX_WS_COLOR
	gr.PushTextForeColor(crText);
	gr.PushTextBackColor(crBack);
#endif // NKMM_
	pcView->GetTextDrawer().DispText(gr, pDispPos, nHeightMargin, szEof, nEofLen, bTrans);
#ifdef NKMM_FIX_WS_COLOR
	gr.PopTextForeColor();
	gr.PopTextBackColor();
#endif // NKMM_
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       改行描画実装                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

//画面描画補助関数
//May 23, 2000 genta
//@@@ 2001.12.21 YAZAKI 改行記号の書きかたが変だったので修正
void _DrawEOL(
	CGraphics&		gr,
	const CMyRect&	rcEol,
#ifdef NKMM_LINE_MARGIN_TOP
	int nMargin,
#endif // NKMM_
	CEol			cEol,
	bool			bBold,
	COLORREF		pColor
);

//2007.08.30 kobake 追加
void _DispEOL(CGraphics& gr, DispPos* pDispPos, CEol cEol, const CEditView* pcView, bool bTrans)
{
	if (!CTypeSupport(pcView,COLORIDX_EOL).IsDisp())
		return;

	CTypeSupport cTextType(pcView,COLORIDX_TEXT);
	COLORREF crText = cTextType.GetTextColor();
	COLORREF crBack = cTextType.GetBackColor();
	//! 色をマージする
	//! @param colText テキスト色
	//! @param colBase ベースとなる色
	//! @return 合成後の色
	auto fnMeargeColor = [](COLORREF colText, COLORREF colBase, int blendPer)
	{
		COLORREF c1 = colText;
		COLORREF c2 = colBase;
		float blendPerN = 1.0f / 100.0f * blendPer;
		const float r1 = (float)GetRValue(c1);
		const float g1 = (float)GetGValue(c1);
		const float b1 = (float)GetBValue(c1);
		const float r2 = (float)GetRValue(c2);
		const float g2 = (float)GetGValue(c2);
		const float b2 = (float)GetBValue(c2);
		float r = r2 + (r1 - r2) * blendPerN;
		float g = g2 + (g1 - g2) * blendPerN;
		float b = b2 + (b1 - b2) * blendPerN;
		return RGB( (BYTE)r, (BYTE)g, (BYTE)b );
	};
	
	static int nBlendPer = RegKey(NKMM_REGKEY).get(_T("WhiteSpaceBlendPer"), NKMM_WS_BLEND_PER);
	// 現在のテキスト色と現在の背景色をブレンドする (空白TABのカラー設定は無視されます)
	COLORREF col1 = cTextType.GetTextColor();
	COLORREF col2 = crBack;	// 合成済みの色を使用する
	crText = fnMeargeColor(col1, col2, nBlendPer);

	RECT rcClip2;
	int fontNo = WCODE::GetFontNo('E');
	int nHeightMargin = pcView->GetTextMetrics().GetCharHeightMarginByFontNo(fontNo);
	CLayoutXInt width = CLayoutXInt(pcView->GetTextMetrics().CalcTextWidth3(L"↵", 1));
	int nDx[1] = {(Int)width};

	if (!pcView->GetTextArea().GenerateClipRect(&rcClip2, *pDispPos, width))
		return;

	gr.PushTextForeColor(crText);

	switch( cEol.GetType() ){
	case EOL_CRLF: // 下左矢印
		{
			static const wchar_t	 szEol[] = L"↵";

			::ExtTextOutW_AnyBuild(
				gr,
				pDispPos->GetDrawPos().x,
#ifdef NKMM_LINE_MARGIN_TOP
				pcView->GetLineMargin() +
#endif // NKMM_
				pDispPos->GetDrawPos().y + nHeightMargin,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				szEol,
				wcslen(szEol),
				nDx
			);
			break;
		}
	case EOL_CR:	 // 左向き矢印
		{
			static const wchar_t	szEol[] = L"←";

			::ExtTextOutW_AnyBuild(
				gr,
				pDispPos->GetDrawPos().x,
#ifdef NKMM_LINE_MARGIN_TOP
				pcView->GetLineMargin() +
#endif // NKMM_
				pDispPos->GetDrawPos().y + nHeightMargin,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				szEol,
				wcslen(szEol),
				nDx
			);
			break;
		}
	case EOL_LF:	 // 下向き矢印
		{
			static const wchar_t	szEol[] = L"↓";

			::ExtTextOutW_AnyBuild(
				gr,
				pDispPos->GetDrawPos().x,
#ifdef NKMM_LINE_MARGIN_TOP
				pcView->GetLineMargin() +
#endif // NKMM_
				pDispPos->GetDrawPos().y + nHeightMargin,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				szEol,
				wcslen(szEol),
				nDx
			);
			break;
		}
	case EOL_NEL:
	case EOL_LS:
	case EOL_PS:
		{
			// 左下矢印(折れ曲がりなし)
			static const wchar_t	szEol[] = L"↙";

			::ExtTextOutW_AnyBuild(
				gr,
				pDispPos->GetDrawPos().x,
#ifdef NKMM_LINE_MARGIN_TOP
				pcView->GetLineMargin() +
#endif // NKMM_
				pDispPos->GetDrawPos().y + nHeightMargin,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				szEol,
				wcslen(szEol),
				nDx
			);
			break;
		}
	}

	gr.PopTextForeColor();

	pDispPos->ForwardDrawCol(width);
}


//	May 23, 2000 genta
/*!
画面描画補助関数:
行末の改行マークを改行コードによって書き分ける（メイン）

@note bBoldがTRUEの時は横に1ドットずらして重ね書きを行うが、
あまり太く見えない。

@date 2001.12.21 YAZAKI 改行記号の描きかたを変更。ペンはこの関数内で作るようにした。
						矢印の先頭を、sx, syにして描画ルーチン書き直し。
*/
void _DrawEOL(
	CGraphics&		gr,		//!< Device Context Handle
	const CMyRect&	rcEol,		//!< 描画領域
#ifdef NKMM_LINE_MARGIN_TOP
	int nMargin,
#endif // NKMM_
	CEol			cEol,		//!< 行末コード種別
	bool			bBold,		//!< TRUE: 太字
	COLORREF		pColor		//!< 色
)
{
	int sx, sy;	//	矢印の先頭
	gr.SetPen( pColor );

	switch( cEol.GetType() ){
	case EOL_CRLF:	//	下左矢印
		{
			sx = rcEol.left;						//X左端
			sy = rcEol.top + ( rcEol.Height() / 2);	//Y中心
#ifdef NKMM_LINE_MARGIN_TOP
			sy += nMargin;
#endif // NKMM_
			DWORD pp[] = { 3, 3 };
			POINT pt[6];
			pt[0].x = sx + rcEol.Width();	//	上へ
			pt[0].y = sy - rcEol.Height() / 4;
			pt[1].x = sx + rcEol.Width();	//	下へ
			pt[1].y = sy;
			pt[2].x = sx;	//	先頭へ
			pt[2].y = sy;
			pt[3].x = sx + rcEol.Height() / 4;	//	先頭から下へ
			pt[3].y = sy + rcEol.Height() / 4;
			pt[4].x = sx;	//	先頭へ戻り
			pt[4].y = sy;
			pt[5].x = sx + rcEol.Height() / 4;	//	先頭から上へ
			pt[5].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 1;	//	上へ（右へずらす）
				pt[0].y += 0;
				pt[1].x += 1;	//	右へ（右にひとつずれている）
				pt[1].y += 1;
				pt[2].x += 0;	//	先頭へ
				pt[2].y += 1;
				pt[3].x += 0;	//	先頭から下へ
				pt[3].y += 1;
				pt[4].x += 0;	//	先頭へ戻り
				pt[4].y += 1;
				pt[5].x += 0;	//	先頭から上へ
				pt[5].y += 1;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_CR:	//	左向き矢印	// 2007.08.17 ryoji EOL_LF -> EOL_CR
		{
			sx = rcEol.left;
			sy = rcEol.top + ( rcEol.Height() / 2 );
#ifdef NKMM_LINE_MARGIN_TOP
			sy += nMargin;
#endif // NKMM_
			DWORD pp[] = { 3, 2 };
			POINT pt[5];
			pt[0].x = sx + rcEol.Width();	//	右へ
			pt[0].y = sy;
			pt[1].x = sx;	//	先頭へ
			pt[1].y = sy;
			pt[2].x = sx + rcEol.Height() / 4;	//	先頭から下へ
			pt[2].y = sy + rcEol.Height() / 4;
			pt[3].x = sx;	//	先頭へ戻り
			pt[3].y = sy;
			pt[4].x = sx + rcEol.Height() / 4;	//	先頭から上へ
			pt[4].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 0;	//	右へ
				pt[0].y += 1;
				pt[1].x += 0;	//	先頭へ
				pt[1].y += 1;
				pt[2].x += 0;	//	先頭から下へ
				pt[2].y += 1;
				pt[3].x += 0;	//	先頭へ戻り
				pt[3].y += 1;
				pt[4].x += 0;	//	先頭から上へ
				pt[4].y += 1;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_LF:	//	下向き矢印	// 2007.08.17 ryoji EOL_CR -> EOL_LF
	// 2013.04.22 Moca NEL,LS,PS対応。暫定でLFと同じにする
		{
			sx = rcEol.left + ( rcEol.Width() / 2 );
			sy = rcEol.top + ( rcEol.Height() * 3 / 4 );
#ifdef NKMM_LINE_MARGIN_TOP
			sy += nMargin;
#endif // NKMM_
			DWORD pp[] = { 3, 2 };
			POINT pt[5];
			pt[0].x = sx;	//	上へ
			pt[0].y = rcEol.top + rcEol.Height() / 4 + 1;
			pt[1].x = sx;	//	上から下へ
			pt[1].y = sy;
			pt[2].x = sx - rcEol.Height() / 4;	//	そのまま左上へ
			pt[2].y = sy - rcEol.Height() / 4;
			pt[3].x = sx;	//	矢印の先端に戻る
			pt[3].y = sy;
			pt[4].x = sx + rcEol.Height() / 4;	//	そして右上へ
			pt[4].y = sy - rcEol.Height() / 4;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if( bBold ){
				pt[0].x += 1;	//	上へ
				pt[0].y += 0;
				pt[1].x += 1;	//	上から下へ
				pt[1].y += 0;
				pt[2].x += 1;	//	そのまま左上へ
				pt[2].y += 0;
				pt[3].x += 1;	//	矢印の先端に戻る
				pt[3].y += 0;
				pt[4].x += 1;	//	そして右上へ
				pt[4].y += 0;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	case EOL_NEL:
	case EOL_LS:
	case EOL_PS:
		{
			// 左下矢印(折れ曲がりなし)
			sx = rcEol.left;			//X左端
			sy = rcEol.top + ( rcEol.Height() * 3 / 4 );	//Y上から3/4
#ifdef NKMM_LINE_MARGIN_TOP
			sy += nMargin;
#endif // NKMM_
			DWORD pp[] = { 2, 3 };
			POINT pt[5];
			int nWidth = t_min(rcEol.Width(), rcEol.Height() / 2);
			pt[0].x = sx + nWidth;	//	右上から
			pt[0].y = sy - nWidth;
			pt[1].x = sx;	//	先頭へ
			pt[1].y = sy;
			pt[2].x = sx + nWidth;	//	右から
			pt[2].y = sy;
			pt[3].x = sx;	//	先頭へ戻り
			pt[3].y = sy;
			pt[4].x = sx;	//	先頭から上へ
			pt[4].y = sy - nWidth;
			::PolyPolyline( gr, pt, pp, _countof(pp));

			if ( bBold ) {
				pt[0].x += 0;	//	右上から
				pt[0].y += 1;
				pt[1].x += 0;	//	先頭へ
				pt[1].y += 1;
				pt[2].x += 0;	//	右から
				pt[2].y -= 1;
				pt[3].x += 1;	//	先頭へ戻り
				pt[3].y -= 1;
				pt[4].x += 1;	//	先頭から上へ
				pt[4].y += 0;
				::PolyPolyline( gr, pt, pp, _countof(pp));
			}
		}
		break;
	}
}
