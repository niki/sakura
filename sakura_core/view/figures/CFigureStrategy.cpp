/*
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
#include "view/CViewFont.h"
#include "CFigureStrategy.h"
#include "doc/layout/CLayout.h"
#include "charset/charcode.h"
#include "types/CTypeSupport.h"

void CFigure_Text::FowardChars(SColorStrategyInfo* pInfo)
{
	int nIdx = pInfo->GetPosInLogic();
	int nLength =	CNativeW::GetSizeOfChar(	// サロゲートペア対策	2008.10.12 ryoji
						pInfo->m_pLineOfLogic,
						pInfo->GetDocLine()->GetLengthWithoutEOL(),
						nIdx
					);
	pInfo->m_nPosInLogic += nLength;
}


CLayoutInt CFigure_Text::GetDrawSize(SColorStrategyInfo* pInfo) const
{
	int nIdx = pInfo->GetPosInLogic();
	return CNativeW::GetKetaOfChar( pInfo->m_pLineOfLogic, pInfo->GetDocLine()->GetLengthWithoutEOL(), nIdx );
}

bool CFigure_Text::DrawImp(SColorStrategyInfo* pInfo, int nPos, int nLength)
{
	int nIdx = nPos;
	bool bTrans = pInfo->m_pcView->IsBkBitmap() && CTypeSupport(pInfo->m_pcView, COLORIDX_TEXT).GetBackColor() == pInfo->m_gr.GetTextBackColor();
	pInfo->m_pcView->GetTextDrawer().DispText(
		pInfo->m_gr,
		pInfo->m_pDispPos,
		&pInfo->m_pLineOfLogic[nIdx],
		nLength,
		bTrans
	);
	// pInfo->m_nPosInLogic += nLength; ここでは進めない
	return true;
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         描画統合                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                      CFigureSpace                           //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
bool CFigureSpace::DrawImp(SColorStrategyInfo* pInfo)
{
	bool bTrans = DrawImp_StyleSelect(pInfo);
	DispPos sPos(*pInfo->m_pDispPos);	// 現在位置を覚えておく
	DispSpace(pInfo->m_gr, pInfo->m_pDispPos,pInfo->m_pcView, bTrans);	// 空白描画
	DrawImp_StylePop(pInfo);
	DrawImp_DrawUnderline(pInfo, sPos);
	// 1文字前提
	pInfo->m_nPosInLogic += CNativeW::GetSizeOfChar(	// 行末以外はここでスキャン位置を１字進める
		pInfo->m_pLineOfLogic,
		pInfo->GetDocLine()->GetLengthWithoutEOL(),
		pInfo->GetPosInLogic()
		);
	return true;
}

bool CFigureSpace::DrawImp_StyleSelect(SColorStrategyInfo* pInfo)
{
	// この DrawImp はここ（基本クラス）でデフォルト動作を実装しているが
	// 仮想関数なので派生クラス側のオーバーライドで個別に仕様変更可能
	CEditView* pcView = pInfo->m_pcView;

	CTypeSupport cCurrentType(pcView, pInfo->GetCurrentColor());	// 周辺の色（現在の指定色/選択色）
	CTypeSupport cCurrentType2(pcView, pInfo->GetCurrentColor2());	// 周辺の色（現在の指定色）
	CTypeSupport cTextType(pcView, COLORIDX_TEXT);				// テキストの指定色
	CTypeSupport cSpaceType(pcView, GetDispColorIdx());	// 空白の指定色
	CTypeSupport cCurrentTypeBg(pcView, pInfo->GetCurrentColorBg());
	CTypeSupport& cCurrentType1 = (cCurrentType.GetBackColor() == cTextType.GetBackColor() ? cCurrentTypeBg: cCurrentType);
	CTypeSupport& cCurrentType3 = (cCurrentType2.GetBackColor() == cTextType.GetBackColor() ? cCurrentTypeBg: cCurrentType2);

	// 空白記号類は特に明示指定した部分以外はなるべく周辺の指定に合わせるようにしてみた	// 2009.05.30 ryoji
	// 例えば、下線を指定していない場合、正規表現キーワード内なら正規表現キーワード側の下線指定に従うほうが自然な気がする。
	// （そのほうが空白記号の「表示」をチェックしていない場合の表示に近い）
	//
	// 前景色・背景色の扱い
	// ・通常テキストとは異なる色が指定されている場合は空白記号の側の指定色を使う
	// ・通常テキストと同じ色が指定されている場合は周辺の色指定に合わせる
	// 太字の扱い
	// ・空白記号か周辺のどちらか一方でも太字指定されていれば「前景色・背景色の扱い」で決定した前景色で太字にする
	// 下線の扱い
	// ・空白記号で下線指定されていれば「前景色・背景色の扱い」で決定した前景色で下線を引く
	// ・空白記号で下線指定されておらず周辺で下線指定されていれば周辺の前景色で下線を引く
	// [選択]レンダリング中
	// ・混合色の場合は従来通り。
	COLORREF crText;
	COLORREF crBack;
	bool blendColor = pInfo->GetCurrentColor() != pInfo->GetCurrentColor2() && cCurrentType.GetTextColor() == cCurrentType.GetBackColor(); // 選択混合色
	bool bBold;
#if REI_MOD_SELAREA
	blendColor = true;
#endif // rei_
	if( blendColor ){
		CTypeSupport& cText = cSpaceType.GetTextColor() == cTextType.GetTextColor() ? cCurrentType2 : cSpaceType;
		CTypeSupport& cBack = cSpaceType.GetBackColor() == cTextType.GetBackColor() ? cCurrentType3 : cSpaceType;
		crText = pcView->GetTextColorByColorInfo2(cCurrentType.GetColorInfo(), cText.GetColorInfo());
		crBack = pcView->GetBackColorByColorInfo2(cCurrentType.GetColorInfo(), cBack.GetColorInfo());
		bBold = cCurrentType2.IsBoldFont();
	}else{
		CTypeSupport& cText = cSpaceType.GetTextColor() == cTextType.GetTextColor() ? cCurrentType : cSpaceType;
		CTypeSupport& cBack = cSpaceType.GetBackColor() == cTextType.GetBackColor() ? cCurrentType1 : cSpaceType;
		crText = cText.GetTextColor();
		crBack = cBack.GetBackColor();
		bBold = cCurrentType.IsBoldFont();
	}
#if REI_MOD_SP_COLOR
	/*!
	* 色をマージする
	* @param[in]	colText テキスト色
	* @param[in]	colBase ベースとなる色
	* @return 合成後の色
	*/
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
	
	EColorIndexType colorIdx = GetColorIdx();
	bool bIgnore	= (
					   colorIdx == COLORIDX_CTRLCODE ||
//					   colorIdx == COLORIDX_COMMENT ||
//					   colorIdx == COLORIDX_BLOCK1 ||
//					   colorIdx == COLORIDX_BLOCK2 ||
//  #if REI_MOD_SP_COLOR == 3
//					   colorIdx == COLORIDX_TEXT ||
//  #endif // rei_
//					   colorIdx == COLORIDX_SSTRING ||
//					   colorIdx == COLORIDX_WSTRING
					0);
	if (! bIgnore) {
	  static int nBlendPer = RegGetDword(L"WhiteSpaceBlendPer", REI_MOD_SP_BLEND_PER);
//	if (1) {
  #if REI_MOD_SP_COLOR == 1 // 現在のテキスト色をブレントする
		crText = pcView->GetTextColorByColorInfo2(cCurrentType2.GetColorInfo(), cText.GetColorInfo(), nBlendPer);
  #elif REI_MOD_SP_COLOR == 2 // 現在のテキスト色の半分 (空白TABのカラー設定は無視されます)
		COLORREF col = cCurrentType2.GetTextColor();
		crText = RGB( GetRValue(col) / 2, GetGValue(col) / 2, GetBValue(col) / 2 );
  #elif REI_MOD_SP_COLOR == 3 // 現在のテキスト色と現在の背景色をブレンドする (空白TABのカラー設定は無視されます)
		COLORREF col1 = cCurrentType2.GetTextColor();
		//COLORREF col2 = cTextType.GetBackColor();
		COLORREF col2 = crBack;	// 合成済みの色を使用する
		crText = fnMeargeColor( col1, col2, nBlendPer );
  #endif // rei_
	}
#endif // rei_
	//cSpaceType.SetGraphicsState_WhileThisObj(pInfo->gr);

	pInfo->m_gr.PushTextForeColor(crText);
	pInfo->m_gr.PushTextBackColor(crBack);
	// Figureが下線指定ならこちらで下線を指定。元の色のほうが下線指定なら、DrawImp_DrawUnderlineで下線だけ指定
	SFONT sFont;
	sFont.m_sFontAttr.m_bBoldFont = cSpaceType.IsBoldFont() || bBold;
	sFont.m_sFontAttr.m_bUnderLine = cSpaceType.HasUnderLine();
	sFont.m_hFont = pInfo->m_pcView->GetFontset().ChooseFontHandle( sFont.m_sFontAttr );
	pInfo->m_gr.PushMyFont(sFont);
	bool bTrans = pcView->IsBkBitmap() && cTextType.GetBackColor() == crBack;
	return bTrans;
}

void CFigureSpace::DrawImp_StylePop(SColorStrategyInfo* pInfo)
{
	pInfo->m_gr.PopTextForeColor();
	pInfo->m_gr.PopTextBackColor();
	pInfo->m_gr.PopMyFont();
}

void CFigureSpace::DrawImp_DrawUnderline(SColorStrategyInfo* pInfo, DispPos& sPos)
{
	CEditView* pcView = pInfo->m_pcView;

	CTypeSupport cCurrentType(pcView, pInfo->GetCurrentColor());	// 周辺の色
	bool blendColor = pInfo->GetCurrentColor() != pInfo->GetCurrentColor2() && cCurrentType.GetTextColor() == cCurrentType.GetBackColor(); // 選択混合色

	CTypeSupport colorStyle(pcView, blendColor ? pInfo->GetCurrentColor2() : pInfo->GetCurrentColor());	// 周辺の色
	CTypeSupport cSpaceType(pcView, GetDispColorIdx());	// 空白の指定色

	if( !cSpaceType.HasUnderLine() && colorStyle.HasUnderLine() )
	{
		// 下線を周辺の前景色で描画する
		SFONT sFont;
		sFont.m_sFontAttr.m_bBoldFont = false;
		sFont.m_sFontAttr.m_bUnderLine = true;
		sFont.m_hFont = pInfo->m_pcView->GetFontset().ChooseFontHandle( sFont.m_sFontAttr );
		pInfo->m_gr.PushMyFont(sFont);

		int nLength = (Int)(pInfo->m_pDispPos->GetDrawCol() - sPos.GetDrawCol());
		wchar_t* pszText = new wchar_t[nLength];
		for( int i = 0; i < nLength; i++ )
			pszText[i] = L' ';
		pInfo->m_pcView->GetTextDrawer().DispText(
			pInfo->m_gr,
			&sPos,
			pszText,
			nLength,
			true		// 背景は透明
		);
		delete []pszText;

		pInfo->m_gr.PopMyFont();
	}
}

