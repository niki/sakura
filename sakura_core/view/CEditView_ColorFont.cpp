/*!	@file
	@brief カラーフォント(絵文字等)描画キューの管理

	CFigure_Text::DrawImp が1文字GDI描画するたびに TryQueueColorGlyph() を呼び、
	1visual行の描画が完全に終わった直後(選択範囲の反転描画も含む)に
	DrawLayoutLine から FlushColorGlyphQueue() が呼ばれ、たまった分をまとめて
	Direct2D/DirectWriteでオーバーレイ描画する。
*/
#include "StdAfx.h"
#include "CEditView.h"

#ifdef NKMM_FIX_COLOR_FONT

#include "CColorFontRenderer.h"
#include "uiparts/CGraphics.h"

void CEditView::TryQueueColorGlyph(HFONT hFont, const wchar_t* pData, int nLength, const RECT& rcCell, int nBaselineTopOffset, COLORREF crFore, COLORREF crBack)
{
	//GDIが実際に確保したピクセル幅をそのままグリフ送り幅として使う(桁ズレ防止)
	float fAdvanceX = (float)(rcCell.right - rcCell.left);
	if( fAdvanceX <= 0.0f ){
		return;
	}

	SColorGlyphCell cell;
	cell.rcCell = rcCell;
	cell.nBaselineTopOffset = nBaselineTopOffset;
	cell.crFore = crFore;
	cell.crBack = crBack;
	cell.bEraseFirst = false;
	if( CColorFontRenderer::getInstance()->TryGetColorLayers(hFont, pData, nLength, fAdvanceX, &cell) ){
		m_vPendingColorGlyphs.push_back(cell);
	}
}

void CEditView::FlushColorGlyphQueue(CGraphics& gr)
{
	if( m_vPendingColorGlyphs.empty() ){
		return;
	}

	RECT rcUnion = m_vPendingColorGlyphs[0].rcCell;
	for( size_t i = 1; i < m_vPendingColorGlyphs.size(); ++i ){
		const RECT& r = m_vPendingColorGlyphs[i].rcCell;
		rcUnion.left   = t_min(rcUnion.left,   r.left);
		rcUnion.top    = t_min(rcUnion.top,    r.top);
		rcUnion.right  = t_max(rcUnion.right,  r.right);
		rcUnion.bottom = t_max(rcUnion.bottom, r.bottom);
	}

	CColorFontRenderer::getInstance()->FlushQueue((HDC)gr, rcUnion, m_vPendingColorGlyphs);
	m_vPendingColorGlyphs.clear();
}

#endif // NKMM_FIX_COLOR_FONT
/*[EOF]*/
