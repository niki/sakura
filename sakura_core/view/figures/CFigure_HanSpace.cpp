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
	if(pcView->GetTextArea().GenerateClipRect(&rcClip,*pDispPos,1))
	{
#ifdef UZ_FIX_HAN_SPACE
		// 塗りつぶしで消去
		gr.FillSolidMyRect(rcClip, gr.GetTextBackColor());
//		// 空白で消去
//		CMyRect rcClipBottom=rcClip;
//		::ExtTextOutW_AnyBuild(
//			gr,
//			pDispPos->GetDrawPos().x,
//#  ifdef UZ_LINE_CENTERING
//			pcView->GetLineMargin() +
//#  endif  // UZ_
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
//#  ifdef UZ_LINE_CENTERING
//		y += pcView->GetLineMargin();
//#  endif  // UZ_
		gr.SetPen( gr.GetCurrentTextForeColor() );
		x--; // 少し左め
#ifdef UZ_FIX_HAN_SPACE
		if (m_nbsp) {
			::MoveToEx( gr, x-1, y-2, NULL );
			::LineTo(   gr, x+4, y+3 );
			::MoveToEx( gr, x+3, y-2, NULL );
			::LineTo(   gr, x-2, y+3 );
		} else {
			::MoveToEx( gr, x, y-1, NULL );
			::LineTo(   gr, x+2, y-1 );
			::MoveToEx( gr, x, y, NULL );
			::LineTo(   gr, x+2, y );
		}
#else
		::MoveToEx( gr, x, y-1, NULL );
		::LineTo(   gr, x+2, y-1 );
		::MoveToEx( gr, x, y, NULL );
		::LineTo(   gr, x+2, y );
#endif  // UZ_
#else
		//小文字"o"の下半分を出力
		CMyRect rcClipBottom=rcClip;
		rcClipBottom.top=rcClip.top+pcView->GetTextMetrics().GetHankakuHeight()/2;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
#  ifdef UZ_LINE_CENTERING
			pcView->GetLineMargin() +
#  endif  // UZ_
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipBottom,
			L"o",
			1,
			pcView->GetTextMetrics().GetDxArray_AllHankaku()
		);
		
		//上半分は普通の空白で出力（"o"の上半分を消す）
		CMyRect rcClipTop=rcClip;
		rcClipTop.bottom=rcClip.top+pcView->GetTextMetrics().GetHankakuHeight()/2;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
#  ifdef UZ_LINE_CENTERING
			pcView->GetLineMargin() +
#  endif  // UZ_
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipTop,
			L" ",
			1,
			pcView->GetTextMetrics().GetDxArray_AllHankaku()
		);
#endif  // UZ_
	}

	//位置進める
	pDispPos->ForwardDrawCol(1);
}


#ifdef  UZ_FIX_HAN_SPACE
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
#endif  // UZ_
