#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CFigure_Tab.h"
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include "types/CTypeSupport.h"

//2007.08.28 kobake 追加
void _DispTab( CGraphics& gr, DispPos* pDispPos, const CEditView* pcView );
//タブ矢印描画関数	//@@@ 2003.03.26 MIK
void _DrawTabArrow( CGraphics& gr, int nPosX, int nPosY, int nWidth, int nHeight, bool bBold, COLORREF pColor );

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         CFigure_Tab                         //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CFigure_Tab::Match(const wchar_t* pText, int nTextLen) const
{
	if( pText[0] == WCODE::TAB ){
		return true;
	}
	return false;
}




// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         描画実装                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*! TAB描画
	@date 2001.03.16 by MIK
	@date 2002.09.22 genta 共通式のくくりだし
	@date 2002.09.23 genta LayoutMgrの値を使う
	@date 2003.03.26 MIK タブ矢印表示
	@date 2013.05.31 novice TAB表示対応(文字指定/短い矢印/長い矢印)
*/
void CFigure_Tab::DispSpace(CGraphics& gr, DispPos* pDispPos, CEditView* pcView, bool bTrans) const
{
	DispPos& sPos=*pDispPos;

	//必要なインターフェース
	const CTextMetrics* pMetrics=&pcView->GetTextMetrics();
	const CTextArea* pArea=&pcView->GetTextArea();

	int nLineHeight = pMetrics->GetHankakuDy();
	int nCharWidth = pMetrics->GetHankakuDx();

	CTypeSupport cTabType(pcView,COLORIDX_TAB);

	// これから描画するタブ幅
	int tabDispWidth = (Int)pcView->m_pcEditDoc->m_cLayoutMgr.GetActualTsvSpace( sPos.GetDrawCol(), WCODE::TAB );

	// タブ記号領域
	RECT rcClip2;
	rcClip2.left = sPos.GetDrawPos().x;
	rcClip2.right = rcClip2.left + nCharWidth * tabDispWidth;
	if( rcClip2.left < pArea->GetAreaLeft() ){
		rcClip2.left = pArea->GetAreaLeft();
	}
	rcClip2.top = sPos.GetDrawPos().y;
	rcClip2.bottom = sPos.GetDrawPos().y + nLineHeight;

	if( pArea->IsRectIntersected(rcClip2) ){
#ifdef REI_MOD_TAB
		// 塗りつぶしで消去
		gr.SetBrushColor(gr.GetTextBackColor());
		::FillRect(gr, &rcClip2, gr.GetCurrentBrush());
		
		int	nPosLeft = rcClip2.left > sPos.GetDrawPos().x ? rcClip2.left : sPos.GetDrawPos().x;
		_DrawTabArrow(
			gr,
			nPosLeft,
//#ifdef REI_LINE_CENTERING
//		(pcView->m_pTypeData->m_nLineSpace / 2) +
//#endif  // rei_
			sPos.GetDrawPos().y,
			nCharWidth * tabDispWidth - (nPosLeft -  sPos.GetDrawPos().x),	// Tab Area一杯に 2013/4/11 Uchi
#ifdef REI_LINE_CENTERING
			pcView->m_pTypeData->m_nLineSpace +
#endif  // rei_
			pMetrics->GetHankakuHeight(),
			gr.GetCurrentMyFontBold() || m_pTypeData->m_ColorInfoArr[COLORIDX_TAB].m_sFontAttr.m_bBoldFont,
			gr.GetCurrentTextForeColor()
		);
#else
		if( cTabType.IsDisp() && TABARROW_STRING == m_pTypeData->m_bTabArrow ){	//タブ通常表示	//@@@ 2003.03.26 MIK
			//@@@ 2001.03.16 by MIK
			::ExtTextOutW_AnyBuild(
				gr,
				sPos.GetDrawPos().x,
				sPos.GetDrawPos().y,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				m_pTypeData->m_szTabViewString,
				tabDispWidth <= 8 ? tabDispWidth : 8, // Sep. 22, 2002 genta
				pMetrics->GetDxArray_AllHankaku()
			);
		}else{
			//背景
			::ExtTextOutW_AnyBuild(
				gr,
				sPos.GetDrawPos().x,
				sPos.GetDrawPos().y,
				ExtTextOutOption() & ~(bTrans? ETO_OPAQUE: 0),
				&rcClip2,
				L"        ",
				tabDispWidth <= 8 ? tabDispWidth : 8, // Sep. 22, 2002 genta
				pMetrics->GetDxArray_AllHankaku()
			);

			//タブ矢印表示
			if( cTabType.IsDisp() ){
				// 文字色や太字かどうかを現在の DC から調べる	// 2009.05.29 ryoji 
				// （検索マッチ等の状況に柔軟に対応するため、ここは記号の色指定には決め打ちしない）
				//	太字かどうか設定も見る様にする 2013/4/11 Uchi
				// 2013.06.21 novice 文字色、太字をCGraphicsから取得

				if( TABARROW_SHORT == m_pTypeData->m_bTabArrow ){
					if( rcClip2.left <= sPos.GetDrawPos().x ){ // Apr. 1, 2003 MIK 行番号と重なる
						_DrawTabArrow(
							gr,
							sPos.GetDrawPos().x,
//#ifdef REI_LINE_CENTERING
//							(pcView->m_pTypeData->m_nLineSpace / 2) +
//#endif  // rei_
							sPos.GetDrawPos().y,
							pMetrics->GetHankakuWidth(),
#ifdef REI_LINE_CENTERING
							pcView->m_pTypeData->m_nLineSpace +
#endif  // rei_
							pMetrics->GetHankakuHeight(),
							gr.GetCurrentMyFontBold() || m_pTypeData->m_ColorInfoArr[COLORIDX_TAB].m_sFontAttr.m_bBoldFont,
							gr.GetCurrentTextForeColor()
						);
					}
				} else if( TABARROW_LONG == m_pTypeData->m_bTabArrow ){
					int	nPosLeft = rcClip2.left > sPos.GetDrawPos().x ? rcClip2.left : sPos.GetDrawPos().x;
					_DrawTabArrow(
						gr,
						nPosLeft,
//#ifdef REI_LINE_CENTERING
//						(pcView->m_pTypeData->m_nLineSpace / 2) +
//#endif  // rei_
						sPos.GetDrawPos().y,
						nCharWidth * tabDispWidth - (nPosLeft -  sPos.GetDrawPos().x),	// Tab Area一杯に 2013/4/11 Uchi
#ifdef REI_LINE_CENTERING
						pcView->m_pTypeData->m_nLineSpace +
#endif  // rei_
						pMetrics->GetHankakuHeight(),
						gr.GetCurrentMyFontBold() || m_pTypeData->m_ColorInfoArr[COLORIDX_TAB].m_sFontAttr.m_bBoldFont,
						gr.GetCurrentTextForeColor()
					);
				}
			}
		}
#endif  // rei_
	}

	//Xを進める
	sPos.ForwardDrawCol(tabDispWidth);
}



/*
	タブ矢印描画関数
*/
void _DrawTabArrow(
	CGraphics&	gr,
	int			nPosX,   //ピクセルX
	int			nPosY,   //ピクセルY
	int			nWidth,  //ピクセルW
	int			nHeight, //ピクセルH
	bool		bBold,
	COLORREF	pColor
)
{
	// ペン設定
	gr.PushPen( pColor, 0 );

	// 矢印の先頭
	int sx = nPosX + nWidth - 2;
	int sy = nPosY + ( nHeight / 2 );
	int sa = nHeight / 4;								// 鏃のsize

#ifdef REI_MOD_TAB
	sy++; // 少し下め
	
	::MoveToEx( gr, nPosX+1, sy, NULL );
	::LineTo(   gr, sx+1, sy );
#else
	DWORD pp[] = { 3, 2 };
	POINT pt[5];
	pt[0].x = nPosX;	//「─」左端から右端
	pt[0].y = sy;
	pt[1].x = sx;		//「／」右端から斜め左下
	pt[1].y = sy;
	pt[2].x = sx - sa;	//	矢印の先端に戻る
	pt[2].y = sy + sa;
	pt[3].x = sx;		//「＼」右端から斜め左上
	pt[3].y = sy;
	pt[4].x = sx - sa;
	pt[4].y = sy - sa;
	::PolyPolyline( gr, pt, pp, _countof(pp));

	if( bBold ){
		pt[0].x += 0;	//「─」左端から右端
		pt[0].y += 1;
		pt[1].x += 0;	//「／」右端から斜め左下
		pt[1].y += 1;
		pt[2].x += 0;	//	矢印の先端に戻る
		pt[2].y += 1;
		pt[3].x += 0;	//「＼」右端から斜め左上
		pt[3].y += 1;
		pt[4].x += 0;
		pt[4].y += 1;
		::PolyPolyline( gr, pt, pp, _countof(pp));
	}
#endif  // rei_

	gr.PopPen();
}
