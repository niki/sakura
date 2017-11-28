#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo

#include "CFigure_HanSpace.h"
#include "types/CTypeSupport.h"

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     CFigure_HanSpace                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CFigure_HanSpace::Match(const wchar_t* pText, int nTextLen) const
{
	if( pText[0] == L' ' ){
		return true;
	}
	return false;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         描画実装                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

//! 半角スペース描画
void CFigure_HanSpace::DispSpace(CGraphics& gr, DispPos* pDispPos, CEditView* pcView, bool bTrans) const
{
	//クリッピング矩形を計算。画面外なら描画しない
	CMyRect rcClip;
	const int Dx = pcView->GetTextMetrics().CalcTextWidth3(L" ", 1);
	const CLayoutXInt nCol = CLayoutXInt(Dx);
	if(pcView->GetTextArea().GenerateClipRect(&rcClip,*pDispPos,nCol))
	{
#ifdef NK_FIX_HAN_SPACE
		// 塗りつぶしで消去
		gr.FillSolidMyRect(rcClip, gr.GetTextBackColor());
//		// 空白で消去
//		CMyRect rcClipBottom=rcClip;
//		::ExtTextOutW_AnyBuild(
//			gr,
//			pDispPos->GetDrawPos().x,
//#ifdef NK_LINE_CENTERING
//			pcView->GetLineMargin() +
//#endif  // NK_LINE_CENTERING
//			pDispPos->GetDrawPos().y,
//			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
//			&rcClipBottom,
//			L" ",//L"・",
//			1,
//			pcView->GetTextMetrics().GetDxArray_AllHankaku()
//		);
		
		// 半角スペースをドットで表現
		int x = rcClip.left + (rcClip.right - rcClip.left) / 2;
		int y = rcClip.top + (rcClip.bottom - rcClip.top) / 2;
		y++; // 少し下め
//#ifdef NK_LINE_CENTERING
//		y += pcView->GetLineMargin();
//#endif  // NK_LINE_CENTERING
		x--; // 少し左め
#ifdef NK_FIX_HAN_SPACE
		if (m_nbsp) {
			gr.SetPen( gr.GetCurrentTextForeColor() );
			::MoveToEx( gr, x-1, y-2, NULL );
			::LineTo(   gr, x+4, y+3 );
			::MoveToEx( gr, x+3, y-2, NULL );
			::LineTo(   gr, x-2, y+3 );
		} else {
			gr.FillSolidMyRect({x, y - 1, x + 2, y + 1}, gr.GetCurrentTextForeColor());
			//::MoveToEx( gr, x, y-1, NULL );
			//::LineTo(   gr, x+2, y-1 );
			//::MoveToEx( gr, x, y, NULL );
			//::LineTo(   gr, x+2, y );
		}
#else
		gr.FillSolidMyRect({x, y - 1, x + 2, y + 1}, gr.GetCurrentTextForeColor());
		//::MoveToEx( gr, x, y-1, NULL );
		//::LineTo(   gr, x+2, y-1 );
		//::MoveToEx( gr, x, y, NULL );
		//::LineTo(   gr, x+2, y );
#endif  // NK_
#else
		//小文字"o"の下半分を出力
		CMyRect rcClipBottom=rcClip;
		rcClipBottom.top=rcClip.top+pcView->GetTextMetrics().GetHankakuHeight()/2;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
#ifdef NK_LINE_CENTERING
			pcView->GetLineMargin() +
#endif  // NK_LINE_CENTERING
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipBottom,
//FIXME:幅が違う
			L"o",
			1,
			&Dx
		);
		
		//上半分は普通の空白で出力（"o"の上半分を消す）
		CMyRect rcClipTop=rcClip;
		rcClipTop.bottom=rcClip.top+pcView->GetTextMetrics().GetHankakuHeight()/2;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
#ifdef NK_LINE_CENTERING
			pcView->GetLineMargin() +
#endif  // NK_LINE_CENTERING
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipTop,
			L" ",
			1,
			&Dx
		);
#endif  // NK_
	}

	//位置進める
	pDispPos->ForwardDrawCol(nCol);
}


#ifdef  NK_FIX_HAN_SPACE
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     CFigure_NBSP                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CFigure_NBSP::Match(const wchar_t* pText, int nTextLen) const
{
	if( pText[0] == L' ' ){
		return true;
	}
	return false;
}
#endif  // NK_
