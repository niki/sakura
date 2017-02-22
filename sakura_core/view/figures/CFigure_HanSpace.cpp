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
#if REI_MOD_HAN_SPACE
		// 空白で消去
		CMyRect rcClipBottom=rcClip;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
  #if REI_LINE_CENTERING
			(pcView->m_pTypeData->m_nLineSpace/2) +
  #endif  // rei_
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipBottom,
			L" ",//L"･",
			1,
			pcView->GetTextMetrics().GetDxArray_AllHankaku()
		);
		
		// 半角スペースをドットで表現
		int x = rcClip.left + (rcClip.right - rcClip.left) / 2;
		int y = rcClip.top + (rcClip.bottom - rcClip.top) / 2;
  #if REI_LINE_CENTERING
		y += (pcView->m_pTypeData->m_nLineSpace/2);
  #endif  // rei_
		gr.SetPen( gr.GetCurrentTextForeColor() );
		x--; // 少し左め
		::MoveToEx( gr, x, y-2, NULL );
		::LineTo(   gr, x+2, y-2 );
		::MoveToEx( gr, x, y-1, NULL );
		::LineTo(   gr, x+2, y-1 );
#else  //REI_MOD_HAN_SPACE
		//小文字"o"の下半分を出力
		CMyRect rcClipBottom=rcClip;
		rcClipBottom.top=rcClip.top+pcView->GetTextMetrics().GetHankakuHeight()/2;
		::ExtTextOutW_AnyBuild(
			gr,
			pDispPos->GetDrawPos().x,
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
			pDispPos->GetDrawPos().y,
			ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
			&rcClipTop,
			L" ",
			1,
			pcView->GetTextMetrics().GetDxArray_AllHankaku()
		);
#endif  //REI_MOD_HAN_SPACE
	}

	//位置進める
	pDispPos->ForwardDrawCol(1);
}
